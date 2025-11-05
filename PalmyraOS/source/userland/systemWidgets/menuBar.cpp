

#include "userland/systemWidgets/menuBar.h"

#include "palmyraOS/stdio.h"   // printf, perror
#include "palmyraOS/stdlib.h"  // malloc
#include "palmyraOS/time.h"
#include "palmyraOS/unistd.h"  // PalmyraOS API

#include "libs/stdio.h"   // snprintf
#include "libs/string.h"  // strlen
#include <cstddef>        // size_t

// TODO: move these to libs (or put them in palmyraOS)
#include "core/Font.h"         // For Fonts
#include "core/FrameBuffer.h"  // FrameBuffer
#include "core/VBE.h"          // for Brush, TextRenderer


// Helper function to allocate memory for back buffer
uint32_t* allocateBackBuffer(size_t requiredMemory) {
    volatile auto backBuffer = (uint32_t*) malloc(requiredMemory);
    if (backBuffer == MAP_FAILED) {
        perror("Failed to map memory\n");
        _exit(-1);
    }
    else printf("Success to map memory\n");
    return backBuffer;
}

// Helper function to initialize window
size_t initializeWindowWrapper(uint32_t** frontBuffer, palmyra_window& windowInfo) {
    uint32_t bufferId = initializeWindow(frontBuffer, &windowInfo);
    if (bufferId == 0) {
        perror("Failed to initialize window\n");
        _exit(-1);
    }
    else printf("Success to initialize window\n");
    return bufferId;
}

// Helper function to calculate elapsed time in seconds
int calculateElapsedTimeInSeconds(const rtc_time& start, const rtc_time& current) {
    int startSeconds   = start.tm_hour * 3600 + start.tm_min * 60 + start.tm_sec;
    int currentSeconds = current.tm_hour * 3600 + current.tm_min * 60 + current.tm_sec;
    return currentSeconds - startSeconds;
}

[[noreturn]] int PalmyraOS::Userland::builtin::MenuBar::main(uint32_t argc, char** argv) {

    // Define dimensions and required memory for the window
    palmyra_window w      = {.x = 0, .y = 0, .width = 1024, .height = 20, .movable = false, .title = "MenuBar"};
    size_t requiredMemory = w.width * w.height * sizeof(uint32_t);

    // Allocate memory for the back buffer
    uint32_t* backBuffer  = allocateBackBuffer(requiredMemory);

    // Request and initialize the front buffer window
    uint32_t* frontBuffer = nullptr;
    size_t bufferId       = initializeWindowWrapper(&frontBuffer, w);

    // Initialize rendering helpers
    PalmyraOS::kernel::FrameBuffer frameBuffer(w.width, w.height, frontBuffer, backBuffer);
    PalmyraOS::kernel::Brush brush(frameBuffer);

    // Load the font for text rendering (requires kernel memory) TODO: only allowed at this stage, not anymore later
    PalmyraOS::kernel::TextRenderer textRenderer(frameBuffer, PalmyraOS::Font::Arial12);

    // Fill the initial background
    brush.fill(PalmyraOS::Color::Gray100);


    timespec time_spec{};
    uint64_t count      = 0;

    // Open the real-time clock device
    size_t epochTime_fd = open("/dev/rtc", 0);
    // assume it has worked

    // Initialize time structure
    rtc_time epochTime{};
    rtc_time startTime{};
    ioctl(epochTime_fd, RTC_RD_TIME, &startTime);

    // Enhanced FPS calculation using bucketed approach (4 half-second buckets = 2 second window)
    constexpr size_t BUCKET_COUNT             = 4;             // 4 buckets for 2 second window
    constexpr uint64_t BUCKET_DURATION_NS     = 500000000ULL;  // 0.5 seconds in nanoseconds
    uint32_t frameCountBuckets[BUCKET_COUNT]  = {0};           // Store frame count for each half-second
    size_t currentBucketIndex                 = 0;             // Current bucket we're writing to
    uint64_t lastBucketStartTime              = 0;             // Start time of current bucket
    uint64_t currentFPS                       = 0;
    uint64_t lastValidFPS                     = 60;  // Fallback

    // Rendering frame rate cap (100 FPS max)
    constexpr uint64_t MIN_RENDER_INTERVAL_NS = 10000000ULL;  // 10ms = 100 FPS
    uint64_t lastRenderTime                   = 0;            // Track last time we actually rendered

    // Main loop for rendering and updates
    while (true) {
        count++;
        clock_gettime(CLOCK_MONOTONIC, &time_spec);

        // Get current timestamp
        uint64_t currentTimestamp = time_spec.tv_sec * 1000000000ULL + time_spec.tv_nsec;

        // Initialize on first frame
        if (lastBucketStartTime == 0) {
            lastBucketStartTime = currentTimestamp;
            lastRenderTime      = currentTimestamp;
        }

        // ============ BUCKETED FPS CALCULATION (continuous, counts ALL frames) ============
        // Check if we need to move to next bucket (rotate every 500ms)
        uint64_t timeSinceBucketStart = currentTimestamp - lastBucketStartTime;
        if (timeSinceBucketStart >= BUCKET_DURATION_NS) {
            // Time to rotate to next bucket
            currentBucketIndex                    = (currentBucketIndex + 1) % BUCKET_COUNT;
            // Don't reset here - let it naturally fill from 0
            frameCountBuckets[currentBucketIndex] = 0;
            lastBucketStartTime                   = currentTimestamp;
        }

        // Increment frame count for current bucket (EVERY frame, including non-rendered ones)
        frameCountBuckets[currentBucketIndex]++;

        // Calculate average FPS over all COMPLETED buckets (exclude the currently active bucket)
        // Only count completed buckets to ensure we have full 500ms of data
        uint32_t totalFrames = 0;
        uint32_t bucketCount = 0;
        for (size_t i = 0; i < BUCKET_COUNT; i++) {
            if (i == currentBucketIndex) continue;  // Skip the currently active bucket being filled
            if (frameCountBuckets[i] > 0) {
                totalFrames += frameCountBuckets[i];
                bucketCount++;
            }
        }

        // Calculate FPS based on completed buckets only
        // Each bucket is 500ms, so: FPS = totalFrames / (bucketCount * 0.5)
        // With 3 completed buckets, this gives us 1.5 seconds of accurate data
        uint64_t calculatedFPS = 0;
        if (bucketCount > 0) {
            calculatedFPS = (totalFrames * 2) / bucketCount;  // Normalize to 2 seconds worth
        }

        // Sanity check: reasonable FPS range
        if (calculatedFPS >= 1 && calculatedFPS <= 5000) { lastValidFPS = calculatedFPS; }
        currentFPS = lastValidFPS;

        // ============ RENDERING FRAME RATE CAP ============
        // Check if enough time has passed since last render (100 FPS max = 10ms minimum)
        // uint64_t timeSinceLastRender = currentTimestamp - lastRenderTime;
        // if (timeSinceLastRender < MIN_RENDER_INTERVAL_NS) {
        //    // Not enough time has passed, yield to other processes (don't render yet)
        //    sched_yield();
        //    continue;  // Skip rendering, go to next iteration
        //}

        // Enough time has passed, proceed with rendering
        // lastRenderTime = currentTimestamp;

        brush.fill(PalmyraOS::Color::Black);
        brush.drawHLine(0, w.width, w.height - 1, PalmyraOS::Color::Gray100);
        brush.drawHLine(0, w.width, w.height - 2, PalmyraOS::Color::Gray500);

        // Render the logo and system information
        textRenderer << Color::Orange << "Palmyra" << Color::LighterBlue << "OS ";
        textRenderer << Color::Gray100 << "v0.01\t";
        textRenderer << "SysTime: " << time_spec.tv_sec << " s\t";
        textRenderer << "Frames: " << count << "\t";

        // Render the current time if available
        if (epochTime_fd) {
            ioctl(epochTime_fd, RTC_RD_TIME, &epochTime);

            char clock_buffer[50];
            snprintf(clock_buffer,
                     50,
                     "%04d-%02d-%02d %02d:%02d:%02d",
                     epochTime.tm_year + 1900,
                     epochTime.tm_mon + 1,
                     epochTime.tm_mday,
                     epochTime.tm_hour,
                     epochTime.tm_min,
                     epochTime.tm_sec);
            textRenderer.setCursor(w.width / 2 - 50, 0);  // Center the clock text
            textRenderer << clock_buffer;

            // Display enhanced FPS (rolling window average)
            textRenderer << "\tFPS: " << (int) currentFPS;
        }

        // Reset text renderer and swap frame buffers for next frame
        textRenderer.reset();
        frameBuffer.swapBuffers();

        // Yield to other processes
        sched_yield();
    }

    // Cleanup: Free allocated memory and close device
    // free(frameTimeHistory); // This line is no longer needed
    close(epochTime_fd);
}

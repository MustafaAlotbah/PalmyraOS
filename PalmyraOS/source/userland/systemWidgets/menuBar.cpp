

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


// ============ APP LAUNCHER STRUCTURE ============
struct AppEntry {
    const char* name;
    const char* path;
};

struct AppLauncherState {
    bool isOpen                                   = false;
    uint32_t windowID                             = 0;
    uint32_t* frontBuffer                         = nullptr;
    uint32_t* backBuffer                          = nullptr;
    PalmyraOS::kernel::FrameBuffer* frameBuffer   = nullptr;
    PalmyraOS::kernel::Brush* brush               = nullptr;
    PalmyraOS::kernel::TextRenderer* textRenderer = nullptr;
    palmyra_window windowInfo                     = {};
    AppEntry apps[5]                              = {{"Terminal", "/bin/terminal.elf"},
                                                     {"File Manager", "/bin/filemanager.elf"},
                                                     {"Task Manager", "/bin/taskmanager.elf"},
                                                     {"Clock", "/bin/clock.elf"},
                                                     {"Image Viewer", "/bin/imgview.elf"}};
    int appCount                                  = 5;
    int itemHeight                                = 25;
    MouseEvent lastMouseEvent                     = {0, 0, false, false, false, false};
};

// Forward declarations
void openAppLauncher(AppLauncherState& launcher);
void updateAppLauncher(AppLauncherState& launcher);
void closeAppLauncher(AppLauncherState& launcher);

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

// ============ APP LAUNCHER FUNCTIONS ============

// Open the app launcher window (non-blocking, per-frame update)
void openAppLauncher(AppLauncherState& launcher) {
    if (launcher.isOpen) return;  // Already open

    printf("[MenuBar] Opening App Launcher\n");

    // Calculate dynamic height: appCount * itemHeight with minimal padding
    int windowHeight      = launcher.appCount * launcher.itemHeight + 4;  // Just 2px top + 2px bottom

    // Setup window info
    launcher.windowInfo   = {.x = 90, .y = 25, .width = 150, .height = (uint32_t) windowHeight, .movable = false, .title = "Apps"};

    // Allocate back buffer
    size_t requiredMemory = launcher.windowInfo.width * launcher.windowInfo.height * sizeof(uint32_t);
    launcher.backBuffer   = allocateBackBuffer(requiredMemory);
    if (!launcher.backBuffer) {
        printf("[MenuBar] Failed to allocate back buffer\n");
        return;
    }

    // Initialize window and get front buffer
    launcher.windowID = initializeWindow(&launcher.frontBuffer, &launcher.windowInfo);
    if (launcher.windowID == 0) {
        printf("[MenuBar] Failed to create launcher window\n");
        free(launcher.backBuffer);
        return;
    }

    // Create rendering objects using malloc + placement new
    launcher.frameBuffer = (PalmyraOS::kernel::FrameBuffer*) malloc(sizeof(PalmyraOS::kernel::FrameBuffer));
    new (launcher.frameBuffer) PalmyraOS::kernel::FrameBuffer(launcher.windowInfo.width, launcher.windowInfo.height, launcher.frontBuffer, launcher.backBuffer);

    launcher.brush = (PalmyraOS::kernel::Brush*) malloc(sizeof(PalmyraOS::kernel::Brush));
    new (launcher.brush) PalmyraOS::kernel::Brush(*launcher.frameBuffer);

    launcher.textRenderer = (PalmyraOS::kernel::TextRenderer*) malloc(sizeof(PalmyraOS::kernel::TextRenderer));
    new (launcher.textRenderer) PalmyraOS::kernel::TextRenderer(*launcher.frameBuffer, PalmyraOS::Font::Arial12);

    launcher.isOpen = true;
    printf("[MenuBar] App Launcher opened (ID: %d, height: %d)\n", launcher.windowID, windowHeight);
}

// Update and render the app launcher (called once per MenuBar frame)
void updateAppLauncher(AppLauncherState& launcher) {
    if (!launcher.isOpen || !launcher.brush || !launcher.textRenderer) return;

    // Fill background (no window chrome - full control!)
    launcher.brush->fill(PalmyraOS::Color::Gray800);

    // Draw app items as simple clickable text
    int yPos                = 2;  // Minimal top padding (2px from actual top - no chrome!)
    bool anyAppClicked      = false;

    // Get mouse event for click detection
    launcher.lastMouseEvent = nextMouseEvent(launcher.windowID);

    for (int i = 0; i < launcher.appCount; i++) {
        const char* appName = launcher.apps[i].name;
        const char* appPath = launcher.apps[i].path;

        // Calculate hover area
        int textY           = yPos;
        int textHeight      = 12;                   // Arial12 font height
        int textWidth       = strlen(appName) * 8;  // Approximate width

        bool isHovering     = launcher.lastMouseEvent.x >= 10 && launcher.lastMouseEvent.x < (10 + textWidth) && launcher.lastMouseEvent.y >= textY &&
                          launcher.lastMouseEvent.y < (textY + textHeight);

        // Choose color based on hover state
        PalmyraOS::Color textColor = isHovering ? PalmyraOS::Color::PrimaryLight : PalmyraOS::Color::White;

        // Check for click
        if (isHovering && launcher.lastMouseEvent.isEvent && launcher.lastMouseEvent.isLeftDown) {
            printf("[MenuBar] Launching app: %s (%s)\n", appName, appPath);
            anyAppClicked = true;
            textColor     = PalmyraOS::Color::PrimaryDark;

            // Launch the selected app using posix_spawn
            char* argv[]  = {const_cast<char*>(appPath), nullptr};
            uint32_t child_pid;
            int status = posix_spawn(&child_pid, appPath, nullptr, nullptr, argv, nullptr);

            if (status == 0) { printf("[MenuBar] Successfully spawned app (PID: %d)\n", child_pid); }
            else { printf("[MenuBar] Failed to spawn app (status: %d)\n", status); }

            // Close the launcher after launching an app
            closeAppLauncher(launcher);
            return;
        }

        // Draw text
        launcher.textRenderer->setCursor(10, textY);
        *launcher.textRenderer << textColor << appName;

        yPos += launcher.itemHeight;
    }

    // Swap buffers
    launcher.frameBuffer->swapBuffers();

    // Get window status
    palmyra_window_status status = getStatus(launcher.windowID);

    // Close if:
    // 1. Window is no longer active (lost focus)
    // 2. User clicks outside the window (isLeftDown but didn't click any app)
    if (!status.isActive) { closeAppLauncher(launcher); }
    else if (launcher.lastMouseEvent.isEvent && launcher.lastMouseEvent.isLeftDown && !anyAppClicked) { closeAppLauncher(launcher); }
}

// Close and cleanup the app launcher
void closeAppLauncher(AppLauncherState& launcher) {
    if (!launcher.isOpen) return;

    printf("[MenuBar] Closing App Launcher\n");

    // Manually call destructors then free memory (no delete operator in bare metal)
    if (launcher.textRenderer) {
        launcher.textRenderer->~TextRenderer();
        free(launcher.textRenderer);
        launcher.textRenderer = nullptr;
    }
    if (launcher.brush) {
        launcher.brush->~Brush();
        free(launcher.brush);
        launcher.brush = nullptr;
    }
    if (launcher.frameBuffer) {
        launcher.frameBuffer->~FrameBuffer();
        free(launcher.frameBuffer);
        launcher.frameBuffer = nullptr;
    }

    // Close window and free buffers
    if (launcher.windowID) {
        closeWindow(launcher.windowID);
        launcher.windowID = 0;
    }
    if (launcher.backBuffer) {
        free(launcher.backBuffer);
        launcher.backBuffer = nullptr;
    }

    launcher.frontBuffer = nullptr;
    launcher.isOpen      = false;
    printf("[MenuBar] App Launcher closed and memory freed\n");
}

[[noreturn]] int PalmyraOS::Userland::builtin::MenuBar::main(uint32_t argc, char** argv) {

    // Define dimensions and required memory for the window
    palmyra_window w      = {.x = 0, .y = 0, .width = 1920, .height = 20, .movable = false, .title = "MenuBar"};
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

    // ============ APP LAUNCHER STATE ============
    AppLauncherState launcher;
    bool appsButtonHovered = false;

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
        currentFPS            = lastValidFPS;

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

        // ============ HANDLE MOUSE EVENTS ============
        MouseEvent mouseEvent = nextMouseEvent(bufferId);
        int mouseX            = mouseEvent.x;
        int mouseY            = mouseEvent.y;
        int appsButtonX       = 120;
        int appsButtonWidth   = 40;

        // Check if mouse is over "Apps" button (positioned at x=80-120, y=0-20)
        appsButtonHovered     = (mouseX >= appsButtonX && mouseX <= appsButtonX + appsButtonWidth && mouseY >= 0 && mouseY <= 20);

        // Check for clicks on "Apps" button
        if (appsButtonHovered && mouseEvent.isLeftDown) {
            // Open the app launcher (non-blocking, updates per-frame)
            openAppLauncher(launcher);
        }

        brush.fill(PalmyraOS::Color::Black);
        brush.drawHLine(0, w.width, w.height - 1, PalmyraOS::Color::Gray100);
        brush.drawHLine(0, w.width, w.height - 2, PalmyraOS::Color::Gray500);

        // Render the logo and system information
        // Determine Apps button color based on hover state
        PalmyraOS::Color appsButtonColor = appsButtonHovered ? PalmyraOS::Color::PrimaryLight : PalmyraOS::Color::Primary;

        textRenderer << Color::Orange << "Palmyra" << Color::LighterBlue << "OS ";
        textRenderer << Color::Gray100 << "v0.01\t";

        textRenderer.setCursor(appsButtonX, 0);
        textRenderer << appsButtonColor << " Apps " << Color::Gray100;
        // textRenderer << "SysTime: " << time_spec.tv_sec << " s\t";
        // textRenderer << "Frames: " << count << "\t";

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

        // ============ UPDATE APP LAUNCHER (per-frame, non-blocking) ============
        updateAppLauncher(launcher);

        // Yield to other processes
        sched_yield();
    }

    // Cleanup: Free allocated memory and close device
    closeAppLauncher(launcher);  // Clean up launcher resources
    close(epochTime_fd);
}

#include "userland/systemWidgets/ImageViewer.h"

// Include our custom headers
#include "libs/string.h"
#include "palmyraOS/errono.h"
#include "palmyraOS/palmyraSDK.h"
#include "palmyraOS/shared/memory/HeapAllocator.h"
#include "palmyraOS/stdio.h"
#include "palmyraOS/stdlib.h"
#include "palmyraOS/unistd.h"

namespace PalmyraOS::Userland::builtin::ImageViewer {

    constexpr size_t MAX_IMAGE_WIDTH  = 640;
    constexpr size_t MAX_IMAGE_HEIGHT = 480;
    constexpr size_t MAX_IMAGE_BYTES  = 16 * 1024 * 1024;

    struct ImageData {
        unsigned char* pixels;
        int width;
        int height;
        int channels;
    };

    static void extractFileName(const char* fullPath, char* fileName, size_t maxSize) {
        const char* lastSlash = fullPath;
        const char* current   = fullPath;

        while (*current != '\0') {
            if (*current == '/') { lastSlash = current; }
            current++;
        }

        if (*lastSlash == '/') { lastSlash++; }
        strncpy(fileName, lastSlash, maxSize - 1);
        fileName[maxSize - 1] = '\0';
    }

    /**
     * Load a BMP image file from disk into memory
     *
     * BMP (Bitmap) File Format:
     * ==========================
     * BMP files have a simple structure:
     * 1. File Header (14 bytes): Contains file signature and pixel data offset
     * 2. DIB Header (40 bytes for BITMAPINFOHEADER): Contains image dimensions, color depth, etc.
     * 3. Optional Color Table: Only present for images with < 24 bits per pixel
     * 4. Pixel Data: Raw pixel values stored row by row
     *
     * Pixel Storage:
     * - BMP stores pixels as BGR (Blue-Green-Red), not RGB!
     * - For 24-bit BMP: 3 bytes per pixel (B, G, R)
     * - For 32-bit BMP: 4 bytes per pixel (B, G, R, A)
     * - Rows are padded to 4-byte boundaries (important for performance)
     * - Images are typically stored bottom-up (first row in file = bottom row of image)
     *
     * @param filePath Path to the BMP file
     * @param width Output parameter for image width
     * @param height Output parameter for image height
     * @return Pointer to RGBA pixel data (4 bytes per pixel) or nullptr on error
     */
    static unsigned char* loadBMPImage(const char* filePath, int* width, int* height) {
        int fd = ::open(filePath, O_RDONLY);
        if (fd < 0) return nullptr;

        // ============================================
        // STEP 1: Read BMP File Header (14 bytes)
        // ============================================
        // BMP File Header Structure:
        // Bytes 0-1:   Signature ('BM' = 0x42 0x4D)
        // Bytes 2-5:   File size (little-endian)
        // Bytes 6-7:   Reserved (unused)
        // Bytes 8-9:   Reserved (unused)
        // Bytes 10-13: Pixel data offset (where pixel data starts in file)
        unsigned char header[14];
        if (!read(fd, header, 14)) {
            close(fd);
            return nullptr;
        }

        // Verify BMP signature - must be 'BM' (0x42 0x4D)
        // This distinguishes BMP from other formats like PNG (0x89 0x50) or JPEG (0xFF 0xD8)
        if (header[0] != 'B' || header[1] != 'M') {
            // Identify actual format
            if (header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47) { printf("ImageViewer: File is PNG - not supported (BMP only)\n"); }
            else if (header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF) { printf("ImageViewer: File is JPEG - not supported (BMP only)\n"); }
            else { printf("ImageViewer: Unsupported format - bytes: %02x %02x %02x %02x\n", header[0], header[1], header[2], header[3]); }
            close(fd);
            return nullptr;
        }

        // ============================================
        // STEP 2: Read DIB Header (40 bytes)
        // ============================================
        // DIB (Device-Independent Bitmap) Header Structure:
        // Bytes 0-3:   Header size (should be 40 for BITMAPINFOHEADER)
        // Bytes 4-7:   Image width (little-endian, signed!)
        // Bytes 8-11:  Image height (little-endian, signed - negative = top-down)
        // Bytes 12-13: Color planes (must be 1)
        // Bytes 14-15: Bits per pixel (1, 4, 8, 16, 24, or 32)
        // Bytes 16-19: Compression type (0 = uncompressed, 1 = RLE8, 2 = RLE4)
        // Bytes 20-23: Image size (can be 0 for uncompressed)
        // Bytes 24-27: Horizontal resolution (pixels per meter)
        // Bytes 28-31: Vertical resolution (pixels per meter)
        // Bytes 32-35: Colors in palette (0 = 2^bitsPerPixel)
        // Bytes 36-39: Important colors (0 = all)
        unsigned char dibHeader[40];
        if (!read(fd, dibHeader, 40)) {
            close(fd);
            return nullptr;
        }

        // Verify DIB header size - must be 40 bytes for standard BITMAPINFOHEADER
        // Other sizes (108, 124) indicate newer formats we don't support
        uint32_t dibHeaderSize = dibHeader[0] | (dibHeader[1] << 8) | (dibHeader[2] << 16) | (dibHeader[3] << 24);
        if (dibHeaderSize != 40) {
            printf("ImageViewer: Unsupported DIB header size: %u\n", dibHeaderSize);
            close(fd);
            return nullptr;
        }

        // Extract image dimensions and color depth
        // BMP uses LITTLE-ENDIAN byte order (least significant byte first)
        // Example: For width = 126, bytes are: [126, 0, 0, 0]
        // We read: byte[0] | (byte[1] << 8) | (byte[2] << 16) | (byte[3] << 24)
        *width                  = dibHeader[4] | (dibHeader[5] << 8) | (dibHeader[6] << 16) | (dibHeader[7] << 24);
        int heightSigned        = dibHeader[8] | (dibHeader[9] << 8) | (dibHeader[10] << 16) | (dibHeader[11] << 24);
        unsigned short bitCount = dibHeader[14] | (dibHeader[15] << 8);

        // Check compression type - we only support uncompressed BMPs
        // 0 = BI_RGB (uncompressed)
        // 1 = BI_RLE8 (run-length encoded, 8-bit)
        // 2 = BI_RLE4 (run-length encoded, 4-bit)
        uint32_t compression    = dibHeader[16] | (dibHeader[17] << 8) | (dibHeader[18] << 16) | (dibHeader[19] << 24);
        if (compression != 0) {
            printf("ImageViewer: Warning: Compression type %u (0=uncompressed, 1=RLE8, 2=RLE4)\n", compression);
            printf("ImageViewer: Compressed BMP not supported!\n");
            close(fd);
            return nullptr;
        }
        printf("ImageViewer: Compression: %u (0=uncompressed)\n", compression);

        // Handle BMP storage direction
        // Negative height = top-down BMP (rare, used for some formats)
        // Positive height = bottom-up BMP (standard, first row in file = bottom row of image)
        bool isTopDown = (heightSigned < 0);
        *height        = (heightSigned < 0) ? -heightSigned : heightSigned;

        if (*width <= 0 || *height <= 0 || *width > (int) MAX_IMAGE_WIDTH || *height > (int) MAX_IMAGE_HEIGHT) {
            close(fd);
            return nullptr;
        }
        if (bitCount != 24 && bitCount != 32) {
            close(fd);
            return nullptr;
        }

        // ============================================
        // STEP 3: Allocate memory for pixel data
        // ============================================
        // We store pixels as RGBA (4 bytes per pixel) regardless of source format
        // This makes rendering easier - we always have Red, Green, Blue, Alpha channels
        // Memory layout: [R0,G0,B0,A0, R1,G1,B1,A1, ...] for each row
        size_t pixelDataSize = (size_t) (*width) * (*height) * 4;
        if (pixelDataSize > MAX_IMAGE_BYTES) {
            close(fd);
            return nullptr;
        }

        unsigned char* pixels = (unsigned char*) malloc(pixelDataSize);
        if (!pixels) {
            close(fd);
            return nullptr;
        }

        // ============================================
        // STEP 4: Seek to pixel data start
        // ============================================
        // The pixel offset tells us where the actual pixel data begins in the file
        // For standard 24-bit BMP: offset = 54 (14 byte header + 40 byte DIB header)
        // For BMPs with color tables: offset > 54
        // BMP header stores offset in little-endian format (bytes 10-13)
        unsigned int pixelOffset = header[10] | (header[11] << 8) | (header[12] << 16) | (header[13] << 24);
        printf("ImageViewer: Pixel offset: %u, width=%d, height=%d, bitCount=%d\n", pixelOffset, *width, *height, bitCount);

        // Verify we're at the right position after reading DIB header
        long currentPos = lseek(fd, 0, SEEK_CUR);
        printf("ImageViewer: Current file position after DIB header: %d (should be 54)\n", (int) currentPos);

        // Check if there's a gap between header end and pixel data
        // This gap would contain a color table (palette) for images with < 24 bits per pixel
        if (pixelOffset > (unsigned int) currentPos) {
            printf("ImageViewer: Warning: Gap of %u bytes between header (%d) and pixel data (%u)\n", pixelOffset - currentPos, (int) currentPos, pixelOffset);
            printf("ImageViewer: This might be a color table or other metadata\n");
        }

        // Seek to pixel data start
        if (lseek(fd, pixelOffset, SEEK_SET) != pixelOffset) {
            printf("ImageViewer: Failed to seek to pixel data\n");
            free(pixels);
            close(fd);
            return nullptr;
        }

        // Debug: Read first 6 bytes to verify we're at pixel data
        unsigned char testBytes[6];
        if (read(fd, testBytes, 6) == 6) {
            printf("ImageViewer: First 6 bytes at pixel offset: %02x %02x %02x %02x %02x %02x\n",
                   testBytes[0],
                   testBytes[1],
                   testBytes[2],
                   testBytes[3],
                   testBytes[4],
                   testBytes[5]);
            printf("ImageViewer: First pixel should be: B=%d G=%d R=%d (for pure red)\n", testBytes[0], testBytes[1], testBytes[2]);
            // Seek back to start of pixel data
            lseek(fd, pixelOffset, SEEK_SET);
        }

        // ============================================
        // STEP 5: Calculate row size with padding
        // ============================================
        // BMP rows are padded to 4-byte boundaries for performance (CPU alignment)
        // Formula: ((width * bitsPerPixel + 31) / 32) * 4
        // Example: width=126, bitsPerPixel=24:
        //   - Unpadded: 126 * 3 = 378 bytes per row
        //   - Padding: round up to multiple of 4 = 380 bytes
        //   - Formula: ((126*24 + 31) / 32) * 4 = (3024 + 31) / 32 * 4 = 95 * 4 = 380 bytes
        // The padding bytes are ignored when reading pixels
        size_t rowSize = ((*width * bitCount + 31) / 32) * 4;
        printf("ImageViewer: Row size: %zu bytes (unpadded: %d bytes)\n", rowSize, (*width * bitCount / 8));

        // Get file size to verify we don't read past end
        long fileSize = lseek(fd, 0, SEEK_END);
        if (fileSize < 0) {
            printf("ImageViewer: Warning: Cannot get file size (lseek returned %d), proceeding without size check\n", (int) fileSize);
            fileSize = 0;  // Set to 0 to skip size check
        }
        else { printf("ImageViewer: File size: %d bytes\n", (int) fileSize); }
        size_t expectedPixelDataSize = (size_t) (*height) * rowSize;
        printf("ImageViewer: Expected pixel data size: %zu bytes (from offset %u)\n", expectedPixelDataSize, pixelOffset);
        printf("ImageViewer: Total bytes needed: %zu (offset %u + pixel data %zu)\n", pixelOffset + expectedPixelDataSize, pixelOffset, expectedPixelDataSize);
        // Only check file size if we successfully got it
        if (fileSize > 0 && pixelOffset + expectedPixelDataSize > (size_t) fileSize) {
            printf("ImageViewer: ERROR: File too small! Need %zu bytes but file is only %d bytes\n", pixelOffset + expectedPixelDataSize, (int) fileSize);
            printf("ImageViewer: Missing %d bytes\n", (int) ((long) (pixelOffset + expectedPixelDataSize) - fileSize));
            free(pixels);
            close(fd);
            return nullptr;
        }
        lseek(fd, pixelOffset, SEEK_SET);  // Reset to pixel data start

        unsigned char* rowBuffer = (unsigned char*) malloc(rowSize);
        if (!rowBuffer) {
            free(pixels);
            close(fd);
            return nullptr;
        }

        // ============================================
        // STEP 6: Read pixel data row by row
        // ============================================
        // BMP files store pixel data row by row, but the order depends on the format:
        // - Bottom-up (standard): First row in file = bottom row of image
        // - Top-down (rare): First row in file = top row of image (height is negative)
        //
        // We need to read rows in the correct order and convert from BGR to RGBA:
        // - BMP format: [B, G, R] for 24-bit or [B, G, R, A] for 32-bit
        // - Our format: [R, G, B, A] always (RGBA)
        //
        // Row reading process:
        // 1. Read entire row (including padding) into temporary buffer
        // 2. Extract each pixel from the row buffer
        // 3. Convert BGR -> RGBA and store in final pixel array
        // 4. Skip padding bytes (they're not part of the image)

        // Read rows in correct order based on BMP format
        if (isTopDown) {
            // Top-down BMP: Rows are stored in file in same order as displayed
            // Read from y=0 (top) to y=height-1 (bottom)
            for (int y = 0; y < *height; y++) {
                if (!read(fd, rowBuffer, rowSize)) {
                    free(rowBuffer);
                    free(pixels);
                    close(fd);
                    return nullptr;
                }
                // Debug: Print first row's first pixel raw bytes
                if (y == 0) { printf("ImageViewer: First row, first pixel raw bytes: B=%d G=%d R=%d\n", rowBuffer[0], rowBuffer[1], rowBuffer[2]); }
                for (int x = 0; x < *width; x++) {
                    // Calculate destination: pixels[y][x] in RGBA format
                    unsigned char* dst = pixels + ((y * *width + x) * 4);
                    // Calculate source: rowBuffer[x] in BGR format
                    // For 24-bit: 3 bytes per pixel, for 32-bit: 4 bytes per pixel
                    unsigned char* src = rowBuffer + (x * (bitCount / 8));
                    // Convert BGR to RGBA:
                    dst[0]             = src[2];                           // R <- BGR[2] (Red)
                    dst[1]             = src[1];                           // G <- BGR[1] (Green)
                    dst[2]             = src[0];                           // B <- BGR[0] (Blue)
                    dst[3]             = (bitCount == 32) ? src[3] : 255;  // A <- BGR[3] if 32-bit, else 255 (opaque)
                }
            }
        }
        else {
            // Bottom-up BMP (standard): Rows are stored in file in reverse order
            // First row in file = bottom row of image (y = height-1)
            // Last row in file = top row of image (y = 0)
            // We read from y=height-1 down to y=0 to get correct visual order
            for (int y = *height - 1; y >= 0; y--) {
                long rowStartPos = lseek(fd, 0, SEEK_CUR);
                // Clear row buffer to avoid garbage data from previous reads
                for (size_t i = 0; i < rowSize; i++) { rowBuffer[i] = 0; }

                // Read row data - handle partial reads (some file systems may return partial data)
                // This loop ensures we read the complete row even if read() returns fewer bytes
                int totalBytesRead = 0;
                while (totalBytesRead < (int) rowSize) {
                    int bytesRead = read(fd, rowBuffer + totalBytesRead, rowSize - totalBytesRead);
                    if (bytesRead <= 0) {
                        // EOF or error - we didn't get all the bytes we need
                        long currentPos = lseek(fd, 0, SEEK_CUR);
                        printf("ImageViewer: ERROR: Failed to read row %d: read %d bytes, expected %zu\n", y, totalBytesRead, rowSize);
                        printf("ImageViewer: Row started at offset %d, current position %d\n", (int) rowStartPos, (int) currentPos);
                        if (fileSize > 0) { printf("ImageViewer: File size is %d, remaining bytes: %d\n", (int) fileSize, (int) (fileSize - rowStartPos)); }
                        else { printf("ImageViewer: File size unknown (lseek SEEK_END not supported)\n"); }
                        free(rowBuffer);
                        free(pixels);
                        close(fd);
                        return nullptr;
                    }
                    totalBytesRead += bytesRead;
                }
                // Debug: Print first and last rows
                if (y == *height - 1) {
                    printf("ImageViewer: First row read (bottom of image) at offset %d, first pixel: B=%d G=%d R=%d\n",
                           (int) rowStartPos,
                           rowBuffer[0],
                           rowBuffer[1],
                           rowBuffer[2]);
                }
                if (y == 0) {
                    printf("ImageViewer: Last row read (top of image) at offset %d, first pixel: B=%d G=%d R=%d\n", (int) rowStartPos, rowBuffer[0], rowBuffer[1], rowBuffer[2]);
                }

                // Extract pixels from row buffer and convert BGR -> RGBA
                for (int x = 0; x < *width; x++) {
                    // Calculate destination: pixels[y][x] in RGBA format
                    // Note: y is the image row (0=top, height-1=bottom), x is the column
                    unsigned char* dst = pixels + ((y * *width + x) * 4);
                    // Calculate source: rowBuffer[x] in BGR format
                    // For 24-bit: offset = x * 3, for 32-bit: offset = x * 4
                    unsigned char* src = rowBuffer + (x * (bitCount / 8));
                    // Convert BGR to RGBA:
                    // BMP stores: [Blue, Green, Red] or [Blue, Green, Red, Alpha]
                    // We store:   [Red, Green, Blue, Alpha]
                    dst[0]             = src[2];                           // R <- BGR[2] (Red is 3rd byte)
                    dst[1]             = src[1];                           // G <- BGR[1] (Green is 2nd byte)
                    dst[2]             = src[0];                           // B <- BGR[0] (Blue is 1st byte)
                    dst[3]             = (bitCount == 32) ? src[3] : 255;  // A <- BGR[3] if 32-bit, else 255 (fully opaque)
                }
            }
        }

        free(rowBuffer);
        close(fd);

        // Debug: Print first few pixels to verify conversion
        if (*width > 0 && *height > 0) {
            unsigned char* firstPixel = pixels;
            printf("ImageViewer: First pixel: R=%d G=%d B=%d A=%d\n", firstPixel[0], firstPixel[1], firstPixel[2], firstPixel[3]);
            unsigned char* midPixel = pixels + ((*height / 2 * *width + *width / 2) * 4);
            printf("ImageViewer: Middle pixel: R=%d G=%d B=%d A=%d\n", midPixel[0], midPixel[1], midPixel[2], midPixel[3]);
        }

        // Return pointer to RGBA pixel data
        // Memory layout: [R0,G0,B0,A0, R1,G1,B1,A1, ..., R(N-1),G(N-1),B(N-1),A(N-1)]
        // Where N = width * height
        // Pixel at (x,y) is at: pixels[(y * width + x) * 4]
        // Caller is responsible for freeing this memory with free()
        return pixels;
    }

    /**
     * Safely load an image file and return an ImageData structure
     *
     * This function wraps loadBMPImage() and handles:
     * - File existence checking
     * - Memory allocation for ImageData structure
     * - Error handling and cleanup
     *
     * @param filePath Path to the BMP image file
     * @return Pointer to ImageData structure or nullptr on error
     *         Caller must call freeImageData() to free the memory
     */
    static ImageData* loadImageSafely(const char* filePath) {
        printf("ImageViewer: Loading image from %s\n", filePath);

        int fd = ::open(filePath, O_RDONLY);
        if (fd < 0) {
            printf("ImageViewer: File not found\n");
            return nullptr;
        }
        close(fd);

        ImageData* imgData = (ImageData*) malloc(sizeof(ImageData));
        if (!imgData) {
            printf("ImageViewer: Failed to allocate ImageData\n");
            return nullptr;
        }

        imgData->pixels   = loadBMPImage(filePath, &imgData->width, &imgData->height);
        imgData->channels = 4;

        if (!imgData->pixels) {
            printf("ImageViewer: Failed to load image\n");
            free(imgData);
            return nullptr;
        }

        printf("ImageViewer: Image loaded successfully (%dx%d)\n", imgData->width, imgData->height);
        return imgData;
    }

    static void freeImageData(ImageData* imgData) {
        if (!imgData) return;
        if (imgData->pixels) { free(imgData->pixels); }
        free(imgData);
    }

    int main(uint32_t argc, char** argv) {
        if (argc < 2) {
            printf("Usage: imgview <image_file>\n");
            _exit(1);
        }

        const char* imagePath = argv[1];

        // Extract filename from path for window title
        char fileName[256]    = {0};
        extractFileName(imagePath, fileName, sizeof(fileName));

        char windowTitle[512] = {0};
        snprintf(windowTitle, sizeof(windowTitle), "Image Viewer - %s", fileName);

        SDK::Window window(100, 100, 640, 480, true, windowTitle);
        SDK::WindowGUI windowGui(window);
        windowGui.setBackground(Color::Black);

        // Show loading message immediately
        // ================================
        // Initialize the framebuffer and display "Loading Image..." while we load the file
        // This prevents the window from appearing black during loading
        windowGui.render();

        // Get framebuffer size to center the loading message
        auto [fbW, fbH] = windowGui.getFrameBufferSize();
        int centerX     = (int) fbW / 2;
        int centerY     = (int) fbH / 2;

        // Display loading message centered on screen
        windowGui.text().setCursor(centerX - 60, centerY);
        windowGui.text() << PalmyraOS::Color::Gray600 << "Loading Image...";
        windowGui.swapBuffers();  // Present the loading frame immediately

        // Load image with memory safety (this may take a moment)
        ImageData* imageData = loadImageSafely(imagePath);
        bool imageLoaded     = (imageData != nullptr && imageData->pixels != nullptr);

        // Main rendering loop
        // ====================
        // The image viewer continuously renders frames:
        // 1. Clear and draw window chrome (title bar, borders, etc.)
        // 2. Draw image information text
        // 3. Draw image pixels pixel-by-pixel
        // 4. Present the frame to the screen
        // 5. Yield to other processes
        while (true) {
            // Prepare frame: Clear framebuffer and draw window chrome (title bar, borders)
            // This must be called first to set up the drawing context
            windowGui.render();

            // Get framebuffer dimensions for layout calculations
            auto [fbW, fbH] = windowGui.getFrameBufferSize();
            const int maxX  = (int) fbW;
            const int maxY  = (int) fbH;

            if (imageLoaded) {
                // ============================================
                // Render image pixels centered in framebuffer
                // ============================================
                // We draw the image pixel-by-pixel using the brush API
                // Each pixel is drawn at a specific screen coordinate
                //
                // Image coordinate system:
                // - (0,0) is top-left corner of image
                // - x increases to the right
                // - y increases downward
                //
                // Screen coordinate system:
                // - offsetX, offsetY: Top-left corner of image on screen (centered)
                // - dx, dy: Screen coordinates where pixel is drawn
                // - We clip pixels that would go outside the framebuffer

                // Reserve space at bottom for text (approximately 60 pixels for 2 lines + padding)
                const int textAreaHeight  = 60;
                const int availableHeight = maxY - textAreaHeight;

                // Calculate centered position for image
                // Center horizontally: (framebuffer_width - image_width) / 2
                // Center vertically: (available_height - image_height) / 2
                const int offsetX         = (maxX - imageData->width) / 2;
                const int offsetY         = (availableHeight - imageData->height) / 2;

                // Iterate through each pixel in the image
                for (int y = 0; y < imageData->height; ++y) {
                    int dy = offsetY + y;              // Screen Y coordinate (centered)
                    if (dy < 0) continue;              // Clip: Skip if above framebuffer
                    if (dy >= availableHeight) break;  // Clip: Don't draw if in text area or below

                    for (int x = 0; x < imageData->width; ++x) {
                        int dx = offsetX + x;   // Screen X coordinate (centered)
                        if (dx < 0) continue;   // Clip: Skip if outside left edge
                        if (dx >= maxX) break;  // Clip: Don't draw if outside right edge

                        // Calculate pixel address in RGBA format
                        // Memory layout: [R0,G0,B0,A0, R1,G1,B1,A1, ...]
                        // For pixel at (x,y): offset = (y * width + x) * 4
                        unsigned char* p = imageData->pixels + ((y * imageData->width + x) * 4);

                        // Extract RGBA components:
                        // p[0] = Red, p[1] = Green, p[2] = Blue, p[3] = Alpha
                        // Create Color object and draw pixel at screen position (dx, dy)
                        PalmyraOS::Color c(p[0], p[1], p[2], p[3]);
                        windowGui.brush().drawPoint((uint32_t) dx, (uint32_t) dy, c);
                    }
                }

                // ============================================
                // Draw text information at the bottom
                // ============================================
                // Position text at the bottom of the window
                // Use a small margin from the bottom edge
                const int bottomMargin = 10;
                const int leftMargin   = 10;
                const int rightMargin  = 10;
                int textY              = maxY - textAreaHeight + bottomMargin;

                windowGui.text().setCursor(leftMargin, textY);

                // Show filename instead of full path (much shorter)
                char displayFileName[256] = {0};
                extractFileName(imagePath, displayFileName, sizeof(displayFileName));

                // Use a conservative maximum width for text (leave plenty of margin)
                const int maxTextWidth     = maxX - leftMargin - rightMargin;

                // Truncate filename if needed - account for "File: " prefix (6 chars)
                // Most monospace fonts are ~8-10 pixels wide, so use conservative estimate of 10 pixels/char
                const int prefixLength     = 6;                                       // "File: "
                const int maxFilenameChars = (maxTextWidth / 10) - prefixLength - 3;  // -3 for "..."
                if (maxFilenameChars > 0 && strlen(displayFileName) > (size_t) maxFilenameChars) {
                    // Truncate and add ellipsis
                    char truncated[256];
                    strncpy(truncated, displayFileName, maxFilenameChars);
                    truncated[maxFilenameChars] = '\0';
                    windowGui.text() << PalmyraOS::Color::Gray600 << "File: " << truncated << "...  ";
                }
                else { windowGui.text() << PalmyraOS::Color::Gray600 << "File: " << displayFileName << "  "; }

                // Build image info string - always show dimensions, conditionally show channels
                char imageInfo[64];

                // Build dimensions string first
                snprintf(imageInfo, sizeof(imageInfo), "Image: %dx%d", imageData->width, imageData->height);

                // Only add channels if there's plenty of room (conservative: 12 pixels per char)
                const int maxInfoChars  = (maxTextWidth / 12);
                const int currentLength = strlen(imageInfo);

                // Add channels only if we have at least 20 characters of room left
                if (currentLength + 20 <= maxInfoChars) {
                    // We have room, try adding channels
                    char withChannels[64];
                    snprintf(withChannels, sizeof(withChannels), "Image: %dx%d (%d channels)", imageData->width, imageData->height, imageData->channels);

                    // Only use it if it fits
                    if (strlen(withChannels) <= (size_t) maxInfoChars) {
                        strncpy(imageInfo, withChannels, sizeof(imageInfo) - 1);
                        imageInfo[sizeof(imageInfo) - 1] = '\0';
                    }
                }
                // Otherwise, imageInfo already has just dimensions (which is safe)

                windowGui.text() << PalmyraOS::Color::Gray600 << imageInfo;
            }
            else {
                // If image failed to load, show error message centered
                const int centerX = maxX / 2;
                const int centerY = maxY / 2;
                windowGui.text().setCursor(centerX - 80, centerY);
                windowGui.text() << PalmyraOS::Color::Red << "Failed to load image\n";
            }

            // Present the frame: Swap buffers and poll for window events
            // This displays the rendered frame and handles user input (close button, etc.)
            windowGui.swapBuffers();

            // Yield CPU to other processes
            sched_yield();
        }

        freeImageData(imageData);
        return 0;
    }
}  // namespace PalmyraOS::Userland::builtin::ImageViewer

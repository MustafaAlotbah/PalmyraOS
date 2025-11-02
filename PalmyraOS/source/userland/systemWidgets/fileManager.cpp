
#include "userland/systemWidgets/fileManager.h"
#include <algorithm>
#include <elf.h>

#include "palmyraOS/palmyraSDK.h"  // Window, Window Frame


namespace PalmyraOS::Userland::builtin::fileManager {
    enum class EntryType { Directory, Invalid, Archive, Elf32, Elf64, ElfLib };

    struct DirectoryEntry {
        types::UString<char> name;
        EntryType dentryType;
        uint32_t size;
    };

    int fetchContent(types::UserHeapManager& heap, types::UVector<types::UString<char>>& currentDirectory, types::UVector<DirectoryEntry>& content);

    void pushArchive(types::UserHeapManager& heap,
                     types::UVector<DirectoryEntry>& content,
                     const types::UString<char>& parentDirectory,
                     const types::UString<char>& entryName);

}  // namespace PalmyraOS::Userland::builtin::fileManager

int PalmyraOS::Userland::builtin::fileManager::main(uint32_t argc, char** argv) {
    // Initialize dynamic memory allocator for the application
    types::UserHeapManager heap;

    // Create and set up the main application window
    SDK::Window window(400, 320, 480, 360, true, "Palmyra File Manager");
    SDK::WindowGUI windowGui(window);

    // Globals
    types::UVector<types::UString<char>> currentDirectory(heap);
    types::UVector<DirectoryEntry> content(heap);
    fetchContent(heap, currentDirectory, content);

    // layout
    int scrollY_content = 0;

    while (true) {
        // Display Current Directory
        {
            SDK::Layout layout(windowGui, nullptr, false, 20);
            windowGui.brush().fillRectangle(layout.getX(),
                                            layout.getY(),
                                            layout.getX() + layout.getWidth(),
                                            layout.getY() + layout.getHeight(),
                                            Color::DarkerGray);

            // Root directory link
            if (windowGui.link("root")) {
                currentDirectory.clear();
                content.clear();
                fetchContent(heap, currentDirectory, content);
                continue;
            }
            windowGui.text() << "/";

            // Render current directory path

            for (int i = 0; i < currentDirectory.size(); ++i) {
                if (windowGui.link(currentDirectory[i].c_str())) {
                    // Go back to the selected directory
                    currentDirectory.erase(currentDirectory.begin() + i + 1, currentDirectory.end());
                    content.clear();
                    fetchContent(heap, currentDirectory, content);
                    break;
                }
                windowGui.text() << "/";
            }
        }

        // Display Content
        {
            SDK::Layout layout(windowGui, &scrollY_content, true);

            uint32_t contentIndex = 0;

            // File Names column
            int maxFilesOffset    = 0;
            int maxTypesOffset    = 0;
            int maxSizesOffset    = 0;
            for (auto& [item, type, size]: content) {
                if (contentIndex++ % 2 == 0) { windowGui.fillRectangle(0, windowGui.text().getCursorY() + 1, layout.getWidth(), 16, Color::DarkerGray); }

                if (type == EntryType::Directory) {
                    // Handle Directory here
                    if (windowGui.link(item.c_str())) {
                        currentDirectory.push_back(item);
                        content.clear();
                        int result = fetchContent(heap, currentDirectory, content);
                        break;
                    }
                    windowGui.text() << "/";
                }
                else if (type == EntryType::Elf32) {
                    // Handle Executables here

                    if (windowGui.link(item.c_str(), false, Color::Red600, Color::Red300, Color::Red900)) {
                        // Construct the directory path
                        char directoryBuffer[512];
                        int offset = SDK::constructDirectoryPath(directoryBuffer, sizeof(directoryBuffer), currentDirectory);
                        if (offset < 0) continue;  // Handle buffer overflow error
                        strcpy(directoryBuffer + offset, item.c_str());

                        char* argv_[] = {const_cast<char*>("/bin/terminal.elf"), const_cast<char*>("exec"), const_cast<char*>(directoryBuffer), nullptr};

                        // Spawn the new process
                        uint32_t child_pid;
                        int status = posix_spawn(&child_pid, "/bin/terminal.elf", nullptr, nullptr, argv_, nullptr);
                    }
                }
                else {
                    if (windowGui.link(item.c_str(), false, Color::Gray300, Color::Gray100, Color::Gray500)) {
                        // Construct the directory path
                        char directoryBuffer[512];
                        int offset = SDK::constructDirectoryPath(directoryBuffer, sizeof(directoryBuffer), currentDirectory);
                        if (offset < 0) continue;  // Handle buffer overflow error
                        strcpy(directoryBuffer + offset, item.c_str());

                        char* argv[] = {const_cast<char*>("/bin/terminal.elf"), const_cast<char*>("cat"), const_cast<char*>(directoryBuffer), nullptr};

                        // Spawn the new process
                        uint32_t child_pid;
                        int status = posix_spawn(&child_pid, "/bin/terminal.elf", nullptr, nullptr, argv, nullptr);
                    }
                }

                maxFilesOffset = std::max(maxFilesOffset, windowGui.text().getCursorX());
                windowGui.text() << "\n";
            }

            // Second Column
            windowGui.text().setCursor(maxFilesOffset, scrollY_content);
            for (auto& [item, type, size]: content) {
                if (type == EntryType::Directory) {
                    windowGui.text().setCursor(maxFilesOffset, windowGui.text().getCursorY());
                    windowGui.text() << "Directory";
                }
                else if (type == EntryType::Elf32) {
                    windowGui.text().setCursor(maxFilesOffset, windowGui.text().getCursorY());
                    windowGui.text() << "Elf32";
                }
                else {
                    windowGui.text().setCursor(maxFilesOffset, windowGui.text().getCursorY());
                    windowGui.text() << "Archive";
                }
                maxTypesOffset = std::max(maxTypesOffset, windowGui.text().getCursorX());
                windowGui.text() << "\n";
            }

            // Third Column - File Sizes
            windowGui.text().setCursor(maxTypesOffset + 15, scrollY_content);
            for (auto& [item, type, size]: content) {
                windowGui.text().setCursor(maxTypesOffset + 15, windowGui.text().getCursorY());
                if (type == EntryType::Directory) { windowGui.text() << "-"; }
                else { windowGui.text() << size << " B"; }
                maxSizesOffset = std::max(maxSizesOffset, windowGui.text().getCursorX());
                windowGui.text() << "\n";
            }
        }

        // Reset Cursor


        // Reset text renderer and swap frame buffers for next frame then yield
        windowGui.swapBuffers();
        sched_yield();
    }

    return 0;
}

int PalmyraOS::Userland::builtin::fileManager::fetchContent(types::UserHeapManager& heap,
                                                            types::UVector<types::UString<char>>& currentDirectory,
                                                            types::UVector<DirectoryEntry>& content) {
    char directoryBuffer[512];

    // Construct the directory path
    int result = SDK::constructDirectoryPath(directoryBuffer, sizeof(directoryBuffer), currentDirectory);
    if (result < 0) return -1;  // Handle buffer overflow error

    // Open the directory specified by directoryBuffer.
    int fileDescriptor = open(directoryBuffer, 0);

    // Error: No such file or directory.
    if (fileDescriptor < 0) { return -2; }

    // Allocate a buffer to hold directory entries from getdents.
    const size_t bufferSize = 4096;
    char* buffer            = static_cast<char*>(heap.alloc(bufferSize));
    if (!buffer) {
        // Error: Could not allocate memory.
        heap.free(buffer);
        close(fileDescriptor);
        return -3;
    }

    // Read directory entries into the buffer.
    int bytesRead = getdents(fileDescriptor, reinterpret_cast<linux_dirent*>(buffer), bufferSize);
    if (bytesRead < 0) {
        // Error: Failed to read directory entries.
        heap.free(buffer);
        close(fileDescriptor);
        return -4;
    }

    // Process each entry in the directory.
    int currentByteIndex = 0;
    while (currentByteIndex < bytesRead) {
        // Get a pointer to the current directory entry.
        auto* dirEntry  = reinterpret_cast<linux_dirent*>(buffer + currentByteIndex);

        // Extract the directory entry type (file, directory, etc.).
        // Note: The d_type field is often stored at the end of the dirent structure.
        char dentryType = *(buffer + currentByteIndex + dirEntry->d_reclen - 1);

        // Create a UString for the entry name using the heap allocator.
        types::UString<char> entryName(heap, dirEntry->d_name);

        if (dentryType == DT_DIR) {
            // Add the directory entry to the content vector.
            content.push_back({
                    .name       = entryName,
                    .dentryType = EntryType::Directory,
                    .size       = 0  // Directories don't have size in traditional sense
            });
        }
        else if (dentryType == DT_REG) { pushArchive(heap, content, types::UString<char>(heap, directoryBuffer), entryName); }
        else {
            // Add the directory entry to the content vector.
            content.push_back({.name = entryName, .dentryType = EntryType::Invalid, .size = 0});
        }


        // Move to the next directory entry.
        currentByteIndex += dirEntry->d_reclen;
    }

    // Sorting the content based on the type
    std::sort(content.begin(), content.end(), [](const DirectoryEntry& a, const DirectoryEntry& b) { return a.dentryType < b.dentryType; });

    // Clean up resources.
    heap.free(buffer);
    close(fileDescriptor);

    return 0;
}

void PalmyraOS::Userland::builtin::fileManager::pushArchive(types::UserHeapManager& heap,
                                                            types::UVector<DirectoryEntry>& content,
                                                            const types::UString<char>& parentDirectory,
                                                            const types::UString<char>& entryName) {
    // Archive or Executable

    // Construct absolute path
    types::UString<char> absolutePath(heap, parentDirectory.c_str());
    absolutePath += entryName;

    int isElf_ = SDK::isElf(heap, absolutePath);

    if (isElf_ == 0) {
        // Not an Elf
        content.push_back({.name = entryName, .dentryType = EntryType::Archive, .size = 0});
    }
    else if (isElf_ == 32) {
        // Elf x86 executable
        content.push_back({.name = entryName, .dentryType = EntryType::Elf32, .size = 0});
    }
    else if (isElf_ == 64) {
        // Elf x86_64 executable
        content.push_back({.name = entryName, .dentryType = EntryType::Elf64, .size = 0});
    }
    else if (isElf_ == 100) {
        // Elf x86_64 library
        content.push_back({.name = entryName, .dentryType = EntryType::ElfLib, .size = 0});
    }
    else {
        // Should not get here
        content.push_back({.name = entryName, .dentryType = EntryType::Invalid, .size = 0});
    }
}

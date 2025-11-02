
#include "userland/systemWidgets/KernelTerminal.h"
#include <elf.h>

#include "palmyraOS/errono.h"
#include "palmyraOS/shared/memory/HeapAllocator.h"  // C++ heap allocator for efficient memory management
#include "palmyraOS/stdio.h"                        // For standard input/output functions: printf, perror
#include "palmyraOS/stdlib.h"                       // For dynamic memory management
#include "palmyraOS/time.h"                         // For sleeping
#include "palmyraOS/unistd.h"                       // Include PalmyraOS system calls

#include "libs/string.h"               // strlen
#include "palmyraOS/circularBuffer.h"  // For efficient FIFO buffer implementation
#include "palmyraOS/palmyraSDK.h"      // Window, Window Frame


// Use the types namespace from PalmyraOS: CircularBuffer, UserHeapManager
using namespace PalmyraOS::types;


namespace PalmyraOS::Userland::builtin::KernelTerminal {

    // Typedefs for buffer types simplify declarations
    using StdoutType            = CircularBuffer<char, 4096>;
    using StdinType             = CircularBuffer<char, 4096>;

    // Current working directory (global state)
    constexpr size_t kPathMax   = 512;  // single place to tune path buffer size
    static char g_cwd[kPathMax] = "/";

    // Helper: Render colored prompt "PalmyraOS:<cwd>$ "
    void appendColoredPrompt(StdoutType& output) {
        output.append("PalmyraOS", 9);
        output.append(":", 1);
        output.append(g_cwd, strlen(g_cwd));
        output.append("$ ", 2);
    }

    // Helper: Resolve path relative to cwd into out buffer
    static void resolvePathToBuffer(const char* pathStr, char* out, size_t outSize) {
        if (outSize == 0) return;
        // Initialize base: absolute -> '/', relative -> current cwd
        if (pathStr && pathStr[0] == '/') {
            out[0] = '/';
            out[1] = '\0';
        }
        else {
            strncpy(out, g_cwd, outSize - 1);
            out[outSize - 1] = '\0';
            if (out[0] == '\0') {
                out[0] = '/';
                out[1] = '\0';
            }
        }

        const char* p = pathStr ? pathStr : "";
        if (p[0] == '/') p++;  // skip leading '/'

        // Normalize segments
        size_t len = strlen(out);
        while (*p) {
            // skip redundant '/'
            while (*p == '/') p++;
            const char* seg = p;
            while (*p && *p != '/') p++;
            size_t segLen = (size_t) (p - seg);
            if (segLen == 0) break;

            // Handle '.' and '..'
            if (segLen == 1 && seg[0] == '.') continue;
            if (segLen == 2 && seg[0] == '.' && seg[1] == '.') {
                // pop last segment
                if (len > 1) {
                    if (out[len - 1] == '/' && len > 1) len--;
                    while (len > 1 && out[len - 1] != '/') len--;
                    out[len] = '\0';
                }
                continue;
            }

            // append '/' if needed
            if (len == 0 || out[len - 1] != '/') {
                if (len + 1 < outSize) {
                    out[len++] = '/';
                    out[len]   = '\0';
                }
            }
            // append segment
            size_t avail  = (len < outSize) ? (outSize - 1 - len) : 0;
            size_t toCopy = segLen > avail ? avail : segLen;
            if (toCopy > 0) {
                strncpy(out + len, seg, toCopy);
                len += toCopy;
                out[len] = '\0';
            }
            // if truncated, stop processing further
            if (toCopy < segLen) break;
        }

        if (len == 0) {
            out[0] = '/';
            out[1] = '\0';
        }
        // Remove trailing '/.' or '/..' should already be normalized by logic above
    }

    // Function declarations for command parsing and execution
    void parseCommand(UserHeapManager& heap, CircularBuffer<char>& input, types::UVector<types::UString<char>>& tokens);
    void executeCommand(UserHeapManager& heap, StdinType& input, StdoutType& output);

    int main(uint32_t argc, char** argv) {
        // Initialize dynamic memory allocator for the application
        UserHeapManager heap;

        // Set initial window position and dimensions
        SDK::Window window(50, 100, 640, 480, true, "Palmyra Terminal");
        SDK::WindowGUI windowGui(window);
        windowGui.setBackground(Color::Black);

        // layout
        int scrollY_content                      = 0;

        // Setup terminal buffers and initial prompt
        auto stdoutBufferPtr                     = heap.createInstance<KernelTerminal::StdoutType>();
        auto stdinBufferPtr                      = heap.createInstance<KernelTerminal::StdinType>();
        KernelTerminal::StdoutType& stdoutBuffer = *stdoutBufferPtr;
        KernelTerminal::StdinType& stdinBuffer   = *stdinBufferPtr;

        uint64_t count                           = 0;
        types::UVector<types::UString<char>> tokens(heap);


        // TODO check for bad allocation
        appendColoredPrompt(stdoutBuffer);

        // If arguments are provided, treat them as a command to execute
        if (argc > 1) {
            // TODO reduce redundancy

            // Build the command string from arguments
            for (int i = 1; i < argc; ++i) {
                stdinBuffer.append(argv[i], strlen(argv[i]));
                if (i < argc - 1) {
                    stdinBuffer.append(' ');  // Add space between arguments
                }
            }
            stdinBuffer.append('\n');  // Append newline to simulate "Enter"

            // Echo the input to the output buffer for display
            stdoutBuffer.append(stdinBuffer.get(), stdinBuffer.size());

            // Execute the command with the input buffer
            executeCommand(heap, stdinBuffer, stdoutBuffer);

            // Clear the input buffer for the next command
            stdinBuffer.clear();

            // Prompt the user again
            appendColoredPrompt(stdoutBuffer);
        }

        while (true) {
            count++;


            // Another loop to catch all keyboard events
            KeyboardEvent event;
            while (true) {
                event = nextKeyboardEvent(window.getID());       // Fetch the next event
                if (!event.isValid || event.key == '\0') break;  // If no key is pressed, break the loop
                if (event.pressed) break;                        // only handle releases
                else if (event.key == 8)
                    stdinBuffer.backspace();  // Handle backspace for corrections
                                              // Append any other key to our input buffer
                else {
                    // TODO improve keyboard events
                    if (event.isShiftDown && event.key == '/') { stdinBuffer.append('_'); }
                    else { stdinBuffer.append(event.key); }
                }


                // When Enter is pressed, process the command
                if (event.key == '\n') {
                    // Get the input from our buffer
                    auto input = stdinBuffer.get();

                    // Echo the input to the output buffer
                    stdoutBuffer.append(input, strlen(input));

                    // Time to execute the command
                    executeCommand(heap, stdinBuffer, stdoutBuffer);

                    // Clear the input buffer for the next command
                    stdinBuffer.clear();

                    // Prompt the user again
                    appendColoredPrompt(stdoutBuffer);
                }
            }


            {
                SDK::Layout layout(windowGui, &scrollY_content, true);

                // Render the prompt with colors
                windowGui.text() << PalmyraOS::Color::Gray100 << stdoutBuffer.get();

                // Display the current input and a blinking cursor for feedback
                windowGui.text() << PalmyraOS::Color::LightGreen << stdinBuffer.get() << PalmyraOS::Color::Gray100;
                if ((count >> 5) % 2) windowGui.text() << "_";
            }


            // Refresh the display and yield control to other processes
            windowGui.swapBuffers();

            // Be a good citizen and yield some CPU time to other processes
            sched_yield();
        }
    }

    void parseCommand(UserHeapManager& heap, StdinType& input, types::UVector<types::UString<char>>& tokens) {
        // Fetch the command string from input buffer
        char* command = (char*) input.get();

        // Begin tokenizing the command by spaces and newlines
        char* token   = strtok(command, " \n");

        // As long as there are tokens...
        while (token != nullptr) {
            // Create a dynamic string for each token
            auto _token = types::UString<char>(heap);

            // Assign the token to our dynamic string and push it
            _token      = token;
            tokens.push_back(_token);

            // Continue to the next token
            token = strtok(nullptr, " \n");
        }
    }

    void executeCommand(UserHeapManager& heap, StdinType& input, StdoutType& output) {
        // Prepare a vector to hold our tokens
        types::UVector<types::UString<char>> tokens(heap);

        // Parse the command from input
        parseCommand(heap, input, tokens);

        // If there are no tokens, do nothing
        if (tokens.empty()) return;

        // ECHO
        if (tokens[0] == "echo") {
            // Loop through all tokens except the first
            for (int i = 1; i < tokens.size(); ++i) {
                // Append each token to output
                output.append(tokens[i].c_str(), tokens[i].size());

                // Add a space between tokens
                output.append(' ');
            }
            output.append('\n');
            return;
        }

        // ECHO
        if (tokens[0] == "exit") {
            _exit(0);
            return;
        }

        // CLEAR
        if (tokens[0] == "clear") {
            output.clear();  // Simply clear the output buffer
            return;
        }

        // UNAME
        if (tokens[0] == "uname") {
            output.append("PalmyraOS Prototype 0.1.0 (x86 32-Bit)\n", 40);
            return;
        }

        // CAT
        if (tokens[0] == "cat") {
            // cat file.txt <offset> <length>
            if (tokens.size() < 2) {
                // Remind the user to specify a file path
                output.append("No path was provided\n", 22);
                return;
            }

            char resolvedPath[kPathMax];
            resolvePathToBuffer(tokens[1].c_str(), resolvedPath, sizeof(resolvedPath));
            const char* filePath = resolvedPath;
            int32_t offset       = 0;     // Default offset
            size_t length        = 4096;  // Default length

            // Check if offset is provided
            if (tokens.size() >= 3) {
                offset = strtol(tokens[2].c_str(), nullptr, 10);
                if (offset < 0) {
                    output.append("Invalid offset provided\n", 24);
                    return;
                }
            }

            // Check if length is provided
            if (tokens.size() >= 4) {
                length = strtol(tokens[3].c_str(), nullptr, 10);
                if (length <= 0) {
                    output.append("Invalid length provided\n", 24);
                    return;
                }
            }

            // Attempt to open the file specified by the user
            int fileDescriptor = open(filePath, 0);
            if (fileDescriptor < 0) {
                // Inform if the file couldn't be opened, perhaps it doesn't exist
                output.append("cat: ", 5);
                output.append(filePath, strlen(filePath));
                output.append(": No such file or directory\n", 29);
                return;
            }

            // Seek to the specified offset if one is provided
            if (offset > 0) {
                if (lseek(fileDescriptor, offset, SEEK_SET) < 0) {
                    output.append("cat: ", 5);
                    output.append(filePath, strlen(filePath));
                    output.append(": Failed to seek to the given offset\n", 37);
                    close(fileDescriptor);
                    return;
                }
            }

            // Allocate a buffer to read the file content
            auto buffer   = (char*) heap.alloc(length * sizeof(char));

            // Read the specified number of bytes from the file
            int bytesRead = read(fileDescriptor, buffer, length);

            // Close the file to tidy up and prevent resource leaks
            close(fileDescriptor);

            // Output the content read from the file and a newline to separate commands
            output.append(buffer, bytesRead);
            output.append('\n');
            heap.free(buffer);  // Free up memory used by the buffer
            return;
        }

        // LS
        if (tokens[0] == "ls") {
            // Use cwd as default, or resolve provided path
            const char* pathArg = (tokens.size() <= 1) ? g_cwd : tokens[1].c_str();
            char resolvedPath[kPathMax];
            resolvePathToBuffer(pathArg, resolvedPath, sizeof(resolvedPath));

            // Try opening the directory specified
            int fileDescriptor = open(resolvedPath, 0);
            if (fileDescriptor < 0) {
                // If directory cannot be opened, inform the user
                output.append("ls: ", 4);
                output.append(resolvedPath, strlen(resolvedPath));
                output.append(": No such file or directory\n", 28);
                return;
            }

            // Prepare a buffer to hold directory entries
            const size_t bufferSize = 4096;
            auto buffer             = (char*) heap.alloc(bufferSize);
            if (!buffer) {
                // If directory cannot be opened, inform the user
                output.append("ls: ", 6);
                output.append(": Could not allocate memory.\n", 30);
                return;
            }
            {
                // Read directory entries
                linux_dirent* DirectoryEntry;
                int currentByteIndex;
                char DentryType;  // File, Directory, ...
                int bytesRead = getdents(fileDescriptor, (linux_dirent*) buffer, bufferSize);

                // Process each entry in the directory
                for (currentByteIndex = 0; currentByteIndex < bytesRead;) {

                    DirectoryEntry = (struct linux_dirent*) (buffer + currentByteIndex);
                    DentryType     = *(buffer + currentByteIndex + DirectoryEntry->d_reclen - 1);

                    // Append directory names with a slash to indicate they are directories
                    output.append(DirectoryEntry->d_name, strlen(DirectoryEntry->d_name));

                    if (DentryType == DT_DIR) output.append("/  ", 4);
                    else output.append("  ", 3);

                    // Move to the next entry
                    currentByteIndex += DirectoryEntry->d_reclen;
                }

                // Add a newline in the end
                if (bytesRead > 0) output.append('\n');
            }

            // Close the directory and free the buffer
            close(fileDescriptor);
            heap.free(buffer);
            return;
        }

        // SLEEP
        if (tokens[0] == "sleep") {
            if (tokens.size() <= 1) {
                // Require an interval
                output.append("No time interval was provided!\n", 32);
                return;
            }

            // Parse the interval (assume it is an integer for now) TODO: float
            char* end;
            long int seconds;
            seconds = strtol(tokens[1].c_str(), &end, 10);
            if (*end != '\0') {
                output.append("Please provide an integer!\n", 28);
                return;
            }

            // Sleep
            timespec req{};
            req.tv_sec  = seconds;
            req.tv_nsec = 0;
            clock_nanosleep(CLOCK_REALTIME, 0, &req, nullptr);
            return;
        }

        // TOUCH - Create an empty file or truncate existing file
        if (tokens[0] == "touch") {
            if (tokens.size() < 2) {
                // Require a file path
                output.append("Usage: touch <filename>\n", 24);
                return;
            }

            char resolvedPath[kPathMax];
            resolvePathToBuffer(tokens[1].c_str(), resolvedPath, sizeof(resolvedPath));
            const char* filePath = resolvedPath;

            // Open with creation only (no truncate) to mimic Linux touch behavior
            // O_CREAT:  Create the file if it doesn't exist
            // O_WRONLY: Open for writing only (any access mode is fine here)
            // No O_TRUNC: Do not erase existing files
            int flags            = O_CREAT | O_WRONLY;
            int fd               = open(filePath, flags);
            if (fd < 0) {
                // Failed to create or open file
                output.append("touch: ", 7);
                output.append(filePath, strlen(filePath));
                output.append(": Failed to create file\n", 24);
                return;
            }

            // File created (or already existed) - close immediately
            close(fd);
            output.append("File touched: ", 14);
            output.append(filePath, strlen(filePath));
            output.append("\n", 1);
            return;
        }

        // MKDIR - Create a directory
        if (tokens[0] == "mkdir") {
            if (tokens.size() < 2) {
                // Require a directory path
                output.append("Usage: mkdir <dirname>\n", 23);
                return;
            }

            char resolvedPath[kPathMax];
            resolvePathToBuffer(tokens[1].c_str(), resolvedPath, sizeof(resolvedPath));
            const char* dirPath = resolvedPath;

            // Call mkdir() syscall with default permissions (0755)
            int result          = mkdir(dirPath, 0755);
            if (result < 0) {
                // Failed to create directory
                output.append("mkdir: ", 7);
                output.append(dirPath, strlen(dirPath));
                if (result == -EEXIST) { output.append(": File exists\n", 14); }
                else if (result == -ENOENT) { output.append(": No such file or directory\n", 28); }
                else if (result == -EFAULT) { output.append(": Bad address\n", 14); }
                else { output.append(": Failed to create directory\n", 29); }
                return;
            }

            output.append("Directory created: ", 19);
            output.append(dirPath, strlen(dirPath));
            output.append("\n", 1);
            return;
        }

        if (tokens[0] == "rm") {
            if (tokens.size() < 2) {
                output.append("Usage: rm <file>\n", 17);
                return;
            }

            char resolvedPath[kPathMax];
            resolvePathToBuffer(tokens[1].c_str(), resolvedPath, sizeof(resolvedPath));
            const char* filePath = resolvedPath;

            int result           = unlink(filePath);
            if (result < 0) {
                output.append("rm: ", 4);
                output.append(filePath, strlen(filePath));
                if (result == -ENOENT) { output.append(": No such file or directory\n", 28); }
                else if (result == -EISDIR) { output.append(": Is a directory\n", 17); }
                else if (result == -EFAULT) { output.append(": Bad address\n", 14); }
                else { output.append(": Failed to remove file\n", 24); }
                return;
            }

            output.append("Removed: ", 9);
            output.append(filePath, strlen(filePath));
            output.append("\n", 1);
            return;
        }

        if (tokens[0] == "cd") {
            const char* arg = (tokens.size() < 2) ? "/" : tokens[1].c_str();
            char resolved[kPathMax];
            resolvePathToBuffer(arg, resolved, sizeof(resolved));

            // Validate the target exists and is a directory: open + getdents
            int fd = open(resolved, 0);
            if (fd < 0) {
                output.append("cd: ", 4);
                output.append(resolved, strlen(resolved));
                output.append(": No such file or directory\n", 28);
                return;
            }
            // Probe directory by getdents
            char tmpBuf[64];
            int dentBytes = getdents(fd, (linux_dirent*) tmpBuf, sizeof(tmpBuf));
            close(fd);
            if (dentBytes < 0) {
                output.append("cd: ", 4);
                output.append(resolved, strlen(resolved));
                output.append(": Not a directory\n", 18);
                return;
            }
            // Accept change
            strncpy(g_cwd, resolved, sizeof(g_cwd) - 1);
            g_cwd[sizeof(g_cwd) - 1] = '\0';
            return;
        }

        // check if elf
        if (tokens[0] == "iself") {
            if (tokens.size() <= 1) {
                // Remind the user to specify a file path
                output.append("No path was provided\n", 22);
                return;
            }

            // Attempt to open the file specified by the user
            char resolvedPath[kPathMax];
            resolvePathToBuffer(tokens[1].c_str(), resolvedPath, sizeof(resolvedPath));
            int fileDescriptor = open(resolvedPath, 0);
            if (fileDescriptor < 0) {
                // Inform if the file couldn't be opened, perhaps it doesn't exist
                output.append("iself: ", 7);
                output.append(resolvedPath, strlen(resolvedPath));
                output.append(": No such file or directory\n", 28);
                return;
            }

            // Allocate a buffer to read the file content
            auto e_ident  = (char*) heap.alloc(EI_NIDENT);

            // Read up to 512 characters from the file
            int bytesRead = read(fileDescriptor, e_ident, EI_NIDENT);

            // Check if the file is large enough to contain the ELF identifier
            if (bytesRead < EI_NIDENT) {
                output.append("iself: ", 6);
                output.append(tokens[1].c_str(), tokens[1].size());
                output.append(": File is too small to be an ELF file.\n", 40);

                heap.free(e_ident);
                close(fileDescriptor);
                return;
            }

            // Check ELF magic number
            if (e_ident[0] != ELFMAG0 || e_ident[1] != ELFMAG1 || e_ident[2] != ELFMAG2 || e_ident[3] != ELFMAG3) {
                output.append("iself: ", 6);
                output.append(tokens[1].c_str(), tokens[1].size());
                output.append(": Not an ELF file.\n", 20);

                heap.free(e_ident);
                close(fileDescriptor);
                return;
            }

            output.append("iself: ", 6);
            output.append(tokens[1].c_str(), tokens[1].size());
            output.append(": is a valid ELF file.\n", 24);


            if (e_ident[EI_CLASS] == ELFCLASS64) { output.append("ELF: x86_64\n", 13); }

            if (e_ident[EI_CLASS] == ELFCLASS32) {
                output.append("ELF: x86 (i386)\n", 17);

                // Move the file offset back to the start
                if (lseek(fileDescriptor, 0, SEEK_SET) == -1) {
                    output.append("iself: ", 6);
                    output.append(": Could not seek.\n", 19);

                    heap.free(e_ident);
                    close(fileDescriptor);
                    return;
                }

                Elf32_Ehdr header32;
                bytesRead = read(fileDescriptor, &header32, sizeof(header32));
                if (bytesRead == -1) {
                    output.append("iself: ", 6);
                    output.append(": Could not read.\n", 19);

                    heap.free(e_ident);
                    close(fileDescriptor);
                    return;
                }

                if (bytesRead < sizeof(header32)) {
                    output.append("iself: ", 6);
                    output.append(": Failed to read the full ELF header.\n", 39);

                    heap.free(e_ident);
                    close(fileDescriptor);
                    return;
                }


                // Append formatted ELF header information to output
                output.append("ELF Header:\n", 13);

                // Type
                char typeBuffer[32];
                snprintf(typeBuffer, sizeof(typeBuffer), "  Type: %u\n", header32.e_type);
                output.append(typeBuffer, strlen(typeBuffer));

                // Machine
                char machineBuffer[32];
                snprintf(machineBuffer, sizeof(machineBuffer), "  Machine: %u\n", header32.e_machine);
                output.append(machineBuffer, strlen(machineBuffer));

                // Version
                char versionBuffer[32];
                snprintf(versionBuffer, sizeof(versionBuffer), "  Version: %u\n", header32.e_version);
                output.append(versionBuffer, strlen(versionBuffer));

                // Entry point address
                char entryPointBuffer[48];
                snprintf(entryPointBuffer, sizeof(entryPointBuffer), "  Entry point address: 0x%x\n", header32.e_entry);
                output.append(entryPointBuffer, strlen(entryPointBuffer));
            }

            // Close the file to tidy up and prevent resource leaks
            close(fileDescriptor);

            // Free up memory used by the buffer
            heap.free(e_ident);
            return;
        }

        // WAITPID
        if (tokens[0] == "waitpid") {
            if (tokens.size() <= 1) {
                // Require a PID to wait for
                output.append("No PID was provided!\n", 22);
                return;
            }

            // Parse the PID (assume it is an integer for now)
            char* end;
            long int pid = strtol(tokens[1].c_str(), &end, 10);
            if (*end != '\0') {
                output.append("Please provide a valid integer PID!\n", 37);
                return;
            }

            // Call the waitpid system call
            int status;
            uint32_t result = waitpid(pid, &status, 0);

            // Check the result
            if (result == (uint32_t) -1 || result == -ECHILD) {
                output.append("waitpid: Failed to wait for the process.\n", 42);
                return;
            }

            output.append("Process with PID ", 18);
            char pidBuffer[12];
            snprintf(pidBuffer, sizeof(pidBuffer), "%d", pid);
            output.append(pidBuffer, strlen(pidBuffer));
            output.append(" terminated with status ", 25);

            // Format the status code
            char statusBuffer[12];
            snprintf(statusBuffer, sizeof(statusBuffer), "%d", status);
            output.append(statusBuffer, strlen(statusBuffer));
            output.append("\n", 1);

            return;
        }


        // EXEC - use posix_spawn to execute a file with arguments
        if (tokens[0] == "exec") {
            if (tokens.size() < 2) {
                output.append("exec: No command specified.\n", 29);
                return;
            }

            // Convert the tokens to a format suitable for posix_spawn
            char resolvedPath[kPathMax];
            resolvePathToBuffer(tokens[1].c_str(), resolvedPath, sizeof(resolvedPath));
            const char* path = resolvedPath;
            size_t argc      = tokens.size() - 1;

            // Prepare argv array for the new process
            char** argv      = (char**) heap.alloc((argc + 1) * sizeof(char*));
            if (!argv) {
                output.append("exec: Failed to allocate memory for arguments.\n", 48);
                return;
            }

            for (int i = 0; i < argc; ++i) { argv[i] = const_cast<char*>(tokens[i + 1].c_str()); }
            argv[argc] = nullptr;  // Null-terminate the argv array

            // Spawn the new process
            uint32_t child_pid;
            int status = posix_spawn(&child_pid, path, nullptr, nullptr, argv, nullptr);

            // Free allocated memory for argv
            heap.free(argv);

            if (status != 0) {
                output.append("exec: Failed to start process.\n", 32);
                return;
            }

            // Wait for the process to finish
            int wait_status;
            uint32_t wait_result = waitpid(child_pid, &wait_status, 0);

            if (wait_result != child_pid) {
                output.append("waitpid: Failed to wait for the process.\n", 42);
                return;
            }

            // Now get the process PID
            char pidBuffer[12];
            snprintf(pidBuffer, sizeof(pidBuffer), "%d", child_pid);

            // Flush its output
            {
                // Construct the path /proc/<pid>/stdout
                char procPath[64];
                snprintf(procPath, sizeof(procPath), "/proc/%s/stdout", pidBuffer);

                // Open the stdout file
                int fd = open(procPath, 0);
                if (fd < 0) {
                    output.append("Failed to open ", 15);
                    output.append(procPath, strlen(procPath));
                    output.append(".\n", 2);
                    return;
                }

                // Read the file content
                char buffer[kPathMax];
                int bytesRead = 0;
                while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) { output.append(buffer, bytesRead); }

                // Close the file
                close(fd);
            }

            // Process with PID 6 terminated with status 0.
            if (wait_status != 0) {
                char statusBuffer[12];
                output.append("Process with PID ", 16);
                output.append(pidBuffer, strlen(pidBuffer));
                output.append(" terminated with status ", 25);
                snprintf(statusBuffer, sizeof(statusBuffer), "%d", wait_status);
                output.append(statusBuffer, strlen(statusBuffer));
                output.append(".\n", 2);
            }
        }

        // Unknown command
        else {
            output.append("Unknown command: '", 18);
            output.append(tokens[0].c_str(), tokens[0].size());
            output.append("'\n", 2);
            return;
        }
    }

}  // namespace PalmyraOS::Userland::builtin::KernelTerminal
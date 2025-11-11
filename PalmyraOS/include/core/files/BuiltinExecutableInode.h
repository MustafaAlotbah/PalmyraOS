// Files/Builtin Executable Inode
// Represents built-in executables that are compiled into the kernel

#pragma once

#include "core/files/VirtualFileSystemBase.h"

namespace PalmyraOS::kernel::vfs {

    /**
     * @class BuiltinExecutableInode
     * @brief Represents a built-in executable that is compiled into the kernel.
     *
     * This class allows built-in programs (like terminal.elf, filemanager.elf, etc.)
     * to appear in the VFS as regular executables in /bin/ without requiring actual
     * disk files. They can be launched via posix_spawn() just like external ELF files.
     *
     * Example:
     *   auto inode = heapManager.createInstance<BuiltinExecutableInode>(
     *       reinterpret_cast<BuiltinExecutableInode::EntryPoint>(MyApp::main)
     *   );
     *   VirtualFileSystem::setInodeByPath(KString("/bin/myapp.elf"), inode);
     */
    class BuiltinExecutableInode : public InodeBase {
    public:
        /**
         * @brief Entry point function signature for built-in executables.
         * 
         * This matches Process::ProcessEntry signature exactly.
         * @param argc Number of command-line arguments (uint32_t to match Process::ProcessEntry)
         * @param argv Array of command-line argument strings
         * @return Exit code (int)
         */
        using EntryPoint = int (*)(uint32_t argc, char** argv);

        /**
         * @brief Constructor for BuiltinExecutableInode.
         * @param entry The entry point function for the executable
         */
        BuiltinExecutableInode(EntryPoint entry);

        /**
         * @brief Destructor.
         * Note: Not marked override because InodeBase destructor is not virtual
         */
        ~BuiltinExecutableInode() = default;

        /**
         * @brief Check if this inode represents a built-in executable.
         * @return Always returns true for BuiltinExecutableInode.
         */
        bool isBuiltinExecutable() const override { return true; }

        /**
         * @brief Get the entry point function for this executable.
         * @return Pointer to the entry point function.
         */
        EntryPoint getEntryPoint() const { return entryPoint_; }

        /**
         * @brief Read operation is not supported for built-in executables.
         * @return 0 (executables are not readable as data files)
         */
        size_t read(char* buffer, size_t size, size_t offset) override;

        /**
         * @brief Write operation is not supported for built-in executables.
         * @return 0 (executables are not writable)
         */
        size_t write(const char* buffer, size_t size, size_t offset) override;

    private:
        EntryPoint entryPoint_;  ///< Function pointer to the executable's entry point
    };

}  // namespace PalmyraOS::kernel::vfs


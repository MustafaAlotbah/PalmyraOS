// Files/Builtin Executable Inode Implementation

#include "core/files/BuiltinExecutableInode.h"

namespace PalmyraOS::kernel::vfs {

    BuiltinExecutableInode::BuiltinExecutableInode(EntryPoint entry)
        : InodeBase(Type::File,
                    Mode::USER_READ | Mode::USER_EXECUTE | Mode::GROUP_READ | Mode::GROUP_EXECUTE | Mode::OTHERS_READ | Mode::OTHERS_EXECUTE,
                    UserID::ROOT,
                    GroupID::ROOT),
          entryPoint_(entry) {
        // Set a nominal size (doesn't represent actual data, just for metadata)
        size_ = 0;
    }

    size_t BuiltinExecutableInode::read(char* buffer, size_t size, size_t offset) {
        // Built-in executables are not readable as data files
        // They are executed directly via their entry point
        return 0;
    }

    size_t BuiltinExecutableInode::write(const char* buffer, size_t size, size_t offset) {
        // Built-in executables are not writable
        return 0;
    }

}  // namespace PalmyraOS::kernel::vfs


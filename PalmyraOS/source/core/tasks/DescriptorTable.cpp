

#include "core/tasks/DescriptorTable.h"

namespace PalmyraOS::kernel {

    // ===== Constructor and Destructor =====

    DescriptorTable::DescriptorTable()
        : nextFd_(3)  // Reserve 0, 1, 2 for stdin, stdout, stderr
    {}

    DescriptorTable::~DescriptorTable() {
        // Clean up all remaining descriptors to prevent memory leaks
        for (auto& [fd, descriptor]: table_) { delete descriptor; }
        table_.clear();
    }

    // ===== Public Methods =====

    fd_t DescriptorTable::allocate(Descriptor* descriptor) {
        // Allocate next available fd
        fd_t fd    = nextFd_++;

        // Store the descriptor (table takes ownership)
        table_[fd] = descriptor;

        return fd;
    }

    void DescriptorTable::release(fd_t fd) {
        // Find the descriptor
        auto it = table_.find(fd);
        if (it == table_.end()) {
            // fd not found - nothing to do
            return;
        }

        // Delete the descriptor object
        delete it->second;

        // Remove from table
        table_.erase(it);
    }

    Descriptor* DescriptorTable::get(fd_t fd) const {
        auto it = table_.find(fd);
        return (it != table_.end()) ? it->second : nullptr;
    }

    bool DescriptorTable::contains(fd_t fd) const { return table_.find(fd) != table_.end(); }

    size_t DescriptorTable::count() const { return table_.size(); }

}  // namespace PalmyraOS::kernel

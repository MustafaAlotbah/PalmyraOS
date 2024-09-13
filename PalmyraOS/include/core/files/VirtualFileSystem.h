// Files/Virtual File System Base

#pragma once

#include "core/files/VirtualFileSystemBase.h"
#include "palmyraOS/unistd.h"    // fd_t
#include <bits/std_function.h>

/*
Class: FunctionInode
- FunctionInode(ReadFunction readFunc = nullptr)
- size_t read(char* buffer, size_t size, size_t offset) override

Class: VirtualFileSystem
- static void initialize()
- static InodeBase* traversePath(InodeBase& rootInode, const KVector<KString>& components)
- static InodeBase* getInodeByPath(InodeBase& rootInode, const KString& path)
- static InodeBase* getInodeByPath(const KString& path)
- static InodeBase* getParentDirectory(InodeBase* rootInode, const KVector<KString>& path)
- static InodeBase* getRootInode()
- static bool initializeFileSystems()
- static bool initializeDeviceFileSystem()
- static bool setInodeByPath(const KString& path, InodeBase* inode)
- static InodeBase* createDirectory(const KString& path, Mode mode, UserID userId, GroupID groupId)
- static KVector<std::pair<KString, InodeBase*>> getContent(const KString& path)
- static InodeBase::Type getType(const KString& path)
- static bool removeInodeByPath(const KString& path)

Class: OpenFile
- OpenFile(InodeBase& inode, int flags)
- InodeBase& getInode() const
- int getFlags() const

Class: FileDescriptorTable
- FileDescriptorTable()
- fd_t allocate(InodeBase* inode, int flags)
- void release(fd_t fd)
- OpenFile* getOpenFile(fd_t fd)
*/

namespace PalmyraOS::kernel::vfs
{

  /**
   * @class FunctionInode
   * @brief Inode type that allows reading, writing, and handling IOCTL operations through provided functions.
   */
  class FunctionInode : public InodeBase
  {
   public:
	  using ReadFunction = std::function<size_t(char* buffer, size_t size, size_t offset)>;
	  using WriteFunction = std::function<size_t(const char* buffer, size_t size, size_t offset)>;
	  using IOCTLFunction = std::function<int(int request, void* arg)>;

	  /**
	   * @brief Constructor for FunctionInode.
	   * @param readFunc Function pointer for reading data.
	   * @param writeFunc Function pointer for writing data.
	   * @param ioctlFunc Function pointer for IOCTL operations.
	   */
	  explicit FunctionInode(
		  ReadFunction readFunc = nullptr,
		  WriteFunction writeFunc = nullptr,
		  IOCTLFunction ioctlFunc = nullptr
	  );

	  /**
	   * @brief Override the read method to read from the provided function.
	   * @param buffer The buffer to read data into.
	   * @param size The number of bytes to read.
	   * @param offset The offset to start reading from.
	   * @return The number of bytes read.
	   */
	  size_t read(char* buffer, size_t size, size_t offset) final;

	  /**
	   * @brief Override the write method to write using the provided function.
	   * @param buffer The buffer containing the data to write.
	   * @param size The number of bytes to write.
	   * @param offset The offset to start writing from.
	   * @return The number of bytes written.
	   */
	  size_t write(const char* buffer, size_t size, size_t offset) final;

	  /**
	   * @brief Override the ioctl method to handle IOCTL requests using the provided function.
	   * @param request The IOCTL request code.
	   * @param arg The argument for the IOCTL request.
	   * @return The result of the IOCTL operation.
	   */
	  int ioctl(int request, void* arg) final;

   private:
	  ReadFunction  readFunction_;    ///< Pointer to the function used for reading
	  WriteFunction writeFunction_;    ///< Pointer to the function used for reading
	  IOCTLFunction ioctlFunction_;    ///< Pointer to the function used for reading
  };

  /**
   * @class VirtualFileSystem
   * @brief Main class for managing the virtual file system.
   */
  class VirtualFileSystem
  {
   public:
	  // Initialize the virtual file system
	  static void initialize();

	  static bool isInitialized();

	  static InodeBase* traversePath(InodeBase& rootInode, const KVector<KString>& components);

	  /**
	   * @brief Get an inode by its path starting from a specific root inode.
	   * @param rootInode The root inode to start the search from.
	   * @param path The path to the inode.
	   * @return Pointer to the inode if found, nullptr otherwise.
	   */
	  static InodeBase* getInodeByPath(InodeBase& rootInode, const KString& path);

	  /**
	   * @brief Get an inode by its path starting from the file system's root inode.
	   * @param path The path to the inode.
	   * @return Pointer to the inode if found, nullptr otherwise.
	   */
	  static InodeBase* getInodeByPath(const KString& path);

	  /**
	   * @brief Get the parent directory inode of a given path.
	   * @param rootInode The root inode to start the search from.
	   * @param path The path to the inode.
	   * @return Pointer to the parent directory inode if found, nullptr otherwise.
	   */
	  static InodeBase* getParentDirectory(InodeBase* rootInode, const KVector<KString>& path);

	  /**
	   * @brief Get the root inode of the virtual file system.
	   * @return Pointer to the root inode.
	   */
	  static InodeBase* getRootInode();

	  /**
	   * @brief Initialize all file systems within the virtual file system.
	   * @return True if all file systems were initialized successfully, false otherwise.
	   */
	  static bool initializeFileSystems();

	  /**
	   * @brief Initialize the device file system.
	   * @return True if the device file system was initialized successfully, false otherwise.
	   */
	  static bool initializeDeviceFileSystem();

	  /**
	   * @brief Set an inode by its path.
	   * @param path The path to set the inode at.
	   * @param inode Pointer to the inode to set.
	   * @return True if the inode was set successfully, false otherwise.
	   */
	  static bool setInodeByPath(const KString& path, InodeBase* inode);

	  /**
	   * @brief Create a directory at a given path.
	   * @param path The path to create the directory at.
	   * @param mode The mode (permissions) of the directory.
	   * @param userId The user ID associated with the directory.
	   * @param groupId The group ID associated with the directory.
	   * @return Pointer to the created directory inode.
	   */
	  static InodeBase* createDirectory(
		  const KString& path,
		  InodeBase::Mode mode,
		  InodeBase::UserID userId = (InodeBase::UserID)0,
		  InodeBase::GroupID groupId = (InodeBase::GroupID)0
	  );

	  /**
	   * @brief Get the contents of a directory at a given path.
	   * @param path The path to the directory.
	   * @return A vector of pairs containing the name and inode of each directory entry.
	   */
	  static KVector<std::pair<KString, InodeBase*>> getContent(const KString& path);

	  /**
	   * @brief Get the type of an inode at a given path.
	   * @param path The path to the inode.
	   * @return The type of the inode.
	   */
	  static InodeBase::Type getType(const KString& path);

	  /**
	   * @brief Remove an inode by its path.
	   * @param path The path to the inode.
	   * @return True if the inode was removed successfully, false otherwise.
	   */
	  static bool removeInodeByPath(const KString& path);

   private:
	  static InodeBase* rootNode_;        ///< Root inode of the virtual file system.
	  static InodeBase* deviceInode_;     ///< Inode for the /dev directory.

  };

  /**
   * @class OpenFile
   * @brief Represents an open file with associated inode and flags.
   */
  class OpenFile
  {
   public:
	  /**
	   * @brief Constructor for OpenFile.
	   * @param inode Reference to the associated inode.
	   * @param flags The open file flags.
	   */
	  explicit OpenFile(InodeBase* inode = nullptr, int flags = 0);

	  // Getters
	  [[nodiscard]] InodeBase* getInode() const;
	  [[nodiscard]] int getFlags() const;
	  [[nodiscard]] size_t getOffset() const;
	  void setOffset(size_t offset);
	  void advanceOffset(size_t bytes);

   private:
	  InodeBase* inode_;    ///< Reference to the associated inode
	  size_t offset_;
	  int    flags_;           ///< Open file flags
  };

  // FileDescriptorTable class managing file descriptors for open files
  class FileDescriptorTable
  {
   public:
	  // Constructor for FileDescriptorTable
	  FileDescriptorTable();

	  /**
	   * @brief Allocate a file descriptor for an inode.
	   * @param inode Pointer to the inode.
	   * @param flags The open file flags.
	   * @return The allocated file descriptor.
	   */
	  fd_t allocate(InodeBase* inode, int flags);

	  /**
	   * @brief Release a file descriptor.
	   * @param fd The file descriptor to release.
	   */
	  void release(fd_t fd);

	  /**
	   * @brief Get the OpenFile associated with a file descriptor.
	   * @param fd The file descriptor.
	   * @return Pointer to the OpenFile associated with the file descriptor.
	   */
	  OpenFile* getOpenFile(fd_t fd);

   private:
	  KMap<fd_t, OpenFile> table_;        ///< Map of file descriptors to OpenFile instances
	  fd_t                 nextFd_;       ///< Next file descriptor to be allocated
  };


} // namespace PalmyraOS::kernel::vfs

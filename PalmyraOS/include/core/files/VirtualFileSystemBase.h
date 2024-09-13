// Files/Virtual File System Base

#pragma once

#include "core/definitions.h"
#include "core/memory/KernelHeapAllocator.h"
#include "libs/string.h"

/*
Class: FileSystemType
	- FileSystemType(const KString& name, int flags)
	- void addSuperBlock(SuperBlockBase* superBlock)
	- const KString& getName() const
	- int getFlags() const

Class: InodeBase
	- InodeBase(Type type, Mode mode, UserID userId, GroupID groupId)
	- void setSuperBlock(SuperBlockBase* superBlock)
	- SuperBlockBase* getSuperBlock()
	- void addDentry(KString& name, InodeBase* dentry)
	- InodeBase* getDentry(const KString& name)
	- bool removeDentry(const KString& name)
	- KVector<std::pair<KString, InodeBase*>> getDentries() const

	- size_t read(char* buffer, size_t size, size_t offset)
	- size_t write(const char* buffer, size_t size, size_t offset)
	- int open()
	- int close()

	- size_t getInodeNumber() const
	- Mode getMode() const
	- UserID getUserId() const
	- GroupID getGroupId() const
	- Type getType() const

Class: SuperBlockBase
	- SuperBlockBase(size_t blockSize, FileSystemType* fileSystemType)
	- void addInode(InodeBase* inode)
	- bool removeInode(InodeBase* inode)

	- virtual InodeBase* allocateInode(InodeBase::Type type)
	- virtual bool destroyInode(InodeBase* inode)

	- size_t getBlockSize() const
	- FileSystemType* getFileSystemType() const
*/


namespace PalmyraOS::kernel::vfs
{


  // Forward declarations
  class SuperBlockBase;

  /**
   * @class FileSystemType
   * @brief Represents a specific type of file system (e.g., ext4, FAT32).
   *
   * This class encapsulates the properties and behaviors specific to different file systems.
   */
  class FileSystemType
  {
   public:
	  /**
	   * @brief Constructor for FileSystemType.
	   * @param name The name of the file system type.
	   * @param flags Flags representing various attributes of the file system type.
	   */
	  FileSystemType(const KString& name, int flags);

	  /**
	   * @brief Add a SuperBlock to the file system type.
	   * @param superBlock Pointer to the SuperBlock to be added.
	   */
	  void addSuperBlock(SuperBlockBase* superBlock);

	  // Getters
	  /**
	   * @brief Get the name of the file system type.
	   * @return The name of the file system type.
	   */
	  [[nodiscard]] const KString& getName() const;

	  /**
	   * @brief Get the flags of the file system type.
	   * @return The flags of the file system type.
	   */
	  [[nodiscard]] int getFlags() const;

   private:
	  KString                  name_;           ///< Name of the file system type, e.g., "ext4".
	  int                      flags_;          ///< Flags, e.g., FS_TYPE_SUPPORTS_JOURNALING.
	  KVector<SuperBlockBase*> superBlocks_;    ///< List of SuperBlocks associated with this file system type.
  };

  /**
   * @class InodeBase
   * @brief Represents an inode, a data structure holding information about a file.
   *
   * Inodes contain metadata about files and directories, such as permissions, ownership, and type.
   */
  class InodeBase
  {
   public:
	  /**
	   * @enum Type
	   * @brief Enum for inode types.
	   */
	  enum class Type : uint8_t
	  {
		  Invalid         = 0,       ///< Invalid inode type.
		  FIFO            = 0x1,     ///< FIFO
		  CharacterDevice = 0x2,     ///< 'char' Device file.
		  Directory       = 0x4,     ///< Directory.
		  BlockDevice     = 0x6,     ///< 'block' Device file.
		  File            = 0x8,     ///< Regular file.
		  Link            = 0x10,     ///< Regular file.
	  };

	  /**
	   * @enum Mode
	   * @brief Enum class for file modes (permissions).
	   */
	  enum class Mode : uint32_t
	  {
		  // Owner
		  USER_READ      = 0x100,    // 00400
		  USER_WRITE     = 0x080,    // 00200
		  USER_EXECUTE   = 0x040,    // 00100
		  // Group
		  GROUP_READ     = 0x020,    // 0040
		  GROUP_WRITE    = 0x010,    // 0020
		  GROUP_EXECUTE  = 0x008,    // 0010
		  // Others
		  OTHERS_READ    = 0x004,    // 004
		  OTHERS_WRITE   = 0x002,    // 002
		  OTHERS_EXECUTE = 0x001    // 001
	  };

	  /**
	   * @enum UserID
	   * @brief Enum class for user IDs.
	   */
	  enum class UserID : uint32_t
	  {
		  ROOT = 0,
		  USER = 1000
	  };

	  /**
	   * @enum GroupID
	   * @brief Enum class for group IDs.
	   */
	  enum class GroupID : uint32_t
	  {
		  ROOT  = 0,
		  GROUP = 1000
	  };

   public:
	  /**
	   * @brief Constructor for InodeBase.
	   * @param type The type of the inode.
	   * @param mode The mode (permissions) of the inode.
	   * @param userId The user ID associated with the inode.
	   * @param groupId The group ID associated with the inode.
	   */
	  InodeBase(Type type, Mode mode, UserID userId, GroupID groupId);

	  ~InodeBase(); // not virtual to avoid overriding delete operator

	  /**
	   * @brief Set the associated SuperBlock for the inode.
	   * @param superBlock Pointer to the SuperBlock.
	   */
	  void setSuperBlock(SuperBlockBase* superBlock);

	  /**
	   * @brief Get the associated SuperBlock.
	   * @return Pointer to the associated SuperBlock.
	   */
	  SuperBlockBase* getSuperBlock();

	  /**
	   * @brief Add a directory entry to the inode.
	   * @param name The name of the directory entry.
	   * @param dentry Pointer to the inode of the directory entry.
	   */
	  void addDentry(KString& name, InodeBase* dentry);

	  /**
	   * @brief Get a directory entry by name.
	   * @param name The name of the directory entry.
	   * @return Pointer to the inode of the directory entry.
	   */
	  InodeBase* getDentry(const KString& name);

	  /**
	   * @brief Remove a directory entry by name.
	   * @param name The name of the directory entry to remove.
	   * @return True if the entry was removed, false otherwise.
	   */
	  bool removeDentry(const KString& name);

	  /**
	   * @brief Retrieve a subset of directory entries starting from a specified offset.
	   *
	   * This function retrieves a specified number of directory entries, starting from a given offset.
	   * The directory entries are returned as a vector of pairs, where each pair consists of the
	   * name of the entry and a pointer to its associated inode.
	   *
	   * @param offset The starting position from which to retrieve directory entries. Defaults to 0.
	   * @param count The number of directory entries to retrieve. Defaults to 10.
	   * @return A vector of pairs, each containing the name (KString) and inode pointer (InodeBase*) of a directory entry.
	   */
	  [[nodiscard]] virtual KVector<std::pair<KString, InodeBase*>> getDentries(size_t offset, size_t count);

	  void clearDentries();

	  /**
	   * @brief Virtual method for reading from the inode.
	   * @param buffer The buffer to read data into.
	   * @param size The number of bytes to read.
	   * @param offset The offset to start reading from.
	   * @return The number of bytes read.
	   */
	  virtual size_t read(char* buffer, size_t size, size_t offset);

	  /**
	   * @brief Virtual method for writing to the inode.
	   * @param buffer The buffer containing data to write.
	   * @param size The number of bytes to write.
	   * @param offset The offset to start writing at.
	   * @return The number of bytes written.
	   */
	  virtual size_t write(const char* buffer, size_t size, size_t offset);

	  /**
	   * @brief Performs device-specific operations.
	   *
	   * This function manipulates the underlying parameters of devices. It is a system call
	   * that provides a means to control hardware devices or kernel operations beyond the
	   * standard file operations such as read or write. The operation to be performed is
	   * determined by the `request` argument and the nature of the request may require
	   * additional arguments.
	   *
	   * @param fd The file descriptor referring to the device.
	   * @param request The device-specific request code, which determines the operation to be performed.
	   * @param ... Additional arguments that vary depending on the request code. These arguments
	   *            can be used to pass data to and from the kernel or device driver.
	   * @return 0 on success, or -1 if an error occurred. The errno variable is set to indicate the error.
	   */
	  virtual int ioctl(int request, void* arg);

	  /**
	   * @brief Virtual method for opening the inode.
	   * @return Status code of the open operation.
	   */
	  virtual int open();

	  /**
	   * @brief Virtual method for closing the inode.
	   * @return Status code of the close operation.
	   */
	  virtual int close();

	  // Getters
	  [[nodiscard]] size_t getInodeNumber() const;
	  [[nodiscard]] Mode getMode() const;
	  [[nodiscard]] UserID getUserId() const;
	  [[nodiscard]] GroupID getGroupId() const;
	  [[nodiscard]] Type getType() const;
	  [[nodiscard]] size_t getSize() const;
   protected:
	  static size_t inodes;                ///< Static variable to track the number of inodes.
	  size_t        inodeNumber_;          ///< Unique inode number.
	  Mode          mode_;                 ///< Permissions of the inode (e.g., 0644).
	  Type          type_;                 ///< Type of the inode (file, directory, device).
	  UserID        userId_;               ///< User ID associated with the inode.
	  GroupID       groupId_;              ///< Group ID associated with the inode.
	  uint64_t      accessTime_;           ///< Access time (epoch time).
	  uint64_t      modificationTime_;     ///< Modification time (epoch time).
	  uint64_t      changeTime_;           ///< Change time (epoch time).
	  size_t        size_;                 ///< Size of the file (e.g., 1024 bytes).
	  SuperBlockBase* superBlock_;         ///< Pointer to the associated SuperBlock. (ptr cause of inheritance)
	  KMap<KString, InodeBase*> dentries_; ///< Directory entries map.
  };

  /**
   * @brief Combine mode bits to form permission values like 0755 or 0644.
   * @param lhs Left-hand side operand.
   * @param rhs Right-hand side operand.
   * @return Combined mode value.
   */
  InodeBase::Mode operator|(InodeBase::Mode lhs, InodeBase::Mode rhs);

  /**
   * @class SuperBlockBase
   * @brief Represents the superblock in a file system.
   *
   * The superblock contains metadata about the file system, such as its size, block size, and the file system type.
   */
  class SuperBlockBase
  {
   public:
	  /**
	   * @brief Constructor for SuperBlockBase.
	   * @param blockSize The block size of the file system.
	   * @param fileSystemType Pointer to the associated FileSystemType.
	   */
	  SuperBlockBase(size_t blockSize, FileSystemType* fileSystemType);

	  /**
	   * @brief Add an inode to the superblock.
	   * @param inode Pointer to the inode to add.
	   */
	  void addInode(InodeBase* inode);

	  /**
	   * @brief Remove an inode from the superblock.
	   * @param inode Pointer to the inode to remove.
	   * @return True if the inode was removed, false otherwise.
	   */
	  bool removeInode(InodeBase* inode);

	  /**
	   * @brief Pure virtual method to allocate an inode.
	   * @param type The type of the inode to allocate.
	   * @return Pointer to the allocated inode.
	   */
	  virtual InodeBase* allocateInode(
		  InodeBase::Type type,
		  InodeBase::Mode mode,
		  InodeBase::UserID userId,
		  InodeBase::GroupID groupId
	  );

	  /**
	   * @brief Pure virtual method to destroy an inode.
	   * @param inode Pointer to the inode to destroy.
	   * @return True if the inode was destroyed, false otherwise.
	   */
	  virtual bool destroyInode(InodeBase* inode);

	  // Getters
	  [[nodiscard]] size_t getBlockSize() const;
	  [[nodiscard]] FileSystemType* getFileSystemType() const;
   private:
	  FileSystemType* fileSystemType_;   ///< Pointer to the associated FileSystemType (e.g., ext4).
	  size_t              blockSize_;    ///< Block size in bytes (e.g., 4096 bytes).
	  KVector<InodeBase*> inodes_;       ///< Vector containing all inodes within this superblock.
  };


} // namespace PalmyraOS::kernel::vfs




#include "core/files/VirtualFileSystem.h"
#include "libs/memory.h"


namespace PalmyraOS::kernel::vfs
{


  ///region FunctionInode

  FunctionInode::FunctionInode(ReadFunction readFunc, WriteFunction writeFunc, IOCTLFunction ioctlFunc)
	  : InodeBase(
	  Type::CharacterDevice,
	  Mode::USER_READ | Mode::USER_WRITE | Mode::GROUP_READ | Mode::OTHERS_READ,
	  UserID::ROOT,
	  GroupID::ROOT
  ),
		readFunction_(std::move(readFunc)),
		writeFunction_(std::move(writeFunc)),
		ioctlFunction_(std::move(ioctlFunc))
  {}

  size_t FunctionInode::read(char* buffer, size_t size, size_t offset)
  {
	  // Check if a custom read function is provided
	  if (readFunction_)
	  {
		  // Use the provided read function
		  return readFunction_(buffer, size, offset);
	  }
	  else
	  {
		  // Fallback to the base class read method
		  return InodeBase::read(buffer, size, offset);
	  }
  }

  size_t FunctionInode::write(const char* buffer, size_t size, size_t offset)
  {
	  // Check if a custom write function is provided
	  if (writeFunction_)
	  {
		  // Use the provided write function
		  return writeFunction_(buffer, size, offset);
	  }
	  else
	  {
		  // Fallback to the base class write method
		  return InodeBase::write(buffer, size, offset);
	  }
  }

  int FunctionInode::ioctl(int request, void* arg)
  {
	  // Check if a custom IOCTL function is provided
	  if (ioctlFunction_)
	  {
		  // Use the provided IOCTL function
		  return ioctlFunction_(request, arg);
	  }
	  else
	  {
		  // Fallback to the base class ioctl method
		  return InodeBase::ioctl(request, arg);
	  }
  }

  ///endregion



  ///region VirtualFileSystem

  InodeBase* VirtualFileSystem::rootNode_    = nullptr;
  InodeBase* VirtualFileSystem::deviceInode_ = nullptr;
  InodeBase* VirtualFileSystem::binaryInode_ = nullptr;

  InodeBase* VirtualFileSystem::traversePath(InodeBase& rootInode, const KVector<KString>& components)
  {
	  InodeBase* currentInode = &rootInode; // Start from the given root inode

	  // Traverse each component in the path
	  for (const KString& component : components)
	  {
		  // Retrieve the directory entry (dentry) for the current component
		  InodeBase* dentry = currentInode->getDentry(component);

		  // If the dentry does not exist, return nullptr indicating invalid path
		  if (!dentry) return nullptr;

		  // Move to the next inode in the path
		  currentInode = dentry;
	  }

	  return currentInode;
  }

  InodeBase* VirtualFileSystem::getInodeByPath(InodeBase& rootInode, const KString& path)
  {
	  return traversePath(rootInode, path.split('/', true));
  }

  InodeBase* VirtualFileSystem::getInodeByPath(const KString& path)
  {
	  // Ensure the root node is initialized before proceeding
	  if (!rootNode_) kernelPanic("Called %s before initializing rootNode_", __PRETTY_FUNCTION__);
	  return getInodeByPath(*rootNode_, path);
  }

  InodeBase* VirtualFileSystem::getParentDirectory(InodeBase* rootInode, const KVector<KString>& path)
  {
	  return traversePath(*rootInode, KVector<KString>(path.begin(), path.end() - 1));
  }

  bool VirtualFileSystem::setInodeByPath(const KString& path, InodeBase* inode)
  {
	  // Ensure the root node is initialized before proceeding
	  if (!rootNode_) kernelPanic("Called %s before initializing rootNode_", __PRETTY_FUNCTION__);

	  // Split the path into components using '/' as delimiter
	  KVector<KString> components = path.split('/', true);
	  if (components.empty()) return false;

	  // Find the parent directory inode
	  InodeBase* currentInode = getParentDirectory(rootNode_, components);
	  if (!currentInode) return false;

	  // Set the final component of the path as the dentry for the new inode
	  KString& finalComponent = components.back();
	  currentInode->addDentry(finalComponent, inode);

	  return true;
  }

  InodeBase* VirtualFileSystem::createDirectory(
	  const KString& path,
	  InodeBase::Mode mode,
	  InodeBase::UserID userId,
	  InodeBase::GroupID groupId
  )
  {
	  // Ensure the root node is initialized before proceeding
	  if (!rootNode_) kernelPanic("Called %s before initializing rootNode_", __PRETTY_FUNCTION__);

	  // Split the path into components using '/' as delimiter
	  KVector<KString> components = path.split('/', true);
	  if (components.empty()) return nullptr;

	  // Find the parent directory if it exists and is a directory
	  InodeBase* currentInode = getParentDirectory(rootNode_, components);
	  if (!currentInode) return nullptr;

	  // Create a new directory inode with the specified attributes
	  auto directory = kernel::heapManager.createInstance<InodeBase>(
		  InodeBase::Type::Directory,
		  mode, userId, groupId
	  );

	  // Set the new directory inode at the specified path
	  if (!setInodeByPath(path, directory))
	  {
		  // Free the directory inode if setting it in the path failed
		  kernel::heapManager.free(directory);
		  return nullptr;
	  }

	  return directory;
  }

  InodeBase* VirtualFileSystem::getRootInode()
  {
	  return rootNode_;
  }

  KVector<std::pair<KString, InodeBase*>> VirtualFileSystem::getContent(const KString& path)
  {
	  // Retrieve the inode at the specified path
	  InodeBase* inode = getInodeByPath(path);

	  // Ensure the inode is a directory
	  if (!inode || inode->getType() != InodeBase::Type::Directory)
	  {
		  // Return empty vector if not a directory
		  return {};
	  }

	  // Retrieve and return the directory entries
	  return inode->getDentries(0, 100);
  }

  InodeBase::Type VirtualFileSystem::getType(const KString& path)
  {
	  // Retrieve the inode at the specified path
	  InodeBase* inode = getInodeByPath(path);

	  // Return the type of the inode, or Invalid if not found
	  return inode ? inode->getType() : InodeBase::Type::Invalid;
  }

  bool VirtualFileSystem::removeInodeByPath(const KString& path)
  {
	  // Find the parent directory inode
	  InodeBase* parentInode = getParentDirectory(rootNode_, path.split('/', true));

	  // Ensure the parent directory exists
	  if (!parentInode) return false;

	  // Get the file name from the path
	  KString fileName = path.split('/', true).back();

	  // Retrieve the inode for the file name
	  auto inode = parentInode->getDentry(fileName);

	  // Ensure the inode exists
	  if (!inode) return false;

	  // Remove the dentry from the parent directory
	  if (!parentInode->removeDentry(fileName)) return false;

	  // Free the inode memory
	  kernel::heapManager.free(inode);
	  return true;
  }

  void VirtualFileSystem::initialize()
  {
	  // Initialize the root inode with directory type and default permissions
	  using Mode = InodeBase::Mode;
	  rootNode_ = kernel::heapManager.createInstance<InodeBase>(
		  InodeBase::Type::Directory,
		  Mode::USER_READ
			  | Mode::USER_WRITE
			  | Mode::USER_EXECUTE
			  | Mode::GROUP_READ
			  | Mode::GROUP_EXECUTE
			  | Mode::OTHERS_READ
			  | Mode::OTHERS_EXECUTE,
		  InodeBase::UserID::ROOT,
		  InodeBase::GroupID::ROOT
	  );

	  // Ensure the root inode was successfully created
	  if (!rootNode_) kernelPanic("%s\n Failed to initialize the root node.", __PRETTY_FUNCTION__);

	  // Initialize all file systems
	  initializeFileSystems();
  }

  bool VirtualFileSystem::initializeFileSystems()
  {
	  // Ensure the root node is initialized before proceeding
	  if (!rootNode_) kernelPanic("Called %s before initializing rootNode_", __PRETTY_FUNCTION__);

	  // Initialize the device file system
	  if (!initializeDeviceFileSystem()) return false;

	  return true;
  }

  bool VirtualFileSystem::initializeDeviceFileSystem()
  {

	  // Define a local alias for convenience
	  using Mode = InodeBase::Mode;

	  // Ensure the root node is initialized before proceeding
	  if (!rootNode_) kernelPanic("Called %s before initializing rootNode_", __PRETTY_FUNCTION__);

	  // Create Device File System instance with name "devfs" and default flags
	  auto deviceFileSystem = kernel::heapManager.createInstance<FileSystemType>(KString("devfs"), 0);
	  if (!deviceFileSystem) return false;

	  // Create Device File System SuperBlock with block size of 4096 bytes
	  auto devSuperBlock = kernel::heapManager.createInstance<SuperBlockBase>(4096, deviceFileSystem);
	  if (!devSuperBlock) return false;

	  // Allocate and set the "/dev" directory inode
	  deviceInode_ = devSuperBlock->allocateInode(
		  InodeBase::Type::Directory,
		  Mode::USER_READ | Mode::USER_WRITE | Mode::GROUP_READ | Mode::OTHERS_READ,
		  InodeBase::UserID::ROOT,
		  InodeBase::GroupID::ROOT
	  );
	  if (!deviceInode_ || !setInodeByPath(KString("/dev"), deviceInode_)) return false;

	  // Allocate and set the "/bin" directory inode
	  binaryInode_ = devSuperBlock->allocateInode(
		  InodeBase::Type::Directory,
		  Mode::USER_READ | Mode::USER_WRITE | Mode::GROUP_READ | Mode::OTHERS_READ,
		  InodeBase::UserID::ROOT,
		  InodeBase::GroupID::ROOT
	  );
	  if (!binaryInode_ || !setInodeByPath(KString("/bin"), binaryInode_)) return false;


	  // Create a test inode with a lambda function for reading test string
	  auto testInode = kernel::heapManager.createInstance<FunctionInode>(
		  [](char* buffer, size_t size, size_t offset) -> size_t
		  {
			const char* testStr = "Test string content";
			size_t len = strlen(testStr);

			// If offset is beyond the test string, return 0 indicating no more data to read
			if (offset >= len) return 0;

			// Calculate the number of bytes to read
			size_t bytesToRead = std::min(size, len - offset);
			memcpy((void*)buffer, (void*)(testStr + offset), bytesToRead);
			return bytesToRead;
		  }
	  );
	  if (!testInode) return false;

	  // Set the test inode at "/dev/test"
	  setInodeByPath(KString("/dev/test"), testInode);

	  // Create a subdirectory and set the test inode within it
	  createDirectory(KString("/dev/sub/"), InodeBase::Mode::USER_READ);
	  setInodeByPath(KString("/dev/sub/test"), testInode);


	  // Processes
	  createDirectory(KString("/proc/"), InodeBase::Mode::USER_READ);
	  createDirectory(KString("/proc/driver"), InodeBase::Mode::USER_READ);


	  return true;
  }

  bool VirtualFileSystem::isInitialized()
  {
	  return rootNode_;
  }


  ///endregion



  ///region OpenFile

  OpenFile::OpenFile(InodeBase* inode, int flags)
	  : inode_(inode),
		flags_(flags),
		offset_(0)
  {}

  InodeBase* OpenFile::getInode() const
  {
	  return inode_;
  }

  int OpenFile::getFlags() const
  {
	  return flags_;
  }

  size_t OpenFile::getOffset() const
  {
	  return offset_;
  }

  void OpenFile::advanceOffset(size_t bytes)
  {
	  offset_ += bytes;
  }

  void OpenFile::setOffset(size_t offset)
  {
	  offset_ = offset;
  }


  ///endregion



  ///region FileDescriptorTable

  FileDescriptorTable::FileDescriptorTable()
	  : nextFd_(3) // 0, 1, 2 for cin, cout, cerr
  {}

  fd_t FileDescriptorTable::allocate(InodeBase* inode, int flags)
  {
	  fd_t fd = nextFd_++;
	  table_[fd] = OpenFile(inode, flags);
	  return fd;
  }

  void FileDescriptorTable::release(fd_t fd)
  {
	  table_.erase(fd);
  }

  OpenFile* FileDescriptorTable::getOpenFile(fd_t fd)
  {
	  auto it = table_.find(fd);
	  return (it != table_.end()) ? &it->second : nullptr;
  }

  ///endregion








}
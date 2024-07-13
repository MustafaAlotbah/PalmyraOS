
#include "core/peripherals/ATA.h"
#include "core/peripherals/Logger.h"
#include "core/SystemClock.h"
#include "libs/memory.h"


namespace PalmyraOS::kernel
{


  ATA::ATA(uint16_t portBase, Type deviceType)
	  : basePort_(portBase),
		dataPort_(portBase),
		errorPort_(portBase + 1),
		sectorCountPort_(portBase + 2),
		lbaLowPort_(portBase + 3),
		lbaMidPort_(portBase + 4),
		lbaHighPort_(portBase + 5),
		devicePort_(portBase + 6),
		commandPort_(portBase + 7),
		controlPort_(portBase + 0x206),
		deviceType_(deviceType)
  {
	  // Initialize strings to null characters
	  memset(serialNumber_, '\0', sizeof(serialNumber_));
	  memset(firmwareVersion_, '\0', sizeof(firmwareVersion_));
	  memset(modelNumber_, '\0', sizeof(modelNumber_));
  }

  bool ATA::identify(uint32_t timeout)
  {

	  // Early exit if the device is not present
	  if (!isDevicePresent())
	  {
		  LOG_ERROR("ATA Port (0x%X) %s: No Device detected.", basePort_, toString(deviceType_));
		  return false;
	  }

	  // Execute the IDENTIFY command
	  if (!executeCommand(Command::Identify, 0, 0, nullptr, timeout))
	  {
		  LOG_ERROR("ATA Port (0x%X) %s: Identify command failed.", basePort_, toString(deviceType_));
		  return false;
	  }

	  // Read the 512-byte identity data
	  uint16_t buffer[256];
	  for (unsigned short& word : buffer)
	  {
		  word = dataPort_.read();
	  }

	  // Extract and store Serial Number
	  extractString(buffer, serialNumber_, 10, 20);

	  // Extract and store Firmware Version
	  extractString(buffer, firmwareVersion_, 23, 8);

	  // Extract and store Model Number
	  extractString(buffer, modelNumber_, 27, 40);

	  // Calculate Total Storage Space in MiB using LBA28
	  sectors28Bit_ = (uint32_t)buffer[61] << 16 | buffer[60];
	  storageSize_  = sectors28Bit_ * 512;

	  // Log some information about the device once identifier
	  LOG_INFO("ATA Port (0x%X) %s: Device Identified: "
			   "Model [%s], Serial [%s], Firmware [%s], Sectors [%zu], Space [%zu MiB]",
			   basePort_, toString(deviceType_),
			   getModelNumber(),
			   getSerialNumber(),
			   getFirmwareVersion(),
			   getSectors28Bit(),
			   getStorageSize() / 1048576 // MiB
	  );

	  return true;
  }

  void ATA::extractString(const uint16_t* source, char* dest, int start, int lengthBytes)
  {
	  for (int i        = 0; i < lengthBytes / 2; ++i)
	  {
		  // Each 16-bit word contains two characters (high byte and low byte)
		  dest[2 * i]     = (char)(source[start + i] >> 8);
		  dest[2 * i + 1] = (char)(source[start + i] & 0xFF);
	  }
	  dest[lengthBytes] = '\0';     // Null terminate the string
  }

  bool ATA::readSector(uint32_t logicalBlockAddress, uint8_t* buffer, uint32_t timeout)
  {
	  // Early exit if the device is not present
	  if (!isDevicePresent())
	  {
		  LOG_INFO("ATA: No Device detected.");
		  return false;
	  }

	  return executeCommand(Command::ReadSectors, logicalBlockAddress, 1, buffer, timeout) && checkStatus();
  }

  bool ATA::writeSector(uint32_t logicalBlockAddress, const uint8_t* buffer, uint32_t timeout)
  {
	  // Early exit if the device is not present
	  if (!isDevicePresent())
	  {
		  LOG_INFO("ATA: No Device detected.");
		  return false;
	  }

	  return executeCommand(Command::WriteSectors, logicalBlockAddress, 1, const_cast<uint8_t*>(buffer), timeout)
		  && checkStatus();
  }

  bool ATA::waitForBusy(uint32_t timeout)
  {
	  // Get the current tick count
	  auto start = SystemClock::getTicks();

	  // Calculate the target tick count for timeout
	  uint64_t targetTicks = start + timeout * SystemClock::getFrequency() / 1'000;

	  // Loop until the BSY flag is cleared or timeout occurs
	  while (commandPort_.read() & 0x80) // BSY Flag
	  {
		  // Check if the current tick count exceeds the target tick count
		  if (SystemClock::getTicks() > targetTicks) return false;
	  }
	  return true;
  }

  bool ATA::waitForReady(uint32_t timeout)
  {
	  // Get the current tick count
	  auto start = SystemClock::getTicks();

	  // Calculate the target tick count for timeout
	  uint64_t targetTicks = start + timeout * SystemClock::getFrequency() / 1'000;

	  // Loop until the DRQ flag is cleared or timeout occurs
	  while (!(commandPort_.read() & 0x08))
	  {

		  // Check if the current tick count exceeds the target tick count
		  if (SystemClock::getTicks() > targetTicks) return false;
	  }
	  return true;
  }

  bool ATA::isDevicePresent()
  {
	  // Set the device selection bit (bit 4) and the LBA mode bit (bit 6)
	  devicePort_.write(deviceType_ == Type::Master ? 0xA0 : 0xB0);

	  // Clear any previous control settings
	  controlPort_.write(0);

	  return (commandPort_.read() != 0x00);
  }

  uint16_t ATA::swapBytes(uint16_t word)
  {
	  return (word >> 8) | (word << 8);
  }

  void ATA::selectDevice(uint32_t logicalBlockAddress)
  {
	  devicePort_.write((deviceType_ == Type::Master ? 0xE0 : 0xF0) | ((logicalBlockAddress >> 24) & 0x0F));
  }

  void ATA::setLBA(uint32_t logicalBlockAddress)
  {
	  lbaLowPort_.write(logicalBlockAddress & 0xFF);
	  lbaMidPort_.write((logicalBlockAddress >> 8) & 0xFF);
	  lbaHighPort_.write((logicalBlockAddress >> 16) & 0xFF);
  }

  bool ATA::executeCommand(
	  Command command,
	  uint32_t logicalBlockAddress,
	  uint8_t sectorCount,
	  uint8_t* buffer,
	  uint32_t timeout
  )
  {
	  selectDevice(logicalBlockAddress);
	  sectorCountPort_.write(sectorCount);
	  setLBA(logicalBlockAddress);
	  commandPort_.write(static_cast<uint8_t>(command));

	  timeout = timeout > 5000 ? 5000 : timeout;    // Maximum 5 seconds timeout

	  if (!waitForBusy(timeout) || !waitForReady(timeout))
	  {
		  LOG_ERROR("ATA Port (0x%X) %s: Command 0x%X timed out.",
					basePort_, toString(deviceType_), static_cast<uint8_t>(command)
		  );
		  return false;
	  }

	  if (command == Command::WriteSectors)
	  {
		  for (int i = 0; i < 256; ++i)
		  {
			  uint16_t data = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
			  dataPort_.write(data);
		  }
	  }
	  else if (command == Command::ReadSectors)
	  {
		  for (int i = 0; i < 256; ++i)
		  {
			  uint16_t data = dataPort_.read();
			  buffer[i * 2]     = data & 0xFF;
			  buffer[i * 2 + 1] = (data >> 8) & 0xFF;
		  }
	  }

	  return true;
  }

  bool ATA::checkStatus()
  {
	  uint8_t status = commandPort_.read();
	  if (status & 0x01) // Check ERR bit
	  {
		  LOG_ERROR("ATA Port (0x%X) %s: Error occurred. Status: 0x%X",
					basePort_, toString(deviceType_), status
		  );
		  return false;
	  }
	  return true;
  }



  // Getters


  const char* ATA::getSerialNumber() const
  {
	  return serialNumber_;
  }

  const char* ATA::getFirmwareVersion() const
  {
	  return firmwareVersion_;
  }

  const char* ATA::getModelNumber() const
  {
	  return modelNumber_;
  }

  uint64_t ATA::getStorageSize() const
  {
	  return storageSize_;
  }

  uint32_t ATA::getSectors28Bit() const
  {
	  return sectors28Bit_;
  }

  uint32_t ATA::getSectors48Bit() const
  {
	  return sectors48Bit_;
  }

  bool ATA::supportsLBA48() const
  {
	  return supports48Bit_;
  }

  constexpr const char* ATA::toString(ATA::Type type)
  {
	  switch (type)
	  {
		  case Type::Master:
			  return "Master";
		  case Type::Slave:
			  return "Slave";
		  default:
			  return "Unknown";
	  }
  }

}
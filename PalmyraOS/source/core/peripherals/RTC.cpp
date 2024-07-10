

#include "core/peripherals/RTC.h"
#include "core/SystemClock.h"
#include "core/files/VirtualFileSystem.h"
#include "libs/memory.h"        // memcpy

#include "palmyraOS/time.h" // rtc_time struct


uint8_t  PalmyraOS::kernel::RTC::seconds_{};
uint8_t  PalmyraOS::kernel::RTC::minutes_{};
uint8_t  PalmyraOS::kernel::RTC::hours_{};
uint8_t  PalmyraOS::kernel::RTC::day_{};
uint8_t  PalmyraOS::kernel::RTC::month_{};
uint16_t PalmyraOS::kernel::RTC::year_{};
uint64_t PalmyraOS::kernel::RTC::lastUpdated_{};
uint64_t PalmyraOS::kernel::RTC::rtc_time_{};

// Convert BCD to decimal
uint8_t bcd_to_dec(uint8_t bcd)
{
	// (((bcd / 16) * 10) + (bcd % 16));
	return (((bcd >> 4) * 10) + (bcd % 16));
}

void PalmyraOS::kernel::RTC::initialize()
{
	if (!vfs::VirtualFileSystem::isInitialized())
	{
		kernel::kernelPanic("Called '%s' before initialized the VFS", __PRETTY_FUNCTION__);
	}

	// just 24 hours mode, BCD activated
	kernel::CMOS::write(cmosControlRegister, mode24Hour);

	// initialize /dev/rtc
	initializeVFSElements();
}

void PalmyraOS::kernel::RTC::update()
{

	seconds_ = bcd_to_dec(kernel::CMOS::read(RTC_Second));
	minutes_ = bcd_to_dec(kernel::CMOS::read(RTC_Minute));
	hours_   = bcd_to_dec(kernel::CMOS::read(RTC_Hour));
	day_     = bcd_to_dec(kernel::CMOS::read(RTC_Day));
	month_   = bcd_to_dec(kernel::CMOS::read(RTC_Month));
	year_    = (bcd_to_dec(kernel::CMOS::read(RTC_Century)) * 100) +
		bcd_to_dec(kernel::CMOS::read(RTC_Year));

	rtc_time_ = toEpochTime(year_, month_, day_, hours_, minutes_, seconds_);
}

uint64_t PalmyraOS::kernel::RTC::now()
{
	uint64_t cpu_now_millis = SystemClock::getMilliseconds();

	// update if at least 250 ms have passed
	if (cpu_now_millis - lastUpdated_ > 250)
	{
		update();
		lastUpdated_ = cpu_now_millis;
	}

	return rtc_time_;
}

bool PalmyraOS::kernel::RTC::isLeapYear(uint32_t year)
{
	return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

uint16_t PalmyraOS::kernel::RTC::daysInYear(uint32_t year)
{
	return isLeapYear(year) ? 366 : 365;
}

uint32_t PalmyraOS::kernel::RTC::daysSinceEpoch(uint16_t year, uint8_t month, uint8_t day)
{
	int      days             = 0;
	for (int y                = epochYear; y < year; ++y)
	{
		days += daysInYear(y);
	}
	int      days_in_months[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if (isLeapYear(year))
	{
		days_in_months[1] = 29; // February has 29 days in a leap year
	}
	for (int m = 0; m < month - 1; ++m)
	{
		days += days_in_months[m];
	}
	days += day - 1; // Subtract 1 because the day is inclusive
	return days;
}

uint64_t PalmyraOS::kernel::RTC::toEpochTime(
	uint16_t year,
	uint8_t month,
	uint8_t day,
	uint8_t hour,
	uint8_t minute,
	uint8_t second
)
{
	uint32_t days    = daysSinceEpoch(year, month, day);
	uint64_t seconds =
				 days * secondsInDay +
					 hour * secondsInHour +
					 minute * secondsInMinute +
					 second;
	return seconds;
}

bool PalmyraOS::kernel::RTC::initializeVFSElements()
{
	// Create a test inode with a lambda function for reading test string
	auto rtcNode = kernel::heapManager.createInstance<vfs::FunctionInode>(read, nullptr, ioctl);
	if (!rtcNode) return false;

	// Set the test inode at "/dev/test"
	vfs::VirtualFileSystem::setInodeByPath(KString("/dev/rtc"), rtcNode);
	vfs::VirtualFileSystem::setInodeByPath(KString("/proc/driver/rtc"), rtcNode);

	return true;
}

int PalmyraOS::kernel::RTC::ioctl(int request, void* arg)
{
	// update
	now();

	auto* destination_ = (rtc_time*)arg;
	destination_->tm_sec  = seconds_;
	destination_->tm_min  = minutes_;
	destination_->tm_hour = hours_;
	destination_->tm_mday = day_;
	destination_->tm_mon  = month_ - 1;    // tm_mon is 0-based
	destination_->tm_year = year_ - 1900; // tm_year is years since 1900

	// tm_wday is days since Sunday, with Jan 1, 1970 being a Thursday
	destination_->tm_wday = static_cast<int>((daysSinceEpoch(year_, month_, day_) + 4) % 7);

	// tm_yday is days since Jan 1
	destination_->tm_yday = static_cast<int>(
		daysSinceEpoch(year_, month_, day_) - daysSinceEpoch(year_, 1, 1)
	);

	// No daylight saving time information available
	destination_->tm_isdst = -1;

	return 0;
}

size_t PalmyraOS::kernel::RTC::read(char* buffer, size_t size, size_t offset)
{
	// Update the RTC time to get the latest values
	update();

	// Create a string with the desired format
	char   output[512];
	size_t written = snprintf(
		output, sizeof(output),
		""
		"rtc_time\t: %02u:%02u:%02u\n"
		"rtc_date\t: %04u-%02u-%02u\n"
		"alrm_time\t: 00:00:00\n"
		"alrm_date\t: 2024-07-10\n" // TODO: add more information
		"alarm_IRQ\t: no\n"
		"alrm_pending\t: no\n"
		"update IRQ enabled\t: no\n"
		"periodic IRQ enabled\t: no\n"
		"periodic IRQ frequency\t: 1024\n"
		"max user IRQ frequency \t: 64\n"
		"24hr\t: yes\n"
		"periodic_IRQ\t: no\n"
		"update_IRQ\t: no\n"
		"HPET_emulated\t: no\n"
		"BCD\t: yes\n"
		"DST_enable\t: no\n"
		"periodic_freq\t: 1024\n"
		"batt_status\t: okay\n",
		hours_, minutes_, seconds_,
		year_, month_, day_
	);

	// Ensure the output fits in the buffer, considering the offset
	if (offset >= written) return 0;

	size_t available = written - offset;
	size_t to_copy   = available < size ? available : size;
	memcpy(buffer, output + offset, to_copy);

	return to_copy;
}

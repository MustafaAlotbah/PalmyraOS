

#include "core/peripherals/RTC.h"
#include "core/SystemClock.h"


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
	return (((bcd >> 4) * 10) + (bcd & 0b111));
}

void PalmyraOS::kernel::RTC::initialize()
{
	// just 24 hours mode, BCD activated
	kernel::CMOS::write(cmosControlRegister, mode24Hour);
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

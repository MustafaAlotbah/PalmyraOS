
#pragma once

#include "core/peripherals/CMOS.h"


// Assume specific register addresses and values for a typical RTC module
#define RTC_I2C_ADDRESS 0x68  // Example I2C address for an RTC like DS3231

// RTC Register addresses
#define RTC_SECONDS_REG  0x00
#define RTC_MINUTES_REG  0x01
#define RTC_HOURS_REG    0x02
#define RTC_DAY_REG      0x03
#define RTC_DATE_REG     0x04
#define RTC_MONTH_REG    0x05
#define RTC_YEAR_REG     0x06
#define RTC_CONTROL_REG  0x0E


uint8_t bcd_to_dec(uint8_t bcd);

namespace PalmyraOS::kernel
{

  constexpr uint8_t cmosControlRegister = 0x0b;
  constexpr uint8_t mode24Hour          = 0x2; // 24 hour mode

  constexpr uint8_t RTC_UpdateInProgress = 0x80;
  constexpr uint8_t RTC_Century          = 0x32;
  constexpr uint8_t RTC_Year             = 0x09;
  constexpr uint8_t RTC_Month            = 0x08;
  constexpr uint8_t RTC_Day              = 0x07;
  constexpr uint8_t RTC_Hour             = 0x04;
  constexpr uint8_t RTC_Minute           = 0x02;
  constexpr uint8_t RTC_Second           = 0x00;

  constexpr uint32_t secondsInDay    = 86400;
  constexpr uint32_t secondsInHour   = 3600;
  constexpr uint32_t secondsInMinute = 60;
  constexpr uint32_t epochYear       = 1970;

  class RTC
  {
   public:
	  static void initialize();

	  static void update();

	  static uint64_t now();

	  static bool initializeVFSElements();

	  static uint64_t getSecondsOfDay();
   public:
	  static bool isLeapYear(uint32_t year);
	  static uint16_t daysInYear(uint32_t year);
	  static uint32_t daysSinceEpoch(uint16_t year, uint8_t month, uint8_t day);
	  static uint64_t toEpochTime(
		  uint16_t year,
		  uint8_t month,
		  uint8_t day,
		  uint8_t hour,
		  uint8_t minute,
		  uint8_t second
	  );
   private:
	  static int ioctl(int request, void* arg);
	  static size_t read(char* buffer, size_t size, size_t offset);
   private:
	  static uint8_t  seconds_;
	  static uint8_t  minutes_;
	  static uint8_t  hours_;
	  static uint8_t  day_;
	  static uint8_t  month_;
	  static uint16_t year_;

	  static uint64_t rtc_time_;    // epoch format time
	  static uint64_t lastUpdated_;    // in CPU time

  };


}

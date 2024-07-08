/*
 * Part of the API of PalmyraOS
 * Partially POSIX compliant (under construction)
 * */


#pragma once

#include <cstdint>


#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID 3

struct timespec
{
	uint64_t tv_sec;  // Seconds
	uint64_t tv_nsec; // Nanoseconds
};

struct rtc_time
{
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

/**
 * @brief Retrieves the current time for the specified clock.
 *
 * This function fills in the given timespec structure with the current time of the specified clock.
 *
 * @param clk_id The clock ID. CLOCK_MONOTONIC is currently supported.
 * @param tp Pointer to a timespec structure where the current time will be stored.
 * @return int Returns 0 on success, or a negative value on error.
 */
int clock_gettime(uint32_t clk_id, timespec* tp);

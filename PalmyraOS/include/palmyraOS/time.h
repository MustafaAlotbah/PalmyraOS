/*
 * Part of the API of PalmyraOS
 * Partially POSIX compliant (under construction)
 * */


#pragma once

#include <cstdint>
//#include <sys/types.h>

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID 3

struct timespec // TODO adjust time_t, long (i.e. uint32_t x2)
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

/**
 * @brief Suspends the execution of the calling thread for a specified duration.
 *
 * This function suspends the calling thread until either the time interval specified
 * by *req has elapsed, or a signal is delivered to the thread and its action is to
 * invoke a signal-catching function or to terminate the thread. The suspension time
 * can be specified as either an absolute time or a relative time, depending on the
 * value of the flags parameter.
 *
 * @param clock_id The clock ID to be used for measuring time. It can be one of:
 * - CLOCK_REALTIME: System-wide clock that measures real (wall-clock) time.
 * - CLOCK_MONOTONIC: Clock that cannot be set and represents monotonic time since an unspecified starting point.
 * - CLOCK_PROCESS_CPUTIME_ID: High-resolution per-process timer from the CPU.
 * - CLOCK_THREAD_CPUTIME_ID: Thread-specific CPU-time clock.
 * @param flags The flags that modify the behavior of the function:
 * - 0: Sleep for a relative interval (the duration specified in req).
 * - TIMER_ABSTIME: Sleep until the absolute time specified in req.
 * @param req Pointer to a timespec structure that specifies the desired sleep interval (if flags is 0) or the absolute wake-up time (if flags is TIMER_ABSTIME).
 * @param rem Pointer to a timespec structure where the remaining time is stored if the sleep is interrupted by a signal. If rem is NULL, the remaining time is not returned.
 * @return int Returns 0 on success. On failure, returns -1 and sets errno to indicate the error:
 * - EINVAL: The value in the tv_nsec field of req is not in the range 0 to 999999999 or req is NULL.
 * - EINTR: The sleep was interrupted by a signal handler.
 */
int clock_nanosleep(uint32_t clock_id, int flags, const struct timespec* req, struct timespec* rem);
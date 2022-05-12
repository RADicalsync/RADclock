/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Linker script for 64-bit vDSO.
 * We #include the file to define the layout details.
 *
 * This file defines the version script giving the user-exported symbols in
 * the DSO.
 */

#define BUILD_VDSO64

#include "vdso-layout.lds.S"

/*
 * This controls what userland symbols we export from the vDSO.
 */
VERSION {
	LINUX_2.6 {
	global:
		clock_gettime;
		__vdso_clock_gettime;
		gettimeofday;
		__vdso_gettimeofday;
		getcpu;
		__vdso_getcpu;
		time;
		__vdso_time;
		ffclock_getcounter;
		__vdso_ffclock_getcounter;
		ffclock_getcounter_latency;
		__vdso_ffclock_getcounter_latency;
	local: *;
	};
}

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Linker script for x32 vDSO.
 * We #include the file to define the layout details.
 *
 * This file defines the version script giving the user-exported symbols in
 * the DSO.
 */

#define BUILD_VDSOX32

#include "vdso-layout.lds.S"

/*
 * This controls what userland symbols we export from the vDSO.
 */
VERSION {
	LINUX_2.6 {
	global:
		__vdso_clock_gettime;
		__vdso_gettimeofday;
		__vdso_getcpu;
		__vdso_time;
		__vdso_ffclock_getcounter;
		__vdso_ffclock_getcounter_latency;
	local: *;
	};
}

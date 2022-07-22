/*
 * Copyright (C) 2006 The RADclock Project (see AUTHORS file)
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "../config.h"
#ifdef WITH_FFKERNEL_LINUX

#include <asm/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <string.h>

#include "radclock.h"
#include "radclock-private.h"
#include "kclock.h"			// not needed yet, but may end up including an equivalant to timeffc.h
#include "logger.h"


/**
 * TODO LINUX:
 *  Consider moving the vcount stamp to ancilary data
 *  - Would mean moving away from standard pcap (maybe to libtrace, which
 *  already supports ancillary data for the sw stamp, or patching pcap to use it
 *  - This would avoid 2 syscalls (one of sw stamp, one for vcount stamp)
 *  - UPDATE: new packet MMAP support should solve all of this
 *
 *  Consider moving the mode to a sockopt
 *  - This would just be cleaner and the right thing to do, no performance benefit
 */

int
found_ffwd_kernel_version (void)
{
	int version = -1;
	FILE *fd = NULL;

	fd = fopen ("/sys/devices/system/ffclock/ffclock0/version_ffclock", "r");
	if (fd) {
		fscanf(fd, "%d", &version);
		fclose(fd);
	} else {
		/* This is the old way we used before explicit versioning */
		fd = fopen ("/proc/sys/net/core/radclock_default_tsmode", "r");
		if (fd) {
			fclose(fd);
			version = 0;
		} else	// no kernel support
			version = -1;
	}

	if (version > -1)
		logger(RADLOG_NOTICE, "Feed-Forward kernel detected (version: %d)", version);

	/* A quick reminder for the administrator. */
	switch (version) {
	case 0:
	case 1:
		logger(RADLOG_WARNING, "The Feed-Forward kernel support is very old, update! ");
		break;
	case 2:
		break;
	case -1:
	default:
		logger(RADLOG_NOTICE, "Feed-Forward kernel support not recognized");
		break;
	}
	return (version);
}


/* Need to check that the passthrough mode is enabled and that the counter can
 * do the job. The latter is a bit "hard coded"
 */
int
has_vm_vcounter(struct radclock *clock)
{
	int bypass_active = 0;
	char clocksource[32];
	FILE *fd = NULL;
	char *pass;

	pass = "/sys/devices/system/ffclock/ffclock0/bypass_ffclock";

	fd = fopen (pass, "r");
	if (!fd) {
		logger(RADLOG_ERR, "Cannot open bypass_ffclock from sysfs");
		return (0);
	}
	fscanf(fd, "%d", &bypass_active);
	fclose(fd);

	if (bypass_active == 0) {
		logger(RADLOG_ERR, "ffclock not in bypass mode. Cannot init virtual machine mode");
		return (0);
	}
	logger(RADLOG_NOTICE, "Found ffclock in bypass mode");

	fd = fopen ("/sys/devices/system/ffclock/ffclock0/bypass_ffclock", "r");
	if (!fd) {
		logger(RADLOG_WARNING, "Cannot open bypass_ffclock from sysfs");
		return (1);
	}
	fscanf(fd, "%s", &clocksource[0]);
	fclose(fd);

	if ( (strcmp(clocksource, "tsc") != 0) && (strcmp(clocksource, "xen") != 0) )
		logger(RADLOG_WARNING, "Clocksource is neither tsc nor xen. There must be something wrong!!");
	else
		logger(RADLOG_WARNING, "Clocksource is %s", clocksource);

	return (1);
}


/* RDTSC detection and rdtsc() definition
 *  configure.ac tries to flag if there is no TSC via NO_TSC_ONPLATFORM, for
 *  example, on arm this is defined.
 *  If it can't, it tries to find a header file where rdtsc() may be defined.
 *  If this fails, we define a rdtsc() manually here, hoping that in fact a
 *  TSC exists, otherwise the assembler will fail!
 */
#define DefinedLocalRDTSC 0		// diagnostics only, 0 means system rdtsc found

#ifdef NO_TSC_ONPLATFORM
#define DefinedLocalRDTSC 2
static inline uint64_t rdtsc(void) {
        return 0;
}
#else	// there may be a TSC, try to find a rdtsc()
#ifdef HAVE_RDTSC_ASM			// DefinedLocalRDTSC will remain 0
#	include <asm/msr.h>			// file is there, but no functions are in it
#else
#define DefinedLocalRDTSC 1
	static inline uint64_t	// same definition as in FreeBSD
	rdtsc(void)
	{
		 u_int32_t low, high;
		 __asm __volatile("rdtsc" : "=a" (low), "=d" (high));
		 return (low | ((u_int64_t)high << 32));
	}
#endif
#endif



inline vcounter_t
radclock_readtsc(void)
{
	return (vcounter_t) rdtsc();
}

/* First argument not used, needed to match template for clock->get_vcounter */
inline int
radclock_get_vcounter_rdtsc(struct radclock *clock, vcounter_t *vcount)
{
	*vcount = radclock_readtsc();
	return (0);
}


int
radclock_init_vcounter_syscall(struct radclock *clock)
{
	switch (clock->kernel_version) {
	case 0:
	case 1:
	case 2:
		clock->syscall_get_vcounter = LINUX_SYSCALL_GET_VCOUNTER; // From config.h
		logger(RADLOG_NOTICE, "registered get_vcounter syscall at %d",
				clock->syscall_get_vcounter);
		break;
	default:
		logger(RADLOG_ERR, "ffclock_getcounter syscall unavailable");
		return (1);
	}

	return (0);
}


int
radclock_get_vcounter_syscall(struct radclock *clock, vcounter_t *vcount)
{
	int ret;

	if (vcount == NULL)
		return (-1);

	//ret = get_vcounter(vcount);		// userspace fn name fails for some reason..
	// glibc provided wrapper autogenerated?
	//  this name is listed in syscall_64.tbl
	//  listed as a weak alias to  vdso call  vclockgettime.c

	ret = syscall(clock->syscall_get_vcounter, vcount);	// direct call

	if (ret < 0) {
		logger(RADLOG_ERR, "error on syscall get_vcounter: %s", strerror(errno));
		return (-1);
	}
	return (0);
}

/* Get the current hardware counter used by the kernel.
 * If it has changed, record the new one and flag this.
 * Return codes:  {-1,0,1} = {couldn't access, no change or initialized, changed}
 * Assumes KV>0 .
 */
int
get_currentcounter(struct radclock *clock)
{
	char hw_counter[32];
	FILE *fd = NULL;

	/* Get name of current counter */
	fd = fopen ("/sys/devices/system/clocksource/clocksource0/current_clocksource", "r");
	if (!fd) {
		logger(RADLOG_ERR, "Cannot open current_clocksource from sysfs");
		return (-1);
	}
	fscanf(fd, "%s", &hw_counter[0]);
	fclose(fd);

	/* If different from registered value, register the new one */
	if (strcmp(clock->hw_counter, hw_counter) != 0) {
		if ( clock->hw_counter[0] == '\0' ) {
			logger(RADLOG_NOTICE, "Hardware counter used is %s", hw_counter);
			strcpy(clock->hw_counter, hw_counter);
		}	else {
			logger(RADLOG_WARNING, "Hardware counter has changed : (%s -> %s)",
									clock->hw_counter, hw_counter);
			strcpy(clock->hw_counter, hw_counter);
			return (1);
		}
	}

	return (0);
}


/* Check the available counter and set it up.
 * Check to see if we can use fast rdtsc() timestamping from userland.
 * Otherwise fall back to syscalls
 */
int
radclock_init_vcounter(struct radclock *clock)
{
	int bypass_active;
	FILE *fd = NULL;

	/* Retrieve available and current sysclock.
	 * Not the job of this function, but a reasonable place to do this.
	 * ** SYSclock choice concept not yet implemented for Linux **
	 */
	logger(RADLOG_NOTICE, "Available sysclocks: traditional (sysclock not implemented)");

	/* Report result of rdtsc search */
	switch (DefinedLocalRDTSC) {
	case 0:
		logger(RADLOG_NOTICE, "System rdtsc() found");
		break;
	case 1:
		logger(RADLOG_NOTICE, "System rdtsc() not found, have defined locally in daemon");
		break;
	case 2:
		logger(RADLOG_NOTICE, "Platform has no TSC, dummy rdtsc() defined for compilation");
	}

	/* Retrieve and register name of counter used by the kernel timecounter */
	if ( get_currentcounter(clock) == -1 )
		return (-1);

//	fd = fopen ("/sys/devices/system/clocksource/clocksource0/current_clocksource", "r");
//	if (!fd) {
//		logger(RADLOG_ERR, "Cannot open current_clocksource from sysfs");
//		return (-1);
//	}
//	fscanf(fd, "%s", &clock->hw_counter[0]);
//	fclose(fd);
//	logger(RADLOG_NOTICE, "Hardware counter used is %s", clock->hw_counter);


	/* Retrieve value of kernel PT mode */
	bypass_active = 0;
	switch (clock->kernel_version) {
	case 0:
		bypass_active = 0;
		break;

	case 1:
	case 2:
		fd = fopen ("/sys/devices/system/ffclock/ffclock0/bypass_ffclock", "r");
		if (!fd) {
			logger(RADLOG_ERR, "Cannot open bypass_ffclock from sysfs");
			return (-1);
		}
		fscanf(fd, "%d", &bypass_active);
		fclose(fd);
		break;
	}
	logger(RADLOG_NOTICE, "Kernel bypass mode is %d", bypass_active);


	/* Make decision on mode and assign corresponding get_vcounter function.
	 *  Here activateBP authorizes deamons to activate the kernal bypass option if
	 *  appropriate, but not to deactivate it.
	 *  Sufficient for now to keep activateBP an internal parameter (see algo Doc)
	 *  If want to make a config parameter later, must read it in clock_init_live
	 *  where handle->conf is available and pass here as 2nd argument.
	 */
	int activateBP = 0;	// user's bypass intention, may set the kernal option!
	if ( (bypass_active == 0 && activateBP == 0) || clock->kernel_version < 2) {
		clock->get_vcounter = &radclock_get_vcounter_syscall;
		logger(RADLOG_NOTICE, "Initialising get_vcounter with syscall.");
		return (0);
	}

	/* Activate if not already, provided it passes basic tests [ needs KV>1 ]*/
	if ( strcmp(clock->hw_counter, "tsc") != 0 ) {		// could be TSC on some systems?
		logger(RADLOG_NOTICE, "Bypass active/requested but counter seems wrong.");
		clock->get_vcounter = &radclock_get_vcounter_syscall;
		logger(RADLOG_NOTICE, "Initialising radclock_get_vcounter using syscall.");
	} else {
		logger(RADLOG_NOTICE, "Counter seems to be a TSC (assumed reliable)");
		if (bypass_active == 0) {
			logger(RADLOG_NOTICE, "Activating kernel bypass mode as requested");
			bypass_active = 1;
			fd = fopen ("/sys/devices/system/ffclock/ffclock0/bypass_ffclock", "w");
			fprintf(fd, "%d", bypass_active);	// conveniently overwrites existing single char!
			fclose(fd);
		}
		clock->get_vcounter = &radclock_get_vcounter_rdtsc;
		logger(RADLOG_NOTICE, "Initialising get_vcounter using rdtsc()");
	}

	return (0);
}

#endif

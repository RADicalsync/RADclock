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
#ifdef WITH_FFKERNEL_FBSD

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>		// needed for module.h
#include <sys/module.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <net/ethernet.h>	// ETHER_HDR_LEN
#include <net/bpf.h>
#include <pcap.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>		// useful?
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stddef.h>	// offesetof macro

#include "radclock.h"
#include "radclock-private.h"
#include "kclock.h"
#include "logger.h"


/* I don't think this is needed, if HAVE_SYS_TIMEFFC_H fails,
 * would be calling the ffkernel-none.c instead, or we should be!
 */
//#ifndef HAVE_SYS_TIMEFFC_H
//int ffclock_getcounter(vcounter_t *vcount)
//{
//	*vcount = 0;
//	return EINVAL;
//}
//#endif


// TODO: move out of library and use IPC call to retrieve it if needed?
int
found_ffwd_kernel_version (void)
{
	int ret;
	int version;
	size_t size_ctl;

	size_ctl = sizeof(version);

	/* Sysctl for version 2 and 3*/
	ret = sysctlbyname("kern.sysclock.ffclock.version", &version, &size_ctl, NULL, 0);

	/* For versions <2 */
	if (ret < 0) {

		/* Sysctl for version 1 */
		ret = sysctlbyname("kern.ffclock.version", &version, &size_ctl, NULL, 0);
		if (ret < 0) {
			/* The old way we used before explicit versioning. */
			ret = sysctlbyname("net.bpf.bpf_pktcap_tsmode", &version, &size_ctl, NULL, 0);
			if (ret == 0)
				version = 0;
			else  // no kernel support
				version = -1;
		}
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
	case 3:
		break;
	case -1:
	default:
		logger(RADLOG_NOTICE, "Feed-Forward kernel support not recognized");
		break;
	}
	return (version);
}



/*
 * Need to check that the passthrough mode is enabled and that the counter can
 * do the job. The latter is a bit "hard coded"
 */
int
has_vm_vcounter(struct radclock *clock)
{
	int ret;
	int passthrough_counter = 0;
	char timecounter[32];
	size_t size_ctl;

	switch (clock->kernel_version) {
// FIXME : sysctl is there but no xen backend in official kernel yet
	case 3:
	case 2:
		size_ctl = sizeof(passthrough_counter);
		ret = sysctlbyname("kern.sysclock.ffclock.ffcounter_bypass",
		    &passthrough_counter, &size_ctl, NULL, 0);
		if (ret == -1) {
			logger(RADLOG_ERR, "Cannot find kern.sysclock.ffclock.ffcounter_bypass in sysctl");
			return (0);
		}
		break;

	case 1:
		size_ctl = sizeof(passthrough_counter);
		ret = sysctlbyname("kern.timecounter.passthrough", &passthrough_counter,
		    &size_ctl, NULL, 0);
		if (ret == -1) {
			logger(RADLOG_ERR, "Cannot find kern.timecounter.passthrough in sysctl");
			return (0);
		}
		break;

	case 0:
	default:
		return (0);
	}

	if (passthrough_counter == 0) {
		logger(RADLOG_ERR, "Timecounter not in passthrough mode. Cannot init virtual machine mode");
		return (0);
	}
	logger(RADLOG_NOTICE, "Found timecounter in pass-through mode");

	size_ctl = sizeof(timecounter);
	ret = sysctlbyname("kern.timecounter.hardware", &timecounter[0],
	    &size_ctl, NULL, 0);
	if (ret == -1) {
		logger(LOG_ERR, "Cannot find kern.timecounter.hardware in sysctl");
		return (0);
	}

	if ((strcmp(timecounter, "TSC") != 0) && (strcmp(timecounter, "ixen") != 0))
		logger(RADLOG_WARNING, "Timecounter is neither TSC nor ixen - something wrong!!");
	else
		logger(RADLOG_WARNING, "Timecounter is %s", timecounter);

	return (1);
}



/* Ensure rdtsc() is defined
 * If we can't find it, we define here based on the definition in cpufunc.h
 */
#define DefinedLocalRDTSC 0
#ifdef HAVE_RDTSC
#	ifdef HAVE_MACHINE_CPUFUNC_H
# 		include <machine/cpufunc.h>
#	else
# 		error "FreeBSD with rdtsc() defined but no machine/cpufunc.h header"
#	endif
#else
#	define DefinedLocalRDTSC 1
	static inline uint64_t	// same definition as in cpufunc.h
	rdtsc(void)
	{
		 u_int32_t low, high;
		 __asm __volatile("rdtsc" : "=a" (low), "=d" (high));
		 return (low | ((u_int64_t)high << 32));
	}
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
	return 0;
}


int
radclock_init_vcounter_syscall(struct radclock *clock)
{
	struct module_stat stat;
	int err;

	switch (clock->kernel_version) {
	case 0:
	case 1:
		stat.version = sizeof(stat);
		err = modstat(modfind("get_vcounter"), &stat);
		if (err < 0) {
			logger(RADLOG_ERR, "Error on modstat (get_vcounter syscall): %s",
				strerror(errno));
			logger(RADLOG_ERR, "Is the radclock kernel module loaded?");
			return (1);
		}
		clock->syscall_get_vcounter = stat.data.intval;
		logger(RADLOG_NOTICE, "Registered get_vcounter syscall at %d",
			clock->syscall_get_vcounter);
		break;

	/* kernel provides ffclock_getcounter through libc */
	case 2:
	case 3:
		logger(RADLOG_NOTICE, "ffclock_getcounter syscall available");
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

	switch (clock->kernel_version) {
	case 0:
	case 1:
		ret = syscall(clock->syscall_get_vcounter, vcount);
		break;
	case 2:
	case 3:
		ret = ffclock_getcounter(vcount);
		break;
	default:
		ret = -1;
		break;
	}

	if (ret < 0) {
		logger(RADLOG_ERR, "error on syscall ffclock_getcounter: %s", strerror(errno));
		return (-1);
	}

	return (0);
}

/* Get the current hardware counter used by the kernel.
 * If it has changed, record the new one and flag this.
 * Return codes:  {-1,0,1} = {couldn't access, no change or initialized, changed}
 * Assumes KV>1 .
 */
int get_currentcounter(struct radclock *clock)
{
	char hw_counter[32];
	size_t size_ctl;
	int ret;

	/* Get name of current counter */
	size_ctl = sizeof(hw_counter);
	ret = sysctlbyname("kern.timecounter.hardware", &hw_counter[0], &size_ctl, NULL, 0);
	if (ret == -1) {
		logger(RADLOG_ERR, "Cannot find kern.timecounter.hardware from sysctl");
		return (-1);
	}

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


/*
 * Check to see if we can use fast rdtsc() timestamping from userland.
 * Otherwise fall back to syscalls
 */
int
radclock_init_vcounter(struct radclock *clock)
{
	size_t size_ctl;
	int bypass_active;
	int ret;

	/* Retrieve available and current sysclock.
	 * Not the job of this function, but a reasonable place to do this. */
	char nameavail[32];
	size_ctl = sizeof(nameavail);
	sysctlbyname("kern.sysclock.available", &nameavail[0], &size_ctl, NULL, 0);
	logger(RADLOG_NOTICE, "Available sysclocks: %s", nameavail);

	size_ctl = sizeof(nameavail);
	sysctlbyname("kern.sysclock.active", &nameavail[0], &size_ctl, NULL, 0);
	logger(RADLOG_NOTICE, "\t Active sysclock: %s", nameavail);
	//logger(RADLOG_NOTICE, "\t Active sysclock: %s [sn = %u]", nameavail, strlen(nameavail));


	/* Retrieve and register name of counter used by the kernel timecounter */
	if ( get_currentcounter(clock) == -1 )
		return (-1);

	/* Retrieve value of kernel PT mode */
	bypass_active = 0;
	switch (clock->kernel_version) {
	case 0:
		bypass_active = 0;
		break;

	case 1:
		size_ctl = sizeof(bypass_active);
		ret = sysctlbyname("kern.timecounter.passthrough", &bypass_active, &size_ctl, NULL, 0);
		if (ret == -1) {
			logger(RADLOG_ERR, "Cannot find kern.timecounter.passthrough from sysctl");
			return (-1);
		}
		break;

	case 2:
	case 3:
		size_ctl = sizeof(bypass_active);
		ret = sysctlbyname("kern.sysclock.ffclock.ffcounter_bypass", &bypass_active, &size_ctl, NULL, 0);
		if (ret == -1) {
			logger(RADLOG_ERR, "Cannot find kern.sysclock.ffclock.ffcounter_bypass");
			bypass_active = 0;
			return (-1);
		}
	}
	logger(RADLOG_NOTICE, "Kernel bypass mode is %d", bypass_active);


	/* Make decision on mode and assign corresponding get_vcounter function.
	 *  Here activateBP authorizes deamons to activate the kernal PT option if
	 *  appropriate, but not to deactivate it.
	 *  Sufficient for now to keep activateBP an internal parameter (see algo Doc)
	 *  If want to make a config parameter later, must read it in clock_init_live
	 *  where handle->conf is available and pass here as 2nd argument.
	 */
	int activateBP = 0;		// user's intention regarding passthrough=bypass

	if ( DefinedLocalRDTSC )
		logger(RADLOG_NOTICE, "rdtsc() not found, have defined locally in daemon");
	else
		logger(RADLOG_NOTICE, "system rdtsc() found");

	if ( (bypass_active == 0 && activateBP == 0) || clock->kernel_version < 3) {
		clock->get_vcounter = &radclock_get_vcounter_syscall;
		logger(RADLOG_NOTICE, "Initialising get_vcounter with syscall.");
		//goto profileit;
		return (0);
	}

	/* Activate if not already, provided it passes basic tests [ needs KV>2 ]*/
	if ( strcmp(clock->hw_counter, "TSC") != 0 ) {
		logger(RADLOG_NOTICE, "Bypass active/requested but counter seems wrong.");
		clock->get_vcounter = &radclock_get_vcounter_syscall;
		logger(RADLOG_NOTICE, "Initialising radclock_get_vcounter using syscall.");
	} else {
		logger(RADLOG_NOTICE, "Counter seems to be a TSC (assumed reliable)");
		if (bypass_active == 0) {
			logger(RADLOG_NOTICE, "Activating kernel bypass mode as requested");
			bypass_active = 1;
			sysctlbyname("kern.sysclock.ffclock.ffcounter_bypass", NULL, NULL, &bypass_active, size_ctl);
		}
		clock->get_vcounter = &radclock_get_vcounter_rdtsc;
		logger(RADLOG_NOTICE, "Initialising get_vcounter using rdtsc()");
	}

//profileit:
//	/* rdtsc and syscall profiling code */
//	if ( HAVE_RDTSC ) {
//
//	int k;
//	vcounter_t rd, rd1, rd2, rd3, rd4, tc;
//
//	rd1 = radclock_readtsc();	// warm up counter
////	for (k=1; k<=2; k++) {
////		rd1 = radclock_readtsc();
////		rd2 = radclock_readtsc();
////		rd3 = (vcounter_t) rdtsc32();
////		rd4 = (vcounter_t) rdtsc32();
////		printf("Direct rdtsc back-back latency test\n");
////		printf(" rd1 = %llu\n rd2 = %llu   Diff = %llu \n",
////		(long long unsigned)rd1, (long long unsigned)rd2, (long long unsigned)(rd2 - rd1));
////		printf(" rd1 = %llX\n rd2 = %llX   Diff = %llX \n",
////		(long long unsigned)rd1, (long long unsigned)rd2, (long long unsigned)(rd2 - rd1));
////		printf(" rd3 = %llX\n rd4 = %llX   Diff = %llX midDiff = %llX \n\n",
////		(long long unsigned)rd3, (long long unsigned)rd4,
////		(long long unsigned)(rd4 - rd3),
////		(long long unsigned)(rd3 - (unsigned)rd2) );
////	}
//
//	/* Profile rdtsc */
//	for (k=1; k<=1; k++) {
//		rd1 = radclock_readtsc();
//		rd  = radclock_readtsc();	// cut out radclock_get_vcounter_rdtsc wrapper
//		rd2 = radclock_readtsc();
//		printf("Bracket test for rdtsc call latency\n");
//		printf(" rd1 = %llu\n rd  = %llu\n rd2 = %llu  \t Diff = %llu \n",
//			(long long unsigned)rd1, (long long unsigned)rd, (long long unsigned)rd2, (long long unsigned)(rd2 - rd1));
//		printf(" rd1 = %llX\n rd  = %llX\n rd2 = %llX  \t Diff = %llX \n\n",
//			(long l
//
//			ong unsigned)rd1, (long long unsigned)rd, (long long unsigned)rd2, (long long unsigned)(rd2 - rd1));	}
//
//	/* Profile syscall */
//	ffclock_getcounter(&tc);	// warm up tc counter
//	for (k=1; k<=5; k++) {
//		rd1 = radclock_readtsc();
//		ffclock_getcounter(&tc);	// cut out radclock_get_vcounter_syscall wrapper
//		rd2 = radclock_readtsc();
//		printf("Bracket test for ffcounter call latency\n");
//		printf(" rd1 = %llu\n tc  = %llu\n rd2 = %llu  \t Diff = %llu  \t rd2-tc = %llu \n",
//			(long long unsigned)rd1, (long long unsigned)tc, (long long unsigned)rd2,
//			(long long unsigned)(rd2 - rd1), (long long unsigned)(rd2 - tc) );
//		printf(" rd1 = %llX\n tc  = %llX\n rd2 = %llX  \t Diff = %llX  \t rd2-tc = %llX \n\n",
//			(long long unsigned)rd1, (long long unsigned)tc, (long long unsigned)rd2,
//			(long long unsigned)(rd2 - rd1), (long long unsigned)(rd2 - tc) );
//	}
//
//	} // HAVE_RDTSC


	return (0);
}

#endif

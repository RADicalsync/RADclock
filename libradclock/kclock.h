/*
 * Copyright (C) 2006-2012, Julien Ridoux and Darryl Veitch
 * Copyright (C) 2013-2020, Darryl Veitch <darryl.veitch@uts.edu.au>
 * All rights reserved.
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

#ifndef _KCLOCK_H
#define _KCLOCK_H

#include "../config.h"

/* Ensure bintime defined (is standard in FreeBSD, but not linux) */
#if defined (__FreeBSD__)
#include <sys/time.h>
#else
struct bintime {
	time_t sec;
	uint64_t frac;
};
#endif


#if defined (__FreeBSD__) && defined (HAVE_SYS_TIMEFFC_H)
#include <sys/timeffc.h>			// in userland, defines syscalls (only)
#endif

/* This is THE interface data structure with FF code.
 * It is also defined in timeffc.h, but only when included within the kernel.
 * Instead, the independent defn here must correspond to it.
 * This ensures radclock compilation even on systems with FFsupport which is
 * absent, or with timeffc.h but out of date, so they can at least run dead.
 * If timeffc.h is present and the definitions don't match, defining it via the include
 * would not save the situation:  this would require KV dependent code.
 * [Note timeffc.h exists even if the FFsupport not active in FreeBSD.]
 *
 * The native FFclock corresponds to the native RADclock Ca(t), namely the
 * clock WithOut leaps since boot added in.
 * Raw timestamp type correspondance
 *    daemon:   typedef uint64_t vcounter_t  [ libradclock/radclock.h ]
 *		FreeBSD:  typedef uint64_t ffcounter   [ timeffc.h ]
 *		Linux:	 typedef u64 		ffcounter   [ radclock.h ]
*/
struct ffclock_estimate {
	struct bintime	update_time;	/* FF clock time of last update, ie Ca(tlast) */
	vcounter_t	update_ffcount;	/* Counter value at last update */
	vcounter_t	leapsec_expected;	/* Estimated counter value of next leap second */
	uint64_t	period;					/* Estimate of current counter period  [2^-64 s] */
	uint32_t	errb_abs;				/* Bound on absolute clock error [ns] */
	uint32_t	errb_rate;				/* Bound on relative counter period error [ps/s] */
	uint32_t	status;					/* Clock status */
	uint16_t	secs_to_nextupdate;	/* Estimated wait til next update [s] */
	int8_t	leapsec_total;			/* Sum of leap secs seen since clock start */
	int8_t	leapsec_next;			/* Next leap second (in {-1,0,1}) */
};

int get_kernel_ffclock(struct radclock *clock, struct ffclock_estimate *cest);
int set_kernel_ffclock(struct radclock *clock, struct ffclock_estimate *cest);

void fill_ffclock_estimate(struct radclock_data *rad_data,
		struct radclock_error *rad_err, struct ffclock_estimate *cest);
void fill_radclock_data(struct ffclock_estimate *cest, struct radclock_data *rad_data);

/*
 * XXX Deprecated
 * Old kernel data structure
 * TODO: remove when backward compatibility for kernel versions < 2 is dropped.
 */
struct radclock_fixedpoint
{
	/** phat as an int shifted phat_shift to the left */
	uint64_t phat_int;

	/** the time reference to add a delta vcounter to as an int (<< TIME_SHIFT) */
	uint64_t time_int;

	/** the vcounter value corresponding to the time reference */
	vcounter_t vcounter_ref;

	/** the shift amount for phat_int */
	uint8_t phat_shift;

	/** the shift amount for ca_int */
	uint8_t time_shift;

	/** maximum bit for vcounter difference without overflow */
	uint8_t countdiff_maxbits;
};

/*
 * XXX Deprecated
 * Set fixedpoint data in the kernel for computing timestamps there 
 */
int set_kernel_fixedpoint(struct radclock *clock, struct radclock_fixedpoint *fp);


#define	TWO32 ((long double)4294967296.0)	// 2^32

static inline void
bintime_to_ld(long double *time, struct bintime *bt)
{
	*time = bt->sec;
	*time += (long double)(bt->frac) / TWO32 / TWO32;
}

static inline void
ld_to_bintime(struct bintime *bt, long double *time)
{
	bt->sec = (time_t)*time;
	bt->frac =  (*time - bt->sec) * TWO32 * TWO32;
}

#endif 	/* _KCLOCK_H */

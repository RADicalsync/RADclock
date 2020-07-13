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

#ifndef _RADCLOCK_H
#define _RADCLOCK_H

#include <sys/time.h>
#include <stdint.h>
#include <pcap.h>

/*
 * This header defines the RADclock API to create a RADclock, access time and
 * get internal clock parameters.
 */

/** RADclock status word **/
/* RADclock not sync'ed (just started, server not reachable) */
#define STARAD_UNSYNC			0x0001
/* RADclock in warmup phase, error estimates unreliable */
#define STARAD_WARMUP			0x0002
/* RADclock kernel time is reliable */
#define STARAD_KCLOCK			0x0004
/* The system clock is fairly accurate (if adjusted by the RADclock) */
#define STARAD_SYSCLOCK			0x0008
/* RADclock is lacking valid input stamps */
#define STARAD_STARVING			0x0010
/* Upward Shift Detected */

#define STARAD_RTT_UPSHIFT		0x0020
/* Quality of the RADclock period estimate is poor */

#define STARAD_PHAT_UPDATED	0x0040
/* phat updated on this stamp (not adopted if sanity triggered) */
#define STARAD_PHAT_SANITY		0x0080
/* phat update seems impossible, suspect server error or path asym change */

#define STARAD_PLOCAL_QUALITY	0x0100
/* Quality of new plocal estimate below level acceptable for adoption */
#define STARAD_PLOCAL_SANITY	0x0200
/* plocal update is suspect, likely server error or path asym change, adoption blocked */

#define STARAD_OFFSET_QUALITY	0x0400
/* Quality of new offset estimate below level acceptable for adoption */
#define STARAD_OFFSET_SANITY	0x0800
/* offset update is suspect, likely server error or path asym change, adoption blocked */

typedef uint64_t vcounter_t;

typedef enum { RADCLOCK_LOCAL_PERIOD_ON, RADCLOCK_LOCAL_PERIOD_OFF }
		radclock_local_period_t;

struct radclock;


/**
 * Read the tsc value from the cpu register. 
 * @return the current value of the TSC register
 */
vcounter_t radclock_readtsc(void);

/**
 * Read the vcounter value based on the current clocksource/timecounter selected. 
 * @return the current value of the vcounter
 */
int radclock_get_vcounter(struct radclock *clock, vcounter_t *vcount);


/**
 * Create a new radclock.
 * Each application needs to create its own copy of the radclock
 * @return a radclock clock or NULL on a failure
 */
struct radclock *radclock_create(void);


/**
 * Destroy the radclock clock.
 * @param Access to the private RADclock
 */
void radclock_destroy(struct radclock *clock);


/**
 * Initialise a RADclock.
 * @param Access to the private RADclock
 * @return 0 on success, -1 on failure
 */
int radclock_init(struct radclock *clock);


/**
 * Set the mode when composing time based on raw vcount values and RADclock parameters.
 * If set to RADCLOCK_LOCAL_PERIOD_ON a local estimate of the CPU
 * period is used instead of the long term estimate. The default behavior is to
 * have the local period estimate used, assuming the RADclock daemon is running
 * with plocal ON. If not this falls back to using the long term period
 * estimate.
 * @param clock The radclock clock
 * @param local_period_mode A reference to the mode used for creating timestamps 
 */
int radclock_set_local_period_mode(struct radclock *clock,
		radclock_local_period_t *local_period_mode);


/**
 * Retrieve the mode of composing time when reading the RADclock.
 * @param clock The radclock clock
 * @param local_period_mode A reference to the mode used for creating timestamps 
 */
int radclock_get_local_period_mode(struct radclock *clock,
		radclock_local_period_t *local_period_mode);


/**
 * Get the time from the radclock in a timeval format (micro second resolution).
 * @param  clock Access to the private RADclock
 * @param  abstime A reference to the long double time to be filled
 * @return 0 on success
 * @return 1 on error
 * @return 2 on possibly poor quality timestamp (clock could not be reached, but
 * last data is recent)
 * @return 3 on very poor quality timestamp (clock could not be reached, and
 * last dat is really old)
 */
int radclock_gettime(struct radclock *clock, long double *abstime);


/**
 * Convert a vcounter value to a timeval struct representation of absolute time,
 * based on the current radclock parameters.
 * @param  clock Access to the private RADclock
 * @param  vcount A reference to the vcounter_t vcounter value to convert 
 * @param  abstime A reference to the long double time to be filled
 * @return 0 on success
 * @return 1 on error
 * @return 2 on possibly poor quality timestamp (clock could not be reached, but
 * last data is recent)
 * @return 3 on very poor quality timestamp (clock could not be reached, and
 * last dat is really old)
 */
int radclock_vcount_to_abstime(struct radclock *clock, const vcounter_t *vcount,
		long double *abstime);


/** 
 * Get the time elapsed since a vcount event in a timeval format based on the
 * current radclock parameters.
 * @param  clock Access to the private RADclock
 * @param  past_vcount A reference to the vcount value corresponding to the past event
 * @param  duration A reference to the long double time interval value to be filled
 * @return 0 on success
 * @return 1 on error
 * @return 2 on possibly poor quality timestamp (clock could not be reached, but
 * last data is recent)
 * @return 3 on very poor quality timestamp (clock could not be reached, and
 * last dat is really old)
 */
int radclock_elapsed(struct radclock *clock, const vcounter_t *past_vcount,
		long double *duration);


/** 
 * Get a duration between two vcount events in a timeval format based on the current
 * radclock parameters.
 * @param  clock Access to the private RADclock
 * @param  start_vcount A reference to the vcount value corresponding to the start event
 * @param  end_vcount A reference to the vcount value corresponding to the ending event
 * @param  duration A reference to the long double time interval to be filled
 * @return 0 on success
 * @return 1 on error
 * @return 2 on possibly poor quality timestamp (clock could not be reached, but
 * last data is recent)
 * @return 3 on very poor quality timestamp (clock could not be reached, and
 * last dat is really old)
 */
int radclock_duration(struct radclock *clock, const vcounter_t *start_vcount,
		const vcounter_t *end_vcount, long double *duration);


/** 
 * Get instantaneous estimate of the clock error bound in seconds
 * @param  clock Access to the private RADclock
 * @param  error_bound A reference to the double time error bound value to be filled
 * @return 0 on success
 * @return 1 on error
 * @return 2 on possibly outdated data (clock could not be reached, but
 * last data is recent)
 * @return 3 on very old data (clock could not be reached)
 */
int radclock_get_clockerror_bound(struct radclock *clock, double *error_bound);


/** 
 * Get averaged estimate of the clock error bound in seconds
 * @param  clock Access to the private RADclock
 * @param  duration A reference to the double average error value to be filled
 * @return 0 on success
 * @return 1 on error
 * @return 2 on possibly outdated data (clock could not be reached, but
 * last data is recent)
 * @return 3 on very old data (clock could not be reached)
 */
int radclock_get_clockerror_bound_avg(struct radclock *clock, double *error_bound_avg);


/** 
 * Get standard deviation estimate of the clock error bound in seconds
 * @param  clock Access to the private RADclock
 * @param  error_bound_std A reference to the double error std value to be filled
 * @return 0 on success
 * @return 1 on error
 * @return 2 on possibly outdated data (clock could not be reached, but
 * last data is recent)
 * @return 3 on very old data (clock could not be reached)
 */
int radclock_get_clockerror_bound_std(struct radclock *clock, double *error_bound_std);


/** 
 * Get estimate of the minimum RTT to reference clock in seconds
 * @param  clock Access to the private RADclock
 * @param  duration A reference to the double minRTT value to be filled
 * @return 0 on success
 * @return 1 on error
 * @return 2 on possibly outdated data (clock could not be reached, but
 * last data is recent)
 * @return 3 on very old data (clock could not be reached)
 */
int radclock_get_min_RTT(struct radclock *clock, double *min_RTT);


/**
 * Get the vcount value corresponding to the last time the clock
 * parameters have been updated.
 * @param  clock Access to the private RADclock
 * @param  last_stamp A reference to the vcounter value to be filled
 * @return 0 on success
 * @return 1 on error
 * @return 2 on possibly outdated data (clock could not be reached, but
 * last data is recent)
 * @return 3 on very old data (clock could not be reached)
 */
int radclock_get_last_changed(struct radclock *clock, vcounter_t *last_stamp);


/**
 * Get the vcount value corresponding to the next time the clock
 * should be updated.
 * @param  clock Access to the private RADclock
 * @param  next_expected A reference to the vcounter value to be filled
 * @return 0 on success
 * @return 1 on error
 * @return 2 on possibly outdated data (clock could not be reached, but
 * last data is recent)
 * @return 3 on very old data (clock could not be reached)
 */
int radclock_get_next_expected(struct radclock *clock, vcounter_t *next_expected);


/**
 * Get the period of the CPU oscillator.
 * @param  clock Access to the private RADclock
 * @param  period A reference to the double period to be filled
 * @return 0 on success
 * @return 1 on error
 * @return 2 on possibly outdated data (clock could not be reached, but
 * last data is recent)
 * @return 3 on very old data (clock could not be reached)
 */
int radclock_get_period(struct radclock *clock, double *period);


/**
 * Get the radclock offset (the ca parameter, essentially a UTC bootime)
 * @param  clock Access to the private RADclock
 * @param  offset A reference to the long double offset to be filled
 * @return 0 on success
 * @return 1 on error
 * @return 2 on possibly outdated data (clock could not be reached, but
 * last data is recent)
 * @return 3 on very old data (clock could not be reached)
 */
int radclock_get_bootoffset(struct radclock *clock, long double *offset);


/**
 * Get the error estimate of period of the CPU oscillator.
 * @param  clock Access to the private RADclock
 * @param  err_period A reference to the double err_period to be filled
 * @return 0 on success
 * @return 1 on error
 * @return 2 on possibly outdated data (clock could not be reached, but
 * last data is recent)
 * @return 3 on very old data (clock could not be reached)
 */
int radclock_get_period_error(struct radclock *clock, double *err_period);


/**
 * Get the error estimate of the radclock offset (the ca parameter).
 * @param  clock Access to the private RADclock
 * @param  err_offset A reference to the double err_offset to be filled
 * @return 0 on success
 * @return 1 on error
 * @return 2 on possibly outdated data (clock could not be reached, but
 * last data is recent)
 * @return 3 on very old data (clock could not be reached)
 */
int radclock_get_bootoffset_error(struct radclock *clock, double *err_offset);


/**
 * Get the status of the radclock.
 * @param  clock Access to the private RADclock
 * @param  status A reference to the unsigned int status to be filled
 * @return 0 on success
 * @return 1 on error
 * @return 2 on possibly outdated data (clock could not be reached, but
 * last data is recent)
 * @return 3 on very old data (clock could not be reached)
 */
int radclock_get_status(struct radclock *clock, unsigned int *status);


/* View these as presets for bpf's _T_ based timestamp type specification.
 * Used for convenience by daemon and libprocesses to set all FORMAT, FFCOUNTER, FLAVOR, CLOCK
 * dimensions of bpf tstype.  All set FLAVOR = NORMAL (UTC and !FAST) and FFCOUNTER = FFC.
 * Descriptions below specify high level intent, whether fully possible given KV or not.
 * Other bpf tstype dimensions are specified in KV dependent code.
 * Main use cases:
 *	  PKTCAP_TSMODE_FBCLOCK:  traditional daemon, to get needed raw + readily
 *      available FB comparison ts, stored in raw output enable fair comparisons
 *	  PKTCAP_TSMODE_FFCLOCK:  same as _FBclock but this time to compare against
 *		  the kernel's monoFF clock
 *   PKTCAP_TSMODE_FFNATIVECLOCK:  libprocesses, who just want to be able to
 *      get RADclock timestamps for each pkt, but made already in the kernel
 *   PKTCAP_TSMODE_DIFFCLOCK:  libprocesses measuring small time intervales such
 *      as RTTs, who want to use the RADclock difference clock, but need to do so
 *      via an absolute clock read.  This clock is allowed to drift to achieve this.
 *   PKTCAP_TSMODE_RADCLOCK:  if dont trust the kernel timestamp, prefer to
 *		  read RADclock (native abs clock) in userland, based off kernel raw ts.
 *      Also useful in KV=2 when have no choice but to create a ts in the daemon.
 **/
enum pktcap_tsmode {
	PKTCAP_TSMODE_NOMODE = 0,			// no FF support in pcap, or very early versions
	PKTCAP_TSMODE_SYSCLOCK = 1,		// get raw, plus normal timestamp from sysclock
	PKTCAP_TSMODE_FBCLOCK = 2,			//                "                    FBclock
	PKTCAP_TSMODE_FFCLOCK = 3,			//                "                    FFclock (mono)
	PKTCAP_TSMODE_FFNATIVECLOCK = 4,	//                "                    FFclock (native)
	PKTCAP_TSMODE_FFDIFFCLOCK = 5,	//                "                    FF difference clock
	PKTCAP_TSMODE_RADCLOCK = 6,		//    "   , plus RADclock timestamp (userland)
};


typedef enum pktcap_tsmode pktcap_tsmode_t ;


/**
 * Register pcap clock into radclock structure
 */
int radclock_register_pcap(struct radclock *clock, pcap_t *pcap_handle);


/**
 * Set the mode of timestamping on the pcap handle
 * This will only work on a live socket.
 * @return 0 on success, non-zero on failure
 */
int pktcap_set_tsmode(struct radclock *clock, pcap_t *p_handle,
		pktcap_tsmode_t mode);


/**
 * Get the mode of timestamping on the pcap handle into the refence
 * This will only work on a live socket.
 * @return 0 on success, non-zero on failure
 */
int pktcap_get_tsmode(struct radclock *clock, pcap_t *p_handle,
		pktcap_tsmode_t *mode);


/**
 * Get a packet all associated information.
 * This is a shorcut function to read a single packet and get all the
 * associated timestamps.
 * @return error code 
 */
int radclock_get_packet(struct radclock *clock, pcap_t *p_handle,
		struct pcap_pkthdr *header, unsigned char **packet, vcounter_t *vcount,
		struct timeval *ts);


#endif

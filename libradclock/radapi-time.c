/*
 * Copyright (C) 2006 The RADclock Project (see AUTHORS file).
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

#include <sys/types.h>
#include <sys/time.h>

#include "radclock.h"
#include "radclock-private.h"
#include "kclock.h"
#include "logger.h"


// TODO: check all return values and error codes

// TODO: in all this file, check which functions need to call get_global_data
// and conditions for that (last update too far in the past ?)
// Should check for clock not synchronised? This is specific to the sync algo
// and should be designed that way. Following is a reminder of outdated code
/*
if ( ts < data->last_changed || ts - data->last_changed * data->phat > 10000 )
{
	logger(RADLOG_WARNING, 
		"radclock seems unsynchronised, last updated %7.1lf [sec] ago",
		(ts - data->last_changed) * data->phat);
}
*/

/* Defines bound on SKM scale. A bit redundant with other defines but easy to
 * fix if needed.
 */
#define OUT_SKM	1024



/*
 * Inspect data to get an idea about the quality.
 * TODO: error codes should be fixed
 * TODO: other stuff to take into account in composing quality estimate? Needed
 * or clock status and clock error take care of it?
 * TODO: massive problem with thread synchronisation ...
 */
int
raddata_quality(vcounter_t now, vcounter_t last, vcounter_t valid, double phat)
{
	/*
	 * Something really bad is happening:
	 * - counter is going backward (should never happen)
	 * - virtual machine read H/W counter then migrated, things are out of whack
	 * - ...?
	 */
// XXX FIXME XXX THIS IS WRONG
// can read counter, then data updated, then compare ... BOOM!
	if (now < last)
		return 3;
   //fprintf(stdout,"raddata_quality:  made it past first rtn3\n");

	/*
	 * Several scenarios again:
	 * - the data is really old, clock status should say the same
	 * - virtual machine migrated, but cannot be sure. Mark data as very bad.
	 */
	//if (phat * (now - valid) > OUT_SKM)
		//return 3;

	/* The data is old, but still in SKM_SCALE */
	if (now > valid) {
		//fprintf(stdout,"raddata_quality:  inside now>valid \n");
		if (phat * (now - valid) > OUT_SKM) {
		   fprintf(stdout,"raddata_quality:  rtn3\n");
			return 3;
		}
		//fprintf(stdout,"raddata_quality:  rtn2\n");
		return 2;
	}
	
	//fprintf(stdout,"raddata_quality:  rtn0 \n");
	return 0;
}




/*
 * Build the time using the UTC absolute clock plus the local relative rate
 * correction.
 */
static inline int
ffcounter_to_abstime_sms(struct radclock *clock, vcounter_t vcount,
		long double *time)
{
	struct radclock_sms *sms;
	vcounter_t valid, last;
	double phat;
	int generation;

	sms = (struct radclock_sms *) clock->ipc_sms;
	do {
		/* Quality ingredients */
		generation = sms->gen;
		valid = SMS_DATA(sms)->next_expected;
		last  = SMS_DATA(sms)->last_changed;
		phat  = SMS_DATA(sms)->phat;

		if ( clock->local_period_mode == RADCLOCK_LOCAL_PERIOD_ON )
			read_RADabs_UTC(SMS_DATA(sms), &vcount, time, 1);
		else
			read_RADabs_UTC(SMS_DATA(sms), &vcount, time, 0);

//		if ((clock->local_period_mode == RADCLOCK_LOCAL_PERIOD_ON)
//			&& ((SMS_DATA(sms)->status & STARAD_WARMUP) != STARAD_WARMUP))
//		{
//			*time += (vcount - last) *
//				(long double)(SMS_DATA(sms)->phat_local - phat);
//		}
	} while (generation != sms->gen || !sms->gen);

	return raddata_quality(vcount, last, valid, phat);
}


static inline int
ffcounter_to_abstime_kernel(struct radclock *clock, vcounter_t vcount,
		long double *time)
{
	struct ffclock_estimate cest;
	struct radclock_data rad_data;

	if (get_kernel_ffclock(clock, &cest))
		return (1);
	fill_radclock_data(&cest, &rad_data);
	read_RADabs_UTC(&rad_data, &vcount, time, 0);	// no point trying plocal

	return raddata_quality(vcount, rad_data.last_changed, rad_data.next_expected,
			rad_data.phat);
}


/*
 * Build a delay using the difference clock.
 * This function does not fail, SKM model should be checked before call
 */
static inline int
ffcounter_to_difftime_sms(struct radclock *clock, vcounter_t from_vcount,
		vcounter_t till_vcount, long double *time)
{
	struct radclock_sms *sms;
	vcounter_t now, valid, last;
	double phat;
	int generation;

	// TODO Stupid performance penalty, but needs more thought
	if (radclock_get_vcounter(clock, &now))
		return (1);

	sms = (struct radclock_sms *) clock->ipc_sms;
	do {
		generation = sms->gen;
		valid = SMS_DATA(sms)->next_expected;
		last  = SMS_DATA(sms)->last_changed;
		phat  = SMS_DATA(sms)->phat;
		if ( clock->local_period_mode == RADCLOCK_LOCAL_PERIOD_ON )
			*time = (till_vcount - from_vcount) * (long double)SMS_DATA(sms)->phat_local;
		else
			*time = (till_vcount - from_vcount) * (long double)SMS_DATA(sms)->phat;
	} while (generation != sms->gen || !sms->gen);

	return raddata_quality(now, last, valid, phat);
}


static inline int
ffcounter_to_difftime_kernel(struct radclock *clock, vcounter_t from_vcount,
		vcounter_t till_vcount, long double *time)
{
	struct ffclock_estimate cest;
	struct radclock_data rad_data;
	vcounter_t now;

	// TODO Stupid performance penalty, but needs more thought
	if (radclock_get_vcounter(clock, &now))
		return (1);

	if (get_kernel_ffclock(clock, &cest))
		return (1);
	fill_radclock_data(&cest, &rad_data);
	*time = ((double)(till_vcount) - (double)from_vcount) * (long double)rad_data.phat_local;
	
	return 	raddata_quality(now, rad_data.last_changed, rad_data.next_expected, rad_data.phat);
}


/*
 * Check if we are in the SKM model bounds or not
 * Need to know if we are in SKM world. If not, can't use the difference clock,
 * need to substract two absolute timestamps. Testing should always be done
 * using the 'current' vcount value since we use the current global data !!!
 * Use phat for this comparison but using plocal should be fine as well
 */
// XXX  quite a few issues here
// 		- the value of the SKM scale is hard coded ... but otherwise?
// 		- Validity of the global data
// 		- error code(s) to return
static inline int
in_SKMwin(struct radclock *clock, const vcounter_t *past_count, const vcounter_t *vc)
{
	struct radclock_sms *sms;
	vcounter_t now;

	if (!vc)		// if counter not passed, read it here
		radclock_get_vcounter(clock, &now);
	else
		now = *vc;

	sms = (struct radclock_sms *) clock->ipc_sms;
	if ((now - *past_count) * SMS_DATA(sms)->phat < 1024)
		return (1);	// is within SKMwin
	else
		return (0);
}


int
radclock_gettime(struct radclock *clock, long double *abstime)
{
	vcounter_t vcount;
	int quality;

	/* Check for critical bad input */
	if (!clock || !abstime)
		return (1);

	/* Make sure we can get a raw timestamp */
	if (radclock_get_vcounter(clock, &vcount) < 0)
		return (1);
	
	/* Retrieve clock data */
	if (clock->ipc_sms)
		quality = ffcounter_to_abstime_sms(clock, vcount, abstime);
	else
		quality = ffcounter_to_abstime_kernel(clock, vcount, abstime);
	return (quality);
}


int
radclock_vcount_to_abstime(struct radclock *clock, const vcounter_t *vcount,
		long double *abstime)
{
	int quality;

	/* Check for critical bad input */
	if (!clock || !vcount || !abstime)
		return (1);

	if (clock->ipc_sms)
		quality = ffcounter_to_abstime_sms(clock, *vcount, abstime);
	else {
		logger(RADLOG_WARNING, "Using kernel data instead of sms, expected?");
		quality = ffcounter_to_abstime_kernel(clock, *vcount, abstime);
	}

	return (quality);
}


int
radclock_elapsed(struct radclock *clock, const vcounter_t *from_vcount,
		long double *duration)
{
	vcounter_t vcount;
	int quality = 0;

	/* Check for critical bad input */
	if (!clock || !from_vcount || !duration)
		return (1);

	/* Make sure we can get a raw timestamp */
	if (radclock_get_vcounter(clock, &vcount) < 0)
		return (1);
	
	/* Retrieve clock data */
	if (clock->ipc_sms) {
		quality = ffcounter_to_difftime_sms(clock, *from_vcount, vcount, duration);
		if (!in_SKMwin(clock, from_vcount, &vcount))
			return (1);
	} else
		quality = ffcounter_to_difftime_kernel(clock, *from_vcount, vcount, duration);

// TODO is this the  good behaviour, we should request the clock data associated
// to from_vcount? maybe not
//	if (!in_SKMwin(clock, from_vcount, &vcount))
//		return (1);

	return (quality);
}


int
radclock_duration(struct radclock *clock, const vcounter_t *from_vcount,
		const vcounter_t *till_vcount, long double *duration)
{
	vcounter_t vcount;
	int quality = 0;

	/* Check for critical bad input */
	if (!clock || !from_vcount || !till_vcount || !duration)
		return (1);

	/* Make sure we can get a raw timestamp */
	if (radclock_get_vcounter(clock, &vcount) < 0)
		return (1);
	
	/* Retrieve clock data */
	if (clock->ipc_sms) {
		quality = ffcounter_to_difftime_sms(clock, *from_vcount, *till_vcount, duration);
		if (!in_SKMwin(clock, from_vcount, &vcount)) {
			fprintf(stdout, "radclock_duration: inside \n");
			return (1);
		}
	} else
		quality = ffcounter_to_difftime_kernel(clock, *from_vcount, *till_vcount, duration);

// TODO is this the  good behaviour, we should request the clock data associated
// to from_vcount? maybe not
//	if (!in_SKMwin(clock, from_vcount, &vcount))
//		return (1);

	return (quality);
}


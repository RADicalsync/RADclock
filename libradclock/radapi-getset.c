/*
 * Copyright (C) 2006-2012, Julien Ridoux, Darryl Veitch
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

#include "../config.h"

#include <sys/types.h>
#include <sys/time.h>
#include <math.h>

#include "radclock.h"
#include "radclock-private.h"
#include "kclock.h"
#include "logger.h"


/*
 * Specifies if local refinement of hardware counter period should be used when
 * computing time.
 */
int
radclock_set_local_period_mode(struct radclock *clock,
		radclock_local_period_t *local_period_mode)
{
	if (!clock || !local_period_mode)
		return (1);

	switch (*local_period_mode) {
		case RADCLOCK_LOCAL_PERIOD_ON:
		case RADCLOCK_LOCAL_PERIOD_OFF:
			break;
		default:
			logger(RADLOG_ERR, "Unknown local period mode");
			return (1);
	}

	clock->local_period_mode = *local_period_mode;
	return (0);
}


// TODO: should kill this? plocal is always used.
int
radclock_get_local_period_mode(struct radclock *clock,
		radclock_local_period_t *local_period_mode)
{
	if (!clock || !local_period_mode)
		return (1);

	*local_period_mode = clock->local_period_mode;
	return (0);
}




int
radclock_get_last_changed(struct radclock *clock, vcounter_t *last_vcount)
{
	struct ffclock_estimate cest; 
	struct radclock_data rad_data;
	struct radclock_shm *shm;
	int generation;

	if (!clock || !last_vcount)
		return (1);

	if (clock->ipc_shm) {
		shm = (struct radclock_shm *) clock->ipc_shm;
		do {
			generation = shm->gen;
			*last_vcount = SHM_DATA(shm)->last_changed;
		} while (generation != shm->gen || !shm->gen);
	} else {

		if (get_kernel_ffclock(clock, &cest))
			return (1);
		fill_radclock_data(&cest, &rad_data);
		*last_vcount = rad_data.last_changed;
	}

	return (0);
}


int
radclock_get_next_expected(struct radclock *clock, vcounter_t *till_vcount)
{
	struct ffclock_estimate cest; 
	struct radclock_data rad_data;
	struct radclock_shm *shm;
	int generation;

	if (!clock || !till_vcount)
		return (1);

	if (clock->ipc_shm) {
		shm = (struct radclock_shm *) clock->ipc_shm;
		do {
			generation = shm->gen;
			*till_vcount = SHM_DATA(shm)->next_expected;
		} while (generation != shm->gen || !shm->gen);
	} else {
		if (get_kernel_ffclock(clock, &cest))
			return (1);
		fill_radclock_data(&cest, &rad_data);
		*till_vcount = rad_data.next_expected;
	}

	return (0);
}


int
radclock_get_period(struct radclock *clock, double *period)
{
	struct ffclock_estimate cest; 
	struct radclock_data rad_data;
	struct radclock_shm *shm;
	int generation;

	if (!clock || !period)
		return (1);

	if (clock->ipc_shm) {
		shm = (struct radclock_shm *) clock->ipc_shm;
		do {
			generation = shm->gen;
			*period = SHM_DATA(shm)->phat;
		} while (generation != shm->gen || !shm->gen);
	} else {
		if (get_kernel_ffclock(clock, &cest))
			return (1);
		fill_radclock_data(&cest, &rad_data);
		*period = rad_data.phat;
	}

	return (0);
}


int
radclock_get_bootoffset(struct radclock *clock, long double *offset)
{
	struct ffclock_estimate cest; 
	struct radclock_data rad_data;
	struct radclock_shm *shm;
	int generation;

	if (!clock || !offset)
		return (1);

	if (clock->ipc_shm) {
		shm = (struct radclock_shm *) clock->ipc_shm;
		do {
			generation = shm->gen;
			*offset = SHM_DATA(shm)->ca;
		} while (generation != shm->gen || !shm->gen);
	} else {
		if (get_kernel_ffclock(clock, &cest))
			return (1);
		fill_radclock_data(&cest, &rad_data);
		*offset = rad_data.ca;
	}

	return (0);
}


int
radclock_get_period_error(struct radclock *clock, double *err_period)
{
	struct ffclock_estimate cest; 
	struct radclock_data rad_data;
	struct radclock_shm *shm;
	int generation;

	if (!clock || !err_period)
		return (1);

	if (clock->ipc_shm) {
		shm = (struct radclock_shm *) clock->ipc_shm;
		do {
			generation = shm->gen;
			*err_period = SHM_DATA(shm)->phat_err;
		} while (generation != shm->gen || !shm->gen);
	} else {
		if (get_kernel_ffclock(clock, &cest))
			return (1);
		fill_radclock_data(&cest, &rad_data);
		*err_period = rad_data.phat_err;
	}

	return (0);
}


int
radclock_get_bootoffset_error(struct radclock *clock, double *err_offset)
{
	struct ffclock_estimate cest; 
	struct radclock_data rad_data;
	struct radclock_shm *shm;
	int generation;

	if (!clock || !err_offset)
		return (1);

	if (clock->ipc_shm) {
		shm = (struct radclock_shm *) clock->ipc_shm;
		do {
			generation = shm->gen;
			*err_offset = SHM_DATA(shm)->ca_err;
		} while (generation != shm->gen || !shm->gen);
	} else {
		if (get_kernel_ffclock(clock, &cest))
			return (1);
		fill_radclock_data(&cest, &rad_data);
		*err_offset = rad_data.ca_err;
	}

	return (0);
}


int
radclock_get_status(struct radclock *clock, unsigned int *status)
{
	struct ffclock_estimate cest; 
	struct radclock_data rad_data;
	struct radclock_shm *shm;
	int generation;

	if (!clock || !status)
		return (1);

	if (clock->ipc_shm) {
		shm = (struct radclock_shm *) clock->ipc_shm;
		do {
			generation = shm->gen;
			*status = SHM_DATA(shm)->status;
		} while (generation != shm->gen || !shm->gen);
	} else {
		if (get_kernel_ffclock(clock, &cest))
			return (1);
		fill_radclock_data(&cest, &rad_data);
		*status = rad_data.status;
	}

	return (0);
}


// TODO: for all 3 functions, implement kernel based fall back case if ipc_shm is NULL
// this may imply adapting get_kernel_ffclock to include return of error metrics
int
radclock_get_clockerror_bound(struct radclock *clock, double *error_bound)
{
	struct radclock_shm *shm;
	int generation;

	if (!clock || !error_bound)
		return (1);

	if (!clock->ipc_shm)
		return (1);

	shm = (struct radclock_shm *) clock->ipc_shm;
	do {
		generation = shm->gen;
		*error_bound = SHM_ERROR(shm)->error_bound;
	} while (generation != shm->gen || !shm->gen);

	return (0);
}


int
radclock_get_clockerror_bound_avg(struct radclock *clock, double *error_bound_avg)
{
	struct radclock_shm *shm;
	int generation;

	if (!clock || !error_bound_avg)
		return (1);

	if (!clock->ipc_shm)
		return (1);

	shm = (struct radclock_shm *) clock->ipc_shm;
	do {
		generation = shm->gen;
		*error_bound_avg = SHM_ERROR(shm)->error_bound_avg;
	} while (generation != shm->gen || !shm->gen);

	return (0);
}


int
radclock_get_clockerror_bound_std(struct radclock *clock, double *error_bound_std)
{
	struct radclock_shm *shm;
	int generation;

	if (!clock || !error_bound_std)
		return (1);

	if (!clock->ipc_shm)
		return (1);

	shm = (struct radclock_shm *) clock->ipc_shm;
	do {
		generation = shm->gen;
		*error_bound_std = SHM_ERROR(shm)->error_bound_std;
	} while (generation != shm->gen || !shm->gen);

	return (0);
}

int
radclock_get_min_RTT(struct radclock *clock, double *min_RTT)
{
	struct radclock_shm *shm;
	int generation;

	if (!clock || !min_RTT)
		return (1);

	if (!clock->ipc_shm)
		return (1);

	shm = (struct radclock_shm *) clock->ipc_shm;
	do {
		generation = shm->gen;
		*min_RTT = SHM_ERROR(shm)->min_RTT;
	} while (generation != shm->gen || !shm->gen);

	return (0);
}


/* Functions to print out rad_data and FF_data neatly */
void
printout_FFdata(struct ffclock_estimate *cest)
{
	logger(RADLOG_NOTICE, "Pretty printing FF_data.");
	logger(RADLOG_NOTICE, "\t period %llu", cest->period);
	logger(RADLOG_NOTICE, "\t update_time: %llu.%llu [bintime]\t\t status: 0x%04X",
		cest->update_time.sec, cest->update_time.frac, cest->status);
	logger(RADLOG_NOTICE, "\t update_ffcount: %llu  next_expected: %llu  (u_diff: %llu)",
		cest->update_ffcount, cest->next_expected,
		cest->next_expected - cest->update_ffcount);
	logger(RADLOG_NOTICE, "\t errb_{abs,rate} = %lu  %lu",
		cest->errb_abs, cest->errb_rate);
	logger(RADLOG_NOTICE, "\t leapsec_{expected, total,next}:  %llu  %u  %u",
		cest->leapsec_expected, cest->leapsec_total, cest->leapsec_next);
	logger(RADLOG_NOTICE,"-------------------------------------------------------------");
}


void
printout_raddata(struct radclock_data *rad_data)
{
	long double UTCtime_l, UTCtime_n;

	logger(RADLOG_NOTICE, "Pretty printing rad_data");
	logger(RADLOG_NOTICE, "\t phat: %10.10le\t phat_local: %l0.10e\t (diff: %8.2le)",
		rad_data->phat, rad_data->phat_local, rad_data->phat - rad_data->phat_local);
	logger(RADLOG_NOTICE, "\t ca: %Lf\t ca_err: %10.3le,\t\t status: 0x%04X",
		rad_data->ca, rad_data->ca_err, rad_data->status);
	logger(RADLOG_NOTICE, "\t last_changed: %llu  next_expected: %llu  (u_diff: %llu)",
		rad_data->last_changed, rad_data->next_expected,
		rad_data->next_expected - rad_data->last_changed);
	logger(RADLOG_NOTICE, "\t phat_err: %7.5le\t phat_local_err: %7.5lf",
		rad_data->phat_err, rad_data->phat_local_err);
	logger(RADLOG_NOTICE, "\t leapsec_{expected,total,next}:  %llu  %u  %u",
		rad_data->leapsec_expected, rad_data->leapsec_total, rad_data->leapsec_next);

	/* Translate raw timestamp fields to UTC for convenient checking */
	logger(RADLOG_NOTICE,"\t --------------------------------------");
	read_RADabs_UTC(rad_data, &rad_data->last_changed,  &UTCtime_l, 1);
	read_RADabs_UTC(rad_data, &rad_data->next_expected, &UTCtime_n, 1);
	logger(RADLOG_NOTICE,"\t UTC at (last_changed, next_expected) = %10.6Lf  %10.6Lf (diff: %Lf)",
		UTCtime_l, UTCtime_n, UTCtime_n - UTCtime_l);
	logger(RADLOG_NOTICE,"-------------------------------------------------------------");
}


/*
 * Copyright (C) 2006-2012, Julien Ridoux, Darryl Veitch
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

#include "../config.h"

#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "radclock.h"
#include "radclock-private.h"
#include "kclock.h"
#include "logger.h"

/*
 * Create radclock structure
 */
struct radclock *
radclock_create(void)
{
	struct radclock *clock;
	clock = (struct radclock*) malloc(sizeof(struct radclock));

	if (!clock)
		return NULL;

	/* Default values before calling init */
	clock->local_period_mode = RADCLOCK_LOCAL_PERIOD_ON;
	clock->kernel_version = -1;

	/* SHM stuff */
	clock->ipc_shm_id = 0;
	clock->ipc_shm = NULL;

// TODO present 3 function pointers instead?
	/* Feed-forward clock kernel interface */
	clock->syscall_set_ffclock = 0;
	clock->syscall_get_vcounter = 0;
	clock->get_vcounter = NULL;

	/* PCAP */
	clock->pcap_handle 	= NULL;

	return (clock);
}


/*
 * Initialise shared memory segment.
 * IPC mechanism to access radclock updated clock parameters and error
 * estimates.
 */
int
shm_init_reader(struct radclock *clock)
{
	key_t shm_key;

	logger(RADLOG_ERR, "Enter init_shm_reader");

	shm_key = ftok(IPC_SHARED_MEMORY, 'a');
	if (shm_key == -1) {
		logger(RADLOG_ERR, "ftok: %s", strerror(errno));
		return (1);
	}

	clock->ipc_shm_id = shmget(shm_key, sizeof(struct radclock_shm), 0);
	if (clock->ipc_shm_id < 0) {
		logger(RADLOG_ERR, "shmget: %s", strerror(errno));
		return (1);
	}

	clock->ipc_shm = shmat(clock->ipc_shm_id, NULL, SHM_RDONLY);
	if (clock->ipc_shm == (void *) -1) {
		logger(RADLOG_ERR, "shmat: %s", strerror(errno));
		clock->ipc_shm = NULL;
		return (1);
	}

	return (0);
}


/*
 * Create and or initialise IPC shared memory to pass radclock data to system
 * processes.
 */
int
shm_init_writer(struct radclock *clock)
{
	struct shmid_ds shm_ctl;
	struct radclock_shm *shm;
	struct stat sb;
	key_t shm_key;
	unsigned int perm_flags;
	int shm_fd, is_new_shm;

	if (stat(RADCLOCK_RUN_DIRECTORY, &sb) < 0) {
		if (mkdir(RADCLOCK_RUN_DIRECTORY, 0755) < 0) {
			logger(RADLOG_ERR, "Cannot create %s directory", RADCLOCK_RUN_DIRECTORY);
			return (1);
		}
	}

	/*
	 * Create shm key (file created if it does not already exist)
	 */
	shm_fd = open(IPC_SHARED_MEMORY, O_RDWR|O_CREAT, 0644);
	close(shm_fd);

	shm_key = ftok(IPC_SHARED_MEMORY, 'a');
	if (shm_key == -1) {
		logger(RADLOG_ERR, "ftok: %s", strerror(errno));
		return (1);
	}

	/*
	 * Create shared memory segment. IPC_EXCL will make this call fail if the
	 * memory segment already exists.
	 * May not be a bad thing, since a former instance of radclock that has
	 * created it. However, cannot be sure the creator is the last one that has
	 * updated it, and if that guy is still alive. Hard to do here, use pid
	 * lockfile instead.
	 */
	is_new_shm = 0;
	perm_flags = SHM_R | SHM_W | (SHM_R>>3) | (SHM_R>>6);
	clock->ipc_shm_id = shmget(shm_key, sizeof(struct radclock_shm),
			IPC_CREAT | IPC_EXCL | perm_flags);
	if (clock->ipc_shm_id < 0) {
		switch(errno) {
		case (EEXIST):
			clock->ipc_shm_id = shmget(shm_key, sizeof(struct radclock_shm), 0);
			shmctl(clock->ipc_shm_id, IPC_STAT, &shm_ctl);
			shm_ctl.shm_perm.mode |= perm_flags;
			shmctl(clock->ipc_shm_id, IPC_SET, &shm_ctl);
			logger(RADLOG_NOTICE, "IPC Shared Memory exists with %u processes "
					"attached", shm_ctl.shm_nattch);
			break;

		default:
			logger(RADLOG_ERR, "shmget failed: %s\n", strerror(errno));
			return (1);
		}
	}
	else
		is_new_shm = 1;

	/*
	 * Attach the process to the memory segment. Round it to kernel page size.
	 */
	clock->ipc_shm = shmat(clock->ipc_shm_id, (void *)0, 0);
	if (clock->ipc_shm == (char *) -1) {
		logger(RADLOG_ERR, "shmat failed: %s\n", strerror(errno));
		return (1);
	}
	shm = (struct radclock_shm *) clock->ipc_shm;

	/* Zero the segment and init the buffer pointers if new. */
	if (is_new_shm) {
		memset(shm, 0, sizeof(struct radclock_shm));
		shm->data_off = offsetof(struct radclock_shm, bufdata);
		shm->data_off_old = shm->data_off + sizeof(struct radclock_data);
		shm->error_off = offsetof(struct radclock_shm, buferr);
		shm->error_off_old = shm->error_off + sizeof(struct radclock_error);
		shm->gen = 1;
	}

	// TODO: need to init version number, clockid, valid / invalid status.
	shm->version = 1;

	return (0);
}


/*
 * Do not issue an IPC_RMID. Looked like a good idea, but it is not.
 * Processes still running will be attached to old shared memory segment
 * and won't catch updates from the new instance of the daemon (the new
 * segment would have a new id).
 * Best is to have the shared memory created once, reused and never deleted.
 */
int
shm_detach(struct radclock *clock)
{
	int err;

	err = shmdt(clock->ipc_shm);
	if (err < 0)
		return (1);

	/* shmctl(handle->ipc_shm_id, IPC_RMID, NULL); */
	return (0);
}

/*
 * Initialise what is common to radclock and other apps that have a clock
 */
int
radclock_init(struct radclock *clock)
{
	int err;

	if (clock == NULL) {
		logger(RADLOG_ERR, "The clock handle is NULL and can't be initialised");
		return (-1);
	}

	/* Make sure we have detected the version of the kernel we are running on */
	clock->kernel_version = found_ffwd_kernel_version();

	err = radclock_init_vcounter_syscall(clock);
	if (err)
		return (-1);

	err = radclock_init_vcounter(clock);
	if (err < 0)
		return (-1);

	/* SHM on library side */
	err = shm_init_reader(clock);
	if (err)
		return (-1);

	return (0);
}


void
radclock_destroy(struct radclock *clock)
{
	/* Detach IPC shared memory */
	shm_detach(clock->ipc_shm);

	/* Free the clock and set to NULL, useful for partner software */
	free(clock);
	clock = NULL;
}


int
radclock_register_pcap(struct radclock *clock, pcap_t *pcap_handle)
{
	if (clock == NULL || pcap_handle == NULL)
		return (1);

	clock->pcap_handle = pcap_handle;
	return (0);
}


/*
 * Map the radclock data into the kernel FFclock data structure.
 */
void
fill_ffclock_estimate(struct radclock_data *rad_data,
		struct radclock_error *rad_err, struct ffclock_estimate *cest)
{
	vcounter_t Tlast;
	long double time;
	uint64_t period;
	uint64_t frac;

	do { // check for asynchronous update for safety, but shouldnt be possible
		Tlast = rad_data->last_changed;

		/* Direct mappings */
		cest->update_ffcount = Tlast;
		cest->status = rad_data->status;
		cest->leapsec_total		= rad_data->leapsec_total;
		cest->leapsec_next 		= rad_data->leapsec_next;
		cest->leapsec_expected	= rad_data->leapsec_expected;

		/* Convert update time estimate from an event ts to wait duration */
		cest->secs_to_nextupdate =     // poll_period, but can't access directly
			(rad_data->next_expected - rad_data->last_changed) * rad_data->phat + 0.5;
			
		/* Would like:  cest->time.frac = (time - (time_t) time) * (1LLU << 64);
		* but cannot push '1' by 64 bits, does not fit in LLU. So push 63 bits,
		* multiply for best resolution and lose resolution of 1/2^64.
		*/
		//read_RADabs_native(rad_data, &Tlast, &time, 0);  // phat only
		time = Tlast * (long double)rad_data->phat + rad_data->ca; // native clock

		cest->update_time.sec = (time_t) time;
		frac = (time - (time_t) time) * (1LLU << 63);
		cest->update_time.frac = frac << 1;

		if ( PLOCAL_ACTIVE )
			period = ((long double) rad_data->phat_local) * (1LLU << 63);
		else
			period = ((long double) rad_data->phat) * (1LLU << 63);
		
		cest->period = period << 1;

		//cest->errb_abs = (uint32_t) rad_err->error_bound_avg * 1e9;
		cest->errb_abs = (uint32_t) (rad_data->ca_err * 1e9);
		cest->errb_rate = (uint32_t) (rad_data->phat_local_err * 1e12);

	} while (Tlast != rad_data->last_changed);
	
/*
	logger(RADLOG_NOTICE, " *** checking on read_RADabs_native: time = %7.9Lf", time);
	read_RADabs_UTC(rad_data, &Tlast, &time);
	logger(RADLOG_NOTICE, " *** checking on read_RADabs_UTC:    time = %7.9Lf", time);
	time = Tlast * (long double)rad_data->phat + rad_data->ca; // native clock traditional calc
	logger(RADLOG_NOTICE, " *** checking on manual calc:        time = %7.9Lf", time);

	fprintf(stdout, "  ** BYPASSING VERB stdout:  period=%llu  phat = %.10lg, ca = %7.4Lf\n",
	(unsigned long long) cest->period, rad_data->phat, rad_data->ca);
	fprintf(stderr, "  ** BYPASSING VERB stderr:  period=%llu  phat = %.10lg, ca = %7.4Lf\n",
	(unsigned long long) cest->period, rad_data->phat, rad_data->ca);
*/
}


/*
 * Map the FFclock data to the radclock data structure.
 * Conversion based on  cest->update_time = Cabs(tlast) = phat*T(tlast) + ca
 */
void
fill_radclock_data(struct ffclock_estimate *cest, struct radclock_data *rad_data)
{
	long double tmp;
	
	/* Direct mappings */
	rad_data->last_changed = (vcounter_t) cest->update_ffcount;
	rad_data->next_expected = 0;	// signal that the kernel can't know this
	rad_data->status = (unsigned int) cest->status;
	rad_data->leapsec_total		= cest->leapsec_total;
	rad_data->leapsec_next 		= cest->leapsec_next;
	rad_data->leapsec_expected = cest->leapsec_expected;
	

	tmp = ((long double) cest->period) / (1LLU << 32);
	rad_data->phat_local = (double) (tmp / (1LLU << 32));
	rad_data->phat = rad_data->phat_local;	// true phat recovery not possible
	
	rad_data->phat_local_err = (double) cest->errb_rate / 1e12;
	rad_data->phat_err = rad_data->phat_local_err;
	rad_data->ca_err = (double) cest->errb_abs / 1e9;
	
	/* Cannot push 64 times in a LLU at once. Push twice 32 instead. In this
	 * direction (get and not set), it is ok to do it that way.
	 */
	/* Want ca = Cabs(tlast) - phat*T(tlast), but phat not in cest, use plocal */
	rad_data->ca = (long double) cest->update_time.sec;
	tmp = ((long double) cest->update_time.frac) / (1LL << 32);
	rad_data->ca += tmp / (1LL << 32);
	rad_data->ca -= (long double) rad_data->phat_local * rad_data->last_changed;
}

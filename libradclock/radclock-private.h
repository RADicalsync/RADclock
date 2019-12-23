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

#ifndef _RADCLOCK_PRIVATE_H
#define _RADCLOCK_PRIVATE_H

#include <netinet/in.h>
#include <pcap.h>


/* Data related to the clock maintain out of the kernel but specific to FreeBSD
 */
struct radclock_impl_bsd
{
	int dev_fd;
};

struct radclock_impl_linux {
	int radclock_gnl_id;
};


/*
 * Structure representing the radclock parameters
 */
struct radclock_data {
	double phat;				// very stable estimate of long term counter period [s]
	double phat_err;			// estimated bound on the relative error of phat [unitless]
	double phat_local;		//	stable estimate on shorter timescale period [s]
	double phat_local_err;  // estimated bound on the relative error of plocal [unitless]
	long double ca;			// K_p - thetahat(t) - leapsectotal? [s]
	double ca_err;				// estimated error (currently minET) in thetahat and ca [s]
	unsigned int status;		// status word (contains 10 bit fields)
	vcounter_t last_changed;	// raw timestamp T(tf) of last stamp processed [counter]
	vcounter_t next_expected;	// estimated T value of next stamp, and hence update [counter]
	vcounter_t leapsec_expected;	// estimated vcount of next leap, or 0 if none
	int leapsec_total;				// sum of leap seconds seen since clock start
	int leapsec_next;					// value of the expected next leap second {-1 0 1}
};


/* 
 * Visible error metrics 
 */ 
struct radclock_error
{
	double error_bound;
	double error_bound_avg;
	double error_bound_std;
	double min_RTT;
};

/*
 * Structure representing radclock data and exposed to system processes via IPC
 * shared memory.
 */
struct radclock_shm {
	int version;
	int status;
	int clockid;
	unsigned int gen;
	size_t data_off;
	size_t data_off_old;
	size_t error_off;
	size_t error_off_old;
	struct radclock_data bufdata[2];
	struct radclock_error buferr[2];
};

#define SHM_DATA(x)		((struct radclock_data *)((void *)x + x->data_off))
#define SHM_ERROR(x)	((struct radclock_error *)((void *)x + x->error_off))


struct radclock {

	/* System specific stuff */
	union {
		struct radclock_impl_bsd 	bsd_data;
		struct radclock_impl_linux 	linux_data;
	};

	/* IPC shared memory */
	int ipc_shm_id;
	void *ipc_shm;

	/* Description of current counter */
	char hw_counter[32];
	int kernel_version;

	radclock_local_period_t	local_period_mode;

	/* Pcap handler for the RADclock only */
	pcap_t *pcap_handle;
	int tsmode;

	/* Syscalls */
	int syscall_set_ffclock;	/* FreeBSD specific, so far */
	int syscall_get_vcounter;

	/* Read Feed-Forward counter */
	int (*get_vcounter) (struct radclock *clock, vcounter_t *vcount);
};

#define PRIV_USERDATA(x) (&(x->user_data))
#ifdef linux
# define PRIV_DATA(x) (&(x->linux_data))
#else 
# define PRIV_DATA(x) (&(x->bsd_data))
#endif


/*
 * IPC using shared memory 
 */
#define RADCLOCK_RUN_DIRECTORY		"/var/run/radclock"
#define IPC_SHARED_MEMORY			( RADCLOCK_RUN_DIRECTORY "/radclock.shm" )


/**
 * Detect possible kernel support for the RADclock prior initialisation 
 * @return The run mode the clock should be initialise to 
 */
int found_ffwd_kernel_version(void);


/**
 * System specific call for getting the capture mode on the pcap capture device.
 */
int descriptor_get_tsmode(struct radclock *clock, pcap_t *p_handle, int *kmode);


/**
 * System specific call for setting the capture mode on the pcap capture device.
 */
int descriptor_set_tsmode(struct radclock *clock, pcap_t *p_handle, int *mode);


/**
 * System specific call for getting the capture mode on the pcap capture device.
 */
int extract_vcount_stamp(
		struct radclock *clock,
		pcap_t *p_handle, 
		const struct pcap_pkthdr *header, 
		const unsigned char *packet,
		vcounter_t *vcount);

int radclock_init_vcounter_syscall(struct radclock *clock);
int radclock_init_vcounter(struct radclock *clock);
int radclock_get_vcounter_syscall(struct radclock *clock, vcounter_t *vcount);
int radclock_get_vcounter_rdtsc(struct radclock *clock, vcounter_t *vcount);

int has_vm_vcounter(struct radclock *clock);
int init_kernel_clock(struct radclock *clock_handle);

int shm_init_writer(struct radclock *clock);
int shm_detach(struct radclock *clock);



/* Read the RADclock absolute clock within the daemon. Two versions:
 * 	read_RADabs_native  leap-free absolute clock of the algo (formerly counter_to_time)
 * 	read_RADabs_UTS	  UTC absolute clock, which includes leaps
 * The clocks can be read into the future or past wrt the rad_data used.
 *
 * Each takes a plocal flag, which controls if the plocal variable is used.
 * For most purposes, including the time sent in client or server NTP packets,
 * the flag is set to PLOCAL_ACTIVE, with a typical default  1 = active = using plocal.
 * The radclock library routines instead use clock->local_period_mode  to select
 * per-clock preferences passed to read_RADabs_{native,UTC} .
 */

/* Global setting for use of plocal refinement for in-daemon timestamping */
# define PLOCAL_ACTIVE	1		//  {0,1} = {inactive, active}

static inline void
read_RADabs_native(struct radclock_data *rad_data, vcounter_t *vcount,
	long double *time, int useplocal)
{
	vcounter_t last;

	do {
		last  = rad_data->last_changed;
		*time = *vcount * (long double)rad_data->phat + rad_data->ca;
		if ( useplocal == 1 && rad_data->phat != rad_data->phat_local )
			*time += (*vcount - last) * (long double)(rad_data->phat_local - rad_data->phat);
	} while (last != rad_data->last_changed);
}

static inline void
read_RADabs_UTC(struct radclock_data *rad_data, vcounter_t *vcount,
	long double *time, int useplocal)
{
	vcounter_t last;

	do {
		last  = rad_data->last_changed;
		*time = *vcount * (long double)rad_data->phat + rad_data->ca;
		if ( useplocal == 1 && rad_data->phat != rad_data->phat_local ) {
			//fprintf(stdout, "** read_RADabs_UTC:  without: %LF, ", *time);
			*time += (*vcount - last) * (long double)(rad_data->phat_local - rad_data->phat);
			//fprintf(stdout, "*** with:  %Lf (plocal = %d)\n", *time, useplocal);
		}
	} while (last != rad_data->last_changed);
	
	/* Include leapseconds in native RADclock to form UTC RADclock */
	*time -= rad_data->leapsec_total;	// subtracting means including leaps!
	if ( rad_data->leapsec_expected != 0  &&  *vcount > rad_data->leapsec_expected)
		*time -= rad_data->leapsec_next; // if new leap flagged, and past it, then include it
}


struct ffclock_estimate;

/* Functions to print out rad_data and FF_data neatly and fully */
void printout_raddata(struct radclock_data *rad_data);
void printout_FFdata(struct ffclock_estimate *cest);



#endif

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

#include <arpa/inet.h>

#include <string.h>
#include <math.h>
#include <syslog.h>
#include <time.h>

#include "../config.h"
#include "radclock.h"
#include "radclock-private.h"
#include "kclock.h"

#include "radclock_daemon.h"
#include "sync_history.h"
#include "sync_algo.h"
#include "fixedpoint.h"
#include "verbose.h"
#include "proto_ntp.h"
#include "misc.h"
#include "stampinput.h"
#include "stampoutput.h"
#include "config_mgr.h"
#include "pthread_mgr.h"
#include "jdebug.h"

#include <sys/sysctl.h>		// TODO remove when pushing sysctl code within arch
							// specific code


#ifdef WITH_FFKERNEL_NONE
int update_FBclock(struct radclock_handle *handle) { return (0); }
int update_ipc_shared_memory(struct radclock_handle *handle) { return (0); };
#else

#ifdef WITH_FFKERNEL_FBSD
#include <sys/timex.h>
#define NTP_ADJTIME(x)	ntp_adjtime(x)
#endif

#ifdef WITH_FFKERNEL_LINUX
#include <sys/timex.h>
#define NTP_ADJTIME(x)	adjtimex(x)
#endif

#ifndef SHIFT_USEC
#define SHIFT_USEC 16
#endif

/* Make TIME_CONSTANT smaller for faster convergence but keep diff between nano
 * and not nano = 4
 */
#ifdef STA_NANO
#define KERN_RES	1e9
#define TIME_CONSTANT	6
#define TX_MODES	( MOD_OFFSET | MOD_STATUS | MOD_NANO )
#else
#define KERN_RES	1e6
#define TIME_CONSTANT	2
#define TX_MODES	( MOD_OFFSET | MOD_STATUS )
#endif


/*
 * Update IPC shared memory segment.
 * Swap pointers and bump generation number to ensure consistency.
 */
int
update_ipc_shared_memory(struct radclock_handle *handle)
{
	struct radclock_shm *shm;
	size_t offset_tmp;
	unsigned int generation;

	JDEBUG

	shm = (struct radclock_shm *) handle->clock->ipc_shm;

	memcpy((void *)shm + shm->data_off_old, &handle->rad_data,
			sizeof(struct radclock_data));
	memcpy((void *)shm + shm->error_off_old, &handle->rad_data,
			sizeof(struct radclock_error));
	generation = shm->gen;

	shm->gen = 0;

	/* Swap current and old buffer offsets in the mapped SHM */
	offset_tmp = shm->data_off;
	shm->data_off = shm->data_off_old;
	shm->data_off_old = offset_tmp;

	offset_tmp = shm->error_off;
	shm->error_off = shm->error_off_old;
	shm->error_off_old = offset_tmp;

	if (generation++ == 0)
		generation = 1;
	shm->gen = generation;

	if ( VERB_LEVEL>2 ) verbose(LOG_NOTICE, "Updated IPC Shared memory");
	return (0);
}


// FIXME: used to be static and inline. But virtual machine loop needed
// it to adjust system clock. This is a worse hack built on top of a bad
// hack and all this mess should be cleaned up.
/* Report back to back timestamps of RADclock and system clock */
void
read_clocks(struct radclock_handle *handle, struct timeval *sys_tv,
	struct timeval *rad_tv, vcounter_t *counter)
{
	vcounter_t before;
	vcounter_t after;
	long double time;
	int i;

	/*
	 * Make up to 5 attempts to bracket a reading of the system clock. A system
	 * call is in the order of 1-2 mus, here we have 3 of them. Pick up an
	 * (arbitrary) bracket threshold: 5 mus.
	 */
	for (i=0; i<5; i++) {
		radclock_get_vcounter(handle->clock, &before);
		gettimeofday(sys_tv, NULL);
		radclock_get_vcounter(handle->clock, &after);

		if ((after - before) < (5e-6 / RAD_DATA(handle)->phat))
			break;
	}
	verbose(VERB_DEBUG, "System clock read_clocks bracket: "
		"%"VC_FMT" [cycles], %.03f [mus]",
		(after - before),
		(after - before) * RAD_DATA(handle)->phat * 1e6 );

	*counter = (vcounter_t) ((before + after)/2);
	read_RADabs_UTC(&handle->rad_data, counter, &time, PLOCAL_ACTIVE);
	UTCld_to_timeval(&time, rad_tv);
}



/* There are a few assumptions on the kernel capabilities, i.e. RFC1589
 * compatible. Should be fairly safe with recent systems these days.  The code
 * in here is in packets chronological order, could have made it prettier with a
 * little state machine.
 */
int
update_FBclock(struct radclock_handle *handle)
{
	long double time;
	vcounter_t vcount;
	struct timeval rad_tv;
	struct timeval sys_tv;
	struct timeval delta_tv;
	struct timex tx;
	double offset; 		/* [sec] */
	double freq; 		/* [PPM] */
	static vcounter_t sys_init;
	static struct timeval sys_init_tv;
	static int next_stamp;
	int poll_period;
	int err;

	JDEBUG

	memset(&tx, 0, sizeof(struct timex));

	/* At the very beginning, we are sending a few packets in burst. Let's be
	 * patient to have a decent radclock data and simply mark initialisation.
	 */
	if (OUTPUT(handle, n_stamps) < NTP_BURST) {
		sys_init = 0;
		return (0);
	}

	/* Set the clock at the end of burst phase. Yes it is a bit harsh since it
	 * can break causality but not worst than using ntpdate or equivalent (and
	 * we do that only once).
	 */
	if (OUTPUT(handle, n_stamps) == NTP_BURST) {
		radclock_get_vcounter(handle->clock, &vcount);
		read_RADabs_UTC(&handle->rad_data, &vcount, &time, PLOCAL_ACTIVE);
		UTCld_to_timeval(&time, &rad_tv);
		err = settimeofday(&rad_tv, NULL);
		if ( err < 0 )
			verbose(LOG_WARNING, "System clock update failed on settimeofday()");
		else
			verbose(VERB_CONTROL, "System clock set to %d.%06d [sec]", rad_tv.tv_sec,
					rad_tv.tv_usec);

		memset(&tx, 0, sizeof(struct timex));
		tx.modes = MOD_FREQUENCY | MOD_STATUS;
		tx.status = STA_UNSYNC;
		tx.freq = 0;
		err = NTP_ADJTIME(&tx);
		return (err);
	}

	/* Want to make sure we never pass here after freq estimation has started.
	 * The condition here should do the trick
	 */
	if (sys_init == 0) {
		/* Use legacy adjtime to bring system clock as close as possible but
		 * with respecting causality and a monotonic clock.
		 */
		read_clocks(handle, &sys_tv, &rad_tv, &vcount);
		subtract_tv(&delta_tv, rad_tv, sys_tv);
		offset = delta_tv.tv_sec + (double)delta_tv.tv_usec / 1e6;

		err = adjtime(&delta_tv, NULL);
		if (err < 0)
			verbose(LOG_WARNING, "System clock update failed on adjtime()");
		else {
			verbose(VERB_DEBUG, "System clock update adjtime(%d.%06d) [s]",
					delta_tv.tv_sec, delta_tv.tv_usec);
		}
	
		memset(&tx, 0, sizeof(struct timex));
		err = NTP_ADJTIME(&tx);
		verbose(VERB_DEBUG, "System clock stats (offset freq status) %.09f %.2f %d",
				(double)(tx.offset/KERN_RES), (double)tx.freq/(1L<<SHIFT_USEC),
				tx.status);

		/* If we have reach a fairly good quality and brought the system clock
		 * close enough, set clock UNSYNC and make freq estimate over ~ 60 sec.
		 * Once the freq skew is set to 0 the clock is potentially running
		 * frantically. The worst case should be a drift clamped down to 500 PPM
		 * by the kernel. Over 60 sec we accumulate about 30 ms of error which
		 * is still acceptable.  Rounding to upper value should deal with poll
		 * periods > 60 sec ...  you can ask "what about the drift then?" Also,
		 * clean up the possible broken estimate of counter frequency skew. As
		 * mentioned in ntpd code, it is equivalent to removing any corrupted
		 * drift file.
		 */
		if (RAD_DATA(handle)->phat_err < 5e-7  && ( fabs(offset) < 1e-3)) {
			next_stamp = (int) (60 / handle->conf->poll_period) + 1;
			next_stamp = next_stamp + ((struct bidir_output*)handle->algo_output)->n_stamps;

			memset(&tx, 0, sizeof(struct timex));
			tx.modes = MOD_FREQUENCY | MOD_STATUS;
			tx.status = STA_UNSYNC;
			tx.freq = 0;
			err = NTP_ADJTIME(&tx);

			verbose(VERB_DEBUG, "System clock stats (offset freq status) %.09f %.2f %d",
				(double)(tx.offset/KERN_RES), (double)tx.freq/(1L<<SHIFT_USEC),
				tx.status);

			/* Left hand side of freq skew estimation */
			read_clocks(handle, &sys_tv, &rad_tv, &vcount);
			sys_init_tv = sys_tv;
			sys_init = vcount;
			verbose(VERB_DEBUG, "System clock frequency skew estimation start "
					"(%d.%.06d | %"VC_FMT")", sys_init_tv.tv_sec,
					sys_init_tv.tv_usec, sys_init);
		}

		return (err);
	}


	/* In here we wait for the freq skew estimation period to elapse. Do not try to
	 * adjust the freq skew in here, that would lead to disastrous results with
	 * a meaningless estimate (I tried ;-))
	 */
	if (OUTPUT(handle, n_stamps) < next_stamp)
		return (0);

	/* End of the skew period estimation. Compute the freq skew and pass it to
	 * the kernel. Go on directly into STA_PLL.
	 */
	if (OUTPUT(handle, n_stamps) == next_stamp) {
		read_clocks(handle, &sys_tv, &rad_tv, &vcount);
		subtract_tv(&delta_tv, sys_tv, sys_init_tv);
		offset = delta_tv.tv_sec + (double)delta_tv.tv_usec / 1e6;
		freq = ((RAD_DATA(handle)->phat * ((vcount - sys_init) / offset)) - 1) * 1e6;

		subtract_tv(&delta_tv, rad_tv, sys_tv);
		offset = delta_tv.tv_sec + (double)delta_tv.tv_usec / 1e6;

		tx.modes = TX_MODES | MOD_FREQUENCY;
		tx.offset = (int32_t) (offset * KERN_RES);
		tx.status = STA_PLL | STA_FLL;
		tx.freq = freq * (1L << SHIFT_USEC);
		err = NTP_ADJTIME(&tx);

		verbose(VERB_DEBUG, "System clock frequency skew estimation end "
			"(%d.%.06d | %"VC_FMT")",
			sys_tv.tv_sec, sys_tv.tv_usec, vcount);

		/* Make up for the frantic run */
		read_clocks(handle, &sys_tv, &rad_tv, &vcount);
		subtract_tv(&delta_tv, rad_tv, sys_tv);
		err = adjtime(&delta_tv, NULL);

		memset(&tx, 0, sizeof(struct timex));
		err = NTP_ADJTIME(&tx);
		verbose(VERB_DEBUG, "System clock freq skew estimated "
			"(offset freq status) %.09f %.2f %d",
			(double)(tx.offset / KERN_RES), (double)tx.freq / (1L<<SHIFT_USEC),
			tx.status);
	}

	/* Here is the normal mode of operation for updating the system clock. Use
	 * the ntp_time interface to the kernel to pass offset estimates and let the
	 * kernel PLL infer the corresponding freq skew.
	 */
	read_clocks(handle, &sys_tv, &rad_tv, &vcount);
	subtract_tv(&delta_tv, rad_tv, sys_tv);
	offset = delta_tv.tv_sec + (double)delta_tv.tv_usec / 1e6;

	tx.modes = TX_MODES | MOD_MAXERROR | MOD_ESTERROR | MOD_TIMECONST;
	tx.offset = (int32_t) (offset * KERN_RES);
	tx.status = STA_PLL;
	tx.maxerror = (long) ((NTP_SERVER(handle)->rootdelay/2 +
			NTP_SERVER(handle)->rootdispersion) * 1e6);
	/* TODO: not the right estimate !! */
	tx.esterror = (long) (RAD_DATA(handle)->phat * 1e6);
	
	/* Play slightly with the rate of convergence of the PLL in the kernel. Try
	 * to converge faster when it is further away
	 * Also set a the status of the sysclock when it gets very good.
	 */
	if (fabs(offset) > 100e-6) {
		tx.constant = TIME_CONSTANT - 2;
		DEL_STATUS(handle, STARAD_SYSCLOCK);
	} else {
		ADD_STATUS(handle, STARAD_SYSCLOCK);
		if (fabs(offset) > 40e-6)
			tx.constant = TIME_CONSTANT - 1;
		else
			tx.constant = TIME_CONSTANT;
	}

	err = NTP_ADJTIME(&tx);

	verbose(VERB_DEBUG, "System clock PLL adjusted "
		"(offset freq status maxerr esterr) %.09f %.2f %d %.06f %.06f",
		(double)(tx.offset/KERN_RES), (double)tx.freq/(1L<<SHIFT_USEC),
		tx.status, (double)tx.maxerror/1e6, (double)tx.esterror/1e6 );

	poll_period = ((struct bidir_peer*)(handle->active_peer))->poll_period;

	if (VERB_LEVEL && !(OUTPUT(handle, n_stamps) % (int)(3600*6/poll_period))) {
		verbose(VERB_CONTROL, "System clock PLL adjusted (offset freq status "
			"maxerr esterr) %.09f %.2f %d %.06f %.06f",
			(double)(tx.offset / KERN_RES), (double)tx.freq / (1L<<SHIFT_USEC),
			tx.status, (double)tx.maxerror / 1e6, (double)tx.esterror / 1e6 );
	}

	return (err);
}

#endif /* KERNEL_NONE */




/*
 * Check stamps are not insane. The world is divided in black, white and ...
 * grey. White stamps are clean. Grey stamps have a qual_warning problem, but it
 * is not clear what to do, and that's up to the algo to deal with them. Black
 * stamps are insane and could break processing (e.g. induce zero division, NaN
 * results, etc.). We get rid of them here.
 */
int
insane_bidir_stamp(struct stamp_t *stamp, struct stamp_t *laststamp)
{
	/* Sanity check if two consecutive stamps are identical
	 *
	 * Two identical bidirectional stamps are physically impossible since it
	 * would require to read twice the same counter value on the outgoing
	 * packet.  This can only happen if we made something stupid when creating
	 * stamp or if we replay a bogus data file. In such a case we skip all the
	 * algo.
	 * note: laststamp initialised at i=0, so can only compare if i>0,
	 * implies we cannot check the stamp at i=0 (but that is obviously OK)
	 */

	if (stamp->type != laststamp->type) {
		verbose(LOG_ERR, "Insane Stamp: Trying to compare two stamps of different types %d and %d",
				stamp->type, laststamp->type);
		return (1);
	}

	if (memcmp(stamp, laststamp, sizeof(struct stamp_t)) == 0) {
		verbose(LOG_WARNING, "Insane Stamp: Two identical consecutive stamps detected");
		return (1);
	}

	/* Non existent stamps */
	if ((BST(stamp)->Ta == 0) || (BST(stamp)->Tb == 0) ||
			(BST(stamp)->Te == 0) || (BST(stamp)->Tf == 0)) {
		verbose(LOG_WARNING, "Insane Stamp: bidir stamp with at least one 0 raw stamp");
		return (1);
	}

	/* Check for strict increment of counter based on previous stamp
	 * Previous version was rejecting overlapping stamps, i.e.
	 * 		stamp->Ta <= laststamp->Tf
	 * was considered insane. With very large RTT and retransmission of the NTP
	 * request if the socket has timed out, this is definitely a possible
	 * scenario. Relax the constraint a bit by limiting to what we know is
	 * insane for sure:
	 * 		stamp->Ta <= laststamp->Ta
	 */
	if (BST(stamp)->Ta <= BST(laststamp)->Tf)
		verbose(VERB_DEBUG, "Insane Stamp: Successive stamps overlapping");

	if (BST(stamp)->Ta <= BST(laststamp)->Ta) {
		verbose(LOG_WARNING, "Successive NTP requests with non-strictly "
				"increasing counter");
		return (1);
	}

	/* RAW stamps completely messed up */
	if ((BST(stamp)->Tf < BST(stamp)->Ta) || (BST(stamp)->Te < BST(stamp)->Tb)) {
		verbose(LOG_WARNING, "Insane Stamp: bidir stamp broke local causality");
		return (1);
	}

	/* This does not apply to SPY_STAMP for example */
	if (stamp->type == STAMP_NTP) {
		/* Sanity checks on null or too small RTT.
		 * Smallest RTT ever: 100 mus
		 * Slowest counter  : 1193182 Hz
		 * Cycles :  ceil( 100e-6 * 1193182 ) = 120
		 * 		i8254 =   1193182
		 * 		 ACPI =   3579545
		 * 		 HPET =  14318180
		 * 		 TSC  > 500000000
		 */
		if ((BST(stamp)->Tf - BST(stamp)->Ta) < 120) {
			verbose(LOG_WARNING, "Insane Stamp: bidir stamp with RTT impossibly low (< 120)"
					": %"VC_FMT" cycles", BST(stamp)->Tf - BST(stamp)->Ta);
			return (1);
		}
	}

	/* If we pass all sanity checks */
	return (0);
}



/**
 * XXX TODO: so far we suppose bidir paradigm only and a single source at a time!!
 */
int
process_stamp(struct radclock_handle *handle, struct bidir_peer *peer)
{
	/* Bi-directionnal stamp passed to the algo for processing */
	struct stamp_t stamp;
	static struct stamp_t laststamp;		// remember last stamp (with original Te,Tf)
	struct ffclock_estimate cest;

	/* Leap second management */
	static int postleap_freeze = 0;		// inhibit leap actions after a leap
	static int leap_warningcount = 0;	// >=0, accumulate count of LI warnings
	static int leap_imminent = 0;			// flag detection of an impending leap
	static long double tleap = 0;			// the UTC second of the expected leap
	struct bidir_stamp stamp_noleap;
	vcounter_t now, vcount;
	long double radtime;
	
	/* Error control logging */
	long double currtime 	= 0;
	double min_RTT 			= 0;
	double timediff 		= 0;
	double error_bound 		= 0;
	double error_bound_avg 	= 0;
	double error_bound_std 	= 0;
	int err, err_read;
	
	/* Check hardware counter has not changed */
	// XXX TODO this is freebsd specific, should be put with arch specific code
#ifdef WITH_FFKERNEL_FBSD
	char hw_counter[32];
	size_t size_ctl;
#endif

	JDEBUG

	/* Generic call for creating stamps depending on the type of input source */
	// Need to differentiate ascii input from pcap input
	err = get_next_stamp(handle, (struct stampsource *)handle->stamp_source, &stamp);

	/* Signal big error */
	if (err == -1)
		return (-1);

	/* Starvation test:  have valid stamps stopped arriving to algo?
	 * Definition based on algo input only, as cannot be evaluated by the algo.
	 * Hence is a function of the duration of the missing stamp gap, measured in
	 * poll period units. Stamps can be missing, or invalid, or any reason.
	 * Test should run once per trigger-grid point. Due to structure of TRIGGER--
	 * PROC interactions, process_stamp runs each time, so this is true.
	 */
	if (handle->run_mode == RADCLOCK_SYNC_LIVE) {
		err_read = radclock_get_vcounter(handle->clock, &vcount);
		if (err_read < 0)
			return (-1);

		if ((vcount - RAD_DATA(handle)->last_changed) * RAD_DATA(handle)->phat >
				10*(((struct bidir_peer*)(handle->active_peer))->poll_period)) {
			if (!HAS_STATUS(handle, STARAD_STARVING)) {
				verbose(LOG_WARNING, "Clock is starving. Gap has exceeded 10 stamps");
				ADD_STATUS(handle, STARAD_STARVING);
			}
		}
	}

	/* No error, but no stamp to process */
	if (err == 1)
		return (1);

	/* If the new stamp looks insane just don't pass it for processing, keep
	 * going and look for the next one. Otherwise, record it.
	 */
	// TODO: this should be stored in a proper structure under the clock handle, why?
	// check if insane_bidir_stamp wont kick a stamp out after a leap, or leap screw ups
	if (OUTPUT(handle, n_stamps) > 1) {
		if (insane_bidir_stamp(&stamp, &laststamp))
			return (0);
	}
	// Starvation release in case of a stamp passing through to algo
	if (HAS_STATUS(handle, STARAD_STARVING)) {
		verbose(LOG_NOTICE, "Clock no longer starving");
		DEL_STATUS(handle, STARAD_STARVING);
	}
	
	memcpy(&laststamp, &stamp, sizeof(struct stamp_t));

	// TODO: - should be stored in a proper structure under the clock handle
	// 		- check if insane_bidir_stamp wont kick a stamp out after a leap..
	/* Valid stamp obtained, take note and pass to the algo */
	OUTPUT(handle, n_stamps)++;



	/* Leapsecond management 
	 * There are three elements :
	 * i) detection of an upcoming leap
	 * ii) processing of clock data before and after a leap
	 * iii) protection of the algo from leap effects
	 */
	
/* Hacks for leapsecond testing */
/*	if (OUTPUT(handle, n_stamps) == 10) {
		leap_imminent = 1;		// set once, will stay set until cleared by leap
		//  put a leap 6 stamps in future
		tleap = stamp.st.bstamp.Te + 6 * peer->poll_period;
		stamp.LI = LEAP_ADDSECOND;
	}
*/
	
	/* (i) Determine if there is enough evidence of an impending leap to declare
	 * leap_imminent. However, only do this and other leap processing if outside 
	 * of the moratoriam following a recent leap, to avoid possible leap jitter.
	 */
	if (postleap_freeze > 0)
		postleap_freeze--;
	else
		//if ((date(stamp.Tf) in 24hr before Jan or July) { // in zone where leaps can occur
		if (leap_imminent) { // deactivating placeholder for compilation, shouldnt be leap_imminent here !!
			/* Collect information on potential leap */
			switch (stamp.LI) {
				case LEAP_ADDSECOND:
					OUTPUT(handle, leapsec_next) = 1;	leap_warningcount++;	break;
				case LEAP_DELSECOND:
					OUTPUT(handle, leapsec_next) = -1;	leap_warningcount++;	break;
				default:		if (leap_warningcount>0) leap_warningcount--;
			}
			/* Conclude imminent if critical mass of warnings present in final hours */
			/* Once triggered, imminent state remains until cleared by leap */
			if (leap_warningcount > 10 && leap_imminent != 1) {
				verbose(VERB_CONTROL, "** Leapsec ctrl: critical mass of LI warnings reached");
				//tleap = (vcounter_t) lastsecofmonth + leapsec_next;	// get this somehow, assume ignores leap!
				tleap = 1; // placeholder for compilation only
				if (tleap - stamp.st.bstamp.Tf < 2*3600)
					leap_imminent = 1;
			}
		}

	/* (ii) If a leap coming, set up clock data to signal this and to cope with
	 * its arrival inbetween algo updates. If already past, incorporate it into
	 * the leapsec_total and silence the signal. The predicted vcount for the 
	 * leap instant, leapsec_expected, will be updated on each stamp before the 
	 * leap, and if ever exceeded, radclock must leap for consistency with 
	 * RADclock reads which may already have occurred, eg in libprocesses.
	 */
	if (leap_imminent) {
		if (handle->run_mode == RADCLOCK_SYNC_LIVE)
			radclock_get_vcounter(handle->clock, &now);
		else
			now = 0;
		//verbose(LOG_NOTICE, "** Leapsec ctrl: now = %llu", now);
		
		if	( stamp.st.bstamp.Te >= tleap || ( OUTPUT(handle, leapsec_expected) > 0
					&& now >= OUTPUT(handle, leapsec_expected) )) { // radclock must leap
			/* Reset radclock leap parameters */
			OUTPUT(handle, leapsec_total) += OUTPUT(handle, leapsec_next);
			OUTPUT(handle, leapsec_expected) = 0;    // signal no leap is imminent
			OUTPUT(handle, leapsec_next) = 0;
			verbose(LOG_ALERT, "** Leapsec ctrl: RADclock just leapt by %d [s], leapsec_total now %d [s]",
				OUTPUT(handle, leapsec_next), OUTPUT(handle, leapsec_total));
			/* Reset leap management parameters */
			postleap_freeze = 1000;
			leap_warningcount = 0;
			leap_imminent = 0;
		} else {							// leap still ahead, update preparations
			read_RADabs_UTC(&handle->rad_data, &(stamp.st.bstamp.Tf), &radtime, 1);
			OUTPUT(handle, leapsec_expected) = stamp.st.bstamp.Tf +
					(vcounter_t) ((tleap - radtime)/(long double)peer->plocal);
			verbose(LOG_NOTICE, "** Leapsec ctrl: jump imminent, leap of %d expected in %4.2Lf [sec] (leapsec_expected = %llu)",
				OUTPUT(handle, leapsec_next), tleap - radtime, OUTPUT(handle, leapsec_expected));
			//verbose(LOG_NOTICE, "** Leapsec ctrl: jump imminent, leap of %d expected in %4.2Lf [sec] (%4.2Lf  %4.2Lf  counter = %llu)", OUTPUT(handle, leapsec_next), tleap - radtime, tleap, radtime, OUTPUT(handle, leapsec_expected));
		}
	}

 
	/* (iii) Run RADclock algorithm to update radclock parameters with new stamp.
	 * Pass it server timestamps which have had leapseconds removed, so algo sees
	 * a continuous timescale and is leapsecond oblivious.
	 * After updating, also update leap second parameters, and incorporate all 
	 * into rad_data ready for publication to the IPC and kernel.
	 */

	/* Update radclock parameters using leap-free stamp */
	stamp_noleap = stamp.st.bstamp;
	stamp_noleap.Tb += OUTPUT(handle, leapsec_total);	// adding means removing
	stamp_noleap.Te += OUTPUT(handle, leapsec_total);
	process_bidir_stamp(handle, peer, &stamp_noleap, stamp.qual_warning);

	/* Update RADclock data with new algo outputs, and leap second update */
	pthread_mutex_lock(&handle->globaldata_mutex);	// ensure consistent reads

	RAD_DATA(handle)->phat				= peer->phat;
	RAD_DATA(handle)->phat_err			= peer->perr;
	RAD_DATA(handle)->phat_local		= peer->plocal;
	RAD_DATA(handle)->phat_local_err	= peer->plocalerr;
	RAD_DATA(handle)->ca					= peer->K - (long double)peer->thetahat;
	RAD_DATA(handle)->ca_err			= RAD_ERROR(handle)->error_bound;
	RAD_DATA(handle)->last_changed	= stamp.st.bstamp.Tf;
	RAD_DATA(handle)->next_expected	= stamp.st.bstamp.Tf +
								(vcounter_t) ((double)peer->poll_period / peer->phat);
	RAD_DATA(handle)->leapsec_total	= OUTPUT(handle, leapsec_total);
	RAD_DATA(handle)->leapsec_next	= OUTPUT(handle, leapsec_next);
	RAD_DATA(handle)->leapsec_expected	= OUTPUT(handle, leapsec_expected);
	
	pthread_mutex_unlock(&handle->globaldata_mutex);
	
	if ( VERB_LEVEL>2 ) {
		verbose(LOG_DEBUG, "In process_stamp");
		printout_raddata(RAD_DATA(handle));
	}

	/*
	 * RAD data copy actions, only relevant when running Live
	 *		- IPC shared memory update
	 *		- FFclock kernel parameters update
	 *		- VM store update
	 *		- FBclock kernel parameters update
	 */
  	if (handle->run_mode == RADCLOCK_SYNC_LIVE) {

		/* Update IPC shared memory segment for used by libprocesses */
		if (handle->conf->server_ipc == BOOL_ON) {
			if (!HAS_STATUS(handle, STARAD_UNSYNC))
				update_ipc_shared_memory(handle);
		}

		/* Update FFclock parameters, provided RADclock is synchronized */
		if (!HAS_STATUS(handle, STARAD_UNSYNC)) {
			if (handle->clock->kernel_version < 2) {
				update_kernel_fixed(handle);
				verbose(VERB_DEBUG, "Sync pthread updated kernel fixed pt data.");
			} else {

			// XXX Out of whack, need cleaning when make next version linux support
			// FIXME
	#ifdef WITH_FFKERNEL_FBSD
				/* If hardware counter has changed, restart over again */
				size_ctl = sizeof(hw_counter);
				err = sysctlbyname("kern.timecounter.hardware", &hw_counter[0],
						&size_ctl, NULL, 0);
				if (err == -1) {
					verbose(LOG_ERR, "Can''t find kern.timecounter.hardware in sysctl");
					return (-1);
				}
				
				if (strcmp(handle->clock->hw_counter, hw_counter) != 0) {
					verbose(LOG_WARNING, "Hardware counter has changed (%s -> %s)."
						" Reinitialising radclock.", handle->clock->hw_counter,
						hw_counter);
					OUTPUT(handle, n_stamps) = 0;
					peer->stamp_i = 0;
					NTP_SERVER(handle)->burst = NTP_BURST;
					strcpy(handle->clock->hw_counter, hw_counter);
				// XXX TODO: Reinitialise the stats structure as well?
					return (0);
				}
	#endif
				fill_ffclock_estimate(&handle->rad_data, &handle->rad_error, &cest);
				if ( VERB_LEVEL>2 ) printout_FFdata(&cest);
				set_kernel_ffclock(handle->clock, &cest);
				verbose(VERB_DEBUG, "FF kernel data has been updated.");
				if ( VERB_LEVEL>2 ) { // check was updated as expected
					get_kernel_ffclock(handle->clock, &cest);
					printout_FFdata(&cest);
				}

			}
		}

		/* Update any virtual machine store if configured */
		if (VM_MASTER(handle)) {
			err = push_data_vm(handle);
			if (err < 0) {
				verbose(LOG_WARNING, "Error attempting to push VM data");
				return (1);
			}
		}

		/* Adjust system FBclock if requested (and if not piggybacking on ntpd) */
		if (handle->conf->adjust_FBclock == BOOL_ON) { // TODO: catch errors
			update_FBclock(handle);
			verbose(VERB_DEBUG, "Feed-back kernel clock has been set.");
		}

   }  // RADCLOCK_SYNC_LIVE actions


	/* Write algo output to matlab file, much less urgent than previous tasks */
	print_out_files(handle, &stamp);
	
	/* View updated RADclock data and compare with NTP server stamps in nice
	 * format. The first 10 then every 6 hours (poll_period can change, but
	 * should be fine with a long term average, do not have to be very precise
	 * anyway).
	 * Note: ->n_stamps has been incremented by the algo to prepare for next
	 * stamp.
	 */
	//poll_period = ((struct bidir_peer *)(handle->active_peer))->poll_period;
	if (VERB_LEVEL && ((OUTPUT(handle, n_stamps) < 50) ||
			!(OUTPUT(handle, n_stamps) % ((int)(3600*6/peer->poll_period)))))
	{
		read_RADabs_UTC(&handle->rad_data, &(RAD_DATA(handle)->last_changed),
				&currtime, PLOCAL_ACTIVE);
		min_RTT = RAD_ERROR(handle)->min_RTT;
		timediff = (double) (currtime - (long double) BST(&stamp)->Te);

		verbose(VERB_CONTROL, "i=%ld: NTPserver stamp %.6Lf, RAD - NTPserver "
				"= %.3f [ms], RTT/2 = %.3f [ms]",
				OUTPUT(handle, n_stamps) - 1,
				BST(&stamp)->Te, timediff * 1000, min_RTT / 2 * 1000);

		error_bound = RAD_ERROR(handle)->error_bound;
		error_bound_avg = RAD_ERROR(handle)->error_bound_avg;
		error_bound_std = RAD_ERROR(handle)->error_bound_std;
		verbose(VERB_CONTROL, "i=%ld: Clock Error Bound (cur,avg,std) %.6f "
				"%.6f %.6f [ms]",
				OUTPUT(handle, n_stamps) - 1,
				error_bound * 1000, error_bound_avg * 1000,
				error_bound_std * 1000);
	}

	/* Set initial state of 'signals' - important !!
	 * Has to be placed here, after the algo handled the possible new
	 * parameters, with the next packets coming.
	 */
	handle->conf->mask = UPDMASK_NOUPD;

	JDEBUG_RUSAGE
	return (0);
}



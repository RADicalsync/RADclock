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

#include <arpa/inet.h>

#include <string.h>
#include <math.h>
#include <syslog.h>
#include <time.h>
#include <signal.h>

#include "../config.h"
#include "radclock.h"
#include "radclock-private.h"
#include "kclock.h"

#include "radclock_daemon.h"
#include "sync_history.h"
#include "proto_ntp.h"
#include "sync_algo.h"
#include "fixedpoint.h"
#include "verbose.h"
#include "misc.h"
#include "ntp_auth.h"
#include "stampinput.h"
#include "stampoutput.h"
#include "config_mgr.h"
#include "pthread_mgr.h"
#include "jdebug.h"
#include "telemetry_producer.h"

#include <sys/sysctl.h>		// TODO remove when pushing sysctl code within arch
							// specific code

/* From create_stamp.c,  needed for perf stamp testing in process_stamp */
int          insertandmatch_halfstamp(struct stamp_queue *q, struct stamp_t *new, int mode);
int get_fullstamp_from_queue_andclean(struct stamp_queue *q, struct stamp_t *stamp);

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
	struct radclock_sms *sms;
	size_t offset_tmp;
	unsigned int generation;

	JDEBUG

	sms = (struct radclock_sms *) handle->clock->ipc_sms;

	memcpy((void *)sms + sms->data_off_old, RAD_DATA(handle),
			sizeof(struct radclock_data));
	memcpy((void *)sms + sms->error_off_old, RAD_ERROR(handle),
			sizeof(struct radclock_error));
	generation = sms->gen;

	sms->gen = 0;

	/* Swap current and old buffer offsets in the mapped SMS */
	offset_tmp = sms->data_off;
	sms->data_off = sms->data_off_old;
	sms->data_off_old = offset_tmp;

	offset_tmp = sms->error_off;
	sms->error_off = sms->error_off_old;
	sms->error_off_old = offset_tmp;

	if (generation++ == 0)
		generation = 1;
	sms->gen = generation;

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
	read_RADabs_UTC(RAD_DATA(handle), counter, &time, PLOCAL_ACTIVE);
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
		read_RADabs_UTC(RAD_DATA(handle), &vcount, &time, PLOCAL_ACTIVE);
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
			next_stamp = next_stamp + OUTPUT(handle, n_stamps);

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
		DEL_STATUS(RAD_DATA(handle), STARAD_SYSCLOCK);
	} else {
		ADD_STATUS(RAD_DATA(handle), STARAD_SYSCLOCK);
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

	poll_period = ((struct bidir_algodata*)handle->algodata)->state[handle->pref_sID].poll_period;

	if (VERB_LEVEL && !(OUTPUT(handle, n_stamps) % (int)(3600*6/poll_period))) {
		verbose(VERB_CONTROL, "System clock PLL adjusted (offset freq status "
			"maxerr esterr) %.09f %.2f %d %.06f %.06f",
			(double)(tx.offset / KERN_RES), (double)tx.freq / (1L<<SHIFT_USEC),
			tx.status, (double)tx.maxerror / 1e6, (double)tx.esterror / 1e6 );
	}

	return (err);
}

#endif /* KERNEL_NONE */



/* Manage and set the three leapsecond state variables held in rad_data
 * The detection of leap seconds, and reaction to them, involved keeping state
 * over an extended period, held in static variables here.
 * There are three elements to the leap second problem :
 * 	i) detection of an upcoming leap
 * 	ii) processing of clock data before and after a leap
 * 	iii) protection of the algo from leap effects  [ done in process_stamp ]
 * TODO: work in progress, finish it in due course!
 */
static void
manage_leapseconds(struct radclock_handle *handle, struct stamp_t *stamp,
	struct radclock_data *rad_data, struct bidir_algooutput *output, int sID)
{
	/* Leap second management */
	static int postleap_freeze = 0;		// inhibit leap actions after a leap
	static int leap_warningcount = 0;	// >=0, accumulate count of LI warnings
	static int leap_imminent = 0;			// flag detection of an impending leap
	static long double tleap = 0;			// the UTC second of the expected leap

	long double radtime;
	vcounter_t now;

	
/* Hacks for leapsecond testing */
/*	if (output->n_stamps == 10) {
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
			switch (stamp->LI) {
				case LEAP_ADDSECOND:
					output->leapsec_next = 1;	leap_warningcount++;	break;
				case LEAP_DELSECOND:
					output->leapsec_next = -1;	leap_warningcount++;	break;
				default:		if (leap_warningcount>0) leap_warningcount--;
			}
			/* Conclude imminent if critical mass of warnings present in final hours */
			/* Once triggered, imminent state remains until cleared by leap */
			if (leap_warningcount > 10 && leap_imminent != 1) {
				verbose(VERB_CONTROL, "** Leapsec ctrl: critical mass of LI warnings reached on server %d", sID);
				//tleap = (vcounter_t) lastsecofmonth + leapsec_next;	// get this somehow, assume ignores leap!
				tleap = 1; // placeholder for compilation only
				if (tleap - stamp->st.bstamp.Tf < 2*3600)
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
		
		if	( stamp->st.bstamp.Te >= tleap || ( output->leapsec_expected > 0
					&& now >= output->leapsec_expected ) ) { // radclock must leap
			/* Reset radclock leap parameters */
			output->leapsec_total += output->leapsec_next;
			output->leapsec_expected = 0;    // signal no leap is imminent
			output->leapsec_next = 0;
			verbose(LOG_ALERT, "** Leapsec ctrl: RADclock %d just leapt by %d [s],"
				" leapsec_total now %d [s]", sID,
				output->leapsec_next, output->leapsec_total);
			/* Reset leap management parameters */
			postleap_freeze = 1000;
			leap_warningcount = 0;
			leap_imminent = 0;
		} else {							// leap still ahead, update preparations
			read_RADabs_UTC(rad_data, &(stamp->st.bstamp.Tf), &radtime, 1);
			output->leapsec_expected = stamp->st.bstamp.Tf +
					(vcounter_t) ((tleap - radtime)/rad_data->phat_local);
			verbose(LOG_NOTICE, "** Leapsec ctrl: jump imminent for RADclock %d, "
				"leap of %d expected in %4.2Lf [sec] (leapsec_expected = %llu)", sID,
				output->leapsec_next, tleap - radtime, output->leapsec_expected);
			//verbose(LOG_NOTICE, "** Leapsec ctrl: jump imminent, leap of %d expected"
			// "in %4.2Lf [sec] (%4.2Lf  %4.2Lf  counter = %llu)",
			//  output->leapsec_next, tleap - radtime, tleap, radtime, output->leapsec_expected);
		}
	}
}


/* Monitor change in server by comparing against the last stamp from it.
 * TODO: add timingloop test: if NTP_SERV running, test if server's refid
 *       not the daemon's own server IP
 * TODO: add n_stamps output  (or stamp_i?) so if running quiet, still know when events occur
 */
static void
flag_serverchange(struct stamp_t *laststamp, struct stamp_t *stamp, int *qual_warning)
{
	unsigned char *c;    // essential this be unsigned !
	unsigned char refid [16];

	if ((laststamp->ttl != stamp->ttl) || (laststamp->LI != stamp->LI) ||
	    (laststamp->refid != stamp->refid) || (laststamp->stratum != stamp->stratum)) {

		verbose(LOG_WARNING, "Change in server info compared to previous stamp");

		/* Signal to algo something dodgy, without overriding existing signal */
		// TODO: consider a separate path_warning to isolate the TTL case, can be picked up by
		//       level shift sub-algos in particular, and rename qual_warning --> server_warning for the remainder
		if (*qual_warning == 0) *qual_warning = 1;

		/* Print out refid reliably [bit of a monster] */
		c = (unsigned char *) &(laststamp->refid);
		if (stamp->stratum <= STRATUM_REFPRIM) // a string if S0 (KissCode) or S1
			snprintf(refid, 16, ".%c%c%c%c" ".", *(c+3), *(c+2), *(c+1), *(c+0));  // EOS+ 6 = 7 chars
		else
			snprintf(refid, 16, "%u.%u.%u.%u", *(c+3), *(c+2), *(c+1), *(c+0)); // EOS+ 4*3+3 = 16

		verbose(LOG_WARNING, " OLD:: IP: %s  STRATUM: %d  LI: %u  RefID: %s  TTL: %d",
		    laststamp->server_ipaddr, laststamp->stratum, laststamp->LI, refid, laststamp->ttl);
		    //inet_ntoa(SNTP_CLIENT(handle,sID)->s_from.sin_addr),

		c = (unsigned char *) &(stamp->refid);
		if (stamp->stratum <= STRATUM_REFPRIM) // a string if S0 (KissCode) or S1
			snprintf(refid, 16, ".%c%c%c%c" ".", *(c+3), *(c+2), *(c+1), *(c+0));  // EOS+ 6 = 7 chars
		else
			snprintf(refid, 16, "%u.%u.%u.%u", *(c+3), *(c+2), *(c+1), *(c+0)); // EOS+ 4*3+3 = 16
			
		verbose(LOG_WARNING, " NEW:: IP: %s  STRATUM: %d  LI: %u  RefID: %s  TTL: %d",
		    stamp->server_ipaddr, stamp->stratum, stamp->LI, refid, stamp->ttl);

//		/* Verbosity for refid sanity checking */
//		// Hack:  Check this format in all cases
//		snprintf(refid, 16, "%c%c%c%c", *(c+3), *(c+2), *(c+1), *(c+0));
//		verbose(LOG_WARNING, " Stratum-1 style RefID: %s", refid);
//		//verbose(LOG_WARNING, "refid manually: IP: %d.%d.%d.%d  .%d",*(c+3), *(c+2), *(c+1), *(c+0), *(c-1));
//		verbose(LOG_WARNING, "refid manually: %X", stamp->refid );
//		verbose(LOG_WARNING, "refid manually: %X.%X.%X.%X", *(c+3), *(c+2), *(c+1), *(c+0));
//		struct in_addr IPrefid;
//		IPrefid.s_addr = htonl(stamp->refid);
//		verbose(LOG_WARNING, "IP style refid the easy way: %s", inet_ntoa(IPrefid) );
	}
	
}




/*
 * Check stamps are not insane. The world is divided in black, white and ...
 * grey. White stamps are clean. Grey stamps have a qual_warning problem, but it
 * is not clear what to do, and that's up to the algo to deal with them. Black
 * stamps are insane and could break processing (e.g. induce zero division, NaN
 * results, etc.). We get rid of them here.
 * Temporal order testing comparing stamp and laststamp assumes they should be
 * ordered. This will be the case if they are from the same server.
 */
static int
insane_bidir_stamp(struct radclock_handle *handle, struct stamp_t *stamp, struct stamp_t *laststamp)
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
	if ((BD_TUPLE(stamp)->Ta == 0) || (BD_TUPLE(stamp)->Tb == 0) ||
		 (BD_TUPLE(stamp)->Te == 0) || (BD_TUPLE(stamp)->Tf == 0)) {
		verbose(LOG_WARNING, "Insane Stamp: bidir stamp with at least one 0 timestamp");
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
	if (BD_TUPLE(stamp)->Ta <= BD_TUPLE(laststamp)->Ta) {
		verbose(LOG_WARNING, "Insane Stamp: Successive NTP requests with"
		    " non-strictly increasing raw timestamps");
		return (1);
	}
	if (BD_TUPLE(stamp)->Ta <= BD_TUPLE(laststamp)->Tf)
		verbose(VERB_DEBUG, "Suspicious Stamp: Successive stamps overlapping");

	/* Raw timestamps break causality */
	if ( BD_TUPLE(stamp)->Tf < BD_TUPLE(stamp)->Ta ) {
		verbose(LOG_WARNING, "Insane Stamp: raw timestamps non-causal");
		return (1);
	}

	/* Server timestamps break causality */
	if ( BD_TUPLE(stamp)->Te < BD_TUPLE(stamp)->Tb ) {
		verbose(LOG_WARNING, "Insane Stamp: server timestamps non-causal");
		return (1);
	}
	/* Server timestamps equal: survivable, but a sign something strange */
	if ( BD_TUPLE(stamp)->Te == BD_TUPLE(stamp)->Tf )
		verbose(LOG_DEBUG, "Suspicious Stamp: server timestamps identical");
		
	/* RTC reset event, and server timestamps seem to predate it.
	 * Algo doesn't run on insane stamps, so RTC reset can be caught subsequently
	 * on a sane stamp.
	 */
//	struct ffclock_data cdat;
//	long double resettime;
//	get_kernel_ffclock(handle->clock, &cdat);
//	bintime_to_ld(&resettime, &cdat.update_time);
//	if ( cdat.secs_to_nextupdate == 0 && BD_TUPLE(stamp)->Tb < resettime ) {
//		verbose(LOG_WARNING, "Insane Stamp: seems to predate last RTC reset");
//		return (1);
//	}
	
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
		if ((BD_TUPLE(stamp)->Tf - BD_TUPLE(stamp)->Ta) < 120) {
			verbose(LOG_WARNING, "Insane Stamp: bidir stamp with RTT impossibly "
			    "low (< 120): %"VC_FMT" cycles", BD_TUPLE(stamp)->Tf - BD_TUPLE(stamp)->Ta);
			return (1);
		}
	}

	/* If we pass all sanity checks */
	return (0);
}


/* Returns the server ID given its resolved IP address, or -1 if no match
 * If running dead, IDs are allocated in order of first occurrence of IP address
 *    pcap input:  actual IP addresses are available
 *   ascii input:  fake local addresses were created based on sID column in file
 *                 [hence in general  input-sID ≠ sID ]
 */
int
serverIPtoID(struct radclock_handle *handle, char *server_ipaddr)
{
	int s;
	struct radclock_ntp_client	*client;

	for (s=0; s < handle->nservers; s++) {
		client = &handle->ntp_client[s];
//		verbose(LOG_NOTICE, "  Comparing stamp IP %s against client sID=%d (%s)",
//			server_ipaddr, s, inet_ntoa(client->s_to.sin_addr) );
		
		/* Assign s to server IP addresses in order of first appearance in input */
		if (handle->run_mode == RADCLOCK_SYNC_DEAD) {
//			if (strcmp("", inet_ntoa(client->s_to.sin_addr)) == 0)
			if (client->s_to.sin_addr.s_addr == 0) {  // address is null
				if (inet_aton(server_ipaddr,&client->s_to.sin_addr) == 0)
					verbose(LOG_ERR, "IP address %s failed to translate", server_ipaddr);
				verbose(LOG_NOTICE, "serverID %d assigned to IP address %s", s, server_ipaddr);
			}
		}

		if (strcmp(server_ipaddr, inet_ntoa(client->s_to.sin_addr)) == 0)
			return (s); // found it
	}

	return (-1);
}


/* Return the sID of the radclock deemed to be of the highest consistent quality.
 * The assessment is made upon the receipt of a valid stamp (not insane or
 * flagged in the servertrust status word, thereby processed by the algo) by any
 * server - All servers are then evaluated. If no such stamp arrives this fn will
 * be starved, but in that case there is no new information, all clocks are drifting
 * at the same rate, and the right response is to retain the preferred server anyway.
 *
 * Quality is not measured directly, instead it is assuming to be higher when
 * path conditions are consistency better. This is assessed via a "path penalty"
 * measure, calculated by each clock, which combined an evaluation of RTT
 * baseline (BL) value and variability, with congestion impact on the absolute clock.
 * Since pathpenalty cannot be updated inbetween stamp arrivals, this function
 * tracks the gap since the last update and uses it to inflate state->pathpenalty.
 * This ensures in particular that during extended starvation of the
 * preferred clock, it will eventually drift out of contention and be replaced.
 *
 * The result of the quality assessment is subject to two anti-churn measures.
 * A recommended switching of the preferred server is inhibited when
 *   - it is during a "churnscale" interval from the previous one
 *   - the improvement falls under a percentage threshold (currently 80%)
 * Together these prevent recomendation churn when servers have similar qualities,
 * and provide robustness to inadequacies in the pathpenalty evaluation.
 * Thus the final recommendation is not dependent on a high accuracy of pathpenalty.
 *
 * << Stamp gap notes >>
 * Gaps commence immediately after the last stamp, no poll period subtracted.
 * Differs from starvation code in that "starvation"
 *  - only operates when running live
 *  - crucially, it operates even if no stamp received from Any server
 *  - is for verbosity and status flag only, not used by the algo
 *  - only kicks in above a threshold, igoring small gaps
 * Whereas stamp gaps are
 *  - calculated whether running live or dead
 *  - needed for pref server selection so that the pathpenalty metric can take
 *    into account gap growth inbetween valid stamps invisible to the algo,
 *    and are reset once a stamp does arrive.
 *  - updated only when some stamp is received that makes it beyond the algo,
 *    hence if only one server (starved or not), its gap never rises above 0,
 *    but in that case pref selection is not required, and in the starvation
 *    case, there is no standard means to report the growing pathpenalty either.
 */
static int
preferred_RADclock(struct radclock_handle *handle, vcounter_t now)
{
	int trusted;
	double pathpenalty, pp_min, pp_curr;  // path metric [s] to minimize
	int s_min = -1;              // serverID of pp_min
	double driftpertick;         // convenience
	int s;
	struct bidir_algostate *state;
	struct bidir_algooutput *output;

	/* Antichurn Filtering (set churn_scale to path_scale) */
	if (RAD_DATA(handle)->phat * (now - handle->pref_date) < handle->conf->metaparam.path_scale)
		return (handle->pref_sID);  // blocked, even pathpenalty update pointless

	/* Precalculate worst case drift per counter-period, using preferred server */
	driftpertick = RAD_DATA(handle)->phat * handle->conf->metaparam.RateErrBOUND;

	/* Find the server with the lowest penalty.
	 * Untrusted candidates are excluded, but the current pref server is always
	 * considered, thus the loop always returns a recommendation. If the pref
	 * server is consistently untrusted then its gap will grow (untrusted stamps
	 * are rejected), and ultimately be replaced.  */
	for (s=0; s < handle->nservers; s++) {
		state  = &((struct bidir_algodata *)handle->algodata)->state[s];
		if (state->stamp_i == -1) continue;    // no stamp for this server yet

		output = &((struct bidir_algodata *)handle->algodata)->output[s];
		trusted = ! (handle->servertrust & (1ULL << s));

		/* Update the accumulated gap since the last stamp */
		state->rawstampgap = now - handle->rad_data[s].last_changed;  // will be 0 for sID

		/* Inflate algo-based penalty with drift bound over stampgap */
		pathpenalty = output->pathpenalty + state->rawstampgap * driftpertick;

		if (s == handle->pref_sID)
			pp_curr = pathpenalty;  // will need later

		/* Find best between the current preferred, and trusted alternatives */
		if ( s == handle->pref_sID || trusted ) {
			if (s_min < 0) {  // first server seen, initialize to it
				pp_min = pathpenalty;
				s_min = s;
			} else
				if (pathpenalty < pp_min) {
					pp_min = pathpenalty;
					s_min = s;
				}
		} else
			verbose(LOG_WARNING, "server %d not trusted, excluded from pref-server selection", s);
	}

	/* Safely blanket: should be impossible unless servers untrusted immediately */
	if (s_min < 0) {
		verbose(LOG_ERR, "No server passed checks, preferred server unchanged");
		return (handle->pref_sID);
	}

	/* Test if a new best server is sufficiently better to switch */
	if (s_min != handle->pref_sID) {
		if (pp_min < 0.8 * pp_curr) {  // apply improvement ratio "antichurn_fac"
			state = &((struct bidir_algodata *)handle->algodata)->state[s_min];
			verbose(LOG_NOTICE, "New preferred clock %d has minRTT %3.1lfms at stamp %d"
			    " (UTC %13.1Lf), with pathpenalty %3.1lfms being %3.1lf%% of old value %3.1lf",
			    s_min, 1000*state->RTThat*state->phat, state->stamp_i, state->stamp.Tb,
			    1000*pp_min, 100*pp_min/pp_curr, 1000*pp_curr);
			handle->pref_date = now;
		} else {
			trusted = ! (handle->servertrust & (1ULL << s_min));
			if (!trusted)
				verbose(LOG_NOTICE, "Warning, preferred clock %d no longer the"
				    " best. It is retained, but not trusted", s_min);
			return (handle->pref_sID);
		}
	}

	return (s_min);
}



/*
 * This function is the core of the RADclock daemon.
 * It checks to see if any of the maintained RADclocks (one per server) is being
 * starved of data.  It then looks for a new stamp. If one is available, it:
 *    assesses it
 *    determines the server it came from (which RADclock it will feed)
 *       - manages the leapsecond issues
 *       - feeds a vetted and leapsecond-safe RAD-stamp to the algo
 *       - updates the central handle->rad_data containing this clock's params and state
 *    Once the new stamp is processed, the preferred clock decision is updated
 *    If running live:
 *       - the parameters of the preferred clock are sent to the relevant
 *         IPC consumers (FF and/or FB kernel clocks, SMS)
 *       - keeps summaries of the new stamp and server state
 *       - if the daemon is an NTC OCN node, outputs a critical summary into a telemetry feed
 */
int
process_stamp(struct radclock_handle *handle)
{
	/* Bi-directional stamp passed to the algo for processing */
	struct stamp_t stamp;
	struct ffclock_data cdat;
	int qual_warning = 0;
	struct bidir_stamp bdstamp_noleap;

	/* Multiple server management */
	int sID;    // server ID of new stamp popped here
	int s;
	struct radclock_data *rad_data;
	struct radclock_error *rad_error;
	struct bidir_algodata *algodata;
	struct stamp_t *laststamp;    // access last stamp (with original Te,Tf)
	struct bidir_algooutput *output;
	struct bidir_algostate *state;
	int pref_updated;
	int pref_sID_new;

	/* Error control logging */
	long double currtime = 0;
	double timediff = 0;
	int err, err_read;

	/* Starvation management */
	vcounter_t now;
	int trusted;

	/* Check hardware counter has not changed */
	// XXX TODO this is freebsd specific, should be put with arch specific code
#ifdef WITH_FFKERNEL_FBSD
	char hw_counter[32];
	size_t size_ctl;
#endif

	JDEBUG

	/* Starvation test: have valid stamps stopped arriving to the algo?
	 * Test is applied to all clocks, even that of the current stamp (if any).
	 * Definition based on algo input only, as cannot be evaluated by the algo.
	 * Hence is a function of the duration of the missing stamp gap, measured in
	 * poll period units. Stamps can be missing, or invalid, or any reason.
	 * Test should run once per trigger-grid point. Due to structure of TRIGGER--
	 * PROC interactions, process_stamp runs each time, so this is true.
	 *
	 * Note: technically starvation occurs whenever any stamp is unavailable, but
	 * STARAD_STARVING only flags it past a given threshold for verbosity purposes.
	 */
	algodata = (struct bidir_algodata*)handle->algodata;
	if (handle->run_mode == RADCLOCK_SYNC_LIVE) {
		err_read = radclock_get_vcounter(handle->clock, &now);
		if (err_read < 0)
			return (-1);

		for (s=0; s < handle->nservers; s++) {
			rad_data = &handle->rad_data[s];
			state = &algodata->state[s];
			if ((now - rad_data->last_changed) * rad_data->phat > 10*state->poll_period) {
				if (!HAS_STATUS(rad_data, STARAD_STARVING)) {
					verbose(LOG_WARNING, "Clock %d is starving. Gap has exceeded 10 stamps", s);
					ADD_STATUS(rad_data, STARAD_STARVING);
				// TODO: alter minRTT metric here: add in appropriate initial ∆drift for 10stamps (make a param)
				}
				// TODO: alter minRTT metric here: add in appropriate ∆drift
			}
		}

		/* Warn/terminate if detect a RTC reset, as currently can't handle this.
		 * Hard core detection based on FFdata being wiped, like at boot time.
		 * Soft detection based on the resetting of secs_to_nextupdate by FFclock RTC processing code,
		 * which only works if the daemon's preferred clock has first set it the first time itself.
		 * Hence a reset is missed if it occurs before this.
		 * TODO: currently not OS-dependent neutral. Need to define these signals to daemon universally
		 */
		if ( get_kernel_ffclock(handle->clock, &cdat) == 0) {
			if ((cdat.update_time.sec == 0) || (cdat.period == 0)) {
				verbose(LOG_WARNING, "FFdata has been hardcore re-initialized! due to RTC reset?");
				printout_FFdata(&cdat);
				//return (-1);
			} else {
				if (cdat.secs_to_nextupdate == 0 && !HAS_STATUS(RAD_DATA(handle), STARAD_UNSYNC)) {
					state = &algodata->state[handle->pref_sID];
					if ( VERB_LEVEL>2 ) {
						verbose(LOG_WARNING, "RADclock noticed a FFdata reset after stamp %d, "
						    "may require a restart I'm afraid", state->stamp_i);
						printout_FFdata(&cdat);
					}
					//return -1;
				}
			}
		}
	}

	/* Generic call for creating stamps depending on the type of input source */
	// Need to differentiate ascii input from pcap input
	err = get_next_stamp(handle, (struct stampsource *)handle->stamp_source, &stamp);
	if (err == -1)
		return (-1);

	/* No error, but no stamp to process */
	if (err == 1) {
		push_telemetry(handle, -1);  // Check for timeout based telemetry trigger
		return (1);
	}

	/* Buffer the RADstamp to supply perfstamp matching in the SHM thread.
	 * Performed immediately here to ensure perfstamp matching has access to maximal
	 * stamp_t level history, even if returning IP address not recognised by
	 * serverIPtoID, or if RADstamp is deemed insane and never passed to algo.
	 * Consequence: perfstamp ascii output may be longer than the algo-oriented
	 * sync and clock ascii outputs. Can also be shorter due to perfstamp match failure.
	// Prob: corresponding PERF stamp tuples, if exported as 6col, will no longer
	// match mat.out lines.  Soln: full 6col can be replayed with SHM off, and
	// both mat.out and matching PERF sync outputted.
	 */
	char *poptype;
	if (handle->conf->server_shm == BOOL_ON)    // implies is_cn, hence perfdata non-NULL
	{
		struct bidir_perfdata *perfdata = handle->perfdata;
		int writehead;  // maps next write index into buffer position to write

		/* Convert the popped algo stamp into a `client side' RAD perf halfstamp */
		stamp.type = STAMP_NTP_PERF;
		stamp.st.pstamp.bstamp = stamp.st.bstamp;
		stamp.st.pstamp.Tout = 0;
		stamp.st.pstamp.Tin = 0;

		/* Corrupt RAD halfstamp inputs for testing */
		//if ((1+output->n_stamps) % 3 == 0) stamp.id -=1;    // corrupt some ids
		//  corrupted server IP on RAD side will break halfRADstamp cleaning:  must trust IPs
		//if (output->n_stamps % 4 == 0)  stamp.server_ipaddr[4] = "8";  // corrupt some IPs

		/* Insert into the RAD halfstamp storage buffer
		 * This production side has priority and no access protection is required.
		 * Data consistency instead occurs on the consumer side in the SHM thread */
		writehead = perfdata->RADbuff_next % perfdata->RADBUFF_SIZE;
		memcpy(&perfdata->RADbuff[writehead], &stamp, sizeof(struct stamp_t));
		perfdata->RADbuff_next ++;

		/* Insert into the perf stamp matching queue */
//		if (1)  // ((1+output->n_stamps) % 3 != 0) {  // randomly miss some insertions
//			verbose(LOG_DEBUG, " >>> trying to insert a RAD halfstamp");
//			pthread_mutex_lock(&handle->matchqueue_mutex);
//			err = insertandmatch_halfstamp(perfdata->q, &stamp, MODE_RAD);
//			pthread_mutex_unlock(&handle->matchqueue_mutex);
//			if (err == 0)
//				verbose(LOG_DEBUG, " >>> ...  yielded a fullstamp");
//			else
//				verbose(LOG_DEBUG, " >>> ... no fullstamp");
//		}
		poptype = "a RAD ";    // RAD half poppped here, full PERF assembled in SHM
	} else
		poptype = "a PERF";    // full perf stamp already available as read from input


	/* If a recognized stamp is returned, record the server it came from */
	sID = serverIPtoID(handle, stamp.server_ipaddr);

	struct bidir_stamp *bd_tuple = BD_TUPLE(&stamp);
	//	BD_TUPLE(bd_tuple,&stamp);  // obtain the 4tuple
	if (stamp.type == STAMP_NTP_PERF)
		verbose(VERB_DEBUG, "Popped %s stamp from server %d: [%llu]  "
		    "%"VC_FMT" %"VC_FMT" %.6Lf %.6Lf    %.6Lf %.6Lf", poptype, sID,
		    (long long unsigned) stamp.id,
		    (long long unsigned) bd_tuple->Ta,
		    (long long unsigned) bd_tuple->Tf,
		    bd_tuple->Tb, bd_tuple->Te,
		    PST(&stamp)->Tin, PST(&stamp)->Tout);
	else
		verbose(VERB_DEBUG, "Popped a stamp from server %d: [%llu]  "
		    "%"VC_FMT" %"VC_FMT" %.6Lf %.6Lf", sID,
		    (long long unsigned) stamp.id,
		    (long long unsigned) bd_tuple->Ta,
		    (long long unsigned) bd_tuple->Tf,
		    bd_tuple->Tb, bd_tuple->Te);

	if (sID < 0) {
		verbose(LOG_WARNING, "Unrecognized stamp popped, skipping it");
		return (1);
	}
	handle->last_sID = sID;    // needed for fake DAG msg code in SHM thread only

	/* Authentication check for a CN using an OCN server */
	if (handle->conf->is_cn) {
		int OCN_id = OCN_ID(handle->conf->time_server_ntc_mapping[sID]);
		if (OCN_id != -1 && stamp.auth_key_id != OCN_id + PRIVATE_CN_NTP_KEYS) {
			verbose(LOG_ERR, "CN skipping received OCN stamp [%d] with incorrect auth_key!", sID);
			return (1);
		}
	}

	/* Set pointers/values to data for this server */
	rad_data  = &handle->rad_data[sID];    // = SRAD_DATA(handle,sID);
	rad_error = &handle->rad_error[sID];
	laststamp = &algodata->laststamp[sID];
	output    = &algodata->output[sID];
	state     = &algodata->state[sID];
	trusted = ! (handle->servertrust & (1ULL << s));

	/* If the stamp fails basic tests we won't endanger the algo with it, just exit
	 * Lower level tests already performed in bad_packet_server() which
	 * could block stamp from ever reaching here.
	 * If there is something potentially challenging but not dangerous, flag it.
	 * TODO: consider if some (eg LI) checks in bad_packet_server should be
	 *       centralized here instead, or seen by leap code.
	 * TODO: check if insane_bidir_stamp wont kick a stamp out after a leap, or leap screw ups
	 */
	if (output->n_stamps > 0) {
		/* Flag a change in this server's advertised NTP characteristics */
		flag_serverchange(laststamp, &stamp, &qual_warning);
		/* Check fundamental properties are satisfied */
		if (insane_bidir_stamp(handle, &stamp, laststamp) || !trusted) {
			memcpy(laststamp, &stamp, sizeof(struct stamp_t));  // always record
			return (0);    // a starved clock will remain starved
		}
	}

	/* Valid stamp obtained: record and flag it, then continue to the algo */
	//	output->n_stamps++;  // now set in RADalgo_bidir
	memcpy(laststamp, &stamp, sizeof(struct stamp_t));
	if (HAS_STATUS(rad_data, STARAD_STARVING)) {
		verbose(LOG_NOTICE, "Clock %d no longer starving", sID);
		DEL_STATUS(rad_data, STARAD_STARVING);
	}

	/* Manage the leapsecond state variables held in output */
	manage_leapseconds(handle, &stamp, rad_data, output, sID);

	/* Run the RADclock algorithm to update radclock parameters with new stamp.
	 * Pass it server timestamps which have had leapseconds removed, so algo sees
	 * a continuous timescale and is leap second oblivious.
	 * After updating, also update leap second parameters, and incorporate all
	 * into rad_data ready for publication to the IPC and kernel.
	 */

	/* Update radclock parameters using leap-free stamp */
	bdstamp_noleap = stamp.st.bstamp;
	bdstamp_noleap.Tb += output->leapsec_total;    // adding means removing
	bdstamp_noleap.Te += output->leapsec_total;
	//get_kernel_ffclock(handle->clock, &cdat);    // check for RTC reset
	// TODO: need to make  RTCreset = secs_to_nextupdate==0 && not yet pushed to kernel
	//  easy and definitive way to to record a stamp_firstpush = stamp_i  in peer
	RADalgo_bidir(handle, state, &bdstamp_noleap, qual_warning, rad_data, rad_error, output);
//    cdat.secs_to_nextupdate == 0 && stamp_i > stamp_firstpush);

	/* Update RADclock data with new algo outputs, and leap second update
	 * The rad_data->status bits are jointly owned by the algo, and this function,
	 * and so are not in algo state.  They are updated individually in-situ.
	 */
	pthread_mutex_lock(&handle->globaldata_mutex);	// ensure consistent reads
	rad_data->phat					= state->phat;
	rad_data->phat_err			= state->perr;
	rad_data->phat_local			= state->plocal;
	rad_data->phat_local_err	= state->plocalerr;
	rad_data->ca					= state->K - (long double)state->thetahat;
	rad_data->ca_err				= state->algo_err.error_bound;
	rad_data->last_changed		= stamp.st.bstamp.Tf;
	// Previously used (pp-1.5) to allow for NTP's varying pp in piggy mode
	// Warning: can be far in future on 1st stamp with poor phat
	rad_data->next_expected		= stamp.st.bstamp.Tf +
	    (vcounter_t) ((double)state->poll_period / state->phat);
	rad_data->leapsec_total		= output->leapsec_total;
	rad_data->leapsec_next		= output->leapsec_next;
	rad_data->leapsec_expected = output->leapsec_expected;
	/* Update all members of rad_error */
	rad_error->error_bound 		= rad_data->ca_err;	// same by definition
	if (state->algo_err.nerror > 0) {
		rad_error->error_bound_avg = state->algo_err.cumsum / state->algo_err.nerror;
		if (state->algo_err.nerror > 1) {
			rad_error->error_bound_std = sqrt((state->algo_err.sq_cumsum -
			  (state->algo_err.cumsum * state->algo_err.cumsum / state->algo_err.nerror))
			  / (state->algo_err.nerror - 1) );
		}
	}
	rad_error->min_RTT = state->RTThat * state->phat;
	pthread_mutex_unlock(&handle->globaldata_mutex);
	
	/* Record typical NTP parameter values for this server */
	if ((stamp.stratum > STRATUM_REFCLOCK) && (stamp.stratum < STRATUM_UNSPEC)
			&& (stamp.LI != LEAP_NOTINSYNC)) {
		handle->ntp_server[sID].stratum        = stamp.stratum;
		handle->ntp_server[sID].rootdelay      = stamp.rootdelay;
		handle->ntp_server[sID].rootdispersion = stamp.rootdispersion;
		verbose(VERB_DEBUG, "Received pkt stratum= %u, rootdelay= %.9f, rootdispersion= %.9f",
		    stamp.stratum, stamp.rootdelay, stamp.rootdispersion);
	}
	handle->ntp_server[sID].refid  = stamp.refid;  // typical not defined, get each time
	handle->ntp_server[sID].minRTT = rad_error->min_RTT;


	if ( VERB_LEVEL>2 ) {
		verbose(VERB_DEBUG, "process_stamp: rad_data updated:");
		printout_raddata(rad_data);
	}



	/* Processing complete on the new stamp wrt the corresponding RADclock.
	 * Reevaluate preferred RADclock
	 * For the new preferred clock, an "update" is noted if this stamp is
	 * from it, or if pref_sID has just changed regardless of stamp identity.
	 */
	pref_updated = 1;
	if (handle->nservers > 1) {
		pref_sID_new = preferred_RADclock(handle, stamp.st.bstamp.Tf);
		if (pref_sID_new != handle->pref_sID) {
			verbose(LOG_NOTICE, "Preferred clock changed from %d to %d",
			     handle->pref_sID, pref_sID_new);

			// Apply asym management for continuity of absolute pref clock
			// TODO: add asym parameter to state and incorporate into all Ca calls, in Algo and elsewhere.
//			if (asym_manage_enable)  // create a handle->conf.asym_manage_enable member
//				pref_asym_manage(handle, handle->pref_sID, pref_sID_new);
			handle->pref_sID = pref_sID_new;    // register change
		} else
			if (sID != handle->pref_sID) pref_updated = 0;
	}


	/*
	 * RADdata copy actions, only relevant when running Live
	 *		- IPC shared memory update
	 *		- FFclock kernel parameters update
	 *		- VM store update
	 *		- FBclock kernel parameters update
	 *
	 * The preferred clock only is used here, and only if an update for it noted.
	 */
	if (handle->run_mode == RADCLOCK_SYNC_LIVE && pref_updated) {

		/* Update IPC shared memory segment for used by libprocesses */
		if (handle->conf->server_ipc == BOOL_ON) {
			if (!HAS_STATUS(RAD_DATA(handle), STARAD_UNSYNC))
				update_ipc_shared_memory(handle);
		}

		/* Update FFclock parameters, provided RADclock is synchronized */
		if (!HAS_STATUS(RAD_DATA(handle), STARAD_UNSYNC)) {
			if (handle->clock->kernel_version < 2) {
				update_kernel_fixed(handle);
				verbose(VERB_DEBUG, "Sync pthread updated kernel fixed pt data.");
			} else {
				// TODO: great many things to do here to perform a clean shutdown or reset...
				if ( get_currentcounter(handle->clock) == 1 ) {
					verbose(LOG_NOTICE, "Hardware counter has changed, shutting down RADclock");
					return (-1);
				}

				/* Examine FF form of new updated rad_data */
				fill_ffclock_data(RAD_DATA(handle), RAD_ERROR(handle), &cdat);
				if ( VERB_LEVEL>2 ) {
					verbose(VERB_DEBUG, "updated raddata in FFdata form is :");
					printout_FFdata(&cdat);
				}

				if (handle->conf->adjust_FFclock == BOOL_ON) {
					if (!set_kernel_ffclock(handle->clock, &cdat))
						//stamp_firstpush = stamp_i;
						verbose(VERB_DEBUG, "FF kernel data has been updated.");
				}

				/* Check FFclock data in the kernel now */
				if ( VERB_LEVEL>2 ) {
					verbose(VERB_DEBUG, "Kernel FFdata is now :");
					get_kernel_ffclock(handle->clock, &cdat);
					printout_FFdata(&cdat);
				}

				/* Check accuracy of RAD->FF->RAD inversion for Ca read */
				long double ca_compare, Ca_compare, CaFF;
				struct radclock_data inverted_raddata;
				if ( VERB_LEVEL>3 ) {
				 	ca_compare = RAD_DATA(handle)->ca;
				 	read_RADabs_UTC(RAD_DATA(handle), &(RAD_DATA(handle)->last_changed), &Ca_compare, 0);
					verbose(LOG_NOTICE, "RADdata from daemon");
					printout_raddata(RAD_DATA(handle));

					get_kernel_ffclock(handle->clock, &cdat);
					fill_radclock_data(&cdat, &inverted_raddata);
					verbose(LOG_NOTICE, "RADdata inverted from matching FFdata");
					printout_raddata(&inverted_raddata);

					ca_compare -= inverted_raddata.ca;
					read_RADabs_UTC(&inverted_raddata, &inverted_raddata.last_changed, &CaFF, 0);
					Ca_compare -= CaFF;
					verbose(LOG_NOTICE, " orig - inverted:   ca: %5.2Lf [ns],  Ca: %5.4Lf [ns]",
					    ca_compare*1e9, Ca_compare*1e9 );
				}

			}

			/* Adjust system FBclock if requested (and if not piggybacking on ntpd) */
			if (handle->conf->adjust_FBclock == BOOL_ON) { // TODO: catch errors
				update_FBclock(handle);
				verbose(VERB_DEBUG, "Kernel FBclock has been set.");
			}

		}  // if !STARAD_UNSYNC

		/* Update any virtual machine store if configured */
		if (VM_MASTER(handle)) {
			err = push_data_vm(handle);
			if (err < 0) {
				verbose(LOG_WARNING, "Error attempting to push VM data");
				return (1);
			}
		}

	}  // RADCLOCK_SYNC_LIVE actions


	/* Send telemetry data through ring buffer and eventually to the NTC's TICK server */
	push_telemetry(handle, sID); // Check if telemetry message needs to be sent

	/* Write ascii output line for this stamp to files if open */
	print_out_clockline(handle, &stamp, output, sID);
	if (handle->conf->server_shm == BOOL_OFF)  // otherwise called within SHM
		print_out_syncline(handle, &stamp, sID);




	/* View updated RADclock data and compare with NTP server stamps in nice
	 * format. The first 10 then every 6 hours (poll_period can change, but
	 * should be fine with a long term average, do not have to be very precise anyway).
	 */
	if (VERB_LEVEL && (output->n_stamps < 10) ||
	    !(output->n_stamps % ((int)(3600*6/state->poll_period))) )
	{
		read_RADabs_UTC(rad_data, &(rad_data->last_changed), &currtime, PLOCAL_ACTIVE);
		timediff = (double) (currtime - (long double) bd_tuple->Te);

		verbose(VERB_CONTROL, "i=%ld: (sID=%d) Response timestamp %.6Lf, "
				"RAD - NTPserver = %.3f [ms], RTT/2 = %.3f [ms]",
				output->n_stamps - 1, sID,
				bd_tuple->Te, 1000 * timediff, 1000 * rad_error->min_RTT / 2 );

		verbose(VERB_CONTROL, "i=%ld: Clock Error Bound (cur,avg,std) %.6f "
				"%.6f %.6f [ms]", output->n_stamps - 1,
				1000 * rad_error->error_bound,
				1000 * rad_error->error_bound_avg,
				1000 * rad_error->error_bound_std);
	}




//	/* TODO: remove this fairly soon. Fake DAG stamp creation, insertion into perfstamp matching queue, popping */
//	if (0)  // (handle->conf->server_shm == BOOL_ON)
//	{
//		/* Obtain the last RADstamp */
//		struct stamp_t RADstamp;
//		//RADstamp = ((struct bidir_algodata*)handle->algodata)->laststamp[handle->last_sID];
//		RADstamp = stamp;
//		struct radclock_error *rad_error;
//		rad_error = &handle->rad_error[handle->last_sID];
//
//		/* Construct fake DAG message to match RADstamp */
//		struct dag_cap dag_msg;
//		char server_ipaddr[INET6_ADDRSTRLEN];
//		/* id field uint64_t --> l_fp conversion */
//		dag_msg.server_reply_org.l_int = htonl(RADstamp.id >> 32);
//		dag_msg.server_reply_org.l_fra = htonl((RADstamp.id << 32) >> 32);
//		inet_aton(RADstamp.server_ipaddr, &dag_msg.ip);
//		dag_msg.Tout = RADstamp.st.bstamp.Tb - rad_error->min_RTT/2 + 0.3e-3;
//		dag_msg.Tin  = RADstamp.st.bstamp.Te + rad_error->min_RTT/2 - 0.3e-3;
//
//		/* Push standard DAG message into a perf stamp */
//		struct stamp_t DAGstamp = {.type = STAMP_NTP_PERF}; // other members will be zero
//		/* id field l_fp --> uint64_t conversion */
//		DAGstamp.id = ((uint64_t) ntohl(dag_msg.server_reply_org.l_int)) << 32;
//		DAGstamp.id |= (uint64_t) ntohl(dag_msg.server_reply_org.l_fra);
//		//DAGstamp.id = RADstamp.id;		// direct fake
//		strcpy(DAGstamp.server_ipaddr, inet_ntoa(dag_msg.ip));
//		DAGstamp.st.pstamp.Tout = dag_msg.Tout;
//		DAGstamp.st.pstamp.Tin  = dag_msg.Tin;
//
//		/* Corrupt DAG halfstamp inputs for testing */
////		if (output->n_stamps % 3 == 0)	DAGstamp.id +=1;  					// corrupt some ids
////		if (output->n_stamps % 4 == 0)	DAGstamp.server_ipaddr[0] = "9";	// corrupt some IPs
//
//		/* Insert DAG perf stamp into the perf stamp matching queue */
//		verbose(LOG_DEBUG, " >>> trying to insert a DAG halfstamp");
//	//pthread_mutex_lock(&handle->matchqueue_mutex);
//		err = insertandmatch_halfstamp(((struct bidir_perfdata*)handle->perfdata)->q, &DAGstamp, MODE_DAG);
//		if (err == 0)
//			verbose(LOG_DEBUG, " >>> ...  yielded a fullstamp");
//		else
//			verbose(LOG_DEBUG, " >>> ... no fullstamp");
//
//		/* Extract full stamp */
//		verbose(LOG_DEBUG, " >>> trying to pop perf fullstamp");
//		err = get_fullstamp_from_queue_andclean(((struct bidir_perfdata*)handle->perfdata)->q, &RADstamp);
//	//pthread_mutex_unlock(&handle->matchqueue_mutex);
//		if (err == 0)
//			verbose(LOG_NOTICE, " >>> Popped");
//		else
//			verbose(LOG_DEBUG, " >>> ... no fullstamp");
//
//		/* If a recognized stamp is returned, record the server it came from */
//		sID = serverIPtoID(handle, RADstamp.server_ipaddr);
//		if (sID < 0) {
//			verbose(LOG_WARNING, " >>> Unrecognized perf stamp popped, skipping it");
//			return (1);
//		} else
//			verbose(VERB_DEBUG, " >>> Popped a PERF stamp from server %d: [%llu]  %llu %llu %.6Lf %.6Lf ",
//			sID, (long long unsigned) RADstamp.id,
//			(long long unsigned) PSTB(&RADstamp)->Ta, (long long unsigned) PSTB(&RADstamp)->Tf,
//			PSTB(&RADstamp)->Tb, PSTB(&RADstamp)->Te);
//	}
//



	/* Set initial state of 'signals' - important !!
	 * Has to be placed here, after the algo handled the possible new
	 * parameters, with the next packets coming.
	 */
	handle->conf->mask = UPDMASK_NOUPD;

	JDEBUG_RUSAGE
	return (0);
}

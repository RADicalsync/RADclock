/*
 * Copyright (C) 2023 The RADclock Project (see AUTHORS file)
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <net/ethernet.h>

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <fcntl.h>

#include "../config.h"
#include "radclock.h"
#include "radclock-private.h"
#include "radclock_daemon.h"
#include "verbose.h"
#include "sync_history.h"
#include "proto_ntp.h"
#include "sync_algo.h"
#include "pthread_mgr.h"
#include "misc.h"
#include "jdebug.h"
#include "config_mgr.h"
#include "create_stamp.h"
#include "Ext_reference/ext_ref.h"    // API to DAG code


/* Live assembly of PERFstamps */
int
assemble_next_perfstamp(struct radclock_handle *handle, struct stamp_t *PERFstamp)
{
	struct bidir_perfdata *perfdata = handle->perfdata;

	/* UNIX socket related */
	int num_bytes;
	struct sockaddr_in client;
	socklen_t socklen = sizeof(client);

	/* Perf stamp matching related */
	const int N = perfdata->RADBUFF_SIZE;
	index_t next0, r;     // unique PERF buffer indices that never wrap
	struct stamp_t copy;  // local copy of stamp from buffer
	struct stamp_t DAGstamp;
	struct stamp_t *st_r;
	struct dag_cap dag_msg;
	int got_dag_msg;
	int fullerr;


	/* Empty RAD halfstamp buffer into the perf matching queue (ie, take a
	 * batch of RAD arrivals from PROC and put under EXTREF control).
	 * Each halfstamp successfully read is marked as read by zeroing the id.
	 * Reads proceed backward in time until no unread stamps are left, or until
	 * the (potentially advancing) write head position is encountered
	 * [new arrivals during emptying are left for next time].
	 * Checks are made to ensure data consistency, possible due to the unique
	 * uint_64 indices (which don't wrap) of the write and read positions.
	 * The buffer and stamp matching queue accept stamps from any clock/server.
	 */
	next0 = perfdata->RADbuff_next;  // record RADbuffer write posn at start of emptying
	r = next0 - 1;
	while ( next0 > 0 && perfdata->RADbuff_next - r < N ) {
		st_r = &perfdata->RADbuff[r % N];  // pointer to stamp r in buffer
		if ( st_r->id > 0) {  // data present
			memcpy(&copy, st_r, sizeof(struct stamp_t));
			verbose(VERB_DEBUG, " RAD halfstamp found in buffer with id %llu, "
			    "%d back from head at %llu", copy.id, next0-r, next0 );
			/* Check if copy can be trusted */
			if ( perfdata->RADbuff_next - r >= N )  // not safe, stop here
				break;

			/* Mark as read. If this corrupts a stamp write in progress (super
			 * unlikely event!), effect will be to lose all halfstamps <=r, but
			 * the copy of the old value at r%N is still good. */
			st_r->id = 0;
			if ( perfdata->RADbuff_next - r >= N ) {
				verbose(LOG_ERR, "EXTREF: halfstamp corrupted during RADstamp buffer"
				    " emptying, up to %d PERFstamps may have been lost", next0 - r + 1);
				insertandmatch_halfstamp(perfdata->q, &copy, MODE_RAD);
				break;
			}

			verbose(VERB_DEBUG, " .. passed corruption checks, trying to insert "
			    " copy into match queue (%d)", next0-r );
			insertandmatch_halfstamp(perfdata->q, &copy, MODE_RAD);
			r -= 1;
		} else
			break;  // end of unread data younger than next0: emptying done
	}


	/* Make an attempt to obtain the next authoritative Ref(erence) halfstamp.
	 * A timeout is used to reduce processing, to allow the thread kill signal to
	 * be checked for, and to give the OS an opportunity to suspend the thread
	 */
	num_bytes = recvfrom(handle->ref_source, &dag_msg, sizeof(struct dag_cap),
	    0, (struct sockaddr*)&client, &socklen);    // client is a sockaddr_in

	if ( num_bytes == (sizeof(struct dag_cap)) ) {
		got_dag_msg = 1;
		verbose(VERB_DEBUG, "DAG msg received");
	} else {
		got_dag_msg = 0;
		if (num_bytes == -1)
			verbose(VERB_DEBUG, "No DAG message received");
		else
			verbose(VERB_DEBUG, "Incomplete DAG message received (%d bytes)", num_bytes);
	}


	/* Fake DAG message creation for testing
	 * Only create if
	 *  - true DAG msg fails.
	 *  - something new in RADbuffer since last draining (works best with single time_server)
	 * Other hack variables to remove: last_next0
	 */
	static index_t last_next0 = 0;  // fake DAG support, and get_fullstamp_ efficiency
	struct stamp_t RADstamp;
	struct stamp_t *stampPTR;
	struct radclock_error *rad_error;

	if (0)  // ( got_dag_msg == 0 && next0 != last_next0 && handle->last_sID != -1)
	{
		//verbose(VERB_DEBUG, "Enter fake, last_sID = %d", handle->last_sID);
		/* Obtain the last accepted RADstamp using the last_sID hack */
		stampPTR = &((struct bidir_algodata*)handle->algodata)->laststamp[handle->last_sID];
		if (stampPTR) {
			RADstamp = *stampPTR;
			rad_error = &handle->rad_error[handle->last_sID];

			/* Construct fake DAG message to match RADstamp */
			/* id field uint64_t --> l_fp conversion */
			dag_msg.stampid.l_int = htonl(RADstamp.id >> 32);
			dag_msg.stampid.l_fra = htonl((RADstamp.id << 32) >> 32);
			inet_aton(RADstamp.server_ipaddr, &dag_msg.ip);
			dag_msg.Tout = RADstamp.st.bstamp.Tb - rad_error->min_RTT/2 + 0.3e-3;
			dag_msg.Tin  = RADstamp.st.bstamp.Te + rad_error->min_RTT/2 - 0.3e-3;
		}
 		//verbose(VERB_DEBUG, "exit fake");
		got_dag_msg = 1;
	}


	/* Map DAG message into a PERFstamp and insert in matchqueue */
	if ( got_dag_msg == 1 ) {
		memset(&DAGstamp, 0, sizeof(struct stamp_t));
		DAGstamp.type = STAMP_NTP_PERF;
		/* id field l_fp --> uint64_t conversion */
		DAGstamp.id = ((uint64_t) ntohl(dag_msg.stampid.l_int)) << 32;
		DAGstamp.id |= (uint64_t) ntohl(dag_msg.stampid.l_fra);
		strcpy(DAGstamp.server_ipaddr, inet_ntoa(dag_msg.ip));
		DAGstamp.ExtRef.Tout = dag_msg.Tout;
		DAGstamp.ExtRef.Tin  = dag_msg.Tin;

		/* Corrupt DAG halfstamp inputs for testing */
		//if (next0 % 3 == 0)	DAGstamp.id +=1;                  // corrupt some ids
		//if (next0 % 2 == 0)	DAGstamp.server_ipaddr[0] = "9";  // corrupt some IPs
		/* Insert DAG perf halfstamp into the PERFstamp matching queue */
		//verbose(LOG_DEBUG, " ||| trying to insert a DAG halfstamp");
		insertandmatch_halfstamp(perfdata->q, &DAGstamp, MODE_EXT);
	}


	/* Check if a fullstamp is available
	 * Don't bother if there has been no new input since last time.
	 * TODO: modify return codes of matching queue calls to keep track of the
	 *       number of full stamps, so this will not be called if not needed,
	 *       and Will be called if there is a fullstamp backlog, despite no new input */
	if (got_dag_msg || next0 != last_next0) {
		verbose(VERB_DEBUG, "New halfstamps inserted: checking for full PERFstamp");
		fullerr = get_fullstamp_from_queue_andclean(perfdata->q, PERFstamp);
	} else
		fullerr = 1;

	last_next0 = next0;    // fake DAG support, and get_fullstamp_ efficiency

	/* If no stamp available, nothing to process, just look again */
	if (fullerr == 1)
		return 1;
	else
		return 0;

}


/* Process the new EXTREF stamp (contained within the PERFstamp).
 * On entry stamp_i is already updated to this stamp.
 * Fake SA generation
 *   {SYD,MEL}:  event based to orchestrate 2-level changes in {SYD,BRI}_OCN
 *   others:  simple periodic approach with some randomness for SA–run
 *            durations for variety
 */
void
SHMalgo(struct radclock_handle *handle, struct bidir_perfstate *state,
    struct bidir_stamp_perf *ptuple, int sID, int NTC_id,
    struct SHM_output *SHMoutput)
{
	int SA_detected = 0;

	/* Parameter access */
	struct bidir_metaparam *metaparam = &(handle->conf->metaparam);
	struct bidir_perfdata *perfdata = handle->perfdata;
	int pp = handle->conf->poll_period;
	int ps = handle->conf->metaparam.path_scale;

	/* Fake SA generation */
	static int SAdur[32] = {0};  // duration of current fake SA-run event (per NTC node)
	static int jitterevent = 0, event2 = 0;  // control jitter event sequences and inter-event scheduling
	int lower, upper;            // SA-run run distribution limits [lower,upper] [s]
	int churnscale = ps;         // for greater clarity of intent, even if churnscale=pathscale
	int switchtime;              // time [s] for starvation to trigger preference to non-local server
	double probshort = 0.6;      // bernoulli parameter: Pr{SA-run} is small
	static int starting = 1;     // startup verb control

	/* switchtime calibration
	 * Based on OCN_{SYD,BRI} where both  ICN_SYD-->ICN_MEL minRTT gap is ~8.2ms
	 * ∆PP = ∆Pquality + ∆Pchange  must be inflated by at least 1/0.8 to combat antichurn_fac=0.8,
	 * and by more for BRI as absolute values larger.
	 */
	switchtime = (0.5 + 8.2*metaparam->relasym_bound_global)/1000/0.7  // ∆PP needed to trigger switch
	     / metaparam->RateErrBOUND;     // map ∆PP to time required

	if (starting) {
		verbose(LOG_NOTICE, "FakeSA starting:  (switchtime, churnscale, pathscale) = (%d, %d, %d) [min]",
		    switchtime/60, churnscale/60, ps/60);
		starting = 0;
	}

	/* Recall NTC_id mapping (see config_mgr.h) :
	 *   TN:  0
	 *  ICN:  (1,2,3,4,5) = (SYD,MEL,BRI,PER,ADL)
	 *  OCN:  (1,2,3,4,5) = (SYD,MEL,BRI,PER,SYD_2)  and +15 for ntc_id:  (16,17,18,19)
	 * Mapping convention into status words:
	 *   - same as for sID into servertrust
	 *   - ie value i mapped to (i+1)-th bit ,  thus the TN is the 1st bit
	 * Impacts:
	 *   TN:  ntc status word causes TN to change PICN, no impact on OCNs
	 *        telemetry: SAs reported in NTC Central DB for all RAD nodes (OCN+TN)
	 *  OCN:  icn status word causes OCNs to avoiding choising SA-affected servers, post unsync status
	 *        this may feedback to the TN to stop using as a preferred server
	 *        telemetry: icn status reported in OCN DBs  (all the same)
	 */
	switch (NTC_id) {
		case -1:  // Undefined
			verbose(LOG_NOTICE, "SHM: Encountered an undefined NTC_id .");
			break;

		// ************************************* ICNs ****************************
		case 1:  // SYD
			/* Trigger new jitterevent sequence
			 * Start is delaying to allow natural PICN to be chosen initially, avoiding antiChurn block.
			 * Inter-sequence gap can be selected here without impacting the sequences.
			 */
			if ( (((state->stamp_i + (6*8-1)*ps/8/pp) % (6*ps/pp)) ? 0 : 1) )
				jitterevent = 1;
			/* While jitter event active, execute on event order */
			if (jitterevent) {
				/* Event 1:  begin SA-run to trigger switch for clients {SYD,BRI}_OCN */
				if ( jitterevent == 1 ) {
					SAdur[NTC_id] = switchtime/pp;
					verbose(VERB_DEBUG, "FakeSA: Event1: SA-run for NTC node %d reset to %d stamps [%d min] at stamp %llu",
					    NTC_id, SAdur[NTC_id], SAdur[NTC_id]*pp/60, state->stamp_i);
				}
				/* Event 2:  begin SA-run in MEL while it is Preferred, to trigger a switch once antiC releases */
				if ( jitterevent == (switchtime + churnscale - 5*switchtime/4)/pp )
					event2 = 1;
				/* Event 3:  begin SA-run to block return to SYD when antiC expires, then end jitter event */
				if ( jitterevent == (switchtime + churnscale - switchtime/2)/pp ) {
					SAdur[NTC_id] = 3*switchtime/4/pp;  // straddle end of antiC period
					verbose(VERB_DEBUG, "FakeSA: Event3: SA-run for NTC node %d reset to %d stamps [%d min] at stamp %llu",
					    NTC_id, SAdur[NTC_id], SAdur[NTC_id]*pp/60, state->stamp_i);
					jitterevent = 0;  // end jitter event
				} else
					jitterevent++;  // advance through event sequence
			}
			/* Play out existing SA-run even if jitter event over */
			if (SAdur[NTC_id] > 0) {
				SAdur[NTC_id]--;
				SA_detected = 1;
			}
			break;

		case 2:  // MEL
			/* While jitter event active, execute on event order */
			if (jitterevent)
				/* Event 2 action: */
				if ( event2 ) { // timing controlled by SYD stamp grid
					SAdur[NTC_id] = 7*switchtime/4/pp;    // longer than SYD trigger to help push BRI over and ensure overlap
					verbose(VERB_DEBUG, "FakeSA: Event2: SA-run for NTC node %d reset to %d stamps [%d min] at stamp %llu",
					    NTC_id, SAdur[NTC_id], SAdur[NTC_id]*pp/60, state->stamp_i);
					event2 = 0;
				}
			/* Play out existing SA-run even if jitter event over */
			if (SAdur[NTC_id] > 0) {
				SAdur[NTC_id]--;
				SA_detected = 1;
			}
			break;

		case 3:  // BRI
			break;
		case 4:  // PER
			SA_detected = (((state->stamp_i + 3) % (160*60/pp)) ? 0 : 1);
			if (SA_detected) {
				lower =  1*60;    // absolute time setting for simplicity
				upper =  20*60;
				SAdur[NTC_id] = lower/pp + rand() % ((upper-lower)/pp + 1);  // initialize SA-lifetime counter [stamps]
				verbose(VERB_DEBUG, "FakeSA: SA-run for NTC node %d reset to %d stamps [%d min] at stamp %llu",
				    NTC_id, SAdur[NTC_id], SAdur[NTC_id]*pp/60, state->stamp_i);
			} else
				if (SAdur[NTC_id] > 0) {
					SAdur[NTC_id]--;
					SA_detected = 1;    // flag SA still active
				}
			break;
		case 5:  // ADL
			SA_detected = (((state->stamp_i + 10*60) % (3*ps/pp)) ? 0 : 1);
			if (SA_detected) {
				if ( rand() < RAND_MAX * (0.7) )  // need short more often to allow MEL to accept ADL
					SAdur[NTC_id] =  0.3*switchtime/pp;
				else
					SAdur[NTC_id] =  2.5*switchtime/pp;
				verbose(VERB_DEBUG, "FakeSA: SA-run for NTC node %d reset to %d stamps [%d min] at stamp %llu",
				    NTC_id, SAdur[NTC_id], SAdur[NTC_id]*pp/60, state->stamp_i);
			} else
				if (SAdur[NTC_id] > 0) {
					SAdur[NTC_id]--;
					SA_detected = 1;    // flag SA still active
				}
			break;
		// ************************************* OCNs ****************************
		case 16:  // OCN SYD
			SA_detected = (((state->stamp_i + 30*60) % (9*ps/4/pp)) ? 0 : 1);
			if (SA_detected) {
				if ( rand() < RAND_MAX * probshort )
					SAdur[NTC_id] =  0.2*switchtime/pp;
				else
					SAdur[NTC_id] =  2.0*switchtime/pp;
				verbose(VERB_DEBUG, "FakeSA: SA-run for NTC node %d reset to %d stamps [%d min] at stamp %llu",
				    NTC_id, SAdur[NTC_id], SAdur[NTC_id]*pp/60, state->stamp_i);
			} else
				if (SAdur[NTC_id] > 0) {
					SAdur[NTC_id]--;
					SA_detected = 1;    // flag SA still active
				}
			break;
		case 17:  // OCN MEL
			SA_detected = ((state->stamp_i % (110*60/pp +5)) ? 0 : 1);   // stamp0 marked
			if (SA_detected) {
				SAdur[NTC_id] =  1*pp/pp;    // single stamp blip
				verbose(VERB_DEBUG, "FakeSA: SA-run for NTC node %d reset to %d stamps [%d min] at stamp %llu",
				    NTC_id, SAdur[NTC_id], SAdur[NTC_id]*pp/60, state->stamp_i);
			} else
				if (SAdur[NTC_id] > 0) {
					SAdur[NTC_id]--;
					SA_detected = 1;    // flag SA still active
				}
			break;
		default:
			SA_detected = 0;
			break;
	}
	//verbose(VERB_DEBUG, "[%llu] (sID, NTC_id) = (%d, %d):   SA_detected = %d",
	//   state->stamp_i, sID, NTC_id, SA_detected);


	/* Update servertrust  [ warn yourself of the SA's discovered ]
	 * TODO: this overrides the servertrust setting - to be reviewed */
	if (state->SA != SA_detected ) {
		handle->servertrust &= ~(1ULL << sID);    // clear bit
		if (SA_detected)
			handle->servertrust |= (1ULL << sID);  // set bit
		verbose(VERB_DEBUG, "[%llu] Change in SA detection for server with "
		    "(sID, NTC_id) = (%d, %d), servertrust now 0x%llX",
		    state->stamp_i, sID, NTC_id, handle->servertrust);
	}

	/* Incorporate SA update in ntc_status word
	 * Currently the existing status is simply overwritten by the new SA value.
	 * Is used for SA telemetry, and for icn_status sent to OCNs inband. */
	if (state->SA != SA_detected && NTC_id != -1) {
		perfdata->ntc_status &= ~(1ULL << NTC_id);    // clear bit
		if (SA_detected)
			perfdata->ntc_status |= (1ULL << NTC_id);  // set bit
		verbose(VERB_DEBUG, "[%llu] Change in SA detection for server with "
		    "(sID, NTC_id) = (%d, %d), ntc_status now 0x%llX",
		    state->stamp_i, sID, NTC_id, perfdata->ntc_status);
	}

	/* Update state */
	state->SA = SA_detected;
	state->SA_total++;

	/* Update outputs */
	SHMoutput->SA = state->SA;
}


/* Evaluate RADclock at this stamp using the Reference timestamps.
 * On entry stamp_i is already updated to this stamp.
 */
void
RADperfeval(struct radclock_handle *handle,
    struct stamp_t *PERFstamp, struct radclock_data *rad_data,
    struct RADperf_output *RPoutput)
{
	long double time;
	double clockerr;

	/* Output a measure of clock error for this stamp by comparing midpoints
	 *  clockerr = ( ( rAd(Ta) + rAd(Tf) ) - ( Tout + Tin ) / 2
	 */
	read_RADabs_UTC(rad_data, &BST(PERFstamp)->Ta, &time, 1);
	clockerr = time;
	read_RADabs_UTC(rad_data, &BST(PERFstamp)->Tf, &time, 1);
	clockerr += time;
	/* If both references available, use Ext */
	if (PERFstamp->type == STAMP_NTP_PERF) {
		clockerr = ( clockerr - (PERFstamp->ExtRef.Tout + PERFstamp->ExtRef.Tin) ) / 2;
		verbose(VERB_QUALITY, "Error in this rAdclock (using ExtRef) on this stamp is %4.2lf [ms]", 1000*clockerr);
	}
	if (PERFstamp->type == STAMP_NTP_INT) {
		clockerr = ( clockerr - (PERFstamp->IntRef.Tout + PERFstamp->IntRef.Tin) ) / 2;
		verbose(VERB_QUALITY, "Error in this rAdclock (using IntRef) on this stamp is %4.2lf [ms]", 1000*clockerr);
	}

	/* Update state */
	//state->RADerror = clockerr;    // perf state no longer passed

	/* Update outputs */
	RPoutput->RADerror = clockerr;
}


/* Function mirrors general structure of process_stamp
 * Attempts to obtain a single PERFstamp, and then processes it.
 * return code:
 *     0 PERFstamp obtained and processed
 *     1 no valid and recognized PERFstamp: nothing to process
 * Supports both live and dead running modes, but dead currently limited to
 * ascii PERFstamp input (hence no dead assembly from RAD + Ref components).
 */
int
process_perfstamp(struct radclock_handle *handle)
{
	int err;
	struct stamp_t PERFstamp;

	/* Multiple server management */
	int sID;       // server ID of new stamp popped here
	int NTC_id;    // NTC global index of serveFr sID
	struct radclock_data *rad_data;
	struct bidir_perfdata *perfdata = handle->perfdata;
	struct bidir_perfstate *state;
	struct SHM_output *SHMoutput;
	struct RADperf_output *RPoutput;

	if (handle->run_mode == RADCLOCK_SYNC_DEAD) {
		sID = handle->last_sID;  // this and laststamp just been set by process_stamp
		PERFstamp = ((struct bidir_algodata*)handle->algodata)->laststamp[sID];
		if (PERFstamp.type != STAMP_NTP_PERF) {
			verbose(LOG_WARNING, "process_perfstamp: not a PERFstamp, skipping it");
			return 1;
		}
	} else {
		err = assemble_next_perfstamp(handle, &PERFstamp);
		if (err == 1)
			return 1;
		/* If a recognized stamp is returned, record the server it came from */
		sID = serverIPtoID(handle, PERFstamp.server_ipaddr);
	}

	verbose(VERB_DEBUG, "Popped a PERFstamp from server %d: [%llu]  "
	    "%"VC_FMT" %"VC_FMT" %.6Lf %.6Lf   %.6Lf %.6Lf  %.6Lf %.6Lf", sID,
	    (long long unsigned) PERFstamp.id,
	    (long long unsigned) BST(&PERFstamp)->Ta,
	    (long long unsigned) BST(&PERFstamp)->Tf,
	    BST(&PERFstamp)->Tb, BST(&PERFstamp)->Te,
	    PERFstamp.IntRef.Tout, PERFstamp.IntRef.Tin,
	    PERFstamp.ExtRef.Tout, PERFstamp.ExtRef.Tin);

	if (sID < 0) {
		verbose(LOG_WARNING, "Unrecognized PERFstamp popped, skipping it");
		return 1;
	}

	/* Set pointers to data for this server. perfdata only defined for ICNs */
	rad_data = &handle->rad_data[sID];
	NTC_id = handle->conf->time_server_ntc_mapping[sID];

	/* Process through the SHM algo */
	// Currently SAs are all fake.   Need to disable when running dead to avoid
	// trust failure from blocking passage of untrusted stamps to the algo and output
	if (handle->conf->is_tn) {
		state     = &perfdata->state[sID];
		SHMoutput = &perfdata->SHMoutput[sID];
		RPoutput  = &perfdata->RPoutput[sID];
		/* Now processing PERFstamp i {0,1,2,...) */
		state->stamp_i++;   // initialized to -1 (no stamps processed)

		// TODO: pass a noleap stamp instead
		if (handle->run_mode == RADCLOCK_SYNC_LIVE)
			SHMalgo(handle, state, &PERFstamp, sID, NTC_id, SHMoutput);
	}


	/* Process through the RADclock performance evaluation */
	struct RADperf_output RPout;
	if (handle->conf->is_ocn)
		PERFstamp.type = STAMP_NTP_INT;    // hack, not even used at present
	RADperfeval(handle, &PERFstamp, rad_data, &RPout);
	if (handle->conf->is_tn) {
		RPoutput->RADerror = RPout.RADerror;
		state->RADerror    = RPout.RADerror;
	}

	/* Write ascii output line for this stamp to file if open */
	print_out_syncline(handle, &PERFstamp, sID);

	/* Telemetry
	 * EXTREF thread telemetry triggering dealt with in PROC:process_stamp .
	 * Here just document EXTREF and Perf variables sent over telemetry.
	 * For each of these, state->stamp_i can be used to detect during trigger
	 * checking if a perfstamp output has been missed by PROC.
	 * Both are based on NTC_ids, with grafana manually separating into ICNs and OCNs. */

	/* EXTREF */
		// NTC Central DB:  perfdata->ntc_status
	/* Perf */
		// TN DB:        state->RADerror for clock sID

	return 0;
}

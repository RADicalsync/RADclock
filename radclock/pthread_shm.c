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
#include "Ext_reference/ext_ref.h"    // API to dag code

/* From create_stamp.c, include in create_stamp.h ?  also MODE_RAD moved there fromm sync_algo.h */
int          insertandmatch_halfstamp(struct stamp_queue *q, struct stamp_t *new, int mode);
int get_fullstamp_from_queue_andclean(struct stamp_queue *q, struct stamp_t *stamp);


/*
 * Integrated thread and thread-work function for SHM module
 */
void *
thread_shm(void *c_handle)
{
	struct radclock_handle *handle = c_handle;

	/* Multiple server management */
	int sID;              // server ID of new stamp popped here
	int NTC_id;           // NTC global index of server sID
	struct radclock_data *rad_data;
	struct bidir_perfdata *perfdata = handle->perfdata;
	struct bidir_perfoutput *output;
	struct bidir_perfstate *state;

	/* Perf stamp matching related */
	const int N = perfdata->RADBUFF_SIZE;
	index_t next0, r;     // unique PERF buffer indices that never wrap
	struct stamp_t copy;  // local copy of stamp from buffer
	struct stamp_t DAGstamp, PERFstamp;
	struct stamp_t *st_r;
	struct dag_cap dag_msg;
	index_t last_next0;   // record RADbuffer write posn at start of emptying
	int got_dag_msg;
	int fullerr;

	/* UNIX socket related */
	int socket_desc;
	struct sockaddr_in server, client;
	socklen_t socklen = sizeof(struct sockaddr_in);
	int num_bytes;


	/* Deal with UNIX signal catching */
	init_thread_signal_mgt();

	/* Create the server socket and initialize to listen to the DAG host client */
	socket_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socket_desc == -1) {
		perror("socket");
		verbose(LOG_ERR, "SHM: Could not create socket");
		pthread_exit(NULL);
	}
	memset((char *) &server, 0, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(DAG_PORT);
	server.sin_addr.s_addr = htonl(INADDR_ANY);  // avoids needing to know DAGhost IP

	/* Eliminates "ERROR on binding: Address already in use" error */
	//	int optval = 1;
	//	setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

	// int flags = fcntl(socket_desc, F_GETFL, 0) | O_NONBLOCK;
	// if (fcntl(socket_desc, F_SETFL, flags) == 1)
	// 	printf("SHM: Failed to set socket to non blocking");

	/* Set a receive timeout */
	struct timeval so_timeout;
	so_timeout.tv_sec = 0;
	so_timeout.tv_usec = 1e6 * 0.2;
	setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO, (void*)(&so_timeout), sizeof(struct timeval));

	/* Bind socket */
	if ( bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) == -1 ) {
		verbose(LOG_ERR, "SHM: Socket bind() error. Killing thread: %s", strerror(errno));
		pthread_exit(NULL);
	}


	verbose(LOG_NOTICE, "SHM: now listening for DAG messages on port %d", DAG_PORT);
	last_next0 = 0;  // enable dag msg code to see if anything new in buffer since last attempt

	while ((handle->pthread_flag_stop & PTH_SHM_CON_STOP) != PTH_SHM_CON_STOP)
	{

		/* Empty RAD halfstamp buffer into the perf matching queue (ie, take a
		 * batch of RAD arrivals from PROC and put under SHM control).
		 * Each halfstamp successfully read is marked as read by zeroing the id.
		 * Reads proceed backward in time until no unread stamps are left, or until
		 * the (potentially advancing) write head position is encountered
		 * [new arrivals during emptying are left for next time].
		 * Checks are made to ensure data consistency, possible due to the unique
		 * uint_64 indices (which don't wrap) of the write and read positions.
		 * The buffer and stamp matching queue accept stamps from any clock/server.
		 */
		next0 = perfdata->RADbuff_next;
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
					verbose(LOG_ERR, "SHM: halfstamp corrupted during RADstamp buffer"
					    " emptying, up to %d perf stamps may have been lost", next0 - r + 1);
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


		/* Make an attempt to obtain the next authoritative halfstamp.
		 * A timeout is used to reduce processing, to allow the thread kill signal to
		 * be checked for, and to give the OS an opportunity to suspend the thread
		 */
		num_bytes = recvfrom(socket_desc, &dag_msg, sizeof(struct dag_cap),
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


		/* Fake DAG stamp creation for testing
		 * Only create if
		 *  - true DAG msg fails.
		 *  - nothing new in RADbuffer since last draining (works best with single time_server)
		 * Other test variables to remove: matchqueue_mutex, last_sID
		 */
		struct radclock_error *rad_error;
		struct stamp_t RADstamp;

		if ( got_dag_msg == 0 && next0 != last_next0 )
		{
			/* Obtain the last accepted RADstamp using the last_sID hack */
			RADstamp = ((struct bidir_algodata*)handle->algodata)->laststamp[handle->last_sID];
			rad_error = &handle->rad_error[handle->last_sID];

			/* Construct fake DAG message to match RADstamp */
			/* id field uint64_t --> l_fp conversion */
			dag_msg.stampid.l_int = htonl(RADstamp.id >> 32);
			dag_msg.stampid.l_fra = htonl((RADstamp.id << 32) >> 32);
			inet_aton(RADstamp.server_ipaddr, &dag_msg.ip);
			dag_msg.Tout = RADstamp.st.bstamp.Tb - rad_error->min_RTT/2 + 0.3e-3;
			dag_msg.Tin  = RADstamp.st.bstamp.Te + rad_error->min_RTT/2 - 0.3e-3;

			got_dag_msg = 1;
		}


		/* Map DAG message into a perf stamp and insert in matchqueue */
		if ( got_dag_msg == 1 ) {
			memset(&DAGstamp, 0, sizeof(struct stamp_t));
			DAGstamp.type = STAMP_NTP_PERF;
			/* id field l_fp --> uint64_t conversion */
			DAGstamp.id = ((uint64_t) ntohl(dag_msg.stampid.l_int)) << 32;
			DAGstamp.id |= (uint64_t) ntohl(dag_msg.stampid.l_fra);
			strcpy(DAGstamp.server_ipaddr, inet_ntoa(dag_msg.ip));
			DAGstamp.st.pstamp.Tout = dag_msg.Tout;
			DAGstamp.st.pstamp.Tin  = dag_msg.Tin;

			/* Corrupt DAG halfstamp inputs for testing */
			//if (next0 % 3 == 0)	DAGstamp.id +=1;                  // corrupt some ids
			//if (next0 % 2 == 0)	DAGstamp.server_ipaddr[0] = "9";  // corrupt some IPs

			/* Insert DAG perf halfstamp into the perf stamp matching queue */
			//verbose(LOG_DEBUG, " ||| trying to insert a DAG halfstamp");
			insertandmatch_halfstamp(perfdata->q, &DAGstamp, MODE_DAG);
		}


		/* Check if a fullstamp is available
		 * Don't bother if there has been no new input since last time.
		 * TODO: modify return codes of matching queue calls to keep track of the
		 *       number of full stamps, so this will not be called if not needed,
		 *       and Will be called if there is a fullstamp backlog, despite no new input */
		if (got_dag_msg || next0 != last_next0) {
			verbose(VERB_DEBUG, "New halfstamps inserted: checking for full perf stamp");
			fullerr = get_fullstamp_from_queue_andclean(perfdata->q, &PERFstamp);
		} else
			fullerr = 1;

		last_next0 = next0;

		/* If no stamp available, nothing to process, just look again */
		if (fullerr == 1)
			continue;


		/* If a recognized stamp is returned, record the server it came from */
		sID = serverIPtoID(handle, PERFstamp.server_ipaddr);
		if (sID < 0) {
			verbose(LOG_WARNING, "Unrecognized perf stamp popped, skipping it");
			continue;
		} else
			verbose(VERB_DEBUG, "Popped a PERF stamp from server %d: [%llu]  "
			    "%"VC_FMT" %"VC_FMT" %.6Lf %.6Lf    %.6Lf %.6Lf", sID,
			    (long long unsigned) PERFstamp.id,
			    (long long unsigned) PSTB(&PERFstamp)->Ta,
			    (long long unsigned) PSTB(&PERFstamp)->Tf,
			    PSTB(&PERFstamp)->Tb, PSTB(&PERFstamp)->Te,
			    PST(&PERFstamp)->Tin, PST(&PERFstamp)->Tout);


		/* Write ascii output line for this stamp to file if open */
		print_out_syncline(handle, &PERFstamp, sID);


		/* Set pointers to data for this server */
		rad_data = &handle->rad_data[sID];
		output = &perfdata->output[sID];
		state  = &perfdata->state[sID];
		NTC_id = handle->conf->time_server_ntc_mapping[sID];


	/* TODO: code below here factor into a  process_perfstamp(handle, &PERFstamp);  ??
	 *  When processing this stamp, state holds the state from Last time, until it updates it at
	 *  the end, just before telemetry
	 */

	/* ********************** SHM specific ****************************/
		/* Test code for SA detector */
		int SA_detected = 0;

		/* Recall NTC_id mapping (see config_mgr.h) :
		 *   CN:  0
		 *  ICN:  (1,2,3,4,5) = (SYD,MEL,BRI,PER,ADL)
		 *  OCN:  (1,2,3,4)   = (SYD,MEL,BRI,PER)  and +15 for ntc_id:  (16,17,18,19)
		 * Mapping convention into status words:
		 *   - same as for sID into servertrust
		 *   - ie value i mapped to (i+1)-th bit ,  thus the CN is the 1st bit
		 * Impacts:
		 *   CN:  ntc status word causes CN to change PICN, no impact on OCNs
		 *        telemetry: SAs reported in NTC Central DB for all RAD nodes (OCN+CN)
		 *  OCN:  icn status word causes OCNs to move PICN off SA-affected servers, post unsync status
		 *        this may feedback to the CN to stop using as a preferred server
		 *        telemetry: icn status reported in OCN DBs  (all the same)
		 */
		switch (NTC_id) {
			case -1:  // Undefined
				verbose(LOG_NOTICE, "SHM: Encountered an undefined NTC_id .");
				break;
			case 1:  // SYD
				SA_detected = ((state->stamp_i % (120*60/4)) ? 0 : 1);  // min*..
				break;
			case 2:  // MEL
				SA_detected = ((state->stamp_i % (3*120*60/4)) ? 0 : 1);
				break;
			case 3:  // BRI
				break;
			case 4:  // PER
				SA_detected = ((state->stamp_i % (160*60/4 +3)) ? 0 : 1);
				break;
			case 5:  // ADL
				SA_detected = ((state->stamp_i % (180*60/4 +5)) ? 0 : 1);
				break;
			case 16:  // OCN SYD
				break;
			case 17:  // OCN MEL
				SA_detected = ((state->stamp_i % (110*60/4 +5)) ? 0 : 1);
				break;
			default:  // other ICN, or an OCN
				SA_detected = 0;
				break;
		}
		//verbose(VERB_DEBUG, "[%llu] (sID, NTC_id) = (%d, %d):   SA_detected = %d",
		//   state->stamp_i, sID, NTC_id, SA_detected);


		/* Update servertrust  [ warn yourself of the SA's discovered ]
		 * TODO: this overrides the servertrust setting - to be reviewed */
		if (state->SA != SA_detected) {
			handle->servertrust &= ~(1ULL << sID);     // clear bit
			if (SA_detected)
			    handle->servertrust |= (1ULL << sID);  // set bit
			verbose(VERB_DEBUG, "[%llu] Change in SA detection for server with "
			   "(sID, NTC_id) = (%d, %d), servertrust now 0x%llX",
			    state->stamp_i, sID, NTC_id, handle->servertrust);
		}

		/* Incorporate SA update in ntc_status word
		 * Currently the existing status is simply overwritten by the new SA value.
		 * TODO: make available to preferred_RADclock() and write smarter logic,
		 *       would require wait for next packet before being actioned
		 *   Is used for SA telemetry, and for icn_status sent to OCNs inband. */
		if (state->SA != SA_detected && NTC_id != -1) {
			perfdata->ntc_status &= ~(1ULL << NTC_id);    // clear bit
			if (SA_detected)
				perfdata->ntc_status |= (1ULL << NTC_id);  // set bit
			verbose(VERB_DEBUG, "[%llu] Change in SA detection for server with "
			    "(sID, NTC_id) = (%d, %d), ntc_status now 0x%llX",
			    state->stamp_i, sID, NTC_id, perfdata->ntc_status);
		}


	/* ********************** RAD Perf specific ****************************/

		/* Output a measure of clock error for this stamp by comparing midpoints
		 *  clockerr = ( ( rAd(Ta) + rAd(Tf) ) - ( Tout + Tin ) / 2  */
		struct bidir_stamp_perf *ptuple;
		long double time;
		double clockerr;

		ptuple = &PERFstamp.st.pstamp;
		read_RADabs_UTC(rad_data, &ptuple->bstamp.Ta, &time, 1);
		clockerr = time;
		read_RADabs_UTC(rad_data, &ptuple->bstamp.Tf, &time, 1);
		clockerr += time;
		clockerr = ( clockerr - (ptuple->Tout + ptuple->Tin) ) / 2;
		verbose(VERB_QUALITY, "Error in this rAdclock on this stamp is %4.2lf [ms]", 1000*clockerr);



	/* ********************** Combined ****************************/

		/* Update state */
		state->stamp_i++;
		/* SHM */
		state->SA = SA_detected;
		state->SA_total++;
		/* Perf */
		state->RADerror = clockerr;

		/* Update dumpable outputs [from state] */
		/* SHM */
		output->SA = state->SA;
		/* Perf */
		output->RADerror = state->RADerror;


		/* Telemetry
		 * SHM thread telemetry triggering dealt with in PROC:process_stamp .
		 * Here just document SHM and Perf variables sent over telemetry.
		 * For each of these, state->stamp_i can be used to detect during trigger
		 * checking if a perfstamp output has been missed by PROC.
		 * Both are based on NTC_ids, with grafana manually separating into ICNs and OCNs. */

		/* SHM */
			// NTC Central DB:  perfdata->ntc_status
		/* Perf */
			// CN DB:        state->RADerror for clock sID


	}  // thread_stop while loop


	if (socket_desc >= 0)
		close(socket_desc);

	/* Thread exit */
	verbose(LOG_NOTICE, "Thread shm is terminating.");
	pthread_exit(NULL);

}


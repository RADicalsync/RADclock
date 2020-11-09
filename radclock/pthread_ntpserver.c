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

#include "../config.h"
#include "radclock.h"
#include "radclock-private.h"
#include "radclock_daemon.h"
#include "verbose.h"
#include "sync_history.h"
#include "sync_algo.h"
#include "pthread_mgr.h"
#include "proto_ntp.h"
#include "misc.h"
#include "jdebug.h"
#include "config_mgr.h"


/*
 * Integrated thread and thread-work function for NTP_SERV
 */
void *
thread_ntp_server(void *c_handle)
{
	struct radclock_handle *handle;
	
	/* RADclock related */
	struct radclock_data rdata;
	double clockerror;
	vcounter_t vcount_rec = 0, vcount_xmt = 0;

	/* UNIX socket related */
	int s_server;
	struct sockaddr_in sin_server, sin_client;
	socklen_t len;
	char *pkt_in;
	int pkt_in_mode;
	struct ntp_pkt *pkt_out;
	int n, err;

	/* NTP packet related */
	double rootdelay;				// is uint32_t in ntp_pkt structure
	double rootdispersion;		// is uint32_t in ntp_pkt structure

	/* Timestamps to send [ NTP format ]
	 * reftime: last time (this host's) clock was updated (local time)
	 * org: 		timestamp from the client
	 * rec: 		timestamp when receiving packet (local time)
	 * xmt: 		timestamp when sending packet (local time) */
	l_fp reftime, rec, xmt;


	/* Deal with UNIX signal catching */
	init_thread_signal_mgt();

	/* Local copy of global data handle */
	handle = (struct radclock_handle *) c_handle;

	/* Create the server socket and initialize to listen to clients */
	if ((s_server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 ) {
		perror("socket");
		pthread_exit(NULL);
	}
	memset((char *) &sin_client, 0, sizeof(struct sockaddr_in));
	memset((char *) &sin_server, 0, sizeof(struct sockaddr_in));
	len = sizeof(sin_client);
	sin_server.sin_family 		= AF_INET;
	sin_server.sin_addr.s_addr = htonl(INADDR_ANY);
	sin_server.sin_port = htons((long)handle->conf->ntp_downstream_port);

	/* Set a receive timeout */
	struct timeval so_timeout;
	so_timeout.tv_sec 	= 1;
	so_timeout.tv_usec 	= 0;
	setsockopt(s_server, SOL_SOCKET, SO_RCVTIMEO, (void*)(&so_timeout),
			sizeof(struct timeval));

	/* Bind socket */
	err = bind(s_server, (struct sockaddr *)&sin_server, sizeof(struct sockaddr_in));
	if (err == -1) {
		verbose(LOG_ERR, "NTPserver: Socket bind() error. Killing thread: %s",
				strerror(errno));
		pthread_exit(NULL);
	}

	/* Create incoming and outgoing NTP packet bodies
	 * We don't know what we are receiving (backward compatibility with exotic
	 * NTP clients?) so allocate a big buffer for the receiving side. But in all
	 * cases, reply with minimal packets.
	 * TODO: security issues?
	 */
	pkt_in  = (char*) malloc(sizeof(char)*NTP_PKT_MAX_LEN);
	JDEBUG_MEMORY(JDBG_MALLOC, pkt_in);
	pkt_out = (struct ntp_pkt*) malloc(sizeof(struct ntp_pkt)); // why malloc this?
	JDEBUG_MEMORY(JDBG_MALLOC, pkt_out);

	verbose(LOG_NOTICE, "NTPserver: service begun.");


	/* Accept packets from clients, process requests, and send back data.
	 * The socket timeout ensures recvfrom will not block forever, so that the
	 * while's thread stopping condition can be tested periodically.
	 * This code currently assumes that the daemon is not a Stratum-1.
	 */
	while ((handle->pthread_flag_stop & PTH_NTP_SERV_STOP) != PTH_NTP_SERV_STOP) {

		/* Receive the UDP request using a blocking call with timeout */
		n = recvfrom(s_server, (void*) pkt_in, NTP_PKT_MAX_LEN, 0,
				(struct sockaddr*)&sin_client, &len);
		if (n < 0) {	// no bytes read after timeout
			continue;	// enable check if thread should STOP
		}

		/* Raw timestamp the request packet arrival: must be done ASAP */
		err = radclock_get_vcounter(handle->clock, &vcount_rec);
		if (err < 0) {
			verbose(LOG_WARNING, "NTPserver: failed to read raw timestamp of incoming NTP request");
			continue;			// response will not be sent
		}

		/* Process according to MODE of incoming packet */
		pkt_in_mode = PKT_MODE( ((struct ntp_pkt*)pkt_in)->li_vn_mode );
		switch (pkt_in_mode) {
		case MODE_UNSPEC:
		case MODE_ACTIVE:
		case MODE_PASSIVE:
		case MODE_SERVER:
		case MODE_BROADCAST:
			verbose(VERB_DEBUG, "NTPserver: received bad mode, ignoring.");
			continue;

		case MODE_PRIVATE:
		case MODE_CONTROL:
			/* Who is using ntpq or ntpdc? */
			verbose(VERB_DEBUG, "NTPserver: received control message, ignoring.");
			continue;

		case MODE_CLIENT:
			// process_MODEclient(...)    // move to this when process multiple modes
			break;
		}

		/* Copy the current raddata, looping to ensure consistency.
		 * Is needed to compute timestamps and timestamp errors. */
		do {
			memcpy(&rdata, RAD_DATA(handle), sizeof(struct radclock_data));
		} while (memcmp(&rdata, RAD_DATA(handle), sizeof(struct radclock_data)) != 0);

		
		/* NTP specification "seems" to indicate that the dispersion grows linear
		 * at worst case rate error set to 15 PPM. The constant component is twice
		 * the precision +  the filter dispersion which is a weighted sum of the
		 * (past?) clock offsets.  The value of 15 PPM is somewhat arbitrary, trying
		 * to reflect the fact that XO are much better than their 500 PPM specs.
		 * Also precision in here is horrible ntpd linguo meaning "period" for us.
		 * XXX Here I use the clock error as an equivalent to the filter
		 * dispersion, I think it is safe to use the handle for that value
		 * (should be some kind of longer term value anyway)
		 */
		clockerror = RAD_ERROR(handle)->error_bound_avg;
		rootdispersion = NTP_SERVER(handle)->rootdispersion + clockerror +
			rdata.phat + (vcount_rec - rdata.last_changed) * rdata.phat_local * 15e-6;

		rootdelay = NTP_SERVER(handle)->rootdelay + NTP_SERVER(handle)->minRTT;


		/* Fill the outgoing packet
		 * Fixed point conversion of rootdispersion and rootdelay with up-down round up
		 * NTP_VERSION:  add an abusive value to enable RAD client<-->server testing
		 */
		memset((char *) pkt_out, 0, sizeof(struct ntp_pkt));
		u_char ntpversion = 5;		// use instead of NTP_VERSION to signal a RADclock server
		
		/* NTP Standard requires special stratum and LI if server not in sync */
		if (HAS_STATUS(handle, STARAD_UNSYNC)) {
			pkt_out->stratum = 0;	// STRATUM_UNSPEC not used in responses, mapped to 0
			pkt_out->li_vn_mode = PKT_LI_VN_MODE(LEAP_NOTINSYNC, ntpversion, MODE_SERVER);
		} else {
			pkt_out->stratum = NTP_SERVER(handle)->stratum + 1;
			// TODO: pass on per-pkt LI from daemon's server, include in  ntp_server to access?
			pkt_out->li_vn_mode = PKT_LI_VN_MODE(LEAP_NOWARNING, ntpversion, MODE_SERVER);
		}
		
		pkt_out->ppoll			= ((struct ntp_pkt*)pkt_in)->ppoll;
		pkt_out->precision		= -18;	/* TODO: should pass min(STA_NANO (or mus), phat) in power of 2 or so */
		pkt_out->rootdelay 		= htonl( (uint32_t)(rootdelay * 65536. + 0.5));
		pkt_out->rootdispersion = htonl( (uint32_t)(rootdispersion * 65536. + 0.5));
	
		/* refid:  wording in the standard, for daemons with stratum>1, is unclear
		 *   Examples frequently suggest that the refid of the daemon should replicate
		 *   that of the daemon's server, ie:
		 *     		pkt_out->refid	= htonl(NTP_SERVER(handle)->refid);
		 *   however this recursion would result in the whole tree under a Stratum-1
		 *   having the Stratum 1's IP address (in fact S1 code string!) as a refid,
		 *   not useful for loop detection.
		 *   We implement the other interpretation, that the refid of the daemon be
		 *   the IP address of its server (whether the server is Stratum-1 or not).
		 *   It is this refid (the daemon's own) that is inserted into response packets.
		 *	TODO: add timingloop test: test if refid of this client not the daemon's server IP
		 * TODO: put in appropriate KISS code (DENY seems only fit) if stratum set to zero above?
		 */
		 // Note:  no htonl conversion on IP addresss, so don't convert back here !
		 pkt_out->refid = NTP_CLIENT(handle)->s_to.sin_addr.s_addr;
		 
		 
      // inet_ntoa(NTP_CLIENT(handle)->s_from.sin_addr)



		/* Fill the timestamp fields
		 * reftime, rec:  use rAdclock
		 * org:           copied over from the xmt field of incoming request pkt
		 * xmt:           use raDclock (more robust, accurate, faster)
		 */
		long double time;
		//memset((char *) &reftime, 0, sizeof(l_fp));
		//memset((char *) &org, 0, sizeof(l_fp));
		//memset((char *) &rec, 0, sizeof(l_fp));
		//memset((char *) &xmt, 0, sizeof(l_fp));
		
		/* Reference time (time RADclock was last updated on this server) */
		read_RADabs_UTC(&handle->rad_data, &rdata.last_changed, &time, PLOCAL_ACTIVE);
		UTCld_to_NTPtime(&time, &reftime);
		pkt_out->reftime.l_int = htonl(reftime.l_int);
		pkt_out->reftime.l_fra = htonl(reftime.l_fra);

		/* Origin Timestamp (Ta in algo language) */
		pkt_out->org = ((struct ntp_pkt*)pkt_in)->xmt;

		/* Receive Timestamp (Tb in algo language) */
		read_RADabs_UTC(&handle->rad_data, &vcount_rec, &time, PLOCAL_ACTIVE);
		UTCld_to_NTPtime(&time, &rec);
		pkt_out->rec.l_int = htonl(rec.l_int);
		pkt_out->rec.l_fra = htonl(rec.l_fra);

		verbose(VERB_DEBUG, "Reply to NTP client %s with statum=%d rdelay=%.06f "
				"rdisp= %.06f clockerror= %.06f diff= %"VC_FMT" Tb= %d.%06d",
				inet_ntoa(sin_client.sin_addr), pkt_out->stratum, rootdelay,
				rootdispersion, clockerror, (vcount_rec - rdata.last_changed),
				rec.l_int, rec.l_fra );

		/* Transmit Timestamp (Te in algo language) */
		err = radclock_get_vcounter(handle->clock, &vcount_xmt); // send ASAP after this
		if (err < 0) {
			verbose(LOG_WARNING, "NTPserver: failed to read raw timestamp of outgoing NTP response");
			continue;			// response will not be sent
		}
		/* Use difference clock:  xmt = rec + Cd(vcount_xmt) - Cd(vcount_rec)
		 * Ignore plocal refinement for greater robustness and simplicity:
		 * at these timescales, the difference is sub-ns */
		time += handle->rad_data.phat*(vcount_xmt - vcount_rec);
		UTCld_to_NTPtime(&time, &xmt);
		pkt_out->xmt.l_int = htonl(xmt.l_int);
		pkt_out->xmt.l_fra = htonl(xmt.l_fra);
		
		
		/* Send data back using the client's address */
		// TODO: So far we send the minimum packet size ... we may change that later
		err = sendto(s_server, (char*)pkt_out, LEN_PKT_NOMAC, 0,
				(struct sockaddr *)&sin_client, len);
		if (err < 0)
			verbose(LOG_ERR, "NTPserver: Socket send() error: %s", strerror(errno));
			
	} /* while */


	/* Thread exit */
	verbose(LOG_NOTICE, "NTPserver: thread is terminating.");
	JDEBUG_MEMORY(JDBG_FREE, pkt_in);
	free(pkt_in);
	JDEBUG_MEMORY(JDBG_FREE, pkt_out);
	free(pkt_out);
	close(s_server);		// test this
	
	pthread_exit(NULL);
}

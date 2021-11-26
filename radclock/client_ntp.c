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

#include "../config.h"
#include <sys/time.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
//#include <netinet/in.h>

#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <syslog.h>
#include <stdlib.h>			// for calloc
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include "radclock.h"
#include "radclock-private.h"
#include "radclock_daemon.h"
#include "verbose.h"
#include "sync_history.h"
#include "sync_algo.h"
#include "config_mgr.h"
#include "proto_ntp.h"
#include "misc.h"
#include "pthread_mgr.h"
#include "jdebug.h"

#define NTP_MIN_SO_TIMEOUT 5000		/* units of [mus] */
#define NTP_INTIAL_SO_TIMEOUT 900000	/* very large in case RTT large */

/*
 * POSIX timer and signal catching mask
 * This requires FreeBSD 7.0 and above for POSIX timers.
 * Also, sigsuspend does not work on Linux in a multi-thread environment
 * (apparently) so use pthread condition wait to sync the thread to SIGALRM
 */
timer_t *ntpclient_timerid;
float *ntpclient_period;
double *ntpclient_timeout;
extern pthread_mutex_t alarm_mutex;
extern pthread_cond_t alarm_cwait;


/* Initilize the client socket info, create the timer */
int
ntp_client_init(struct radclock_handle *handle)
{
	/* Multiple server management */
	int s;
	struct radclock_ntp_client	*client;
	timer_t *timerid;
	char *domain;			// start address of string for this server
	float poll_period;
	
	/* Socket data */
	struct hostent *he;
	struct timeval so_timeout;
	double timeout;

	/* Signal catching */
	sigset_t alarm_mask;
	struct sigaction sig_struct;
	struct sigevent evp;


	/* Do we have what it takes? */
	// mRAD: update after integrated conf changes
	if (strlen(handle->conf->time_server) == 0) {
		verbose(LOG_ERR, "NTPclient: No NTP server specified!");
		return (1);
	}

	/* Allocate parameter arrays to global pointers */
	ntpclient_timerid = calloc(handle->nservers,sizeof(timer_t));
	ntpclient_period  = calloc(handle->nservers,sizeof(float));
	ntpclient_timeout = calloc(handle->nservers,sizeof(double));

	handle->lastalarm_sID = -1;

	/* Initialize socket timeout value to well over expected congested RTT */
	so_timeout.tv_sec = 0;
	so_timeout.tv_usec = NTP_INTIAL_SO_TIMEOUT;
	timeout = so_timeout.tv_sec + 1e-6 * so_timeout.tv_usec;

	/* Set up SIGALRM signal that timers will fire, and provide server info */
	evp.sigev_notify = SIGEV_SIGNAL;
	evp.sigev_signo = SIGALRM;
	sigemptyset(&alarm_mask);
	sigaddset(&alarm_mask, SIGALRM);
	sig_struct.sa_mask = alarm_mask;
	sig_struct.sa_flags = SA_SIGINFO;
	sig_struct.sa_sigaction = catch_alarm;	// info prototype since SA_SIGINFO set
	/* Initialise the signal data */
	sigaction(SIGALRM, &sig_struct, NULL);
		
	/* Initialize thread comms to return to TRIGGER after MAIN catches signal */
	pthread_mutex_init(&alarm_mutex, NULL);
	pthread_cond_init(&alarm_cwait, NULL);

	/* Loop over all servers to set up networking data and timers for each */
	for (s=0; s < handle->nservers; s++) {

		client = &handle->ntp_client[s];
		timerid = &ntpclient_timerid[s];
		poll_period = (float) handle->conf->poll_period;	// upgrade after

		ntpclient_period[s] = MIN(BURST_DELAY,poll_period);
		domain = handle->conf->time_server + s*MAXLINE;
		
		/* Build server infos */
		client->s_to.sin_family = PF_INET;
		client->s_to.sin_port = ntohs(handle->conf->ntp_upstream_port);
		if ((he=gethostbyname(domain)) == NULL) {
			herror("gethostbyname");
			return (1);
		}
		client->s_to.sin_addr.s_addr = *(in_addr_t *)he->h_addr_list[0];
		verbose(LOG_NOTICE, "server %d: %s ,  resolved to %s", s, domain,
			inet_ntoa(client->s_to.sin_addr));

		/* Create the socket */
		if ((client->socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("socket");
			return (1);
		}
		/* Set timeout on the recv side to avoid blocking for lost packets */
		setsockopt(client->socket, SOL_SOCKET,
				SO_RCVTIMEO, (void *)(&so_timeout), sizeof(struct timeval));
		ntpclient_timeout[s] = timeout;


		/* Create and arm periodic timers to generate the pkt sending grid. */
		evp.sigev_value.sival_int = s;	// pass server ID to the signal handler
		if (timer_create(CLOCK_REALTIME, &evp, timerid) < 0) {	// timerid is stored
			verbose(LOG_ERR, "NTPclient: POSIX timer create failed");
			return (1);
		}
		/* After a short wait, stagger starting grids over the first half poll_period */
		if (set_ptimer(*timerid, 0.5 + poll_period*s/(2*handle->nservers), poll_period) < 0) {
			verbose(LOG_ERR, "NTPclient: POSIX timer cannot be set");
			return (1);
		}
		verbose(VERB_DEBUG, "server %d: POSIX timer set with timerid %d", s, *timerid);
	}

	return (0);
}



/* Create a fresh NTP request pkt.
 * Each call will return a unique pkt since the vcounter is read afresh.
 * Potential nonce uniqueness failure due to finite resolution is dealt with.
 * The preferred clock is used to fill reftime and xmt fields, and statics.
 * Hence with multiple servers, the function does not act on the server the
 * packet is destined for.
 */
static int
create_ntp_request(struct radclock_handle *handle, struct ntp_pkt *pkt,
		struct timeval *xmt_tval)
{
	static long double last_time = 0.0;		// for timestamp sanity check
	static l_fp last_xmt = {0,0};				// used to ensure nonce is unique

	long double time;
	l_fp reftime, xmt;
	vcounter_t vcount;
	int err;

	pkt->li_vn_mode	= PKT_LI_VN_MODE(LEAP_NOTINSYNC, NTP_VERSION, MODE_CLIENT);
	pkt->stratum		= STRATUM_UNSPEC;
	pkt->stratum		= NTP_SERVER(handle)->stratum + 1;
	pkt->ppoll			= NTP_MINPOLL;
	pkt->precision		= -6;		/* Like ntpdate */
	pkt->rootdelay		= htonl(FP_SECOND);
	pkt->rootdispersion	= htonl(FP_SECOND);
	
	/* Must give IP of daemon's server. See notes in pthread_ntpserver.c */
	pkt->refid = NTP_CLIENT(handle)->s_to.sin_addr.s_addr;
	/* Wrong. Eg, if the server an S1, would be a string, not an IP as reqd. */
	//pkt->refid			= htonl(NTP_SERVER(handle)->refid);

	/* Reference time [ time client clock was last updated ] */
	vcount = RAD_DATA(handle)->last_changed;
	read_RADabs_UTC(RAD_DATA(handle), &vcount, &time, PLOCAL_ACTIVE);
	
	UTCld_to_NTPtime(&time, &reftime);
	pkt->reftime.l_int = htonl(reftime.l_int);
	pkt->reftime.l_fra = htonl(reftime.l_fra);

	pkt->org.l_int	= pkt->org.l_fra = 0;
	pkt->rec.l_int	= pkt->rec.l_fra = 0;


	/* Transmit time (also used as a nonce for packet matching) */
	err = radclock_get_vcounter(handle->clock, &vcount);
	if (err < 0)
		return (1);
	read_RADabs_UTC(RAD_DATA(handle), &vcount, &time, PLOCAL_ACTIVE);

	/* Sanity test for apparently identical pkts based on ld time
	 * TODO: this problem probably no longer can occur, if not, could remove test.
	 *    If counter super coarse, could occur, but not an error in that case, and
	 *    nonce still unique, another reason to drop this.
	 *  After kernel init of FFclock data, but before any kernel setting, and before
	 *  any valid stamp (or first two stamps), period=0 and so time = 0 = last_time,
	 *  so sanity test will not run. In that initial phase, 1 or more responses
	 *  will be sent with xmt=NTP0.
	 * Once time>0, test can run, but shouldn't ever
	 */
	//verbose (LOG_ERR, "last_time = %10.9Lf, time = %10.9Lf, vcount= %llu",
	// last_time, time, (long long unsigned) vcount);
	if (last_time > 0 && time == last_time) {	// subsequent pkts have same ld timestamp
		verbose(LOG_ERR, "NTPclient: xmt = last_time !! pkt id not unique! "
							"last_time =%10.9Lf, time = %10.9Lf, vcount= %llu",
							last_time, time, (long long unsigned) vcount);
		//return (1);  // return to try again
	}

	UTCld_to_NTPtime(&time, &xmt);

	//verbose(LOG_DEBUG, "last nonce: %10lu.%10lu [s]", last_xmt.l_int, last_xmt.l_fra);
	//verbose(LOG_DEBUG, "xmt  nonce: %10lu.%10lu [s]", xmt.l_int, xmt.l_fra);

	/* Check, and ensure, that xmt timestamp unique, needed for use as a nonce
	 * This only protects against uniqueness wrt the previous packet, not globally
	 */
//	if ( last_time > 0 && xmt.l_int == last_xmt.l_int
	if ( xmt.l_int == last_xmt.l_int && xmt.l_fra == last_xmt.l_fra ) {
		verbose(LOG_WARNING, "NTPclient: xmt nonce not unique: %10lu.%10lu [s], "
				"vcount= %llu", xmt.l_int, xmt.l_fra, (long long unsigned) vcount);
		xmt.l_fra += 1;
		if (xmt.l_fra == 0) xmt.l_int += 1;		// if overflowed, advance second

		verbose(LOG_WARNING,"  nonce fraction increased to %10lu", xmt.l_fra);
	}

	pkt->xmt.l_int = htonl(xmt.l_int);
	pkt->xmt.l_fra = htonl(xmt.l_fra);

	last_time = time;
	last_xmt.l_int = xmt.l_int;
	last_xmt.l_fra = xmt.l_fra;
	
	UTCld_to_timeval(&time, xmt_tval);			// need to pass back a timeval
	
	return (0);
}


/* Failed match test based on standard xmt-->org nonce. */
static int
unmatched_ntp_pair(struct ntp_pkt *spkt, struct ntp_pkt *rpkt)
{
	if ((spkt->xmt.l_int == rpkt->org.l_int) &&	   // matches
		 (spkt->xmt.l_fra == rpkt->org.l_fra))
		return (0);
	else
		return (1);		// ie returns true if Unmatched
}


/* Perform the trigger thread work in the case of a bidir NTP exchange.
 * An infinite loop of NTP request packets are send on a period grid, and
 * responses received and matched.   Incorporates
 *   - "adjusted_period" to enable variations of the sending grid
 *   - "maxattempts" to allow retries in case of missing responses.
 *		  Each attempt is unique, with a distinct xmt nonce field
 *   - "timeout" to control socket timeout as a function of RTT conditions
 * Timeouts are managed as a function of grid period to avoid retries overlapping,
 * keeping grid points distinct for simplicity.
 * Simple (request,response) matching is used (based on the nonce) to give a
 * indication for debugging if the expected match has occurred, and to help keep
 * the client in sync with its sending attempts, however technically no actual
 * matching is performed, TRIGGER is not even responsible for collecting the
 * packet data!
 * After a grid point is dealt with (with or without a response pkt found,
 * matched or not), the client continues on and signals PROC thread to take a look.
 */
int
ntp_client(struct radclock_handle *handle)
{
	/* Multiple server management */
	int sID;
	struct radclock_ntp_client	*client;
	struct radclock_ntp_server	*server;
	timer_t *timerid;
	struct bidir_algodata *algodata;
	struct bidir_algostate *state;
	struct radclock_error *rad_error;
	float poll_period, gridgap;

	/* Timer and polling grid data */
	float adjusted_period;		// actual inter-request grid period used [s]
	float starve_ratio = 1.0;	// placeholder for all servers. Not yet set by algo.
	int maxattempts, retry;

	/* Packet stuff */
	struct ntp_pkt spkt;
	struct ntp_pkt rpkt;
	unsigned int socklen;
	socklen_t tvlen;
	int ret;
	struct timeval tv;
	double timeout, newtimeout;		// timeout in [s]

	JDEBUG

	socklen = sizeof(struct sockaddr_in);
	tvlen = (socklen_t) sizeof(struct timeval);
	algodata = handle->algodata;

	/* Sleep until alarm for next grid point goes off */
	pthread_mutex_lock(&alarm_mutex);
	pthread_cond_wait(&alarm_cwait, &alarm_mutex);
	sID = handle->lastalarm_sID;		// discover the server of this grid point
	verbose(VERB_DEBUG, "Alarm caught for server %d", sID);
	pthread_mutex_unlock(&alarm_mutex);

	/* Set to data for this server */
	client = &handle->ntp_client[sID];
	server = &handle->ntp_server[sID];
	timerid = &ntpclient_timerid[sID];
	timeout = ntpclient_timeout[sID];
	rad_error = &handle->rad_error[sID];
	state = &algodata->state[sID];
	poll_period = (float) handle->conf->poll_period;	// upgrade after
	gridgap = poll_period / handle->nservers;	// currently universal, hard to generalise

	/* Update adjusted_period [ the actual period used by timers ]
	 *  - startup burst (using ntpd's BURST (#of pkts), BURST_DELAY (interval) )
	 *  - sync algo as a function of starvation (not yet implemented)
	 */
	if (server->burst > 0) {
		server->burst -= 1;
		if (server->burst == 1)	server->burst = 0;	// since next firing locked in
		adjusted_period = MIN(BURST_DELAY,poll_period);
	} else
		adjusted_period = poll_period / starve_ratio;
	/* Reset sending grid if needed (1-2ms hiccup). Impacts after next firing. */
	if (adjusted_period != ntpclient_period[sID]) {
		assess_ptimer(*timerid, adjusted_period);	// TODO: change name of fn
		ntpclient_period[sID] = adjusted_period;
	}

	/* Control the retry code.
    * If poll period is small, then data is plentiful for this server and we
    * disable retries (set maxattempts = 1) to avoid complexity and risk of
    * overlapping future grid points of this server.
    * If grid density is higher overall, we disable retries to ensure we can
    * complete this grid pt before the next one arrives.
    * Otherwise, apply conservative controls to ensure all retries (successful
    * or not) will complete before the next grid point of this server.
	 * This does not ensure that responses can actually return within these
	 * timeouts (ie RTT<timeout). That is ensured by timeout setting code below.
	 */
	maxattempts = 3;		// if pure loss, 3 enough to get response with high prob
	if (adjusted_period < 4 || gridgap < 3 )
		maxattempts = 1;
	else
		if (maxattempts > adjusted_period/timeout - 1) {   // -1 provides a buffer
			maxattempts = MAX(1, (int)(adjusted_period/timeout)-1);
			verbose(LOG_NOTICE, "NTPclient: reducing max retry attempts on this "
						"packet to %d to avoid stamp overlap (timeout %3.1lf [ms])",
						maxattempts, 1e3*timeout);
		}


	/* The retry loop covers the special case of no retries (maxattempts=1).
	 * In all cases, the first `retry' first sinks outstanding responses waiting
	 * on the socket, cleaning the slate for the new reponse of the request just
	 * made which has yet to arrive. If a retry is attempted, a further blocking
	 * wait is made with timeout calibrated based on an inflated minRTT.
	 * If not, then we are done.
	 *
	 * The goal of retries is to `replace` a lost stamp, to help avoid starving the
	 * algo due to loss or unavailability. This is particularly important
	 * if poll_period high, where stamps are rare.
	 * A small value is good:  adequate for pure loss effects, and more
	 * is pointless if the server is seriously unreachable/unavailable.
	 * Retries are NOT copies, but a freshly created pkt from create_ntp_request,
	 * which moreover ensures uniqueness of the xmt nonce. It one retries but
	 * the earlier attempts do end up arriving, the result is two or more
	 * distinct stamps sent in close proximity (and potentially overlapping).
	 * The stamp queue logic can handle all cases.
	 * The final retry is received with a nonblocking recvfrom: we do not wait
	 * to see if it succeeds.
	 */
	retry = maxattempts;
	while (retry > 0) {
		
		/* Create and send an NTP packet */
		ret = create_ntp_request(handle, &spkt, &tv);
		if (ret)
			continue;	// retry never decremented ==> inf loop if create always fails!

		ret = sendto(client->socket,
				(char *)&spkt, LEN_PKT_NOMAC /* No auth */, 0,
				(struct sockaddr *)&client->s_to, socklen);
		if (ret < 0) {
			verbose(LOG_ERR, "NTPclient: NTP request failed, sendto: %s", strerror(errno));
			return (1);
		}

		verbose(VERB_DEBUG, "Sent NTP request to %s at %lu.%lu with id %llu with maxattempts = %d",
				inet_ntoa(client->s_to.sin_addr), tv.tv_sec, tv.tv_usec,
				((uint64_t) ntohl(spkt.xmt.l_int)) << 32 |
				(uint64_t) ntohl(spkt.xmt.l_fra),
				maxattempts);

		/* Sink any response packets that are already waiting.
		 * This includes those from pkts assumed lost following retry timeout. */
		do {
			ret = recvfrom(client->socket, &rpkt, sizeof(struct ntp_pkt), MSG_DONTWAIT,
				(struct sockaddr*)&client->s_from, &socklen);

			if (ret > 0)
				verbose(VERB_DEBUG, "Received NTP reply from %s, id %llu from prior gridpt",
					inet_ntoa(client->s_from.sin_addr),
					((uint64_t) ntohl(rpkt.org.l_int)) << 32 |
					(uint64_t) ntohl(rpkt.org.l_fra));
		} while (ret > 0);

		/* In case of retries, recvfrom with timeout aiming to catch the response
		 * of the pkt just sent. */
		if (retry > 1) {
			ret = recvfrom(client->socket, &rpkt, sizeof(struct ntp_pkt), MSG_WAITALL,
					(struct sockaddr*)&client->s_from, &socklen);

			if (ret > 0) {
				verbose(VERB_DEBUG, "Received NTP reply from %s, id %llu, on attempt %d",
					inet_ntoa(client->s_from.sin_addr),
					((uint64_t) ntohl(rpkt.org.l_int)) << 32 |
					(uint64_t) ntohl(rpkt.org.l_fra), maxattempts-retry+1);

				if (unmatched_ntp_pair(&spkt, &rpkt))
					verbose(VERB_DEBUG, "  response is not matching ");
				else {
					verbose(VERB_DEBUG, "  response matches ");
					break;
				}
			} else
				verbose(VERB_DEBUG, "No response (%3.lf [ms]) on attempt %d",
					 1e3*timeout, maxattempts-retry+1);
		}

		retry--;
	}

	/* In the case a socket timeout is needed, update its value to adjust
	 * to client<-->server path conditions.
	 * The timeout must be large enough to cover almost all actual RTTs.
	 * If it is too small, unnecessary retries will be transmitted as the
	 * original response was not in fact lost.
	 *
	 * The initial timeout is set to NTP_INTIAL_SO_TIMEOUT [mus] in case the
	 * server is far away, before the algo has a chance to measure RTT reliably.
	 * Subsequently a value based on inflating the minRTT is used.
	 * A lower bound of NTP_MIN_SO_TIMEOUT [mus] ensures robustness.
	 # The upper bound ensures timeouts stay clear of the grid period:
	 * if the client can't finish with this grid point before the next arrives,
	 * it will not be able to send the next one on time!
	 * The problematic case (which can arise in server survey experiments) is
	 * that of small poll_period yet very large RTT, eg (pp,RTT)=(1,600ms)s =>
	 * newtimeout=1.2s > pp .
	 * In more normal cases (pp,RTT)=(16,50ms)s => newtimeout=200ms << pp  :
	 * the grid points are distant from each other and interference is not an
	 * issue.
	 * This timeout is not about avoiding running into the next grid point in the
	 * multiple server setting. It is a single server idea concerned with catching
	 * the response pkt, required for the retry code if enabled.
	 */
	if (maxattempts > 1)		// ie, if retries are activated
		if (algodata && state->stamp_i>0) {
			newtimeout = MIN(1, 2*rad_error->min_RTT);
			if (newtimeout * 1e6 < NTP_MIN_SO_TIMEOUT)
				newtimeout = NTP_MIN_SO_TIMEOUT * 1e-6;
			if (newtimeout > adjusted_period * 0.7)
				newtimeout = adjusted_period * 0.7;	// ==> maxattempts=1 next time

			if ( fabs(newtimeout - timeout) > 4e-3 ) {	// skip trivial updates
				tv.tv_sec = (time_t)newtimeout;
				tv.tv_usec = (useconds_t)(1e6 * (newtimeout - (time_t)newtimeout));
				setsockopt(client->socket, SOL_SOCKET, SO_RCVTIMEO, (void *)(&tv), tvlen);
				ntpclient_timeout[sID] = newtimeout;
				verbose(VERB_DEBUG, "NTPclient: Adjusting NTP client socket timeout "
				"for server %d from %3.0lf to %3.0lf [ms]", sID, 1e3*timeout, 1e3*newtimeout);
			}
		}

	return (0);
}

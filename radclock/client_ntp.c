/*
 * Copyright (C) 2006-2012, Julien Ridoux and Darryl Veitch
 * Copyright (C) 2013-2017, Darryl Veitch <darryl.veitch@uts.edu.au>
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
#ifdef HAVE_POSIX_TIMER
#include <sys/time.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
//#include <netinet/in.h>

#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "radclock.h"
#include "radclock-private.h"
#include "radclock_daemon.h"
#include "misc.h"
#include "verbose.h"
#include "sync_history.h"
#include "sync_algo.h"
#include "config_mgr.h"
#include "proto_ntp.h"
#include "pthread_mgr.h"
#include "jdebug.h"

#define NTP_MIN_SO_TIMEOUT 150000		/* units of [mus] */
#define NTP_INTIAL_SO_TIMEOUT 500000	/* very large in case RTT large */

/*
 * POSIX timer and signal catching mask
 * This requires FreeBSD 7.0 and above for POSIX timers.
 * Also, sigsuspend does not work on Linux in a multi-thread environment
 * (apparently) so use pthread condition wait to sync the thread to SIGALRM
 */
#ifdef HAVE_POSIX_TIMER
timer_t ntpclient_timerid;
#endif
extern pthread_mutex_t alarm_mutex;
extern pthread_cond_t alarm_cwait;



int
ntp_client_init(struct radclock_handle *handle)
{
	/* Socket data */
	struct hostent *he;
	struct timeval so_timeout;

	/* Signal catching */
	struct sigaction sig_struct;
	sigset_t alarm_mask;

	/* Do we have what it takes? */
	if (strlen(handle->conf->time_server) == 0) {
		verbose(LOG_ERR, "No NTP server specified, I cannot be a client!");
		return (1);
	}

	/* Build server infos */
	NTP_CLIENT(handle)->s_to.sin_family = PF_INET;
	NTP_CLIENT(handle)->s_to.sin_port = ntohs(handle->conf->ntp_upstream_port);
	if((he=gethostbyname(handle->conf->time_server)) == NULL) {
		herror("gethostbyname");
		return (1);
	}
	NTP_CLIENT(handle)->s_to.sin_addr.s_addr = *(in_addr_t *)he->h_addr_list[0];

	/* Create the socket */
	if ((NTP_CLIENT(handle)->socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return (1);
	}

	/*
	 * Set a timeout on the recv side to avoid blocking for lost packets.
	 * Set to large value well over expected congested RTT.
	 */
	so_timeout.tv_sec = 0;
	so_timeout.tv_usec = NTP_INTIAL_SO_TIMEOUT;
	setsockopt(NTP_CLIENT(handle)->socket, SOL_SOCKET,
			SO_RCVTIMEO, (void *)(&so_timeout), sizeof(struct timeval));

	/* Initialise the signal data */
	sigemptyset(&alarm_mask);
	sigaddset(&alarm_mask, SIGALRM);
	sig_struct.sa_handler = catch_alarm; /* Not so dummy handler */
	sig_struct.sa_mask = alarm_mask;
	sig_struct.sa_flags = 0;
	sigaction(SIGALRM, &sig_struct, NULL);

	/* Initialize mutex and condition variable objects */
	pthread_mutex_init(&alarm_mutex, NULL);
	pthread_cond_init(&alarm_cwait, NULL);

	/* Create the NTP request periodic sending grid */
#ifdef HAVE_POSIX_TIMER
	/* CLOCK_REALTIME_HR does not exist on FreeBSD */
	if (timer_create(CLOCK_REALTIME, NULL, &ntpclient_timerid) < 0) {
		verbose(LOG_ERR, "ntp_init: POSIX timer create failed");
		return (1);
	}
	if (set_ptimer(ntpclient_timerid, 0.5 /* !0 */,
				(float) handle->conf->poll_period) < 0) {
		verbose(LOG_ERR, "ntp_init: POSIX timer cannot be set");
		return (1);
	}
#else
	if (set_itimer(0.5 /* !0 */, (float) handle->conf->poll_period) < 0) {
		verbose(LOG_ERR, "ntp_init: itimer cannot be set");
		return (1);
	}
#endif
	return (0);
}


/* Create a fresh NTP request pkt.
 * Each call will return a unique pkt since the vcounter is read afresh.
 * Potential uniqueness failure due to limited resolution is dealt with.
 * NTP timestamp fields are in NTP format, requiring UTC --> NTP conversion.
 *   epoch:		  (NTP,UTC) = Jan 1 (1900,1970)        NTP = UTC + JAN_1970
 *   frac format:(NTP,UTC) = (#(2^-32)s, #mus)        NTP = UTC*1e-6 *2^32
 */
static int
create_ntp_request(struct radclock_handle *handle, struct ntp_pkt *pkt,
		struct timeval *xmt)
{
	struct timeval reftime;
	long double time;
	vcounter_t vcount;
	int err;

	static long double last_xmt = 0.0;			// used to let first packet through
	static struct timeval last_nonce = {0,0};	// used to ensure nonce is unique

	pkt->li_vn_mode	= PKT_LI_VN_MODE(LEAP_NOTINSYNC, NTP_VERSION, MODE_CLIENT);
	pkt->stratum		= STRATUM_UNSPEC;
	pkt->stratum		= NTP_SERVER(handle)->stratum + 1;
	pkt->ppoll			= NTP_MINPOLL;
	pkt->precision		= -6;		/* Like ntpdate */
	pkt->rootdelay		= htonl(FP_SECOND);
	pkt->rootdispersion	= htonl(FP_SECOND);
	pkt->refid			= htonl(NTP_SERVER(handle)->refid);

	/* Reference time [ time client clock was last updated ] */
	vcount = RAD_DATA(handle)->last_changed;
	read_RADabs_UTC(&handle->rad_data, &vcount, &time);
	timeld_to_timeval(&time, &reftime);
	pkt->reftime.l_int = htonl(reftime.tv_sec + JAN_1970);
	pkt->reftime.l_fra = htonl(reftime.tv_usec * 4294967296.0 / 1e6);

	// TODO: need a more symmetric version of the packet exchange?
	pkt->org.l_int		= 0;
	pkt->org.l_fra		= 0;
	pkt->rec.l_int		= 0;
	pkt->rec.l_fra		= 0;

	/* Transmit time (also used as a nonce for packet matching) */
	err = radclock_get_vcounter(handle->clock, &vcount);
	if (err < 0)
		return (1);
	read_RADabs_UTC(&handle->rad_data, &vcount, &time);

	/* Sanity test for apparently identical pkts:  should be impossible */
	//verbose (LOG_ERR, "last_xmt = %10.9Lf, time = %10.9Lf, vcount= %llu", last_xmt, time, (long long unsigned) vcount);
	if (last_xmt > 0 && time == last_xmt) {	// subsequent pkts have same ld timestamp
		verbose(LOG_ERR, "xmt = last_xmt !! pkt id not unique! last_xmt =%10.9Lf,"
			" time = %10.9Lf, vcount= %llu",
			last_xmt, time, (long long unsigned) vcount);
		return (1);  // return to prevent infinite loop in this insane case
	/* This can provoke an infinite loop! under a naturally occurring condition after a full
	   kernel rebuild, where no old RTC value (I guess) triggering the wierd `bogus' event of
		get_kernel_ffclock (which by the way is just verbosity, with no real effect, needs fixing)
		Need to understand bogus and either fix, or test for it here so 'time' advances.
		Need to avoid the infinite loop possibility, in which case, need to see if the
		return statement should be put back in, or remove this sanity test completely.
		Already have test below for non-unique nonce no matter what.
	   After some initialization, all symptoms dissappear. Note that after reboot also get the bonus
	   ERROR, but this bug does not occur.
	 */
	}

	timeld_to_timeval(&time, xmt);
	//verbose(LOG_NOTICE, "last nonce: %10ld.%6lu [s]", last_nonce.tv_sec, last_nonce.tv_usec);
	//verbose(LOG_NOTICE, "xmt  nonce: %10ld.%6lu [s]", xmt->tv_sec, xmt->tv_usec);

	/* Check, and ensure, that xmt timestamp unique, needed for use as a nonce */
	if ( last_xmt > 0 && xmt->tv_sec == last_nonce.tv_sec
						&& xmt->tv_usec == last_nonce.tv_usec) {
		verbose(LOG_WARNING, "xmt nonce not unique: %10ld.%6lu [s], vcount= %llu",
			xmt->tv_sec, xmt->tv_usec, (long long unsigned) vcount);
		xmt->tv_usec += 1;	// TODO: treat overflow case properly
		verbose(LOG_WARNING,"    nonce fraction increased to %7lu [s]", xmt->tv_usec);
	}

	pkt->xmt.l_int = htonl(xmt->tv_sec + JAN_1970);
	pkt->xmt.l_fra = htonl(xmt->tv_usec * 4294967296.0 / 1e6);

	last_xmt = time;
	last_nonce = *xmt;
	
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
		return (1);
}


/* Transmit the NTP response packet.  Incorporates
 *   - "adjusted_period" to enable variations on sending grid
 *   - "maxattempts" to allow retries in case of missing responses.
 *		  Each attempt is unique, with a distinct xmt nonce field
 *   - "timeout" to control socket timeout as a function of RTT conditions
 */
int
ntp_client(struct radclock_handle *handle)
{
	/* Timer and polling grid data */
	float adjusted_period;		// actual inter-request period used [s]
	float starve_ratio;
	int maxattempts, retry;

	/* Packet stuff */
	struct ntp_pkt spkt;
	struct ntp_pkt rpkt;
	unsigned int socklen;
	socklen_t tvlen;
	int ret;

	struct bidir_peer *peer;
	struct timeval tv;
	double timeout;

	JDEBUG

	peer = (struct bidir_peer *)handle->active_peer;
	starve_ratio = 1.0;	// placeholder
	maxattempts = 3;		// if pure loss, 3 enough to get response with high prob
	socklen = sizeof(struct sockaddr_in);
	tvlen = (socklen_t) sizeof(struct timeval);

	/* Use adjusted_period to modulate sending grid (nominally poll_period) as:
	 *  - startup burst (using ntpd's BURST (#of pkts), BURST_DELAY (interval) )
	 *  - sync algo as a function of starvation (not yet implemented)
	 */
	if (NTP_SERVER(handle)->burst > 0) {
		NTP_SERVER(handle)->burst -= 1;
		adjusted_period = MIN(BURST_DELAY,handle->conf->poll_period);
	} else {
		// TODO: implement logic for starvation ratio in sync algo
		adjusted_period = handle->conf->poll_period / starve_ratio;
	}

	/* Limit maxattempts to ensure they all (whether successful or not) will
	 * complete before the next grid point, to reduce complexity.
	 * Calculation assumes RTT < timeout.
	 */
	getsockopt(NTP_CLIENT(handle)->socket, SOL_SOCKET, SO_RCVTIMEO,
			(void *)(&tv), &tvlen);
	timeout = tv.tv_sec + 1e-6 * tv.tv_usec;
	if (maxattempts > adjusted_period/timeout - 1) {   // -1 provides a buffer
		maxattempts = MAX(1, (int)(adjusted_period/timeout)-1);
		verbose(VERB_DEBUG, "SO_TIMEOUT too big at %3.1lf [ms], reducing max"
			" attempts to %d", 1e3*timeout, maxattempts);
	}

	/* Reset sending grid interval if needed (get 1-2ms hiccup if reset) */
#ifdef HAVE_POSIX_TIMER
	assess_ptimer(ntpclient_timerid, adjusted_period);
#else
	assess_itimer(adjusted_period);
#endif

	/* Sleep until alarm for next grid point goes off */
	pthread_mutex_lock(&alarm_mutex);
	pthread_cond_wait(&alarm_cwait, &alarm_mutex);
	pthread_mutex_unlock(&alarm_mutex);

	/* Make maxattempts to obtain (request,response) pair if encounter timeouts,
	 * to help avoid starving the algo due to loss or unavailability.
	 * Particularly important if poll_period high, where stamps are rare.
	 * A small value is good:  adequate for pure loss effects, and more
	 * is pointless if the server is unreachable/unavailable.
	 * Retries are NOT copies, but a freshly created pkt from create_ntp_request,
	 * which moreover ensures uniqueness of the xmt nonce.
	 */
	retry = maxattempts;
	while (retry > 0) {
		
		/* Create and send an NTP packet */
		ret = create_ntp_request(handle, &spkt, &tv);
		if (ret)
			continue;	// retry never decremented ==> inf loop if create fails!

		ret = sendto(NTP_CLIENT(handle)->socket,
				(char *)&spkt, LEN_PKT_NOMAC /* No auth */, 0,
				(struct sockaddr *) &(NTP_CLIENT(handle)->s_to), socklen);

		if (ret < 0) {
			verbose(LOG_ERR, "NTP request failed, sendto: %s", strerror(errno));
			return (1);
		}

		verbose(VERB_DEBUG, "Sent NTP request to %s at %lu.%lu with id %llu",
				inet_ntoa(NTP_CLIENT(handle)->s_to.sin_addr), tv.tv_sec, tv.tv_usec,
				((uint64_t) ntohl(spkt.xmt.l_int)) << 32 |
				(uint64_t) ntohl(spkt.xmt.l_fra));

		/* This will block then timeout if nothing received */
		ret = recvfrom(NTP_CLIENT(handle)->socket,
				&rpkt, sizeof(struct ntp_pkt), 0,
				(struct sockaddr*)&NTP_CLIENT(handle)->s_from, &socklen);

		/* If dont find a response within timeout, try again.
		 * If response doesn't match request, issue warning. It is likely that it
		 * is due to delayed PCAP captures (a response from an earlier request),
		 * rather than a lost packet
		 * All packets will eventually be caught and stamp matching code
		 * in DATA_PROC will deal with out-of-order and other complexities.
		 * The goal here is to manage timeouts and have effective retries in the
		 * common loss case, not to be a foolproof pkt matcher.
		 */
		if (ret > 0) {
			verbose(VERB_DEBUG, "Received NTP reply from %s, id %llu, on attempt %d",
				inet_ntoa(NTP_CLIENT(handle)->s_from.sin_addr),
				((uint64_t) ntohl(rpkt.org.l_int)) << 32 |
				(uint64_t) ntohl(rpkt.org.l_fra), maxattempts-retry+1);

			if (unmatched_ntp_pair(&spkt, &rpkt))
				verbose(LOG_WARNING, "NTP client got a non matching pair. "
						"Try increasing socket timeout");
			break;
		}
		else
			verbose(VERB_DEBUG, "Socket timed out after %.3lf [ms] on attempt %d",
				 1e3*timeout, maxattempts-retry+1);

		retry--;
	}

	/*
	 * Update socket timeout to adjust to server conditions. Athough the delay
	 * may be large, the jitter is usually fairly low (< 1ms). Give an extra 5ms
	 * to cover ugly cases. Make sure we never go below the minimum socket
	 * timeout value, and bound upper values.
	 */
	if (peer) {
		timeout = peer->RTThat * RAD_DATA(handle)->phat + 5e-3; // minRTT + buffer
		if (timeout * 1e6 < NTP_MIN_SO_TIMEOUT)
			timeout = NTP_MIN_SO_TIMEOUT * 1e-6;
		if (timeout > adjusted_period / 3)
			timeout = adjusted_period / 3;

		tv.tv_sec = (time_t)timeout;
		tv.tv_usec = (useconds_t)(1e6 * timeout - (time_t)timeout);
		setsockopt(NTP_CLIENT(handle)->socket, SOL_SOCKET, SO_RCVTIMEO,
				(void *)(&tv), tvlen);
		verbose(VERB_DEBUG, "Adjusting NTP client socket timeout to %.3lf [ms]",
				1e3 * timeout);
	}

	return (0);
}


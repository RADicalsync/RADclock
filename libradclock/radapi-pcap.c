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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

//#include <pcap.h>

#include <radclock.h>
#include "radclock-private.h"
#include "logger.h"


/* The value of mode passed here can be overridden by descriptor_set_tsmode
 * before being recorded in clock.
 */
int
pktcap_set_tsmode(struct radclock *clock, pcap_t *p_handle, pktcap_tsmode_t mode, u_int custom)
{

	if (clock == NULL) {
		logger(RADLOG_ERR, "Clock handle is null, can't set mode");
		return (-1);
	}

	/* working from non-live capture return silently */
	if (pcap_fileno(p_handle) == -1) {
		return (0);
	}

	switch (mode) {
		case PKTCAP_TSMODE_NOMODE:
			logger(RADLOG_NOTICE, "Requesting pkt timestamping mode PKTCAP_TSMODE_NOMODE");
			break;
		case PKTCAP_TSMODE_SYSCLOCK:
			logger(RADLOG_NOTICE, "Requesting pkt timestamping mode PKTCAP_TSMODE_SYSCLOCK");
			break;
		case PKTCAP_TSMODE_FBCLOCK:
			logger(RADLOG_NOTICE, "Requesting pkt timestamping mode PKTCAP_TSMODE_FBCLOCK");
			break;
		case PKTCAP_TSMODE_FFCLOCK:
			logger(RADLOG_NOTICE, "Requesting pkt timestamping mode PKTCAP_TSMODE_FFCLOCK");
			break;
		case PKTCAP_TSMODE_FFNATIVECLOCK:
			logger(RADLOG_NOTICE, "Requesting pkt timestamping mode PKTCAP_TSMODE_FFNATIVECLOCK");
			break;
		case PKTCAP_TSMODE_FFDIFFCLOCK:
			logger(RADLOG_NOTICE, "Requesting pkt timestamping mode PKTCAP_TSMODE_FFDIFFCLOCK");
			break;
		case PKTCAP_TSMODE_RADCLOCK:
			logger(RADLOG_NOTICE, "Requesting pkt timestamping mode PKTCAP_TSMODE_RADCLOCK");
			break;
		case PKTCAP_TSMODE_CUSTOM:
			logger(RADLOG_NOTICE, "Requesting pkt timestamping mode PKTCAP_TSMODE_CUSTOM");
			break;
			
	}
	
	/* Call to system specific method to set the bpf ts type */
	if (descriptor_set_tsmode(clock, p_handle, (int *)&mode, custom))
		return (-1);

	clock->tsmode = mode;

	return (0);
}



int
pktcap_get_tsmode(struct radclock *clock, pcap_t *p_handle, pktcap_tsmode_t *mode)
{
	int inferredmode;  // can remove this if descriptor_get_tsmode wont set if error
	
	if (clock == NULL) {
		logger(RADLOG_ERR, "Clock handle is null, can't set mode");
		return (-1);
	}
	
	/* Call to system specific method to get the mode */
	inferredmode = 0;
	if (descriptor_get_tsmode(clock, p_handle, &inferredmode))
		return (-1);

	*mode = (pktcap_tsmode_t) inferredmode;		// only set if get safe value

	switch(inferredmode) {
	case PKTCAP_TSMODE_SYSCLOCK:
		logger(RADLOG_NOTICE, "Capture mode consistent with SYSCLOCK");
		break;
	case PKTCAP_TSMODE_FBCLOCK:
		logger(RADLOG_NOTICE, "Capture mode consistent with FBCLOCK");
		break;
	case PKTCAP_TSMODE_FFCLOCK:
		logger(RADLOG_NOTICE, "Capture mode consistent with FFCLOCK");
		break;
	case PKTCAP_TSMODE_FFNATIVECLOCK:
		logger(RADLOG_NOTICE, "Capture mode consistent with FFNATIVECLOCK");
		break;
	case PKTCAP_TSMODE_FFDIFFCLOCK:
		logger(RADLOG_NOTICE, "Capture mode consistent with FFDIFFCLOCK");
		break;
	case PKTCAP_TSMODE_RADCLOCK:
		logger(RADLOG_NOTICE, "Capture mode consistent with PKTCAP_TSMODE_RADCLOCK");
		break;
	default:
		logger(RADLOG_ERR, "Capture mode not a recognized preset, custom I hope!");
	}

	return (0);
}


struct routine_priv_data
{
	struct radclock *clock;
	pcap_t *p_handle;
	struct pcap_pkthdr *pcap_header;
	unsigned char *packet;
	vcounter_t *vcount;
	struct timeval *ts;
	int ret;
};


/* Callback for pcap_loop
 * Compare the standard parameter list with the callback used in the daemon :
 * void fill_rawdata_pcap(u_char *c_handle, const struct pcap_pkthdr *pcap_hdr,
		const u_char *packet_data)
 */
void kernelclock_routine(u_char *user, const struct pcap_pkthdr *phdr, const u_char *pdata)
{
	struct routine_priv_data *data = (struct routine_priv_data *) user;
	//memcpy(data->pcap_header, phdr, sizeof(struct pcap_pkthdr));
	data->ret = extract_vcount_stamp(data->clock, data->p_handle, phdr, pdata,
												data->vcount, data->pcap_header);
	data->packet = (unsigned char*)pdata;
	
	if (data->clock->tsmode == PKTCAP_TSMODE_RADCLOCK) {
		logger(LOG_WARNING, "Ignoring tsmode PKTCAP_TSMODE_RADCLOCK, timestamp"
		"in pcap hdr not overridden. User radclocks should just read the radclock"
		" using the vcount returned with pcap.");
	}
	
	/* Now the corrected timestamp is in the header copy, can extract it */
	memcpy(data->ts, &(data->pcap_header->ts), sizeof(struct timeval));
	//memcpy(data->ts, &(phdr->ts), sizeof(struct timeval));
}


// TODO check if this the right pcap_* to provide (packet per packet ??)
/* Ugly stuff ?
 * Because of the integration of the user version of the clock, we need to know
 * which routine to call here. Also this function is exported to the user API
 * so that anybody can call the pcap oriented capture while receovering the
 * vcount padded in the pcap header.
 * No other choice than having a clock handle as a parameter input ...
 */
int radclock_get_packet( struct radclock *clock, 
						pcap_t *p_handle, 
						struct pcap_pkthdr *header, 
						unsigned char **packet, 
						vcounter_t *vcount_ptr,
						struct timeval *ts_ptr)
{
	/* Assemble the user data pcap_loop will pass to our callback function */
	struct routine_priv_data data =
	{
		.clock			= clock,
		.p_handle		= p_handle,
		.pcap_header	= header,
		.vcount			= vcount_ptr,
		.ts				= ts_ptr,
		.ret				= 0,
		.packet			= NULL,
	};
	
	/* Need to call the low level pcap_loop function to be able to pass our 
	 * own callback, needed in order to extract the vcount timestamp */
	int err;
	err = pcap_loop(p_handle, 1 /* pkt */, kernelclock_routine, (u_char *) &data);
	*packet = data.packet;

	/* Error can be -1 (read error) or -2 (explicit loop break) */
	if (err < 0) {
		perror("pcap_loop:");
		return (err);
	}
	if (data.ret) {
		logger(LOG_ERR, "extract_vcount_stamp error");
		return (-1);
	}
	return (0);
}


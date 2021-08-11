/*
 * Copyright (C) 2006-2012, Julien Ridoux and Darryl Veitch
 * Copyright (C) 2006-2007, Thomas Young <tfyoung@gmail.com>
 * Copyright (C) 2013-2021, Darryl Veitch <darryl.veitch@uts.edu.au>
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
#ifdef WITH_FFKERNEL_LINUX

#include <asm/types.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <string.h>

#include "radclock.h"
#include "radclock-private.h"
#include "logger.h"

/* Here you go, some dirty tricks */
//#include <linux/version.h>

// Not good, also includes linux/clocksource.h which you dont want to be user accessible
//#include <linux/radclock.h>

/* IOCTLs for tsmode get/set and timespec timestamps on socket */
#define DETECTED_KERNEL_RADCLOCK_IOCTL 1	// default: assume ioctl already defined
#ifndef SIOCSRADCLOCKTSMODE
#define DETECTED_KERNEL_RADCLOCK_IOCTL 0
#define SIOCSRADCLOCKTSMODE 	0x8908		// defined in uapi/asm-generic/sockios.h
#define SIOCGRADCLOCKTSMODE 	0x8909		// defined in uapi/asm-generic/sockios.h
#define SIOCGRADCLOCKSTAMP 	0x89b2		// defined in uapi/linux/sockios.h
#endif


/* Ensure definition of ffclock-side tsmode language
 * Originally these were also the daemon modes, but daemon side now PKTCAP_TSMODE_..
 */
//#if defined (HAVE_SYS_RADCLOCK_H)
//#include <sys/radclock.h>			// TODO: when included in userland, restrict defns to needed only
//#else
#define RADCLOCK_TSMODE_SYSCLOCK	1		// leave normal pcap ts field, hide vcount in padded hdr
#define RADCLOCK_TSMODE_RADCLOCK	2		// fill ts with RADclock Abs clock, hide vcount in hdr
#define RADCLOCK_TSMODE_FAIRCOMPARE	3	// as _SYSCLOCK, but ts and raw timestamps back-to-back
//#endif


/* Verbose decoding of kernel tsmodes modes.
 * Currently trivial, as the kernel modes are radclock specific.
 */
static void
decode_ffclock_tsflags_KV1(long bd_tstamp)
{
		logger(RADLOG_NOTICE, "Decoding af_packet ffclock timestamp mode (bd_tstamp = 0x%04x)",
				 bd_tstamp);

		switch (bd_tstamp) {
		case RADCLOCK_TSMODE_SYSCLOCK:
			logger(RADLOG_NOTICE, "     tsmode = RADCLOCK_TSMODE_SYSCLOCK");
			break;
		case RADCLOCK_TSMODE_RADCLOCK:
			logger(RADLOG_NOTICE, "     tsmode = RADCLOCK_TSMODE_RADCLOCK");
			break;
		case RADCLOCK_TSMODE_FAIRCOMPARE:
			logger(RADLOG_NOTICE, "     tsmode = RADCLOCK_TSMODE_FAIRCOMPARE");
			break;
		default:
			logger(RADLOG_NOTICE, "     tsmode = %ld (unrecognised)", bd_tstamp);
			break;
		}
}


int
descriptor_set_tsmode(struct radclock *handle, pcap_t *p_handle, int *mode, u_int custom)
{
	long bd_tstamp = 0;
	long override_mode = *mode;		// record input mode

	if ( DETECTED_KERNEL_RADCLOCK_IOCTL )
		logger(RADLOG_ERR, "have DETECTED_KERNEL_RADCLOCK_IOCTL !");
	else
		logger(RADLOG_ERR, "DETECTED_KERNEL_RADCLOCK_IOCTL failed, defining them here");


	switch (handle->kernel_version) {
	case 0:

	case 1:
		/* Initial test of value of tsmode */
//		logger(RADLOG_NOTICE, "Checking current tsmode before setting it :");
//		if (ioctl(pcap_fileno(p_handle), SIOCGRADCLOCKTSMODE, (caddr_t)(&bd_tstamp)) == -1)
//			logger(RADLOG_ERR, "Getting initial timestamping mode failed: %s", strerror(errno));
//		decode_ffclock_tsflags_KV1(bd_tstamp);

		switch (*mode) {
		case PKTCAP_TSMODE_SYSCLOCK:
			bd_tstamp = PKTCAP_TSMODE_SYSCLOCK;
			break;
		case PKTCAP_TSMODE_FBCLOCK:
		case PKTCAP_TSMODE_FFCLOCK:
		case PKTCAP_TSMODE_FFDIFFCLOCK:
		logger(RADLOG_ERR, "Requested mode unsupported: "
				"switching to PKTCAP_TSMODE_RADCLOCK (ts supplied by radclock)");
			override_mode = PKTCAP_TSMODE_RADCLOCK;
		case PKTCAP_TSMODE_RADCLOCK:
		case PKTCAP_TSMODE_FFNATIVECLOCK:	// should have kernel = radclock, enables comparison
			bd_tstamp = RADCLOCK_TSMODE_RADCLOCK ;
			break;
		case PKTCAP_TSMODE_CUSTOM:
			bd_tstamp = custom;
			break;
		default:
			logger(RADLOG_ERR, "descriptor_set_tsmode: unknown timestamping mode.");
			return (1);
		}
		*mode = override_mode;	// inform caller of final mode
		logger(RADLOG_ERR, "descriptor_set_tsmode: bd_tstamp set to %ld", bd_tstamp);

		if (ioctl(pcap_fileno(p_handle), SIOCSRADCLOCKTSMODE, (caddr_t)(&bd_tstamp)) == -1) {
			logger(RADLOG_ERR, "Setting timestamping mode failed: %s", strerror(errno));
			return (1);
		}
		break;

	default:
		logger(RADLOG_ERR, "Unknown kernel version");
		return (1);
	}

	return (0);
}


int
descriptor_get_tsmode(struct radclock *handle, pcap_t *p_handle, int *mode)
{
	long bd_tstamp = 0;

	switch (handle->kernel_version) {
	case 0:
	case 1:
		if (ioctl(pcap_fileno(p_handle), SIOCGRADCLOCKTSMODE, (caddr_t)(&bd_tstamp)) == -1) {
			logger(RADLOG_ERR, "Getting timestamping mode failed: %s", strerror(errno));
			return (1);
		}
		//logger(RADLOG_NOTICE, "Decoding returned Linux RADCLOCKTSMODE mode (mode = 0x%04x)", bd_tstamp);
		decode_ffclock_tsflags_KV1(bd_tstamp);

		/* Map back to tsmode intent */
		switch (bd_tstamp) {
		case RADCLOCK_TSMODE_SYSCLOCK:
			*mode = PKTCAP_TSMODE_SYSCLOCK;
			break;
		case RADCLOCK_TSMODE_RADCLOCK:
		case RADCLOCK_TSMODE_FAIRCOMPARE:
			*mode = PKTCAP_TSMODE_FFNATIVECLOCK;
			break;
		default:
			*mode = PKTCAP_TSMODE_NOMODE;
			break;
		}
		break;

	default:
		logger(RADLOG_ERR, "Unknown kernel version");
		return (1);
	}

	return (0);
}


/* Test for the consistency of vcount in padding and pcap header.
 * Gives detailed verbosity -- in case of error only.
 */
//static void
//header_match_test(pcap_t *p_handle, const struct pcap_pkthdr *header,
//		const unsigned char *packet)
//{
//	vcounter_t 	*raw;
//	struct timeval tv;
//
//  /* Examine actual timestamp values, but limit checks to first few packets */
//	logger(RADLOG_NOTICE, "Detailed look at first packets: ");
//
//	/* Simply recover ffcount from bpf header */
//	logger(RADLOG_NOTICE, " bh_ffcounter:  %llu", (long long unsigned)h->bh_ffcounter);
//
//	/* Assuming timeval, compare timestamps */
//	raw = (vcounter_t *) &h->bh_tstamp;	// raw only 64bits, so if timeval is 128, will see which half the cast gets
//	logger(RADLOG_NOTICE, " bh_tstamp:  %lld.%06lld and as cast to raw: %llu",
//		h->bh_tstamp.tv_sec, h->bh_tstamp.tv_usec, (long long unsigned) *raw );
//
//	raw = (vcounter_t *) &(header->ts);	// works regardless of size of timeval, no need to test first, nice
//	logger(RADLOG_NOTICE, " pcap ts:    %lld.%06lld and as cast to raw: %llu",
//		header->ts.tv_sec, header->ts.tv_usec, (long long unsigned) *raw );
//
//
//	/* Translate the sec member to a time and date */
//	struct tm *t;
//	t = localtime(&(tv.tv_sec));
//	const char *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
//	logger(RADLOG_NOTICE, " %s %02d %04d, %02d:%02d:%02d", months[t->tm_mon], t->tm_mday,
//			1900+t->tm_year, t->tm_hour, t->tm_min, t->tm_sec);
//
//}



/* Note on data arguments,:
 *   header:  ptr to the actual pcap_hdr structure owned/supplied by pcap
 *   packet:              "     frame data           "
 *	  vcount:  direct access into the rbd where the extract raw ts is to be stored
 * Trying to make quick with inline, but makes sense?
 */
inline int
extract_vcount_stamp(struct radclock *clock, pcap_t *p_handle,
		const struct pcap_pkthdr *header, const unsigned char *packet,
		vcounter_t *vcount, struct pcap_pkthdr *rdhdr)
{
	int err = 0;

	memcpy(rdhdr, header, sizeof(struct pcap_pkthdr));

	switch (clock->kernel_version) {
	case 0:
	case 1: {
//#if defined(TPACKET_HDRLEN) && defined (HAVE_PCAP_ACTIVATE)  // effectively choose IOCTL
#if defined (HAVE_PCAP_ACTIVATE)	// check PCAP supports memory mapping, assume kernel does  // choose MMAP
		//logger(RADLOG_NOTICE, "using memory mapping, not IOCTL");
 	 	char *bp;
		bp = (char*)packet - sizeof(vcounter_t);	// assume just before frame data
		memcpy(vcount, bp, sizeof(vcounter_t));

#else
		logger(RADLOG_NOTICE, "using IOCTL");
		if (ioctl(pcap_fileno(p_handle), SIOCGRADCLOCKSTAMP, (caddr_t)vcount) == -1) {
			perror("ioctl");
			logger(RADLOG_ERR, "IOCTL failed to get vcount");
			err = 1;
		}
#endif
	}
		break;
	default:
		err = 1;
	}

	/* Test returned vcount */
	/* TODO: make a compile time check, or initialization somehow */
	static int notchecked = 1;		// trigger checks and verbosity once only
	if (notchecked) {
		vcounter_t now = 0;
		radclock_get_vcounter(clock, &now);
		logger(RADLOG_NOTICE, " Counter is now   = %llu", now);
		logger(RADLOG_NOTICE, " Extracted vcount = %llu   (diff = %llu)", *vcount, now - *vcount);
		notchecked = 0;
	}

	if (err) {
		logger(RADLOG_ERR, "Cannot extract raw timestamp from pcap data, "
			"pcap timestamp reads: %ld.%ld", header->ts.tv_sec, header->ts.tv_usec);
		return (1);
	}

	return (0);
}



/* This routine interprets the contents of the timeval-typed ts field from
 * the pcap header according to the linux kernel timestamping options,
 * and converts the timestamp to a long double.
 */
void
ts_format_to_double(struct timeval *pcapts, int tstype, long double *timestamp)
{
	static int limit = 0;					// limit verbosity to once-only

	if (sizeof(struct timeval) < 16)
			fprintf(stderr, "Warning, timeval frac field is only 32 bits! \n");

	*timestamp = pcapts->tv_sec;

	/* FIXME: currently just assuming is a tval, not even using tstype */
	switch (tstype) {
	default:
		if (!limit) fprintf(stdout, "converting assumed microtime\n");
		*timestamp += 1e-6 * pcapts->tv_usec;
		break;
	}

	limit++;
}



#endif

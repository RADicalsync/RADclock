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
#ifdef WITH_FFKERNEL_FBSD

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/module.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#ifdef HAVE_SYS_TIMEFFC_H
#include <sys/timeffc.h>   // TODO, remove this include kclock.h instead
#endif
#include <sys/socket.h>

#include <net/ethernet.h>	// ETHER_HDR_LEN
#include <net/bpf.h>
#include <pcap.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>		// useful?
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stddef.h>		// offesetof macro

#include "radclock.h"
#include "radclock-private.h"
#include "logger.h"



/* XXX Deprecated
 * Old kernel patches for feed-forward support versions 0 and 1.
 * Used to add more IOCTL to the BPF device. The actual IOCTL number depends on
 * the OS version, detected in configure script.
 */

/* for setting radclock timestamping mode */
#ifndef BIOCSRADCLOCKTSMODE
#define BIOCSRADCLOCKTSMODE	_IOW('B', FREEBSD_RADCLOCK_IOCTL + 2, int8_t)
#endif

/* for getting radclock timestamping mode */
#ifndef BIOCGRADCLOCKTSMODE
#define BIOCGRADCLOCKTSMODE	_IOR('B', FREEBSD_RADCLOCK_IOCTL + 3, int8_t)
#endif



/* Ensure all symbols needed for compilation of KV=2,3 related code are defined,
 * (without any overwritting), regardless of compiled kernel
 * where some (or all) may be missing. */

/* Ensure definition of symbols in common to both KV=2 and 3, using KV2 values */
#ifndef	BPF_T_MICROTIME
#define	BPF_T_MICROTIME		0x0000
#define	BPF_T_NANOTIME			0x0001
#define	BPF_T_BINTIME			0x0002
#define	BPF_T_NONE				0x0003
#define	BPF_T_FORMAT_MASK		0x0007
#define	BPF_T_NORMAL			0x0000
#define	BPF_T_MONOTONIC		0x0100
#define	BPF_T_SYSCLOCK			0x0000
#define	BPF_T_FBCLOCK			0x1000
#define	BPF_T_FFCLOCK			0x2000
#define	BPF_T_CLOCK_MASK		0x3000
#define	BPF_T_FORMAT(t)		((t) & BPF_T_FORMAT_MASK)
#define	BPF_T_CLOCK(t)			((t) & BPF_T_CLOCK_MASK)
#define	BIOCGTSTAMP	_IOR('B', 131, u_int)
#define	BIOCSTSTAMP	_IOW('B', 132, u_int)
#endif

/* Ensure definition of symbols unique to KV=2 */
#ifndef	BPF_T_FFCOUNTER
#define	BPF_T_FFCOUNTER		0x0004
#define	BPF_T_FORMAT_MAX		0x0004
#define	BPF_T_FLAG_MASK		0x0100
#define	BPF_T_CLOCK_MAX		0x2000
#define	BPF_T_FLAG(t)			((t) & BPF_T_FLAG_MASK)
#endif
	

/* Ensure definition of symbols unique to KV=3 */
#ifndef	BPF_T_NOFFC
#define	BPF_T_NOFFC				0x0000   // no FFcount
#define	BPF_T_FFC				0x0010   // want FFcount
#define	BPF_T_FFRAW_MASK		0x0010
#define	BPF_T_FAST				0x0100   // UTC,  FAST
#define	BPF_T_MONOTONIC		0x0200	// UPTIME, !FAST Exception, redefined to allow compile
#define	BPF_T_MONOTONIC_FAST	0x0300	// UPTIME,  FAST
#define	BPF_T_FLAVOR_MASK		0x0300
#define	BPF_T_FFNATIVECLOCK	0x3000	// read native FF
#define	BPF_T_FFDIFFCLOCK		0x4000	// read FF difference clock
#define	BPF_T_FFRAW(t)			((t) & BPF_T_FFRAW_MASK)
#define	BPF_T_FLAVOR(t)		((t) & BPF_T_FLAVOR_MASK)
#define	BPF_T_CLOCK_MASK		0x7000	// shouldnt be needed..
#endif




/* The decoding functions display what the given bpf timestamp type descriptor
 * corresponds to, explicity and exhaustively, in terms of its independent
 * components, and fields within each component.
 */
static void
decode_bpf_tsflags_KV2(u_int bd_tstamp)
{
		logger(RADLOG_NOTICE, "Decoding bpf timestamp type _T_ flags (bd_tstamp = %u (0x%04x))",
				 bd_tstamp, bd_tstamp);
	 	switch (BPF_T_FORMAT(bd_tstamp)) {
		case BPF_T_MICROTIME:
			logger(RADLOG_NOTICE, "     FORMAT = MICROTIME");
			break;
		case BPF_T_NANOTIME:
			logger(RADLOG_NOTICE, "     FORMAT = NANOTIME");
			break;
		case BPF_T_BINTIME:
			logger(RADLOG_NOTICE, "     FORMAT = BINTIME");
			break;
		case BPF_T_NONE:
			logger(RADLOG_NOTICE, "     FORMAT = NONE");
			break;
	 	case BPF_T_FFCOUNTER:
			logger(RADLOG_NOTICE, "     FORMAT = FFCOUNTER");
			break;
		}
		
		switch (BPF_T_FLAG(bd_tstamp)) {
		case BPF_T_MONOTONIC:
			logger(RADLOG_NOTICE, "     FLAG = MONOTONIC [uptime]");
			break;
		default:
			logger(RADLOG_NOTICE, "     FLAG = not MONOTONIC [UTC]");
		}
		
		switch (BPF_T_CLOCK(bd_tstamp)) {
		case BPF_T_SYSCLOCK:
			logger(RADLOG_NOTICE, "     CLOCK = SYSCLOCK");
			break;
	 	case BPF_T_FBCLOCK:
			logger(RADLOG_NOTICE, "     CLOCK = FBCLOCK");
			break;
		case BPF_T_FFCLOCK:
			logger(RADLOG_NOTICE, "     CLOCK = FFCLOCK");
			break;
		}
}

static void
decode_bpf_tsflags_KV3(u_int bd_tstamp)
{
		logger(RADLOG_NOTICE, "Decoding bpf timestamp type _T_ flags (bd_tstamp = 0x%04x)",
				 bd_tstamp);
				 
	 	switch (BPF_T_FORMAT(bd_tstamp)) {
		case BPF_T_MICROTIME:
			logger(RADLOG_NOTICE, "     FORMAT = MICROTIME");
			break;
		case BPF_T_NANOTIME:
			logger(RADLOG_NOTICE, "     FORMAT = NANOTIME");
			break;
		case BPF_T_BINTIME:
			logger(RADLOG_NOTICE, "     FORMAT = BINTIME");
			break;
		case BPF_T_NONE:
			logger(RADLOG_NOTICE, "     FORMAT = NONE");
		}
		
		switch (BPF_T_FFRAW(bd_tstamp)) {
		case BPF_T_NOFFC:
			logger(RADLOG_NOTICE, "     FFRAW = NOFFC");
			break;
		case BPF_T_FFC:
			logger(RADLOG_NOTICE, "     FFRAW = FFC");
		}
		
		switch (BPF_T_FLAVOR(bd_tstamp)) {
		case BPF_T_NORMAL:
			logger(RADLOG_NOTICE, "     FLAVOR = NORMAL [UTC, !FAST]");
			break;
		case BPF_T_FAST:
			logger(RADLOG_NOTICE, "     FLAVOR = FAST [UTC, FAST]");
			break;
		case BPF_T_MONOTONIC:
			logger(RADLOG_NOTICE, "     FLAVOR = MONOTONIC [Uptime, !FAST]");
			break;
		case BPF_T_MONOTONIC_FAST:
			logger(RADLOG_NOTICE, "     FLAVOR = MONOTONIC_FAST [Uptime, FAST]");
		}
		
		switch (BPF_T_CLOCK(bd_tstamp)) {
		case BPF_T_SYSCLOCK:
			logger(RADLOG_NOTICE, "     CLOCK = SYSCLOCK");
			break;
	 	case BPF_T_FBCLOCK:
			logger(RADLOG_NOTICE, "     CLOCK = FBCLOCK");
			break;
		case BPF_T_FFCLOCK:
			logger(RADLOG_NOTICE, "     CLOCK = FFCLOCK");
			break;
		case BPF_T_FFNATIVECLOCK:
			logger(RADLOG_NOTICE, "     CLOCK = FFNATIVECLOCK");
			break;
		case BPF_T_FFDIFFCLOCK:
			logger(RADLOG_NOTICE, "     CLOCK = FFDIFFCLOCK");
		}
}


int
descriptor_set_tsmode(struct radclock *handle, pcap_t *p_handle, int *mode)
{
	u_int bd_tstamp = 0U;
	int override_mode = *mode;		// record input mode

	switch (handle->kernel_version) {
	case 0:
	case 1:
		if (ioctl(pcap_fileno(p_handle), BIOCSRADCLOCKTSMODE, (caddr_t)mode) == -1) {
			logger(RADLOG_ERR, "Setting capture mode failed");
			return (1);
		}
		break;

	case 2:
		switch (*mode) {
			case PKTCAP_TSMODE_SYSCLOCK:
			case PKTCAP_TSMODE_FBCLOCK:
			case PKTCAP_TSMODE_FFCLOCK:
			case PKTCAP_TSMODE_FFNATIVECLOCK:
			case PKTCAP_TSMODE_FFDIFFCLOCK:
				logger(RADLOG_ERR, "Requested mode unsupported: "
					"switching to PKTCAP_TSMODE_RADCLOCK (ts supplied by radclock)");
				override_mode = PKTCAP_TSMODE_RADCLOCK;
			case PKTCAP_TSMODE_RADCLOCK:
				//bd_tstamp = BPF_T_FFCOUNTER;	// only valid deamon choice given hacky KV=2 kernel
				bd_tstamp = BPF_T_MICROTIME | BPF_T_NORMAL | BPF_T_SYSCLOCK ;
				break;
			default:
				logger(RADLOG_ERR, "descriptor_set_tsmode: unknown timestamping mode.");
				return (1);
		}
		*mode = override_mode;	// inform caller of final mode
		if (ioctl(pcap_fileno(p_handle), BIOCSTSTAMP, (caddr_t)&bd_tstamp) == -1) {
			logger(RADLOG_ERR, "Setting capture mode failed: %s", strerror(errno));
			return (1);
		}
		break;

	case 3:
		/* Set FORMAT, FFC, and FLAVOR type components */
		bd_tstamp = BPF_T_MICROTIME | BPF_T_FFC | BPF_T_NORMAL;
		/* Set CLOCK type component*/
		switch (*mode) {
			case PKTCAP_TSMODE_SYSCLOCK:
				bd_tstamp |= BPF_T_SYSCLOCK;
				break;
			case PKTCAP_TSMODE_FBCLOCK:
				bd_tstamp |= BPF_T_FBCLOCK;
				break;
			case PKTCAP_TSMODE_FFCLOCK:
				bd_tstamp |= BPF_T_FFCLOCK;
				break;
			case PKTCAP_TSMODE_RADCLOCK:	// should have kernel = radclock, enables comparison
			case PKTCAP_TSMODE_FFNATIVECLOCK:
				bd_tstamp |= BPF_T_FFNATIVECLOCK;
				break;
			case PKTCAP_TSMODE_FFDIFFCLOCK:
				bd_tstamp |= BPF_T_FFDIFFCLOCK;
				break;
			default:
				logger(RADLOG_ERR, "descriptor_set_tsmode: Unknown timestamping mode.");
				return (1);
		}
		*mode = override_mode;	// inform caller of final mode
		if (ioctl(pcap_fileno(p_handle), BIOCSTSTAMP, (caddr_t)&bd_tstamp) == -1) {
			logger(RADLOG_ERR, "Setting capture mode to bd_tstamp = 0x%04x "
					"failed: %s", bd_tstamp, strerror(errno));
			return (1);
		}
		break;

	default:
		logger(RADLOG_ERR, "Unknown kernel version");
		return (1);
	}

	return (0);
}


/* Get the bpf timestamp type descriptor from the kernel, decode it verbosely,
 * and then attempt to invert it to a value in radclocks's PKTCAP_TSMODE_  tsmodes.
 * It returns its best guess but this function is primarily about verbosity
 * for checking purposes.
 */
int
descriptor_get_tsmode(struct radclock *handle, pcap_t *p_handle, int *mode)
{
	u_int bd_tstamp = 0U;		// need to be ≥16 bits to fit BPF_T_{,,,} flags

	switch (handle->kernel_version) {

	case 0:
	case 1:
		if (ioctl(pcap_fileno(p_handle), BIOCGRADCLOCKTSMODE, (caddr_t)mode) == -1) {
			logger(RADLOG_ERR, "Getting timestamping mode failed");
			return (1);
		}
		break;

	case 2:
		if (ioctl(pcap_fileno(p_handle), BIOCGTSTAMP, (caddr_t)(&bd_tstamp)) == -1) {
			logger(RADLOG_ERR, "Getting timestamping mode failed: %s", strerror(errno));
			return (1);
		}
		decode_bpf_tsflags_KV2(bd_tstamp);

		if (bd_tstamp == BPF_T_FFCOUNTER)
			/* Set tsmode intent to be the only one possible, a radclock Ca read */
			*mode = PKTCAP_TSMODE_RADCLOCK;
		else {
			logger(RADLOG_ERR, "bpf timestamp type not BPF_T_FFCOUNTER as expected !");
			*mode = PKTCAP_TSMODE_NOMODE; 		// mark inversion error
		}
		break;

	case 3:
		if (ioctl(pcap_fileno(p_handle), BIOCGTSTAMP, (caddr_t)(&bd_tstamp)) == -1) {
			logger(RADLOG_ERR, "Getting timestamping mode failed: %s", strerror(errno));
			return (1);
		}
		decode_bpf_tsflags_KV3(bd_tstamp);

		/* Map back to tsmode intent, correspondence is 1-1 */
		switch (bd_tstamp & BPF_T_CLOCK_MASK) {
		case BPF_T_SYSCLOCK:
			*mode = PKTCAP_TSMODE_SYSCLOCK;
			break;
		case BPF_T_FBCLOCK:
			*mode = PKTCAP_TSMODE_FBCLOCK;
			break;
		case BPF_T_FFCLOCK:
			*mode = PKTCAP_TSMODE_FFCLOCK;
			break;
		case BPF_T_FFNATIVECLOCK:
			*mode = PKTCAP_TSMODE_FFNATIVECLOCK;
			break;
		case BPF_T_FFDIFFCLOCK:
			*mode = PKTCAP_TSMODE_FFDIFFCLOCK;
		break;
		}
		break;

	default:
		logger(RADLOG_ERR, "Unknown kernel version");
		return (1);
	}
	
	return (0);
}



/*
 * The BPF subsystem adds padding after the bpf header to WORDALIGN the NETWORK
 * layer header, and not the MAC layer. In other words:
 * WORDALIGN( * bpf_hdr->bh_hrdlen + MAC_header ) - MAC_header
 *
 * Let's look at the components of WORDALIGN.
 * Because we add the vcounter_t member at the end of the hacked bpf_header, the
 * compiler adds padding before the vcount to align it on to 64 boundary.
 * Things are architecture dependent.
 *
 * - On 32 bit system, the timeval is twice 32 bit integers, and the original
 * bpf_header is 18 bytes. The compiler adds 6 bytes of padding and the vcounter
 * takes 8 bytes. The hacked header is 32 bytes long.  
 *
 * - On 64 bit systems, the timeval is twice 64 bits integers, and the hacked
 * header is 40 bytes long.
 *
 * The word alignement is based on sizeof(long), which is 4 bytes on i386 system
 * and 8 bytes on amd64 systems.  So the bpf_header is always WORDALIGNED by
 * default as 8*sizeof(long) for i386 and 5*sizeof(long) on amd64.
 *
 * Now, we have always captured packets over Ethernet (without 802.1Q), that is
 * a MAC header of 14 bytes. On both i386 and amd64 that gives a WORDALIGN at 16
 * bytes, and an extra padding of 2 bytes.
 *
 * Example of ethernet capture on amd64:
 * [[bpf_header 40bytes][padding 2bytes]]  [Ether 14bytes]  [IPv4 20 bytes]
 *
 * XXX
 * As soon as we move to capture on other MAC layer or use 802.1Q, things will
 * break, and we need a new implementation that provides the MAC header length
 * base on the DLT of the pcap handler.
 * XXX
 */



/* XXX Can we clean that ??
 * Redefinition of the BPF header as in bpf.h Just to avoid to have to include
 * the file again and define the RADCLOCK symbol at compilation time.  Changed
 * name to avoid redefinition problem. pcap.h includes bpf.h but without the
 * vcount field.
 */

struct bpf_hdr_hack_v1 {
	struct timeval bh_tstamp;	/* time stamp */
	bpf_u_int32 bh_caplen;		/* length of captured portion */
	bpf_u_int32 bh_datalen;		/* original length of packet */
	u_short bh_hdrlen;			/* length of bpf header (this struct plus alignment padding) */
	u_short padding;				/* padding to align the fields */
	vcounter_t vcount;			/* raw vcount value for this packet */
};

/*
 * Also make sure we compute the padding inside the hacked bpf header the same
 * way as in the kernel to avoid different behaviour across compilers.
 */
// these already defined in bpf.h
//#define BPF_ALIGNMENT sizeof(long)
//#define BPF_WORDALIGN(x) (((x)+(BPF_ALIGNMENT-1))&~(BPF_ALIGNMENT-1))

#define SIZEOF_BPF_HDR_v1(type)	\
	(offsetof(type, vcount) + sizeof(((type *)0)->vcount))

#define BPF_HDR_LEN_v1		\
	(BPF_WORDALIGN(SIZEOF_BPF_HDR_v1(struct bpf_hdr_hack_v1) + ETHER_HDR_LEN) \
	 - ETHER_HDR_LEN)

// FIXME inline should be in a header file, d'oh...
// FIXME should convert to void, make these tests once and not on each packet
static inline int
extract_vcount_stamp_v1(pcap_t *p_handle, const struct pcap_pkthdr *header,
		const unsigned char *packet, vcounter_t *vcount)
{
	struct bpf_hdr_hack_v1 *hack;

	/*
	 * Find the beginning of the hacked header starting from the MAC header.
	 * Useful for checking we are doing the right thing.
	 */
	hack = (struct bpf_hdr_hack_v1 *) (packet - BPF_HDR_LEN_v1);

	/* Check we did the right thing by comparing hack and pcap header pointer */
	// TODO: it may be that BPF_FFCOUNTER was not defined in the kernel.
	//    it is not a bug anymore, but a new case to handle
	// 2nd check compares contents of what hack and header point to. They point
	// to different memory, but Contents would be the same IF the pcap header
	// is populated by copying first three traditional fields of bpf_hdr
	if ((hack->bh_hdrlen != BPF_HDR_LEN_v1)   // bpf header check
		|| (memcmp(hack, header, sizeof(struct pcap_pkthdr)) != 0)) {
		logger(RADLOG_ERR, "Either modified kernel not installed, "
				"or bpf interface has changed");
		return (1);
	}

	*vcount= hack->vcount;
	return (0);
}




/* XXX this one is based off pcap_pkthdr */
//struct bpf_hdr_hack_v2 {
//	union {
//		struct timeval tv;	/* time stamp */
//		vcounter_t vcount;
//	} bh_ustamp;
//	bpf_u_int32 caplen;		/* length of captured portion */
//	bpf_u_int32 len;			/* original length of packet */
//};

static void
ts_extraction_tester(pcap_t *p_handle, const struct pcap_pkthdr *header,
		const unsigned char *packet, vcounter_t *vcount);

// FIXME inline should be in a header file, d'oh...
// FIXME should convert to void, make these tests once and not on each packet to
// improve perfs  DV:  indeed, stupid !
static inline int
extract_vcount_stamp_v2(pcap_t *p_handle, const struct pcap_pkthdr *header,
		const unsigned char *packet, vcounter_t *vcount)
{
	vcounter_t *hack;
	
	ts_extraction_tester(p_handle, header, packet, vcount);
	
	// Note:  hack will point to the sec field, as it appears first in the timeval layout
	//       If ts is 2*8 bytes, then the vcount will appear only in the sec field
	hack = (vcounter_t*) &(header->ts);
	*vcount = *hack;
	return (0);
}


/* This header is less a hack, more a guess of what the bpf header will be, as
 * under KV3  ffcounter bh_ffcounter  is `officially` in each of bpf_[x]hdr[32].
 * However still a hack in that we have to reach into the bpf header to extract
 * the raw ts, because pcap cannot provide it.
 * The only issue is to determine the current bpf-aligned header length, so that
 * a pointer to it can be found based on the assumption it prepends the frame data.
 * Solution here not completely general but will cover almost all cases.
 * Scenarios:
 *  timeval = 128bits:  should work in all cases, as both [x]hdr are 128bit with `same` layout
 *  timeval =  64bits:  should work in traditional timestamps, where catchpacket code
 *                      will select bpf_hdr, and both it and bpf_hdr_hack_v3 use timeval
 *                      Hence the layouts are compatible and the raw ts member will be recovered
 * The above cases are neatly handled together without having to explicitly measure sizeof(timeval).
 *  timeval =  64bits:  will fail if select non-traditional FORMAT which will force the use of xhdr
 * Could be fixed with more effort, by detecting sizeof(timeval), and testing if
 * the header pointer is right under the expected, and a backup scenario.  For full
 * generality however probably have to define and use the compat32 stuff, not defined in bpf.h
 * Approach therefore is to assume don't have this, and apply a test to check if wrong.
 *
 * Notes
 *  i) use our own defn rather than bpf.c:bpf_hdr to ease compilation:
 *     vcounter_t already in radclock language,  but same size as  ffcounter bh_ffcounter
 * ii) given we are finding the bpf_hdr, the bh_tstamp is also available.
 *     For a traditional ts we don't need it as it is already in pcap.ts, otherwise
 *     in the timeval=128 bit case we could extract a NANOTIME from bh_tstamp
 *     by extending the hack. What pcap.ts would equal in that case is unknown.
 */
struct bpf_hdr_hack_v3 {
	struct timeval bh_tstamp;	/* could be 2*{4,8} bytes {64,128} bits */
	vcounter_t vcount;			/* raw ts value, vcount is a reminder is a vcounter_t */
	bpf_u_int32 bh_caplen;		/* length of captured portion */
	bpf_u_int32 bh_datalen;		/* original length of packet */
	u_short bh_hdrlen;			/* length of bpf header (this struct plus alignment padding) */
};


//#define DLT_NULL    0   /* BSD loopback encapsulation */
//#define DLT_EN10MB  1   /* Ethernet (10Mb) */

static int dlt_header_size[] = {
	[DLT_NULL] = 4,
	[DLT_EN10MB] = ETHER_HDR_LEN
};

// Strip trailing compiler padding macro. Same as SIZEOF_BPF_HDR defined in bpf.c
#define MY_SIZEOF_BPF_HDR(type)	\
	(offsetof(type, bh_hdrlen) + sizeof(((type *)0)->bh_hdrlen))

// Reproduces calculation of bpf.c:bpf_hdrlen(), in 64bit case, to calculate a
// length satisfying bpf-alignment rules. Will be  ≤ sizeof(bpf_hdr)
#define BPF_HDRLEN_V3(length)		\
	(BPF_WORDALIGN(MY_SIZEOF_BPF_HDR(struct bpf_hdr_hack_v3) + length) - length)

#define BPF_HDRLEN_XHDR(length)		\
	(BPF_WORDALIGN(MY_SIZEOF_BPF_HDR(struct bpf_xhdr) + length) - length)


/*
 * Test for the presence and type of bpf headers, and ts field correspondence
 * with pcap_hdr.
 * Note, when using under KV2, the KV2 bpf does use (but not exploit) the
 * draft v3 header here, there is no `KV2' header to test on.
 * Also, KV2 Does populate the vcount field
 */
static void
ts_extraction_tester(pcap_t *p_handle, const struct pcap_pkthdr *header,
		const unsigned char *packet, vcounter_t *vcount)
{
	static int  checkcnt = 0;		// use to trigger checks and verbosity once only

	struct bpf_hdr	*h;
	vcounter_t 	*raw;
	struct timeval tv;
	int hlen, blen;

	hlen = dlt_header_size[pcap_datalink(p_handle)];
	blen = BPF_HDRLEN_V3(hlen);			// length of bpf aligned bpf_hdr

	/* Infer head of bpf_hdr */
	h = (struct bpf_hdr *)(packet - blen);
	
	/* Perfect once-only checks */
	/* TODO: make these compiler checks, but retain verbosity if fail somehow */
	if (!checkcnt) {
		/* Check header and member sizes currently accessible */
		logger(RADLOG_NOTICE, "Performing initial checks of bpf header, timeval and timestamp sizes");
		logger(RADLOG_NOTICE, " bpf header sizes:   hdr: %d, xhdr: %d", sizeof(struct bpf_hdr), sizeof(struct bpf_xhdr));
		logger(RADLOG_NOTICE, " bpf aligned sizes:  hdr: %d, xhdr: %d", blen, BPF_HDRLEN_XHDR(hlen));
		logger(RADLOG_NOTICE, " pcap:  hdr: %d, ts: %d, timeval: %d",
				sizeof(struct pcap_pkthdr), sizeof(header->ts), sizeof(struct timeval) );

		/* Basic consistency check */
		if (h->bh_hdrlen != blen)
			logger(RADLOG_ERR, " bpf_hdr length mismatch: recorded %d , inferred %d", h->bh_hdrlen, blen);
		else
			logger(RADLOG_NOTICE, " bpf_hdr length confirmed at %d, bh_tstamp at %d bytes", blen, sizeof(h->bh_tstamp) );

		/* Apply cross-ref checks between bpf and pcap headers */
		/* Cant just use sizeof, will only get back your own cast, need values of members */
		if (h->bh_caplen==header->caplen && h->bh_datalen==header->len)
			logger(RADLOG_NOTICE, " bpf_hdr crossref checks with pcap_hdr passed: caplen=%d , datalen=%d", h->bh_caplen, h->bh_datalen);
		else
			logger(RADLOG_ERR, " bpf_hdr crossref checks failed, inferred: caplen=%d , datalen=%d", h->bh_caplen, h->bh_datalen);
			
		checkcnt++; 	// switchoff
	}
		
	/* Examine actual timestamp values, but limit checks to first few packets */
	if (checkcnt<=2) {
		
		/* Simply recover ffcount from bpf header */
		logger(RADLOG_NOTICE, "bh_ffcounter:  %llu", (long long unsigned)h->bh_ffcounter);

		/* Assuming timeval, compare timestamps */
		raw = (vcounter_t *) &h->bh_tstamp;
		logger(RADLOG_NOTICE, "bh_tstamp:  %lld.%06lld and as raw: %llu",
			h->bh_tstamp.tv_sec, h->bh_tstamp.tv_usec, (long long unsigned) *raw );

		raw = (vcounter_t *) &(header->ts);	// works regardless of size of timeval, no need to test first, nice
		logger(RADLOG_NOTICE, "pcap ts:    %lld.%06lld and as raw: %llu\n",
			header->ts.tv_sec, header->ts.tv_usec, (long long unsigned) *raw );

		/* Smart timeval printing */
		if (sizeof(struct timeval)==16) {		// 64 bit machines
			tv = h->bh_tstamp;
			logger(RADLOG_NOTICE, "bpf  (tval 2*64):  %lld  %lld", tv.tv_sec, tv.tv_usec);
			tv = header->ts;
			logger(RADLOG_NOTICE, "pcap (tval 2*64):  %lld  %lld", tv.tv_sec, tv.tv_usec);
		} else {		// traditional timeval, 32 machines
			tv = h->bh_tstamp;
			logger(RADLOG_NOTICE, "bpf  (tval 2*32):  %ld  %ld"  , tv.tv_sec, tv.tv_usec);
			tv = header->ts;
			logger(RADLOG_NOTICE, "pcap (tval 2*32):  %ld  %ld"  , tv.tv_sec, tv.tv_usec);
		}

		/* Translate the sec member to a time and date */
		struct tm *t;
		t = localtime(&(tv.tv_sec));
		const char *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
		logger(RADLOG_NOTICE, "%s %02d %04d, %02d:%02d:%02d\n", months[t->tm_mon], t->tm_mday,
				1900+t->tm_year, t->tm_hour, t->tm_min, t->tm_sec);
				
		checkcnt++;
	}
				
				
//	h = (struct bpf_hdr_hack_v3 *)(packet - BPF_HDRLEN(hlen));
// bh =        (struct bpf_hdr *)(packet - sizeof(struct bpf_hdr));
//	logger(RADLOG_NOTICE, ">> header sizes:  h = %d, bh = %d", BPF_HDRLEN(hlen), sizeof(struct bpf_hdr));
//
//	logger(RADLOG_NOTICE, "h = %p,   bh = %p,  p-q = %td, and real diff = %d",
//			h, bh, ((struct bpf_hdr *)h)-bh,  (long long int)h - (long long int)bh );
//
//	/* timeval timestamp testing between preferred bpf header and pcap hdr */
//	if (memcmp(&h->bh_tstamp, &header->ts, sizeof(struct timeval)) != 0) {
//		 logger(RADLOG_ERR, "Feed-forward kernel v3 error: BPF headers do not match");
//		 return (1);
//	}
	
//	/* sanity check, are they different, right?? */
//   if (h==(struct bpf_hdr *)header)
//   	logger(RADLOG_NOTICE, "inferred hdr and pcap header pointers agree!");
//	else
//		logger(RADLOG_NOTICE, "inferred hdr and pcap header pointers disagree, of course");

	*vcount= h->bh_ffcounter;
}


/*
 * Libpcap not yet aware of bpf_xhdr.
 * Must therefore extract the raw ts from the bpf_hdr structure, prepended
 * to the frame data (clear from catchpackets code).
 */
static inline int
extract_vcount_stamp_v3(pcap_t *p_handle, const struct pcap_pkthdr *header,
		const unsigned char *packet, vcounter_t *vcount)
{
	struct bpf_hdr_hack_v3 *h;
	int hlen, blen;

	ts_extraction_tester(p_handle, header, packet, vcount);

	hlen = dlt_header_size[pcap_datalink(p_handle)];	// detect actual LL header
	blen = BPF_HDRLEN_V3(hlen);			// length of bpf-aligned bpf_hdr

	/* Infer head of bpf_hdr and perform basic consistency check */
	h = (struct bpf_hdr_hack_v3 *)(packet - blen);
	if (h->bh_hdrlen != blen) {
		logger(RADLOG_ERR, "Failed to find bpf hdr: (expected,apparent) header "
				"length mismatch (%d, %d)", blen, h->bh_hdrlen);
		return (1);
	}
	
	/* Extra consistency checks between corresponding bpf and pcap hdr fields */
   if (h->bh_caplen!=header->caplen || h->bh_datalen!=header->len) {
		logger(RADLOG_ERR, "bpf_hdr caplen or datalen fails to match pcap");
		return (1);
	}
	
	/* Assuming structures align, are bpf and pcap timestamps identical? */
	if (memcmp(&h->bh_tstamp, &header->ts, sizeof(struct timeval)) != 0) {
		logger(RADLOG_ERR, "bpf_hdr and pcap timestamps do not match");
		return (1);
	}

	*vcount = h->vcount;
	return (0);
}


/* Note on data arguments, passed directly to  extract_vcount_stamp_v#
 *   header:  ptr to the actual pcap_hdr structure owned/supplied by pcap
 *   packet:              "     frame data           "
 *	  vcount:  direct access into the rbd where the extract raw ts is to be stored
 */
int
extract_vcount_stamp(struct radclock *clock, pcap_t *p_handle,
		const struct pcap_pkthdr *header, const unsigned char *packet,
		vcounter_t *vcount)
{
	int err;

	/* Check we are running live */
	if (pcap_fileno(p_handle) < 0)
		return (1);

	// FIXME : need a function pointer to the correct extract_vcount function
	//         Yes, would avoid need for testing each time
	switch (clock->kernel_version) {
	case 0:
	case 1:
		err = extract_vcount_stamp_v1(clock->pcap_handle, header, packet, vcount);
		break;
	case 2:
		/* This version supports a single timestamp at a time */
		if (clock->tsmode == PKTCAP_TSMODE_RADCLOCK)
			err = extract_vcount_stamp_v2(clock->pcap_handle, header, packet, vcount);
		else {
			*vcount = 0;
			logger(RADLOG_ERR, "Timestamping mode should be RADCLOCK");
			err = 1;
		}
		break;
	case 3:
		err = extract_vcount_stamp_v3(clock->pcap_handle, header, packet, vcount);
		break;
	default:
		err = 1;
	}

	if (err) {
		logger(RADLOG_ERR, "Cannot extract raw timestamp from pcap data"
			"pcap timestamp reads: %ld.%ld", header->ts.tv_sec, header->ts.tv_usec);
		return (1);
	}

	return (0);
}


#endif

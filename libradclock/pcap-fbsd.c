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
#include "kclock.h"
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
#define	BPF_T_FLAG_MASK		0x0300
#define	BPF_T_FORMAT(t)		((t) & BPF_T_FORMAT_MASK)
#define	BPF_T_FLAG(t)			((t) & BPF_T_FLAG_MASK)
#define	BPF_T_CLOCK(t)			((t) & BPF_T_CLOCK_MASK)
#define	BIOCGTSTAMP	_IOR('B', 131, u_int)
#define	BIOCSTSTAMP	_IOW('B', 132, u_int)
#endif

/* Ensure definition of symbols unique to KV=2 */
#ifndef	BPF_T_FFCOUNTER
#define	BPF_T_FFCOUNTER		0x0004
#define	BPF_T_FORMAT_MAX		0x0004
#define	BPF_T_CLOCK_MAX		0x2000
#endif
	

/* Ensure definition of symbols unique to KV=3 */
#ifndef	BPF_T_NOFFC
#define	BPF_T_NOFFC				0x0000   // no FFcount
#define	BPF_T_FFC				0x0010   // want FFcount
#define	BPF_T_FFRAW_MASK		0x0010
#define	BPF_T_FAST				0x0100   // UTC,  FAST
#define	BPF_T_MONOTONIC		0x0200	// UPTIME, !FAST Exception, redefined to allow compile
#define	BPF_T_MONOTONIC_FAST	0x0300	// UPTIME,  FAST
//#define	BPF_T_FLAVOR_MASK		0x0300
#define	BPF_T_FFNATIVECLOCK	0x3000	// read native FF
#define	BPF_T_FFDIFFCLOCK		0x4000	// read FF difference clock
#define	BPF_T_FFRAW(t)			((t) & BPF_T_FFRAW_MASK)
//#define	BPF_T_FLAVOR(t)		((t) & BPF_T_FLAVOR_MASK)
#define	BPF_T_CLOCK_MASK		0x7000	// shouldnt be needed..
#endif



/* The decoding functions display what the given bpf timestamp type descriptor
 * corresponds to, explicity and exhaustively, in terms of its independent
 * components, and fields within each component.
 */
static void
decode_bpf_tsflags_KV2(long bd_tstamp)
{
		logger(RADLOG_NOTICE, "Decoding bpf timestamp type _T_ flags (bd_tstamp = %ld (0x%04x))",
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
decode_bpf_tsflags_KV3(long bd_tstamp)
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
		
		switch (BPF_T_FLAG(bd_tstamp)) {
		case BPF_T_NORMAL:
			logger(RADLOG_NOTICE, "     FLAG = NORMAL [UTC, !FAST]");
			break;
		case BPF_T_FAST:
			logger(RADLOG_NOTICE, "     FLAG = FAST [UTC, FAST]");
			break;
		case BPF_T_MONOTONIC:
			logger(RADLOG_NOTICE, "     FLAG = MONOTONIC [Uptime, !FAST]");
			break;
		case BPF_T_MONOTONIC_FAST:
			logger(RADLOG_NOTICE, "     FLAG = MONOTONIC_FAST [Uptime, FAST]");
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
descriptor_set_tsmode(struct radclock *handle, pcap_t *p_handle, int *mode, u_int custom)
{
	long bd_tstamp = 0;
	long override_mode = *mode;		// record input mode

	switch (handle->kernel_version) {
	case 0:
	case 1:
		if (ioctl(pcap_fileno(p_handle), BIOCSRADCLOCKTSMODE, (caddr_t)mode) == -1) {
			logger(RADLOG_ERR, "Setting timestamping mode failed");
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
			logger(RADLOG_ERR, "Setting timestamping mode failed: %s", strerror(errno));
			return (1);
		}
		break;

	case 3:
		/* Initial test of value of tsmode */
//		logger(RADLOG_NOTICE, "Checking current tsmode before setting it :");
//		if (ioctl(pcap_fileno(p_handle), BIOCGTSTAMP, (caddr_t)(&bd_tstamp)) == -1)
//			logger(RADLOG_ERR, "Getting initial timestamping mode failed: %s", strerror(errno));
//		decode_bpf_tsflags_KV3(bd_tstamp);

		/* Set FORMAT, FFC, and FLAG type components */
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
			case PKTCAP_TSMODE_CUSTOM:
				bd_tstamp = custom;
				break;
			default:
				logger(RADLOG_ERR, "descriptor_set_tsmode: Unknown timestamping mode %d.", *mode);
				return (1);
		}
		*mode = override_mode;	// inform caller of final mode

		int pcaprtn = 0;
		if ( (pcaprtn = ioctl(pcap_fileno(p_handle), BIOCSTSTAMP, (caddr_t)&bd_tstamp)) == -1) {
			logger(RADLOG_ERR, "Setting timestamping mode to bd_tstamp = 0x%04x "
					"failed: %s", bd_tstamp, strerror(errno));
			return (1);
		}
		//logger(RADLOG_NOTICE, "tsmode setting ioctl returned %d\n", pcaprtn); // to be removed
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
	long bd_tstamp = 0;		// need to be ≥16 bits to fit BPF_T_{,,,} flags

	switch (handle->kernel_version) {

	case 0:
	case 1:
		if (ioctl(pcap_fileno(p_handle), BIOCGRADCLOCKTSMODE, (caddr_t)&bd_tstamp) == -1) {
			logger(RADLOG_ERR, "Getting timestamping mode failed");
			return (1);
		}
		*mode = bd_tstamp;
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



/* Redefinition of the BPF header as in bpf.h Just to avoid to have to include
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

	*vcount = hack->vcount;
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
 *     in the timeval=128 bit case we could extract a NANOTIME or BINTIME from bh_tstamp
 *     by casting the ts.
 */
struct bpf_hdr_hack_v3 {
	struct timeval bh_tstamp;	/* could be 2*{4,8} bytes {64,128} bits */
	bpf_u_int32 bh_caplen;		/* length of captured portion */
	bpf_u_int32 bh_datalen;		/* original length of packet */
	u_short bh_hdrlen;			/* length of bpf header (this struct plus alignment padding) */
	//ffcounter	bh_ffcounter;	/* [True FF header member] feed-forward counter stamp */
	vcounter_t vcount;			/* daemon version for compilation */
};

struct bpf_xhdr_hack_v3 {
	struct bpf_ts	bh_tstamp;	/* time stamp */
	bpf_u_int32	bh_caplen;		/* length of captured portion */
	bpf_u_int32	bh_datalen;		/* original length of packet */
	u_short		bh_hdrlen;		/* length of bpf header (this struct plus alignment padding) */
	//ffcounter	bh_ffcounter;	/* [True FF header member] feed-forward counter stamp */
	vcounter_t vcount;			/* daemon version for compilation */
};

//#define DLT_NULL    0   /* BSD loopback encapsulation */
//#define DLT_EN10MB  1   /* Ethernet (10Mb) */
static int dlt_header_size[] = {
	[DLT_NULL] = 4,
	[DLT_EN10MB] = ETHER_HDR_LEN
};

/* Macros for calculating the length of a structure, minus the trailing padding
 * potentially added by the compiler. This is done to match the same procedure
 * in bpf defining `bpf alignment`.
 * MY_SIZEOF_BPF_HDR:  same as SIZEOF_BPF_HDR defined in bpf.c, but using vcount
 * (last member of locally defined bpf_hdr_hack_* version), instead of actual
 * last member of bpf_{x}hdr.
 * Previously the last member in all cases was bh_hdrlen, but moved to end to
 * avoid ABI issues for applications (like pcap!) compiled against headers
 * missing the raw member.  Idea is precompiled offsets from the start of the
 * header will still be correct, so all old members will still work, including
 * bh_hdrlen, which thanks to the macro use in bpf.c, will contain the true
 * header size including the raw member at run time.
 */

/* length-endpadding = lengthtolastmember + sizeoflastmember */
#define MY_SIZEOF_BPF_HDR(type)	\
	(offsetof(type, vcount) + sizeof(((type *)0)->vcount))

/* Reproduces calculation of bpf.c:bpf_hdrlen(), in 64bit case, to calculate a
 * length satisfying bpf-alignment rules. Will be  ≤ sizeof(bpf_hdr) */
#define BPF_HDRLEN_V3(length)		\
	(BPF_WORDALIGN(MY_SIZEOF_BPF_HDR(struct bpf_hdr_hack_v3) + length) - length)
#define BPF_HDRLEN_XHDR(length)		\
	(BPF_WORDALIGN(MY_SIZEOF_BPF_HDR(struct bpf_xhdr_hack_v3) + length) - length)


/* Version to support testing, too verbose for user radclocks. To be retired.
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
	blen = BPF_HDRLEN_V3(hlen);		// length of bpf aligned bpf_hdr, based on equivalent bpf_hdr_hack_v3

	/* Infer head of bpf_hdr */
	h = (struct bpf_hdr *)(packet - blen);
	
	/* Perfect once-only checks */
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
	if (checkcnt<=1) {
		if (checkcnt==1) logger(RADLOG_NOTICE, "Detailed look at first packets: ");

		/* Simply recover ffcount from bpf header */
		logger(RADLOG_NOTICE, " bh_ffcounter:  %llu", (long long unsigned)h->bh_ffcounter);

		/* Assuming timeval, compare timestamps */
		raw = (vcounter_t *) &h->bh_tstamp;	// raw only 64bits, so if timeval is 128, will see which half the cast gets
		logger(RADLOG_NOTICE, " bh_tstamp:  %lld.%06lld and as cast to raw: %llu",
			h->bh_tstamp.tv_sec, h->bh_tstamp.tv_usec, (long long unsigned) *raw );

		raw = (vcounter_t *) &(header->ts);	// works regardless of size of timeval, no need to test first, nice
		logger(RADLOG_NOTICE, " pcap ts:    %lld.%06lld and as cast to raw: %llu",
			header->ts.tv_sec, header->ts.tv_usec, (long long unsigned) *raw );

		/* Smart timeval printing */
		if (sizeof(struct timeval)==16) {		// 64 bit machines
			tv = h->bh_tstamp;
			logger(RADLOG_NOTICE, " bpf  (tval 2*64):  %lld  %lld", tv.tv_sec, tv.tv_usec);
			tv = header->ts;
			logger(RADLOG_NOTICE, " pcap (tval 2*64):  %lld  %lld", tv.tv_sec, tv.tv_usec);
		} else {		// traditional timeval, 32 machines
			tv = h->bh_tstamp;
			logger(RADLOG_NOTICE, " bpf  (tval 2*32):  %ld  %ld"  , tv.tv_sec, tv.tv_usec);
			tv = header->ts;
			logger(RADLOG_NOTICE, " pcap (tval 2*32):  %ld  %ld"  , tv.tv_sec, tv.tv_usec);
		}

		/* Translate the sec member to a time and date */
		struct tm *t;
		t = localtime(&(tv.tv_sec));
		const char *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
		logger(RADLOG_NOTICE, " %s %02d %04d, %02d:%02d:%02d", months[t->tm_mon], t->tm_mday,
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



/* Version for user radclocks, KV>2 only.
 * Test for the presence and type of bpf headers, and ts field correspondence
 * with pcap_hdr.
 * Gives detailed verbosity -- in case of error only.
 */
static void
header_match_test(pcap_t *p_handle, const struct pcap_pkthdr *header,
		const unsigned char *packet)
{
	struct bpf_hdr	*h;
	vcounter_t 	*raw;
	struct timeval tv;
	int hlen, blen;

	hlen = dlt_header_size[pcap_datalink(p_handle)];
	blen = BPF_HDRLEN_V3(hlen);		// length of bpf aligned bpf_hdr, based on equivalent bpf_hdr_hack_v3

	/* Infer head of bpf_hdr */
	h = (struct bpf_hdr *)(packet - blen);
	
	/* Error if header fields common to pcap and bpf disagree */
	int error = 0;
	error = (h->bh_hdrlen != blen) || (h->bh_caplen  != header->caplen)
											 || (h->bh_datalen != header->len) ;
	
	/* Perfect once-only checks */
	/* TODO: make these compiler checks, but retain verbosity if fail somehow */
	if (error) {
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
					
					
  /* Examine actual timestamp values, but limit checks to first few packets */
		logger(RADLOG_NOTICE, "Detailed look at first packets: ");

		/* Simply recover ffcount from bpf header */
		logger(RADLOG_NOTICE, " bh_ffcounter:  %llu", (long long unsigned)h->bh_ffcounter);

		/* Assuming timeval, compare timestamps */
		raw = (vcounter_t *) &h->bh_tstamp;	// raw only 64bits, so if timeval is 128, will see which half the cast gets
		logger(RADLOG_NOTICE, " bh_tstamp:  %lld.%06lld and as cast to raw: %llu",
			h->bh_tstamp.tv_sec, h->bh_tstamp.tv_usec, (long long unsigned) *raw );

		raw = (vcounter_t *) &(header->ts);	// works regardless of size of timeval, no need to test first, nice
		logger(RADLOG_NOTICE, " pcap ts:    %lld.%06lld and as cast to raw: %llu",
			header->ts.tv_sec, header->ts.tv_usec, (long long unsigned) *raw );

		/* Smart timeval printing */
		if (sizeof(struct timeval)==16) {		// 64 bit machines
			tv = h->bh_tstamp;
			logger(RADLOG_NOTICE, " bpf  (tval 2*64):  %lld  %lld", tv.tv_sec, tv.tv_usec);
			tv = header->ts;
			logger(RADLOG_NOTICE, " pcap (tval 2*64):  %lld  %lld", tv.tv_sec, tv.tv_usec);
		} else {		// traditional timeval, 32 machines
			tv = h->bh_tstamp;
			logger(RADLOG_NOTICE, " bpf  (tval 2*32):  %ld  %ld"  , tv.tv_sec, tv.tv_usec);
			tv = header->ts;
			logger(RADLOG_NOTICE, " pcap (tval 2*32):  %ld  %ld"  , tv.tv_sec, tv.tv_usec);
		}

		/* Translate the sec member to a time and date */
		struct tm *t;
		t = localtime(&(tv.tv_sec));
		const char *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
		logger(RADLOG_NOTICE, " %s %02d %04d, %02d:%02d:%02d", months[t->tm_mon], t->tm_mday,
				1900+t->tm_year, t->tm_hour, t->tm_min, t->tm_sec);
				
	}
				
}



/*
 * Libpcap does not give full access to bpf_xhdr, it only extracts the ts.
 * Must therefore extract the raw ts from the bpf_hdr structure, prepended
 * to the frame data (this fact clear from catchpackets code).
 *
 * ** This version also sanity checks the ts timestamp, and corrects it if it is
 * wrong by taking it directly from the bpf header (which fully respects the
 * tsmode we select). Need this as version 9.1.1_1 of pcap breaks the semantics
 * of tsmode, by assuming a FORMAT of BPF_T_BINTIME in all cases, regardless of
 * the passed mode. The fix here works for old pcap, the buggy version, and
 * should be future proof.
 */
static inline int
extract_vcount_stamp_v3(pcap_t *p_handle,  const struct pcap_pkthdr *header,
		const unsigned char *packet, vcounter_t *vcount, struct pcap_pkthdr *rdhdr)
{
	struct bpf_hdr_hack_v3 *h;	// use local hdr defn to get a vcounter_t directly
	int hlen, blen;

	/* TODO: make a compile time check, or initialization somehow */
	static int notchecked = 1;		// trigger checks and verbosity once only
	if (notchecked) {
		header_match_test(p_handle, header, packet);
		notchecked = 0;
	}

	hlen = dlt_header_size[pcap_datalink(p_handle)];	// detect actual LL header
	blen = BPF_HDRLEN_V3(hlen);		// length of bpf-aligned bpf_hdr

	/* Infer head of bpf_hdr and perform basic consistency check */
	h = (struct bpf_hdr_hack_v3 *)(packet - blen);
	
	/* Recover the raw ts from the bpf header */
	*vcount = h->vcount;

//	if (h->bh_hdrlen != blen) {
//		logger(RADLOG_ERR, "Failed to find bpf hdr: (expected,apparent) header "
//				"length mismatch (%d, %d)", blen, h->bh_hdrlen);
//		return (1);
//	}

	/* Extra consistency checks between corresponding bpf and pcap hdr fields */
//   if (h->bh_caplen!=header->caplen || h->bh_datalen!=header->len) {
//		logger(RADLOG_ERR, "bpf_hdr caplen or datalen fails to match pcap");
//		return (1);
//	}

	/* Copy the pcap hdr to the rdb, assuming for the moment the ts is good */
	memcpy(rdhdr, header, sizeof(struct pcap_pkthdr));

	/* Assuming structures align, are bpf and pcap timestamps identical?
	 * Is likely to occur on 0, or All pkts: limit warning to first occurrence.
	 * TODO: make the whole thing a compiler, or init, check, speed this up!
	 *       implement by using a fn ptr, with two versions: check or not?
	 */
	static int tstested = 0;
	if ( memcmp(&h->bh_tstamp, &header->ts, sizeof(struct timeval)) != 0 ) {
		if ( !tstested ) {
			logger(RADLOG_WARNING, "bpf_hdr and pcap timestamps do not match");
			logger(RADLOG_WARNING, "Overwriting pcap ts with bpf ts (which will "
					"obey the requested tsmode), silencing future transgressions.");
			tstested = 1;
		}
		/* Could fail if timeval 8 bytes, but shouldnt execute in that case */
		memcpy(&rdhdr->ts, &h->bh_tstamp, sizeof(struct timeval));
	}
	
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
		vcounter_t *vcount, struct pcap_pkthdr *rdhdr)
{
	int err;

	/* Check we are running live */
	if (pcap_fileno(p_handle) == -1)
		return (1);

	// FIXME : need a function pointer to the correct extract_vcount function
	//         Yes, would avoid need for testing each time
	switch (clock->kernel_version) {
	case 0:
	case 1:
		err = extract_vcount_stamp_v1(clock->pcap_handle, header, packet, vcount);
		memcpy(rdhdr, header, sizeof(struct pcap_pkthdr));
		break;
	case 2:
		/* This version supports a single timestamp at a time */
		if (clock->tsmode == PKTCAP_TSMODE_RADCLOCK) {
			err = extract_vcount_stamp_v2(clock->pcap_handle, header, packet, vcount);
			memcpy(rdhdr, header, sizeof(struct pcap_pkthdr));
		} else {
			*vcount = 0;
			logger(RADLOG_ERR, "Timestamping mode should be RADCLOCK");
			err = 1;
		}
		break;
	case 3:
		/* Copy pcap header to rdhdr internally as _v3 may correct the ts first */
		err = extract_vcount_stamp_v3(clock->pcap_handle, header, packet, vcount, rdhdr);
		break;
	default:
		err = 1;
	}

	if (err) {
		logger(RADLOG_ERR, "Cannot extract raw timestamp from pcap data, "
			"pcap timestamp reads: %ld.%ld", header->ts.tv_sec, header->ts.tv_usec);
		return (1);
	}

	return (0);
}


/* This routine interprets the contents of the timeval-typed ts field from
 * the pcap header according to the BPF_T_FORMAT options, and converts the
 * timestamp to a long double.
 * The options where the tv_usec field have been used to store the fractional
 * component of time (MICROTIME (mus, the original tval format), and
 * and NANOTIME (ns)) both fit within either 32 or 64 bits tv_usec field,
 * whereas BINTIME can only work in the 64 bit case.
 */
#define	MS_AS_BINFRAC	(uint64_t)18446744073709551ULL	// floor(2^64/1e3)

void
ts_format_to_double(struct timeval *pcapts, int tstype, long double *timestamp)
{
	static int limit = 0;					// limit verbosity to once-only
	//long double two32 = 4294967296.0;	// 2^32
	//long double timetest;

	*timestamp = pcapts->tv_sec;

	switch (BPF_T_FORMAT(tstype)) {
	case BPF_T_MICROTIME:
		if (!limit) fprintf(stdout, "converting microtime\n");
		*timestamp += 1e-6 * pcapts->tv_usec;
		break;
	case BPF_T_NANOTIME:
		if (!limit) fprintf(stdout, "converting nanotime\n");
		*timestamp += 1e-9 * pcapts->tv_usec;
		break;
	case BPF_T_BINTIME:
		if (!limit) fprintf(stdout, "converting bintime\n");
		if (sizeof(struct timeval) < 16)
			fprintf(stderr, "Looks like timeval frac field is only 32 bits, ignoring!\n");
		else {
//			*timestamp += ((long double)pcapts->tv_usec)/( 1000*(long double)MS_AS_BINFRAC );
//			*timestamp += ((long double)((long long unsigned)pcapts->tv_usec))/( two32*two32 );
			bintime_to_ld(timestamp, (struct bintime *) pcapts);	// treat the ts as a bintime
			//if (*timestamp - timetest != 0)
			//	fprintf(stderr, "conversion error:  %3.4Lf [ns] \n", 1e9*(*timestamp - timetest));
		}
		break;
	}

	limit++;
}


/* Takes a bintime input, converts it to the desired tstype FORMAT, then forces
 * it into a bpf_ts type which can handle all cases.
 */
//static void
//bpf_bintime2ts(struct bintime *bt, struct bpf_ts *ts, int tstype)
//{
//	struct timeval tsm;
//	struct timespec tsn;
//
//	switch (BPF_T_FORMAT(tstype)) {
//	case BPF_T_MICROTIME:
//		bintime2timeval(bt, &tsm);
//		ts->bt_sec = tsm.tv_sec;
//		ts->bt_frac = tsm.tv_usec;
//		break;
//	case BPF_T_NANOTIME:
//		bintime2timespec(bt, &tsn);
//		ts->bt_sec = tsn.tv_sec;
//		ts->bt_frac = tsn.tv_nsec;
//		break;
//	case BPF_T_BINTIME:
//		ts->bt_sec = bt->sec;
//		ts->bt_frac = bt->frac;
//		break;
//	}
//}



#endif

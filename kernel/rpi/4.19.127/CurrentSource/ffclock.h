#ifndef __LINUX_FFCLOCK_H
#define __LINUX_FFCLOCK_H

#include <linux/types.h>

typedef u64 ffcounter;	// perhaps put in a _ffcounter.h


/* The following code block provides needed support for the bintime format.
 * The code is a subset of that released under the FreeBSD project, under
 * 	SPDX-License-Identifier: BSD-3-Clause
 * We retain that license here.  For full details, see sys/sys/time.h of the
 * FreeBSD project where the code appears.
 */

struct bintime {
	time_t sec;
	__u64 frac;
};

static __inline void
bintime_addx(struct bintime *_bt, uint64_t _x)
{
        uint64_t _u;

        _u = _bt->frac;
        _bt->frac += _x;
        if (_u > _bt->frac)
                _bt->sec++;
}

static __inline void
bintime_add(struct bintime *_bt, const struct bintime *_bt2)
{
        uint64_t _u;

        _u = _bt->frac;
        _bt->frac += _bt2->frac;
        if (_u > _bt->frac)
                _bt->sec++;
        _bt->sec += _bt2->sec;
}

static __inline void
bintime_sub(struct bintime *_bt, const struct bintime *_bt2)
{
        uint64_t _u;

        _u = _bt->frac;
        _bt->frac -= _bt2->frac;
        if (_u < _bt->frac)
                _bt->sec--;
        _bt->sec -= _bt2->sec;
}

static __inline void
bintime_mul(struct bintime *_bt, u_int _x)
{
        uint64_t _p1, _p2;

        _p1 = (_bt->frac & 0xffffffffull) * _x;
        _p2 = (_bt->frac >> 32) * _x + (_p1 >> 32);
        _bt->sec *= _x;
        _bt->sec += (_p2 >> 32);
        _bt->frac = (_p2 << 32) | (_p1 & 0xffffffffull);
}

#define bintime_clear(a)        ((a)->sec = (a)->frac = 0)
#define bintime_isset(a)        ((a)->sec || (a)->frac)
#define bintime_cmp(a, b, cmp)                                          \
        (((a)->sec == (b)->sec) ?                                       \
            ((a)->frac cmp (b)->frac) :                                 \
            ((a)->sec cmp (b)->sec))


/*
 * Feed-forward clock estimate
 * Holds time mark as a ffcounter and conversion to bintime based on current
 * timecounter period and offset estimate passed by the synchronization daemon.
 * Provides time of last daemon update, clock status and bound on error.
 */
struct ffclock_estimate {
	struct bintime	update_time;	/* FF clock time of last update, ie Ca(tlast) */
	ffcounter	update_ffcount;	/* Counter value at last update */
	ffcounter	leapsec_expected;	/* Estimated counter value of next leap second */
	__u64	period;					/* Estimate of current counter period  [2^-64 s] */
	__u32	errb_abs;				/* Bound on absolute clock error [ns] */
	__u32	errb_rate;				/* Bound on relative counter period error [ps/s] */
	__u32	status;					/* Clock status */
	__u16	secs_to_nextupdate;	/* Estimated wait til next update [s] */
	int8_t	leapsec_total;			/* Sum of leap secs seen since clock start */
	int8_t	leapsec_next;			/* Next leap second (in {-1,0,1}) */
};

/* Constants to hold errors and error rates in 64bit binary fraction fields */
#define	MS_AS_BINFRAC	(uint64_t)18446744073709551ULL	// floor(2^64/1e3)
#define	MUS_AS_BINFRAC	(uint64_t)18446744073709ULL		// floor(2^64/1e6)
#define	NS_AS_BINFRAC	(uint64_t)18446744073ULL			// floor(2^64/1e9)
#define	PS_AS_BINFRAC	(uint64_t)18446744ULL				// floor(2^64/1e12)


/* Flag defining if counter bypass mode is desired or not.
 * This is only possible if the counter is a TSC with rdtsc defined.
 */
extern uint8_t ffcounter_bypass;

/*
 * Parameters of counter characterisation required by feed-forward algorithms.
 */
#define	FFCLOCK_SKM_SCALE	1024

/*
 * Feed-forward clock status
 */
#define	FFCLOCK_STA_UNSYNC	1
#define	FFCLOCK_STA_WARMUP	2



/* Timestamping mode (tsmode) support for the FFclock */
extern int ffclock_tsmode;		// needed in sock.c

/* Deprecated */
//#define FFCLOCK_TSMODE_SYSCLOCK	1		// leave normal pcap ts field, hide vcount in padded hdr
//#define FFCLOCK_TSMODE_FFCLOCK	2		// fill ts with FFclock Abs clock, hide vcount in hdr
//#define FFCLOCK_TSMODE_FAIRCOMPARE	3	// as _SYSCLOCK, but ts and raw timestamps back-to-back

/* Time stamping flags */
/* FORMAT flags 	[ mutually exclusive, not to be ORed ]*/
#define	BPF_T_MICROTIME	0x0000
#define	BPF_T_NANOTIME		0x0001
#define	BPF_T_BINTIME		0x0002
#define	BPF_T_NONE			0x0003	// relates to ts only, FFRAW independent
#define	BPF_T_FORMAT_MASK	0x0003
/* FFRAW flag */
#define	BPF_T_NOFFC			0x0000   // no FFcount
#define	BPF_T_FFC			0x0010   // want FFcount
#define	BPF_T_FFRAW_MASK	0x0010
/* FLAG flags   [ can view bits as ORable flags ] */
#define	BPF_T_NORMAL		0x0000	// UTC, !FAST
#define	BPF_T_FAST			0x0100   // UTC,  FAST
#define	BPF_T_MONOTONIC	0x0200	// UPTIME, !FAST
#define	BPF_T_MONOTONIC_FAST	0x0300// UPTIME,  FAST
#define	BPF_T_FLAG_MASK	0x0300
/* CLOCK flags   [ mutually exclusive, not to be ORed ] */
#define	BPF_T_SYSCLOCK		0x0000	// read current sysclock
#define	BPF_T_FBCLOCK		0x1000   // read FB
#define	BPF_T_FFCLOCK		0x2000   // read mono FF (standard reads are mono)
#define	BPF_T_FFNATIVECLOCK	0x3000	// read native FF
#define	BPF_T_FFDIFFCLOCK	0x4000	// read FF difference clock
#define	BPF_T_CLOCK_MASK	0x7000

/* Extract FORMAT, FFRAW, FLAG, CLOCK  bits */
#define	BPF_T_FORMAT(t)	((t) & BPF_T_FORMAT_MASK)
#define	BPF_T_FFRAW(t)		((t) & BPF_T_FFRAW_MASK)
#define	BPF_T_FLAG(t)		((t) & BPF_T_FLAG_MASK)
#define	BPF_T_CLOCK(t)		((t) & BPF_T_CLOCK_MASK)

/* Read the FFclocks (both raw and normal timestamps) according to the
 * BPT_T_ based specification in tsmode */
void ffclock_fill_timestamps(ffcounter ffc, long tsmode, ffcounter *rawts, ktime_t *ts);
//void radclock_fill_ktime(ffcounter ffcount, ktime_t *ktime);

/* Return the current value of the feed-forward clock counter. */
void ffclock_read_counter(ffcounter *ffcount);


/* Netlink related */
#define FFCLOCK_NAME "ffclock"

enum {
	FFCLOCK_ATTR_DUMMY,
	FFCLOCK_ATTR_DATA,
	__FFCLOCK_ATTR_MAX,
};
#define FFCLOCK_ATTR_MAX (__FFCLOCK_ATTR_MAX - 1)

enum {
	FFCLOCK_CMD_UNSPEC,
	FFCLOCK_CMD_GETATTR,
	FFCLOCK_CMD_SETATTR,
	__FFCLOCK_CMD_MAX,
};
#define FFCLOCK_CMD_MAX (__FFCLOCK_CMD_MAX - 1)


#endif /* __LINUX_FFCLOCK_H */

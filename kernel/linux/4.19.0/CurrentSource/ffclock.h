#ifndef __LINUX_RADCLOCK_H
#define __LINUX_RADCLOCK_H

#include <linux/types.h>
//#include <linux/clocksource.h>

typedef u64 ffcounter;	// perhaps put in a _ffcounter.h


/* Copied from FFclock source file: libradclock/kclock.h
 * Structure representing the ffclock parameters
 */
struct bintime {
	time_t sec;
	__u64 frac;
};

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


/* Defines timestamping modes for the FFclock
 * Deprecated... work with for now
 */
#define RADCLOCK_TSMODE_SYSCLOCK	1		// leave normal pcap ts field, hide vcount in padded hdr
#define RADCLOCK_TSMODE_RADCLOCK	2		// fill ts with FFclock Abs clock, hide vcount in hdr
#define RADCLOCK_TSMODE_FAIRCOMPARE	3	// as _SYSCLOCK, but ts and raw timestamps back-to-back
extern int ffclock_tsmode;		// needed in sock.c

/* Return the current value of the feed-forward clock counter. */
void ffclock_read_counter(ffcounter *ffcount);

/*
 * Parameters of counter characterisation required by feed-forward algorithms.
 */
#define	FFCLOCK_SKM_SCALE	1024

/*
 * Feed-forward clock status
 */
#define	FFCLOCK_STA_UNSYNC	1
#define	FFCLOCK_STA_WARMUP	2


/* Netlink related */
#define RADCLOCK_NAME "radclock"

enum {
	RADCLOCK_ATTR_DUMMY,
	RADCLOCK_ATTR_DATA,
	__RADCLOCK_ATTR_MAX,
};
#define RADCLOCK_ATTR_MAX (__RADCLOCK_ATTR_MAX - 1)

enum {
	RADCLOCK_CMD_UNSPEC,
	RADCLOCK_CMD_GETATTR,
	RADCLOCK_CMD_SETATTR,
	__RADCLOCK_CMD_MAX,
};
#define RADCLOCK_CMD_MAX (__RADCLOCK_CMD_MAX - 1)

void radclock_fill_ktime(ffcounter ffcount, ktime_t *ktime);



/* bintime related */
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

#endif

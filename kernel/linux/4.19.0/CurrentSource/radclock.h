#ifndef __LINUX_RADCLOCK_H
#define __LINUX_RADCLOCK_H

#include <linux/types.h>
//#include <linux/clocksource.h>

typedef u64 ffcounter;	// perhaps put in a _ffcounter.h


/* Copied from RADclock source file: libradclock/kclock.h
 * Structure representing the ffclock parameters
 */
struct bintime {
	time_t sec;
	__u64 frac;
};

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


/* Defines timestamping modes for the RADclock
 * Deprecated... work with for now
 */
#define RADCLOCK_TSMODE_SYSCLOCK	1		// leave normal pcap ts field, hide vcount in padded hdr
#define RADCLOCK_TSMODE_RADCLOCK	2		// fill ts with RADclock Abs clock, hide vcount in hdr
#define RADCLOCK_TSMODE_FAIRCOMPARE	3	// as _SYSCLOCK, but ts and raw timestamps back-to-back
extern int ffclock_tsmode;		// needed in sock.c

/* Return the current value of the feed-forward clock counter. */
void ffclock_read_counter(ffcounter *ffcount);




struct radclock_fixedpoint
{
	/* phat as an int shifted phat_shift to the left */
	__u64 phat_int;
	/* Record of last time update from synchronization algorithm as an int */
	__u64 time_int;
	/* The counter value to convert in seconds */
	ffcounter vcount;
	/* the shift amount for phat_int */
	__u8 phat_shift;
	/* the shift amount for time_int */
	__u8 time_shift;
	/* Warn if stamp is over this many bits */
	__u8 countdiff_maxbits;
};


/* Netlink related */
#define RADCLOCK_NAME "radclock"

enum {
	RADCLOCK_ATTR_DUMMY,
	RADCLOCK_ATTR_DATA,
	RADCLOCK_ATTR_FIXEDPOINT,
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

#endif

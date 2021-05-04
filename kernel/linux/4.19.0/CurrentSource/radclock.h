#ifndef __LINUX_RADCLOCK_H
#define __LINUX_RADCLOCK_H

#include <linux/types.h>
#include <linux/clocksource.h>


/*
 * Version of feed-forward clock kernel support.
 */
#define FFCLOCK_VERSION 1


/*
 * Defines timestamping modes for the RADclock
 */
#define RADCLOCK_TSMODE_SYSCLOCK	1
#define RADCLOCK_TSMODE_RADCLOCK	2
#define RADCLOCK_TSMODE_FAIRCOMPARE	3


struct radclock_data
{
	double phat;
	double phat_err;
	double phat_local;
	double phat_local_err;
	long double ca;
	double ca_err;
	__u32 status;
	vcounter_t last_changed;
	vcounter_t valid_till;
};

struct radclock_fixedpoint
{
	/* phat as an int shifted phat_shift to the left */
	__u64 phat_int;
	/* Record of last time update from synchronization algorithm as an int */
	__u64 time_int;
	/* The counter value to convert in seconds */
	vcounter_t vcount;
	/* the shift amount for phat_int */
	__u8 phat_shift;
	/* the shift amount for time_int */
	__u8 time_shift;
	/* Warn if stamp is over this many bits */
	__u8 countdiff_maxbits;
};

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

#define RADCLOCK_NAME "radclock"

void radclock_fill_ktime(vcounter_t vcounter, ktime_t *ktime);

#endif

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
 * Deprecated... work with for now
 */
#define RADCLOCK_TSMODE_SYSCLOCK	1		// leave normal pcap ts field, hide vcount in padded hdr
#define RADCLOCK_TSMODE_RADCLOCK	2		// fill ts with RADclock Abs clock, hide vcount in hdr
#define RADCLOCK_TSMODE_FAIRCOMPARE	3	// as _SYSCLOCK, but ts and raw timestamps back-to-back



/* Copied from RADclock source file:  libradclock/radclock.h
 * Enumeration of the possible higher-level intents of the deamon.
 * All except PKTCAP_TSMODE_NOMODE set FLAG = NORMAL (UTC and !FAST) and
 * FCOUNTER = FFC  (try to get raw counter as well a normal timestamp).
 *
 * Descriptions below specify high level intent, whether fully possible given KV or not. */
enum pktcap_tsmode {
	PKTCAP_TSMODE_NOMODE = 0,			// no FF support in pcap, or very early versions
	PKTCAP_TSMODE_SYSCLOCK = 1,		// get raw, plus normal timestamp from sysclock
	PKTCAP_TSMODE_FBCLOCK = 2,			//                "                    FBclock
	PKTCAP_TSMODE_FFCLOCK = 3,			//                "                    FFclock (mono)
	PKTCAP_TSMODE_FFNATIVECLOCK = 4,	//                "                    FFclock (native)
	PKTCAP_TSMODE_FFDIFFCLOCK = 5,	//                "                    FF difference clock
	PKTCAP_TSMODE_RADCLOCK = 6,		//    "   , plus RADclock timestamp (userland)
	PKTCAP_TSMODE_CUSTOM = 100			// adopt the customised tsmode
};


/* Copied from RADclock source file: libradclock/radclock-private.h
 * Structure representing the radclock parameters
 */
struct radclock_data {
	double phat;				// very stable estimate of long term counter period [s]
	double phat_err;			// estimated bound on the relative error of phat [unitless]
	double phat_local;		//	stable estimate on shorter timescale period [s]
	double phat_local_err;  // estimated bound on the relative error of plocal [unitless]
	long double ca;			// K_p - thetahat(t) - leapsectotal? [s]
	double ca_err;				// estimated error (currently minET) in thetahat and ca [s]
	unsigned int status;		// status word (contains 10 bit fields)
	vcounter_t last_changed;	// raw timestamp T(tf) of last stamp processed [counter]
	vcounter_t next_expected;	// estimated T value of next stamp, and hence update [counter]
	vcounter_t leapsec_expected;	// estimated vcount of next leap, or 0 if none
	int leapsec_total;				// sum of leap seconds seen since clock start
	int leapsec_next;					// value of the expected next leap second {-1 0 1}
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

void radclock_fill_ktime(vcounter_t vcounter, ktime_t *ktime);

#endif

#ifndef __LINUX_FFCLOCK_H
#define __LINUX_FFCLOCK_H

#include <linux/ktime.h>


#ifdef CONFIG_FFCLOCK
/* Feed-forward time type (ffclock) */
typedef u64 ffcounter_t;


/* Managed by both the FFClock module and timekeeping.c */
extern struct feedforward_clock ffclock;


/* Timestamping modes for the FFClock */
#define FFCLOCK_TSMODE_SYSCLOCK	1
#define FFCLOCK_TSMODE_FFCLOCK	2


/* XXX XXX XXX TODO XXX XXX XXX
 *
 * Copyright / Licence issues !!! This is BSD code, we will have to change this
 * to make sure we do not create trouble.
 *
 * XXX XXX XXX TODO XXX XXX XXX
 */
struct bintime {
	int64_t sec;
	uint64_t frac;
};

static inline void bintime_add(struct bintime *bt, const struct bintime *bt2)
{
	uint64_t u;

	u = bt->frac;
	bt->frac += bt2->frac;
	if (u > bt->frac)
		bt->sec++;
	bt->sec += bt2->sec;
}

static inline void bintime_mul(struct bintime *bt, u_int x)
{
	uint64_t p1, p2;

	p1 = (bt->frac & 0xffffffffull) * x;
	p2 = (bt->frac >> 32) * x + (p1 >> 32);
	bt->sec *= x;
	bt->sec += (p2 >> 32);
	bt->frac = (p2 << 32) | (p1 & 0xffffffffull);
}

/* XXX XXX XXX XXX XXX XXX XXX XXX */

struct ffclock_data {
	/* Time conversion of ffcounter below */
	struct bintime time;
	/* Last synchronization daemon update or update_ffclock() */
	ffcounter_t ffcounter;
	/* Timecounter period estimate (<< per_shift) in nanoseconds */
	uint64_t period;
	/* Clock status word */
	uint32_t status;
	/* Average of clock error bound in nanoseconds */
	uint32_t error_bound_avg;
};

struct ffclock_estimate {
	uint8_t gen;
	struct ffclock_data cdata;
};

/* Current feed-forward estimation of time and the previous estimate */
struct feedforward_clock {
	uint8_t updated;
	struct ffclock_estimate *cest;
	struct ffclock_estimate *ocest;
	struct ffclock_estimate *ucest;
};


ffcounter_t read_ffcounter(void);
void ffcounter_to_ktime(const ffcounter_t *ffcounter, ktime_t *ktime);

#endif /* CONFIG_FFCLOCK */


#endif /* __LINUX_FFCLOCK_H */

/*-
 * SPDX-License-Identifier: Beerware
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * Copyright (c) 2011, 2015, 2016 The FreeBSD Foundation
 *
 * Portions of this software were developed by Julien Ridoux and Darryl Veitch
 * at the University of Melbourne under sponsorship from the FreeBSD Foundation.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ntp.h"
#include "opt_ffclock.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/sleepqueue.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/timeffc.h>
#include <sys/timepps.h>
#include <sys/timetc.h>
#include <sys/timex.h>
#include <sys/vdso.h>

/*
 * A large step happens on boot.  This constant detects such steps.
 * It is relatively small so that ntp_update_second gets called enough
 * in the typical 'missed a couple of seconds' case, but doesn't loop
 * forever when the time step is large.
 */
#define LARGE_STEP	200

/*
 * Implement a dummy timecounter which we can use until we get a real one
 * in the air.  This allows the console and other early stuff to use
 * time services.
 */

static u_int
dummy_get_timecount(struct timecounter *tc)
{
	static u_int now;

	return (++now);
}

static struct timecounter dummy_timecounter = {
	dummy_get_timecount, 0, ~0u, 1000000, "dummy", -1000000
};

struct timehands {
	/* These fields must be initialized by the driver. */
	struct timecounter	*th_counter;
	int64_t			th_adjustment;
	uint64_t		th_scale;
	u_int			th_large_delta;
	u_int	 		th_offset_count;
	struct bintime		th_offset;
	struct bintime		th_bintime;
	struct timeval		th_microtime;
	struct timespec		th_nanotime;
	struct bintime		th_boottime;
	/* Fields not to be copied in tc_windup start with th_generation. */
	u_int			th_generation;
	struct timehands	*th_next;
};

static struct timehands ths[16] = {
    [0] =  {
	.th_counter = &dummy_timecounter,
	.th_scale = (uint64_t)-1 / 1000000,
	.th_large_delta = 1000000,
	.th_offset = { .sec = 1 },
	.th_generation = 1,
    },
};

static struct timehands *volatile timehands = &ths[0];
struct timecounter *timecounter = &dummy_timecounter;
static struct timecounter *timecounters = &dummy_timecounter;

/* Mutex to protect the timecounter list. */
static struct mtx tc_lock;

int tc_min_ticktock_freq = 1;

volatile time_t time_second = 1;
volatile time_t time_uptime = 1;

static int sysctl_kern_boottime(SYSCTL_HANDLER_ARGS);
SYSCTL_PROC(_kern, KERN_BOOTTIME, boottime,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_kern_boottime, "S,timeval",
    "System boottime");

SYSCTL_NODE(_kern, OID_AUTO, timecounter, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "");
static SYSCTL_NODE(_kern_timecounter, OID_AUTO, tc,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "");

static int timestepwarnings;
SYSCTL_INT(_kern_timecounter, OID_AUTO, stepwarnings, CTLFLAG_RWTUN,
    &timestepwarnings, 0, "Log time steps");

static int timehands_count = 2;
SYSCTL_INT(_kern_timecounter, OID_AUTO, timehands_count,
    CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
    &timehands_count, 0, "Count of timehands in rotation");

struct bintime bt_timethreshold;
struct bintime bt_tickthreshold;
sbintime_t sbt_timethreshold;
sbintime_t sbt_tickthreshold;
struct bintime tc_tick_bt;
sbintime_t tc_tick_sbt;
int tc_precexp;
int tc_timepercentage = TC_DEFAULTPERC;
static int sysctl_kern_timecounter_adjprecision(SYSCTL_HANDLER_ARGS);
SYSCTL_PROC(_kern_timecounter, OID_AUTO, alloweddeviation,
    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, 0, 0,
    sysctl_kern_timecounter_adjprecision, "I",
    "Allowed time interval deviation in percents");

volatile int rtc_generation = 1;

static int tc_chosen;	/* Non-zero if a specific tc was chosen via sysctl. */
static char tc_from_tunable[16];

static void tc_windup(struct bintime *new_boottimebin);
static void cpu_tick_calibrate(int);

void dtrace_getnanotime(struct timespec *tsp);
void dtrace_getnanouptime(struct timespec *tsp);

static int
sysctl_kern_boottime(SYSCTL_HANDLER_ARGS)
{
	struct timeval boottime;

	getboottime(&boottime);

/* i386 is the only arch which uses a 32bits time_t */
#ifdef __amd64__
#ifdef SCTL_MASK32
	int tv[2];

	if (req->flags & SCTL_MASK32) {
		tv[0] = boottime.tv_sec;
		tv[1] = boottime.tv_usec;
		return (SYSCTL_OUT(req, tv, sizeof(tv)));
	}
#endif
#endif
	return (SYSCTL_OUT(req, &boottime, sizeof(boottime)));
}

static int
sysctl_kern_timecounter_get(SYSCTL_HANDLER_ARGS)
{
	u_int ncount;
	struct timecounter *tc = arg1;

	ncount = tc->tc_get_timecount(tc);
	return (sysctl_handle_int(oidp, &ncount, 0, req));
}

static int
sysctl_kern_timecounter_freq(SYSCTL_HANDLER_ARGS)
{
	uint64_t freq;
	struct timecounter *tc = arg1;

	freq = tc->tc_frequency;
	return (sysctl_handle_64(oidp, &freq, 0, req));
}

/*
 * Return the difference between the timehands' counter value now and what
 * was when we copied it to the timehands' offset_count.
 */
static __inline u_int
tc_delta(struct timehands *th)
{
	struct timecounter *tc;

	tc = th->th_counter;
	return ((tc->tc_get_timecount(tc) - th->th_offset_count) &
	    tc->tc_counter_mask);
}

static __inline void
bintime_add_tc_delta(struct bintime *bt, uint64_t scale,
    uint64_t large_delta, uint64_t delta)
{
	uint64_t x;

	if (__predict_false(delta >= large_delta)) {
		/* Avoid overflow for scale * delta. */
		x = (scale >> 32) * delta;
		bt->sec += x >> 32;
		bintime_addx(bt, x << 32);
		bintime_addx(bt, (scale & 0xffffffff) * delta);
	} else {
		bintime_addx(bt, scale * delta);
	}
}

/*
 * Functions for reading the time.  We have to loop until we are sure that
 * the timehands that we operated on was not updated under our feet.  See
 * the comment in <sys/time.h> for a description of these 12 functions.
 */

static __inline void
bintime_off(struct bintime *bt, u_int off)
{
	struct timehands *th;
	struct bintime *btp;
	uint64_t scale;
	u_int delta, gen, large_delta;

	do {
		th = timehands;
		gen = atomic_load_acq_int(&th->th_generation);
		btp = (struct bintime *)((vm_offset_t)th + off);
		*bt = *btp;
		scale = th->th_scale;
		delta = tc_delta(th);
		large_delta = th->th_large_delta;
		atomic_thread_fence_acq();
	} while (gen == 0 || gen != th->th_generation);

	bintime_add_tc_delta(bt, scale, large_delta, delta);
}
#define	GETTHBINTIME(dst, member)					\
do {									\
	_Static_assert(_Generic(((struct timehands *)NULL)->member,	\
	    struct bintime: 1, default: 0) == 1,			\
	    "struct timehands member is not of struct bintime type");	\
	bintime_off(dst, __offsetof(struct timehands, member));		\
} while (0)

static __inline void
getthmember(void *out, size_t out_size, u_int off)
{
	struct timehands *th;
	u_int gen;

	do {
		th = timehands;
		gen = atomic_load_acq_int(&th->th_generation);
		memcpy(out, (char *)th + off, out_size);
		atomic_thread_fence_acq();
	} while (gen == 0 || gen != th->th_generation);
}
#define	GETTHMEMBER(dst, member)					\
do {									\
	_Static_assert(_Generic(*dst,					\
	    __typeof(((struct timehands *)NULL)->member): 1,		\
	    default: 0) == 1,						\
	    "*dst and struct timehands member have different types");	\
	getthmember(dst, sizeof(*dst), __offsetof(struct timehands,	\
	    member));							\
} while (0)

#ifdef FFCLOCK
void
fbclock_binuptime(struct bintime *bt)
{

	GETTHBINTIME(bt, th_offset);
}

void
fbclock_nanouptime(struct timespec *tsp)
{
	struct bintime bt;

	fbclock_binuptime(&bt);
	bintime2timespec(&bt, tsp);
}

void
fbclock_microuptime(struct timeval *tvp)
{
	struct bintime bt;

	fbclock_binuptime(&bt);
	bintime2timeval(&bt, tvp);
}

void
fbclock_bintime(struct bintime *bt)
{

	GETTHBINTIME(bt, th_bintime);
}

void
fbclock_nanotime(struct timespec *tsp)
{
	struct bintime bt;

	fbclock_bintime(&bt);
	bintime2timespec(&bt, tsp);
}

void
fbclock_microtime(struct timeval *tvp)
{
	struct bintime bt;

	fbclock_bintime(&bt);
	bintime2timeval(&bt, tvp);
}

void
fbclock_getbinuptime(struct bintime *bt)
{

	GETTHMEMBER(bt, th_offset);
}

void
fbclock_getnanouptime(struct timespec *tsp)
{
	struct bintime bt;

	GETTHMEMBER(&bt, th_offset);
	bintime2timespec(&bt, tsp);
}

void
fbclock_getmicrouptime(struct timeval *tvp)
{
	struct bintime bt;

	GETTHMEMBER(&bt, th_offset);
	bintime2timeval(&bt, tvp);
}

void
fbclock_getbintime(struct bintime *bt)
{

	GETTHMEMBER(bt, th_bintime);
}

void
fbclock_getnanotime(struct timespec *tsp)
{

	GETTHMEMBER(tsp, th_nanotime);
}

void
fbclock_getmicrotime(struct timeval *tvp)
{

	GETTHMEMBER(tvp, th_microtime);
}
#else /* !FFCLOCK */

void
binuptime(struct bintime *bt)
{

	GETTHBINTIME(bt, th_offset);
}

void
nanouptime(struct timespec *tsp)
{
	struct bintime bt;

	binuptime(&bt);
	bintime2timespec(&bt, tsp);
}

void
microuptime(struct timeval *tvp)
{
	struct bintime bt;

	binuptime(&bt);
	bintime2timeval(&bt, tvp);
}

void
bintime(struct bintime *bt)
{

	GETTHBINTIME(bt, th_bintime);
}

void
nanotime(struct timespec *tsp)
{
	struct bintime bt;

	bintime(&bt);
	bintime2timespec(&bt, tsp);
}

void
microtime(struct timeval *tvp)
{
	struct bintime bt;

	bintime(&bt);
	bintime2timeval(&bt, tvp);
}

void
getbinuptime(struct bintime *bt)
{

	GETTHMEMBER(bt, th_offset);
}

void
getnanouptime(struct timespec *tsp)
{
	struct bintime bt;

	GETTHMEMBER(&bt, th_offset);
	bintime2timespec(&bt, tsp);
}

void
getmicrouptime(struct timeval *tvp)
{
	struct bintime bt;

	GETTHMEMBER(&bt, th_offset);
	bintime2timeval(&bt, tvp);
}

void
getbintime(struct bintime *bt)
{

	GETTHMEMBER(bt, th_bintime);
}

void
getnanotime(struct timespec *tsp)
{

	GETTHMEMBER(tsp, th_nanotime);
}

void
getmicrotime(struct timeval *tvp)
{

	GETTHMEMBER(tvp, th_microtime);
}
#endif /* FFCLOCK */

void
getboottime(struct timeval *boottime)
{
	struct bintime boottimebin;

	getboottimebin(&boottimebin);
	bintime2timeval(&boottimebin, boottime);
}

void
getboottimebin(struct bintime *boottimebin)
{

	GETTHMEMBER(boottimebin, th_boottime);
}

#ifdef FFCLOCK
/*
 * Support for feed-forward synchronization algorithms. This is heavily inspired
 * by the timehands mechanism but kept independent from it. *_windup() functions
 * have some connection to avoid accessing the timecounter hardware more than
 * necessary.
 */

/* Feed-forward clock estimates kept updated by the synchronization daemon. */
struct ffclock_estimate ffclock_estimate;
struct bintime ffclock_boottime;	/* Feed-forward boot time estimate. */
uint32_t ffclock_status;		/* Feed-forward clock status. */
int8_t ffclock_updated;			/* New estimates are available. */
struct mtx ffclock_mtx;			/* Mutex on ffclock_estimate. */

struct fftimehands {
	struct ffclock_estimate	cest;
	struct bintime		tick_time;
	struct bintime		tick_time_lerp;
	struct bintime		tick_time_diff;
	struct bintime		tick_error;
	ffcounter		tick_ffcount;
	uint64_t		period_lerp;
	volatile uint8_t	gen;
	struct fftimehands	*next;
};

#define	NUM_ELEMENTS(x) (sizeof(x) / sizeof(*x))

static struct fftimehands ffth[10];
static struct fftimehands *volatile fftimehands = ffth;

static void
ffclock_init(void)
{
	struct fftimehands *cur;
	struct fftimehands *last;

	memset(ffth, 0, sizeof(ffth));

	last = ffth + NUM_ELEMENTS(ffth) - 1;
	for (cur = ffth; cur < last; cur++)
		cur->next = cur + 1;
	last->next = ffth;

	ffclock_updated = 0;
	ffclock_status = FFCLOCK_STA_UNSYNC;
	ffclock_boottime.sec = ffclock_boottime.frac = 0;
	mtx_init(&ffclock_mtx, "ffclock lock", NULL, MTX_DEF);
}


/*
 * Sub-routine to convert a time interval measured in RAW counter units to time
 * in seconds stored in binary fraction units, when the time interval may be
 * over one second.  If is is guaranteed to be under a second, then
 *   bintime_addx(bt, period * ffdelta);
 * should be used instead.
 * NOTE: bintime_mul requires u_int, but the value of the ffcounter may be
 * larger than the max value of u_int (on 32 bit architecture). Loop to consume
 * extra cycles.
 */
static void
ffclock_convert_delta(ffcounter ffdelta, uint64_t period, struct bintime *bt)
{
	struct bintime bt2;
	ffcounter delta, delta_max;

	delta_max = (1ULL << (8 * sizeof(unsigned int))) - 1;
	bintime_clear(bt);
	do {
		if (ffdelta > delta_max)
			delta = delta_max;
		else
			delta = ffdelta;
		bt2.sec = 0;
		bt2.frac = period;
		bintime_mul(&bt2, (unsigned int)delta);
		bintime_add(bt, &bt2);
		ffdelta -= delta;
	} while (ffdelta > 0);
}


/*
 * Update the fftimehands. The updated tick state is based on the previous tick if
 * there has been no actionable update in the FFclock parameters during the current
 * tick (ffclock_updated <= 0), and each of the native, monotonic, and difference
 * FFclocks advance linearly. Otherwise it is based off the updated parameters at
 * the time of the update. The native FFclock will then jump, the monotonic clock
 * will not (except under special conditions).  The diff clock will never
 * jump, to ensure its intended use as a difference clock, used to measure
 * time differences. The linear interpolation parameters of the
 * monotonic FFclock ({tick_time,period}_lerp) are recomputed for the new tick.
 *
 * The instant defining the start of the new tick is the  delta=tc_delta call
 *	from tc_windup. This is simply mirrored here in the FF counter `read'.
 *
 * If a RTC reset occurs, then tc_windup is called within tc_setclock with a
 * bootime argument, passed here as reset_FBbootime. If non-NULL, FFclocks are
 * reset using this and the UTC reset calculated in tc_windup, and FFdata is
 * reinitialized to basic values.
 */
static void
ffclock_windup(unsigned int delta, struct bintime *reset_FBbootime,
					struct bintime *reset_UTC)
{
	struct ffclock_estimate *cest;
	struct fftimehands *ffth;
	struct bintime bt, gap_lerp, upt;
	ffcounter ffdelta;
	uint64_t frac;
	uint8_t forward_jump, ogen;

	forward_jump = 0;
	
	/* Prepare next fftimehand where tick state will be updated */
	ffth = fftimehands->next;
	ogen = ffth->gen;
	ffth->gen = 0;
	cest = &ffth->cest;

	/* Move FF counter forward to existing tick start */
	ffdelta = (ffcounter)delta;
	ffth->tick_ffcount = fftimehands->tick_ffcount + ffdelta;

	/*
	 * RTC reset: reset all FFclocks, and the global FFdata.
	 * The period is initialized only if needed.
	 */
	if (reset_FBbootime) {
		memcpy(cest, &ffclock_estimate, sizeof(struct ffclock_estimate));

		/*
		 * Determine value of ffclock_boottime to maximize Upclock continuity.
		 * sysclock = FB :  kernel will not see a jump now, better to align FF
		 *                  and FB to minimize jump if sysclock changes
		 *          = FF :  ensure continuity in FF and hence sysclock Uptime
		 */
		if (sysclock_active == SYSCLOCK_FB)
			ffclock_boottime = *reset_FBbootime;
		else {
			/* First calculate what FFmono would be at tick start, if not reset */
			ffth->tick_time_lerp = fftimehands->tick_time_lerp;
			ffclock_convert_delta(ffdelta, ffth->period_lerp, &bt);
			bintime_add(&ffth->tick_time_lerp, &bt);

			/* Cancel out jump in Uptime due to reset */
			bintime_clear(&gap_lerp);
			if (bintime_cmp(reset_UTC, &ffth->tick_time_lerp, >)) {
				gap_lerp = *reset_UTC;
				bintime_sub(&gap_lerp, &ffth->tick_time_lerp);
				bintime_add(&ffclock_boottime, &gap_lerp);
			} else {
				gap_lerp = ffth->tick_time_lerp;
				bintime_sub(&gap_lerp, reset_UTC);
				bintime_sub(&ffclock_boottime, &gap_lerp);
			}
		}

		/* For UTC clocks, align FF to the FB reset derived from the RTC reset */
		ffth->tick_time = *reset_UTC;
		ffth->tick_time_lerp = *reset_UTC;
		ffth->tick_time_diff = *reset_UTC;

		/* Reset FFdata to reflect the reset, taken to occur at tick-start. */
		cest->update_time = *reset_UTC;
		cest->update_ffcount = ffth->tick_ffcount;
		if (cest->period == 0)		// if never set
			cest->period = ((1ULL << 63)/ timehands->th_counter->tc_frequency) << 1;

		cest->errb_abs = 0;
		cest->errb_rate = 0;
		cest->status = FFCLOCK_STA_UNSYNC;
		cest->secs_to_nextupdate = 0;		// signals no daemon update since reset
		cest->leapsec_expected = 0;
		cest->leapsec_total = 0;
		cest->leapsec_next = 0;
		mtx_lock(&ffclock_mtx);
		memcpy(&ffclock_estimate, cest, sizeof(struct ffclock_estimate));
		ffclock_updated = 0;		// signal no daemon update to process
		mtx_unlock(&ffclock_mtx);

		ffclock_status = FFCLOCK_STA_UNSYNC;

		/* Reset remaining fftimehands members */
		ffth->tick_error.sec = ffth->tick_error.frac = 0;
		ffth->period_lerp = cest->period;

		upt = ffth->tick_time_lerp;
		bintime_sub(&upt, &ffclock_boottime);
		printf("FFclock processing RTC reset: UTC: %ld.%03lu"
				 " boottime: %llu.%03lu, uptime: %llu.%03lu\n",
				(long)ffth->tick_time_lerp.sec,
				(unsigned long)(ffth->tick_time_lerp.frac / MS_AS_BINFRAC),
				(unsigned long long)ffclock_boottime.sec,
				(long unsigned)(ffclock_boottime.frac / MS_AS_BINFRAC),
				(unsigned long long)upt.sec,
				(long unsigned)(upt.frac / MS_AS_BINFRAC) );

	}



	/*
	 * Signal to ignore a stale (pre-reset) daemon update following a RTC reset.
	 */
	if (ffclock_updated > 0 && fftimehands->cest.secs_to_nextupdate == 0
	                        && bintime_cmp(&fftimehands->cest.update_time, \
	                                       &ffclock_estimate.update_time,>) ) {
		ffclock_updated = 0;
		printf("Ignoring stale FFdata update following RTC reset.\n");
	}


	/*
	 * No acceptable update in FFclock parameters to process.
	 * Includes case of daemon update following a RTC reset that must be ignored.
	 * Tick state update based on copy or simple projection from previous tick.
	 */
	if (ffclock_updated <= 0 && reset_FBbootime == NULL) {

		/* Update native FFclock members {cest, tick_time{_diff}, tick_error} */
		memcpy(cest, &fftimehands->cest, sizeof(struct ffclock_estimate));
		ffth->tick_time      = fftimehands->tick_time;
		ffth->tick_time_diff = fftimehands->tick_time_diff;
		ffclock_convert_delta(ffdelta, cest->period, &bt);  // use bintime_addx(bt, period * ffdelta); ?
		bintime_add(&ffth->tick_time, &bt);
		bintime_add(&ffth->tick_time_diff, &bt);
		bintime_mul(&bt, cest->errb_rate * PS_AS_BINFRAC);	// errb_rate in [ps/s]
		bintime_add(&ffth->tick_error, &bt);

		/* Update mono FFclock members {period_lerp, tick_time_lerp} */
		ffth->period_lerp    = fftimehands->period_lerp;
		ffth->tick_time_lerp = fftimehands->tick_time_lerp;
		ffclock_convert_delta(ffdelta, ffth->period_lerp, &bt);
		bintime_add(&ffth->tick_time_lerp, &bt);

		/* Check if the clock status should be set to unsynchronized.
		 * Assessment based on age of last/current update, and the daemon's
		 * estimate of the wait to the next update.
		 * If the daemon's UNSYNC status is deemed too stale, it is over-ridden.
		 */
		if (ffclock_updated == 0) {
			bt = ffth->tick_time;
			bintime_sub(&bt, &cest->update_time);	// bt = now - timeoflastupdate
			if (bt.sec > 3 * FFCLOCK_SKM_SCALE &&
			    bt.sec > 3 * cest->secs_to_nextupdate)
				ffclock_status |= FFCLOCK_STA_UNSYNC;
		}
		
	}
	
	/*
	 * An update in FFclock parameters is available in this tick.
	 * Generate the new tick state based on this, projected from the update time.
	 */
	if (ffclock_updated > 0) {

		/* Update native FFclock members {cest, tick_time, tick_error} */
		memcpy(cest, &ffclock_estimate, sizeof(struct ffclock_estimate));
		ffdelta = ffth->tick_ffcount - cest->update_ffcount; // time since update
		ffth->tick_time = cest->update_time;
		ffclock_convert_delta(ffdelta, cest->period, &bt);
		bintime_add(&ffth->tick_time, &bt);
		bintime_mul(&bt, cest->errb_rate * PS_AS_BINFRAC);	// errb_rate in [ps/s]
		bintime_addx(&bt, cest->errb_abs * NS_AS_BINFRAC);	// errb_abs in [ns]
		ffth->tick_error = bt;

		/*
		 * Update native FF difference clock member {tick_time_diff},
		 * ensuring continuity over ticks.
		 */
		ffth->tick_time_diff = fftimehands->tick_time_diff;
		ffclock_convert_delta((ffcounter)delta, cest->period, &bt);
		bintime_add(&ffth->tick_time_diff, &bt);

		/*
		 * Update mono FFclock member tick_time_lerp, standard case,
		 * ensuring continuity over ticks.
		 */
		ffth->tick_time_lerp = fftimehands->tick_time_lerp;
		ffclock_convert_delta((ffcounter)delta, fftimehands->period_lerp, &bt);
		bintime_add(&ffth->tick_time_lerp, &bt);

		/* Record dirn of jump between monoFFclock and FFclock at tick-start */
		if (bintime_cmp(&ffth->tick_time, &ffth->tick_time_lerp, >))
			forward_jump = 1;		// else = 0

		/* Record magnitude of jump */
		bintime_clear(&gap_lerp);
		if (forward_jump) {		// monoFFclock < FFclock
			gap_lerp = ffth->tick_time;
			bintime_sub(&gap_lerp, &ffth->tick_time_lerp);
		} else {
			gap_lerp = ffth->tick_time_lerp;
			bintime_sub(&gap_lerp, &ffth->tick_time);
		}

		/*
		 * Update mono FFclock member tick_time_lerp, exceptional case.
		 * Break monotonicity by allowing monoFFclock jump to meet native FFclock
		 * Only occurs under tight conditions to prevent a poor monoFFclock
		 * initialization from taking a very long time to catch up to FFclock.
		 * Absorb the jump into ffclock_boottime to ensure continuity of
		 * uptime functions.
		 */
		if (((ffclock_status & FFCLOCK_STA_UNSYNC) == FFCLOCK_STA_UNSYNC) &&
		    ((cest->status & FFCLOCK_STA_UNSYNC) == 0) ) {
			if (forward_jump) {
				printf("ffwindup:  Jumping monotonic FFclock forward by %llu.%03lu",
				         (unsigned long long)gap_lerp.sec,
				         (long unsigned)(gap_lerp.frac / MS_AS_BINFRAC) );
				bintime_add(&ffclock_boottime, &gap_lerp);
			} else {
				printf("ffwindup:  Jumping monotonic FFclock backward by %llu.%03lu",
				         (unsigned long long)gap_lerp.sec,
				         (long unsigned)(gap_lerp.frac / MS_AS_BINFRAC) );
				bintime_sub(&ffclock_boottime, &gap_lerp);
			}

			ffth->tick_time_lerp = ffth->tick_time;

			upt = ffth->tick_time_lerp;
			bintime_sub(&upt, &ffclock_boottime);
			printf(" (uptime preserved at: %llu.%03lu)\n",
			            (unsigned long long)upt.sec,
			            (long unsigned)(upt.frac / MS_AS_BINFRAC) );

			bintime_clear(&gap_lerp); // signal nothing to do to period_lerp algo
		}


		/*
		 * Update mono FFclock member period_lerp
		 * The goal of the monoFF algorithm is to reduce the gap between monoFF and the
		 * native FF to zero by the next FFclock update. The reduction uses linear
		 * interpolation via selecting period_lerp.  To ensure rate quality,
		 * |period_lerp - period| is capped to 5000PPM (5ms/s).
		 * If there is no gap, the clocks will agree throughout the new tick.
		 */
		ffth->period_lerp = cest->period;   // re-initialize

		/* Keep default if no (or negligible) gap or no daemon updates yet */
		if (bintime_isset(&gap_lerp) && cest->secs_to_nextupdate > 0) {

			/* Calculate cap */
			bt.sec = 0;
			bt.frac = 5000000 * NS_AS_BINFRAC;
			bintime_mul(&bt, cest->secs_to_nextupdate);

			/* Determine the amount of gap to close over the next update interval */
			if (bintime_cmp(&gap_lerp, &bt, >))
				gap_lerp = bt;		// gap_lerp = min(gap_lerp, bt)

			/* Convert secs_to_nextupdate to counter units */
			frac = 0;
			frac -= 1;		// approximate 2^64 with (2^64)-1 to ease arithmetic
			ffdelta = (frac / cest->period) * cest->secs_to_nextupdate;

			/* Store the portion of gap per cycle in frac */
			frac = 0;
			if (gap_lerp.sec > 0) {
				frac -= 1;
				frac /= ffdelta / gap_lerp.sec;
				//frac = (cest->period * gap_lerp.sec) / cest->secs_to_nextupdate;
			}
			frac += gap_lerp.frac / ffdelta;	// very small gaps rounded to zero

			if (forward_jump)
				ffth->period_lerp += frac;
			else
				ffth->period_lerp -= frac;

		}

		ffclock_status = cest->status;	// unsets FFCLOCK_STA_UNSYNC
		ffclock_updated = 0;		// signal that latest update processed
	}

	/* Bump generation of new tick, avoiding the reserved 0 value */
	if (++ogen == 0)
		ogen = 1;
	ffth->gen = ogen;

	fftimehands = ffth;

}


/*
 * Adjust the fftimehands when the timecounter is changed. Stating the obvious,
 * the old and new hardware counter cannot be read simultaneously.
 * Because there is no locking here, simply force to ignore pending or next
 * update to give daemon a chance to realize the counter has changed.
 */
static void
ffclock_change_tc(struct timehands *th, u_int ncount)
{
	struct fftimehands *ffth;
	struct ffclock_estimate *cest;
	struct timecounter *tc;
	uint8_t ogen;
	ffcounter now;

	tc = th->th_counter;
	
	/* Prepare next fftimehand where tick state will be updated */
	ffth = fftimehands->next;
	ogen = ffth->gen;
	ffth->gen = 0;
	cest = &ffth->cest;
	
	/*
	 * Reset FFcounter to match start of current tick.
	 *  If a TSC derived counter, get correct higher order bits to ensure the
	 *  FFcounter origin matches that of the counter, rather than the time the
	 *  counter was adopted. If not TSC, the origin will be ncount in the past.
	 *  In all cases, the lower bits of FFcounter and th_offset_count will agree.
	 */
	if ( strcmp(tc->tc_name, "TSC") != 0 )
		ffth->tick_ffcount = (ffcounter)ncount;  // origin matches timehands
	else {
		now = (ffcounter) rdtsc();
		if (strcmp(tc->tc_name, "TSC-low") == 0) // TSC reads are shifted
			now >>= (int)(intptr_t)tc->tc_priv;
		/* reconstruct the counter value at the time ncount was taken */
		ffth->tick_ffcount = now - (ffcounter)((u_int)now - ncount);
	}

	/*
	 * This update does not advance the tick itself, intead it reinitiates when
	 * existing values are not appropriate for the new counter.
	 * Note cest->update_time is still valid as a record of when the
	 * data was changed, wrt the new counter.
	 */
	memcpy(cest, &(fftimehands->cest), sizeof(struct ffclock_estimate));
	cest->update_ffcount = ffth->tick_ffcount;
	cest->secs_to_nextupdate = 0;
	cest->period = ((1ULL << 63) / tc->tc_frequency ) << 1;
	cest->errb_abs = 0;
	cest->errb_rate = 0;
	cest->status |= FFCLOCK_STA_UNSYNC;
	cest->leapsec_expected = 0;
	cest->leapsec_total = 0;
	cest->leapsec_next = 0;

	ffth->tick_time = fftimehands->tick_time;
	ffth->tick_error = fftimehands->tick_error;
	ffth->tick_time_diff = fftimehands->tick_time_diff;
	ffth->tick_time_lerp = fftimehands->tick_time_lerp;
	ffth->period_lerp = cest->period;
	
	/* Push the reset FFdata to the global variable */
	mtx_lock(&ffclock_mtx);
	memcpy(&ffclock_estimate, cest, sizeof(struct ffclock_estimate));
	mtx_unlock(&ffclock_mtx);

	/* Do not lock but ignore next update from synchronization daemon. */
	ffclock_updated--;
	
	if (++ogen == 0)
		ogen = 1;
	ffth->gen = ogen;
	fftimehands = ffth;
	
	printf("ffclock_change_tc: new tick_ffcount = %llu = %#llX, with tc %s (%llu Hz)\n",
	    (unsigned long long)ffth->tick_ffcount,
	    (unsigned long long)ffth->tick_ffcount,
	    tc->tc_name, (unsigned long long)tc->tc_frequency);
}

/*
 * Retrieve feed-forward counter and time of last kernel tick.
 */
void
ffclock_last_tick(ffcounter *ffcount, struct bintime *bt, uint32_t flags)
{
	struct fftimehands *ffth;
	uint8_t gen;

	/* No locking but check generation has not changed. */
	do {
		ffth = fftimehands;
		gen = ffth->gen;
		if ((flags & FFCLOCK_LERP) == FFCLOCK_LERP)
			*bt = ffth->tick_time_lerp;
		else
			*bt = ffth->tick_time;
		*ffcount = ffth->tick_ffcount;
	} while (gen == 0 || gen != ffth->gen);
}

/*
 * Absolute clock conversion. Low level function to convert ffcounter to
 * bintime. The ffcounter is converted using the current ffclock period estimate
 * or the "interpolated period" to ensure monotonicity.
 * NOTE: this conversion may have been deferred, and the clock updated since the
 * hardware counter has been read.
 */
void
ffclock_convert_abs(ffcounter ffcount, struct bintime *bt, uint32_t flags)
{
	struct fftimehands *ffth;
	struct bintime bt2;
	ffcounter ffdelta;
	uint8_t gen;

	/* No locking but check generation has not changed */
	do {
		ffth = fftimehands;
		gen = ffth->gen;
		if (ffcount > ffth->tick_ffcount)
			ffdelta = ffcount - ffth->tick_ffcount;
		else
			ffdelta = ffth->tick_ffcount - ffcount;

		if ((flags & FFCLOCK_LERP) == FFCLOCK_LERP) {
			*bt = ffth->tick_time_lerp;
			ffclock_convert_delta(ffdelta, ffth->period_lerp, &bt2);
		} else {
			*bt = ffth->tick_time;
			ffclock_convert_delta(ffdelta, ffth->cest.period, &bt2);
		}

		if (ffcount > ffth->tick_ffcount)
			bintime_add(bt, &bt2);
		else
			bintime_sub(bt, &bt2);
	} while (gen == 0 || gen != ffth->gen);
}

/*
 * Difference clock conversion.
 * Low level function to Convert a time interval measured in RAW counter units
 * into bintime. The difference clock allows measuring small intervals much more
 * reliably than the absolute clock.
 */
void
ffclock_convert_diff(ffcounter ffdelta, struct bintime *bt)
{
	struct fftimehands *ffth;
	uint8_t gen;

	/* No locking but check generation has not changed. */
	do {
		ffth = fftimehands;
		gen = ffth->gen;
		ffclock_convert_delta(ffdelta, ffth->cest.period, bt);
	} while (gen == 0 || gen != ffth->gen);
}

/*
 * Access to current ffcounter value.
 * If bypass mode on, assume the counter is TSC, and access it directly.
 */
void
ffclock_read_counter(ffcounter *ffcount)
{
	struct timehands *th;
	struct fftimehands *ffth;
	unsigned int gen, delta;

	if (sysctl_kern_ffclock_ffcounter_bypass == 1) {
		*ffcount = (ffcounter) rdtsc();
		return;
	}
	
	/*
	 * ffclock_windup() called from tc_windup(), safe to rely on
	 * th->th_generation only, for correct delta and ffcounter.
	 */
	do {
		th = timehands;
		gen = atomic_load_acq_int(&th->th_generation);
		ffth = fftimehands;
		delta = tc_delta(th);
		*ffcount = ffth->tick_ffcount;
		atomic_thread_fence_acq();
	} while (gen == 0 || gen != th->th_generation);

	*ffcount += delta;
}

void
binuptime(struct bintime *bt)
{

	binuptime_fromclock(bt, sysclock_active);
}

void
nanouptime(struct timespec *tsp)
{

	nanouptime_fromclock(tsp, sysclock_active);
}

void
microuptime(struct timeval *tvp)
{

	microuptime_fromclock(tvp, sysclock_active);
}

void
bintime(struct bintime *bt)
{

	bintime_fromclock(bt, sysclock_active);
}

void
nanotime(struct timespec *tsp)
{

	nanotime_fromclock(tsp, sysclock_active);
}

void
microtime(struct timeval *tvp)
{

	microtime_fromclock(tvp, sysclock_active);
}

void
getbinuptime(struct bintime *bt)
{

	getbinuptime_fromclock(bt, sysclock_active);
}

void
getnanouptime(struct timespec *tsp)
{

	getnanouptime_fromclock(tsp, sysclock_active);
}

void
getmicrouptime(struct timeval *tvp)
{

	getmicrouptime_fromclock(tvp, sysclock_active);
}

void
getbintime(struct bintime *bt)
{

	getbintime_fromclock(bt, sysclock_active);
}

void
getnanotime(struct timespec *tsp)
{

	getnanotime_fromclock(tsp, sysclock_active);
}

void
getmicrotime(struct timeval *tvp)
{

	getmicrouptime_fromclock(tvp, sysclock_active);
}

#endif /* FFCLOCK */

/*
 * This is a clone of getnanotime and used for walltimestamps.
 * The dtrace_ prefix prevents fbt from creating probes for
 * it so walltimestamp can be safely used in all fbt probes.
 */
void
dtrace_getnanotime(struct timespec *tsp)
{

	GETTHMEMBER(tsp, th_nanotime);
}

/*
 * This is a clone of getnanouptime used for time since boot.
 * The dtrace_ prefix prevents fbt from creating probes for
 * it so an uptime that can be safely used in all fbt probes.
 */
void
dtrace_getnanouptime(struct timespec *tsp)
{
	struct bintime bt;

	GETTHMEMBER(&bt, th_offset);
	bintime2timespec(&bt, tsp);
}

/*
 * System clock currently providing time to the system. Modifiable via sysctl
 * when the FFCLOCK option is defined.
 */
int sysclock_active = SYSCLOCK_FB;

/* Internal NTP status and error estimates. */
extern int time_status;
extern long time_esterror;

/*
 * Take a raw timestamp (timecounter reading), and then snapshot the sysclock
 * data which can be used to compare system clocks and generate timestamps
 * of all possible types after the fact.
 * If bypass mode on, assume the counter is TSC, and access it directly.
 */
void
sysclock_getsnapshot(struct sysclock_snap *clock_snap, int fast)
{
	struct fbclock_info *fbi;
	struct timehands *th;
	struct bintime bt;
	unsigned int delta, gen;
#ifdef FFCLOCK
	struct ffclock_info *ffi;
	struct fftimehands *ffth;
	struct ffclock_estimate cest;
#endif

	delta = 0;
	do {
		th = timehands;
		gen = atomic_load_acq_int(&th->th_generation);
		if (!fast) {
#ifdef FFCLOCK
			if (sysctl_kern_ffclock_ffcounter_bypass == 1)
				delta = rdtsc32() - th->th_offset_count;
			else
#endif
				delta = tc_delta(th);
		}
		fbi = &clock_snap->fb_info;
		fbi->th_scale = th->th_scale;
		fbi->tick_time = th->th_offset;
#ifdef FFCLOCK
		ffi = &clock_snap->ff_info;
		ffth = fftimehands;
		ffi->tick_time = ffth->tick_time;
		ffi->tick_time_diff = ffth->tick_time_diff;
		ffi->tick_time_lerp = ffth->tick_time_lerp;
		ffi->period = ffth->cest.period;
		ffi->period_lerp = ffth->period_lerp;
		clock_snap->ffcount = ffth->tick_ffcount;
		cest = ffth->cest;
#endif
		atomic_thread_fence_acq();
	} while (gen == 0 || gen != th->th_generation);

	clock_snap->delta = delta;
	clock_snap->sysclock_active = sysclock_active;

	/* Record feedback clock status and error. */
	fbi->status = time_status;
	/* XXX: Very crude estimate of feedback clock error. */
	bt.sec = time_esterror / 1000000;			// time_esterror is in mus
	bt.frac = (time_esterror - bt.sec * 1000000) * MUS_AS_BINFRAC;
	fbi->error = bt;

#ifdef FFCLOCK
	if (!fast)
		clock_snap->ffcount += delta;

	/* Precalculate total leap second adjustment appropriate to this ffcount.
	 * Includes total leaps so far and impending leap ffcount may have surpassed.
	 */
	ffi->leapsec_adjustment = cest.leapsec_total;
	if (cest.leapsec_expected != 0 && clock_snap->ffcount > cest.leapsec_expected)
		ffi->leapsec_adjustment += cest.leapsec_next;

	/* Record feed-forward clock status and error. */
	ffi->status = cest.status;
	ffi->error = ffth->tick_error;
	if (!fast) {
		ffclock_convert_delta((ffcounter)delta, cest.period, &bt);
		bintime_mul(&bt, cest.errb_rate * PS_AS_BINFRAC);	// errb_rate in [ps/s]
		bintime_add(&ffi->error, &bt);
	}
#endif
}

/*
 * Convert a sysclock snapshot into a struct bintime based on the specified
 * clock paradigm, and flags.
 * wantUptime and wantLerp supercede wantDiff if (erroneously) used together
 */
int
sysclock_snap2bintime(struct sysclock_snap *cs, struct bintime *bt,
    int whichclock, int wantFast, int wantUptime, int wantLerp, int wantDiff)
{
	struct bintime boottimebin;
#ifdef FFCLOCK
	struct bintime bt2;
	uint64_t period;
#endif

	switch (whichclock) {
	case SYSCLOCK_FB:
		*bt = cs->fb_info.tick_time;

		/* If snapshot was created with !fast, delta will be >0. */
		if (!wantFast && cs->delta > 0)
			bintime_addx(bt, cs->fb_info.th_scale * cs->delta);

		/* Native FBclock is Uptime, need to adjust if want UTC */
		if (!wantUptime) {
			getboottimebin(&boottimebin);
			bintime_add(bt, &boottimebin);
		}
		break;
#ifdef FFCLOCK
	case SYSCLOCK_FF:
		if (wantLerp) {	// Lerp supercedes Diff
			period = cs->ff_info.period_lerp;
			*bt = cs->ff_info.tick_time_lerp;
		} else {
			period = cs->ff_info.period;
			if (wantDiff)
				*bt = cs->ff_info.tick_time_diff;
			else
				*bt = cs->ff_info.tick_time;
		}

		/* If snapshot was created with !fast, delta will be >0. */
		if (!wantFast && cs->delta > 0) {
			ffclock_convert_delta((ffcounter)cs->delta, period, &bt2);
			bintime_add(bt, &bt2);
		}

		/* Add appropriate constant to create Uptime, UTC, or Diff FF clock */
		if (wantUptime) // Uptime superceded Diff
			bintime_sub(bt, &ffclock_boottime);
		else
			if (!wantDiff)				 // UTC
				bt->sec -= cs->ff_info.leapsec_adjustment;
			// else Diff
		break;
#endif
	default:
		return (EINVAL);
		break;
	}

	return (0);
}

/*
 * Initialize a new timecounter and possibly use it.
 */
void
tc_init(struct timecounter *tc)
{
	u_int u;
	struct sysctl_oid *tc_root;

	u = tc->tc_frequency / tc->tc_counter_mask;
	/* XXX: We need some margin here, 10% is a guess */
	u *= 11;
	u /= 10;
	if (u > hz && tc->tc_quality >= 0) {
		tc->tc_quality = -2000;
		if (bootverbose) {
			printf("Timecounter \"%s\" frequency %ju Hz",
			    tc->tc_name, (uintmax_t)tc->tc_frequency);
			printf(" -- Insufficient hz, needs at least %u\n", u);
		}
	} else if (tc->tc_quality >= 0 || bootverbose) {
		printf("Timecounter \"%s\" frequency %ju Hz quality %d\n",
		    tc->tc_name, (uintmax_t)tc->tc_frequency,
		    tc->tc_quality);
	}

	/*
	 * Set up sysctl tree for this counter.
	 */
	tc_root = SYSCTL_ADD_NODE_WITH_LABEL(NULL,
	    SYSCTL_STATIC_CHILDREN(_kern_timecounter_tc), OID_AUTO, tc->tc_name,
	    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
	    "timecounter description", "timecounter");
	SYSCTL_ADD_UINT(NULL, SYSCTL_CHILDREN(tc_root), OID_AUTO,
	    "mask", CTLFLAG_RD, &(tc->tc_counter_mask), 0,
	    "mask for implemented bits");
	SYSCTL_ADD_PROC(NULL, SYSCTL_CHILDREN(tc_root), OID_AUTO,
	    "counter", CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_MPSAFE, tc,
	    sizeof(*tc), sysctl_kern_timecounter_get, "IU",
	    "current timecounter value");
	SYSCTL_ADD_PROC(NULL, SYSCTL_CHILDREN(tc_root), OID_AUTO,
	    "frequency", CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_MPSAFE, tc,
	    sizeof(*tc), sysctl_kern_timecounter_freq, "QU",
	    "timecounter frequency");
	SYSCTL_ADD_INT(NULL, SYSCTL_CHILDREN(tc_root), OID_AUTO,
	    "quality", CTLFLAG_RD, &(tc->tc_quality), 0,
	    "goodness of time counter");

	mtx_lock(&tc_lock);
	tc->tc_next = timecounters;
	timecounters = tc;

	/*
	 * Do not automatically switch if the current tc was specifically
	 * chosen.  Never automatically use a timecounter with negative quality.
	 * Even though we run on the dummy counter, switching here may be
	 * worse since this timecounter may not be monotonic.
	 */
	if (tc_chosen)
		goto unlock;
	if (tc->tc_quality < 0)
		goto unlock;
	if (tc_from_tunable[0] != '\0' &&
	    strcmp(tc->tc_name, tc_from_tunable) == 0) {
		tc_chosen = 1;
		tc_from_tunable[0] = '\0';
	} else {
		if (tc->tc_quality < timecounter->tc_quality)
			goto unlock;
		if (tc->tc_quality == timecounter->tc_quality &&
		    tc->tc_frequency < timecounter->tc_frequency)
			goto unlock;
	}
	(void)tc->tc_get_timecount(tc);
	timecounter = tc;
unlock:
	mtx_unlock(&tc_lock);
}

/* Report the frequency of the current timecounter. */
uint64_t
tc_getfrequency(void)
{

	return (timehands->th_counter->tc_frequency);
}

static bool
sleeping_on_old_rtc(struct thread *td)
{

	/*
	 * td_rtcgen is modified by curthread when it is running,
	 * and by other threads in this function.  By finding the thread
	 * on a sleepqueue and holding the lock on the sleepqueue
	 * chain, we guarantee that the thread is not running and that
	 * modifying td_rtcgen is safe.  Setting td_rtcgen to zero informs
	 * the thread that it was woken due to a real-time clock adjustment.
	 * (The declaration of td_rtcgen refers to this comment.)
	 */
	if (td->td_rtcgen != 0 && td->td_rtcgen != rtc_generation) {
		td->td_rtcgen = 0;
		return (true);
	}
	return (false);
}

static struct mtx tc_setclock_mtx;
MTX_SYSINIT(tc_setclock_init, &tc_setclock_mtx, "tcsetc", MTX_SPIN);

/*
 * Step our concept of UTC.  This is done by modifying our estimate of
 * when we booted.
 * Since this function sets the FB clock, if FFCLOCK defined, ensure all
 * timestamp calls are in fact from the FB clock for consistency.
 */
void
tc_setclock(struct timespec *ts)
{
	struct timespec tbef, taft;
	struct bintime bt, bt2;

	timespec2bintime(ts, &bt);
#ifdef FFCLOCK
	fbclock_nanotime(&tbef);
#else
	nanotime(&tbef);
#endif
	mtx_lock_spin(&tc_setclock_mtx);
	cpu_tick_calibrate(1);
#ifdef FFCLOCK
	fbclock_binuptime(&bt2);
#else
	binuptime(&bt2);
#endif
	bintime_sub(&bt, &bt2);

	/* XXX fiddle all the little crinkly bits around the fiords... */
	tc_windup(&bt);
	mtx_unlock_spin(&tc_setclock_mtx);

	/* Avoid rtc_generation == 0, since td_rtcgen == 0 is special. */
	atomic_add_rel_int(&rtc_generation, 2);
	sleepq_chains_remove_matching(sleeping_on_old_rtc);
	if (timestepwarnings) {
#ifdef FFCLOCK
		fbclock_nanotime(&taft);
#else
		nanotime(&taft);
#endif
		log(LOG_INFO,
		    "Time stepped from %jd.%09ld to %jd.%09ld (%jd.%09ld)\n",
		    (intmax_t)tbef.tv_sec, tbef.tv_nsec,
		    (intmax_t)taft.tv_sec, taft.tv_nsec,
		    (intmax_t)ts->tv_sec, ts->tv_nsec);
	}
}

/*
 * Recalculate the scaling factor.  We want the number of 1/2^64
 * fractions of a second per period of the hardware counter, taking
 * into account the th_adjustment factor which the NTP PLL/adjtime(2)
 * processing provides us with.
 *
 * The th_adjustment is nanoseconds per second with 32 bit binary
 * fraction and we want 64 bit binary fraction of second:
 *
 *	 x = a * 2^32 / 10^9 = a * 4.294967296
 *
 * The range of th_adjustment is +/- 5000PPM so inside a 64bit int
 * we can only multiply by about 850 without overflowing, that
 * leaves no suitably precise fractions for multiply before divide.
 *
 * Divide before multiply with a fraction of 2199/512 results in a
 * systematic undercompensation of 10PPM of th_adjustment.  On a
 * 5000PPM adjustment this is a 0.05PPM error.  This is acceptable.
 *
 * We happily sacrifice the lowest of the 64 bits of our result
 * to the goddess of code clarity.
 */
static void
recalculate_scaling_factor_and_large_delta(struct timehands *th)
{
	uint64_t scale;

	scale = (uint64_t)1 << 63;
	scale += (th->th_adjustment / 1024) * 2199;
	scale /= th->th_counter->tc_frequency;
	th->th_scale = scale * 2;
	th->th_large_delta = MIN(((uint64_t)1 << 63) / scale, UINT_MAX);
}

/*
 * Initialize the next struct timehands in the ring and make
 * it the active timehands.  Along the way we might switch to a different
 * timecounter and/or do seconds processing in NTP.  Slightly magic.
 */
static void
tc_windup(struct bintime *new_boottimebin)
{
	struct bintime bt;
	struct timehands *th, *tho;
	u_int delta, ncount, ogen;
	int i;
	time_t t;

	/*
	 * Make the next timehands a copy of the current one, but do
	 * not overwrite the generation or next pointer.  While we
	 * update the contents, the generation must be zero.  We need
	 * to ensure that the zero generation is visible before the
	 * data updates become visible, which requires release fence.
	 * For similar reasons, re-reading of the generation after the
	 * data is read should use acquire fence.
	 */
	tho = timehands;
	th = tho->th_next;
	ogen = th->th_generation;
	th->th_generation = 0;
	atomic_thread_fence_rel();
	memcpy(th, tho, offsetof(struct timehands, th_generation));
	if (new_boottimebin != NULL)
		th->th_boottime = *new_boottimebin;

	/*
	 * Capture a timecounter delta on the current timecounter and if
	 * changing timecounters, a counter value from the new timecounter.
	 * Update the offset fields accordingly.
	 */
	delta = tc_delta(th);
	if (th->th_counter != timecounter)
		ncount = timecounter->tc_get_timecount(timecounter);
	else
		ncount = 0;

	th->th_offset_count += delta;
	th->th_offset_count &= th->th_counter->tc_counter_mask;
	bintime_add_tc_delta(&th->th_offset, th->th_scale,
	    th->th_large_delta, delta);

	/*
	 * Hardware latching timecounters may not generate interrupts on
	 * PPS events, so instead we poll them.  There is a finite risk that
	 * the hardware might capture a count which is later than the one we
	 * got above, and therefore possibly in the next NTP second which might
	 * have a different rate than the current NTP second.  It doesn't
	 * matter in practice.
	 */
	if (tho->th_counter->tc_poll_pps)
		tho->th_counter->tc_poll_pps(tho->th_counter);

	/*
	 * Deal with NTP second processing.  The loop normally
	 * iterates at most once, but in extreme situations it might
	 * keep NTP sane if timeouts are not run for several seconds.
	 * At boot, the time step can be large when the TOD hardware
	 * has been read, so on really large steps, we call
	 * ntp_update_second only twice.  We need to call it twice in
	 * case we missed a leap second.
	 */
	bt = th->th_offset;
	bintime_add(&bt, &th->th_boottime);
#ifdef FFCLOCK
	ffclock_windup(delta, new_boottimebin, &bt);
#endif
	i = bt.sec - tho->th_microtime.tv_sec;
	if (i > 0) {
		if (i > LARGE_STEP)
			i = 2;

		do {
			t = bt.sec;
			ntp_update_second(&th->th_adjustment, &bt.sec);
			if (bt.sec != t)
				th->th_boottime.sec += bt.sec - t;
			--i;
		} while (i > 0);

		recalculate_scaling_factor_and_large_delta(th);
	}

	/* Update the UTC timestamps used by the get*() functions. */
	th->th_bintime = bt;
	bintime2timeval(&bt, &th->th_microtime);
	bintime2timespec(&bt, &th->th_nanotime);

	/* Now is a good time to change timecounters. */
	if (th->th_counter != timecounter) {
		printf("Changing tc counter on this tc-tick:  %s = %#X --> %s = %#X \n",
			th->th_counter->tc_name, th->th_offset_count,
			timecounter->tc_name, ncount);
#ifndef __arm__
		if ((timecounter->tc_flags & TC_FLAGS_C2STOP) != 0)
			cpu_disable_c2_sleep++;
		if ((th->th_counter->tc_flags & TC_FLAGS_C2STOP) != 0)
			cpu_disable_c2_sleep--;
#endif
		th->th_counter = timecounter;
		th->th_offset_count = ncount;
		tc_min_ticktock_freq = max(1, timecounter->tc_frequency /
		    (((uint64_t)timecounter->tc_counter_mask + 1) / 3));
		recalculate_scaling_factor_and_large_delta(th);
#ifdef FFCLOCK
		ffclock_change_tc(th,ncount);
#endif
	}

	/*
	 * Now that the struct timehands is again consistent, set the new
	 * generation number, making sure to not make it zero.
	 */
	if (++ogen == 0)
		ogen = 1;
	atomic_store_rel_int(&th->th_generation, ogen);

	/* Go live with the new struct timehands. */
#ifdef FFCLOCK
	switch (sysclock_active) {
	case SYSCLOCK_FB:
#endif
		time_second = th->th_microtime.tv_sec;
		time_uptime = th->th_offset.sec;
#ifdef FFCLOCK
		break;
	case SYSCLOCK_FF:
		ffclock_getbintime(&bt);
		time_second = bt.sec;
		ffclock_getbinuptime(&bt);
		time_uptime = bt.sec;
		break;
	}
#endif

	timehands = th;
	timekeep_push_vdso();
}

/* Report or change the active timecounter hardware. */
static int
sysctl_kern_timecounter_hardware(SYSCTL_HANDLER_ARGS)
{
	char newname[32];
	struct timecounter *newtc, *tc;
	int error;

	mtx_lock(&tc_lock);
	tc = timecounter;
	strlcpy(newname, tc->tc_name, sizeof(newname));
	mtx_unlock(&tc_lock);

	error = sysctl_handle_string(oidp, &newname[0], sizeof(newname), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	mtx_lock(&tc_lock);
	/* Record that the tc in use now was specifically chosen. */
	tc_chosen = 1;
	if (strcmp(newname, tc->tc_name) == 0) {
		mtx_unlock(&tc_lock);
		return (0);
	}
	for (newtc = timecounters; newtc != NULL; newtc = newtc->tc_next) {
		if (strcmp(newname, newtc->tc_name) != 0)
			continue;

		/* Warm up new timecounter. */
		(void)newtc->tc_get_timecount(newtc);

		timecounter = newtc;

		/*
		 * The vdso timehands update is deferred until the next
		 * 'tc_windup()'.
		 *
		 * This is prudent given that 'timekeep_push_vdso()' does not
		 * use any locking and that it can be called in hard interrupt
		 * context via 'tc_windup()'.
		 */
		break;
	}
	mtx_unlock(&tc_lock);
	return (newtc != NULL ? 0 : EINVAL);
}
SYSCTL_PROC(_kern_timecounter, OID_AUTO, hardware,
    CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE, 0, 0,
    sysctl_kern_timecounter_hardware, "A",
    "Timecounter hardware selected");

/* Report the available timecounter hardware. */
static int
sysctl_kern_timecounter_choice(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sb;
	struct timecounter *tc;
	int error;

	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	sbuf_new_for_sysctl(&sb, NULL, 0, req);
	mtx_lock(&tc_lock);
	for (tc = timecounters; tc != NULL; tc = tc->tc_next) {
		if (tc != timecounters)
			sbuf_putc(&sb, ' ');
		sbuf_printf(&sb, "%s(%d)", tc->tc_name, tc->tc_quality);
	}
	mtx_unlock(&tc_lock);
	error = sbuf_finish(&sb);
	sbuf_delete(&sb);
	return (error);
}

SYSCTL_PROC(_kern_timecounter, OID_AUTO, choice,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, 0, 0,
    sysctl_kern_timecounter_choice, "A",
    "Timecounter hardware detected");

/*
 * RFC 2783 PPS-API implementation.
 */

/*
 *  Return true if the driver is aware of the abi version extensions in the
 *  pps_state structure, and it supports at least the given abi version number.
 */
static inline int
abi_aware(struct pps_state *pps, int vers)
{

	return ((pps->kcmode & KCMODE_ABIFLAG) && pps->driver_abi >= vers);
}

static int
pps_fetch(struct pps_fetch_args *fapi, struct pps_state *pps)
{
	int err, timo;
	pps_seq_t aseq, cseq;
	struct timeval tv;

	if (fapi->tsformat && fapi->tsformat != PPS_TSFMT_TSPEC)
		return (EINVAL);

	/*
	 * If no timeout is requested, immediately return whatever values were
	 * most recently captured.  If timeout seconds is -1, that's a request
	 * to block without a timeout.  WITNESS won't let us sleep forever
	 * without a lock (we really don't need a lock), so just repeatedly
	 * sleep a long time.
	 */
	if (fapi->timeout.tv_sec || fapi->timeout.tv_nsec) {
		if (fapi->timeout.tv_sec == -1)
			timo = 0x7fffffff;
		else {
			tv.tv_sec = fapi->timeout.tv_sec;
			tv.tv_usec = fapi->timeout.tv_nsec / 1000;
			timo = tvtohz(&tv);
		}
		aseq = atomic_load_int(&pps->ppsinfo.assert_sequence);
		cseq = atomic_load_int(&pps->ppsinfo.clear_sequence);
		while (aseq == atomic_load_int(&pps->ppsinfo.assert_sequence) &&
		    cseq == atomic_load_int(&pps->ppsinfo.clear_sequence)) {
			if (abi_aware(pps, 1) && pps->driver_mtx != NULL) {
				if (pps->flags & PPSFLAG_MTX_SPIN) {
					err = msleep_spin(pps, pps->driver_mtx,
					    "ppsfch", timo);
				} else {
					err = msleep(pps, pps->driver_mtx, PCATCH,
					    "ppsfch", timo);
				}
			} else {
				err = tsleep(pps, PCATCH, "ppsfch", timo);
			}
			if (err == EWOULDBLOCK) {
				if (fapi->timeout.tv_sec == -1) {
					continue;
				} else {
					return (ETIMEDOUT);
				}
			} else if (err != 0) {
				return (err);
			}
		}
	}

	pps->ppsinfo.current_mode = pps->ppsparam.mode;
	fapi->pps_info_buf = pps->ppsinfo;

	return (0);
}

int
pps_ioctl(u_long cmd, caddr_t data, struct pps_state *pps)
{
	pps_params_t *app;
	struct pps_fetch_args *fapi;
#ifdef FFCLOCK
	struct pps_fetch_ffc_args *fapi_ffc;
#endif
#ifdef PPS_SYNC
	struct pps_kcbind_args *kapi;
#endif

	KASSERT(pps != NULL, ("NULL pps pointer in pps_ioctl"));
	switch (cmd) {
	case PPS_IOC_CREATE:
		return (0);
	case PPS_IOC_DESTROY:
		return (0);
	case PPS_IOC_SETPARAMS:
		app = (pps_params_t *)data;
		if (app->mode & ~pps->ppscap)
			return (EINVAL);
#ifdef FFCLOCK
		/* Ensure only a single clock is selected for ffc timestamp. */
		if ((app->mode & PPS_TSCLK_MASK) == PPS_TSCLK_MASK)
			return (EINVAL);
#endif
		pps->ppsparam = *app;
		return (0);
	case PPS_IOC_GETPARAMS:
		app = (pps_params_t *)data;
		*app = pps->ppsparam;
		app->api_version = PPS_API_VERS_1;
		return (0);
	case PPS_IOC_GETCAP:
		*(int*)data = pps->ppscap;
		return (0);
	case PPS_IOC_FETCH:
		fapi = (struct pps_fetch_args *)data;
		return (pps_fetch(fapi, pps));
#ifdef FFCLOCK
	case PPS_IOC_FETCH_FFCOUNTER:
		fapi_ffc = (struct pps_fetch_ffc_args *)data;
		if (fapi_ffc->tsformat && fapi_ffc->tsformat !=
		    PPS_TSFMT_TSPEC)
			return (EINVAL);
		if (fapi_ffc->timeout.tv_sec || fapi_ffc->timeout.tv_nsec)
			return (EOPNOTSUPP);
		pps->ppsinfo_ffc.current_mode = pps->ppsparam.mode;
		fapi_ffc->pps_info_buf_ffc = pps->ppsinfo_ffc;
		/* Overwrite timestamps if feedback clock selected. */
		switch (pps->ppsparam.mode & PPS_TSCLK_MASK) {
		case PPS_TSCLK_FBCK:
			fapi_ffc->pps_info_buf_ffc.assert_timestamp =
			    pps->ppsinfo.assert_timestamp;
			fapi_ffc->pps_info_buf_ffc.clear_timestamp =
			    pps->ppsinfo.clear_timestamp;
			break;
		case PPS_TSCLK_FFWD:
			break;
		default:
			break;
		}
		return (0);
#endif /* FFCLOCK */
	case PPS_IOC_KCBIND:
#ifdef PPS_SYNC
		kapi = (struct pps_kcbind_args *)data;
		/* XXX Only root should be able to do this */
		if (kapi->tsformat && kapi->tsformat != PPS_TSFMT_TSPEC)
			return (EINVAL);
		if (kapi->kernel_consumer != PPS_KC_HARDPPS)
			return (EINVAL);
		if (kapi->edge & ~pps->ppscap)
			return (EINVAL);
		pps->kcmode = (kapi->edge & KCMODE_EDGEMASK) |
		    (pps->kcmode & KCMODE_ABIFLAG);
		return (0);
#else
		return (EOPNOTSUPP);
#endif
	default:
		return (ENOIOCTL);
	}
}

void
pps_init(struct pps_state *pps)
{
	pps->ppscap |= PPS_TSFMT_TSPEC | PPS_CANWAIT;
	if (pps->ppscap & PPS_CAPTUREASSERT)
		pps->ppscap |= PPS_OFFSETASSERT;
	if (pps->ppscap & PPS_CAPTURECLEAR)
		pps->ppscap |= PPS_OFFSETCLEAR;
#ifdef FFCLOCK
	pps->ppscap |= PPS_TSCLK_MASK;
#endif
	pps->kcmode &= ~KCMODE_ABIFLAG;
}

void
pps_init_abi(struct pps_state *pps)
{

	pps_init(pps);
	if (pps->driver_abi > 0) {
		pps->kcmode |= KCMODE_ABIFLAG;
		pps->kernel_abi = PPS_ABI_VERSION;
	}
}

void
pps_capture(struct pps_state *pps)
{
	struct timehands *th;

	KASSERT(pps != NULL, ("NULL pps pointer in pps_capture"));
	th = timehands;
	pps->capgen = atomic_load_acq_int(&th->th_generation);
	pps->capth = th;
#ifdef FFCLOCK
	pps->capffth = fftimehands;
#endif
	pps->capcount = th->th_counter->tc_get_timecount(th->th_counter);
	atomic_thread_fence_acq();
	if (pps->capgen != th->th_generation)
		pps->capgen = 0;
}

void
pps_event(struct pps_state *pps, int event)
{
	struct bintime bt;
	struct timespec ts, *tsp, *osp;
	u_int tcount, *pcount;
	int foff;
	pps_seq_t *pseq;
#ifdef FFCLOCK
	struct timespec *tsp_ffc;
	pps_seq_t *pseq_ffc;
	ffcounter *ffcount;
#endif
#ifdef PPS_SYNC
	int fhard;
#endif

	KASSERT(pps != NULL, ("NULL pps pointer in pps_event"));
	/* Nothing to do if not currently set to capture this event type. */
	if ((event & pps->ppsparam.mode) == 0)
		return;
	/* If the timecounter was wound up underneath us, bail out. */
	if (pps->capgen == 0 || pps->capgen !=
	    atomic_load_acq_int(&pps->capth->th_generation))
		return;

	/* Things would be easier with arrays. */
	if (event == PPS_CAPTUREASSERT) {
		tsp = &pps->ppsinfo.assert_timestamp;
		osp = &pps->ppsparam.assert_offset;
		foff = pps->ppsparam.mode & PPS_OFFSETASSERT;
#ifdef PPS_SYNC
		fhard = pps->kcmode & PPS_CAPTUREASSERT;
#endif
		pcount = &pps->ppscount[0];
		pseq = &pps->ppsinfo.assert_sequence;
#ifdef FFCLOCK
		ffcount = &pps->ppsinfo_ffc.assert_ffcount;
		tsp_ffc = &pps->ppsinfo_ffc.assert_timestamp;
		pseq_ffc = &pps->ppsinfo_ffc.assert_sequence;
#endif
	} else {
		tsp = &pps->ppsinfo.clear_timestamp;
		osp = &pps->ppsparam.clear_offset;
		foff = pps->ppsparam.mode & PPS_OFFSETCLEAR;
#ifdef PPS_SYNC
		fhard = pps->kcmode & PPS_CAPTURECLEAR;
#endif
		pcount = &pps->ppscount[1];
		pseq = &pps->ppsinfo.clear_sequence;
#ifdef FFCLOCK
		ffcount = &pps->ppsinfo_ffc.clear_ffcount;
		tsp_ffc = &pps->ppsinfo_ffc.clear_timestamp;
		pseq_ffc = &pps->ppsinfo_ffc.clear_sequence;
#endif
	}

	/*
	 * If the timecounter changed, we cannot compare the count values, so
	 * we have to drop the rest of the PPS-stuff until the next event.
	 */
	if (pps->ppstc != pps->capth->th_counter) {
		pps->ppstc = pps->capth->th_counter;
		*pcount = pps->capcount;
		pps->ppscount[2] = pps->capcount;
		return;
	}

	/* Convert the count to a timespec. */
	tcount = pps->capcount - pps->capth->th_offset_count;
	tcount &= pps->capth->th_counter->tc_counter_mask;
	bt = pps->capth->th_bintime;
	bintime_addx(&bt, pps->capth->th_scale * tcount);
	bintime2timespec(&bt, &ts);

	/* If the timecounter was wound up underneath us, bail out. */
	atomic_thread_fence_acq();
	if (pps->capgen != pps->capth->th_generation)
		return;

	*pcount = pps->capcount;
	(*pseq)++;
	*tsp = ts;

	if (foff) {
		timespecadd(tsp, osp, tsp);
		if (tsp->tv_nsec < 0) {
			tsp->tv_nsec += 1000000000;
			tsp->tv_sec -= 1;
		}
	}

#ifdef FFCLOCK
	*ffcount = pps->capffth->tick_ffcount + tcount;
	bt = pps->capffth->tick_time;
	ffclock_convert_delta(tcount, pps->capffth->cest.period, &bt);
	bintime_add(&bt, &pps->capffth->tick_time);
	bintime2timespec(&bt, &ts);
	(*pseq_ffc)++;
	*tsp_ffc = ts;
#endif

#ifdef PPS_SYNC
	if (fhard) {
		uint64_t scale;

		/*
		 * Feed the NTP PLL/FLL.
		 * The FLL wants to know how many (hardware) nanoseconds
		 * elapsed since the previous event.
		 */
		tcount = pps->capcount - pps->ppscount[2];
		pps->ppscount[2] = pps->capcount;
		tcount &= pps->capth->th_counter->tc_counter_mask;
		scale = (uint64_t)1 << 63;
		scale /= pps->capth->th_counter->tc_frequency;
		scale *= 2;
		bt.sec = 0;
		bt.frac = 0;
		bintime_addx(&bt, scale * tcount);
		bintime2timespec(&bt, &ts);
		hardpps(tsp, ts.tv_nsec + 1000000000 * ts.tv_sec);
	}
#endif

	/* Wakeup anyone sleeping in pps_fetch().  */
	wakeup(pps);
}

/*
 * Timecounters need to be updated every so often to prevent the hardware
 * counter from overflowing.  Updating also recalculates the cached values
 * used by the get*() family of functions, so their precision depends on
 * the update frequency.
 */

static int tc_tick;
SYSCTL_INT(_kern_timecounter, OID_AUTO, tick, CTLFLAG_RD, &tc_tick, 0,
    "Approximate number of hardclock ticks in a millisecond");

void
tc_ticktock(int cnt)
{
	static int count;

	if (mtx_trylock_spin(&tc_setclock_mtx)) {
		count += cnt;
		if (count >= tc_tick) {
			count = 0;
			tc_windup(NULL);
		}
		mtx_unlock_spin(&tc_setclock_mtx);
	}
}

static void __inline
tc_adjprecision(void)
{
	int t;

	if (tc_timepercentage > 0) {
		t = (99 + tc_timepercentage) / tc_timepercentage;
		tc_precexp = fls(t + (t >> 1)) - 1;
		FREQ2BT(hz / tc_tick, &bt_timethreshold);
		FREQ2BT(hz, &bt_tickthreshold);
		bintime_shift(&bt_timethreshold, tc_precexp);
		bintime_shift(&bt_tickthreshold, tc_precexp);
	} else {
		tc_precexp = 31;
		bt_timethreshold.sec = INT_MAX;
		bt_timethreshold.frac = ~(uint64_t)0;
		bt_tickthreshold = bt_timethreshold;
	}
	sbt_timethreshold = bttosbt(bt_timethreshold);
	sbt_tickthreshold = bttosbt(bt_tickthreshold);
}

static int
sysctl_kern_timecounter_adjprecision(SYSCTL_HANDLER_ARGS)
{
	int error, val;

	val = tc_timepercentage;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	tc_timepercentage = val;
	if (cold)
		goto done;
	tc_adjprecision();
done:
	return (0);
}

/* Set up the requested number of timehands. */
static void
inittimehands(void *dummy)
{
	struct timehands *thp;
	int i;

	TUNABLE_INT_FETCH("kern.timecounter.timehands_count",
	    &timehands_count);
	if (timehands_count < 1)
		timehands_count = 1;
	if (timehands_count > nitems(ths))
		timehands_count = nitems(ths);
	for (i = 1, thp = &ths[0]; i < timehands_count;  thp = &ths[i++])
		thp->th_next = &ths[i];
	thp->th_next = &ths[0];

	TUNABLE_STR_FETCH("kern.timecounter.hardware", tc_from_tunable,
	    sizeof(tc_from_tunable));

	mtx_init(&tc_lock, "tc", NULL, MTX_DEF);
}
SYSINIT(timehands, SI_SUB_TUNABLES, SI_ORDER_ANY, inittimehands, NULL);

static void
inittimecounter(void *dummy)
{
	u_int p;
	int tick_rate;

	/*
	 * Set the initial timeout to
	 * max(1, <approx. number of hardclock ticks in a millisecond>).
	 * People should probably not use the sysctl to set the timeout
	 * to smaller than its initial value, since that value is the
	 * smallest reasonable one.  If they want better timestamps they
	 * should use the non-"get"* functions.
	 */
	if (hz > 1000)
		tc_tick = (hz + 500) / 1000;
	else
		tc_tick = 1;
	tc_adjprecision();
	FREQ2BT(hz, &tick_bt);
	tick_sbt = bttosbt(tick_bt);
	tick_rate = hz / tc_tick;
	FREQ2BT(tick_rate, &tc_tick_bt);
	tc_tick_sbt = bttosbt(tc_tick_bt);
	p = (tc_tick * 1000000) / hz;
	printf("Timecounters tick every %d.%03u msec\n", p / 1000, p % 1000);

#ifdef FFCLOCK
	ffclock_init();
#endif

	/* warm up new timecounter (again) and get rolling. */
	(void)timecounter->tc_get_timecount(timecounter);
	mtx_lock_spin(&tc_setclock_mtx);
	tc_windup(NULL);
	mtx_unlock_spin(&tc_setclock_mtx);
}

SYSINIT(timecounter, SI_SUB_CLOCKS, SI_ORDER_SECOND, inittimecounter, NULL);

/* Cpu tick handling -------------------------------------------------*/

static int cpu_tick_variable;
static uint64_t	cpu_tick_frequency;

DPCPU_DEFINE_STATIC(uint64_t, tc_cpu_ticks_base);
DPCPU_DEFINE_STATIC(unsigned, tc_cpu_ticks_last);

static uint64_t
tc_cpu_ticks(void)
{
	struct timecounter *tc;
	uint64_t res, *base;
	unsigned u, *last;

	critical_enter();
	base = DPCPU_PTR(tc_cpu_ticks_base);
	last = DPCPU_PTR(tc_cpu_ticks_last);
	tc = timehands->th_counter;
	u = tc->tc_get_timecount(tc) & tc->tc_counter_mask;
	if (u < *last)
		*base += (uint64_t)tc->tc_counter_mask + 1;
	*last = u;
	res = u + *base;
	critical_exit();
	return (res);
}

void
cpu_tick_calibration(void)
{
	static time_t last_calib;

	if (time_uptime != last_calib && !(time_uptime & 0xf)) {
		cpu_tick_calibrate(0);
		last_calib = time_uptime;
	}
}

/*
 * This function gets called every 16 seconds on only one designated
 * CPU in the system from hardclock() via cpu_tick_calibration()().
 *
 * Whenever the real time clock is stepped we get called with reset=1
 * to make sure we handle suspend/resume and similar events correctly.
 */

static void
cpu_tick_calibrate(int reset)
{
	static uint64_t c_last;
	uint64_t c_this, c_delta;
	static struct bintime  t_last;
	struct bintime t_this, t_delta;
	uint32_t divi;

	if (reset) {
		/* The clock was stepped, abort & reset */
		t_last.sec = 0;
		return;
	}

	/* we don't calibrate fixed rate cputicks */
	if (!cpu_tick_variable)
		return;

	getbinuptime(&t_this);
	c_this = cpu_ticks();
	if (t_last.sec != 0) {
		c_delta = c_this - c_last;
		t_delta = t_this;
		bintime_sub(&t_delta, &t_last);
		/*
		 * Headroom:
		 * 	2^(64-20) / 16[s] =
		 * 	2^(44) / 16[s] =
		 * 	17.592.186.044.416 / 16 =
		 * 	1.099.511.627.776 [Hz]
		 */
		divi = t_delta.sec << 20;
		divi |= t_delta.frac >> (64 - 20);
		c_delta <<= 20;
		c_delta /= divi;
		if (c_delta > cpu_tick_frequency) {
	//		if (0 && bootverbose)
				printf("cpu_tick increased to %ju Hz\n",
				    c_delta);
			cpu_tick_frequency = c_delta;
		}
	}
	c_last = c_this;
	t_last = t_this;
}

void
set_cputicker(cpu_tick_f *func, uint64_t freq, unsigned var)
{

	if (func == NULL) {
		cpu_ticks = tc_cpu_ticks;
	} else {
		cpu_tick_frequency = freq;
		cpu_tick_variable = var;
		cpu_ticks = func;
	}
}

uint64_t
cpu_tickrate(void)
{

	if (cpu_ticks == tc_cpu_ticks) 
		return (tc_getfrequency());
	return (cpu_tick_frequency);
}

/*
 * We need to be slightly careful converting cputicks to microseconds.
 * There is plenty of margin in 64 bits of microseconds (half a million
 * years) and in 64 bits at 4 GHz (146 years), but if we do a multiply
 * before divide conversion (to retain precision) we find that the
 * margin shrinks to 1.5 hours (one millionth of 146y).
 * With a three prong approach we never lose significant bits, no
 * matter what the cputick rate and length of timeinterval is.
 */

uint64_t
cputick2usec(uint64_t tick)
{

	if (tick > 18446744073709551LL)		/* floor(2^64 / 1000) */
		return (tick / (cpu_tickrate() / 1000000LL));
	else if (tick > 18446744073709LL)	/* floor(2^64 / 1000000) */
		return ((tick * 1000LL) / (cpu_tickrate() / 1000LL));
	else
		return ((tick * 1000000LL) / cpu_tickrate());
}

cpu_tick_f	*cpu_ticks = tc_cpu_ticks;

static int vdso_th_enable = 1;
static int
sysctl_fast_gettime(SYSCTL_HANDLER_ARGS)
{
	int old_vdso_th_enable, error;

	old_vdso_th_enable = vdso_th_enable;
	error = sysctl_handle_int(oidp, &old_vdso_th_enable, 0, req);
	if (error != 0)
		return (error);
	vdso_th_enable = old_vdso_th_enable;
	return (0);
}
SYSCTL_PROC(_kern_timecounter, OID_AUTO, fast_gettime,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    NULL, 0, sysctl_fast_gettime, "I", "Enable fast time of day");

uint32_t
tc_fill_vdso_timehands(struct vdso_timehands *vdso_th)
{
	struct timehands *th;
	uint32_t enabled;

	th = timehands;
	vdso_th->th_scale = th->th_scale;
	vdso_th->th_offset_count = th->th_offset_count;
	vdso_th->th_counter_mask = th->th_counter->tc_counter_mask;
	vdso_th->th_offset = th->th_offset;
	vdso_th->th_boottime = th->th_boottime;
	if (th->th_counter->tc_fill_vdso_timehands != NULL) {
		enabled = th->th_counter->tc_fill_vdso_timehands(vdso_th,
		    th->th_counter);
	} else
		enabled = 0;
	if (!vdso_th_enable)
		enabled = 0;
	return (enabled);
}

#ifdef COMPAT_FREEBSD32
uint32_t
tc_fill_vdso_timehands32(struct vdso_timehands32 *vdso_th32)
{
	struct timehands *th;
	uint32_t enabled;

	th = timehands;
	*(uint64_t *)&vdso_th32->th_scale[0] = th->th_scale;
	vdso_th32->th_offset_count = th->th_offset_count;
	vdso_th32->th_counter_mask = th->th_counter->tc_counter_mask;
	vdso_th32->th_offset.sec = th->th_offset.sec;
	*(uint64_t *)&vdso_th32->th_offset.frac[0] = th->th_offset.frac;
	vdso_th32->th_boottime.sec = th->th_boottime.sec;
	*(uint64_t *)&vdso_th32->th_boottime.frac[0] = th->th_boottime.frac;
	if (th->th_counter->tc_fill_vdso_timehands32 != NULL) {
		enabled = th->th_counter->tc_fill_vdso_timehands32(vdso_th32,
		    th->th_counter);
	} else
		enabled = 0;
	if (!vdso_th_enable)
		enabled = 0;
	return (enabled);
}
#endif

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(timecounter, db_show_timecounter)
{
	struct timehands *th;
	struct timecounter *tc;
	u_int val1, val2;

	th = timehands;
	tc = th->th_counter;
	val1 = tc->tc_get_timecount(tc);
	__compiler_membar();
	val2 = tc->tc_get_timecount(tc);

	db_printf("timecounter %p %s\n", tc, tc->tc_name);
	db_printf("  mask %#x freq %ju qual %d flags %#x priv %p\n",
	    tc->tc_counter_mask, (uintmax_t)tc->tc_frequency, tc->tc_quality,
	    tc->tc_flags, tc->tc_priv);
	db_printf("  val %#x %#x\n", val1, val2);
	db_printf("timehands adj %#jx scale %#jx ldelta %d off_cnt %d gen %d\n",
	    (uintmax_t)th->th_adjustment, (uintmax_t)th->th_scale,
	    th->th_large_delta, th->th_offset_count, th->th_generation);
	db_printf("  offset %jd %jd boottime %jd %jd\n",
	    (intmax_t)th->th_offset.sec, (uintmax_t)th->th_offset.frac,
	    (intmax_t)th->th_boottime.sec, (uintmax_t)th->th_boottime.frac);
}
#endif

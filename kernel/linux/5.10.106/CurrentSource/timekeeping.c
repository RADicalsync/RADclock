// SPDX-License-Identifier: GPL-2.0
/*
 *  Kernel timekeeping code and accessor functions. Based on code from
 *  timer.c, moved in commit 8524070b7982.
 */
#include <linux/timekeeper_internal.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/percpu.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/nmi.h>
#include <linux/sched.h>
#include <linux/sched/loadavg.h>
#include <linux/sched/clock.h>
#include <linux/syscore_ops.h>
#include <linux/clocksource.h>
#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/tick.h>
#include <linux/stop_machine.h>
#include <linux/pvclock_gtod.h>
#include <linux/compiler.h>
#include <linux/audit.h>

#include "tick-internal.h"
#include "ntp_internal.h"
#include "timekeeping_internal.h"

#ifdef CONFIG_FFCLOCK
#include <linux/ffclock.h>
#endif

#define TK_CLEAR_NTP		(1 << 0)
#define TK_MIRROR		(1 << 1)
#define TK_CLOCK_WAS_SET	(1 << 2)

enum timekeeping_adv_mode {
	/* Update timekeeper when a tick has passed */
	TK_ADV_TICK,

	/* Update timekeeper on a direct frequency change */
	TK_ADV_FREQ
};

DEFINE_RAW_SPINLOCK(timekeeper_lock);

/*
 * The most important data for readout fits into a single 64 byte
 * cache line.
 */
static struct {
	seqcount_raw_spinlock_t	seq;
	struct timekeeper	timekeeper;
} tk_core ____cacheline_aligned = {
	.seq = SEQCNT_RAW_SPINLOCK_ZERO(tk_core.seq, &timekeeper_lock),
};

static struct timekeeper shadow_timekeeper;

/* flag for if timekeeping is suspended */
int __read_mostly timekeeping_suspended;

/**
 * struct tk_fast - NMI safe timekeeper
 * @seq:	Sequence counter for protecting updates. The lowest bit
 *		is the index for the tk_read_base array
 * @base:	tk_read_base array. Access is indexed by the lowest bit of
 *		@seq.
 *
 * See @update_fast_timekeeper() below.
 */
struct tk_fast {
	seqcount_latch_t	seq;
	struct tk_read_base	base[2];
};

/* Suspend-time cycles value for halted fast timekeeper. */
static u64 cycles_at_suspend;

static u64 dummy_clock_read(struct clocksource *cs)
{
	if (timekeeping_suspended)
		return cycles_at_suspend;
	return local_clock();
}

static struct clocksource dummy_clock = {
	.read = dummy_clock_read,
};

/*
 * Boot time initialization which allows local_clock() to be utilized
 * during early boot when clocksources are not available. local_clock()
 * returns nanoseconds already so no conversion is required, hence mult=1
 * and shift=0. When the first proper clocksource is installed then
 * the fast time keepers are updated with the correct values.
 */
#define FAST_TK_INIT						\
	{							\
		.clock		= &dummy_clock,			\
		.mask		= CLOCKSOURCE_MASK(64),		\
		.mult		= 1,				\
		.shift		= 0,				\
	}

static struct tk_fast tk_fast_mono ____cacheline_aligned = {
	.seq     = SEQCNT_LATCH_ZERO(tk_fast_mono.seq),
	.base[0] = FAST_TK_INIT,
	.base[1] = FAST_TK_INIT,
};

static struct tk_fast tk_fast_raw  ____cacheline_aligned = {
	.seq     = SEQCNT_LATCH_ZERO(tk_fast_raw.seq),
	.base[0] = FAST_TK_INIT,
	.base[1] = FAST_TK_INIT,
};

static inline void tk_normalize_xtime(struct timekeeper *tk)
{
	while (tk->tkr_mono.xtime_nsec >= ((u64)NSEC_PER_SEC << tk->tkr_mono.shift)) {
		tk->tkr_mono.xtime_nsec -= (u64)NSEC_PER_SEC << tk->tkr_mono.shift;
		tk->xtime_sec++;
	}
	while (tk->tkr_raw.xtime_nsec >= ((u64)NSEC_PER_SEC << tk->tkr_raw.shift)) {
		tk->tkr_raw.xtime_nsec -= (u64)NSEC_PER_SEC << tk->tkr_raw.shift;
		tk->raw_sec++;
	}
}

static inline struct timespec64 tk_xtime(const struct timekeeper *tk)
{
	struct timespec64 ts;

	ts.tv_sec = tk->xtime_sec;
	ts.tv_nsec = (long)(tk->tkr_mono.xtime_nsec >> tk->tkr_mono.shift);
	return ts;
}

static void tk_set_xtime(struct timekeeper *tk, const struct timespec64 *ts)
{
	tk->xtime_sec = ts->tv_sec;
	tk->tkr_mono.xtime_nsec = (u64)ts->tv_nsec << tk->tkr_mono.shift;
}

static void tk_xtime_add(struct timekeeper *tk, const struct timespec64 *ts)
{
	tk->xtime_sec += ts->tv_sec;
	tk->tkr_mono.xtime_nsec += (u64)ts->tv_nsec << tk->tkr_mono.shift;
	tk_normalize_xtime(tk);
}

static void tk_set_wall_to_mono(struct timekeeper *tk, struct timespec64 wtm)
{
	struct timespec64 tmp;

	/*
	 * Verify consistency of: offset_real = -wall_to_monotonic
	 * before modifying anything
	 */
	set_normalized_timespec64(&tmp, -tk->wall_to_monotonic.tv_sec,
					-tk->wall_to_monotonic.tv_nsec);
	WARN_ON_ONCE(tk->offs_real != timespec64_to_ktime(tmp));
	tk->wall_to_monotonic = wtm;
	set_normalized_timespec64(&tmp, -wtm.tv_sec, -wtm.tv_nsec);
	tk->offs_real = timespec64_to_ktime(tmp);
	tk->offs_tai = ktime_add(tk->offs_real, ktime_set(tk->tai_offset, 0));
}

static inline void tk_update_sleep_time(struct timekeeper *tk, ktime_t delta)
{
	tk->offs_boot = ktime_add(tk->offs_boot, delta);
	/*
	 * Timespec representation for VDSO update to avoid 64bit division
	 * on every update.
	 */
	tk->monotonic_to_boot = ktime_to_timespec64(tk->offs_boot);
}

/*
 * tk_clock_read - atomic clocksource read() helper
 *
 * This helper is necessary to use in the read paths because, while the
 * seqcount ensures we don't return a bad value while structures are updated,
 * it doesn't protect from potential crashes. There is the possibility that
 * the tkr's clocksource may change between the read reference, and the
 * clock reference passed to the read function.  This can cause crashes if
 * the wrong clocksource is passed to the wrong read function.
 * This isn't necessary to use when holding the timekeeper_lock or doing
 * a read of the fast-timekeeper tkrs (which is protected by its own locking
 * and update logic).
 */
static inline u64 tk_clock_read(const struct tk_read_base *tkr)
{
	struct clocksource *clock = READ_ONCE(tkr->clock);

	return clock->read(clock);
}

#ifdef CONFIG_DEBUG_TIMEKEEPING
#define WARNING_FREQ (HZ*300) /* 5 minute rate-limiting */

static void timekeeping_check_update(struct timekeeper *tk, u64 offset)
{

	u64 max_cycles = tk->tkr_mono.clock->max_cycles;
	const char *name = tk->tkr_mono.clock->name;

	if (offset > max_cycles) {
		printk_deferred("WARNING: timekeeping: Cycle offset (%lld) is larger than allowed by the '%s' clock's max_cycles value (%lld): time overflow danger\n",
				offset, name, max_cycles);
		printk_deferred("         timekeeping: Your kernel is sick, but tries to cope by capping time updates\n");
	} else {
		if (offset > (max_cycles >> 1)) {
			printk_deferred("INFO: timekeeping: Cycle offset (%lld) is larger than the '%s' clock's 50%% safety margin (%lld)\n",
					offset, name, max_cycles >> 1);
			printk_deferred("      timekeeping: Your kernel is still fine, but is feeling a bit nervous\n");
		}
	}

	if (tk->underflow_seen) {
		if (jiffies - tk->last_warning > WARNING_FREQ) {
			printk_deferred("WARNING: Underflow in clocksource '%s' observed, time update ignored.\n", name);
			printk_deferred("         Please report this, consider using a different clocksource, if possible.\n");
			printk_deferred("         Your kernel is probably still fine.\n");
			tk->last_warning = jiffies;
		}
		tk->underflow_seen = 0;
	}

	if (tk->overflow_seen) {
		if (jiffies - tk->last_warning > WARNING_FREQ) {
			printk_deferred("WARNING: Overflow in clocksource '%s' observed, time update capped.\n", name);
			printk_deferred("         Please report this, consider using a different clocksource, if possible.\n");
			printk_deferred("         Your kernel is probably still fine.\n");
			tk->last_warning = jiffies;
		}
		tk->overflow_seen = 0;
	}
}

static inline u64 timekeeping_get_delta(const struct tk_read_base *tkr)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	u64 now, last, mask, max, delta;
	unsigned int seq;

	/*
	 * Since we're called holding a seqcount, the data may shift
	 * under us while we're doing the calculation. This can cause
	 * false positives, since we'd note a problem but throw the
	 * results away. So nest another seqcount here to atomically
	 * grab the points we are checking with.
	 */
	do {
		seq = read_seqcount_begin(&tk_core.seq);
		now = tk_clock_read(tkr);
		last = tkr->cycle_last;
		mask = tkr->mask;
		max = tkr->clock->max_cycles;
	} while (read_seqcount_retry(&tk_core.seq, seq));

	delta = clocksource_delta(now, last, mask);

	/*
	 * Try to catch underflows by checking if we are seeing small
	 * mask-relative negative values.
	 */
	if (unlikely((~delta & mask) < (mask >> 3))) {
		tk->underflow_seen = 1;
		delta = 0;
	}

	/* Cap delta value to the max_cycles values to avoid mult overflows */
	if (unlikely(delta > max)) {
		tk->overflow_seen = 1;
		delta = tkr->clock->max_cycles;
	}

	return delta;
}
#else
static inline void timekeeping_check_update(struct timekeeper *tk, u64 offset)
{
}
static inline u64 timekeeping_get_delta(const struct tk_read_base *tkr)
{
	u64 cycle_now, delta;

	/* read clocksource */
	cycle_now = tk_clock_read(tkr);

	/* calculate the delta since the last update_wall_time */
	delta = clocksource_delta(cycle_now, tkr->cycle_last, tkr->mask);

	return delta;
}
#endif

#ifdef CONFIG_FFCLOCK
/*
 * Support for feed-forward synchronization algorithms. This is heavily inspired
 * by the FFclock code for FreeBSD.
 */

/* Feed-forward clock estimates kept updated by the synchronization daemon. */
struct ffclock_estimate ffclock_estimate;
struct bintime ffclock_boottime;	/* Feed-forward boot time estimate. */
uint32_t ffclock_status;		/* Feed-forward clock status. */
int8_t ffclock_updated;			/* New estimates are available. */
DECLARE_RWSEM(ffclock_mtx);	/* semaphore on ffclock_estimate: declare&init */

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

static struct fftimehands ffth[10];
static struct fftimehands *volatile fftimehands = ffth;

/* Perform a initial setup of the ffclock variables, one time only
 *  - circularly link fftimehands buffers, and init all fields to zero
 *  - set ffclock status to UNSYNC, and signal no ffdata updates available
 */
static void
ffclock_init(void)
{
	struct fftimehands *cur;
	struct fftimehands *last;

	memset(ffth, 0, sizeof(ffth));

	last = ffth + (sizeof(ffth) / sizeof(*ffth)) - 1;	// (./.) = #elements(ffth)
	for (cur = ffth; cur < last; cur++)
		cur->next = cur + 1;
	last->next = ffth;

	memset(&ffclock_estimate, 0, sizeof(ffclock_estimate));
	ffclock_updated = 0;
	ffclock_status = FFCLOCK_STA_UNSYNC;
	ffclock_boottime.sec = ffclock_boottime.frac = 0;
//	mtx_init(&ffclock_mtx, "ffclock lock", NULL, MTX_DEF);
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

/**
 * ffclock_read_counter - Return the value of the ffcount to functions within the kernel.
 *  Old comment from read_ffcounter_bypass: Direct reads from hardware, required for virtual OS (e.g. Xen)
 *  So is in fact:  bypass = don't use FFC, use the direct cs    [ a kernel thing, daemon doesn't need to know ]
 *        passthrough = use rdtsc                                       [ both kernel & daemon need to know ]
 */
void ffclock_read_counter(ffcounter *ffcount)
{
	struct timekeeper *tk;
	struct fftimehands *ffth;
	unsigned seq;
	u64 delta;

	tk = &tk_core.timekeeper;
	if (ffcounter_bypass == 1 && strcmp(tk->tkr_raw.clock->name, "tsc") == 0) {
#ifdef __x86_64__
		*ffcount = rdtsc_ordered();
#else
		*ffcount = 0;
#endif
		return;
	}

	/* Read the FFC: obtain a consistent view using tk_core.seq to check */
	do {
		tk = &tk_core.timekeeper;
		seq = read_seqcount_begin(&tk_core.seq);
		if (ffcounter_bypass == 1)
			*ffcount = (ffcounter) tk_clock_read(&tk->tkr_raw);
		else {
			ffth = fftimehands;
			delta = timekeeping_get_delta(&tk->tkr_raw);		// calls tk_clock_read
			if (delta == 0)
				printk("ffclock_read_counter: got delta = %llu with tick_ffcount = %llu \n",
						delta, ffth->tick_ffcount);
			*ffcount = ffth->tick_ffcount + delta;
		}
	} while (read_seqcount_retry(&tk_core.seq, seq));

}
EXPORT_SYMBOL(ffclock_read_counter);

/* Given a raw ffcount timestamp, use it to read the FFclock system, and pass
 * back timestamp(s) as requested and specified by tsmode.
 * Apart from BPF_T_NONE, the FORMAT dimension of tsmode is ignored as linux
 * requires an underlying ktime format.
 */
void
ffclock_fill_timestamps(ffcounter ffc, long tsmode, ffcounter *rawts, ktime_t *ts)
{

	struct fftimehands *ffth;
	struct bintime bt2, bt;
	ffcounter ffdelta;
	uint64_t period;
	uint8_t gen;

//	printk("ffclock_fill_timestamps: ffc = %llu, tsmode = 0x%04lx\n", ffc, (unsigned long)tsmode);

	/* Raw timestamp processing */
	if (BPF_T_FFRAW(tsmode) == BPF_T_FFC)
		if (rawts) *rawts = ffc;

	/* Normal timestamp: process FORMAT dimension - as much as possible here */
	switch (BPF_T_FORMAT(tsmode)) {
		case BPF_T_NONE:
			if (ts) *ts = 0;
			return;
		case BPF_T_MICROTIME:
		case BPF_T_BINTIME:
//			printk(KERN_INFO "ffclock_fill_timestamps: sorry, you are stuck with NANOTIME + pcap.. \n");
		case BPF_T_NANOTIME:
		default:
			break;
	}

	/* Normal timestamp: ensure consistent data used for remaining dimensions */
	do {
		ffth = fftimehands;	// copy the current fftimehands, it may change
		gen = ffth->gen;

		/* Read the desired clock at tick-start
		 * If it is not an FFclock, we are done, the existing *ts will stand.
		 * If it is not an FFclock, wipe *ts to prompt subsequent normal filling by sysclock */
		switch (BPF_T_CLOCK(tsmode)) {
			case BPF_T_SYSCLOCK:
			case BPF_T_FBCLOCK:
				if (ts) *ts = 0;
				return;
			case BPF_T_FFNATIVECLOCK:
				bt = ffth->tick_time;
				period = ffth->cest.period;
				break;
			case BPF_T_FFDIFFCLOCK:
				bt = ffth->tick_time_diff;
				period = ffth->cest.period;
				break;
			case BPF_T_FFCLOCK:
			default:
				bt = ffth->tick_time_lerp;
				period = ffth->period_lerp;
				break;
		}
		/* If tick resoln suffices or speed required, just use tick-start values,
		 * otherwise calculate the delta wrt tick-start and apply to bt.
		 * Use of FAST not recommended here as the FFC is already read! may
		 * result in an error much larger than a tick duration.
		 */
		if ((BPF_T_FLAG(tsmode) & BPF_T_FAST) != BPF_T_FAST)  {
			if (ffc > ffth->tick_ffcount)
				ffdelta = ffc - ffth->tick_ffcount;
			else
				ffdelta = ffth->tick_ffcount - ffc;

			ffclock_convert_delta(ffdelta, period, &bt2);
			if (ffc > ffth->tick_ffcount)
				bintime_add(&bt, &bt2);
			else
				bintime_sub(&bt, &bt2);
		}

		/* Adjust for leap seconds (if not the FFdiff clock).
		 * Must add in total leaps seen since daemon start, then include
		 * the possibility that an upcoming one (predicted for leapsec_expected)
		 * has just past. Include it based on ffc, even if BPF_T_FAST.
		 */
		if (BPF_T_CLOCK(tsmode) < BPF_T_FFDIFFCLOCK) {
			bt.sec -= ffth->cest.leapsec_total;	// subtract means include leaps
			if (ffth->cest.leapsec_expected != 0 && ffc > ffth->cest.leapsec_expected)
				bt.sec -= ffth->cest.leapsec_next;
		}

		/* Convert to uptime/monotonic form if requested.
		 * Note: an uptime FFdiff clock is defined here, but is not useful.
		 */
		if ((BPF_T_FLAG(tsmode) & BPF_T_MONOTONIC) == BPF_T_MONOTONIC) {
			if (bintime_cmp(&ffclock_boottime, &bt, >)) {	// would go -ve !
				printk("** Uptime going -ve !  bt:  %llu.%lu  ffclock_boottime: %llu.%lu\n",
						(unsigned long long)bt.sec,
						(long unsigned)(bt.frac / MUS_AS_BINFRAC),
						(unsigned long long)ffclock_boottime.sec,
						(long unsigned)(ffclock_boottime.frac / NS_AS_BINFRAC) );
				bt = ffclock_boottime;			// ensure Uptime >= 0
			}
			bintime_sub(&bt, &ffclock_boottime);
		}

	} while (gen == 0 || gen != ffth->gen);

	/* Convert final result from bintime --> ktime */
	*ts = NSEC_PER_SEC * bt.sec + (s64)(bt.frac / (uint64_t)NS_AS_BINFRAC);

}

EXPORT_SYMBOL_GPL(ffclock_fill_timestamps);


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
 * The instant defining the start of the new tick is effectively defined by the
 * passed delta.  This is simply mirrored here in the FF counter `read'.
 *
 * The boolean skip_update optionally suppresses the processing of an FFdata
 * update, if present, leaving it for a later tick.
 */
static void
ffclock_windup(unsigned int delta, int skip_update)
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
	 * Signal to ignore a stale (pre-reset) daemon update following a RTC reset.
	 */
	if (ffclock_updated > 0 && fftimehands->cest.secs_to_nextupdate == 0
									&& bintime_cmp(&fftimehands->cest.update_time, \
														&ffclock_estimate.update_time,>) ) {
		ffclock_updated = 0;
		printk("Ignoring stale FFdata update following RTC reset.\n");
	}


	/*
	 * No acceptable update in FFclock parameters to process.
	 * Includes case of daemon update following a RTC reset that must be ignored.
	 * Tick state update based on copy or simple projection from previous tick.
	 */
	if (skip_update || ffclock_updated <= 0) {

		/* Update native FFclock members {cest, tick_time{_diff}, tick_error} */
		memcpy(cest, &fftimehands->cest, sizeof(struct ffclock_estimate));
		ffth->tick_time		= fftimehands->tick_time;
		ffth->tick_time_diff = fftimehands->tick_time_diff;
		ffclock_convert_delta(ffdelta, cest->period, &bt);  // use bintime_addx(bt, period * ffdelta); ?
		bintime_add(&ffth->tick_time, &bt);
		bintime_add(&ffth->tick_time_diff, &bt);
		bintime_mul(&bt, cest->errb_rate * PS_AS_BINFRAC);	// errb_rate in [ps/s]
		bintime_add(&ffth->tick_error, &bt);

		/* Update mono FFclock members {period_lerp, tick_time_lerp} */
		ffth->period_lerp 	= fftimehands->period_lerp;
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
	if (ffclock_updated > 0 && !skip_update) {

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
				printk("ffwindup:  Jumping monotonic FFclock forward by %llu.%03lu",
							(unsigned long long)gap_lerp.sec,
			 				(long unsigned)(gap_lerp.frac / MS_AS_BINFRAC) );
				bintime_add(&ffclock_boottime, &gap_lerp);
			} else {
				printk("ffwindup:  Jumping monotonic FFclock backward by %llu.%03lu",
							(unsigned long long)gap_lerp.sec,
			 				(long unsigned)(gap_lerp.frac / MS_AS_BINFRAC) );
				bintime_sub(&ffclock_boottime, &gap_lerp);
			}

			ffth->tick_time_lerp = ffth->tick_time;

			upt = ffth->tick_time_lerp;
			bintime_sub(&upt, &ffclock_boottime);
			printk(" (uptime preserved at: %llu.%03lu)\n",
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


/* Set or reset FFclock variables following special events :
 *   initialization:     [ flagged by ncount = 0 ]
 *   change in counter:  [ flagged by ncount > 0 ]
 *   time-setting:       [ flagged by reset_UTC non-NULL ]
 *   time-shifting:      [ flagged by delta_UTC non-NULL ]
 * This function does not advance the tick itself, instead it updates the tick
 * state to reflect the post-event view (of course fftimehands is advanced).
 * To that end it assumes that the ffclock has, just before invocation, been
 * advanced to the current tick via ffclock_windup, using the previous counter.
 * Here tk is assumed to have been updated to the new counter if applicable.
 * No effort is made to align the FB bootime variable with ffclock_boottime, the
 * FFclock Up variants are based on the FFclocks only.
 * If invoked with both reset args set, delta_UTC is ignored.
 */
static void
ffclock_reset(struct timekeeper *tk, u64 ncount, const struct timespec64 *reset_UTC,
				const struct timespec64 *delta_UTC)
{
	struct fftimehands *ffth;
	struct ffclock_estimate *cest;
	struct bintime reset_bin, gap, upt;
	struct clocksource *cs;

	uint8_t ogen;

	/* Prepare next fftimehand where tick state will be updated */
	ffth = fftimehands->next;
	ogen = ffth->gen;
	ffth->gen = 0;
	cest = &ffth->cest;

	/* Reset FFC at tick start if needed to match new counter.
	 * Code is the same whether this be an initialization, or a clock change.
	 * In each case the FFC origin must be set to a value consistent with the
	 * read, held in "ncount", of the new counter.
	 * If the TSC, ncount should already be a full read since the true origin.
	 * If not TSC, we cannot guarantee recovery of the true origin, so we accept
	 * the tk view, where implicitly the origin will be ncount in the past.
	 * In all cases, the lower bits of FFcounter and the tk counter will agree.
	 *
	 * Thus in both cases we simply set  tick_ffcount = ncount  .
	 * Leave the testing here for the moment as a reminder that a reconstruction
	 * may be needed in some cases (eg a TSC-low type shift).
	 */
	cs = tk->tkr_raw.clock;
	if (ncount>0) {
		if ( strcmp(cs->name, "tsc") != 0 )
			ffth->tick_ffcount = (ffcounter) ncount;  // origin matches tk view
		else {
			ffth->tick_ffcount = (ffcounter) ncount;  // origin = true TSC origin
		}
		printk("FFclock processing cs change: new tick_ffcount = %llu = %#llX, with cs %s \n",
			(unsigned long long)ffth->tick_ffcount,
			(unsigned long long)ffth->tick_ffcount, cs->name);
	} else
		ffth->tick_ffcount = fftimehands->tick_ffcount;	// existing value valid


	/* If UTC time has been reset, align FFclock to it, preserving Uptime */
	if (reset_UTC || delta_UTC) {

		/* Convert input reset type to a UTC bintime target "reset_bin" */
		if (reset_UTC) {
			reset_bin.sec  = reset_UTC->tv_sec; // TODO: incorporate leap seconds
			reset_bin.frac = reset_UTC->tv_nsec * (uint64_t)NS_AS_BINFRAC;
		} else {
			reset_bin.sec  = delta_UTC->tv_sec;
			reset_bin.frac = delta_UTC->tv_nsec * (uint64_t)NS_AS_BINFRAC;
			bintime_add(&reset_bin, &fftimehands->tick_time);
		}

		/* Adjust ffclock_boottime to preserve UPclock continuity
		 * TODO: don't want this is you suspended.. how to tell cause of reset? */
		ffth->tick_time_lerp = fftimehands->tick_time_lerp;
		bintime_clear(&gap);
		if (bintime_cmp(&reset_bin, &ffth->tick_time_lerp, >)) {
			gap = reset_bin;
			bintime_sub(&gap, &ffth->tick_time_lerp);
			bintime_add(&ffclock_boottime, &gap);
		} else {
			gap = ffth->tick_time_lerp;
			bintime_sub(&gap, &reset_bin);
			bintime_sub(&ffclock_boottime, &gap);
		}

		/* Reset FFclocks */
		ffth->tick_time      = reset_bin;
		ffth->tick_time_lerp = reset_bin;
		if (ncount>0)	// know counter was discontinuous, reset
			ffth->tick_time_diff = reset_bin;
		else           // hope counter is continuous, keep it that way
			ffth->tick_time_diff = fftimehands->tick_time_diff;

		upt = ffth->tick_time_lerp;
		bintime_sub(&upt, &ffclock_boottime);
		printk("FFclock processing reset: UTC: %ld.%03lu"
			 	 " boottime: %llu.%03lu, uptime preserved at: %llu.%03lu\n",
			(long)ffth->tick_time_lerp.sec,
			(unsigned long)(ffth->tick_time_lerp.frac / MS_AS_BINFRAC),
			(unsigned long long)ffclock_boottime.sec,
			(long unsigned)(ffclock_boottime.frac / MS_AS_BINFRAC),
			(unsigned long long)upt.sec,
			(long unsigned)(upt.frac / MS_AS_BINFRAC) );

	} else {
		ffth->tick_time      = fftimehands->tick_time;
		ffth->tick_time_diff = fftimehands->tick_time_diff;
		ffth->tick_time_lerp = fftimehands->tick_time_lerp;
	}

	/* Alter FFdata to consistently reflect reset, taken to occur @ tick-start */
	memcpy(cest, &(fftimehands->cest), sizeof(struct ffclock_estimate));
	cest->update_time    = ffth->tick_time;
	cest->update_ffcount = ffth->tick_ffcount;
	cest->secs_to_nextupdate = 0;		// signals no daemon update since reset

	/* If never set or counter change, init periods to nominal */
	if (cest->period == 0 || ncount > 0) {
		cest->period = ((u64) NS_AS_BINFRAC * cs->mult) >> cs->shift ;
		cest->errb_rate = 0;
		cest->errb_abs = 0;
		cest->status   = FFCLOCK_STA_UNSYNC;
		ffclock_status = FFCLOCK_STA_UNSYNC;
		ffth->period_lerp = cest->period;
		printk("FFclock processing init: period = %llu [binfrac], %lu [ps]\n",
				cest->period, (long unsigned) ((cest->period)/PS_AS_BINFRAC)  );
	} else
		ffth->period_lerp = fftimehands->period_lerp;

   /* Push modiied FFdata back to global to communicate reset to daemon. */
	down_read(&ffclock_mtx);
	memcpy(&ffclock_estimate, cest, sizeof(struct ffclock_estimate));
	ffclock_updated = 0;		// suppress processing of this altered update
	if (ncount > 0)
		ffclock_updated--;	// ignore next daemon update, likely based on old cs
	up_read(&ffclock_mtx);

	/* Reset remaining fftimehands members TODO: make this more accurate */
	ffth->tick_error = fftimehands->tick_error;

	if (++ogen == 0)
		ogen = 1;
	ffth->gen = ogen;
	fftimehands = ffth;

}



#endif	// CONFIG_FFCLOCK





/**
 * tk_setup_internals - Set up internals to use clocksource clock.
 *
 * @tk:		The target timekeeper to setup.
 * @clock:		Pointer to clocksource.
 *
 * Calculates a fixed cycle/nsec interval for a given clocksource/adjustment
 * pair and interval request.
 *
 * Unless you're the timekeeping code, you should not be using this!
 */
static void tk_setup_internals(struct timekeeper *tk, struct clocksource *clock)
{
	u64 interval;
	u64 tmp, ntpinterval;
	struct clocksource *old_clock;

	++tk->cs_was_changed_seq;
	old_clock = tk->tkr_mono.clock;
	tk->tkr_mono.clock = clock;
	tk->tkr_mono.mask = clock->mask;
	tk->tkr_mono.cycle_last = tk_clock_read(&tk->tkr_mono);

	tk->tkr_raw.clock = clock;
	tk->tkr_raw.mask = clock->mask;
	tk->tkr_raw.cycle_last = tk->tkr_mono.cycle_last;

	/* Do the ns -> cycle conversion first, using original mult */
	tmp = NTP_INTERVAL_LENGTH;
	tmp <<= clock->shift;
	ntpinterval = tmp;
	tmp += clock->mult/2;
	do_div(tmp, clock->mult);
	if (tmp == 0)
		tmp = 1;

	interval = (u64) tmp;
	tk->cycle_interval = interval;

	/* Go back from cycles -> shifted ns */
	tk->xtime_interval = interval * clock->mult;
	tk->xtime_remainder = ntpinterval - tk->xtime_interval;
	tk->raw_interval = interval * clock->mult;

	 /* if changing clocks, convert xtime_nsec shift units */
	if (old_clock) {
		int shift_change = clock->shift - old_clock->shift;
		if (shift_change < 0) {
			tk->tkr_mono.xtime_nsec >>= -shift_change;
			tk->tkr_raw.xtime_nsec >>= -shift_change;
		} else {
			tk->tkr_mono.xtime_nsec <<= shift_change;
			tk->tkr_raw.xtime_nsec <<= shift_change;
		}
	}

	tk->tkr_mono.shift = clock->shift;
	tk->tkr_raw.shift = clock->shift;

	tk->ntp_error = 0;
	tk->ntp_error_shift = NTP_SCALE_SHIFT - clock->shift;
	tk->ntp_tick = ntpinterval << tk->ntp_error_shift;

	/*
	 * The timekeeper keeps its own mult values for the currently
	 * active clocksource. These value will be adjusted via NTP
	 * to counteract clock drifting.
	 */
	tk->tkr_mono.mult = clock->mult;
	tk->tkr_raw.mult = clock->mult;
	tk->ntp_err_mult = 0;
	tk->skip_second_overflow = 0;
}

/* Timekeeper helper functions. */

#ifdef CONFIG_ARCH_USES_GETTIMEOFFSET
static u32 default_arch_gettimeoffset(void) { return 0; }
u32 (*arch_gettimeoffset)(void) = default_arch_gettimeoffset;
#else
static inline u32 arch_gettimeoffset(void) { return 0; }
#endif

static inline u64 timekeeping_delta_to_ns(const struct tk_read_base *tkr, u64 delta)
{
	u64 nsec;

	nsec = delta * tkr->mult + tkr->xtime_nsec;
	nsec >>= tkr->shift;

	/* If arch requires, add in get_arch_timeoffset() */
	return nsec + arch_gettimeoffset();
}

static inline u64 timekeeping_get_ns(const struct tk_read_base *tkr)
{
	u64 delta;

	delta = timekeeping_get_delta(tkr);
	return timekeeping_delta_to_ns(tkr, delta);
}

static inline u64 timekeeping_cycles_to_ns(const struct tk_read_base *tkr, u64 cycles)
{
	u64 delta;

	/* calculate the delta since the last update_wall_time */
	delta = clocksource_delta(cycles, tkr->cycle_last, tkr->mask);
	return timekeeping_delta_to_ns(tkr, delta);
}

/**
 * update_fast_timekeeper - Update the fast and NMI safe monotonic timekeeper.
 * @tkr: Timekeeping readout base from which we take the update
 *
 * We want to use this from any context including NMI and tracing /
 * instrumenting the timekeeping code itself.
 *
 * Employ the latch technique; see @raw_write_seqcount_latch.
 *
 * So if a NMI hits the update of base[0] then it will use base[1]
 * which is still consistent. In the worst case this can result is a
 * slightly wrong timestamp (a few nanoseconds). See
 * @ktime_get_mono_fast_ns.
 */
static void update_fast_timekeeper(const struct tk_read_base *tkr,
				   struct tk_fast *tkf)
{
	struct tk_read_base *base = tkf->base;

	/* Force readers off to base[1] */
	raw_write_seqcount_latch(&tkf->seq);

	/* Update base[0] */
	memcpy(base, tkr, sizeof(*base));

	/* Force readers back to base[0] */
	raw_write_seqcount_latch(&tkf->seq);

	/* Update base[1] */
	memcpy(base + 1, base, sizeof(*base));
}

/**
 * ktime_get_mono_fast_ns - Fast NMI safe access to clock monotonic
 *
 * This timestamp is not guaranteed to be monotonic across an update.
 * The timestamp is calculated by:
 *
 *	now = base_mono + clock_delta * slope
 *
 * So if the update lowers the slope, readers who are forced to the
 * not yet updated second array are still using the old steeper slope.
 *
 * tmono
 * ^
 * |    o  n
 * |   o n
 * |  u
 * | o
 * |o
 * |12345678---> reader order
 *
 * o = old slope
 * u = update
 * n = new slope
 *
 * So reader 6 will observe time going backwards versus reader 5.
 *
 * While other CPUs are likely to be able observe that, the only way
 * for a CPU local observation is when an NMI hits in the middle of
 * the update. Timestamps taken from that NMI context might be ahead
 * of the following timestamps. Callers need to be aware of that and
 * deal with it.
 */
static __always_inline u64 __ktime_get_fast_ns(struct tk_fast *tkf)
{
	struct tk_read_base *tkr;
	unsigned int seq;
	u64 now;

	do {
		seq = raw_read_seqcount_latch(&tkf->seq);
		tkr = tkf->base + (seq & 0x01);
		now = ktime_to_ns(tkr->base);

		now += timekeeping_delta_to_ns(tkr,
				clocksource_delta(
					tk_clock_read(tkr),
					tkr->cycle_last,
					tkr->mask));
	} while (read_seqcount_latch_retry(&tkf->seq, seq));

	return now;
}

u64 ktime_get_mono_fast_ns(void)
{
	return __ktime_get_fast_ns(&tk_fast_mono);
}
EXPORT_SYMBOL_GPL(ktime_get_mono_fast_ns);

u64 ktime_get_raw_fast_ns(void)
{
	return __ktime_get_fast_ns(&tk_fast_raw);
}
EXPORT_SYMBOL_GPL(ktime_get_raw_fast_ns);

/**
 * ktime_get_boot_fast_ns - NMI safe and fast access to boot clock.
 *
 * To keep it NMI safe since we're accessing from tracing, we're not using a
 * separate timekeeper with updates to monotonic clock and boot offset
 * protected with seqcounts. This has the following minor side effects:
 *
 * (1) Its possible that a timestamp be taken after the boot offset is updated
 * but before the timekeeper is updated. If this happens, the new boot offset
 * is added to the old timekeeping making the clock appear to update slightly
 * earlier:
 *    CPU 0                                        CPU 1
 *    timekeeping_inject_sleeptime64()
 *    __timekeeping_inject_sleeptime(tk, delta);
 *                                                 timestamp();
 *    timekeeping_update(tk, TK_CLEAR_NTP...);
 *
 * (2) On 32-bit systems, the 64-bit boot offset (tk->offs_boot) may be
 * partially updated.  Since the tk->offs_boot update is a rare event, this
 * should be a rare occurrence which postprocessing should be able to handle.
 */
u64 notrace ktime_get_boot_fast_ns(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;

	return (ktime_get_mono_fast_ns() + ktime_to_ns(tk->offs_boot));
}
EXPORT_SYMBOL_GPL(ktime_get_boot_fast_ns);

/*
 * See comment for __ktime_get_fast_ns() vs. timestamp ordering
 */
static __always_inline u64 __ktime_get_real_fast(struct tk_fast *tkf, u64 *mono)
{
	struct tk_read_base *tkr;
	u64 basem, baser, delta;
	unsigned int seq;

	do {
		seq = raw_read_seqcount_latch(&tkf->seq);
		tkr = tkf->base + (seq & 0x01);
		basem = ktime_to_ns(tkr->base);
		baser = ktime_to_ns(tkr->base_real);

		delta = timekeeping_delta_to_ns(tkr,
				clocksource_delta(tk_clock_read(tkr),
				tkr->cycle_last, tkr->mask));
	} while (read_seqcount_latch_retry(&tkf->seq, seq));

	if (mono)
		*mono = basem + delta;
	return baser + delta;
}

/**
 * ktime_get_real_fast_ns: - NMI safe and fast access to clock realtime.
 */
u64 ktime_get_real_fast_ns(void)
{
	return __ktime_get_real_fast(&tk_fast_mono, NULL);
}
EXPORT_SYMBOL_GPL(ktime_get_real_fast_ns);

/**
 * ktime_get_fast_timestamps: - NMI safe timestamps
 * @snapshot:	Pointer to timestamp storage
 *
 * Stores clock monotonic, boottime and realtime timestamps.
 *
 * Boot time is a racy access on 32bit systems if the sleep time injection
 * happens late during resume and not in timekeeping_resume(). That could
 * be avoided by expanding struct tk_read_base with boot offset for 32bit
 * and adding more overhead to the update. As this is a hard to observe
 * once per resume event which can be filtered with reasonable effort using
 * the accurate mono/real timestamps, it's probably not worth the trouble.
 *
 * Aside of that it might be possible on 32 and 64 bit to observe the
 * following when the sleep time injection happens late:
 *
 * CPU 0				CPU 1
 * timekeeping_resume()
 * ktime_get_fast_timestamps()
 *	mono, real = __ktime_get_real_fast()
 *					inject_sleep_time()
 *					   update boot offset
 *	boot = mono + bootoffset;
 *
 * That means that boot time already has the sleep time adjustment, but
 * real time does not. On the next readout both are in sync again.
 *
 * Preventing this for 64bit is not really feasible without destroying the
 * careful cache layout of the timekeeper because the sequence count and
 * struct tk_read_base would then need two cache lines instead of one.
 *
 * Access to the time keeper clock source is disabled accross the innermost
 * steps of suspend/resume. The accessors still work, but the timestamps
 * are frozen until time keeping is resumed which happens very early.
 *
 * For regular suspend/resume there is no observable difference vs. sched
 * clock, but it might affect some of the nasty low level debug printks.
 *
 * OTOH, access to sched clock is not guaranteed accross suspend/resume on
 * all systems either so it depends on the hardware in use.
 *
 * If that turns out to be a real problem then this could be mitigated by
 * using sched clock in a similar way as during early boot. But it's not as
 * trivial as on early boot because it needs some careful protection
 * against the clock monotonic timestamp jumping backwards on resume.
 */
void ktime_get_fast_timestamps(struct ktime_timestamps *snapshot)
{
	struct timekeeper *tk = &tk_core.timekeeper;

	snapshot->real = __ktime_get_real_fast(&tk_fast_mono, &snapshot->mono);
	snapshot->boot = snapshot->mono + ktime_to_ns(data_race(tk->offs_boot));
}

/**
 * halt_fast_timekeeper - Prevent fast timekeeper from accessing clocksource.
 * @tk: Timekeeper to snapshot.
 *
 * It generally is unsafe to access the clocksource after timekeeping has been
 * suspended, so take a snapshot of the readout base of @tk and use it as the
 * fast timekeeper's readout base while suspended.  It will return the same
 * number of cycles every time until timekeeping is resumed at which time the
 * proper readout base for the fast timekeeper will be restored automatically.
 */
static void halt_fast_timekeeper(const struct timekeeper *tk)
{
	static struct tk_read_base tkr_dummy;
	const struct tk_read_base *tkr = &tk->tkr_mono;

	memcpy(&tkr_dummy, tkr, sizeof(tkr_dummy));
	cycles_at_suspend = tk_clock_read(tkr);
	tkr_dummy.clock = &dummy_clock;
	tkr_dummy.base_real = tkr->base + tk->offs_real;
	update_fast_timekeeper(&tkr_dummy, &tk_fast_mono);

	tkr = &tk->tkr_raw;
	memcpy(&tkr_dummy, tkr, sizeof(tkr_dummy));
	tkr_dummy.clock = &dummy_clock;
	update_fast_timekeeper(&tkr_dummy, &tk_fast_raw);
}

static RAW_NOTIFIER_HEAD(pvclock_gtod_chain);

static void update_pvclock_gtod(struct timekeeper *tk, bool was_set)
{
	raw_notifier_call_chain(&pvclock_gtod_chain, was_set, tk);
}

/**
 * pvclock_gtod_register_notifier - register a pvclock timedata update listener
 */
int pvclock_gtod_register_notifier(struct notifier_block *nb)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned long flags;
	int ret;

	raw_spin_lock_irqsave(&timekeeper_lock, flags);
	ret = raw_notifier_chain_register(&pvclock_gtod_chain, nb);
	update_pvclock_gtod(tk, true);
	raw_spin_unlock_irqrestore(&timekeeper_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(pvclock_gtod_register_notifier);

/**
 * pvclock_gtod_unregister_notifier - unregister a pvclock
 * timedata update listener
 */
int pvclock_gtod_unregister_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	raw_spin_lock_irqsave(&timekeeper_lock, flags);
	ret = raw_notifier_chain_unregister(&pvclock_gtod_chain, nb);
	raw_spin_unlock_irqrestore(&timekeeper_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(pvclock_gtod_unregister_notifier);

/*
 * tk_update_leap_state - helper to update the next_leap_ktime
 */
static inline void tk_update_leap_state(struct timekeeper *tk)
{
	tk->next_leap_ktime = ntp_get_next_leap();
	if (tk->next_leap_ktime != KTIME_MAX)
		/* Convert to monotonic time */
		tk->next_leap_ktime = ktime_sub(tk->next_leap_ktime, tk->offs_real);
}

/*
 * Update the ktime_t based scalar nsec members of the timekeeper
 */
static inline void tk_update_ktime_data(struct timekeeper *tk)
{
	u64 seconds;
	u32 nsec;

	/*
	 * The xtime based monotonic readout is:
	 *	nsec = (xtime_sec + wtm_sec) * 1e9 + wtm_nsec + now();
	 * The ktime based monotonic readout is:
	 *	nsec = base_mono + now();
	 * ==> base_mono = (xtime_sec + wtm_sec) * 1e9 + wtm_nsec
	 */
	seconds = (u64)(tk->xtime_sec + tk->wall_to_monotonic.tv_sec);
	nsec = (u32) tk->wall_to_monotonic.tv_nsec;
	tk->tkr_mono.base = ns_to_ktime(seconds * NSEC_PER_SEC + nsec);

	/*
	 * The sum of the nanoseconds portions of xtime and
	 * wall_to_monotonic can be greater/equal one second. Take
	 * this into account before updating tk->ktime_sec.
	 */
	nsec += (u32)(tk->tkr_mono.xtime_nsec >> tk->tkr_mono.shift);
	if (nsec >= NSEC_PER_SEC)
		seconds++;
	tk->ktime_sec = seconds;

	/* Update the monotonic raw base */
	tk->tkr_raw.base = ns_to_ktime(tk->raw_sec * NSEC_PER_SEC);
}

/* must hold timekeeper_lock */
static void timekeeping_update(struct timekeeper *tk, unsigned int action)
{
	if (action & TK_CLEAR_NTP) {
		tk->ntp_error = 0;
		ntp_clear();
	}

	tk_update_leap_state(tk);
	tk_update_ktime_data(tk);

	update_vsyscall(tk);
	update_pvclock_gtod(tk, action & TK_CLOCK_WAS_SET);

	tk->tkr_mono.base_real = tk->tkr_mono.base + tk->offs_real;
	update_fast_timekeeper(&tk->tkr_mono, &tk_fast_mono);
	update_fast_timekeeper(&tk->tkr_raw,  &tk_fast_raw);

	if (action & TK_CLOCK_WAS_SET)
		tk->clock_was_set_seq++;
	/*
	 * The mirroring of the data to the shadow-timekeeper needs
	 * to happen last here to ensure we don't over-write the
	 * timekeeper structure on the next update with stale data
	 */
	if (action & TK_MIRROR)
		memcpy(&shadow_timekeeper, &tk_core.timekeeper,
		       sizeof(tk_core.timekeeper));
}

/**
 * timekeeping_forward_now - update clock to the current time
 *
 * Forward the current clock to update its state since the last call to
 * update_wall_time(). This is useful before significant clock changes,
 * as it avoids having to deal with this time offset explicitly.
 */
static void timekeeping_forward_now(struct timekeeper *tk)
{
	u64 cycle_now, delta;

	cycle_now = tk_clock_read(&tk->tkr_mono);
	delta = clocksource_delta(cycle_now, tk->tkr_mono.cycle_last, tk->tkr_mono.mask);
	tk->tkr_mono.cycle_last = cycle_now;
	tk->tkr_raw.cycle_last  = cycle_now;

	tk->tkr_mono.xtime_nsec += delta * tk->tkr_mono.mult;

	/* If arch requires, add in get_arch_timeoffset() */
	tk->tkr_mono.xtime_nsec += (u64)arch_gettimeoffset() << tk->tkr_mono.shift;

	tk->tkr_raw.xtime_nsec += delta * tk->tkr_raw.mult;

	/* If arch requires, add in get_arch_timeoffset() */
	tk->tkr_raw.xtime_nsec += (u64)arch_gettimeoffset() << tk->tkr_raw.shift;

	tk_normalize_xtime(tk);

#ifdef CONFIG_FFCLOCK
	ffclock_windup(delta, 1);	// advance delta cycles, ignore any FFdata update
#endif

}

/**
 * ktime_get_real_ts64 - Returns the time of day in a timespec64.
 * @ts:		pointer to the timespec to be set
 *
 * Returns the time of day in a timespec64 (WARN if suspended).
 */
void ktime_get_real_ts64(struct timespec64 *ts)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	u64 nsecs;

	WARN_ON(timekeeping_suspended);

	do {
		seq = read_seqcount_begin(&tk_core.seq);

		ts->tv_sec = tk->xtime_sec;
		nsecs = timekeeping_get_ns(&tk->tkr_mono);

	} while (read_seqcount_retry(&tk_core.seq, seq));

	ts->tv_nsec = 0;
	timespec64_add_ns(ts, nsecs);
}
EXPORT_SYMBOL(ktime_get_real_ts64);

ktime_t ktime_get(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	ktime_t base;
	u64 nsecs;

	WARN_ON(timekeeping_suspended);

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		base = tk->tkr_mono.base;
		nsecs = timekeeping_get_ns(&tk->tkr_mono);

	} while (read_seqcount_retry(&tk_core.seq, seq));

	return ktime_add_ns(base, nsecs);
}
EXPORT_SYMBOL_GPL(ktime_get);

u32 ktime_get_resolution_ns(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	u32 nsecs;

	WARN_ON(timekeeping_suspended);

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		nsecs = tk->tkr_mono.mult >> tk->tkr_mono.shift;
	} while (read_seqcount_retry(&tk_core.seq, seq));

	return nsecs;
}
EXPORT_SYMBOL_GPL(ktime_get_resolution_ns);

static ktime_t *offsets[TK_OFFS_MAX] = {
	[TK_OFFS_REAL]	= &tk_core.timekeeper.offs_real,
	[TK_OFFS_BOOT]	= &tk_core.timekeeper.offs_boot,
	[TK_OFFS_TAI]	= &tk_core.timekeeper.offs_tai,
};

ktime_t ktime_get_with_offset(enum tk_offsets offs)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	ktime_t base, *offset = offsets[offs];
	u64 nsecs;

	WARN_ON(timekeeping_suspended);

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		base = ktime_add(tk->tkr_mono.base, *offset);
		nsecs = timekeeping_get_ns(&tk->tkr_mono);

	} while (read_seqcount_retry(&tk_core.seq, seq));

	return ktime_add_ns(base, nsecs);

}
EXPORT_SYMBOL_GPL(ktime_get_with_offset);

ktime_t ktime_get_coarse_with_offset(enum tk_offsets offs)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	ktime_t base, *offset = offsets[offs];
	u64 nsecs;

	WARN_ON(timekeeping_suspended);

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		base = ktime_add(tk->tkr_mono.base, *offset);
		nsecs = tk->tkr_mono.xtime_nsec >> tk->tkr_mono.shift;

	} while (read_seqcount_retry(&tk_core.seq, seq));

	return ktime_add_ns(base, nsecs);
}
EXPORT_SYMBOL_GPL(ktime_get_coarse_with_offset);

/**
 * ktime_mono_to_any() - convert mononotic time to any other time
 * @tmono:	time to convert.
 * @offs:	which offset to use
 */
ktime_t ktime_mono_to_any(ktime_t tmono, enum tk_offsets offs)
{
	ktime_t *offset = offsets[offs];
	unsigned int seq;
	ktime_t tconv;

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		tconv = ktime_add(tmono, *offset);
	} while (read_seqcount_retry(&tk_core.seq, seq));

	return tconv;
}
EXPORT_SYMBOL_GPL(ktime_mono_to_any);

/**
 * ktime_get_raw - Returns the raw monotonic time in ktime_t format
 */
ktime_t ktime_get_raw(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	ktime_t base;
	u64 nsecs;

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		base = tk->tkr_raw.base;
		nsecs = timekeeping_get_ns(&tk->tkr_raw);

	} while (read_seqcount_retry(&tk_core.seq, seq));

	return ktime_add_ns(base, nsecs);
}
EXPORT_SYMBOL_GPL(ktime_get_raw);

/**
 * ktime_get_ts64 - get the monotonic clock in timespec64 format
 * @ts:		pointer to timespec variable
 *
 * The function calculates the monotonic clock from the realtime
 * clock and the wall_to_monotonic offset and stores the result
 * in normalized timespec64 format in the variable pointed to by @ts.
 */
void ktime_get_ts64(struct timespec64 *ts)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	struct timespec64 tomono;
	unsigned int seq;
	u64 nsec;

	WARN_ON(timekeeping_suspended);

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		ts->tv_sec = tk->xtime_sec;
		nsec = timekeeping_get_ns(&tk->tkr_mono);
		tomono = tk->wall_to_monotonic;

	} while (read_seqcount_retry(&tk_core.seq, seq));

	ts->tv_sec += tomono.tv_sec;
	ts->tv_nsec = 0;
	timespec64_add_ns(ts, nsec + tomono.tv_nsec);
}
EXPORT_SYMBOL_GPL(ktime_get_ts64);

/**
 * ktime_get_seconds - Get the seconds portion of CLOCK_MONOTONIC
 *
 * Returns the seconds portion of CLOCK_MONOTONIC with a single non
 * serialized read. tk->ktime_sec is of type 'unsigned long' so this
 * works on both 32 and 64 bit systems. On 32 bit systems the readout
 * covers ~136 years of uptime which should be enough to prevent
 * premature wrap arounds.
 */
time64_t ktime_get_seconds(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;

	WARN_ON(timekeeping_suspended);
	return tk->ktime_sec;
}
EXPORT_SYMBOL_GPL(ktime_get_seconds);

/**
 * ktime_get_real_seconds - Get the seconds portion of CLOCK_REALTIME
 *
 * Returns the wall clock seconds since 1970. This replaces the
 * get_seconds() interface which is not y2038 safe on 32bit systems.
 *
 * For 64bit systems the fast access to tk->xtime_sec is preserved. On
 * 32bit systems the access must be protected with the sequence
 * counter to provide "atomic" access to the 64bit tk->xtime_sec
 * value.
 */
time64_t ktime_get_real_seconds(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	time64_t seconds;
	unsigned int seq;

	if (IS_ENABLED(CONFIG_64BIT))
		return tk->xtime_sec;

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		seconds = tk->xtime_sec;

	} while (read_seqcount_retry(&tk_core.seq, seq));

	return seconds;
}
EXPORT_SYMBOL_GPL(ktime_get_real_seconds);

/**
 * __ktime_get_real_seconds - The same as ktime_get_real_seconds
 * but without the sequence counter protect. This internal function
 * is called just when timekeeping lock is already held.
 */
noinstr time64_t __ktime_get_real_seconds(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;

	return tk->xtime_sec;
}

/**
 * ktime_get_snapshot - snapshots the realtime/monotonic raw clocks with counter
 * @systime_snapshot:	pointer to struct receiving the system time snapshot
 */
void ktime_get_snapshot(struct system_time_snapshot *systime_snapshot)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	ktime_t base_raw;
	ktime_t base_real;
	u64 nsec_raw;
	u64 nsec_real;
	u64 now;

	WARN_ON_ONCE(timekeeping_suspended);

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		now = tk_clock_read(&tk->tkr_mono);
		systime_snapshot->cs_was_changed_seq = tk->cs_was_changed_seq;
		systime_snapshot->clock_was_set_seq = tk->clock_was_set_seq;
		base_real = ktime_add(tk->tkr_mono.base,
				      tk_core.timekeeper.offs_real);
		base_raw = tk->tkr_raw.base;
		nsec_real = timekeeping_cycles_to_ns(&tk->tkr_mono, now);
		nsec_raw  = timekeeping_cycles_to_ns(&tk->tkr_raw, now);
	} while (read_seqcount_retry(&tk_core.seq, seq));

	systime_snapshot->cycles = now;
	systime_snapshot->real = ktime_add_ns(base_real, nsec_real);
	systime_snapshot->raw = ktime_add_ns(base_raw, nsec_raw);
}
EXPORT_SYMBOL_GPL(ktime_get_snapshot);

/* Scale base by mult/div checking for overflow */
static int scale64_check_overflow(u64 mult, u64 div, u64 *base)
{
	u64 tmp, rem;

	tmp = div64_u64_rem(*base, div, &rem);

	if (((int)sizeof(u64)*8 - fls64(mult) < fls64(tmp)) ||
	    ((int)sizeof(u64)*8 - fls64(mult) < fls64(rem)))
		return -EOVERFLOW;
	tmp *= mult;

	rem = div64_u64(rem * mult, div);
	*base = tmp + rem;
	return 0;
}

/**
 * adjust_historical_crosststamp - adjust crosstimestamp previous to current interval
 * @history:			Snapshot representing start of history
 * @partial_history_cycles:	Cycle offset into history (fractional part)
 * @total_history_cycles:	Total history length in cycles
 * @discontinuity:		True indicates clock was set on history period
 * @ts:				Cross timestamp that should be adjusted using
 *	partial/total ratio
 *
 * Helper function used by get_device_system_crosststamp() to correct the
 * crosstimestamp corresponding to the start of the current interval to the
 * system counter value (timestamp point) provided by the driver. The
 * total_history_* quantities are the total history starting at the provided
 * reference point and ending at the start of the current interval. The cycle
 * count between the driver timestamp point and the start of the current
 * interval is partial_history_cycles.
 */
static int adjust_historical_crosststamp(struct system_time_snapshot *history,
					 u64 partial_history_cycles,
					 u64 total_history_cycles,
					 bool discontinuity,
					 struct system_device_crosststamp *ts)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	u64 corr_raw, corr_real;
	bool interp_forward;
	int ret;

	if (total_history_cycles == 0 || partial_history_cycles == 0)
		return 0;

	/* Interpolate shortest distance from beginning or end of history */
	interp_forward = partial_history_cycles > total_history_cycles / 2;
	partial_history_cycles = interp_forward ?
		total_history_cycles - partial_history_cycles :
		partial_history_cycles;

	/*
	 * Scale the monotonic raw time delta by:
	 *	partial_history_cycles / total_history_cycles
	 */
	corr_raw = (u64)ktime_to_ns(
		ktime_sub(ts->sys_monoraw, history->raw));
	ret = scale64_check_overflow(partial_history_cycles,
				     total_history_cycles, &corr_raw);
	if (ret)
		return ret;

	/*
	 * If there is a discontinuity in the history, scale monotonic raw
	 *	correction by:
	 *	mult(real)/mult(raw) yielding the realtime correction
	 * Otherwise, calculate the realtime correction similar to monotonic
	 *	raw calculation
	 */
	if (discontinuity) {
		corr_real = mul_u64_u32_div
			(corr_raw, tk->tkr_mono.mult, tk->tkr_raw.mult);
	} else {
		corr_real = (u64)ktime_to_ns(
			ktime_sub(ts->sys_realtime, history->real));
		ret = scale64_check_overflow(partial_history_cycles,
					     total_history_cycles, &corr_real);
		if (ret)
			return ret;
	}

	/* Fixup monotonic raw and real time time values */
	if (interp_forward) {
		ts->sys_monoraw = ktime_add_ns(history->raw, corr_raw);
		ts->sys_realtime = ktime_add_ns(history->real, corr_real);
	} else {
		ts->sys_monoraw = ktime_sub_ns(ts->sys_monoraw, corr_raw);
		ts->sys_realtime = ktime_sub_ns(ts->sys_realtime, corr_real);
	}

	return 0;
}

/*
 * cycle_between - true if test occurs chronologically between before and after
 */
static bool cycle_between(u64 before, u64 test, u64 after)
{
	if (test > before && test < after)
		return true;
	if (test < before && before > after)
		return true;
	return false;
}

/**
 * get_device_system_crosststamp - Synchronously capture system/device timestamp
 * @get_time_fn:	Callback to get simultaneous device time and
 *	system counter from the device driver
 * @ctx:		Context passed to get_time_fn()
 * @history_begin:	Historical reference point used to interpolate system
 *	time when counter provided by the driver is before the current interval
 * @xtstamp:		Receives simultaneously captured system and device time
 *
 * Reads a timestamp from a device and correlates it to system time
 */
int get_device_system_crosststamp(int (*get_time_fn)
				  (ktime_t *device_time,
				   struct system_counterval_t *sys_counterval,
				   void *ctx),
				  void *ctx,
				  struct system_time_snapshot *history_begin,
				  struct system_device_crosststamp *xtstamp)
{
	struct system_counterval_t system_counterval;
	struct timekeeper *tk = &tk_core.timekeeper;
	u64 cycles, now, interval_start;
	unsigned int clock_was_set_seq = 0;
	ktime_t base_real, base_raw;
	u64 nsec_real, nsec_raw;
	u8 cs_was_changed_seq;
	unsigned int seq;
	bool do_interp;
	int ret;

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		/*
		 * Try to synchronously capture device time and a system
		 * counter value calling back into the device driver
		 */
		ret = get_time_fn(&xtstamp->device, &system_counterval, ctx);
		if (ret)
			return ret;

		/*
		 * Verify that the clocksource associated with the captured
		 * system counter value is the same as the currently installed
		 * timekeeper clocksource
		 */
		if (tk->tkr_mono.clock != system_counterval.cs)
			return -ENODEV;
		cycles = system_counterval.cycles;

		/*
		 * Check whether the system counter value provided by the
		 * device driver is on the current timekeeping interval.
		 */
		now = tk_clock_read(&tk->tkr_mono);
		interval_start = tk->tkr_mono.cycle_last;
		if (!cycle_between(interval_start, cycles, now)) {
			clock_was_set_seq = tk->clock_was_set_seq;
			cs_was_changed_seq = tk->cs_was_changed_seq;
			cycles = interval_start;
			do_interp = true;
		} else {
			do_interp = false;
		}

		base_real = ktime_add(tk->tkr_mono.base,
				      tk_core.timekeeper.offs_real);
		base_raw = tk->tkr_raw.base;

		nsec_real = timekeeping_cycles_to_ns(&tk->tkr_mono,
						     system_counterval.cycles);
		nsec_raw = timekeeping_cycles_to_ns(&tk->tkr_raw,
						    system_counterval.cycles);
	} while (read_seqcount_retry(&tk_core.seq, seq));

	xtstamp->sys_realtime = ktime_add_ns(base_real, nsec_real);
	xtstamp->sys_monoraw = ktime_add_ns(base_raw, nsec_raw);

	/*
	 * Interpolate if necessary, adjusting back from the start of the
	 * current interval
	 */
	if (do_interp) {
		u64 partial_history_cycles, total_history_cycles;
		bool discontinuity;

		/*
		 * Check that the counter value occurs after the provided
		 * history reference and that the history doesn't cross a
		 * clocksource change
		 */
		if (!history_begin ||
		    !cycle_between(history_begin->cycles,
				   system_counterval.cycles, cycles) ||
		    history_begin->cs_was_changed_seq != cs_was_changed_seq)
			return -EINVAL;
		partial_history_cycles = cycles - system_counterval.cycles;
		total_history_cycles = cycles - history_begin->cycles;
		discontinuity =
			history_begin->clock_was_set_seq != clock_was_set_seq;

		ret = adjust_historical_crosststamp(history_begin,
						    partial_history_cycles,
						    total_history_cycles,
						    discontinuity, xtstamp);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(get_device_system_crosststamp);

/**
 * do_settimeofday64 - Sets the time of day.
 * @ts:     pointer to the timespec64 variable containing the new time
 *
 * Sets the time of day to the new time and update NTP and notify hrtimers
 */
int do_settimeofday64(const struct timespec64 *ts)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	struct timespec64 ts_delta, xt;
	unsigned long flags;
	int ret = 0;

	if (!timespec64_valid_settod(ts))
		return -EINVAL;

	raw_spin_lock_irqsave(&timekeeper_lock, flags);
	write_seqcount_begin(&tk_core.seq);

	timekeeping_forward_now(tk);

	xt = tk_xtime(tk);
	ts_delta = timespec64_sub(*ts, xt);

	if (timespec64_compare(&tk->wall_to_monotonic, &ts_delta) > 0) {
		ret = -EINVAL;
		goto out;
	}

	tk_set_wall_to_mono(tk, timespec64_sub(tk->wall_to_monotonic, ts_delta));

	tk_set_xtime(tk, ts);
#ifdef CONFIG_FFCLOCK
	ffclock_reset(tk, 0, ts, NULL);		// reset UTC to ts
#endif

out:
	timekeeping_update(tk, TK_CLEAR_NTP | TK_MIRROR | TK_CLOCK_WAS_SET);

	write_seqcount_end(&tk_core.seq);
	raw_spin_unlock_irqrestore(&timekeeper_lock, flags);

	/* signal hrtimers about time change */
	clock_was_set();

	if (!ret)
		audit_tk_injoffset(ts_delta);

	return ret;
}
EXPORT_SYMBOL(do_settimeofday64);

/**
 * timekeeping_inject_offset - Adds or subtracts from the current time.
 * @tv:		pointer to the timespec variable containing the offset
 *
 * Adds or subtracts an offset value from the current time.
 */
static int timekeeping_inject_offset(const struct timespec64 *ts)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned long flags;
	struct timespec64 tmp;
	int ret = 0;

	if (ts->tv_nsec < 0 || ts->tv_nsec >= NSEC_PER_SEC)
		return -EINVAL;

	raw_spin_lock_irqsave(&timekeeper_lock, flags);
	write_seqcount_begin(&tk_core.seq);

	timekeeping_forward_now(tk);

	/* Make sure the proposed value is valid */
	tmp = timespec64_add(tk_xtime(tk), *ts);
	if (timespec64_compare(&tk->wall_to_monotonic, ts) > 0 ||
	    !timespec64_valid_settod(&tmp)) {
		ret = -EINVAL;
		goto error;
	}

	tk_xtime_add(tk, ts);
	tk_set_wall_to_mono(tk, timespec64_sub(tk->wall_to_monotonic, *ts));

error: /* even if we error out, we forwarded the time, so call update */
	timekeeping_update(tk, TK_CLEAR_NTP | TK_MIRROR | TK_CLOCK_WAS_SET);

	write_seqcount_end(&tk_core.seq);
	raw_spin_unlock_irqrestore(&timekeeper_lock, flags);

	/* signal hrtimers about time change */
	clock_was_set();

	return ret;
}

/*
 * Indicates if there is an offset between the system clock and the hardware
 * clock/persistent clock/rtc.
 */
int persistent_clock_is_local;

/*
 * Adjust the time obtained from the CMOS to be UTC time instead of
 * local time.
 *
 * This is ugly, but preferable to the alternatives.  Otherwise we
 * would either need to write a program to do it in /etc/rc (and risk
 * confusion if the program gets run more than once; it would also be
 * hard to make the program warp the clock precisely n hours)  or
 * compile in the timezone information into the kernel.  Bad, bad....
 *
 *						- TYT, 1992-01-01
 *
 * The best thing to do is to keep the CMOS clock in universal time (UTC)
 * as real UNIX machines always do it. This avoids all headaches about
 * daylight saving times and warping kernel clocks.
 */
void timekeeping_warp_clock(void)
{
	if (sys_tz.tz_minuteswest != 0) {
		struct timespec64 adjust;

		persistent_clock_is_local = 1;
		adjust.tv_sec = sys_tz.tz_minuteswest * 60;
		adjust.tv_nsec = 0;
		timekeeping_inject_offset(&adjust);
	}
}

/**
 * __timekeeping_set_tai_offset - Sets the TAI offset from UTC and monotonic
 *
 */
static void __timekeeping_set_tai_offset(struct timekeeper *tk, s32 tai_offset)
{
	tk->tai_offset = tai_offset;
	tk->offs_tai = ktime_add(tk->offs_real, ktime_set(tai_offset, 0));
}

/**
 * change_clocksource - Swaps clocksources if a new one is available
 *
 * Accumulates current time interval and initializes new clocksource
 */
static int change_clocksource(void *data)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	struct clocksource *new, *old;
	unsigned long flags;

	new = (struct clocksource *) data;

	raw_spin_lock_irqsave(&timekeeper_lock, flags);
	write_seqcount_begin(&tk_core.seq);

	timekeeping_forward_now(tk);
	/*
	 * If the cs is in module, get a module reference. Succeeds
	 * for built-in code (owner == NULL) as well.
	 */
	if (try_module_get(new->owner)) {
		if (!new->enable || new->enable(new) == 0) {
			old = tk->tkr_mono.clock;
			tk_setup_internals(tk, new);
#ifdef CONFIG_FFCLOCK
			printk("FFclock processing cs change: old tick_ffcount = %llu = %#llX, with cs %s \n",
			(unsigned long long)fftimehands->tick_ffcount,
			(unsigned long long)fftimehands->tick_ffcount, old->name);
			ffclock_reset(tk, tk->tkr_raw.cycle_last, NULL, NULL); // cs change
#endif
			if (old->disable)
				old->disable(old);
			module_put(old->owner);
		} else {
			module_put(new->owner);
		}
	}
	timekeeping_update(tk, TK_CLEAR_NTP | TK_MIRROR | TK_CLOCK_WAS_SET);

	write_seqcount_end(&tk_core.seq);
	raw_spin_unlock_irqrestore(&timekeeper_lock, flags);

	return 0;
}

/**
 * timekeeping_notify - Install a new clock source
 * @clock:		pointer to the clock source
 *
 * This function is called from clocksource.c after a new, better clock
 * source has been registered. The caller holds the clocksource_mutex.
 */
int timekeeping_notify(struct clocksource *clock)
{
	struct timekeeper *tk = &tk_core.timekeeper;

	if (tk->tkr_mono.clock == clock)
		return 0;
	stop_machine(change_clocksource, clock, NULL);
	tick_clock_notify();
	return tk->tkr_mono.clock == clock ? 0 : -1;
}

/**
 * ktime_get_raw_ts64 - Returns the raw monotonic time in a timespec
 * @ts:		pointer to the timespec64 to be set
 *
 * Returns the raw monotonic time (completely un-modified by ntp)
 */
void ktime_get_raw_ts64(struct timespec64 *ts)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	u64 nsecs;

	do {
		seq = read_seqcount_begin(&tk_core.seq);
		ts->tv_sec = tk->raw_sec;
		nsecs = timekeeping_get_ns(&tk->tkr_raw);

	} while (read_seqcount_retry(&tk_core.seq, seq));

	ts->tv_nsec = 0;
	timespec64_add_ns(ts, nsecs);
}
EXPORT_SYMBOL(ktime_get_raw_ts64);


/**
 * timekeeping_valid_for_hres - Check if timekeeping is suitable for hres
 */
int timekeeping_valid_for_hres(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	int ret;

	do {
		seq = read_seqcount_begin(&tk_core.seq);

		ret = tk->tkr_mono.clock->flags & CLOCK_SOURCE_VALID_FOR_HRES;

	} while (read_seqcount_retry(&tk_core.seq, seq));

	return ret;
}

/**
 * timekeeping_max_deferment - Returns max time the clocksource can be deferred
 */
u64 timekeeping_max_deferment(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	u64 ret;

	do {
		seq = read_seqcount_begin(&tk_core.seq);

		ret = tk->tkr_mono.clock->max_idle_ns;

	} while (read_seqcount_retry(&tk_core.seq, seq));

	return ret;
}

/**
 * read_persistent_clock64 -  Return time from the persistent clock.
 *
 * Weak dummy function for arches that do not yet support it.
 * Reads the time from the battery backed persistent clock.
 * Returns a timespec with tv_sec=0 and tv_nsec=0 if unsupported.
 *
 *  XXX - Do be sure to remove it once all arches implement it.
 */
void __weak read_persistent_clock64(struct timespec64 *ts)
{
	ts->tv_sec = 0;
	ts->tv_nsec = 0;
}

/**
 * read_persistent_wall_and_boot_offset - Read persistent clock, and also offset
 *                                        from the boot.
 *
 * Weak dummy function for arches that do not yet support it.
 * wall_time	- current time as returned by persistent clock
 * boot_offset	- offset that is defined as wall_time - boot_time
 * The default function calculates offset based on the current value of
 * local_clock(). This way architectures that support sched_clock() but don't
 * support dedicated boot time clock will provide the best estimate of the
 * boot time.
 */
void __weak __init
read_persistent_wall_and_boot_offset(struct timespec64 *wall_time,
				     struct timespec64 *boot_offset)
{
	read_persistent_clock64(wall_time);
	*boot_offset = ns_to_timespec64(local_clock());
}

/*
 * Flag reflecting whether timekeeping_resume() has injected sleeptime.
 *
 * The flag starts of false and is only set when a suspend reaches
 * timekeeping_suspend(), timekeeping_resume() sets it to false when the
 * timekeeper clocksource is not stopping across suspend and has been
 * used to update sleep time. If the timekeeper clocksource has stopped
 * then the flag stays true and is used by the RTC resume code to decide
 * whether sleeptime must be injected and if so the flag gets false then.
 *
 * If a suspend fails before reaching timekeeping_resume() then the flag
 * stays false and prevents erroneous sleeptime injection.
 */
static bool suspend_timing_needed;

/* Flag for if there is a persistent clock on this platform */
static bool persistent_clock_exists;

/*
 * timekeeping_init - Initializes the clocksource and common timekeeping values
 */
void __init timekeeping_init(void)
{
	struct timespec64 wall_time, boot_offset, wall_to_mono;
	struct timekeeper *tk = &tk_core.timekeeper;
	struct clocksource *clock;
	unsigned long flags;

	read_persistent_wall_and_boot_offset(&wall_time, &boot_offset);
	if (timespec64_valid_settod(&wall_time) &&
	    timespec64_to_ns(&wall_time) > 0) {
		persistent_clock_exists = true;
	} else if (timespec64_to_ns(&wall_time) != 0) {
		pr_warn("Persistent clock returned invalid value");
		wall_time = (struct timespec64){0};
	}

	if (timespec64_compare(&wall_time, &boot_offset) < 0)
		boot_offset = (struct timespec64){0};

	/*
	 * We want set wall_to_mono, so the following is true:
	 * wall time + wall_to_mono = boot time
	 */
	wall_to_mono = timespec64_sub(boot_offset, wall_time);

	raw_spin_lock_irqsave(&timekeeper_lock, flags);
	write_seqcount_begin(&tk_core.seq);
	ntp_init();

	clock = clocksource_default_clock();
	if (clock->enable)
		clock->enable(clock);
	tk_setup_internals(tk, clock);

#ifdef CONFIG_FFCLOCK
	ffclock_init();		// initial setup of ffclock variables
	ffclock_reset(tk, 0, &wall_time, NULL);	// reset UTC to wall_time
#endif

	tk_set_xtime(tk, &wall_time);
	tk->raw_sec = 0;

	tk_set_wall_to_mono(tk, wall_to_mono);

	timekeeping_update(tk, TK_MIRROR | TK_CLOCK_WAS_SET);

	write_seqcount_end(&tk_core.seq);
	raw_spin_unlock_irqrestore(&timekeeper_lock, flags);
}

/* time in seconds when suspend began for persistent clock */
static struct timespec64 timekeeping_suspend_time;

/**
 * __timekeeping_inject_sleeptime - Internal function to add sleep interval
 * @delta: pointer to a timespec delta value
 *
 * Takes a timespec offset measuring a suspend interval and properly
 * adds the sleep offset to the timekeeping variables.
 */
static void __timekeeping_inject_sleeptime(struct timekeeper *tk,
					   const struct timespec64 *delta)
{
	if (!timespec64_valid_strict(delta)) {
		printk_deferred(KERN_WARNING
				"__timekeeping_inject_sleeptime: Invalid "
				"sleep delta value!\n");
		return;
	}
	tk_xtime_add(tk, delta);
	tk_set_wall_to_mono(tk, timespec64_sub(tk->wall_to_monotonic, *delta));
	tk_update_sleep_time(tk, timespec64_to_ktime(*delta));
	tk_debug_account_sleep_time(delta);
}

#if defined(CONFIG_PM_SLEEP) && defined(CONFIG_RTC_HCTOSYS_DEVICE)
/**
 * We have three kinds of time sources to use for sleep time
 * injection, the preference order is:
 * 1) non-stop clocksource
 * 2) persistent clock (ie: RTC accessible when irqs are off)
 * 3) RTC
 *
 * 1) and 2) are used by timekeeping, 3) by RTC subsystem.
 * If system has neither 1) nor 2), 3) will be used finally.
 *
 *
 * If timekeeping has injected sleeptime via either 1) or 2),
 * 3) becomes needless, so in this case we don't need to call
 * rtc_resume(), and this is what timekeeping_rtc_skipresume()
 * means.
 */
bool timekeeping_rtc_skipresume(void)
{
	return !suspend_timing_needed;
}

/**
 * 1) can be determined whether to use or not only when doing
 * timekeeping_resume() which is invoked after rtc_suspend(),
 * so we can't skip rtc_suspend() surely if system has 1).
 *
 * But if system has 2), 2) will definitely be used, so in this
 * case we don't need to call rtc_suspend(), and this is what
 * timekeeping_rtc_skipsuspend() means.
 */
bool timekeeping_rtc_skipsuspend(void)
{
	return persistent_clock_exists;
}

/**
 * timekeeping_inject_sleeptime64 - Adds suspend interval to timeekeeping values
 * @delta: pointer to a timespec64 delta value
 *
 * This hook is for architectures that cannot support read_persistent_clock64
 * because their RTC/persistent clock is only accessible when irqs are enabled.
 * and also don't have an effective nonstop clocksource.
 *
 * This function should only be called by rtc_resume(), and allows
 * a suspend offset to be injected into the timekeeping values.
 */
void timekeeping_inject_sleeptime64(const struct timespec64 *delta)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned long flags;

	raw_spin_lock_irqsave(&timekeeper_lock, flags);
	write_seqcount_begin(&tk_core.seq);

	suspend_timing_needed = false;

	timekeeping_forward_now(tk);

	__timekeeping_inject_sleeptime(tk, delta);

	timekeeping_update(tk, TK_CLEAR_NTP | TK_MIRROR | TK_CLOCK_WAS_SET);

	write_seqcount_end(&tk_core.seq);
	raw_spin_unlock_irqrestore(&timekeeper_lock, flags);

	/* signal hrtimers about time change */
	clock_was_set();
}
#endif

/**
 * timekeeping_resume - Resumes the generic timekeeping subsystem.
 */
void timekeeping_resume(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	struct clocksource *clock = tk->tkr_mono.clock;
	unsigned long flags;
	struct timespec64 ts_new, ts_delta;
	u64 cycle_now, nsec;
	bool inject_sleeptime = false;
#ifdef CONFIG_FFCLOCK
	u64 delta;
#endif

	read_persistent_clock64(&ts_new);

	clockevents_resume();
	clocksource_resume();

	raw_spin_lock_irqsave(&timekeeper_lock, flags);
	write_seqcount_begin(&tk_core.seq);

	/*
	 * After system resumes, we need to calculate the suspended time and
	 * compensate it for the OS time. There are 3 sources that could be
	 * used: Nonstop clocksource during suspend, persistent clock and rtc
	 * device.
	 *
	 * One specific platform may have 1 or 2 or all of them, and the
	 * preference will be:
	 *	suspend-nonstop clocksource -> persistent clock -> rtc
	 * The less preferred source will only be tried if there is no better
	 * usable source. The rtc part is handled separately in rtc core code.
	 */
	cycle_now = tk_clock_read(&tk->tkr_mono);
	nsec = clocksource_stop_suspend_timing(clock, cycle_now);
	if (nsec > 0) {
		ts_delta = ns_to_timespec64(nsec);
		inject_sleeptime = true;
	} else if (timespec64_compare(&ts_new, &timekeeping_suspend_time) > 0) {
		ts_delta = timespec64_sub(ts_new, timekeeping_suspend_time);
		inject_sleeptime = true;
	}

	if (inject_sleeptime) {
		suspend_timing_needed = false;
		__timekeeping_inject_sleeptime(tk, &ts_delta);
	}

	/* Re-base the last cycle value */
	tk->tkr_mono.cycle_last = cycle_now;
	tk->tkr_raw.cycle_last  = cycle_now;

	tk->ntp_error = 0;
	timekeeping_suspended = 0;
	timekeeping_update(tk, TK_MIRROR | TK_CLOCK_WAS_SET);
	write_seqcount_end(&tk_core.seq);
	raw_spin_unlock_irqrestore(&timekeeper_lock, flags);

	touch_softlockup_watchdog();

	tick_resume();
	hrtimers_resume();
}

int timekeeping_suspend(void)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned long flags;
	struct timespec64		delta, delta_delta;
	static struct timespec64	old_delta;
	struct clocksource *curr_clock;
	u64 cycle_now;

	read_persistent_clock64(&timekeeping_suspend_time);

	/*
	 * On some systems the persistent_clock can not be detected at
	 * timekeeping_init by its return value, so if we see a valid
	 * value returned, update the persistent_clock_exists flag.
	 */
	if (timekeeping_suspend_time.tv_sec || timekeeping_suspend_time.tv_nsec)
		persistent_clock_exists = true;

	suspend_timing_needed = true;

	raw_spin_lock_irqsave(&timekeeper_lock, flags);
	write_seqcount_begin(&tk_core.seq);
	timekeeping_forward_now(tk);
	timekeeping_suspended = 1;

	/*
	 * Since we've called forward_now, cycle_last stores the value
	 * just read from the current clocksource. Save this to potentially
	 * use in suspend timing.
	 */
	curr_clock = tk->tkr_mono.clock;
	cycle_now = tk->tkr_mono.cycle_last;
	clocksource_start_suspend_timing(curr_clock, cycle_now);

	if (persistent_clock_exists) {
		/*
		 * To avoid drift caused by repeated suspend/resumes,
		 * which each can add ~1 second drift error,
		 * try to compensate so the difference in system time
		 * and persistent_clock time stays close to constant.
		 */
		delta = timespec64_sub(tk_xtime(tk), timekeeping_suspend_time);
		delta_delta = timespec64_sub(delta, old_delta);
		if (abs(delta_delta.tv_sec) >= 2) {
			/*
			 * if delta_delta is too large, assume time correction
			 * has occurred and set old_delta to the current delta.
			 */
			old_delta = delta;
		} else {
			/* Otherwise try to adjust old_system to compensate */
			timekeeping_suspend_time =
				timespec64_add(timekeeping_suspend_time, delta_delta);
		}
	}

	timekeeping_update(tk, TK_MIRROR);
	halt_fast_timekeeper(tk);
	write_seqcount_end(&tk_core.seq);
	raw_spin_unlock_irqrestore(&timekeeper_lock, flags);

	tick_suspend();
	clocksource_suspend();
	clockevents_suspend();

	return 0;
}

/* sysfs resume/suspend bits for timekeeping */
static struct syscore_ops timekeeping_syscore_ops = {
	.resume		= timekeeping_resume,
	.suspend	= timekeeping_suspend,
};

static int __init timekeeping_init_ops(void)
{
	register_syscore_ops(&timekeeping_syscore_ops);
	return 0;
}
device_initcall(timekeeping_init_ops);

/*
 * Apply a multiplier adjustment to the timekeeper
 */
static __always_inline void timekeeping_apply_adjustment(struct timekeeper *tk,
							 s64 offset,
							 s32 mult_adj)
{
	s64 interval = tk->cycle_interval;

	if (mult_adj == 0) {
		return;
	} else if (mult_adj == -1) {
		interval = -interval;
		offset = -offset;
	} else if (mult_adj != 1) {
		interval *= mult_adj;
		offset *= mult_adj;
	}

	/*
	 * So the following can be confusing.
	 *
	 * To keep things simple, lets assume mult_adj == 1 for now.
	 *
	 * When mult_adj != 1, remember that the interval and offset values
	 * have been appropriately scaled so the math is the same.
	 *
	 * The basic idea here is that we're increasing the multiplier
	 * by one, this causes the xtime_interval to be incremented by
	 * one cycle_interval. This is because:
	 *	xtime_interval = cycle_interval * mult
	 * So if mult is being incremented by one:
	 *	xtime_interval = cycle_interval * (mult + 1)
	 * Its the same as:
	 *	xtime_interval = (cycle_interval * mult) + cycle_interval
	 * Which can be shortened to:
	 *	xtime_interval += cycle_interval
	 *
	 * So offset stores the non-accumulated cycles. Thus the current
	 * time (in shifted nanoseconds) is:
	 *	now = (offset * adj) + xtime_nsec
	 * Now, even though we're adjusting the clock frequency, we have
	 * to keep time consistent. In other words, we can't jump back
	 * in time, and we also want to avoid jumping forward in time.
	 *
	 * So given the same offset value, we need the time to be the same
	 * both before and after the freq adjustment.
	 *	now = (offset * adj_1) + xtime_nsec_1
	 *	now = (offset * adj_2) + xtime_nsec_2
	 * So:
	 *	(offset * adj_1) + xtime_nsec_1 =
	 *		(offset * adj_2) + xtime_nsec_2
	 * And we know:
	 *	adj_2 = adj_1 + 1
	 * So:
	 *	(offset * adj_1) + xtime_nsec_1 =
	 *		(offset * (adj_1+1)) + xtime_nsec_2
	 *	(offset * adj_1) + xtime_nsec_1 =
	 *		(offset * adj_1) + offset + xtime_nsec_2
	 * Canceling the sides:
	 *	xtime_nsec_1 = offset + xtime_nsec_2
	 * Which gives us:
	 *	xtime_nsec_2 = xtime_nsec_1 - offset
	 * Which simplfies to:
	 *	xtime_nsec -= offset
	 */
	if ((mult_adj > 0) && (tk->tkr_mono.mult + mult_adj < mult_adj)) {
		/* NTP adjustment caused clocksource mult overflow */
		WARN_ON_ONCE(1);
		return;
	}

	tk->tkr_mono.mult += mult_adj;
	tk->xtime_interval += interval;
	tk->tkr_mono.xtime_nsec -= offset;
}

/*
 * Adjust the timekeeper's multiplier to the correct frequency
 * and also to reduce the accumulated error value.
 */
static void timekeeping_adjust(struct timekeeper *tk, s64 offset)
{
	u32 mult;

	/*
	 * Determine the multiplier from the current NTP tick length.
	 * Avoid expensive division when the tick length doesn't change.
	 */
	if (likely(tk->ntp_tick == ntp_tick_length())) {
		mult = tk->tkr_mono.mult - tk->ntp_err_mult;
	} else {
		tk->ntp_tick = ntp_tick_length();
		mult = div64_u64((tk->ntp_tick >> tk->ntp_error_shift) -
				 tk->xtime_remainder, tk->cycle_interval);
	}

	/*
	 * If the clock is behind the NTP time, increase the multiplier by 1
	 * to catch up with it. If it's ahead and there was a remainder in the
	 * tick division, the clock will slow down. Otherwise it will stay
	 * ahead until the tick length changes to a non-divisible value.
	 */
	tk->ntp_err_mult = tk->ntp_error > 0 ? 1 : 0;
	mult += tk->ntp_err_mult;

	timekeeping_apply_adjustment(tk, offset, mult - tk->tkr_mono.mult);

	if (unlikely(tk->tkr_mono.clock->maxadj &&
		(abs(tk->tkr_mono.mult - tk->tkr_mono.clock->mult)
			> tk->tkr_mono.clock->maxadj))) {
		printk_once(KERN_WARNING
			"Adjusting %s more than 11%% (%ld vs %ld)\n",
			tk->tkr_mono.clock->name, (long)tk->tkr_mono.mult,
			(long)tk->tkr_mono.clock->mult + tk->tkr_mono.clock->maxadj);
	}

	/*
	 * It may be possible that when we entered this function, xtime_nsec
	 * was very small.  Further, if we're slightly speeding the clocksource
	 * in the code above, its possible the required corrective factor to
	 * xtime_nsec could cause it to underflow.
	 *
	 * Now, since we have already accumulated the second and the NTP
	 * subsystem has been notified via second_overflow(), we need to skip
	 * the next update.
	 */
	if (unlikely((s64)tk->tkr_mono.xtime_nsec < 0)) {
		tk->tkr_mono.xtime_nsec += (u64)NSEC_PER_SEC <<
							tk->tkr_mono.shift;
		tk->xtime_sec--;
		tk->skip_second_overflow = 1;
	}
}

/**
 * accumulate_nsecs_to_secs - Accumulates nsecs into secs
 *
 * Helper function that accumulates the nsecs greater than a second
 * from the xtime_nsec field to the xtime_secs field.
 * It also calls into the NTP code to handle leapsecond processing.
 *
 */
static inline unsigned int accumulate_nsecs_to_secs(struct timekeeper *tk)
{
	u64 nsecps = (u64)NSEC_PER_SEC << tk->tkr_mono.shift;
	unsigned int clock_set = 0;

	while (tk->tkr_mono.xtime_nsec >= nsecps) {
		int leap;

		tk->tkr_mono.xtime_nsec -= nsecps;
		tk->xtime_sec++;

		/*
		 * Skip NTP update if this second was accumulated before,
		 * i.e. xtime_nsec underflowed in timekeeping_adjust()
		 */
		if (unlikely(tk->skip_second_overflow)) {
			tk->skip_second_overflow = 0;
			continue;
		}

		/* Figure out if its a leap sec and apply if needed */
		leap = second_overflow(tk->xtime_sec);
		if (unlikely(leap)) {
			struct timespec64 ts;

			tk->xtime_sec += leap;

			ts.tv_sec = leap;
			ts.tv_nsec = 0;
			tk_set_wall_to_mono(tk,
				timespec64_sub(tk->wall_to_monotonic, ts));

			__timekeeping_set_tai_offset(tk, tk->tai_offset - leap);

			clock_set = TK_CLOCK_WAS_SET;
		}
	}
	return clock_set;
}

/**
 * logarithmic_accumulation - shifted accumulation of cycles
 *
 * This functions accumulates a shifted interval of cycles into
 * a shifted interval nanoseconds. Allows for O(log) accumulation
 * loop.
 *
 * Returns the unconsumed cycles.
 */
static u64 logarithmic_accumulation(struct timekeeper *tk, u64 offset,
				    u32 shift, unsigned int *clock_set)
{
	u64 interval = tk->cycle_interval << shift;
	u64 snsec_per_sec;

	/* If the offset is smaller than a shifted interval, do nothing */
	if (offset < interval)
		return offset;

	/* Accumulate one shifted interval */
	offset -= interval;
	tk->tkr_mono.cycle_last += interval;
	tk->tkr_raw.cycle_last  += interval;

	tk->tkr_mono.xtime_nsec += tk->xtime_interval << shift;
	*clock_set |= accumulate_nsecs_to_secs(tk);

#ifdef CONFIG_FFCLOCK
	ffclock_windup(interval,0); // advance interval cycles, process FFdata update
#endif

	/* Accumulate raw time */
	tk->tkr_raw.xtime_nsec += tk->raw_interval << shift;
	snsec_per_sec = (u64)NSEC_PER_SEC << tk->tkr_raw.shift;
	while (tk->tkr_raw.xtime_nsec >= snsec_per_sec) {
		tk->tkr_raw.xtime_nsec -= snsec_per_sec;
		tk->raw_sec++;
	}

	/* Accumulate error between NTP and clock interval */
	tk->ntp_error += tk->ntp_tick << shift;
	tk->ntp_error -= (tk->xtime_interval + tk->xtime_remainder) <<
						(tk->ntp_error_shift + shift);

	return offset;
}

/*
 * timekeeping_advance - Updates the timekeeper to the current time and
 * current NTP tick length
 */
static void timekeeping_advance(enum timekeeping_adv_mode mode)
{
	struct timekeeper *real_tk = &tk_core.timekeeper;
	struct timekeeper *tk = &shadow_timekeeper;
	u64 offset;
	int shift = 0, maxshift;
	unsigned int clock_set = 0;
	unsigned long flags;

	raw_spin_lock_irqsave(&timekeeper_lock, flags);

	/* Make sure we're fully resumed: */
	if (unlikely(timekeeping_suspended))
		goto out;

#ifdef CONFIG_ARCH_USES_GETTIMEOFFSET
	offset = real_tk->cycle_interval;

	if (mode != TK_ADV_TICK)
		goto out;
#else
	// fn prepares tk to replace real_tk at end. At entry, they are equal
	offset = clocksource_delta(tk_clock_read(&tk->tkr_mono),
				   tk->tkr_mono.cycle_last, tk->tkr_mono.mask);

	/* Check if there's really nothing to do */
	if (offset < real_tk->cycle_interval && mode == TK_ADV_TICK)
		goto out;
#endif

	/* Do some additional sanity checking */
	timekeeping_check_update(tk, offset);

	/*
	 * With NO_HZ we may have to accumulate many cycle_intervals
	 * (think "ticks") worth of time at once. To do this efficiently,
	 * we calculate the largest doubling multiple of cycle_intervals
	 * that is smaller than the offset.  We then accumulate that
	 * chunk in one go, and then try to consume the next smaller
	 * doubled multiple.
	 */
	shift = ilog2(offset) - ilog2(tk->cycle_interval);
	shift = max(0, shift);
	/* Bound shift to one less than what overflows tick_length */
	maxshift = (64 - (ilog2(ntp_tick_length())+1)) - 1;
	shift = min(shift, maxshift);
	while (offset >= tk->cycle_interval) {
		offset = logarithmic_accumulation(tk, offset, shift, &clock_set);
		if (offset < tk->cycle_interval<<shift)
			shift--;
	}

	/* Adjust the multiplier to correct NTP error */
	timekeeping_adjust(tk, offset);

	/*
	 * Finally, make sure that after the rounding
	 * xtime_nsec isn't larger than NSEC_PER_SEC
	 */
	clock_set |= accumulate_nsecs_to_secs(tk);

	write_seqcount_begin(&tk_core.seq);
	/*
	 * Update the real timekeeper.
	 *
	 * We could avoid this memcpy by switching pointers, but that
	 * requires changes to all other timekeeper usage sites as
	 * well, i.e. move the timekeeper pointer getter into the
	 * spinlocked/seqcount protected sections. And we trade this
	 * memcpy under the tk_core.seq against one before we start
	 * updating.
	 */
	/* BUG: weirdness, this fn updates the passed tk (which is shadow_tk), but at the end,
	 * overwrites shadow_tk directly with real_tk , thereby throwing away all updates
	 * Then when we come here, they are already the same, and so the entire fn fails to update */
	timekeeping_update(tk, clock_set);	// this fn does tk <-- real_tk
	memcpy(real_tk, tk, sizeof(*tk));	// already the same, does nth ??
	/* The memcpy must come last. Do not put anything here! */
	write_seqcount_end(&tk_core.seq);
out:
	raw_spin_unlock_irqrestore(&timekeeper_lock, flags);
	if (clock_set)
		/* Have to call _delayed version, since in irq context*/
		clock_was_set_delayed();
}

/**
 * update_wall_time - Uses the current clocksource to increment the wall time
 *
 */
void update_wall_time(void)
{
	timekeeping_advance(TK_ADV_TICK);
}

/**
 * getboottime64 - Return the real time of system boot.
 * @ts:		pointer to the timespec64 to be set
 *
 * Returns the wall-time of boot in a timespec64.
 *
 * This is based on the wall_to_monotonic offset and the total suspend
 * time. Calls to settimeofday will affect the value returned (which
 * basically means that however wrong your real time clock is at boot time,
 * you get the right time here).
 */
void getboottime64(struct timespec64 *ts)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	ktime_t t = ktime_sub(tk->offs_real, tk->offs_boot);

	*ts = ktime_to_timespec64(t);
}
EXPORT_SYMBOL_GPL(getboottime64);

void ktime_get_coarse_real_ts64(struct timespec64 *ts)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;

	do {
		seq = read_seqcount_begin(&tk_core.seq);

		*ts = tk_xtime(tk);
	} while (read_seqcount_retry(&tk_core.seq, seq));
}
EXPORT_SYMBOL(ktime_get_coarse_real_ts64);

void ktime_get_coarse_ts64(struct timespec64 *ts)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	struct timespec64 now, mono;
	unsigned int seq;

	do {
		seq = read_seqcount_begin(&tk_core.seq);

		now = tk_xtime(tk);
		mono = tk->wall_to_monotonic;
	} while (read_seqcount_retry(&tk_core.seq, seq));

	set_normalized_timespec64(ts, now.tv_sec + mono.tv_sec,
				now.tv_nsec + mono.tv_nsec);
}
EXPORT_SYMBOL(ktime_get_coarse_ts64);

/*
 * Must hold jiffies_lock
 */
void do_timer(unsigned long ticks)
{
	jiffies_64 += ticks;
	calc_global_load();
}

/**
 * ktime_get_update_offsets_now - hrtimer helper
 * @cwsseq:	pointer to check and store the clock was set sequence number
 * @offs_real:	pointer to storage for monotonic -> realtime offset
 * @offs_boot:	pointer to storage for monotonic -> boottime offset
 * @offs_tai:	pointer to storage for monotonic -> clock tai offset
 *
 * Returns current monotonic time and updates the offsets if the
 * sequence number in @cwsseq and timekeeper.clock_was_set_seq are
 * different.
 *
 * Called from hrtimer_interrupt() or retrigger_next_event()
 */
ktime_t ktime_get_update_offsets_now(unsigned int *cwsseq, ktime_t *offs_real,
				     ktime_t *offs_boot, ktime_t *offs_tai)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	unsigned int seq;
	ktime_t base;
	u64 nsecs;

	do {
		seq = read_seqcount_begin(&tk_core.seq);

		base = tk->tkr_mono.base;
		nsecs = timekeeping_get_ns(&tk->tkr_mono);
		base = ktime_add_ns(base, nsecs);

		if (*cwsseq != tk->clock_was_set_seq) {
			*cwsseq = tk->clock_was_set_seq;
			*offs_real = tk->offs_real;
			*offs_boot = tk->offs_boot;
			*offs_tai = tk->offs_tai;
		}

		/* Handle leapsecond insertion adjustments */
		if (unlikely(base >= tk->next_leap_ktime))
			*offs_real = ktime_sub(tk->offs_real, ktime_set(1, 0));

	} while (read_seqcount_retry(&tk_core.seq, seq));

	return base;
}

/**
 * timekeeping_validate_timex - Ensures the timex is ok for use in do_adjtimex
 */
static int timekeeping_validate_timex(const struct __kernel_timex *txc)
{
	if (txc->modes & ADJ_ADJTIME) {
		/* singleshot must not be used with any other mode bits */
		if (!(txc->modes & ADJ_OFFSET_SINGLESHOT))
			return -EINVAL;
		if (!(txc->modes & ADJ_OFFSET_READONLY) &&
		    !capable(CAP_SYS_TIME))
			return -EPERM;
	} else {
		/* In order to modify anything, you gotta be super-user! */
		if (txc->modes && !capable(CAP_SYS_TIME))
			return -EPERM;
		/*
		 * if the quartz is off by more than 10% then
		 * something is VERY wrong!
		 */
		if (txc->modes & ADJ_TICK &&
		    (txc->tick <  900000/USER_HZ ||
		     txc->tick > 1100000/USER_HZ))
			return -EINVAL;
	}

	if (txc->modes & ADJ_SETOFFSET) {
		/* In order to inject time, you gotta be super-user! */
		if (!capable(CAP_SYS_TIME))
			return -EPERM;

		/*
		 * Validate if a timespec/timeval used to inject a time
		 * offset is valid.  Offsets can be postive or negative, so
		 * we don't check tv_sec. The value of the timeval/timespec
		 * is the sum of its fields,but *NOTE*:
		 * The field tv_usec/tv_nsec must always be non-negative and
		 * we can't have more nanoseconds/microseconds than a second.
		 */
		if (txc->time.tv_usec < 0)
			return -EINVAL;

		if (txc->modes & ADJ_NANO) {
			if (txc->time.tv_usec >= NSEC_PER_SEC)
				return -EINVAL;
		} else {
			if (txc->time.tv_usec >= USEC_PER_SEC)
				return -EINVAL;
		}
	}

	/*
	 * Check for potential multiplication overflows that can
	 * only happen on 64-bit systems:
	 */
	if ((txc->modes & ADJ_FREQUENCY) && (BITS_PER_LONG == 64)) {
		if (LLONG_MIN / PPM_SCALE > txc->freq)
			return -EINVAL;
		if (LLONG_MAX / PPM_SCALE < txc->freq)
			return -EINVAL;
	}

	return 0;
}


/**
 * do_adjtimex() - Accessor function to NTP __do_adjtimex function
 */
int do_adjtimex(struct __kernel_timex *txc)
{
	struct timekeeper *tk = &tk_core.timekeeper;
	struct audit_ntp_data ad;
	unsigned long flags;
	struct timespec64 ts;
	s32 orig_tai, tai;
	int ret;

	/* Validate the data before disabling interrupts */
	ret = timekeeping_validate_timex(txc);
	if (ret)
		return ret;

	if (txc->modes & ADJ_SETOFFSET) {
		struct timespec64 delta;
		delta.tv_sec  = txc->time.tv_sec;
		delta.tv_nsec = txc->time.tv_usec;
		if (!(txc->modes & ADJ_NANO))
			delta.tv_nsec *= 1000;
		ret = timekeeping_inject_offset(&delta);
		if (ret)
			return ret;

		audit_tk_injoffset(delta);
	}

	audit_ntp_init(&ad);

	ktime_get_real_ts64(&ts);

	raw_spin_lock_irqsave(&timekeeper_lock, flags);
	write_seqcount_begin(&tk_core.seq);

	orig_tai = tai = tk->tai_offset;
	ret = __do_adjtimex(txc, &ts, &tai, &ad);

	if (tai != orig_tai) {
		__timekeeping_set_tai_offset(tk, tai);
		timekeeping_update(tk, TK_MIRROR | TK_CLOCK_WAS_SET);
	}
	tk_update_leap_state(tk);

	write_seqcount_end(&tk_core.seq);
	raw_spin_unlock_irqrestore(&timekeeper_lock, flags);

	audit_ntp_log(&ad);

	/* Update the multiplier immediately if frequency was set directly */
	if (txc->modes & (ADJ_FREQUENCY | ADJ_TICK))
		timekeeping_advance(TK_ADV_FREQ);

	if (tai != orig_tai)
		clock_was_set();

	ntp_notify_cmos_timer();

	return ret;
}

#ifdef CONFIG_NTP_PPS
/**
 * hardpps() - Accessor function to NTP __hardpps function
 */
void hardpps(const struct timespec64 *phase_ts, const struct timespec64 *raw_ts)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&timekeeper_lock, flags);
	write_seqcount_begin(&tk_core.seq);

	__hardpps(phase_ts, raw_ts);

	write_seqcount_end(&tk_core.seq);
	raw_spin_unlock_irqrestore(&timekeeper_lock, flags);
}
EXPORT_SYMBOL(hardpps);
#endif /* CONFIG_NTP_PPS */

/**
 * xtime_update() - advances the timekeeping infrastructure
 * @ticks:	number of ticks, that have elapsed since the last call.
 *
 * Must be called with interrupts disabled.
 */
void xtime_update(unsigned long ticks)
{
	raw_spin_lock(&jiffies_lock);
	write_seqcount_begin(&jiffies_seq);
	do_timer(ticks);
	write_seqcount_end(&jiffies_seq);
	raw_spin_unlock(&jiffies_lock);
	update_wall_time();
}

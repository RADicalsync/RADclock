/*
 * Copyright (C) 2006 The RADclock Project (see AUTHORS file)
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

#include <sys/types.h>
#include <sys/time.h>

#include <math.h>
#include <pcap.h>
#include <pthread.h>  // TODO remove once globaldata locking is fixed
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>


#include "../config.h"
#include "radclock.h"
/* The algo needs access to the global_data structure to update the user level clock */
#include "radclock-private.h"
#include "radclock_daemon.h"
#include "sync_history.h"
#include "proto_ntp.h"	// XXX ??
#include "sync_algo.h"
#include "create_stamp.h"
#include "rawdata.h"
#include "verbose.h"
#include "config_mgr.h"
#include "proto_ntp.h"
#include "misc.h"
#include "jdebug.h"



/* Function to copy a stamp. Note argument reversal. */
inline void copystamp(struct bidir_stamp *orig, struct bidir_stamp *copy)
{
	memcpy(copy, orig, sizeof(struct bidir_stamp));
}




/* =============================================================================
 * INITIALISATION ROUTINES
 * ===========================================================================*/

/* Set algo error thresholds based on the parameters of the counter model */
static void
set_err_thresholds(struct bidir_metaparam *metaparam, struct bidir_algostate *state)
{
	/* XXX Tuning history:
	 * original: 10*TSLIMIT = 150 mus
	 *    state->Eshift =  35*metaparam->TSLIMIT;  // 525 mus for Shouf Shouf?
	 */
	state->Eshift    = 10*metaparam->TSLIMIT;
	state->Ep        =  3*metaparam->TSLIMIT;
	state->Ep_qual   =    metaparam->RateErrBOUND/5;
	state->Ep_sanity =  3*metaparam->RateErrBOUND;

	/* XXX Tuning history:
	 *    state->Eplocal_qual = 4*metaparam->BestSKMrate;  // Original
	 *    state->Eplocal_qual = 4*2e-7;   // Big Hack during TON paper
	 *    state->Eplocal_qual = 40*1e-7;  // Tuning for Shouf Shouf tests ??
	 * but finally introduced a new parameter in the config file */
	state->Eplocal_qual   =   metaparam->plocal_quality;
	state->Eplocal_sanity = 3*metaparam->RateErrBOUND;

	/* XXX Tuning history:
	 *    *Eoffset = 6*metaparam->TSLIMIT;  // Original
	 * but finally introduced a new parameter in the config file */
	state->Eoffset = metaparam->offset_ratio * metaparam->TSLIMIT;

	/* XXX Tuning history:
	 * We should decouple Eoffset and Eoffset_qual ... conclusion of shouf shouf
	 * Added the effect of poll period as a first try (~ line 740)
	 * [UPDATE] - reverted ... see below
	 */
	state->Eoffset_qual        = 3*(state->Eoffset);
	state->Eoffset_sanity_min  = 100*metaparam->TSLIMIT;
	state->Eoffset_sanity_rate	= 20*metaparam->RateErrBOUND;
}


/* Derive algo windows measured in stamp-index units from timescales in [s]
 * The top level history window (width h_win) specifies the greatest memory depth
 * used by the overall algo, that is no stamp information further back than h_win
 * is used. All other windows are thus smaller than h_win, and in fact of h_win/2.
 * The value of h_win is hardwired as it is so large, the algos are very
 * insensitive to its value, so it does not need to be a configuration parameter.
 * Its goals are to
 *   (i)  limit memory complexity requirements of the code to be O(1)
 *   (ii) insure that the past is ultimately forgotten, this is essential to
 *        take into account unforseen circumstances, and ultimately oscillator ageing
 */
static void
set_algo_windows(struct bidir_metaparam *metaparam, struct bidir_algostate *p )
{
	index_t history_scale = 3600 * 24 * 7;  // time-scale [s] of h_win
	p->h_win = (index_t) ( history_scale / p->poll_period );

	/* shift detection. Ensure min of 100 samples (reference poll=16).
	 * Initially based on upjump thres sh_thres = (ceil(Eshift/phat) modelling time for drift to
	 * build to 150ms = 10*TSLIMIT=10*15mus for consistency, but now reset in parameters_calibration
	 * TODO: needs deep rethink */
	p->shift_win = MAX( (index_t) ceil( (10*metaparam->TSLIMIT/1e-7)/p->poll_period ), 100 );

	/* offset estimation, based on SKM scale (don't allow too small) */
	p->offset_win = (index_t) MAX( (metaparam->SKM_SCALE/p->poll_period), 2 );

	/* local period, not the right function! should be # samples and time based */
	p->plocal_win = (index_t) MAX( ceil(p->offset_win*5), 4);
   //	p->plocal_win = (index_t) MAX( ceil(p->offset_win*4), 4);  // Tuning for SIGCOMM
}


/* Adjust window values to avoid window mismatches generating unnecessary complexity
 * Key goal is to ensure warmup_win consistent with algo windows for easy
 * initialisation of main algo after warmup.
 * This version initialises warmup_win on the first stamp, otherwise, after a
 * parameter reload, it takes the new algo windows and again increases the warmup
 * period if necessary. It will:
 * - never decrease it in time to avoid problems, so will always be more stamps to serve
 * - always recommend keeping existing packets so warmup history is not lost
 * - ensure that h_win is large enough
 */
static void
adjust_warmup_win(index_t i, struct bidir_algostate *p, unsigned int plocal_winratio)
{
	index_t win;
	double WU_dur;

	verbose(VERB_CONTROL,"Adjusting Warmup window");
	
	/* Adjust warmup wrt shift, plocal, and offset windows */
	win = MAX(p->offset_win, MAX(p->shift_win, p->plocal_win + p->plocal_win/(plocal_winratio/2) ));

	if (i==0) {
		if ( win > p->warmup_win ) {    // simplify full algo a little
			verbose(VERB_CONTROL, "Warmup window smaller than algo windows, increasing "
			    "from %lu to %lu stamps", p->warmup_win, win);
			p->warmup_win = win;
		}
	} else { // Simply add on entire new Warmup using new param: code can't fail
		WU_dur = (double) (win * p->poll_period);    // new warmup remaining [s]
		p->warmup_win = (index_t) ceil(WU_dur / p->poll_period) + i;
		verbose(VERB_CONTROL,
		    "After adjustment, %4.1lf [sec] of warmup left to serve, or %lu stamps. "
		    "Warmup window now %lu", WU_dur, p->warmup_win - i, p->warmup_win);
	}

	/* Adjust history window wrt warmup and shift windows */
	/* Ensures warmup and shift windows < h_win/2 , so h_win can be ignored in warmup */
	if ( p->warmup_win + p->shift_win > p->h_win/2 ) {
		win = 3*( (p->warmup_win + p->shift_win) * 2 + 1 ); // 3x minimum, as short history bad
		verbose(VERB_CONTROL, "Warmup + shift window hits history half window,"
		    " increasing history window from %lu to %lu", p->h_win, win);
		p->h_win = win;
	}

	verbose(VERB_CONTROL,"Warmup Adjustment Complete");
}


/* Print parameter summary: physical, network, windows, thresholds, sanity */
static void
print_algo_parameters(struct bidir_metaparam *metaparam, struct bidir_algostate *state)
{
	verbose(VERB_CONTROL, "Machine Parameters:  TSLIMIT: %g, SKM_SCALE: %d, RateErrBOUND: %g, BestSKMrate: %g",
	    metaparam->TSLIMIT, (int)metaparam->SKM_SCALE, metaparam->RateErrBOUND, metaparam->BestSKMrate);

	// TODO: why is h_win a network param?  if anything h_scale is a machine+network param
	verbose(VERB_CONTROL, "Network Parameters:  poll_period: %u, h_win: %d ", state->poll_period, state->h_win);

	verbose(VERB_CONTROL, "Windows (in pkts):   warmup: %lu, history: %lu, shift: %lu "
	    "(thres = %4.0lf [mus]), plocal: %lu, offset: %lu (SKM scale is %u)",
	    state->warmup_win, state->h_win, state->shift_win, state->Eshift*1000000, state->plocal_win, state->offset_win,
	    (int) (metaparam->SKM_SCALE/state->poll_period) );

	verbose(VERB_CONTROL, "Error thresholds :   phat:  Ep %3.2lg [ms], Ep_qual %3.2lg [PPM], "
	    "plocal:  Eplocal_qual %3.2lg [PPM]", 1000*state->Ep, 1.e6*state->Ep_qual, 1.e6*state->Eplocal_qual);

	verbose(VERB_CONTROL, "                     offset:  Eoffset %3.1lg [ms], Eoffset_qual %3.1lg [ms]",
	    1000*state->Eoffset, 1000*state->Eoffset_qual);

	verbose(VERB_CONTROL, "Sanity Levels:       phat  %5.3lg, plocal  %5.3lg, offset: "
	    "absolute:  %5.3lg [ms], rate: %5.3lg [ms]/[sec] ( %5.3lg [ms]/[stamp])",
	    state->Ep_sanity, state->Eplocal_sanity, 1000*state->Eoffset_sanity_min,
	    1000*state->Eoffset_sanity_rate, 1000*state->Eoffset_sanity_rate*state->poll_period);
}


static void
update_state(struct bidir_metaparam *metaparam, struct bidir_algostate *state,
    unsigned int plocal_winratio, int poll_period)
{
	index_t si = state->stamp_i;  // convenience

	unsigned int stamp_sz;        // Stamp max history size
	unsigned int RTT_sz;          // RTT max history size
	unsigned int RTThat_sz;       // RTThat max history size
	unsigned int thnaive_sz;      // thnaive max history size
	index_t oldest_i;             // index of oldest item in a history, or -1 flag if empty
	vcounter_t *RTThat_tmp;       // RTThat value pointer

	verbose(VERB_CONTROL, "** update_state triggered on stamp %lu **", si);

	/* Initialize the error thresholds */
	set_err_thresholds(metaparam, state);

	/* Record change of poll period.
	 * Set poll_transition_th, poll_ratio and poll_changed_i for thetahat
	 * processing. poll_transition_th could be off by one depending on when NTP
	 * pkt rate changed, but not important.
	 */
	if (poll_period != state->poll_period) {
		state->poll_transition_th = state->offset_win;
		state->poll_ratio = (double)poll_period / (double)state->poll_period;
		state->poll_period = poll_period;
		state->poll_changed_i = si;
	}

	/* Set pkt-index algo windows.
	 * With possibly new metaparam and/or polling period
	 * These control the algo, independent of implementation.
	 */
	set_algo_windows(metaparam, state);

	/* Ensure warmup_win consistent with algo windows for easy
	 * initialisation of main algo after warmup 
	 */
	if (si < state->warmup_win)
		adjust_warmup_win(si, state, plocal_winratio);
	else {
		/* Re-init shift window from stamp i-1 back
		 * ensure don't go past 1st stamp, using history constraint (follows 
		 * that of RTThat already tracked). Window was reset, not 'slid'
		 * note: currently stamps [_end,i-1] in history, i not yet processed
		 */
		if ( (si-1) > (state->shift_win-1) )
			state->shift_end = (si-1) - (state->shift_win-1);
		else
			state->shift_end = 0;

		oldest_i = history_old(&state->RTT_hist);
		state->shift_end = MAX(state->shift_end, oldest_i);
		// TODO: check if this is stupidly complicated
		RTThat_tmp = history_find(&state->RTT_hist, history_min(&state->RTT_hist, state->shift_end, si-1) );
		state->RTThat_shift = *RTThat_tmp;
	}

	/* Set timeseries history array sizes.
	 * These detailed histories always lie within the history window of width h_win.
	 * If warmup is to be sacred, each must be larger than the current warmup_win 
	 * NOTE:  if set right, new histories will be big enough for future needs, 
	 * doesn't mean required data is in them after window resize!!
	 */
	if ( si < state->warmup_win ) {
		stamp_sz   = state->warmup_win;
		RTT_sz     = state->warmup_win;
		RTThat_sz  = state->warmup_win;
		thnaive_sz = state->warmup_win;
	} else {
		stamp_sz   = MAX(state->plocal_win + state->plocal_win/(plocal_winratio/2), state->offset_win);
		RTT_sz     = MAX(state->plocal_win + state->plocal_win/(plocal_winratio/2), MAX(state->offset_win, state->shift_win));
		RTThat_sz  = state->offset_win;    // need >= offset_win
		thnaive_sz = state->offset_win;    // need >= offset_win
	}


	/* Resize timeseries histories if needed.
	 * Currently stamps [_end,stamp_i-1] in history, stamp_i not yet processed.
	 */
	unsigned long int curr_i;    // store index of most recent stamp in history

	if ( state->stamp_hist.buffer_sz != stamp_sz )
	{
		oldest_i = history_old(&state->stamp_hist);
		if (oldest_i == -1)    // flags empty buffer
			curr_i = 0;
		else
			curr_i = oldest_i + state->stamp_hist.item_count - 1;
		verbose(VERB_CONTROL, "Resizing st_win history from %u to %u. Current stamp range is [%ld %lu]",
		    state->stamp_hist.buffer_sz, stamp_sz, oldest_i, curr_i);
		history_resize(&state->stamp_hist, stamp_sz);
		oldest_i = history_old(&state->stamp_hist);
		verbose(VERB_CONTROL, "Range on exit: [%ld %lu]", oldest_i, curr_i);
	}

	/* Resize RTT History */
	if ( state->RTT_hist.buffer_sz != RTT_sz )
	{
		oldest_i = history_old(&state->RTT_hist);
		if (oldest_i == -1)    // flags empty buffer
			curr_i = 0;
		else
			curr_i = oldest_i + state->RTT_hist.item_count - 1;
		verbose(VERB_CONTROL, "Resizing RTT_win history from %u to %u. Current stamp range is [%ld %lu]",
		    state->RTT_hist.buffer_sz, RTT_sz, oldest_i, curr_i);
		history_resize(&state->RTT_hist, RTT_sz);
		oldest_i = history_old(&state->RTT_hist);
		verbose(VERB_CONTROL, "Range on exit: [%ld %lu]", oldest_i, curr_i);
	}

	/* Resize RTThat History */
	if ( state->RTThat_hist.buffer_sz != RTThat_sz )
	{
		/* Unnecessary alternative since history_old already flags empty buffer */
		//		unsigned int count;           // number of items held in a history
		//		count = state->RTThat_hist.item_count;
		//		if (count == 0) {  // empty buffer
		//			oldest_i = -1;   // flags empty buffer
		//			curr_i = 0;      // incorrect but must remain unsigned
		//		} else {
		//			oldest_i = history_old(&state->RTThat_hist);
		//			curr_i = oldest_i + count - 1;
		//		}
		oldest_i = history_old(&state->RTThat_hist);
		if (oldest_i == -1)    // flags empty buffer  [** Bizzarely, (oldest_i < 0) test consistently failed!! ]
			curr_i = 0;
		else
			curr_i = oldest_i + state->RTThat_hist.item_count - 1;
		verbose(VERB_CONTROL, "Resizing RTThat_win history from %u to %u. Current stamp range is [%ld %lu]",
		    state->RTThat_hist.buffer_sz, RTThat_sz, oldest_i, curr_i);
		history_resize(&state->RTThat_hist, RTThat_sz);
		oldest_i = history_old(&state->RTThat_hist);
		verbose(VERB_CONTROL, "Range on exit: [%ld %lu]", oldest_i, curr_i);
	}

	/* Resize thnaive History */
	if ( state->thnaive_hist.buffer_sz != thnaive_sz ) {
		oldest_i = history_old(&state->thnaive_hist);
		if (oldest_i == -1)    // flags empty buffer
			curr_i = 0;
		else
			curr_i = oldest_i + state->thnaive_hist.item_count - 1;
		verbose(VERB_CONTROL, "Resizing thnaive_win history from %u to %u.  Current stamp range is [%ld %lu]",
		    state->thnaive_hist.buffer_sz, thnaive_sz, oldest_i, curr_i);
		history_resize(&state->thnaive_hist, thnaive_sz);
		oldest_i = history_old(&state->thnaive_hist);
		verbose(VERB_CONTROL, "Range on exit: [%ld %lu]", oldest_i, curr_i);
	}

	/* OWD and Asym resizing has the same parameters as RTT, no need for verbosity */
	history_resize(&state->Df_hist,      RTT_sz);
	history_resize(&state->Db_hist,      RTT_sz);
	history_resize(&state->Dfhat_hist,   RTT_sz);
	history_resize(&state->Dbhat_hist,   RTT_sz);
	history_resize(&state->Asymhat_hist, RTT_sz);

	print_algo_parameters(metaparam, state);

}




/* =============================================================================
 * ALGO ROUTINES
 * ===========================================================================*/

/* Manage the sliding forward of the top level history window when full, and
 * re-initialize any variables affected by the history loss.
 * At the end of warmup history window is initialized to [0 history_end] where
 * history_end = h_win-1 .  When this is full and more space is needed,
 * we extend the history window by sliding it forward by h_win/2.
 * Thus we forget half of the prior full history, and make room for the
 * next half window of stamps to come, starting with the current stamp.
 * Half way through a history window we begin preparing the re-initialization of
 * variables that will take over when the window is next slid, so that the
 * new values will only depend on stamp indices lying within the next window.
 */
static void
manage_historywin(struct bidir_algostate *state, struct bidir_stamp *stamp, vcounter_t RTT)
{
	index_t si = state->stamp_i;  // convenience

	/* Initialize:  middle of first history window [0 ... i ... history_end] */
	if ( si == state->h_win/2 ) {

		/* Reset half window estimate (up until now RTThat=next_RTThat)
		 * This simple initialization will be updated with the current stamp. */
		state->next_RTThat = state->stamp.Tf - state->stamp.Ta;

		/* Initialize on-line algo for new pstamp_i calculation [needs to be in
		 * surviving half of window!] */
		state->jsearch_win = state->warmup_win;
		state->jcount = 1;
		state->next_pstamp_i = si;
		state->next_pstamp_RTThat = state->RTThat;
		state->next_pstamp_perr = state->phat*((double)(RTT) - state->RTThat);
		copystamp(stamp, &state->next_pstamp);
		/* Now DelTb >= h_win/2,  become fussier */
		state->Ep_qual /= 10;

		/* Successor error bounds reinitialisation */
		ALGO_ERROR(state)->cumsum_hwin = 0;
		ALGO_ERROR(state)->sq_cumsum_hwin = 0;
		ALGO_ERROR(state)->nerror_hwin = 0;

		verbose(VERB_CONTROL, "Adjusting history window before normal "
		    "processing of stamp %lu. FIRST 1/2 window reached", si);
	}

	/* History window full, slide it, and replace variables with prepared values */
	if ( si == state->history_end ) {

		/* Advance window by h_win/2 so i is the first stamp in the new 2nd half */
		state->history_end += state->h_win/2;

		/* Replace online estimates with their successors (Upshifts must be built in) */
		state->Dfhat = state->next_Dfhat;
		state->Dbhat = state->next_Dbhat;
		state->RTThat = state->next_RTThat;

		/* Reset successor estimates (prior shifts irrelevant) using last raw values */
		double *D_ptr;
		D_ptr = history_find(&state->Df_hist, si - 1);
		state->next_Dfhat = *D_ptr;
		D_ptr = history_find(&state->Db_hist, si - 1);
		state->next_Dbhat = *D_ptr;
		state->next_RTThat = state->stamp.Tf - state->stamp.Ta;

		/* Take care of effects on phat algo
		* - begin using next_pstamp_i that has been precalculated in previous h_win/2
		* - reinitialise on-line algo for new next_pstamp_i calculation
		*   Record [index RTT RTThat stamp ]
		*/
		state->pstamp_i      = state->next_pstamp_i;
		state->pstamp_RTThat = state->next_pstamp_RTThat;
		state->pstamp_perr   = state->next_pstamp_perr;
		copystamp(&state->next_pstamp, &state->pstamp);
		state->jcount = 1;
		state->next_pstamp_i      = si;
		state->next_pstamp_RTThat = state->RTThat;
		state->next_pstamp_perr   = state->phat*((double)(RTT) - state->RTThat);
		copystamp(stamp, &state->next_pstamp);

		/* Successor error bounds being adopted, then reset */
		ALGO_ERROR(state)->cumsum    = ALGO_ERROR(state)->cumsum_hwin;
		ALGO_ERROR(state)->sq_cumsum = ALGO_ERROR(state)->sq_cumsum_hwin;
		ALGO_ERROR(state)->nerror    = ALGO_ERROR(state)->nerror_hwin;
		ALGO_ERROR(state)->cumsum_hwin    = 0;
		ALGO_ERROR(state)->sq_cumsum_hwin = 0;
		ALGO_ERROR(state)->nerror_hwin    = 0;

		verbose(VERB_CONTROL, "Total number of sanity events:  phat: %u, plocal: %u, Offset: %u ",
		    state->phat_sanity_count, state->plocal_sanity_count, state->offset_sanity_count);
		verbose(VERB_CONTROL, "Total number of low quality events:  Offset: %u ",
		    state->offset_quality_count);
		verbose(VERB_CONTROL, "Adjusting history window before normal processing of stamp %lu. "
		    "New pstamp_i = %lu ", si, state->pstamp_i);
	}

}


/* Performs initialisation of all sub-algos based on the first stamp (stamp0).
 * At this early stage phat cannot be estimated, and most state variables are
 * left at their null initialisation (0).
 * On entry, the only state variable already set is stamp_i.
 */
static void
init_algos(struct radclock_config *conf, struct bidir_algostate *state,
    struct bidir_stamp *stamp, struct radclock_data *rad_data, vcounter_t RTT,
    struct bidir_algooutput *output)
{
	index_t si = state->stamp_i;  // convenience

	/* Print the first timestamp tuple obtained */
	verbose(VERB_SYNC, "i=%lu: Beginning Warmup Phase. Stamp read check: %llu "
	    "%22.10Lf %22.10Lf %llu", si, stamp->Ta, stamp->Tb, stamp->Te, stamp->Tf);

	verbose(VERB_SYNC, "i=%lu: Assuming 1Ghz oscillator, 1st vcounter stamp is %5.3lf [days] "
	    "(%5.1lf [min]) since reset, RTT is %5.3lf [ms], SD %5.3Lf [mus]", si,
	    (double) stamp->Ta * 1e-9/3600/24, (double) stamp->Ta * 1e-9/60,
	    (double) (stamp->Tf - stamp->Ta) * 1e-9*1000, (stamp->Te - stamp->Tb) * 1e6);

	/* RTT */
	state->RTThat = RTT;    // already in history

	/* phat algo
	 * unavailable after only 1 stamp, use config value, rough guess beats zero!
	 * XXX: update comment, if reading first data from kernel timecounter /
	 * clocksource info. If the user put a default value in the config file,
	 * trust his choice. Otherwise, use kernel info from the first ffclock_getdata.
	 */
	if (conf->phat_init == DEFAULT_PHAT_INIT)    // ie if no user override
		// FIXME: kernel value not yet extracted, rad_data still blank, need to revisit all this
		// state->phat = rad_data->phat;
		state->phat = conf->phat_init;    // trust conf based value for now
	else
		state->phat = conf->phat_init;
	/* Initialize phat online warmup algo */
	state->wwidth = 1;

	/* OWD and Asymmetry
	 * RADclock not calibrated on first stamp, so base OWD on config setting of
	 * Asym plus rough initialized phat.  Errors in OWDs can be very large here,
	 * but will not impact the initial Asym value.
	 */
	state->base_asym = conf->asym_host + conf->time_server[state->sID].asym;
	double Df, Db, Asym;
	Asym = 0;    // make Asym related to ∆asym only, so initialize to 0
	Df = (RTT * state->phat + Asym) / 2;
	Db = Df - Asym;
	state->Dfhat = Df;
	state->Dbhat = Db;
	state->Asymhat = Asym;
	history_add(&state->Df_hist, si, &Df);
	history_add(&state->Db_hist, si, &Db);
	//history_add(&state->Asym_hist, si, &Asym);

	/* plocal algo */
	state->plocal = state->phat;

	/* thetahat algo  [ basic rAdclock definition ]
	 * K now determined, implies Ca(t) = stamp0->Tb  initially
	 * Subsequently, K only changes to null phat changes.  */
	state->K = stamp->Tb - (long double) (stamp->Ta * state->phat);
	verbose(VERB_SYNC, "i=%lu: After initialisation: (far,near) = (%lu,%lu), "
	    "phat = %.10lg, perr = %5.3lg, K = %7.4Lf",
	    si, 0, 0, state->phat, state->perr, state->K);

	/* Initialize thetahat online warmup algo */
	double th_naive = 0;  // no estimated correction, already doing best one can
	state->thetahat = th_naive;
	history_add(&state->thnaive_hist, si, &th_naive);
	copystamp(stamp, &state->thetastamp); // this is ok, but thetastamp setting needs detailed revision
	output->best_Tf = stamp->Tf;  // other local output fields are zero/unavailable

	/* pathpenalty measurement */
	output->pathpenalty = RTT * state->phat * conf->metaparam.relasym_bound_global;
}


/* Initializations for online minimum history and level_shift minima for
 * RTT, OWD and Asym. As these share the same shift window, they
 * share the same shift_end and _end indices.
 * Currently there are no actions for Asym as it is not based on a direct minima.
 */
void init_RTTOWDAsym_full(struct bidir_algostate *state, struct bidir_stamp *stamp)
{
	index_t si = state->stamp_i;  // convenience

	index_t end;          // index of last pkts in histories
	vcounter_t *RTT_tmp;  // vcount history pointer
	double *D;            // double history pointer


	/* Convenient to start tracking next_*hat now instead of in middle of the 1st
	 * history window, even if not used until then. */
	state->next_RTThat = state->RTThat;
	state->next_Dfhat = state->Dfhat;
	state->next_Dbhat = state->Dbhat;
	//state->next_Asym = state->Asym;

	/* Initialize shift_end (index of left boundary of shift window).
	 * Check to ensure stamps at shift_end are actually in history.
	 */
	if (si > state->shift_win)
		state->shift_end = si - (state->shift_win-1);
	else
		state->shift_end = 0;    // just in case window sizes incorrectly set
	end = history_old(&state->RTT_hist);
	end = MAX(state->shift_end, end);
	state->shift_end     = end;
	state->shift_end_OWD = end;    // assumes history storage always in sync

	/* Initialize the variables holding the minima over the shift window.
	 * Needed so that the min is in window, so history_min_slide() will work later
	 */
	D = history_find(&state->Df_hist, history_min_dbl(&state->Df_hist, end, si) );
	state->Dfhat_shift = *D;
	D = history_find(&state->Db_hist, history_min_dbl(&state->Db_hist, end, si) );
	state->Dbhat_shift = *D;

	RTT_tmp = history_find(&state->RTT_hist, history_min(&state->RTT_hist, end, si) );
	state->RTThat_shift = *RTT_tmp;

	/* RTThat history
	 * Not needed for RTT algos themselves, but maintained within them */

	/* RTThat_hist [Needed in thetahat_full]
	 *   was ignored in warmup. Now initialize for full by filling with current
	 *   RTThat: not a true history, but sensible for thetahat_full  */
	for (index_t j=0; j<=si; j++)
		history_add(&state->RTThat_hist, j, &state->RTThat);

	/* RTThat pre-jump history [Needed for update_pathpenalty_full] */
	state->prevRTThat = state->RTThat;


// // Alternative brute force instead of via history_add API
//	unsigned int RTThat_sz = state->RTThat_hist.buffer_sz;
//	for ( int j=0; j<RTThat_sz; j++ ) {  // ie [0,warmup_win-1]
//		RTT_tmp = history_find(&state->RTThat_hist, j);
//		*RTT_tmp = state->RTThat;
//	}
//	state->RTThat_hist.item_count = RTThat_sz;
//	//state->RTThat_hist.curr_i = si;  // member now obseleted

	verbose(VERB_CONTROL, "i=%lu: Initializing full RTTOWDAsym algos", si);
}



/* Initializations for normal phat algo
 * Need: reliable phat, estimate of its error, initial quality pstamp and associated data
 * Approximate error of current estimate from warmup algo, must be beaten before phat updated
 */
void init_phat_full(struct bidir_algostate *state, struct bidir_stamp *stamp)
{
	struct bidir_stamp *stamp_ptr;
	struct bidir_stamp *stamp_near;
	struct bidir_stamp *stamp_far;
	vcounter_t *RTT_far;
	vcounter_t *RTT_near;

	/* pstamp_i has been tracking the location of the smallest RTT during warmup,
	 * so we initialize pstamp to it. The recorded point error is zero.
	 * Error due to poor RTThat value at the time will be picked in the `global'
	 * error component baseerr in phat_full. To calculate baserr the RTThat
	 * associated to pstamp is taken to be the current (end-warmup) value, not
	 * the value at pstamp_i (not recorded) which may be unreliable.
	 */
	stamp_ptr = history_find(&state->stamp_hist, state->pstamp_i);
	copystamp(stamp_ptr, &state->pstamp);
	state->pstamp_perr = 0;
	state->pstamp_RTThat = state->RTThat;

	/* RTThat detection and last phat update may not correspond to the same
	 * stamp. Here we have reliable phat, but the corresponding point error may
	 * be outdated if RTThat is detected after last update. Reassess point 
	 * error with latest RTThat value.
	 */
	stamp_near = history_find(&state->stamp_hist, state->near_i);
	stamp_far  = history_find(&state->stamp_hist, state->far_i);
	RTT_near   = history_find(&state->RTT_hist, state->near_i);
	RTT_far    = history_find(&state->RTT_hist, state->far_i);
	state->perr	= ((double)(*RTT_far + *RTT_near) - 2*state->RTThat)
	    * state->phat / (stamp_near->Tb - stamp_far->Tb);

	/* Reinitialise sanity count at the end of warmup */
	state->phat_sanity_count = 0;
	verbose(VERB_CONTROL, "i=%lu: Initializing full phat algo, pstamp_i = %lu, perr = %6.3lg",
	    state->stamp_i, state->pstamp_i, state->perr);
}


/*
 * Initialization function for normal plocal algo. Resets wwidth as well as
 * finding near and far pkts * poll_period dependence:  via plocal_win
 */
static void
reset_plocal(struct bidir_algostate *state, unsigned int plocal_winratio, index_t i)
{
	/* History lookup window boundaries */
	index_t lhs;
	index_t rhs;

	/* XXX Tuning ... Accept too few packets in the window makes plocal varies
	 * a lot. Let's accept more packets to increase quality.
	 *   state->wwidth = MAX(1,plocal_win/plocal_winratio);
	 */
	/* not used again for phat */
	state->wwidth = MAX(4, state->plocal_win/plocal_winratio);
	lhs = i - state->plocal_win + 1 - state->wwidth/2;
	rhs = i - state->plocal_win + state->wwidth - state->wwidth/2;
	state->far_i  = history_min(&state->RTT_hist, lhs, rhs);

	lhs = i - state->wwidth + 1;
	rhs = i;
	state->near_i = history_min(&state->RTT_hist, lhs, rhs);
	verbose(VERB_CONTROL, "i=%lu: Initializing full plocal algo, wwidth= %lu, "
	    "(far_i,near_i) = (%lu,%lu)",
	    state->stamp_i, state->wwidth, state->far_i, state->near_i);
	state->plocal_problem = 0;
}


/* Initializations for normal plocal algo
 * [may not be enough history even in warmup, poll_period will only change timescale]
 */
void
init_plocal_full(struct bidir_algostate *state, struct bidir_stamp *stamp,
    unsigned int plocal_winratio)
{
	index_t si = state->stamp_i;  // convenience

	index_t st_end;    // indices of last pkts in stamp history
	index_t RTT_end;   // indices of last pkts in RTT history

	/*
	 * Index of stamp we require to be available before proceeding (different
	 * usage to shift_end etc!)
	 */
	state->plocal_end = si - state->plocal_win + 1 -
	    state->wwidth-state->wwidth / 2;
	st_end  = history_old(&state->stamp_hist);
	RTT_end = history_old(&state->RTT_hist);
	if (state->plocal_end >= MAX(state->poll_changed_i, MAX(st_end, RTT_end))) {
		/* if fully past poll transition and have history read
		 * resets wwidth as well as finding near and far pkts */
		reset_plocal(state, plocal_winratio, si);
	}
	else {
		/* record a problem, will have to restart when it resolves */
		state->plocal_problem = 1;
		verbose(VERB_CONTROL, "i=%lu:  plocal problem following parameter "
		    "changes (desired window first stamp %lu unavailable), "
		    "defaulting to phat while windows fill", si,
		    state->plocal_end);
	}
	state->plocal_sanity_count = 0;
}


/* Initialisation for normal thetahat algo */
void init_thetahat_full(struct bidir_algostate *state, struct bidir_stamp *stamp)
{
	state->offset_sanity_count = 0;
	state->offset_quality_count = 0;
	/* Initialize thetastamp tracking (ie stamp of last true update) */
	 // TODO: consider starting this facility immediately in warmup
	copystamp(stamp, &state->thetastamp);
	verbose(VERB_CONTROL, "i=%lu: Switching to full thetahat algo, RTThat_hist "
	    "set to RTThat=%llu, current est'd minimum error= %5.3lg [ms]",
	    state->stamp_i, state->RTThat, 1000*state->minET);
}


/* Adjust parameters based on nearby/far away server.
 * Is adhoc, and ugly in that can overwrite values which should have been set
 * correctly already, in particular those based off metaparams. In a way an
 * attempt to define, again, excel/good/poor idea, but for path, not the host.
 * But some things do depend on server behaviour and path characteristics,
 * makes sense to recalibrate what we can after warmup, eg if path sucks,
 * need to be less fussy with plocal.  The list of such will no doubt grow.
 * TODO: do this more systematically at some point.
 * TODO: check if this should be earlier, or split into calibrations according to sub-algos
 *       eg: currently is ONLY set here, so it not a modification over an init, this is it!
 */
void parameters_calibration( struct bidir_algostate *state)
{
	index_t si = state->stamp_i;  // convenience

	/* Near server = <3ms away */
	if ( state->RTThat < (3e-3 / state->phat) ) {
		verbose(VERB_CONTROL, "i=%lu: Detected close server based on minimum RTT", si);
		/* make RTThat_shift_thres constant to avoid possible phat dynamics */
		state->RTThat_shift_thres = (vcounter_t) ceil( state->Eshift/state->phat );
		/* Decoupling Eoffset and Eoffset_qual .. , for nearby servers, the
		 * looser quality has no (or very small?) effect */
		state->Eoffset_qual = 3 * state->Eoffset;
	} else {
		verbose(VERB_CONTROL, "i=%lu: Detected far away server based on minimum RTT", si);
		state->RTThat_shift_thres = (vcounter_t) ceil( 3*state->Eshift/state->phat );

		/* Decoupling Eoffset and Eoffset_qual .. , for far away servers, increase the number
		 * of points accepted for processing. The bigger the pool period the looser we
		 * have to be. Provide here a rough estimate of the increase of the window based
		 * on data observed with sugr-glider and shouf shouf plots
		 *     Eoffset_qual =  exp(log((double) poll_period)/2)/2 * 3 * Eoffset;
		 * [UPDATE]: actually doesn't work because the gaussian penalty function makes these 
		 * additional points be insignificant. Reverted back
		 */
		state->Eoffset_qual = 6 * state->Eoffset;
		state->Eplocal_qual = 2 * state->Eplocal_qual;  // adding path considerations to local physical ones
	}

	/* Select OWD upshift detection thresholds
	 * TODO: make these more intelligent, ultimately adaptive to path conditions
	 */
	state->Dfhat_shift_thres = state->RTThat_shift_thres * state->phat;
	state->Dbhat_shift_thres = state->Dfhat_shift_thres;

	verbose(VERB_CONTROL, "i=%lu:   Adjusted Eoffset_qual %3.1lg [ms] (Eoffset %3.1lg [ms])",
	    si, 1000*state->Eoffset_qual, 1000*state->Eoffset);
	verbose(VERB_CONTROL, "i=%lu:   Adjusted Eplocal_qual %4.2lg [PPM]",
	    si, 1000000*state->Eplocal_qual);

	verbose(VERB_CONTROL, "i=%lu:   Upward RTT shift detection activated, "
	    "threshold set at %llu [vcounter] (%4.0lf [mus])",
	    si, state->RTThat_shift_thres,
	    state->RTThat_shift_thres * state->phat*1000000);
}


/* Simple server delay (SD) stats.
 * Maintain counts in categories: (good, average, bad)
 */
void collect_stats_state(struct bidir_algostate *state, struct bidir_stamp *stamp)
{
	long double SD;
	SD = stamp->Te - stamp->Tb;

	if (SD < 50e-6)
		state->stats_sd[0]++;
	else if (SD < 200e-6)
		state->stats_sd[1]++;
	else
		state->stats_sd[2]++;
}


void print_stats_state(struct bidir_algostate *state)
{
	index_t si = state->stamp_i;  // convenience

	int total_sd;
	long basediff;

	/* Most of the variables needed for these stats are not used during warmup */
	if (si < state->warmup_win) {
		state->stats_sd[0] = 0;
		state->stats_sd[1] = 0;
		state->stats_sd[2] = 0;
		return;
	}

	if (si % (int)(6 * 3600 / state->poll_period))
		return;
	
	total_sd = state->stats_sd[0] + state->stats_sd[1] + state->stats_sd[2];

	verbose(VERB_CONTROL, "i=%lu: Server recent statistics:", si);
	verbose(VERB_CONTROL, "i=%lu:   Internal delay: %d%% < 50us, %d%% < 200us, %d%% > 200us",
	    si,
	    100 * state->stats_sd[0]/total_sd,
	    100 * state->stats_sd[1]/total_sd,
	    100 * state->stats_sd[2]/total_sd);

	verbose(VERB_CONTROL, "i=%lu:   Last stamp check: %llu %22.10Lf %22.10Lf %llu",
	    si, state->stamp.Ta, state->stamp.Tb, state->stamp.Te, state->stamp.Tf);

	verbose(VERB_SYNC, "i=%lu: Timekeeping summary:", si);
	verbose(VERB_SYNC, "i=%lu:   Period = %.10g, Period error = %.10g",
	    si, state->phat, state->perr);

	if (state->RTThat > state->pstamp_RTThat)
		basediff = state->RTThat - state->pstamp_RTThat;
	else
		basediff = state->pstamp_RTThat - state->RTThat;

	/* TODO check near and far what we want */
	verbose(VERB_SYNC, "i=%lu:   Tstamp pair = (%lu,%lu), base err = %.10g, "
	    "DelTb = %.3Lg [hrs]", si, state->far_i, state->near_i,
	    state->phat * basediff, (state->stamp.Tb - state->pstamp.Tb)/3600);

	verbose(VERB_SYNC, "i=%lu:   Thetahat = %5.3lf [ms], minET = %.3lf [ms], "
	    "RTThat = %.3lf [ms]", si,
	    1000 * state->thetahat,
	    1000 * state->minET,
	    1000 * state->phat * state->RTThat);

	/* Reset stats for this period */
	state->stats_sd[0] = 0;
	state->stats_sd[1] = 0;
	state->stats_sd[2] = 0;
}


/* =============================================================================
 * OWD BL Tracking
 * ============================================================================*/

/* Aims to detect a Downward level shift, a "D", by hunting over a recent hunt
 * window for a characteristic pattern of local raw drops (d), relative to an
 * current local level estimated over a preceding initialisation window:
 *                     [<---------------  initwin -------------->][<--Hwin-->]
 * relative hunt index                                            1         Hwin
 * external hunt index                                            i-Hwin+1  i
 * Drops are relative to the traditional Forward timeseries order, ie new
 * data appear on the right, at relative index Hwin, and a drop means a
 * lower value than older data on the left.
 * In this version initwin is not given directly, instead the refLevel measured
 * over it is passed in.
 *
 * The fn can be used for Up detection by traversing the data in reverse order.
 * However, there are inherent differences in its use in the two orders in a
 * online setting, due to the fact that in forward order the first drop found in
 * a cluster is the one we want and subsequent ones are to be avoided,
 * whereas in reverse order the opposite is true. Additional order-asymmetric
 * code is needed to manage this externally to this function.
 *
 * Internally, indices are relative to the Hwin window over j in [1,Hwin]
 * "order" denotes traversal direction of the window;
 *   1: traditional forward order, set si to  newest  data pt calib_i
 *   0:    reverse/backward order, set si to "oldest" data at calib_i-Uinitwin-Hwin+1
 *
 * ctrigger: threshold parameter for the number of raw drops below refLevel
 *           needed to trigger a detection
 */
int LocalDjumpDetect(history *data, index_t si, int order, double refLevel, int ctrigger, int Hwin)
{
	int j, firstdrop_ind;  // indices relative to Hwin
	int detect_ind = 0;    // Drop posn estimate (relative index) if D detected
	double *D;             // history pointer for data extraction

	/* Initialize drop hunt parameters */
	int rdcount = 0;                // number of raw drops found over Hwin so far
	long double hlevel = refLevel;  // min level over Hwin given drops found so far

	/* Scan huntwin zone until detect criteria met */
	for (j=1; j<=Hwin; j++) {

		/* Extract j-th relative data value within Hwin */
		if (order == 1)    // forward order
			D = history_find(data, si-Hwin+j);
		else               // backward order
			D = history_find(data, si+Hwin-j);
		/* Track any rdrops below reference */
		if ( *D < refLevel ) {
			if (rdcount == 0)
				firstdrop_ind = j;
			rdcount++;
			hlevel = MIN(hlevel,*D);

			// Detection
			if (rdcount == ctrigger) {
				detect_ind = firstdrop_ind;  // obvious but effective posn estimate
				if ( VERB_LEVEL>2 )
					verbose(VERB_DEBUG, "i=%lu: [%d] LocalDetect at j=%d (rdrop = %d), inferred Djump at j=%d"
					    " (delay = %d),  total drop of Delta = %4.3lf [ms] wrt %4.3lf",
					    si, order, j, rdcount, detect_ind, j-detect_ind, 1000*(refLevel-hlevel), 1000*refLevel);
			}
		}
	}

	return detect_ind;
}


/* Parameter setting and state variable initialization (beyond null init
 * performed by create_calib) for BL_track.
 *
 * History window depths are mainly calculated internally according to the needs
 * of BL_track, but min_datawin allows the caller to specify a minimum width for
 * data history.
 */
void
BL_track_init(struct BLtrack_state *state, int datawin)
{
	/* *** Detection parameters ****/
	state->ctrigger = 6;
	state->DHwin    = state->UHwin = 19;
	state->Dinitwin = state->Uinitwin = 96 - state->DHwin;
	state->Wwin = 1500;
	state->DWthres = state->UWthres = 150e-6;

	/* Ensure variables holding minima not trapped below true min */
	state->BL = state->local_ref = state->winmin = 10;

	/* Set initial local pauses to allow full Local search windows, essential! */
	state->Dpause = state->DHwin + state->Dinitwin - 1;  // win-1 ==> start at i=win
	state->Upause = state->UHwin + state->Uinitwin - 1;
	/* Set initial window pauses to allow full window, to avoid FPs */
	state->DWpause = state->Wwin - 1;
	state->UWpause = state->Wwin - 1;

	/* Create sufficient storage in needed per-stamp histories */
	datawin = MAX(datawin,state->Wwin);
	datawin = MAX(datawin,state->DHwin+state->Dinitwin);
	datawin = MAX(datawin,state->UHwin+state->Uinitwin);
	history_init(&state->data_hist, datawin+1, sizeof(double) ); // supports sliding
	history_init(&state->BL_hist, state->Wwin, sizeof(double) ); // no sliding
}


/* Process a single new data value, with index si, looking for D or U in
 * the given scalar timeseries obeying a piecewise constant BaseLine model.
 *
 * Two types of detectors are used to capture Down and Up change points:
 *  - local, a new approach whose core is implemented by LocalDjumpDetect()
 *  - sliding min-window based  (as in RADclock’s RTT Upshift tracking)
 * but applies each in both forward and backward ‘order’, resulting in four
 * detectors:  D, WD, U, WU, applied in this order, with (almost no)
 * coupling between them.
 * The result is a detector of change in either direction (Up/Down) with
 * good FP and FN performance, capable of detecting short duration holes
 * like fast packets.
 *
 * When a detection is made using D, U, or WU on a stamp si, the inferred
 * position (stamp) where the change actually occurred is Before si.
 * The argument detections: returns the indices of the inferred detection
 * point as a 4-vector [D WD U WU] detections, where a zero denotes no
 * detection.  Multiple detections scenarios (at the same or different
 * positions, detected at the same or different si) must be dealt with
 * by the calling program.
 */
int
BL_track(struct BLtrack_state *state, double *d, index_t si, index_t detections[])
{
	/* History conveniences */
	history *data = &state->data_hist;
	history *BL   = &state->BL_hist;

	double *D;    // double history pointer for data extraction
	int H, j;

	/* Local D variables */
	index_t new_Ddetect;
	/* Local U variables */
	double ref_level;
	index_t this_detect;
	/* Window D variables */
	/* Window U variables */
	index_t UWdetect;
	int swin;    // = serarchwin = initwin + Hwin

	/* Record new data point in history */
	history_add(data, si, d);

	/* Global Dshift adaptation */
	//newD = history_find(data, si);     // ptr to new data point
	state->BL = MIN(state->BL, *d);
	history_add(BL, si, &state->BL);

	/* Global-Local D detection (global since feed current BL as reference) */
	H = state->DHwin;
	if (state->Dpause)
		state->Dpause--;
	else {
		D = history_find(BL, si - H);  // ptr to global BL used as reference
		//ref_ind = si-DHwin;
		new_Ddetect = LocalDjumpDetect(data, si, 1, *D, state->ctrigger, H);
		if (new_Ddetect) {
			new_Ddetect = si - H + new_Ddetect;  // convert relative posn to absolute
			verbose(VERB_SYNC, "i=%lu: D Detection inferred Djump at i=%lu (delay = %ld)",
			    si, new_Ddetect, si-new_Ddetect);
			// Avoid guaranteed repeat detections on the same trigger point
			state->Dpause = H;
			// Try to avoid aftershock detections within tail of cluster
			state->Dpause += H;
			detections[0] = new_Ddetect;
		}
	}

	/* Window based D detection */
	if (state->DWpause)
		state->DWpause--;
	else  // prior to winmin update, pertains to window ending at si-1, as reqd here
		if (state->winmin > state->BL + state->DWthres) {
			verbose(VERB_SYNC, "i=%lu: DW Detection (delay = 0), of Delta = %4.3lf [ms]",
			    si, 1000*(state->winmin - state->BL));
			// Avoid possible after-shock detections due to multiple d`s over DWthres
			state->DWpause = state->DHwin;
			detections[1] = si;
		}


	/* Local U detection */
	H = state->UHwin;
	swin = H + state->Uinitwin;

	/* Update reference level over searchwin */
	if ( si < state->local_ref_end + state->Uinitwin )
		ref_level = MIN(state->local_ref,*d);  // window not full, don't slide
	else {
		ref_level = history_min_slide_value_dbl(data, state->local_ref, state->local_ref_end, si-1);
		state->local_ref_end++;
	}
	state->local_ref = ref_level;

	if (state->Upause)
		state->Upause--;
	else {
		this_detect = LocalDjumpDetect(data, si-swin+1, 0, ref_level, state->ctrigger, H);

		/* Buffer detections believed to be from the same cluster ( from aftershock end) */
		if (this_detect) {
			this_detect = si - (state->Uinitwin-1) - this_detect;  // convert relative posn to absolute
			if (state->bufferedU == 0) {  // new cluster
				state->first_lUdetect = this_detect;    // index of first detection (cluster tail end)
				state->last_lUdetect  = this_detect;    // index of last detection found (closest to U end)
				state->bufferedU = 1;                   // start counting detections in this cluster
				verbose(VERB_DEBUG, "i=%lu: BUFFering local U Detection #%d, inferred Ujump at i=%d ***",
				    si, state->bufferedU, this_detect+1);
			}
			// Skip repeat detections, only buffer and report distinct ones
			if (this_detect != state->last_lUdetect) {  // new detection
				state->last_lUdetect = this_detect;
				state->bufferedU += 1;
				verbose(VERB_DEBUG, "i=%lu: BUFFering local U Detection #%d, inferred Ujump at i=%d",
				    si, state->bufferedU, this_detect+1);
			}
		}

		/* Finalize detection for this cluster, using the last one found
		 * Based on when the last detection is beyond the expected cluster width
		 */
		if (state->bufferedU > 0 && si - state->first_lUdetect > swin + H) {
			state->last_lUdetect += 1;  // CP convention: is 1st pt After change in forward order
			verbose(VERB_SYNC, "i=%lu: U Detection, inferred Ujump at i=%d "
			    "(delay = %d) [%d buffered detections]", si, state->last_lUdetect,
			    si - state->last_lUdetect, state->bufferedU);
			// Reset for next cluster
			state->bufferedU = 0;

			/* Backdate global BL following U detect by imposing ref_level
			 * This may miss D's within Uwinitwin, and therefore be trapped low
			 * before them. */
//			for (j=state->last_lUdetect; j<=si; j++) {
//				D = history_find(BL, j);
//				*D = ref_level;
//			}
//			state->BL = ref_level;

			/* Backdate global BL following U detect by trimmed online
			 * WIP   This avoids immediate nbhd of U in case of -ve PE */
//			for (j=state->last_lUdetect+1; j<=last_lUdetect+5; j++) {
//				D = history_find(BL, j);
//				*D = ref_level;
//			}
			// Update to current stamp
//			for (j=state->last_lUdetect+6; j<=si; j++) {
//			}
//			state->BL = lastBL;

			/* Backdate BL history following U detect using online restart
			 * This approach will capture any D's over Uwinitwin and up to detection point */
			double *BLh;
			double lastBL;
			// Initialize to data at detection point
			D   = history_find(data, state->last_lUdetect);
			BLh = history_find(BL,   state->last_lUdetect);
			*BLh = *D;
			// Initialize to ref_level at detection point [match matlab version]
//			BLh = history_find(BL, state->last_lUdetect);
//			*BLh = ref_level;
			// Update to current stamp
			lastBL = *BLh;
			for (j=state->last_lUdetect+1; j<=si; j++) {
				D = history_find(data, j);
				BLh = history_find(BL, j);
				*BLh = MIN(lastBL,*D);
				lastBL = *BLh;
			}
			state->BL = lastBL;

			detections[2] = state->last_lUdetect;
		}
	}


	/* Update winmin over Wwin for UW, and next DW */
	if ( si < state->winmin_end + state->Wwin )
		ref_level = MIN(state->winmin,*d);  // window not full, don't slide
	else {
		ref_level = history_min_slide_value_dbl(data, state->winmin, state->winmin_end, si-1);
		state->winmin_end++;
	}
	state->winmin = ref_level;

	/* Window based U detection */
	if (state->UWpause)
		state->UWpause--;
	else
		if (state->winmin > state->BL + state->UWthres) {
			UWdetect = si - state->Wwin + 1;
			verbose(VERB_SYNC, "i=%lu: UW Detection inferred Ujump at i=%lu (delay = %d),"
			    " of Delta = %4.3lf [ms]",
			    si, UWdetect, si-UWdetect, 1000*(state->winmin - state->BL));
			detections[3] = UWdetect;

			/* Backdate global BL following U detect [traditional approach] */
			state->BL = ref_level;
			for (j=UWdetect; j<=si; j++) {
				D = history_find(BL, j);
				*D = ref_level;
			}
		}
}


/* Online asymmetry calibration algorithm, build on top of U/D detections
 * from BL_track in each of Df and Db separately. Detections from either
 * series are processed as they are found to form a set of candidate
 * Inferred Clear Zones (ICZs) designed to be free of dangerous regions
 * where CP detection is difficult, and/or congestion serious, as well as
 * free from position errors in CP detections by the use of a trimming
 * parameter. The minimum (Df,Db) baselines, and hence uA is calculated
 * and details of the best (longest) one is stored.
 * Initialization (beyond null init performed by create_calib) performed
 * internally. The algorithm represents the unification of two scalar
 * detections into a genuine 2-vector detection algorithm.
 */
void
asym_calibration(struct bidir_calibstate *state, struct bidir_stamp *bstamp,
    struct bidir_refstamp *rstamp, int finalize)
{

	/* Now processing calibration stamp i {0,1,2,...) */
	state->stamp_i++;   // initialized to -1 (no stamps processed)

	index_t si = state->stamp_i;  // convenience

	/* First stamp (stamp 0): perform basic state initialization beyond null */
	if (si == 0) {
		state->trim = 100;
		state->minlength = 1000;
		/* Ensure sliding window minima not trapped below true min */
		state->mDf = state->mDb = 10;
		/* BLtrack init, including history init (of data size at least trim) */
		BL_track_init(&state->Df_state, 1 + state->trim);
		BL_track_init(&state->Db_state, 1 + state->trim);
	}

	double Df, Db, uA;
	double *D;    // double history pointer for data extraction

	/* Calibration variables */
 	index_t newICP, ICPsafe;
	index_t L, R, N;
	index_t maxj = 0, minj = -1;    // since unsigned, -1 sets to maxint

//verbose(VERB_SYNC, "[%d] i=%lu: detections min max: [%lu %lu]",state->sID, si,minj,maxj);

	/* Extract new OWDs */
	Df =   bstamp->Tb - rstamp->Tout;
	Db = -(bstamp->Te - rstamp->Tin);
	state->Df = Df;
	state->Db = Db;

	/* Detect CPs in each dimension */
	index_t detectionsDf[4] = {0,0,0,0};
	index_t detectionsDb[4] = {0,0,0,0};

	//D = history_find(&state->Df_state.data_hist, si - 1);  // get previous point
	BL_track(&state->Df_state, &Df, si, detectionsDf);
	if (detectionsDf[0] || detectionsDf[1] || detectionsDf[2] || detectionsDf[3])
		verbose(LOG_NOTICE, "[%d] i=%lu: Df detections: [%lu %lu %lu %lu], (BL,Df) = (%4.4lf,%4.4lf) [ms]",
		    state->sID, si, detectionsDf[0], detectionsDf[1], detectionsDf[2], detectionsDf[3],
		    1000*state->Df_state.BL, 1000*Df);

	//D = history_find(&state->Db_state.data_hist, si - 1);  // get previous point
	BL_track(&state->Db_state, &Db, si, detectionsDb);
	if (detectionsDb[0] || detectionsDb[1] || detectionsDb[2] || detectionsDb[3])
		verbose(LOG_NOTICE, "[%d] i=%lu: Db detections: [%lu %lu %lu %lu], (BL,Db) = (%4.4lf,%4.4lf) [ms]",
		    state->sID, si, detectionsDb[0], detectionsDb[1], detectionsDb[2], detectionsDb[3],
		    1000*state->Db_state.BL, 1000*Db);

	/* Determine jump scenario */
	for (int k=0; k<4; k++) {
		if (detectionsDf[k] != 0) {  // ignore non-detections
			maxj = MAX(maxj,detectionsDf[k]);
			minj = MIN(minj,detectionsDf[k]);
		}
		if (detectionsDb[k] != 0) {  // ignore non-detections
			maxj = MAX(maxj,detectionsDb[k]);
			minj = MIN(minj,detectionsDb[k]);
		}
	}
	// Add fake CP if needed to trigger final ICZ aligned at end of data
	if ( finalize && maxj == 0) {
		maxj = si + 1 + state->trim;
		minj = maxj;
	}
//verbose(VERB_SYNC, "[%d] i=%lu: detections min max: [%lu %lu]",state->sID, si,minj,maxj);

	/* If detection(s), process the candidate ICZ */
	if (maxj > 0) {
		newICP = maxj;    // will be used as lastICP for the next ICZ
		ICPsafe = minj;   // used to avoid trouble for this ICZ only

		/* Record new ICZ if passes quality check based on length */
		if (state->lastICP > ICPsafe) {
			N = state->lastICP - ICPsafe;
			// in multiple detection case, All detections ignored
			verbose(LOG_NOTICE, "[%d] i=%lu: predates last detection by %lu, ignoring", state->sID, si, N);
		} else if (state->lastICP == ICPsafe) {
			verbose(LOG_NOTICE, "[%d] i=%lu: coincides with last detection, no new ICZ to process", state->sID, si);
		} else {
			N = ICPsafe - state->lastICP;        // length of [lastICP, ICPsafe-1]
			if (N > 2*state->trim + state->minlength) {
				// Candidate ICZ passed, collect needed stats
				L = state->lastICP + state->trim;    // start index of trimmed ICZ
				R = ICPsafe - 1 - state->trim;       //   end index of trimmed ICZ
				N = R - L + 1;                       //      length of trimmed ICZ
				uA = state->mDf - state->mDb;

				/* Record if best so far */
				if (N >= state->N_best) {
					state->N_best = N;
					state->Df_best = state->mDf;
					state->Db_best = state->mDb;
					state->uA_best = uA;
					state->posn_best = R;
				}

				verbose(LOG_NOTICE, "[%d] i=%lu: ** Viable ICZ found at [%lu,%lu], results from trimmed ICZ"
				    " over [%lu,%lu] (len %lu): [mDf,mDb] = [%4.3lf, %4.3lf],  asym = %4.3lf [ms]",
				    state->sID, si, state->lastICP, ICPsafe - 1, L, R, N,
				    1000*state->mDf, 1000*state->mDb, 1000*uA);
			} else
				verbose(LOG_NOTICE, "[%d] i=%lu:  Candidate raw ICZ at [%lu,%lu] too short "
				    " ((raw,trim) = (%lu,%d)), skipping",
				    state->sID, si, state->lastICP, ICPsafe-1, N, N - 2*state->trim);
		}

		/* Detection(s) processed, reset lastICP to safe value if needed */
		if (newICP > state->lastICP) {
			state->lastICP = newICP;
			state->mDf = 10;
			state->mDb = 10;
		}
	} else
		/* Track OWD minima over next potential (trimmed) ICZ */
		if ( si >= state->lastICP + 2*state->trim) {  // if trimmed length > 0
			D = history_find(&state->Df_state.data_hist, si - state->trim);
			state->mDf = MIN(state->mDf,*D);
			D = history_find(&state->Db_state.data_hist, si - state->trim);
			state->mDb = MIN(state->mDb,*D);
		}

	/* Calibration Exit report */
	if (finalize) {
		if (state->N_best > 0)
			verbose(LOG_NOTICE, "[%d] cal_i=%lu: Calibration success: ICZ at [%lu,%lu] selected"
			    " (len=%lu), [mDf,mDb] = [%4.lf, %4.3lf],  asym = %4.3lf [ms]",
			    state->sID, si, state->posn_best - state->N_best + 1, state->posn_best, state->N_best,
			    1000*state->Df_best , 1000*state->Db_best , 1000*state->uA_best);
		else
			verbose(LOG_NOTICE, "[%d] cal_i=%lu: Calibration failure: no acceptable ICZ found"
			    " respecting (trim,minlen) = (%d,%d)\n", state->sID, si, state->trim, state->minlength);
	}
}




/* =============================================================================
 * RTT / OWD-Asym
 * ============================================================================*/

/* The raw RTT is calculated and stored in RTT_hist prior to these fns */

void
process_RTT_warmup(struct bidir_algostate *state, vcounter_t RTT)
{
	index_t si = state->stamp_i;  // convenience

	/* Rough threshold during warmup TODO: set this up properly */
//	if (si == 10) {    // prior to this is zero I think
//		state->RTThat_shift_thres = (vcounter_t) ceil(state->Eshift/4/state->phat);
//		verbose(VERB_SYNC, "i=%lu: Restricting alerts for downward shifts exceeding %3.0lf [mus]",
//		    si, 1.e6 * state->Eshift/3);
//	}

	/* Downward Shifts.
	 * Informational only, reaction is automatic and already achieved.
	 * Shift location is simply the current stamp.
	 */
//	if ( state->RTThat > (RTT + state->RTThat_shift_thres) ) {
	if ( RTT < state->RTThat ) {    // flag all shifts, no matter how small
		verbose(VERB_SYNC, "i=%lu: Downward RTT shift of %5.1lf [mus], RTT now %4.1lf [ms]",
		    si, (state->RTThat - RTT) * state->phat * 1.e6, RTT * state->phat * 1.e3);
	}

	/* Record the new minimum
	 * Record corresponding index for full phat processing
	 * No storage in RTThat_hist in warmup */
	if (RTT < state->RTThat) {
		state->RTThat = RTT;
		state->pstamp_i = si;    // needed for phat
	}

	/* Upward Shifts.  Information only.
	 * This checks for detection over window of width shift_win prior to stamp i
	 * lastshift is the index of first known stamp after shift
	 */
//	if ( state->RTThat_shift > (state->RTThat + state->RTThat_shift_thres) ) {
//		lastshift = si - state->shift_win + 1;
//		verbose(VERB_SYNC, "Upward shift of %5.1lf [mus] triggered when i = %lu ! "
//		    "shift detected at stamp %lu",
//		    (state->RTThat_shift - state->RTThat)*state->phat*1.e6,
//		    si, lastshift);

}


/* OWD algorithm [ identical for each direction ]
 *  OWDwarmup period (<=filstart) Minimum filtering is skipped, since RADclock
 *  parameters are unreliable, and minimum estimates could be trapped at an
 *  incorrect low value, or temporarily be at a very large value (eg for stamp 1).
 *  To avoid these cases, all estimates are clipped to lie in [0,globalmin]
 *  (however the raw values are stored).
 *
 *  After OWDwarmup period
 *   - ignore non +ve values, otherwise global minimum over all stamps
 *   - no upward detection (not even informational, as has window complexities)
 *     A (silent) exception is made in case of an increase from an impossible 0
 *   - no storage into defined *hat_hist histories, as never used
 *   - all downshifts are flagged, no matter how small for simplicity (are rare).
 *
 * Asym algorithm
 *   - no native raw or filtered asym value, instead estimate based off OWD estimates
 *   - if clipping detected, the OWDs are unreliable, revert to asym = 0.  In
 *     this way asym can be protected without disturbing the OWD algorithm.
 *
 * Subtlety: effectively, the current clock is used to evaluate a shift in the new stamp,
 * which will immediately be factored into the clock subsequently in thetanaive,
 * but not necessarilty into the new theta until later.
 * TODO: consider if good to merge with RTT_warmup to form _Path_warmup
 */
void
process_OWDAsym_warmup(struct bidir_algostate *state, struct bidir_stamp *stamp)
{
	index_t si = state->stamp_i;  // convenience

	double Df, Db, Asym;   // `raw' values calculated for this stamp
	double TS;             // central server value, defines asym, ensures Df+Db=RTT
	int filstart = 20;     // don't use minimum filtering before this
	float globalmin = 0.2; // upper bound on OWD minimum [s] for any sane client<->server path
	int crazy = 0;

	/* Calculate raw OWD values for this stamp, and record */
	TS = (stamp->Tb + stamp->Te) / 2;
	Df = -((long double)state->phat * (long double)stamp->Ta + state->K - state->thetahat - TS);
	Db =   (long double)state->phat * (long double)stamp->Tf + state->K - state->thetahat - TS;
	history_add(&state->Df_hist, si, &Df);
	history_add(&state->Db_hist, si, &Db);

	/* Perform minimum filtering after OWDwarmup for each OWD separately */
	/* Update Df minimum */
	if ( si < filstart ) {
		state->Dfhat = MIN(globalmin,MAX(0,Df));
		verbose(VERB_SYNC, "i=%lu: Df startup: (Df,minDf) = (%5.2lf, %5.2lf) [ms]",
		    si, Df*1e3, state->Dfhat*1e3);
	} else
		if (state->Dfhat == 0 && Df > 0)	// allow Upjump immediately if Dfhat bogus
			state->Dfhat = Df;
		else
			if (Df < state->Dfhat && Df > 0) {
				verbose(VERB_SYNC, "i=%lu: Downward Df shift of %5.2lf [mus], %5.2lf --> %5.2lf [ms]",
				    si, (state->Dfhat - Df)*1.e6, state->Dfhat*1e3, Df*1e3);
				state->Dfhat = Df;
			}

	/* Update Db minimum */
	if ( si < filstart ) {
		state->Dbhat = MIN(globalmin,MAX(0,Db));
		verbose(VERB_SYNC, "i=%lu: Db startup: (Db,minDb) = (%5.2lf, %5.2lf) [ms]",
		    si, Db*1e3, state->Dbhat*1e3);
	} else
		if (state->Dbhat == 0 && Db > 0)	// allow Upjump immediately if Dbhat bogus
			state->Dbhat = Db;
		else
			if (Db < state->Dbhat && Db > 0) {
				verbose(VERB_SYNC, "i=%lu: Downward Db shift of %5.2lf [mus], %5.2lf --> %5.2lf [ms]",
				    si, (state->Dbhat - Db)*1.e6, state->Dbhat*1e3, Db*1e3);
				state->Dbhat = Db;
			}

	/* Flag an unreliable OWD in at least one direction
	 * Typically triggered on pkt 1 as Ca parameters still set as pkt 0 */
	if ( state->Dfhat == globalmin || state->Dfhat == 0 ||
		  state->Dbhat == globalmin || state->Dbhat == 0 ) {
		crazy = 1;
		verbose(VERB_SYNC, "i=%lu: OWD found to be crazy:  (minDf,minDb) = (%5.2lf, %5.2lf) [ms]",
		    si, state->Dfhat*1e3, state->Dbhat*1e3);
	}

	/* Update Asym */
	if (crazy)
		Asym = 0;
	else
		Asym = state->Dfhat - state->Dbhat;

	state->Asymhat = Asym;

}


/* Normal updating of OWD minima tracking, and OWD Upshift detection.
 * Resultant Asym estimate.
 * Windows are the same as for RTT
 * Non +ve values for estimates are ignored, but raw values stored.
 * TODO: consider adding status bits for OWD upshift detection
 */
void
process_OWDAsym_full(struct bidir_algostate *state, struct bidir_stamp *stamp)
{
	index_t si = state->stamp_i;  // convenience

	double Df, Db, Asym;    // `raw' values calculated for this stamp
	double TS;              // central server value, defines asym, ensures Df+Db=RTT

	index_t lastshift = 0;  // index of first stamp after last detected upward shift
//	index_t j;              // loop index, signed to avoid problem when j hits zero
//	double* D_ptr;          // points to a given entry in in D#hat_hist
//	double next_Df, last_Df;
//	double next_Db, last_Db;

	/* Calculate raw OWD values for this stamp, and record */
	TS = (stamp->Tb + stamp->Te) / 2;
	Df = -((long double)state->phat * (long double)stamp->Ta + state->K - state->thetahat - TS);
	Db =   (long double)state->phat * (long double)stamp->Tf + state->K - state->thetahat - TS;
	history_add(&state->Df_hist, si, &Df);
	history_add(&state->Db_hist, si, &Db);

	/* Perform minimum filtering for each OWD separately.
	 * All downshifts are flagged, no matter how small for simplicity (are rare).
	 * Filtered update ignored if value is negative.
	 * TODO: add informational Dshift detections
	 */
//	last_Df = state->Dfhat;
//	last_Db = state->Dbhat;
	if (Df < state->Dfhat && Df > 0) {
		verbose(VERB_SYNC, "i=%lu: Downward Df shift of %5.2lf [mus], minDf now %5.2lf [ms]",
		    si, (state->Dfhat - Df) * 1.e6, Df*1e3);
		state->Dfhat = Df;
	}
	if (Db < state->Dbhat && Db > 0) {
		verbose(VERB_SYNC, "i=%lu: Downward Db shift of %5.2lf [mus], minDb now %5.2lf [ms]",
		    si, (state->Dbhat - Db) * 1.e6, Db*1e3);
		state->Dbhat = Db;
	}

	/* Perform same algorithm for online next_*hat estimators */
	if (Df < state->next_Dfhat && Df > 0)
		state->next_Dfhat = Df;
	if (Db < state->next_Dbhat && Db > 0)
		state->next_Dbhat = Db;

	/* Update shift window minima and the window itself (shared over both OWDs) */
	if ( si < (state->shift_win-1) + state->shift_end_OWD ) {
		verbose(VERB_CONTROL, "In shift_win_OWD transition following window change, "
		    "[shift transition] windows are [%lu %lu] wide",
		    state->shift_win, si - state->shift_end_OWD + 1);
		state->Dfhat_shift =  MIN(state->Dfhat_shift,Df);
		state->Dbhat_shift =  MIN(state->Dbhat_shift,Db);
	} else {
		state->Dfhat_shift = history_min_slide_value_dbl(&state->Df_hist,
		    state->Dfhat_shift, state->shift_end_OWD, si-1);
		state->Dbhat_shift = history_min_slide_value_dbl(&state->Db_hist,
		    state->Dbhat_shift, state->shift_end_OWD, si-1);
		state->shift_end_OWD++;
	}


	/* Perform OWD Upshift detection [general comments as for RTT case]
	 * Currently D*hat_hist records the post-shift value if any, but does not
	 * overwrite history back to shift point, unlike RTT.
	 */
	/* Df */
	if ( state->Dfhat_shift > (state->Dfhat + state->Dfhat_shift_thres) ) {
		lastshift = si - state->shift_win + 1;
		verbose(VERB_SYNC, "i=%lu: Upward Df shift of %5.1lf [mus], detected at stamp %lu, minDf now %5.2lf [ms]",
		    si, (state->Dfhat_shift - state->Dfhat)*1.e6, lastshift, 1e3 * state->Dfhat_shift);

		/* Reset online estimators to match post-shift value */
		state->Dfhat 		= state->Dfhat_shift;
		state->next_Dfhat = state->Dfhat_shift;
		//verbose(VERB_SYNC, "i=%lu: Minima(Df,Db) = (%5.2lf, %5.2lf) [ms]", si,
		//    1e3*state->Dfhat, 1e3*state->Dbhat);
	}
	history_add(&state->Dfhat_hist, si, &state->Dfhat);

	/* Db */
	if ( state->Dbhat_shift > (state->Dbhat + state->Dbhat_shift_thres) ) {
		lastshift = si - state->shift_win + 1;
		verbose(VERB_SYNC, "i=%lu: Upward Db shift of %5.1lf [mus], detected at stamp %lu, minDb now %5.2lf [ms]",
		    si, (state->Dbhat_shift - state->Dbhat)*1.e6, lastshift, 1e3 * state->Dbhat_shift);

		/* Reset online estimators to match post-shift value */
		state->Dbhat      = state->Dbhat_shift;
		state->next_Dbhat = state->Dbhat_shift;
	}
	history_add(&state->Dbhat_hist, si, &state->Dbhat);

	/* Asym */
	Asym = state->Dfhat - state->Dbhat;  // raw value for this stamp
	state->Asymhat = Asym;  // is also final estimate, already a fn of minima filtering
	history_add(&state->Asymhat_hist, si, &Asym);

}


/* Full RTT updating algos.
 * Algos always simple via hierarchical organisation :
 *    - raw RTT for stamp i already stored in history
 *    - history window management transparently handled prior to this function
 *    - online RTT algos can therefore assume windows are fine
 *    - shift update code assumes online RTTs up to date
 *    - Upshift code assumes RTT and shift windows up to date
 * Note
 *  - RTThat always below or equal RTThat_shift since h_win/2 > shift_win
 *  - next_RTThat above or below RTThat_shift depending on position in history
 *  - RTThat_end tracks last element stored (held internally in RTThat history)
 */
void
process_RTT_full(struct bidir_algostate *state, struct radclock_data *rad_data, vcounter_t RTT)
{
	index_t si = state->stamp_i;  // convenience

	index_t lastshift = 0;   // last Upshift start
	index_t j;               // loop index, signed to avoid problem when j hits zero
	index_t jmin = 0;        // index that hits low end of loop
	vcounter_t* RTThat_ptr;  // points to a given RTThat in RTThat_hist
	vcounter_t next_RTT;

	/* Update on-line RTT minima:  RTThat and next_RTThat */
	state->prevRTThat = state->RTThat;  // immune to Upshift overwritting
	state->RTThat = MIN(state->RTThat, RTT);
	history_add(&state->RTThat_hist, si, &state->RTThat);
	state->next_RTThat = MIN(state->next_RTThat, RTT);

	/* Update of shift window minima RTThat_shift.
	 * If shift_win itself has changed due to an on-the-fly poll_period change,
	 * then stamps [0..i] may not yet be enough to fill it.
	 * In that case must also grow the window by keeping the lhs (ie shift_end)
	 * fixed and add stamp i on the right. Else window is full just as in the
	 * normal case (and min inside it thanks to reinit in update_state if applicable),
	 * so can update RTThat_shift via slide.
	 */
	if ( si < (state->shift_win-1) + state->shift_end ) {
		verbose(VERB_CONTROL, "In shift_win transition following window change, "
		    "[shift transition] windows are [%lu %lu] wide",
		    state->shift_win, si - state->shift_end + 1);
		state->RTThat_shift =  MIN(state->RTThat_shift,RTT);
	} else {
		state->RTThat_shift = history_min_slide_value(&state->RTT_hist,
		    state->RTThat_shift, state->shift_end, si-1);
		state->shift_end++;
	}

	/* Upward Shifts.
	 * This checks for detection over window of width shift_win prior to stamp i 
	 * At this point all minima have been updated so detection at stamp i visible
	 * Detection about reaction to RTThat_shift. RTThat_shift itself is simple,
	 * always just a sliding window.
	 * If detected, needed recalculations based on upshifted stamps over
	 *  [lastshift, i] = [i-shift_win+1 i]  By design, won't run into last history change.
	 */
	if ( state->RTThat_shift > (state->RTThat + state->RTThat_shift_thres) )
	{
		lastshift = si - state->shift_win + 1;
		verbose(VERB_SYNC, "i=%lu: Upward RTT shift of %5.1lf [mus], detected at "
		    "stamp %lu, minRTT now %4.1lf [ms]", si,
		    (state->RTThat_shift - state->RTThat)*state->phat*1.e6, lastshift,
		    state->RTThat_shift * state->phat*1.e3 );

		/* Recalc for online RTT estimators back to Upshift start */
		state->RTThat = state->RTThat_shift;
		state->next_RTThat = state->RTThat;  // ensure successor Upshift corrected

		/* Recalc necessary for phat
		 * - pstamp_i < lastshift by design, but next_pstamp_i may not be
		 * - phat used in error calc not the same as before, but that's ok
		 */
		if ( state->next_pstamp_i >= lastshift) {
			next_RTT = state->next_pstamp.Tf - state->next_pstamp.Ta;
			verbose(VERB_SYNC, "        Recalc necessary for RTT state recorded at"
			    " next_pstamp_i = %lu", state->next_pstamp_i);
			state->next_pstamp_perr = state->phat*((double)next_RTT - state->RTThat);
			state->next_pstamp_RTThat = state->RTThat;
		} else
			verbose(VERB_SYNC, "        Recalc not needed for RTT state recorded at"
			    " next_pstamp_i = %lu", state->next_pstamp_i);


		/* Recalc of RTThist necessary for offset
		 * Correct RTThat history back as far as necessary or possible
		 * (typically shift_win >> offset_win, so lastshift won't bite)
		 *  FIXME: not true in OCN context, and need big re-think anyway.
		 *   ** offset_win straddling an Upshift ==> mixing different asym values! need
		 *  to restrict index or enforce offset_win <= shift_win, but that is
		 *  inflexible wrt future improvement in upshift detection methods
		 *  TODO:  check if zero test necessary, in full algo jmin=0 shouldn't be possible
		 */
		if ( si > (state->offset_win-1) ) jmin = si - (state->offset_win-1); else jmin = 0;
		jmin = MAX(lastshift, jmin);

		for (j=si; j>=jmin; j--) {
			RTThat_ptr = history_find(&state->RTThat_hist, j);
			*RTThat_ptr = state->RTThat;
		}
		verbose(VERB_SYNC, "i=%lu: Recalc necessary for RTThat for %lu stamps back to i=%lu",
		    si, state->shift_win, lastshift);
		ADD_STATUS(rad_data, STARAD_RTT_UPSHIFT);
	} else
		DEL_STATUS(rad_data, STARAD_RTT_UPSHIFT);


	/* Downward Shifts.
	 * Informational only, reaction is automatic and already achieved.
	 * Shift location is simply the current stamp.
	 * TODO: establish a dedicated downward threshold parameter and setting code
	 */
	if ( state->prevRTThat > (state->RTThat + (state->RTThat_shift_thres)/4) ) {
		verbose(VERB_SYNC, "i=%lu: Downward RTT shift of %5.1lf [mus], RTT now %4.1lf [ms]",
		    si, (state->prevRTThat - state->RTThat)*state->phat * 1.e6, RTT * state->phat*1.e3);
	}
}







/* =============================================================================
 * PHAT ALGO 
 * ===========================================================================*/

/* Compute a naive phat given two stamps, with just a basic sanity check */
double compute_phat(struct bidir_algostate* state,
    struct bidir_stamp* far, struct bidir_stamp* near)
{
	long double DelTb, DelTe;  // Server time intervals between stamps j and i
	vcounter_t DelTa, DelTf;   // Counter intervals between stamps j and i
	double phat;               // Period estimate for current stamp
	double phat_b;             // Period estimate for current stamp (backward dir)
	double phat_f;             // Period estimate for current stamp (forward dir)

	DelTa = near->Ta - far->Ta;
	DelTb = near->Tb - far->Tb;
	DelTe = near->Te - far->Te;
	DelTf = near->Tf - far->Tf;

	/* Check for crazy values, and NaN cases induced by DelTa or DelTf equal
	 * zero Log a major error */
	if ( ( DelTa <= 0 ) || ( DelTb <= 0 ) || (DelTe <= 0 ) || (DelTf <= 0) ) {
		verbose(LOG_ERR, "i=%lu we picked up the same i and j stamp. "
		    "Contact developer.", state->stamp_i);
		return 0;
	}

	/* Use naive estimates from chosen stamps {i,j} */
	phat_f = (double) (DelTb / DelTa);    // forward  (OUTGOING, sender)
	phat_b = (double) (DelTe / DelTf);    // backward (INCOMING, receiver)
	phat = (phat_f + phat_b) / 2;

	return phat;
}


int process_phat_warmup (struct bidir_algostate* state, vcounter_t RTT,
    unsigned int warmup_winratio)
{
	index_t si = state->stamp_i;  // convenience

	vcounter_t *RTT_tmp;
	vcounter_t *RTT_far;
	vcounter_t *RTT_near;
	struct bidir_stamp *stamp_near;
	struct bidir_stamp *stamp_far;
	long double DelTb;    // Server time intervals between stamps j and i
	double phat;          // Period estimate for current stamp

	long near_i = 0;
	long far_i = 0;

	near_i = state->near_i;
	far_i = state->far_i;

	/*
	 * Select indices for new estimate. Indices taken from a far window 
	 * (stamps in [0 wwidth-1]) and a near window (stamps in [i-wwidth+1 i])
	 * Still works if poll_period changed, but rate increase of end windows can
	 * be different
	 * if stamp index not yet a multiple of warmup_winratio
	 *   find near_i by sliding along one on RHS
	 * else
	 *   increase near and far windows by 1, find index of new min RTT in
	 *   both, increase window width
	*/

	if ( si%warmup_winratio ) {
		state->near_i = history_min_slide(&state->RTT_hist, state->near_i,
		    si - state->wwidth, si - 1);
	}
	else {
		RTT_tmp  = history_find(&state->RTT_hist, state->wwidth);
		RTT_near = history_find(&state->RTT_hist, state->near_i);
		RTT_far  = history_find(&state->RTT_hist, state->far_i);
		if ( *RTT_tmp < *RTT_far )
			state->far_i = state->wwidth;
		if ( RTT < *RTT_near )
			state->near_i = si;
		state->wwidth++;
	}

	/* Compute time intervals between NTP timestamps of selected stamps */
	stamp_near = history_find(&state->stamp_hist, state->near_i);
	stamp_far  = history_find(&state->stamp_hist, state->far_i);

	phat = compute_phat(state, stamp_far, stamp_near);
	/* Emergency sanity check rejecting update TODO: check if needed */
	if (phat == 0) {
		/* Something bad happen, most likely, we have a bug. The algo may
		 * recover from this, so do not update and keep goin. */
		return 1;
	}

	/* Clock correction
	 * correct K to keep C(t) continuous at time of last stamp // TODO: add comment on why this is Laststamp
	 */
//	if ( state->phat != phat ) {
	if ( (near_i != state->near_i) || (far_i != state->far_i) )
	{
		state->K += state->stamp.Ta * (long double) (state->phat - phat);
		verbose(VERB_SYNC, "i=%lu: phat update (far,near) = (%lu,%lu), "
		    "phat = %.10g, rel diff = %.10g, perr = %.10g, K = %7.4Lf",
		    si, state->far_i, state->near_i,
		    phat, (phat - state->phat)/phat, state->perr, state->K);
		state->phat = phat;
		RTT_far  = history_find(&state->RTT_hist, state->far_i);
		RTT_near = history_find(&state->RTT_hist, state->near_i);
		DelTb = stamp_near->Tb - stamp_far->Tb;
		//state->perr = state->phat * (double)((*RTT_far - state->RTThat) + (*RTT_near - state->RTThat)) / DelTb;
		state->perr = state->phat * ((double)(*RTT_far + *RTT_near) - 2*state->RTThat) / DelTb;
	}
	return 0;
}


/* On-line update of next_pstamp, the value that will replace pstamp in the phat
 * full algo when the history window slides. The point of the online updating is
 * that pstamp will be far in the past, and so will not be accessible via the
 * shorter term timeseries histories maintained for other purposes.
 * This function only updates a Future candidate for pstamp, the phat algo uses
 * the Current value. Thus this algorithn belongs as much to history management
 * as to the full phat algo per-se.
 *
 * The update only occurs during the "jsearch window" attached to start of the
 * current second half of the history window, to ensure that the stamp will be
 * far in the past wrt the current stamp i. This window is initialized to 1
 * (thus no searching) at the end of warmup as a good initial value of
 * pstamp is already available from warmup. It is opened up to its normal
 * value at the end of the first h_win/2 (see manage_historywin).
 * Thus, whenever the history window is full, a new of pstamp will be available
 * that will be roughly h_win/2 in the past.
 *
 * During the search win, the best quality stamp is updated, and we record its
 * [index RTThat point-error stamp] . The RTT of the stamp is also required but
 * can be calculated from the stored stamp.
 */
void update_next_pstamp(struct bidir_algostate* state, struct bidir_stamp* stamp, vcounter_t RTT)
{
	vcounter_t next_pstamp_RTT;

	if ( state->jcount <= state->jsearch_win ) {
		state->jcount++;
		next_pstamp_RTT = state->next_pstamp.Tf - state->next_pstamp.Ta;
		if ( RTT < next_pstamp_RTT ) {
			state->next_pstamp_i      = state->stamp_i;
			state->next_pstamp_RTThat = state->RTThat;
			state->next_pstamp_perr   = state->phat * (double)(RTT - state->RTThat);
			copystamp(stamp, &state->next_pstamp);
		}
	}
}


/* The full phat algo is based on selecting two stamps [pstamp, recent] that are
 * of high quality as well as very far apart (pstamp_i << recent_i). Here pstamp
 * is selected as the best (according to its local point error) in a wide search
 * window anchored at the centre of the current history window. It is fixed
 * until the next pstamp update. Since pstamp is far in the past, it is found
 * and recorded via an on-line algorithm, not via a (very long) timeseries
 * history (see update_next_pstamp). The large gap between the stamps reduces
 * errors in point error estimates in a very robust way.
 *
 * "Recent" is based on testing, for each new stamp i, whether total quality of an
 * estimate arising from the (pstamp, i) pair is sufficiently excellent. If so,
 * phat is updated (there is no need as it turns out to record the corresponding i).
 * Total quality includes the point qualities of each stamp, the distance between
 * them, and the quality of the minRTT baseline.
 *
 * The return value `ret' allows a signal to be sent to plocal algo that phat
 * sanity triggered on This stamp.
 */
int process_phat_full(struct bidir_metaparam *metaparam, struct bidir_algostate* state,
    struct bidir_stamp* stamp, struct radclock_data *rad_data, vcounter_t RTT,
    int qual_warning)
{
	index_t si = state->stamp_i;  // convenience

	int ret;
	long double DelTb;  // Server time interval between stamps j and i
	double phat;        // Period estimate for current stamp
	double perr_ij;     // Estimate of error of phat using given stamp pair [i,j]
	double perr_i;      // Estimate of error of phat at stamp i
	double baseerr;     // Holds difference in quality RTTmin values at different stamps

	ret = 0;
	DEL_STATUS(rad_data, STARAD_PHAT_UPDATED);    // typical case if no update

	/* Determine if quality of i sufficient to bother, if so, if (j,i)
	 * sufficient to update phat
	 * if error smaller than Ep, quality pkt, proceed, else do nothing
	 */
	perr_i = state->phat * ((double)(RTT) - state->RTThat);

	if ( perr_i >= state->Ep )
		return 0;

	/*
	 * Point errors (local)
	 * Level shifts (global)  (can also correct for error in RTThat assuming no shifts)
	 * (total err)/Del(t) = (queueing/Delta(vcount))/p  ie. error relative to p
	 */
	DelTb = stamp->Tb - state->pstamp.Tb;
	perr_ij = fabs(perr_i) + fabs(state->pstamp_perr);
	if (state->RTThat > state->pstamp_RTThat)
		baseerr = state->phat * (state->RTThat - state->pstamp_RTThat);
	else
		baseerr = state->phat * (state->pstamp_RTThat - state->RTThat);
	//baseerr = state->phat * (double)labs((long)(state->RTThat - state->pstamp_RTThat));
	perr_ij = (perr_ij + baseerr) / DelTb;

	/* Only continue if quality better than current, or extremely good.
	 * The latter allows graceful tracking, avoids possible lock-in due to
	 * overly optimistic error estimates.
	 */
	if ( (perr_ij >= state->perr) && (perr_ij >= state->Ep_qual) )
		return 0;


	/* Candidate accepted based on quality, flag the change if large */
	phat = compute_phat(state, &state->pstamp, stamp);
	if ( fabs((phat - state->phat)/phat) > metaparam->RateErrBOUND/3 ) {
		verbose(VERB_SYNC, "i=%lu: Jump in phat update, phat stats: (j,i)=(%lu,%lu), "
		    "rel diff = %.10g, perr = %.3g, baseerr = %.10g, DelTb = %5.3Lg [hrs]",
		    si, state->pstamp_i, si, (phat - state->phat)/phat,
		    perr_ij, baseerr, DelTb/3600);
	}

	/* Clock correction and phat update.
	 * Sanity check applies here 
	 * correct K to keep C(t) continuous at time of last stamp
	 */
	if ((fabs(state->phat - phat)/state->phat > state->Ep_sanity) || qual_warning) {
		if (qual_warning)
			verbose(VERB_QUALITY, "i=%lu: qual_warning received, following "
			    "sanity check for phat", si);

		verbose(VERB_SANITY, "i=%lu: phat update fails sanity check. "
		    "phat stats: (j,i)=(%lu,%lu), rel diff = %.10g, perr = %.3g,"
		    "perr_ij = %.3g, baseerr = %.10g, DelTb = %5.3Lg [hrs]",
		    si, state->pstamp_i, si, (phat - state->phat)/phat,
		    state->perr, perr_ij, baseerr, DelTb/3600);

		state->phat_sanity_count++;
		ADD_STATUS(rad_data, STARAD_PHAT_SANITY);
		ret = STARAD_PLOCAL_SANITY;
	} else {
		state->K += state->stamp.Ta * (long double) (state->phat - phat);
		state->phat = phat;    // candidate accepted! update phat
		state->perr = perr_ij;
		ADD_STATUS(rad_data, STARAD_PHAT_UPDATED);
		DEL_STATUS(rad_data, STARAD_PHAT_SANITY);
	}

	return (ret);
}




/* =============================================================================
 * PLOCAL ALGO
 * ===========================================================================*/

/* Warmup: refinement pointless here, just copy.
 * If not active, never used, no cleanup needed.
 */

int
process_plocal_full(struct bidir_algostate* state, struct radclock_data *rad_data,
    unsigned int plocal_winratio, int p_insane, int qual_warning, struct bidir_algooutput *output)
{
	index_t si = state->stamp_i;  // convenience

	index_t st_end;        // indices of last pkts in stamp history
	index_t RTT_end;       // indices of last pkts in RTT history

	/* History lookup window boundaries */
	index_t lhs;
	index_t rhs;
	long double DelTb;     // Time between j and i based on each NTP timestamp
	struct bidir_stamp *stamp_near;
	struct bidir_stamp *stamp_far;
	double plocal;         // Local period estimate for current stamp
	double plocalerr;      // estimate of total error of plocal [unitless]
	vcounter_t *RTT_far;   // RTT value holder
	vcounter_t *RTT_near;  // RTT value holder


	/*
	 * Compute index of stamp we require to be available before proceeding
	 * (different usage to shift_end etc!)
	 * if not fully past poll transition and have not history ready then
	 *   - default to phat copy if problems with data or transitions
	 *   - record a problem, will have to restart when it resolves
	 * else proceed with plocal processing
	 */
	state->plocal_end = si - state->plocal_win + 1 - state->wwidth - state->wwidth / 2;
	st_end  = history_old(&state->stamp_hist);
	RTT_end = history_old(&state->RTT_hist);

	/*
	 * If there are not enough points, cannot compute plocal. Flag problem for
	 * other parts of algo and default plocal to phat.
	 * Otherwise, it may be that we just recover from not having enough data
	 * points in window, and need to re-init plocal.
	 */
	if (state->plocal_end < MAX(state->poll_changed_i, MAX(st_end, RTT_end))) {
		state->plocal = state->phat;
		state->plocal_problem = 1;

// TODO this is very chatty when it happens ... module the rate?
		verbose(VERB_CONTROL, "plocal problem following parameter changes "
		    "(desired window first stamp %lu unavailable), defaulting to "
		    "phat while windows fill", state->plocal_end);
		verbose(VERB_CONTROL, "[plocal_end, lastpoll_i, st_end, RTT_end] : "
		    "%lu %lu %lu %lu ", state->plocal_end, state->poll_changed_i, st_end, RTT_end);
		return (0);
	}
	else if (state->plocal_problem)
		reset_plocal(state, plocal_winratio, si);

	lhs = si - state->wwidth - state->plocal_win - state->wwidth/2;
	rhs = si - 1             - state->plocal_win - state->wwidth/2;
	state->far_i  = history_min_slide(&state->RTT_hist, state->far_i, lhs, rhs);
	//       if get qual warning, should we Then exclude the current stamp?
	state->near_i = history_min_slide(&state->RTT_hist, state->near_i,
	    si-state->wwidth, si-1); // is normal to give old win as input

	/* Compute time intervals between NTP timestamps of selected stamps */
	stamp_near = history_find(&state->stamp_hist, state->near_i);
	stamp_far  = history_find(&state->stamp_hist, state->far_i);

	plocal = compute_phat(state, stamp_far, stamp_near);
	/* Something bad happen, most likely, we have a bug. The algo may recover
	 * from this, so do not update and keep going. TODO: check if needed or better handled under sanity */
	if ( plocal == 0 )
		return 1;

	RTT_far  = history_find(&state->RTT_hist, state->far_i);
	RTT_near = history_find(&state->RTT_hist, state->near_i);
	DelTb = stamp_near->Tb - stamp_far->Tb;
	plocalerr = state->phat * ((double)(*RTT_far + *RTT_near) - 2*state->RTThat) / DelTb;

	/* If quality looks bad, retain previous value */
	if ( fabs(plocalerr) >= state->Eplocal_qual ) {
		verbose(VERB_QUALITY, "i=%lu: plocal quality low,  (far,near) = "
		    "(%lu,%lu), not updating plocalerr = %5.3lg, "
		    "Eplocal_qual = %5.3lg, RTT (near,far,hat) = (%5.3lg, %5.3lg, %5.3lg) ",
		    si, state->far_i, state->near_i, state->plocalerr, state->Eplocal_qual,
		    state->phat * (double)(*RTT_near),
		    state->phat * (double)(*RTT_far),
		    state->phat * (double)(state->RTThat) );
		ADD_STATUS(rad_data, STARAD_PLOCAL_QUALITY);
		DEL_STATUS(rad_data, STARAD_PLOCAL_SANITY);  // sanity applies only to quality stamps
	} else {
		/* Candidate acceptable based on quality, record this */
		DEL_STATUS(rad_data, STARAD_PLOCAL_QUALITY);

		/* Apply Sanity Check
		 * If quality looks good, continue but refuse to update if result looks
		 * insane. qual_warning may not apply to stamp_near or stamp_far, but we
		 * still follow the logic "there is something strange going on in here".
		 * Also, plocal searches in two windows for best stamps, which is a decent
		 * damage control.
		 */
		if ( (fabs(state->plocal-plocal)/state->plocal > state->Eplocal_sanity) || qual_warning) {
			if (qual_warning)
				verbose(VERB_QUALITY, "qual_warning received, i=%lu, following "
				    "sanity check for plocal", si);
			verbose(VERB_SANITY, "i=%lu: plocal update fails sanity check: relative "
			    "difference is: %5.3lg estimated error was %5.3lg",
			    si, fabs(state->plocal-plocal)/state->plocal, plocalerr);
			ADD_STATUS(rad_data, STARAD_PLOCAL_SANITY);
			state->plocal_sanity_count++;
		} else {
			state->plocal = plocal;
			// TODO: should actually age this stored value if quality is
			// bad or sanity and we cannot update to the latest computed
			state->plocalerr = plocalerr;
			DEL_STATUS(rad_data, STARAD_PLOCAL_SANITY);
		}
	}

	/* Copy selected internal (not held in state) local vars to output structure */
	output->plocalerr = plocalerr;  // value for This stamp, perhaps not accepted into state

	return 0;
}



/* =============================================================================
 * THETAHAT ALGO
 * ===========================================================================*/

void
process_thetahat_warmup(struct bidir_metaparam *metaparam, struct bidir_algostate* state,
    struct bidir_stamp* stamp, struct radclock_data *rad_data, vcounter_t RTT,
    struct bidir_algooutput *output)
{
	index_t si = state->stamp_i;  // convenience

	double thetahat;     // double ok since this corrects clock which is already almost right
	double wj;           // weight of pkt i
	double wsum = 0;     // sum of weights
	double th_naive = 0; // thetahat naive estimate
	double ET = 0;       // error thetahat ?
	double minET = 0;    // error thetahat ?

	double *thnaive_tmp;
	double gapsize;      // size in seconds between pkts, used to track widest gap in offset_win

	index_t adj_win;     // adjusted window
	index_t j;           // loop index, needs to be signed to avoid problem when j hits zero
	index_t jmin  = 0;   // index that hits low end of loop
	index_t jbest = 0;   // record best packet selected in window

	double Ebound;         // per-stamp error bound on thetahat  (currently just ET)
	double Ebound_min = 0; // smallest Ebound in offset win
	index_t RTT_end;       // indices of last pkts in RTT history
	index_t thnaive_end;   // indices of last pkts in thnaive history
	struct bidir_stamp *stamp_tmp;
	vcounter_t *RTT_tmp;   // RTT value holder


	/* During warmup, no plocal refinement, no gap detection, no SD error
	 * correction, only simple sanity warning 
	 */
	if ( (stamp->Te - stamp->Tb) >= RTT*state->phat*0.95 ) {
		verbose(VERB_SYNC, "i=%d: Apparent server timestamping error, RTT<SD: "
		    "RTT = %6.4lg [ms], SD= %6.4lg [ms], SD/RTT= %6.4lg.",
		    si, 1000*RTT*state->phat, 1000*(double)(stamp->Te-stamp->Tb),
		    (double)(stamp->Te-stamp->Tb)/RTT/state->phat );
	}
	th_naive = (state->phat*((long double)stamp->Ta + (long double)stamp->Tf) +
	    (2*state->K - (stamp->Tb + stamp->Te)))/2.0;
	th_naive += state->base_asym/2.0;    // correct for 'known' asymmetry
	history_add(&state->thnaive_hist, si, &th_naive);


	/* Calculate weighted sum */
	wsum = 0;
	thetahat = 0;
	/* Fix old end of thetahat window:  poll_period changes, offset_win changes, history limitations */
	if ( state->poll_transition_th > 0 ) {
		/* linear interpolation over new offset_win */
		adj_win = (state->offset_win - state->poll_transition_th) +
		    (ceil)(state->poll_transition_th * state->poll_ratio);
		verbose(VERB_CONTROL, "In offset_win transition following poll_period change, "
		    "[offset transition] windows are [%lu %lu]", state->offset_win,adj_win);

		// Safe form of jmin = MAX(1, si-adj_win+1);
		if ( si > (adj_win - 1) ) jmin = si - (adj_win - 1); else jmin = 0;
		jmin = MAX(1, jmin);
		state->poll_transition_th--;
	}
	else {
		/* ensure don't go past 1st stamp, and don't use 1st, as thnaive set to zero there */
		// Safe form of  jmin = MAX(1, si-state->offset_win+1);
		if ( si > (state->offset_win - 1 )) jmin = si - (state->offset_win - 1); else jmin = 0;
		jmin = MAX(1, jmin);
	}
	
	/* find history constraint */
	RTT_end     = history_old(&state->RTT_hist);
	thnaive_end = history_old(&state->thnaive_hist);
	jmin = MAX(jmin, MAX(RTT_end, thnaive_end));

	for ( j = si; j >= jmin; j-- ) {
		/* Reassess pt errors each time, as RTThat not stable in warmup.
		 * Errors due to phat errors are small
		 * then add aging with pessimistic rate (safer to trust recent)
		 */
		RTT_tmp   = history_find(&state->RTT_hist, j);
		stamp_tmp = history_find(&state->stamp_hist, j);
		ET  = state->phat * ((double)(*RTT_tmp) - state->RTThat );
		ET += state->phat * (double)( stamp->Tf - stamp_tmp->Tf ) * metaparam->BestSKMrate;

		/* Per point bound error is simply ET in here */
		//Ebound  = ET;

		/* Record best in window, smaller the better. When i<offset_win, must
		 * to be zero since arg minRTT is at the new stamp, which has no aging
		 */
		if ( j == si ) {
			minET = ET;
			jbest = j;
		} else {
			if ( ET < minET ) {
				minET = ET;
				jbest = j;
				//Ebound_min = Ebound;    // could give weighted version instead
			}
		}
		/* calculate weight, is <=1
		 * note: Eoffset initialised to non-0 value, safe to divide
		 */
		wj = exp(- ET * ET / state->Eoffset / state->Eoffset);
		wsum += wj;
		thnaive_tmp = history_find(&state->thnaive_hist, j);
		thetahat = thetahat + wj * *thnaive_tmp;
	}

	/* Set Ebound_min to second best ET in window */
	for ( j = si; j >= jmin; j-- ) {
		if ( j == jbest ) continue;

		RTT_tmp   = history_find(&state->RTT_hist, j);
		stamp_tmp = history_find(&state->stamp_hist, j);
		ET  = state->phat * ((double)(*RTT_tmp) - state->RTThat );
		ET += state->phat * (double)( stamp->Tf - stamp_tmp->Tf ) * metaparam->BestSKMrate;

		/* Record best in window excluding jbest */
		if ( j == si || (jbest == 1) && (j == si - 1) )
			Ebound_min = ET;		// initialize
		else
			if ( ET < Ebound_min ) Ebound_min = ET;

	}



//	/* If a RTCreset, must adopt the most recent UTC available: normal quality
//	 * weighting or checks, and sanity checks, don't apply.
//	 * TODO: check settings for other variables not used in this branch, and for
//	 *		any other impacts of having the counter frozen, effectively an invisible jump backward
//	 *    check phat as well, other copies?  if change K, must change it IMMediately
//	 */
//	if (RTCreset) {
//		verbose(VERB_QUALITY, "i=%lu: RTCreset (warmup): setting to current stamp, "
//			"disabling window, quality may be poor until window refills.\n"
//			"      Jumping rAdclock by = %5.3lf [s]", si, - th_naive);
//
//		state->K -= th_naive;				// absorb current apparent error into K
//		thetahat = 0;						// error is zero wrt new K
//		//state->phat -= 2 * (state->thetahat - th_naive) / (double)(stamp->Ta + stamp->Tf);   // check this!!
//		state->thetahat = thetahat;
//		jbest = si;
//
//		/* Overwrite thetahats in window to ensure reset value is adopted.
//		 * This resets the already stored th_naive also. Retain th_naive variable
//		 * for verbosity and output to preserve original data and mark event.
//		 */
//		for ( j = si; j >= jmin; j-- )
//			*(double *)(history_find(&state->thnaive_hist, j)) = 0;
//
//		DEL_STATUS(rad_data, STARAD_OFFSET_QUALITY);	// since accepted stamp, even though quality likely poor
//		DEL_STATUS(rad_data, STARAD_OFFSET_SANITY);
//
//		ALGO_ERROR(state)->Ebound_min_last = Ebound_min;
//		copystamp(stamp, &state->thetastamp);
//	}
//	else {

	/* Check Quality
	 * quality over window looks good, continue otherwise log quality warning
	 */
	gapsize = state->phat * (double)(stamp->Tf - state->stamp.Tf);
	if ( minET < state->Eoffset_qual ) {
		DEL_STATUS(rad_data, STARAD_OFFSET_QUALITY);
		if ( wsum==0 ) {
			verbose(VERB_QUALITY, "i=%lu, quality looks good (minET = %lg) yet wsum=0! "
			    "Eoffset_qual = %lg may be too large", si, minET, state->Eoffset_qual);
			thetahat = state->thetahat;
		} else {
			thetahat /= wsum;
			/* store est'd quality of new estimate */
			state->minET = minET;
			/* Record last good estimate of error bound.
			 * Also need to track last time we updated theta to do proper aging of
			 * clock error bound in warmup
			 */
			ALGO_ERROR(state)->Ebound_min_last = Ebound_min;	// if quality bad, old value naturally remains
			copystamp(stamp, &state->thetastamp);
		}

		/* if result looks insane, give warning */
		if ( fabs(state->thetahat - thetahat) > (state->Eoffset_sanity_min + state->Eoffset_sanity_rate * gapsize) ) {
			verbose(VERB_SANITY, "i=%lu: thetahat update fails sanity check: "
			    "difference is: %5.3lg [ms], estimated error was  %5.3lg [ms]",
			    si, 1000*(thetahat - state->thetahat), 1000*minET);
			state->offset_sanity_count++;
			ADD_STATUS(rad_data, STARAD_OFFSET_SANITY);
		} else
			DEL_STATUS(rad_data, STARAD_OFFSET_SANITY);

		/* update value of thetahat, even if sanity triggered */
		state->thetahat = thetahat;

	} else {
		verbose(VERB_QUALITY, "i=%lu: thetahat quality over offset window poor "
		    "(minET %5.3lg [ms], thetahat = %5.3lg [ms]), repeating current value",
		    si, 1000*minET, 1000*thetahat);
		ADD_STATUS(rad_data, STARAD_OFFSET_QUALITY);
	}

//	}    // RTCreset hack

	/* Cauality Check  (monitoring only)
	 * Perceived OWDs (ie OWDs as seen by Ca) should always be +ve
	 */
	double pDf;    // perceived Df,  ie  Tb - Ca(Ta)
	double pDb;    // perceived Db,  ie  Ca(Tf) - Te
	pDf = (double)((long double)stamp->Tb - ((long double)stamp->Ta * state->phat + state->K));
	pDf += state->thetahat;
	pDb = (double)((long double)stamp->Tf * state->phat + state->K - (long double) stamp->Te);
	pDb -= state->thetahat;

	if (pDf <= 0)
		verbose(VERB_CAUSALITY, "i=%lu: causality breach seen on C(Ta), pDf = %5.3lf "
		    "[ms], thetahat = %5.3lf [ms]", si, 1000*pDf, 1000*state->thetahat);
	if (pDb <= 0)
		verbose(VERB_CAUSALITY, "i=%lu: causality breach seen on C(Tf), pDb = %5.3lf "
		    "[ms], thetahat = %5.3lf [ms]", si, 1000*pDb, 1000*state->thetahat);


	/* Warmup to warmup is to pass offset_win */
	if ( (si < state->offset_win*2) || !(si % 50) ) {
		verbose(VERB_SYNC, "i=%lu: th_naive = %5.3lf [ms], thetahat = %5.3lf [ms], "
		    "wsum = %7.5lf, minET = %7.5lf [ms], RTThat/2 = %5.3lf [ms]",
		    si, 1000*th_naive, 1000*thetahat, wsum,
		    1000*minET, 1000*state->phat*state->RTThat/2.);
	}

	/* Copy selected internal (not held in state) local vars to output structure */
	output->pDf      = pDf;
	output->pDb      = pDb;
	output->th_naive = th_naive;
	output->minET    = minET;  // value for This stamp, perhaps not accepted into state
	output->wsum     = wsum;
	stamp_tmp = history_find(&state->stamp_hist, jbest);
	output->best_Tf  = stamp_tmp->Tf;
}



void process_thetahat_full(struct bidir_metaparam *metaparam, struct bidir_algostate* state,
    struct bidir_stamp* stamp, struct radclock_data *rad_data, vcounter_t RTT,
    int qual_warning,  struct bidir_algooutput *output)
{
	index_t si = state->stamp_i;  // convenience

	double thetahat;     // double ok since this corrects clock which is almost right
	double wj;           // weight of pkt i
	double wsum = 0;     // sum of weights
	double th_naive = 0; // thetahat naive estimate
	double ET = 0;       // error thetahat ?
	double minET = 0;    // error thetahat ?

	double *thnaive_tmp;
	double gapsize;    // size in seconds between pkts, tracks widest gap in offset_win
	//int gap = 0;    // logical: 1 = have found a large gap at THIS stamp  // not used

	index_t adj_win;        // adjusted window
	index_t j;              // loop index, needs to be signed to avoid problem when j hits zero
	index_t jmin  = 0;      // index that hits low end of loop
	index_t jbest = 0;      // record best packet selected in window

	double Ebound;          // per-stamp error bound on thetahat  (currently just ET)
	double Ebound_min = 0;  // smallest Ebound in offset win
	index_t st_end;         // indices of last pkts in stamp history
	index_t RTT_end;        // indices of last pkts in RTT history
	index_t RTThat_end;     // indices of last pkts in RTThat history
	index_t thnaive_end;    // indices of last pkts in thnaive history
	struct bidir_stamp *stamp_tmp;
	struct bidir_stamp *stamp_tmp2;
	vcounter_t *RTT_tmp;    // RTT value holder
	vcounter_t *RTThat_tmp; // RTThat value holder


	if ((stamp->Te - stamp->Tb) >= RTT*state->phat * 0.95) {
		verbose(VERB_SYNC, "i=%lu: Apparent server timestamping error, RTT<SD: "
		    "RTT = %6.4lg [ms], SD= %6.4lg [ms], SD/RTT= %6.4lg.",
		    si, 1000 * RTT*state->phat, 1000 * (double)(stamp->Te-stamp->Tb),
		    (double)(stamp->Te-stamp->Tb)/RTT/state->phat);
	}

	/* Calculate naive estimate at stamp i
	 * Also track last element stored in thnaive_end */
	th_naive = (state->phat * ((long double)stamp->Ta + (long double)stamp->Tf) +
	    (2 * state->K - (stamp->Tb + stamp->Te))) / 2.0;
	th_naive += state->base_asym/2.0;    // correct for 'known' asymmetry
	history_add(&state->thnaive_hist, si, &th_naive);

	/* Initialize gapsize
	 * Detect gaps and note large gaps (due to high loss)
	 * Default is no gap, if one, computed below
	 * gapsize is initialized here for this i, to localize big gaps
	 */
	gapsize = state->phat * (double)(stamp->Tf - state->stamp.Tf);

	/* gapsize is in [sec], but here looking for loss events */
	if ( gapsize > (double) state->poll_period * 4.5 ) {
		verbose(VERB_SYNC, "i=%lu: Non-trivial gap found: gapsize = %5.1lf "
		    "stamps or %5.3lg [sec]", si,
		    gapsize/state->poll_period, gapsize);
		/* In `big gap' mode, mistrust plocal and trust local th more */
		if (gapsize > (double) metaparam->SKM_SCALE) {
			//gap = 1;
			verbose(VERB_SYNC, "i=%lu: End of big gap found width = %5.3lg [day] "
			    "or %5.2lg [hr]", si, gapsize/(3600*24), gapsize/3600);
		}
	}

	/* Calculate weighted sum */
	wsum = 0;
	thetahat = 0;

	/* Fix old end of thetahat window:  poll_period changes, offset_win changes,
	 * history limitations
	 */
	if (state->poll_transition_th > 0) {
		/* linear interpolation over new offset_win */
		adj_win = (state->offset_win - state->poll_transition_th) +
		    (ceil)(state->poll_transition_th * state->poll_ratio);
		verbose(VERB_CONTROL, "In offset_win transition following poll_period "
		    "change, [offset transition] windows are [%lu %lu]",
		    state->offset_win, adj_win);

		// TODO: check if in fact this is safe when given arguments that are all signed, like here
		// Safe form of  jmin = MAX(2, si-adj_win+1);
		if (si > (adj_win - 1)) jmin = si - (adj_win - 1); else jmin = 0;
		jmin = MAX(2, jmin);    // check why this should be 1 not 2
		state->poll_transition_th--;
	}
	else {
		/* ensure don't go past 1st stamp, and don't use 1st, as thnaive set to 0 there */
		// safe form of  min = MAX(1, si-state->offset_win+1);
		if (si > (state->offset_win-1)) jmin = si - (state->offset_win - 1); else jmin = 0;
		jmin = MAX(1, jmin);
	}
	/* find history constraint */
	st_end      = history_old(&state->stamp_hist);
	RTT_end     = history_old(&state->RTT_hist);
	RTThat_end  = history_old(&state->RTThat_hist);
	thnaive_end = history_old(&state->thnaive_hist);

	jmin = MAX(jmin, MAX(st_end, MAX(RTT_end, MAX(RTThat_end, thnaive_end))));

	for (j = si; j >= jmin; j--) {
		/* first one done, and one fewer intervals than stamps
		 * find largest gap between stamps in window */
		if (j < si - 1) {
			stamp_tmp = history_find(&state->stamp_hist, j);
			stamp_tmp2 = history_find(&state->stamp_hist, j+1);
			gapsize = MAX(gapsize, state->phat * (double) (stamp_tmp2->Tf - stamp_tmp->Tf));
		}

		/*
		 * Don't reassess pt errors (shifts already accounted for) then add SD
		 * quality measure (large SD at small RTT=> delayed Te, distorting
		 * th_naive) then add aging with pessimistic rate (safer to trust recent)
		 */
		RTT_tmp    = history_find(&state->RTT_hist, j);
		RTThat_tmp = history_find(&state->RTThat_hist, j);
		stamp_tmp  = history_find(&state->stamp_hist, j);
		ET  = state->phat * ((double)(*RTT_tmp) - *RTThat_tmp);
		ET += state->phat * (double) ( stamp->Tf - stamp_tmp->Tf ) * metaparam->BestSKMrate;

		/* Per point bound error is ET without the SD penalty */
		//Ebound  = ET;

		/* Add SD penalty to ET
		 * SD quality measure has been problematic in different cases:
		 * - kernel timestamping with hardware based servers(DAG, 1588), punish good stamps
		 * - with bad NTP servers that have SD > Eoffset_qual all the time.
		 * Definitively removed on 28/07/2011
		 */
		//ET += stamp_tmp->Te - stamp_tmp->Tb;

		/* Record best in window, smaller the better. When i<offset_win, must be
		 * zero since arg minRTT also in win. Initialise minET to 1st in window. */
		if ( j == si ) {
			minET = ET; jbest = j; Ebound_min = Ebound;
		} else {
			if (ET < minET) { minET = ET; jbest = j; }
			//if (Ebound < Ebound_min) { Ebound_min = Ebound; } // dont assume min index same here
		}
		/* Calculate weight, is <=1 Note: Eoffset initialised to non-0 value, safe to divide */
		wj = exp(- ET * ET / state->Eoffset / state->Eoffset);
		wsum += wj;

		/* Correct phat already used by difference with more locally accurate plocal */
		thnaive_tmp = history_find(&state->thnaive_hist, j);
		if (state->plocal_problem)
			thetahat += wj * (*thnaive_tmp);
		else
			thetahat += wj * (*thnaive_tmp - (state->plocal / state->phat - 1) *
			    state->phat * (double) (stamp->Tf - stamp_tmp->Tf));
	}

	/* Set Ebound_min to second best ET in window */
	for (j = si; j >= jmin; j--) {
		if (j == jbest) continue;

		RTT_tmp   = history_find(&state->RTT_hist, j);
		stamp_tmp = history_find(&state->stamp_hist, j);
		ET  = state->phat * ((double)(*RTT_tmp) - state->RTThat );
		ET += state->phat * (double)( stamp->Tf - stamp_tmp->Tf ) * metaparam->BestSKMrate;

		/* Record best in window excluding jbest */
		if ( j == si || (jbest == 1) && (j == si - 1) )
			Ebound_min = ET;    // initialize
		else
			if ( ET < Ebound_min ) Ebound_min = ET;
	}

	/* Check Quality and calculate new candidate estimate
	 * If quality over window looks good, use weights over window, otherwise
	 * forget weights (and plocal refinement) and lean on last reliable estimate
	 * TODO: replace convoluted thetahat "UPDATE SUPPRESSED' overwritting with clear logic
	 */
	if (minET < state->Eoffset_qual) {
		DEL_STATUS(rad_data, STARAD_OFFSET_QUALITY);
		/* if wsum==0 just copy thetahat to avoid crashing (can't divide by zero)
		 * (this must be addressed by operator), else safe to normalise */
		if (wsum == 0) {
			verbose(VERB_QUALITY, "i=%lu: quality looks good (minET = %lg) yet "
			    "wsum=0! Eoffset_qual = %lg may be too large",
			    si, minET,state->Eoffset_qual);
			thetahat = state->thetahat;  // UPDATE SUPPRESSED
		} else
			thetahat /= wsum;
	} else {
		//  BUG  where has gap code gone!!
		thetahat = state->thetahat;  // this will effectively suppress sanity check TODO: make logic clearer
		verbose(VERB_QUALITY, "i=%lu: thetahat quality very poor. wsum = %5.3lg, "
		    "curr err = %5.3lg, old = %5.3lg, this pt-err = [%5.3lg] [ms]",
		    si, wsum, 1000*minET, 1000*state->minET, 1000*ET);
		state->offset_quality_count++;
		ADD_STATUS(rad_data, STARAD_OFFSET_QUALITY);
	}


	/* Apply Sanity Check 
	 * sanity also relative to duration of lockouts due to low quality
	 */
	gapsize = MAX(gapsize, state->phat * (double)(stamp->Tf - state->thetastamp.Tf) );
	/* if looks insane given gapsize, refuse update */
	if ((fabs(state->thetahat-thetahat) > (state->Eoffset_sanity_min +
	    state->Eoffset_sanity_rate * gapsize)) || qual_warning) {
		if (qual_warning)
			verbose(VERB_QUALITY, "i=%lu: qual_warning received, following sanity"
			    " check for thetahat", si);
		verbose(VERB_SANITY, "i=%lu: thetahat update fails sanity check."
		    "diff= %5.3lg [ms], est''d err= %5.3lg [ms], sanity level: %5.3lg [ms] "
		    "with total gapsize = %.0lf [sec]",
		    si, 1000*(thetahat-state->thetahat), 1000*minET,
		    1000*(state->Eoffset_sanity_min+state->Eoffset_sanity_rate*gapsize), gapsize);
		state->offset_sanity_count++;
		ADD_STATUS(rad_data, STARAD_OFFSET_SANITY);
	}
	else {  // either quality and sane, or repeated due to bad quality
		state->thetahat = thetahat;    // it passes! update current value
		if ( ( minET < state->Eoffset_qual ) && ( wsum != 0 ) ) { // genuine update, ie if good and !(UPDATE SUPPRESSED)
			state->minET = minET;
			copystamp(stamp, &state->thetastamp);
			// BUG??  where is "lastthetahat = i;" ?  ahh, seems never to have been used..
			/* Record last good estimate of error bound after sanity check */
			ALGO_ERROR(state)->Ebound_min_last = Ebound_min;
		}
		DEL_STATUS(rad_data, STARAD_OFFSET_SANITY);
	}


	/* Cauality Check  (monitoring only)
	 * Perceived OWDs (ie OWDs as seen by Ca) should always be +ve
	 * TODO: For more sanity verb, could consider adding a version comparing against
	 *       the sanity-rejected candidate thetahat?
	 */
	double pDf;    // perceived Df,  ie  Tb - Ca(Ta)
	double pDb;    // perceived Db,  ie  Ca(Tf) - Te
	pDf = (double)((long double)stamp->Ta * state->phat + state->K - (long double) stamp->Tb);
	pDf = -(pDf - state->thetahat);
	pDb = (double)((long double)stamp->Tf * state->phat + state->K - (long double) stamp->Te);
	pDb -= state->thetahat;

	if (pDf <= 0)
		verbose(VERB_CAUSALITY, "i=%lu: post-sanity causality breach on C(Ta), pDf = %5.3lf "
		    "[ms], thetahat = %5.3lf [ms]", si, 1000*pDf, 1000*state->thetahat);
	if (pDb <= 0)
		verbose(VERB_CAUSALITY, "i=%lu: post-sanity causality breach on C(Tf), pDb = %5.3lf "
		    "[ms], thetahat = %5.3lf [ms]", si, 1000*pDb, 1000*state->thetahat);

	if ( !(si % (int)(6 * 3600 / state->poll_period)) ) {
		verbose(VERB_SYNC, "i=%lu: th_naive = %5.3lf [ms], thetahat = %5.3lf [ms], "
		    "wsum = %7.5lf, minET = %7.5lf [ms], RTThat/2 = %5.3lf [ms]",
		    si, 1000*th_naive, 1000*thetahat, wsum,
		    1000*minET, 1000*state->phat*state->RTThat/2.);
	}

	/* Copy selected internal (not held in state) local vars to output structure */
	output->pDf      = pDf;
	output->pDb      = pDb;
	output->th_naive = th_naive;
	output->minET    = minET;  // value for This stamp, perhaps not accepted into state
	output->wsum     = wsum;
	stamp_tmp = history_find(&state->stamp_hist, jbest);
	output->best_Tf  = stamp_tmp->Tf;

}



/* =============================================================================
 * PATHPENALTY METRIC
 * ===========================================================================*/

/* Goal: to track a robust measure of path quality over a "path_scale" timescale
 * relevant for the prediction of typical rAdclock quality over that path
 * over that timescale. The metric is used by preferred_RADclock as the
 * main basis of preferred server selection.
 *
 * The penalty comprises three components measured in [s]
 *    Pbase       impact of the value of the RTT BaseLine
 *    Pchange     impact of the changes in RTT BL (asym implications of level shifts)
 *    Pquality    impact of stamp absence, congestion quality, or insanity
 * and is defined as
 *    pathpenalty = Pbase + Pchange + Pquality
 *
 * The three components are designed to be approximately decoupled, focussed
 * on different types of impacts :
 *  Pbase:    basic environment (insensitive to BL shifts and quality)
 *  Pchange:  quantifying a bound on the asymmetry-error impact of BL shifts
 *  Pquality: dynamic performance of rAdclock, given the base and irrespective of
 *            asymmetry, incorporating path congestion and stamp availability
 *
 * Pathpenalty is a heuristic that does not correspond directly to clock
 * error or a clock error bound, which are hard to estimate and therefore would
 * not be a robust choice for the important task of preferred server selection.
 * However in some circumstances pathpenalty's components do correspond to
 * an estimated clock error bound. For example Pbase is a bound on the
 * in-practice worse case path asymmetry error. Under persistent starvation
 * Pquality bounds worst case drift that will dominate pathpenalty, and
 * then be understood as a bound on the clock error.
 * Pathpenalty can be thought of as an "error risk" metric that reflects
 * risks from all the main sources of clock error.
 *
 * Notes on stampgap definition and treatment
 *  - a stampgap processed internally here once a new stamp finally arrives
 *    corresponds to the state->rawstampgap tracked externally in preferred_RADclock
 *    Before it arrives, but is handled differently. Externally it is added in
 *    directly (linear increase in pathpenalty), here it is subject to EWMA smoothing,
 *    which will decrease its impact. This is appropriate when seeking to measure the
 *    average impact of gaps of different durations over the path. Externally,
 *    the main goal is to detect persistent starvation (unavailable server) rapidly
 *    (unconstrained by path_scale), for timely transition to a suitable backup.
 *
 * Notes on timescale for path evaluation
 * Considerations:
 *  - should use same timescale for each server for fair comparison
 *  - make sense to use the same scale for each of Pchange, Pquality, churn_scale
 *  - should be smaller than history_scale for consistency (but no need to enforce)
 *  - should be larger than shift window, but this varies with server, too complicated
 *  - should be large enough to see typical results, away from algo reaction
 *    to changing path conditions, and to avoid jitter from most source
 *  - but small enough so no long wait to move to clearly better alternative
 *  - complicated to automate: going with expert setting around 5hrs
 * Context: time to drift 1ms ~ 3hrs (@ excellent 1e-7 rate)
 *   (small,classic,typical,max) shiftwin: 100samples @ pp=1,16,64,1024) ~ (1.7m,27m,1.8h,28h)
 * */
static void
update_pathpenalty_full(struct bidir_metaparam *metaparam, struct bidir_algostate* state,
    struct bidir_stamp *stamp, struct bidir_algooutput *output)
{

	/* EWMA  (with timescale path_scale) */
	double alpha = 1 - exp(- state->poll_period / metaparam->path_scale);  // EWMA param
	double TV, Err;        // time series variables to be depreciated or smoothed
	double driftpersample; // drift bound over poll-period (=EWMA sample)
	double stampgap;       // gap between stamps [s] for drift assessment
	int nsamples;          // number of EWMA samples over stampgap

	/* Pbase
	 * An EWMA smooth of RTThat, initialized to Pbase = RTThat at the end of warmup.
	 *  sRh = (1-al)sRh + al*RTThat  [ state variable is sRh = state->Pbase ]
	 * Purely a fn of RTThat timeseries (except for phat conversion) */
	state->Pbase *= 1 - alpha;
	state->Pbase += alpha * state->phat * state->RTThat * metaparam->relasym_bound_global;


	/* Pchange
	 * The Total Variation (TV) of the RTT BL over a sliding window, implemented
	 * softly via exponential depreciation. Initialized to dTV = 0.
	 *  dTV = (1-al)dTV + TV/2   [ state variable is dTV = state->Pchange ]
	 * Purely a fn of RTThat timeseries (except for phat conversion)
	 *
	 * As TV is a bound on asym jitter, we use TV/2 as a bound on clock impact.
	 * During loss, shift detail is not observed and depreciation steps are missed.
	 * Using the aggregate shift movement measured on the first stamp after loss
	 * is reasonable as shifts are rare.
	 * If shifts cease Pchange is guaranteed to converge to 0.
	 */
	/* Calculate the TV increase for this stamp */
	if (state->RTThat != state->prevRTThat) {  // common case is no change
		if (state->RTThat > state->prevRTThat)  // use unsigned-safe |RTThat - prevRTThat|
			TV = (state->RTThat - state->prevRTThat) * state->phat / 2;
		else
			TV = (state->prevRTThat - state->RTThat) * state->phat / 2;
	} else
		TV = 0;

	/* Update the depreciated state. Do so at double the path_scale so
	 * BL jitter amplitudes not overly discounted. */
	state->Pchange *= pow(1 - alpha,0.5);
	state->Pchange += TV;


	/* Stamp gap calculation [s]
	 * This internal calculation is independent of external gap tracking, ensuring
	 * it works in all cases, even if running dead, and/or only a single server. */
	stampgap = state->phat * (stamp->Tf - state->stamp.Tf);

	/* Pquality
	 * An EWMA smooth of a metric Err capturing stamp quality related error.
	 * Initialized to sErr = 0 .
	 *  sErr = (1-al)sErr + al*Err  [ state variable is sErr = state->Pquality ]
	 * Is a fn of RTThat (via minET) and thetahat algo (update indices, minET).
	 *
	 * Here Err is the error just Before this stamp is processed, accounting for
	 * conditions in vogue over the gap (hence independent of current stamp quality).
	 * The current stamp is used only to reinitialize Err for next time.
	 * The bound on drift accumulates as long as loss/mistrust, poor quality
	 * (including due to UpJump before detection), or sanity prevails,
	 * resetting only when a valid stamp leads to a true thetahat update.
	 *
	 * During gaps EMWA samples are missed, ignoring these would dramatically under-
	 * estimate accumulating drift: provide the EWMA update for missing samples
	 * by iterating to consume stampgap in pp chunks, assuming linear drift growth.
	 */
	Err = state->Pquality_err;  // initialize with error after last stamp
	driftpersample = state->poll_period * metaparam->RateErrBOUND;
	nsamples = (int) MAX(1,round(stampgap / state->poll_period));
	for (int i=1; i<=nsamples; i++) {
		Err += driftpersample;
		state->Pquality *= 1 - alpha;
		state->Pquality += alpha * Err;
	}

	/* Reset initial error at this stamp, for use next time */
	if ( !memcmp(&state->thetastamp, stamp, sizeof(*stamp)) )  // if thetahat update accepted
		state->Pquality_err = state->minET;  // reset with point error estimate
	else
		state->Pquality_err = Err;           // drift bound accumulation continues


	/* Update path penalty */
	output->pathpenalty = state->Pbase + state->Pchange + state->Pquality;

}



/* =============================================================================
 * CLOCK SYNCHRONISATION ALGORITHM
 * ===========================================================================*/

/* This routine takes in a new bi-directional stamp, a four-tuple (Ta,Tb,Te,Tf)
 * associated to a given time server, that is assumed to be fundamentally sane,
 * and uses it to update the parameters of the bidir RADclock.
 *
 * The RADclock can we thought of a dual algorithm, which maintains a robust
 * estimation of the parameters (phat,K) of the "Uncorrected clock" :
 *   C(t) = vcount(t)*phat + K
 * which forms the basis of the "difference clock" Cd, used for the measurement
 * of time intervals below a certain timescale (~1000s), and the "Corrected" or
 * "Absolute clock"
 *   Ca(t) = C(t) - theta(t)
 * where the estimation of the drift correction function theta(t) enables its
 * use for the measurement of Absolute time (on a leap-free timescale).
 *
 * state: holds all variables, stored from the previous stamp (i-1), needed to
 *   process the new stamp (i) and hence update algo parameters.
 *   As the algo proceeds, state members are progressively updated, starting with
 *   stamp_i and RTT before the calling of the sub-algos, and ending with the
 *   stamp argument. Other members are updated within the sub-algos in order:
 *   {[window management], RTT, phat, OWDasym, plocal, thetahat}.  Once updated,
 *   they move from an interpretation of "previous stamp value" to "current value".
 *   Outside the algo (inbetween stamp arrivals), all members relate to the last
 *   stamp processed.
 *   Note that updated variables may not actually have changed in value: the
 *   previous value may still be appropriate, or a candidate replacement value
 *   rejected due to quality or sanity-checking reasons.
 *
 * RADclock is structured hierarchically for robustness. The sub-algos
 *    {window management, RTT, phat, plocal,  thetahat, pathpenalty}
 *    {                              OWDasym,                      }
 * address progressively more difficult problems and depend only on the
 * sub-algos on its left in this list, but there is no dependency to the right.
 * An exception is when RTT (measured natively in raw counter units) needs
 * to be expressed in seconds for some purposes, requiring an estimate of phat
 * to do so. However for these purposes this estimate need not be highly accurate.
 */
int
RADalgo_bidir(struct radclock_handle *handle, struct bidir_algostate *state,
    struct bidir_stamp *stamp, int qual_warning,
    struct radclock_data *rad_data, struct radclock_error *rad_error,
    struct bidir_algooutput *output)
{
	struct bidir_metaparam *metaparam;
	struct radclock_config *conf;
	unsigned int warmup_winratio;
	unsigned int plocal_winratio;
	vcounter_t RTT;  // Current RTT (vcount units to avoid pb if phat bad)
	int p_insane;    // records if phat algo inferred insanity on This stamp

	JDEBUG

	conf = handle->conf;
	metaparam = &(conf->metaparam);

	/* Search window meta parameters  (not in algo state)
	 * Gives fraction of Delta(t) sacrificed to near and far search windows. */
	warmup_winratio = 4;    // used in phat warmup algo
	plocal_winratio = 5;    // used in plocal full algo

	/* Now processing stamp i {0,1,2,...) */
	state->stamp_i++;   // initialized to -1 (no stamps processed)

	/* First stamp (stamp 0): perform basic state initialization beyond null. */
	if (state->stamp_i == 0) {
		verbose(VERB_SYNC, "Initialising RADclock synchronization");

		/* Set algo error thresholds based on counter model parameters */
		set_err_thresholds(metaparam, state);

		/* Derive algo windows measured in stamp-index from timescales [s] */
		state->poll_period = conf->poll_period;
		state->poll_ratio = 1;
		set_algo_windows(metaparam, state);

		/* Ensure warmup duration (warmup_win) long enough wrt algo windows */
		state->warmup_win = 100;
		adjust_warmup_win(state->stamp_i, state, plocal_winratio);

		/* Create sufficient storage in needed per-stamp histories */
		history_init(&state->stamp_hist,   (unsigned int) state->warmup_win, sizeof(struct bidir_stamp) );
		history_init(&state->Df_hist,      (unsigned int) state->warmup_win, sizeof(double) );
		history_init(&state->Db_hist,      (unsigned int) state->warmup_win, sizeof(double) );
		history_init(&state->Dfhat_hist,   (unsigned int) state->warmup_win, sizeof(double) );
		history_init(&state->Dbhat_hist,   (unsigned int) state->warmup_win, sizeof(double) );
		history_init(&state->Asymhat_hist, (unsigned int) state->warmup_win, sizeof(double) );
		history_init(&state->RTT_hist,     (unsigned int) state->warmup_win, sizeof(vcounter_t) );
		history_init(&state->RTThat_hist,  (unsigned int) state->warmup_win, sizeof(vcounter_t) );
		history_init(&state->thnaive_hist, (unsigned int) state->warmup_win, sizeof(double) );

		/* Parameter summary: physical, network, windows, thresholds, sanity */
		print_algo_parameters(metaparam, state);
	}

	/* React to on-the-fly configuration updates: if the poll period or
	 * environment quality changed, key algorithm parameters have to be updated. */

//	if (state->stamp_i==1000) {
//		conf->mask = UPDMASK_TEMPQUALITY;
//	}


	if (HAS_UPDATE(conf->mask, UPDMASK_POLLPERIOD) ||
	    HAS_UPDATE(conf->mask, UPDMASK_TEMPQUALITY)) {
		update_state(metaparam, state, plocal_winratio, conf->poll_period);
	}



	/* ==========================================================================
	 * BEGIN SYNCHRONISATION
	 * ========================================================================*/

	/* Current RTT (in raw counter units) */
	RTT = stamp->Tf - stamp->Ta;

	/* These variables not yet updated in state, but insert into history
	 * immediately for availability in history hunting loops. */
	history_add(&state->RTT_hist,   state->stamp_i, &RTT);
	history_add(&state->stamp_hist, state->stamp_i, stamp);


	/* =============================================================================
	* INITIALIZATION Phase :  i=0
	*  Only basic state can be set
	*
	* WARMUP Phase :  0<i<warmup_win
	*  Here pt errors are unreliable, need more robust algo forms
	*    history window:  no overlap by design, so no need to map stamp indices
	*    RTT:          obvious on-line
	*                  upward shift detection:  disabled
	*    phat:         use plocal-type algo
	*                  no sanity checking (can't trust inference)
	*    plocal:       just copies phat
	*    thetahat:     simple on-line weighted average with aging
	*                  no sanity checking
	*    pathpenality: simple based off the current minRTT
	*
	* FULL ALGO Initialization :  i=warmup_win-1
	*  Initialization of on-line components of all full sub-algos
	*
	* FULL ALGO Phase :  i >= warmup_win
	*  Available history now sufficient
	*    history window:  initialize and begin on-line history forgetting `algo`
   *    Use full sub-algos with stricter checking and performance enhancements
	* =========================================================================*/

	if (state->stamp_i == 0) {
		init_algos(conf, state, stamp, rad_data, RTT, output);
		ADD_STATUS(rad_data, STARAD_WARMUP);
		ADD_STATUS(rad_data, STARAD_UNSYNC);
	}
	else if (state->stamp_i < state->warmup_win) {
		process_RTT_warmup(state, RTT);
		process_OWDAsym_warmup(state, stamp);
		process_phat_warmup(state, RTT, warmup_winratio);
		state->plocal = state->phat;
		process_thetahat_warmup(metaparam, state, stamp, rad_data, RTT, output);
		output->pathpenalty = state->RTThat * state->phat * metaparam->relasym_bound_global;
		// TODO: review UNSYNC un/re-setting in general, should be more quality based
		if (state->stamp_i >= NTP_BURST)
			DEL_STATUS(rad_data, STARAD_UNSYNC);

		/* End of warmup: full algo initialization */
		if (state->stamp_i == state->warmup_win - 1) {
			state->history_end = state->h_win - 1;  // 1st history is [0, history_end]
			init_RTTOWDAsym_full(state, stamp);
			init_phat_full(state, stamp);
			init_plocal_full(state, stamp, plocal_winratio);
			init_thetahat_full(state, stamp);
			state->Pbase = output->pathpenalty;
			parameters_calibration(state);
			DEL_STATUS(rad_data, STARAD_WARMUP);
			verbose(VERB_CONTROL, "i=%lu: End of Warmup Phase. Stamp read check: "
			    "%llu %22.10Lf %22.10Lf %llu",
			    state->stamp_i, stamp->Ta, stamp->Tb, stamp->Te, stamp->Tf);
		}
	}
	else {
		manage_historywin(state, stamp, RTT);    // performs next_pstamp init
		process_RTT_full(state, rad_data, RTT);
		process_OWDAsym_full(state, stamp);
		update_next_pstamp(state, stamp, RTT);
		p_insane = process_phat_full(metaparam, state, stamp, rad_data, RTT, qual_warning);
		process_plocal_full(state, rad_data, plocal_winratio, p_insane, qual_warning, output);
		process_thetahat_full(metaparam, state, stamp, rad_data, RTT, qual_warning, output);
		update_pathpenalty_full(metaparam, state, stamp, output);
	}

	/* Processing complete */
	copystamp(stamp, &state->stamp);    // complete update with the current stamp

	collect_stats_state(state, stamp);
	print_stats_state(state);


/* =============================================================================
 * OUTPUTS
 * ===========================================================================*/

	/* Lock to ensure data consistency for data consumers, in particular the SMS */
	pthread_mutex_lock(&handle->globaldata_mutex);

	/* Clock error estimates.
	 * Applies an ageing from thetastamp to the current state, similar to gap recovery.
	 * Don't update the null-initialization on first stamp, as algo parameters v-poor.
	 * Thus nerror counts number of times stats collected, = value of stamp_i .
	 */
	double error_bound;
	if (state->stamp_i > 0) {
		//rad_data->ca_err = state->minET;    // initialize to this stamp
		error_bound = ALGO_ERROR(state)->Ebound_min_last +
			state->phat * (double)(stamp->Tf - state->thetastamp.Tf) * metaparam->RateErrBOUND;

		/* Update _hwin members of  state->algo_err  */
		ALGO_ERROR(state)->cumsum_hwin    += error_bound;
		ALGO_ERROR(state)->sq_cumsum_hwin += error_bound * error_bound;
		ALGO_ERROR(state)->nerror_hwin    += 1;

		/* Update remaining members of  state->algo_err  */
		ALGO_ERROR(state)->error_bound = error_bound;
		ALGO_ERROR(state)->cumsum     += error_bound;
		ALGO_ERROR(state)->sq_cumsum  += error_bound * error_bound;
		ALGO_ERROR(state)->nerror     += 1;
	}

	pthread_mutex_unlock(&handle->globaldata_mutex);  // unlock


	/* Fill the output structure, used mainly to write to the .mat output file
	 * Members not sourced from state are sub-algo internal variables of use in
	 * algo diagnostics.
	 * Ownership notes:
	 *   status:  jointly owned by RADalgo_bidir and process_stamp. Update made here is
	 *     complete wrt both, with exception of STARAD_SYSCLOCK set in update_FBclock and VM code.
	 *   leapsecond_*   output owns, rad_data gets a copy in process_stamp
	 *            best to reverse this? is true that leapsec cant live in state.
	 */
	output->n_stamps     = 1 + state->stamp_i;  // # stamps seen by algo, is ≥1
	output->RTT          = RTT;           // value for This stamp
	output->RTThat       = state->RTThat;
	output->RTThat_new   = state->next_RTThat;
	output->RTThat_shift = state->RTThat_shift;
	output->phat         = state->phat;
	output->perr         = state->perr;
	output->plocal       = state->plocal;
	output->K            = state->K;
	output->thetahat     = state->thetahat;
	output->minET_last   = state->minET;  // value at last thetastamp update

	/* Internal variables saved directly within process_plocal_full */
	//	 output->plocalerr = plocalerr;     // value for This stamp
	/* Internal variables saved directly within process_thetahat_{warmup,full} */
	//  output->th_naive  = th_naive;
	//  output->minET     = minET;         // value for This stamp
	//  output->pDf       = pDf;
	//  output->pDb       = pDb;
	//  output->wsum      = wsum;
	//  stamp_tmp = history_find(&state->stamp_hist, jbest);
	//  output->best_Tf   = stamp_tmp->Tf;

	output->status       = rad_data->status;

	/* Pathpenalty saved directly within update_pathpenalty_full */
	//  output->pathpenalty
	output->Pchange      = state->Pchange;
	output->Pquality     = state->Pquality;


	return (0);
}

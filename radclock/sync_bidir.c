/*
 * Copyright (C) 2006-2020, Darryl Veitch <darryl.veitch@uts.edu.au>
 * Copyright (C) 2006-2012, Julien Ridoux
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

#include <sys/types.h>
#include <sys/time.h>

#include <math.h>
#include <pcap.h>
#include <pthread.h>	// TODO remove once globaldata locking is fixed
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
#include "sync_algo.h"
#include "create_stamp.h"
#include "rawdata.h"
#include "verbose.h"
#include "config_mgr.h"
#include "proto_ntp.h"	// XXX ??
#include "misc.h"
#include "jdebug.h"




/* Function to copy a stamp. Not the reverse of the origina/copy convention */
inline void copystamp(struct bidir_stamp *orig, struct bidir_stamp *copy)
{
	memcpy(copy, orig, sizeof(struct bidir_stamp));
}




/* =============================================================================
 * ALGO INITIALISATION ROUTINES
 * =============================================================================
 */


static void
set_algo_windows(struct radclock_phyparam *phyparam, struct bidir_algostate *p )
{
	index_t history_scale = 3600 * 24 * 7;		/* time-scale of top level window */

	/* top level window, must forget past */
	p->top_win = (index_t) ( history_scale / p->poll_period );

	/* shift detection. Ensure min of 100 samples (reference poll=16).
	 * TODO: right function? 
	 */
	p->shift_win = MAX( (index_t) ceil( (10*phyparam->TSLIMIT/1e-7)/p->poll_period ), 100 );

	/* offset estimation, based on SKM scale (don't allow too small) */
	p->offset_win = (index_t) MAX( (phyparam->SKM_SCALE/p->poll_period), 2 );

	/* local period, not the right function! should be # samples and time based */ 
	p->plocal_win = (index_t) MAX( ceil(p->offset_win*5), 4);
/*	p->plocal_win = (index_t) MAX( ceil(p->offset_win*4), 4);  // XXX Tuning purposes SIGCOMM */ 
}


/* Adjust window values to avoid window mismatches generating unnecessary complexity
 * This version initialises warmup_win on the first pkt, otherwise, after a parameter reload,
 * it takes the new algo windows and again increases the warmup period if necessary.
 * it will never decrease it in time to avoid problems, so there will always be more stamps to serve.
 * it will always recommend keeping existing packets so warmup history is not lost.
 * It also ensures that top_win is large enough.
 */ 
static void
adjust_warmup_win(index_t i, struct bidir_algostate *p, unsigned int plocal_winratio)
{
	index_t win;
	double WU_dur;

	verbose(VERB_CONTROL,"Adjusting Warmup window");
	
	/* Adjust warmup wrt shift, plocal, and offset windows */
	win = MAX(p->offset_win, MAX(p->shift_win, p->plocal_win + p->plocal_win/(plocal_winratio/2) ));
	// hack to avoid bug in RTT history wrapping killing final value needed for shift algo
	// No idea why win+=1 won't work, as that makes the buffer history longer than the needed shift_win
	// Without the hack I think I get it now:  RTT is pushed into history very early on, effectively updated
	// to the current stamp i already, whereas the RTThat_sh code in process_RTT_full is still
	// operating in the `i am updating for it now' mode, working with pre-update data
	win += 2;
	
	if (i==0) {
		if ( win > p->warmup_win ) {	// simplify full algo a little
			verbose(VERB_CONTROL, "Warmup window smaller than algo windows, increasing "
					"from %lu to %lu stamps", p->warmup_win, win);
			p->warmup_win = win;
		} 
	} else { // Simply add on entire new Warmup using new param: code can't fail
		WU_dur = (double) (win * p->poll_period);	// new warmup remaining [s]
		p->warmup_win = (index_t) ceil(WU_dur / p->poll_period) + i;
		verbose(VERB_CONTROL, 
				"After adjustment, %4.1lf [sec] of warmup left to serve, or %lu stamps. "
				"Warmup window now %lu", WU_dur, p->warmup_win - i, p->warmup_win);
	}

   /* Adjust top window wrt warmup and shift windows */
	/* Ensures warmup and shift windows < top_win/2 , so top win can be ignoreed in warmup */
	if ( p->warmup_win + p->shift_win > p->top_win/2 ) {
		win = 3*( (p->warmup_win + p->shift_win) * 2 + 1 ); // 3x minimum, as short history bad
		verbose(VERB_CONTROL,
				"Warmup + shift window hits history half window, increasing history "
				"window from %lu to %lu", p->top_win, win);
		p->top_win = win;
	}

	verbose(VERB_CONTROL,"Warmup Adjustment Complete");
}





/*
 * Initialization function for normal plocal algo. Resets wwidth as well as
 * finding near and far pkts * poll_period dependence:  via plocal_win
 */
static void
init_plocal(struct bidir_algostate *state, unsigned int plocal_winratio, index_t i)
{
	/* History lookup window boundaries */
	index_t lhs;
	index_t rhs;

	/* XXX Tuning ... Accept too few packets in the window makes plocal varies
	 * a lot. Let's accept more packets to increase quality.
	 * 		state->wwidth = MAX(1,plocal_win/plocal_winratio);
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



/* Initialise the error threshold.
 * This procedure is a trick to modify static variables on 
 * reception of the first packet only.
 */
static void
init_errthresholds( struct radclock_phyparam *phyparam, struct bidir_algostate *state)
{
	/* XXX Tuning history:
	 * original: 10*TSLIMIT = 150 mus
	 * 		  state->Eshift =  35*phyparam->TSLIMIT;  // 525 mus for Shouf Shouf?
	 */
	state->Eshift			= 10*phyparam->TSLIMIT;
	state->Ep					= 3*phyparam->TSLIMIT;
	state->Ep_qual			= phyparam->RateErrBOUND/5;
	state->Ep_sanity		= 3*phyparam->RateErrBOUND;
	
	/* XXX Tuning history:
	 * 		state->Eplocal_qual	= 4*phyparam->BestSKMrate;  // Original
	 * 		state->Eplocal_qual	= 4*2e-7; 		// Big Hack during TON paper
	 * 		state->Eplocal_qual	= 40*1e-7;		// Tuning for Shouf Shouf tests ??
	 * but finally introduced a new parameter in the config file
	 */
	state->Eplocal_qual		= phyparam->plocal_quality;
	state->Eplocal_sanity		= 3*phyparam->RateErrBOUND;

	/* XXX Tuning history:
	 * 		*Eoffset		= 6*phyparam->TSLIMIT;  // Original
	 * but finally introduced a new parameter in the config file
	 */
	state->Eoffset				= phyparam->offset_ratio * phyparam->TSLIMIT;

	/* XXX Tuning history:
	 * We should decouple Eoffset and Eoffset_qual ... conclusion of shouf shouf analysis 
	 * Added the effect of poll period as a first try (~ line 740) 
	 * [UPDATE] - reverted ... see below
	 */
	state->Eoffset_qual			= 3*(state->Eoffset);
	state->Eoffset_sanity_min	= 100*phyparam->TSLIMIT;
	state->Eoffset_sanity_rate	= 20*phyparam->RateErrBOUND;
}




static void
print_algo_parameters(struct radclock_phyparam *phyparam, struct bidir_algostate *state)
{
	verbose(VERB_CONTROL, "Machine Parameters:  TSLIMIT: %g, SKM_SCALE: %d, RateErrBOUND: %g, BestSKMrate: %g",
			phyparam->TSLIMIT, (int)phyparam->SKM_SCALE, phyparam->RateErrBOUND, phyparam->BestSKMrate);

	verbose(VERB_CONTROL, "Network Parameters:  poll_period: %u, h_win: %d ", state->poll_period, state->top_win);

	verbose(VERB_CONTROL, "Windows (in pkts):   warmup: %lu, history: %lu, shift: %lu "
			"(thres = %4.0lf [mus]), plocal: %lu, offset: %lu (SKM scale is %u)",
			state->warmup_win, state->top_win, state->shift_win, state->Eshift*1000000, state->plocal_win, state->offset_win,
			(int) (phyparam->SKM_SCALE/state->poll_period) );

	verbose(VERB_CONTROL, "Error thresholds :   phat:  Ep %3.2lg [ms], Ep_qual %3.2lg [PPM], "
			"plocal:  Eplocal_qual %3.2lg [PPM]", 1000*state->Ep, 1.e6*state->Ep_qual, 1.e6*state->Eplocal_qual);

	verbose(VERB_CONTROL, "                     offset:  Eoffset %3.1lg [ms], Eoffset_qual %3.1lg [ms]",
			1000*state->Eoffset, 1000*state->Eoffset_qual);

	verbose(VERB_CONTROL, "Sanity Levels:       phat  %5.3lg, plocal  %5.3lg, offset: "
			"absolute:  %5.3lg [ms], rate: %5.3lg [ms]/[sec] ( %5.3lg [ms]/[stamp])",
		   	state->Ep_sanity, state->Eplocal_sanity, 1000*state->Eoffset_sanity_min,
			1000*state->Eoffset_sanity_rate, 1000*state->Eoffset_sanity_rate*state->poll_period);
}





/* Peer initialisation */
void init_state( struct radclock_handle *handle, struct radclock_phyparam *phyparam,
				struct bidir_algostate *state, struct radclock_data *rad_data,
				struct bidir_stamp *stamp, unsigned int plocal_winratio, int poll_period)
{

	vcounter_t RTT;			// current RTT value
	double th_naive;		// thetahat naive estimate

	verbose(VERB_SYNC, "Initialising RADclock synchronization");

	state->warmup_win = 100;

	/* Initialise the state polling period to the configuration value
	* poll_transition_th: begin with no transition for thetahat
	* poll_changed_i: index of last change
	*/
	state->poll_period = poll_period;
	state->poll_transition_th = 0;
	state->poll_ratio = 1;
	state->poll_changed_i = 0;

	state->next_RTThat = 0;
	state->RTThat_shift = 0;

	state->plocalerr = 0;

	/* UPDATE The following was extracted from the block related to first packet
	 * and reaction to poll period and external environment parameters
	 */

	/* Initialize the error thresholds */
	init_errthresholds( phyparam, state );

	/* Set pkt-index algo windows.
	 * These control the algo, independent of implementation.
	 */
	set_algo_windows( phyparam, state);

	/* Ensure warmup_win consistent with algo windows for easy 
	 * initialisation of main algo after warmup 
	 */
	adjust_warmup_win(state->stamp_i, state, plocal_winratio);

	/* Create histories
	 * Set all history sizes to warmup window size
	 * note: currently stamps [_end,i-1] in history, i not yet processed
	 */
	history_init(&state->stamp_hist, (unsigned int) state->warmup_win, sizeof(struct bidir_stamp) );
	history_init(&state->RTT_hist, (unsigned int) state->warmup_win, sizeof(vcounter_t) );
	history_init(&state->RTThat_hist, (unsigned int) state->warmup_win, sizeof(vcounter_t) );
	history_init(&state->thnaive_hist, (unsigned int) state->warmup_win, sizeof(double) );

	/* Print out summary of parameters:
	 * physical, network, thresholds, and sanity 
	 */
	print_algo_parameters( phyparam, state );

	/* Initialise state stamp to default value */
	memset(&state->stamp, 0, sizeof(struct bidir_stamp));	// previous, not current, stamp

	/* Print the first timestamp tuple obtained */
	verbose(VERB_SYNC, "i=%lu: Beginning Warmup Phase. Stamp read check: %llu %22.10Lf %22.10Lf %llu",
			state->stamp_i, stamp->Ta, stamp->Tb, stamp->Te, stamp->Tf);

	verbose(VERB_SYNC, "i=%lu: Assuming 1Ghz oscillator, 1st vcounter stamp is %5.3lf [days] "
			"(%5.1lf [min]) since reset, RTT is %5.3lf [ms], SD %5.3Lf [mus]",
			state->stamp_i,
			(double) stamp->Ta * 1e-9/3600/24, (double) stamp->Ta * 1e-9/60, 
			(double) (stamp->Tf - stamp->Ta) * 1e-9*1000, (stamp->Te - stamp->Tb) * 1e6);


	/* MinET_old
	 * Initialise to 0 on first packet (static variable) 
	 */
	state->minET = 0;

	/* Record stamp 0 */
	history_add(&state->stamp_hist, state->stamp_i, stamp);

	/* RTT */
	RTT = MAX(1,stamp->Tf - stamp->Ta);
	state->RTThat = RTT;
	history_add(&state->RTT_hist, state->stamp_i, &RTT);
	state->pstamp_i = 0;

	/* vcount period and clock definition.
	 * Once determined, K only altered to correct phat changes
	 * note: phat unavailable after only 1 stamp, use config value, rough guess of 1Ghz beats zero!
	 * XXX: update comment, if reading first data from kernel timecounter /
	 * clocksource info. If the user put a default value in the config file,
	 * trust his choice. Otherwise, use kernel info from the first
	 * ffclock_getestimate.
	 */
	if (handle->conf->phat_init == DEFAULT_PHAT_INIT)	// ie if no user override
		// FIXME: kernel value not yet extracted, rad_data still blank, need to revisit all this
		// state->phat = rad_data->phat;
		state->phat = handle->conf->phat_init;		// trust conf based value for now
	else
		state->phat = handle->conf->phat_init;
	state->perr = 0;
	state->plocal = state->phat;

	/* switch off pstamp_i search until initialised at top_win/2 */
	state->jcount = 1;
	state->jsearch_win = 0;

	/* Initializations for phat warmup algo 
	 * wwidth: initial width of end search windows. 
	 * near_i: index of stamp with minimal RTT in near window  (larger i)
	 * far_i: index of stamp with minimal RTT in  far window  (smaller i)
	 */
	state->wwidth = 1;
	state->near_i = 0;
	state->far_i  = 0;

	/* K now determined.  For now C(t) = t_init */
	state->K = stamp->Tb - (long double) (stamp->Ta * state->phat);
	verbose(VERB_SYNC, "i=%lu: After initialisation: (far,near) = (%lu,%lu), "
			"phat = %.10lg, perr = %5.3lg, K = %7.4Lf",
			state->stamp_i, 0, 0, state->phat, state->perr, state->K);


	/* thetahat algo initialise on-line warmup algo */
	th_naive = 0;
	state->thetahat = th_naive;
	history_add(&state->thnaive_hist, state->stamp_i, &th_naive);

	/* Peer error metrics */
	PEER_ERROR(state)->Ebound_min_last	= 0;
	PEER_ERROR(state)->error_bound	= 0;
	PEER_ERROR(state)->nerror 			= 0;
	PEER_ERROR(state)->cumsum 			= 0;
	PEER_ERROR(state)->sq_cumsum 		= 0;
	PEER_ERROR(state)->nerror_hwin 		= 0;
	PEER_ERROR(state)->cumsum_hwin 		= 0;
	PEER_ERROR(state)->sq_cumsum_hwin 	= 0;

	/* Peer statistics */
	state->stats_sd[0] = 0;
	state->stats_sd[1] = 0;
	state->stats_sd[2] = 0;
}


void update_state(struct bidir_algostate *state, struct radclock_phyparam *phyparam, int poll_period,
					unsigned int plocal_winratio )
{
	index_t stamp_sz;			// Stamp max history size
	index_t RTT_sz;				// RTT max history size
	index_t RTThat_sz;			// RTThat max history size
	index_t thnaive_sz;			// thnaive max history size
	index_t st_end;				// index of last pkts in stamp history
	index_t RTT_end;			// index of last pkts in RTT history
	index_t RTThat_end;			// index of last pkts in RTThat history
	index_t thnaive_end;		// index of last pkts in thnaive history
	vcounter_t *RTThat_tmp;		// RTT hat value holder

	/* Initialize the error thresholds */
	init_errthresholds( phyparam, state );

	/* Record change of poll period.
	 * Set poll_transition_th, poll_ratio and poll_changed_i for thetahat
	 * processing. poll_transition_th could be off by one depending on when NTP
	 * pkt rate changed, but not important.
	 */
	if ( poll_period != state->poll_period )
	{
		state->poll_transition_th = state->offset_win;
		state->poll_ratio = (double)poll_period / (double)state->poll_period;
		state->poll_period = poll_period;
		state->poll_changed_i = state->stamp_i;
	}

	/* Set pkt-index algo windows.
	 * With possibly new phyparam and/or polling period
	 * These control the algo, independent of implementation.
	 */
	set_algo_windows(phyparam, state);

	/* Ensure warmup_win consistent with algo windows for easy 
	 * initialisation of main algo after warmup 
	 */
	if (state->stamp_i < state->warmup_win)
		adjust_warmup_win(state->stamp_i, state, plocal_winratio);
	else 
	{
		/* Re-init shift window from stamp i-1 back
		 * ensure don't go past 1st stamp, using history constraint (follows 
		 * that of RTThat already tracked). Window was reset, not 'slid'
	 	 * note: currently stamps [_end,i-1] in history, i not yet processed
		 *
		 */
		// Former code which requires to cast into signed variables. This macro
		// is dangerous because it does not check the sign of the variables (if
		// unsigned rolls over, it gives the wrong result
		//state->shift_end = MAX(0, (state->stamp_i-1) - state->shift_win+1);
		if ( (state->stamp_i-1) > (state->shift_win-1) )
			state->shift_end = (state->stamp_i-1) - (state->shift_win-1);
		else
			state->shift_end = 0;

		RTT_end = history_end(&state->RTT_hist);
		state->shift_end = MAX(state->shift_end, RTT_end);
		RTThat_tmp = history_find(&state->RTT_hist, history_min(&state->RTT_hist, state->shift_end, state->stamp_i-1) );
		state->RTThat_shift = *RTThat_tmp;
	}

	/* Set history array sizes.
	 * If warmup is to be sacred, each must be larger than the current warmup_win 
	 * NOTE:  if set right, new histories will be big enough for future needs, 
	 * doesn't mean required data is in them after window resize!!
	 */
	if ( state->stamp_i < state->warmup_win ) {
		stamp_sz 	= state->warmup_win;
		RTT_sz 		= state->warmup_win;
		RTThat_sz 	= state->warmup_win;
		thnaive_sz	= state->warmup_win;
	}
	else {
		stamp_sz 	= MAX(state->plocal_win + state->plocal_win/(plocal_winratio/2), state->offset_win);
		RTT_sz 		= MAX(state->plocal_win + state->plocal_win/(plocal_winratio/2), MAX(state->offset_win, state->shift_win));
		RTThat_sz 	= state->offset_win;				// need >= offset_win
		thnaive_sz 	= state->offset_win;				// need >= offset_win
	}

	/* Resize histories if needed.
	 * Note: currently stamps [_end,stamp_i-1] in history, stamp_i not yet processed, so all
	 * global index based on stamp_i-1.
	 */

	/* Resize Stamp History */
	if ( state->stamp_hist.buffer_sz != stamp_sz )
	{
		st_end = history_end(&state->stamp_hist);
		verbose(VERB_CONTROL, "Resizing st_win history from %lu to %lu. Current stamp range is [%lu %lu]",
			state->stamp_hist.buffer_sz, stamp_sz, st_end, state->stamp_i-1);
		history_resize(&state->stamp_hist, stamp_sz, state->stamp_i-1);
		st_end = history_end(&state->stamp_hist);
		verbose(VERB_CONTROL, "Range on exit: [%lu %lu]", st_end, state->stamp_i-1);
	}

	/* Resize RTT History */
	if ( state->RTT_hist.buffer_sz != RTT_sz )
	{
		RTT_end = history_end(&state->RTT_hist);
		verbose(VERB_CONTROL, "Resizing RTT_win history from %lu to %lu. Current stamp range is [%lu %lu]", 
			state->RTT_hist.buffer_sz, RTT_sz, RTT_end, state->stamp_i-1);
		history_resize(&state->RTT_hist, RTT_sz, state->stamp_i-1);
		RTT_end = history_end(&state->RTT_hist);
		verbose(VERB_CONTROL, "Range on exit: [%lu %lu]", RTT_end, state->stamp_i-1);
	}

	/* Resize RTThat History */
	if ( state->RTThat_hist.buffer_sz != RTThat_sz )
	{
		RTThat_end = history_end(&state->RTThat_hist);
		verbose(VERB_CONTROL, "Resizing RTThat_win history from %lu to %lu. Current stamp range is [%lu %lu]", 
				state->RTThat_hist.buffer_sz, RTThat_sz, RTThat_end, state->stamp_i-1);
		history_resize(&state->RTThat_hist, RTThat_sz, state->stamp_i-1);
		RTThat_end = history_end(&state->RTThat_hist);
		verbose(VERB_CONTROL, "Range on exit: [%lu %lu]", RTThat_end, state->stamp_i-1);
	}

	/* Resize Thnaive History */
	if ( state->thnaive_hist.buffer_sz != thnaive_sz ) {
		thnaive_end = history_end(&state->thnaive_hist);
		verbose(VERB_CONTROL, "Resizing thnaive_win history from %lu to %lu.  Current stamp range is [%lu %lu]",
				state->thnaive_hist.buffer_sz, thnaive_sz, thnaive_end, state->stamp_i-1);
		history_resize(&state->thnaive_hist, thnaive_sz, state->stamp_i-1);
		thnaive_end = history_end(&state->thnaive_hist);
		verbose(VERB_CONTROL, "Range on exit: [%lu %lu]", thnaive_end, state->stamp_i-1);
	}

	/* Print out summary of parameters:
	 * physical, network, thresholds, and sanity 
	 */
	print_algo_parameters( phyparam, state );


}




/* RTT Initializations for normal history and level_shift code */
void end_warmup_RTT( struct bidir_algostate *state, struct bidir_stamp *stamp)
{
	index_t RTT_end;			// index of last pkts in RTT history
	vcounter_t *RTT_tmp;		// RTT hat value holder

	/* Start tracking next_RTThat instead of in middle of 1st window, even if it
	 * is not used until then.
	 */
	state->next_RTThat = state->RTThat;

	/* Start tracking shift_end and first RTThat_shift estimate.
	 * shift_end = stamp_i-(shift_win-1) should work all the time, but some
	 * extra care does not harm. We also make sure shift_end corresponds to a
	 * stamp that is actually stored in history.
	 */
	if ( state->stamp_i > state->shift_win )
		state->shift_end = state->stamp_i - (state->shift_win-1);
	else
		state->shift_end = 0;
		
	RTT_end = history_end(&state->RTT_hist);
	state->shift_end = MAX(state->shift_end, RTT_end);

	RTT_tmp = history_find(&state->RTT_hist, history_min(&state->RTT_hist, state->shift_end, state->stamp_i) );
	state->RTThat_shift  = *RTT_tmp;
}



/* Initializations for normal phat algo
 * Need: reliable phat, estimate of its error, initial quality pstamp and associated data
 * Approximate error of current estimate from warmup algo, must be beaten before phat updated
 */
void end_warmup_phat(struct bidir_algostate *state, struct bidir_stamp *stamp)
{
	struct bidir_stamp *stamp_ptr;
	struct bidir_stamp *stamp_near;
	struct bidir_stamp *stamp_far;
	vcounter_t *RTT_far;
	vcounter_t *RTT_near;

	/* pstamp_i has been tracking RTThat during warmup, which is the stamp we
	 * choose to initialise pstamp to.
	 * This first pstamp is supposed to be 'perfect'. Error due to poor RTT 
	 * estimate will be picked in `global' error component baseerr.
	 * Note: the RTThat estimate associated to pstamp is the current RTThat and
	 * *not* the one in use at the time pstamp_i was last recorded. They should
	 * be the same, no?
	 */
	stamp_ptr = history_find(&state->stamp_hist, state->pstamp_i);
	copystamp(stamp_ptr, &state->pstamp);
	state->pstamp_perr 	= 0;
	state->pstamp_RTThat = state->RTThat;

	/* RTThat detection and last phat update may not correspond to the same
	 * stamp. Here we have reliable phat, but the corresponding point error may
	 * be outdated if RTThat is detected after last update. Reassess point 
	 * error with latest RTThat value.
	 */
	stamp_near	= history_find(&state->stamp_hist, state->near_i);
	stamp_far	= history_find(&state->stamp_hist, state->far_i);
	RTT_near		= history_find(&state->RTT_hist, state->near_i);
	RTT_far		= history_find(&state->RTT_hist, state->far_i);
	state->perr	= ((double)(*RTT_far + *RTT_near) - 2*state->RTThat)
						* state->phat / (stamp_near->Tb - stamp_far->Tb);

	/* Reinitialise sanity count at the end of warmup */
	state->phat_sanity_count = 0;
	verbose(VERB_CONTROL, "i=%lu: Initializing full phat algo, pstamp_i = %lu, perr = %6.3lg",
		   	state->stamp_i, state->pstamp_i, state->perr);
}


/* Initializations for normal plocal algo
 * [may not be enough history even in warmup, poll_period will only change timescale]
 */
void
end_warmup_plocal(struct bidir_algostate *state, struct bidir_stamp *stamp,
		unsigned int plocal_winratio)
{
	index_t st_end;				// indices of last pkts in stamp history
	index_t RTT_end;			// indices of last pkts in RTT history

	/*
	 * Index of stamp we require to be available before proceeding (different
	 * usage to shift_end etc!)
	 */
	state->plocal_end = state->stamp_i - state->plocal_win + 1 -
			state->wwidth-state->wwidth / 2;
	st_end  = history_end(&state->stamp_hist);
	RTT_end = history_end(&state->RTT_hist);
	if (state->plocal_end >= MAX(state->poll_changed_i, MAX(st_end, RTT_end))) {
		/* if fully past poll transition and have history read
		 * resets wwidth as well as finding near and far pkts
		 */
		init_plocal(state, plocal_winratio, state->stamp_i);
	}
	else {
		/* record a problem, will have to restart when it resolves */
		state->plocal_problem = 1;
		verbose(VERB_CONTROL, "i=%lu:  plocal problem following parameter "
				"changes (desired window first stamp %lu unavailable), "
				"defaulting to phat while windows fill", state->stamp_i,
				state->plocal_end);
	}
	state->plocal_sanity_count = 0;
}


/* Initialisation for normal thetahat algo */
void end_warmup_thetahat(struct bidir_algostate *state, struct bidir_stamp *stamp)
{
	index_t RTThat_sz;		// RTThat max history size
	vcounter_t *RTThat_tmp;		// RTT hat value holder
	int j;

	/* fill entire RTThat history with current RTThat not a true history, but
	 * appropriate for offset
	 */
	RTThat_sz = state->RTThat_hist.buffer_sz;
	for ( j=0; j<RTThat_sz; j++ )
	{
		RTThat_tmp = history_find(&state->RTThat_hist, j);
		*RTThat_tmp = state->RTThat;
	}
	state->RTThat_hist.item_count = RTThat_sz;
	state->RTThat_hist.curr_i = state->stamp_i;

	state->offset_sanity_count = 0;
	state->offset_quality_count = 0;
// XXX TODO we should probably track the correct stamp in warmup instead of this
// bogus one ...
	copystamp(stamp, &state->thetastamp);
	verbose(VERB_CONTROL, "i=%lu: Switching to full thetahat algo, RTThat_hist "
			"set to RTThat=%llu, current est'd minimum error= %5.3lg [ms]", 
			state->stamp_i, state->RTThat, 1000*state->minET);
}


/* Adjust parameters based on nearby/far away server.
 * Is adhoc, and ugly in that can overwrite values which should have been set
 * correctly already, in particular those based off phyparams. In a way an
 * attempt to define, again, excel/good/poor idea, but for path, not the host.
 * But some things do depend on server behaviour and path characteristics,
 * makes sense to recalibrate what we can after warmup, eg if path sucks,
 * need to be less fussy with plocal.  The list of such will no doubt grow.
 * TODO: do this more systematically at some point.
 */
void parameters_calibration( struct bidir_algostate *state)
{
	/* Near server = <3ms away */
	if ( state->RTThat < (3e-3 / state->phat) ) {
		verbose(VERB_CONTROL, "i=%lu: Detected close server based on minimum RTT", state->stamp_i);
		/* make RTThat_shift_thres constant to avoid possible phat dynamics */
		state->RTThat_shift_thres = (vcounter_t) ceil( state->Eshift/state->phat );
		/* Decoupling Eoffset and Eoffset_qual .. , for nearby servers, the
		 * looser quality has no (or very small?) effect
		 */
		state->Eoffset_qual = 3 * state->Eoffset;
	}
	else {
		verbose(VERB_CONTROL, "i=%lu: Detected far away server based on minimum RTT", state->stamp_i);
		state->RTThat_shift_thres = (vcounter_t) ceil( 3*state->Eshift/state->phat );

		/* Decoupling Eoffset and Eoffset_qual .. , for far away servers, increase the number
		 * of points accepted for processing. The bigger the pool period the looser we
		 * have to be. Provide here a rough estimate of the increase of the window based
		 * on data observed with sugr-glider and shouf shouf plots
		 * 		Eoffset_qual =  exp(log((double) poll_period)/2)/2 * 3 * Eoffset;
		 * [UPDATE]: actually doesn't work because the gaussian penalty function makes these 
		 * additional points be insignificant. Reverted back
		 */
		state->Eoffset_qual = 6 * state->Eoffset;
		state->Eplocal_qual = 2 * state->Eplocal_qual;  // adding path considerations to local physical ones
	}

	verbose(VERB_CONTROL, "i=%lu:   Adjusted Eoffset_qual %3.1lg [ms] (Eoffset %3.1lg [ms])",
			state->stamp_i, 1000*state->Eoffset_qual, 1000*state->Eoffset);
	verbose(VERB_CONTROL, "i=%lu:   Adjusted Eplocal_qual %4.2lg [PPM]",
			state->stamp_i, 1000000*state->Eplocal_qual);

	verbose(VERB_CONTROL, "i=%lu:   Upward shift detection activated, "
			"threshold set at %llu [vcounter] (%4.0lf [mus])",
			state->stamp_i, state->RTThat_shift_thres,
			state->RTThat_shift_thres * state->phat*1000000);
}


void collect_stats_state(struct bidir_algostate *state, struct bidir_stamp *stamp)
{
	long double SD;	
	SD = stamp->Te - stamp->Tb;

	/*
	 * Fairly ad-hoc values based on observed servers. Good if less than 
	 * 100 us, avg if less than 300 us, bad otherwise.
	 */
	if (SD < 50e-6)
		state->stats_sd[0]++;
	else if (SD < 200e-6)
		state->stats_sd[1]++;
	else 
		state->stats_sd[2]++;
}


void print_stats_state(struct bidir_algostate *state)
{
	int total_sd;
	long basediff;

	/* 
	 * Most of the variables needed for these stats are not used during warmup
	 */
	if (state->stamp_i < state->warmup_win) {
		state->stats_sd[0] = 0;
		state->stats_sd[1] = 0;
		state->stats_sd[2] = 0;
		return;
	}

	if (state->stamp_i % (int)(6 * 3600 / state->poll_period))
		return;
	
	total_sd = state->stats_sd[0] + state->stats_sd[1] + state->stats_sd[2];

	verbose(VERB_CONTROL, "i=%lu: Server recent statistics:", state->stamp_i);
	verbose(VERB_CONTROL, "i=%lu:   Internal delay: %d%% < 50us, %d%% < 200us, %d%% > 200us",
		state->stamp_i,
		100 * state->stats_sd[0]/total_sd,
		100 * state->stats_sd[1]/total_sd,
		100 * state->stats_sd[2]/total_sd);

	verbose(VERB_CONTROL, "i=%lu:   Last stamp check: %llu %22.10Lf %22.10Lf %llu",
		state->stamp_i,
		state->stamp.Ta, state->stamp.Tb, state->stamp.Te, state->stamp.Tf);

	verbose(VERB_SYNC, "i=%lu: Timekeeping summary:",
		state->stamp_i);
	verbose(VERB_SYNC, "i=%lu:   Period = %.10g, Period error = %.10g",
		state->stamp_i, state->phat, state->perr);
		
	if (state->RTThat > state->pstamp_RTThat)
		basediff = state->RTThat - state->pstamp_RTThat;
	else
		basediff = state->pstamp_RTThat - state->RTThat;
		
	/* TODO check near and far what we want */
	verbose(VERB_SYNC, "i=%lu:   Tstamp pair = (%lu,%lu), base err = %.10g, "
		"DelTb = %.3Lg [hrs]",
		state->stamp_i, state->far_i, state->near_i,
		state->phat * basediff,
		(state->stamp.Tb - state->pstamp.Tb)/3600);

	verbose(VERB_SYNC, "i=%lu:   Thetahat = %5.3lf [ms], minET = %.3lf [ms], "
		"RTThat = %.3lf [ms]", 
		state->stamp_i,
		1000 * state->thetahat,
		1000 * state->minET,
		1000 * state->phat * state->RTThat);

	/* Reset stats for this period */
	state->stats_sd[0] = 0;
	state->stats_sd[1] = 0;
	state->stats_sd[2] = 0;
}



/* =============================================================================
 * RTT
 * =============================================================================
 */

void
process_RTT_warmup(struct bidir_algostate *state, vcounter_t RTT)
{
	/* Rough threshold during warmup TODO: set this up properly */
//	if (state->stamp_i == 10) {		// prior to this is zero I think
//		state->RTThat_shift_thres = (vcounter_t) ceil(state->Eshift/4/state->phat);
//		verbose(VERB_SYNC, "i=%lu: Restricting alerts for downward shifts exceeding %3.0lf [mus]",
//				state->stamp_i, 1.e6 * state->Eshift/3);
//	}

	/* Downward Shifts.
	 * Informational only, reaction is automatic and already achieved.
	 * Shift location is simply the current stamp.
	 */
//	if ( state->RTThat > (RTT + state->RTThat_shift_thres) ) {
	if ( state->RTThat > RTT ) {		// flag all shifts, no matter how small
		verbose(VERB_SYNC, "i=%lu: Downward shift of %5.1lf [mus]", state->stamp_i,
				(state->RTThat - RTT) * state->phat * 1.e6);
	}
	
	/* Record the minimum of RTT
	 * Record corresponding index for full phat processing */
	if (RTT < state->RTThat) {
		state->RTThat = RTT;
		state->pstamp_i = state->stamp_i;
	}
	
	
	/* Upward Shifts.  Information only.
	 * This checks for detection over window of width shift_win prior to stamp i
	 * lastshift is the index of first known stamp after shift
	 */
//	if ( state->RTThat_shift > (state->RTThat + state->RTThat_shift_thres) ) {
//		lastshift = state->stamp_i - state->shift_win + 1;
//		verbose(VERB_SYNC, "Upward shift of %5.1lf [mus] triggered when i = %lu ! "
//				"shift detected at stamp %lu",
//				(state->RTThat_shift - state->RTThat)*state->phat*1.e6,
//				state->stamp_i, lastshift);

}



/* Normal RTT updating.
 * This processes the new RTT=RTT_hist[i] of stamp i
 * Algos always simple: History transparently handled before stamp i
 * shifts below after normal i
 * - RTThat always below or equal RTThat_shift since top_win/2 > shift_win
 * - next_RTThat above or below RTThat_shift depending on position in history
 * - RTThat_end tracks last element stored
 */
void
process_RTT_full(struct bidir_algostate *state, struct radclock_data *rad_data, vcounter_t RTT)
{
	index_t lastshift = 0;	//  index of first stamp after last detected upward shift 
	index_t j;				// loop index, needs to be signed to avoid problem when j hits zero
	index_t jmin = 0;		// index that hits low end of loop
	vcounter_t* RTThat_ptr;	// points to a given RTThat in RTThat_hist
	vcounter_t next_RTT, last_RTT;

	last_RTT = state->RTThat;
	state->RTThat = MIN(state->RTThat, RTT);
	history_add(&state->RTThat_hist, state->stamp_i, &state->RTThat);
	state->next_RTThat = MIN(state->next_RTThat, RTT);

	/* if window (including processing of i) is not full,
	 * keep left hand side (ie shift_end) fixed and add pkt i on the right.
	 * Otherwise, window is full, and min inside it (thanks to reinit), can slide
	 */
	// MAX not safe with subtracting unsigned
//	if ( MAX(0, state->stamp_i - state->shift_win+1) < state->shift_end ) {
	if ( state->stamp_i < (state->shift_win-1) + state->shift_end )
	{
		verbose(VERB_CONTROL, "In shift_win transition following window change, "
				"[shift transition] windows are [%lu %lu] wide", 
				state->shift_win, state->stamp_i-state->shift_end+1);
		state->RTThat_shift =  MIN(state->RTThat_shift,RTT);
	}
	else
	{
		 state->RTThat_shift = history_min_slide_value(&state->RTT_hist, state->RTThat_shift, state->shift_end, state->stamp_i-1);
		 state->shift_end++;
	}

	/* Upward Shifts.
	 * This checks for detection over window of width shift_win prior to stamp i 
	 * Detection about reaction to RTThat_shift. RTThat_shift itself is simple,
	 * always just a sliding window.
	 * lastshift is the index of first known stamp after shift
	 */
	if ( state->RTThat_shift > (state->RTThat + state->RTThat_shift_thres) ) {
		lastshift = state->stamp_i - state->shift_win + 1;
		verbose(VERB_SYNC, "i=%lu: Upward shift of %5.1lf [mus], detected at stamp %lu",
				state->stamp_i, (state->RTThat_shift - state->RTThat)*state->phat*1.e6, lastshift);
				// DV:  bug?  is stamp_i my "i" ? in fact triggered at stamp_i + 1 ?
	
		/* Recalc from [i-lastshift+1 i]
		 * - note by design, won't run into last history change 
		 */
		state->RTThat = state->RTThat_shift;
		state->next_RTThat = state->RTThat;

		/* Recalc necessary for phat
		 * - note pstamp_i must be before lastshift by design
		 * - note that phat not the same as before, but that's ok
		 */
		if ( state->next_pstamp_i >= lastshift) {
			next_RTT = state->next_pstamp.Tf - state->next_pstamp.Ta;
			verbose(VERB_SYNC, "        Recalc necessary for RTT state recorded at"
				" next_pstamp_i = %lu", state->next_pstamp_i);
			state->next_pstamp_perr = state->phat*((double)next_RTT - state->RTThat);
			state->next_pstamp_RTThat = state->RTThat;
		} else
			verbose(VERB_SYNC, "        Recalc not need for RTT state recorded at"
				"next_pstamp_i = %lu", state->next_pstamp_i);


		/* Recalc necessary for offset 
		 * typically shift_win >> offset_win, so lastshift won't bite
		 * correct RTThat history back as far as necessary or possible
		 */
		// MAX not safe with subtracting unsigned
		//for ( j=state->stamp_i; j>=MAX(lastshift,state->stamp_i-state->offset_win+1); j--)
		if ( state->stamp_i > (state->offset_win-1) )
			jmin = state->stamp_i - (state->offset_win-1);
		else
			jmin = 0;
		jmin = MAX(lastshift, jmin);

		for ( j=state->stamp_i; j>=jmin; j--) {
			RTThat_ptr = history_find(&state->RTThat_hist, j);
			*RTThat_ptr = state->RTThat;
		}
		verbose(VERB_SYNC, "i=%lu: Recalc necessary for RTThat for %lu stamps back to i=%lu",
				state->stamp_i, state->shift_win, lastshift);
		ADD_STATUS(rad_data, STARAD_RTT_UPSHIFT);
	} else
		DEL_STATUS(rad_data, STARAD_RTT_UPSHIFT);

	/* Downward Shifts.
	 * Informational only, reaction is automatic and already achieved.
	 * Shift location is simply the current stamp.
	 * TODO: establish a dedicated downward threshold parameter and setting code
	 */
	if ( last_RTT > (state->RTThat + (state->RTThat_shift_thres)/4) ) {
		verbose(VERB_SYNC, "i=%lu: Downward shift of %5.1lf [mus]", state->stamp_i,
				(last_RTT - state->RTThat) * state->phat * 1.e6);
	}

}







/* =============================================================================
 * PHAT ALGO 
 * =============================================================================
 */

double compute_phat (struct bidir_algostate* state,
		struct bidir_stamp* far, struct bidir_stamp* near)
{
	long double DelTb, DelTe;	// Server time intervals between stamps j and i
	vcounter_t DelTa, DelTf;	// Counter intervals between stamps j and i 
	double phat;					// Period estimate for current stamp
	double phat_b;					// Period estimate for current stamp (backward dir)
	double phat_f; 				// Period estimate for current stamp (forward dir)

	DelTa = near->Ta - far->Ta;
	DelTb = near->Tb - far->Tb;
	DelTe = near->Te - far->Te;
	DelTf = near->Tf - far->Tf;

	/* 
	 * Check for crazy values, and NaN cases induced by DelTa or DelTf equal
	 * zero Log a major error and hope someone will call us
	 */
	if ( ( DelTa <= 0 ) || ( DelTb <= 0 ) || (DelTe <= 0 ) || (DelTf <= 0) ) {
		verbose(LOG_ERR, "i=%lu we picked up the same i and j stamp. "
				"Contact developer.", state->stamp_i);
		return 0;
	}

	/*
	 * Use naive estimates from chosen stamps {i,j}
	 * forward  (OUTGOING, sender)
	 * backward (INCOMING, receiver)
	 */
	phat_f	= (double) (DelTb / DelTa);
	phat_b	= (double) (DelTe / DelTf);
	phat	= (phat_f + phat_b) / 2;

	return phat;
}




int process_phat_warmup (struct bidir_algostate* state, vcounter_t RTT,
		unsigned int warmup_winratio)
{
	vcounter_t *RTT_tmp;		// RTT value holder
	vcounter_t *RTT_far;		// RTT value holder
	vcounter_t *RTT_near;		// RTT value holder
	struct bidir_stamp *stamp_near;
	struct bidir_stamp *stamp_far;
	long double DelTb; 		// Server time intervals between stamps j and i
	double phat;			// Period estimate for current stamp

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
	 * 		find near_i by sliding along one on RHS
	 * else
	 * 		increase near and far windows by 1, find index of new min RTT in 
	 * 		both, increase window width
	*/

	if ( state->stamp_i%warmup_winratio ) {
		state->near_i = history_min_slide(&state->RTT_hist, state->near_i,
				state->stamp_i - state->wwidth, state->stamp_i - 1);
	}
	else {
		RTT_tmp  = history_find(&state->RTT_hist, state->wwidth);
		RTT_near = history_find(&state->RTT_hist, state->near_i);
		RTT_far  = history_find(&state->RTT_hist, state->far_i);
		if ( *RTT_tmp < *RTT_far )
			state->far_i = state->wwidth;
		if ( RTT < *RTT_near )
			state->near_i = state->stamp_i;
		state->wwidth++;
	}

	/* Compute time intervals between NTP timestamps of selected stamps */
	stamp_near = history_find(&state->stamp_hist, state->near_i);
	stamp_far  = history_find(&state->stamp_hist, state->far_i);

	phat = compute_phat(state, stamp_far, stamp_near);
	if ( phat == 0 )
	{
		/* Something bad happen, most likely, we have a bug. The algo may
		 * recover from this, so do not update and keep going.
		 */
		return 1;
	}

	/* Clock correction
	 * correct K to keep C(t) continuous at time of last stamp
	 */
//	if ( state->phat != phat ) {
	if ( (near_i != state->near_i) || (far_i != state->far_i) )
	{
		state->K += state->stamp.Ta * (long double) (state->phat - phat);
		verbose(VERB_SYNC, "i=%lu: phat update (far,near) = (%lu,%lu), "
				"phat = %.10g, rel diff = %.10g, perr = %.10g, K = %7.4Lf",
				state->stamp_i, state->far_i, state->near_i,
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


/* on-line calculation of new pstamp_i
 * If we are still in the jsearch window attached to start of current half
 * top_win then record this stamp if it is of better quality.
 * Record [index RTT RTThat point-error stamp ]
 * Only track and record the value that will be used in the next top_win/2
 * window it is NOT used for computing phat with the current stamp.
 */
void record_packet_j (struct bidir_algostate* state, vcounter_t RTT,
		struct bidir_stamp* stamp)
{
	vcounter_t next_RTT;
	next_RTT = state->next_pstamp.Tf - state->next_pstamp.Ta;

	if ( state->jcount <= state->jsearch_win )
	{
		state->jcount++;
		if ( RTT < next_RTT ) {
			state->next_pstamp_i 	= state->stamp_i;
			state->next_pstamp_RTThat 	= state->RTThat;
			state->next_pstamp_perr 	= state->phat * ((double)(RTT) - state->RTThat);
			copystamp(stamp, &state->next_pstamp);
		}
	}
}


/* Return value `ret' allows signal to be sent to plocal algo that phat sanity
 * triggered on This stamp.
 */
int process_phat_full(struct bidir_algostate* state, struct radclock_data *rad_data,
	struct radclock_phyparam *phyparam, vcounter_t RTT,
	struct bidir_stamp* stamp, int qual_warning)
{
	int ret;
	long double DelTb; 	// Server time interval between stamps j and i
	double phat;		// Period estimate for current stamp
	double perr_ij;		// Estimate of error of phat using given stamp pair [i,j]
	double perr_i;		// Estimate of error of phat at stamp i
	double baseerr;		// Holds difference in quality RTTmin values at
						// different stamps

	ret = 0;
	DEL_STATUS(rad_data, STARAD_PHAT_UPDATED);		// typical case if no update

	/* Compute new phat based on pstamp and current stamp */
	phat = compute_phat(state, &state->pstamp, stamp);
	if ( phat == 0 )
	{
		/* Something bad happen, most likely, we have a bug. The algo may
		 * recover from this, so do not update and keep going.
		 */
		return 1;
	}

	/*
	 * Determine if quality of i sufficient to bother, if so, if (j,i)
	 * sufficient to update phat
	 * if error smaller than Ep, quality pkt, proceed, else do nothing
	 */
	perr_i = state->phat * ((double)(RTT) - state->RTThat);

	if ( perr_i >= state->Ep )
		return 0;

	/*
	 * Point errors (local)
	 * Level shifts (global)  (can also correct for error in RTThat assuming no true shifts)
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


	/* Candidate accepted based on quality, record this */
	ADD_STATUS(rad_data, STARAD_PHAT_UPDATED);
	state->perr = perr_ij;

	if ( fabs((phat - state->phat)/phat) > phyparam->RateErrBOUND/3 ) {
		verbose(VERB_SYNC, "i=%lu: Jump in phat update, phat stats: (j,i)=(%lu,%lu), "
			"rel diff = %.10g, perr = %.3g, baseerr = %.10g, DelTb = %5.3Lg [hrs]",
			state->stamp_i, state->pstamp_i, state->stamp_i, (phat - state->phat)/phat,
			state->perr, baseerr, DelTb/3600);
	}

	/* Clock correction and phat update.
	 * Sanity check applies here 
	 * correct K to keep C(t) continuous at time of last stamp
	 */
	if ((fabs(state->phat - phat)/state->phat > state->Ep_sanity) || qual_warning) {
		if (qual_warning)
			verbose(VERB_QUALITY, "i=%lu: qual_warning received, following "
					"sanity check for phat", state->stamp_i);

		verbose(VERB_SANITY, "i=%lu: phat update fails sanity check. "
			"phat stats: (j,i)=(%lu,%lu), "
			"rel diff = %.10g, perr = %.3g, baseerr = %.10g, "
			"DelTb = %5.3Lg [hrs]",
			state->pstamp_i,
			state->stamp_i, (phat - state->phat)/phat,
			state->perr, baseerr, DelTb/3600);

		state->phat_sanity_count++;
		ADD_STATUS(rad_data, STARAD_PHAT_SANITY);
		ret = STARAD_PLOCAL_SANITY;
	} else {
		state->K += state->stamp.Ta * (long double) (state->phat - phat);
		state->phat = phat;
		DEL_STATUS(rad_data, STARAD_PHAT_SANITY);
	}

	return (ret);
}




/* =============================================================================
 * PLOCAL ALGO
 * =============================================================================
 */


/*
 * Refinement pointless here, just copy. If not active, never used, no cleanup
 * needed.
 */
void
process_plocal_warmup(struct bidir_algostate* state)
{
	state->plocal = state->phat;
}


int
process_plocal_full(struct bidir_algostate* state, struct radclock_data *rad_data,
		unsigned int plocal_winratio, struct bidir_stamp* stamp,
		int phat_sanity_raised, int qual_warning)
{
	index_t st_end;			// indices of last pkts in stamp history
	index_t RTT_end;			// indices of last pkts in RTT history

	/* History lookup window boundaries */
	index_t lhs;
	index_t rhs;
	long double DelTb;			// Time between j and i based on each NTP timestamp
	struct bidir_stamp *stamp_near;
	struct bidir_stamp *stamp_far;
	double plocal;				// Local period estimate for current stamp
	double plocalerr;			// estimate of total error of plocal [unitless]
	vcounter_t *RTT_far;		// RTT value holder
	vcounter_t *RTT_near;		// RTT value holder


	/*
	 * Compute index of stamp we require to be available before proceeding
	 * (different usage to shift_end etc!)
	 * if not fully past poll transition and have not history ready then
	 *   default to phat copy if problems with data or transitions
	 *   record a problem, will have to restart when it resolves
	 * else proceed with plocal processing
	 */
	state->plocal_end = state->stamp_i - state->plocal_win + 1 - state->wwidth -
		state->wwidth / 2;
	st_end  = history_end(&state->stamp_hist);
	RTT_end = history_end(&state->RTT_hist);

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
				"%lu %lu %lu %lu ", state->plocal_end, state->poll_changed_i,
				st_end, RTT_end);
		return (0);
	}
	else if (state->plocal_problem)
		init_plocal(state, plocal_winratio, state->stamp_i);

	lhs = state->stamp_i - state->wwidth - state->plocal_win - state->wwidth/2;
	rhs = state->stamp_i - 1 			  - state->plocal_win - state->wwidth/2;
	state->far_i  = history_min_slide(&state->RTT_hist, state->far_i, lhs, rhs);
	state->near_i = history_min_slide(&state->RTT_hist, state->near_i,
			state->stamp_i-state->wwidth, state->stamp_i-1);

	/* Compute time intervals between NTP timestamps of selected stamps */
	stamp_near = history_find(&state->stamp_hist, state->near_i);
	stamp_far  = history_find(&state->stamp_hist, state->far_i);

	plocal = compute_phat(state, stamp_far, stamp_near);
	/*
	 * Something bad happen, most likely, we have a bug. The algo may recover
	 * from this, so do not update and keep going.
	 */
	if ( plocal == 0 ) {
		return 1;
	}

	RTT_far  = history_find(&state->RTT_hist, state->far_i);
	RTT_near = history_find(&state->RTT_hist, state->near_i);
	DelTb = stamp_near->Tb - stamp_far->Tb;
	plocalerr = state->phat * ((double)(*RTT_far + *RTT_near) - 2*state->RTThat) / DelTb;
	state->plocalerr = plocalerr;  // this was omitted, a bug, stopping true quality variation being seen

	/* If quality looks bad, retain previous value */
	if ( fabs(plocalerr) >= state->Eplocal_qual ) {
		verbose(VERB_QUALITY, "i=%lu: plocal quality low,  (far,near) = "
				"(%lu,%lu), not updating plocalerr = %5.3lg, "
				"Eplocal_qual = %5.3lg, RTT (near,far,hat) = (%5.3lg, %5.3lg, %5.3lg) ",
				state->stamp_i, state->far_i,
				state->near_i, state->plocalerr, state->Eplocal_qual,
				state->phat * (double)(*RTT_near),
				state->phat * (double)(*RTT_far),
				state->phat * (double)(state->RTThat)	);
		ADD_STATUS(rad_data, STARAD_PLOCAL_QUALITY);
		return 0;
	}

	/* Candidate acceptable based on quality, record this */
	DEL_STATUS(rad_data, STARAD_PLOCAL_QUALITY);

	/* if quality looks good, continue but refuse to update if result looks
	 * insane. qual_warning may not apply to stamp_near or stamp_far, but we
	 * still follow the logic "there is something strange going on in here".
	 * Also, plocal searches in two windows for best stamps, which is a decent
	 * damage control.
	 */
	if ( (fabs(state->plocal-plocal)/state->plocal > state->Eplocal_sanity) || qual_warning) {
		if (qual_warning)
			verbose(VERB_QUALITY, "qual_warning received, i=%lu, following "
					"sanity check for plocal", state->stamp_i);
		verbose(VERB_SANITY, "i=%lu: plocal update fails sanity check: relative "
				"difference is: %5.3lg estimated error was %5.3lg",
				state->stamp_i, fabs(state->plocal-plocal)/state->plocal, plocalerr);
		ADD_STATUS(rad_data, STARAD_PLOCAL_SANITY);
		state->plocal_sanity_count++;
	} else {
		state->plocal = plocal;
		// TODO, we should actually age this stored value if quality is
		// bad or sanity and we cannot update to the latest computed
		state->plocalerr = plocalerr;
		DEL_STATUS(rad_data, STARAD_PLOCAL_SANITY);
	}
	return 0;
}




/* =============================================================================
 * THETAHAT ALGO
 * =============================================================================
 */

void
process_thetahat_warmup(struct bidir_algostate* state, struct radclock_data *rad_data,
		struct radclock_phyparam *phyparam, vcounter_t RTT,
		struct bidir_stamp* stamp, struct bidir_output *output)
{

	double thetahat;	// double ok since this corrects clock which is already almost right
	double errTa = 0;	// calculate causality errors for correction of thetahat
	double errTf = 0;	// calculate causality errors for correction of thetahat
	double wj;			// weight of pkt i
	double wsum = 0;	// sum of weights
	double th_naive = 0;// thetahat naive estimate
	double ET = 0;		// error thetahat ?
	double minET = 0;	// error thetahat ?

	double *thnaive_tmp;
	double gapsize;		// size in seconds between pkts, used to track widest gap in offset_win

	index_t adj_win;		// adjusted window
	index_t j;				// loop index, needs to be signed to avoid problem when j hits zero
	index_t jmin  = 0;		// index that hits low end of loop
	index_t jbest = 0;		// record best packet selected in window

	double Ebound;				// per-stamp error bound on thetahat  (currently just ET)
	double Ebound_min = 0;	// smallest Ebound in offset win
	index_t RTT_end;			// indices of last pkts in RTT history
	index_t thnaive_end;		// indices of last pkts in thnaive history
	struct bidir_stamp *stamp_tmp;
	vcounter_t *RTT_tmp;		// RTT value holder


	/* During warmup, no plocal refinement, no gap detection, no SD error
	 * correction, only simple sanity warning 
	 */
	if ( (stamp->Te - stamp->Tb) >= RTT*state->phat*0.95 ) {
		verbose(VERB_SYNC, "i=%d: Apparent server timestamping error, RTT<SD: "
				"RTT = %6.4lg [ms], SD= %6.4lg [ms], SD/RTT= %6.4lg.",
				state->stamp_i, 1000*RTT*state->phat, 1000*(double)(stamp->Te-stamp->Tb),
				(double)(stamp->Te-stamp->Tb)/RTT/state->phat );
	}
	/* Calculate naive estimate at stamp i
	 * Also track last element stored in thnaive_end
	 */
	th_naive = (state->phat*((long double)stamp->Ta + (long double)stamp->Tf) +
				(2*state->K - (stamp->Tb + stamp->Te)))/2.0;
	history_add(&state->thnaive_hist, state->stamp_i, &th_naive);


	/* Calculate weighted sum */
	wsum = 0;
	thetahat = 0;
	/* Fix old end of thetahat window:  poll_period changes, offset_win changes, history limitations */
	if ( state->poll_transition_th > 0 ) {
		/* linear interpolation over new offset_win */
		adj_win = (state->offset_win - state->poll_transition_th) + (ceil)(state->poll_transition_th * state->poll_ratio);
		verbose(VERB_CONTROL, "In offset_win transition following poll_period change, "
				"[offset transition] windows are [%lu %lu]", state->offset_win,adj_win);

		// Former code which requires to cast into signed variables. This macro
		// is dangerous because it does not check the sign of the variables (if
		// unsigned rolls over, it gives the wrong result
		//jmin = MAX(1, state->stamp_i-adj_win+1);
		if ( state->stamp_i > (adj_win - 1) )
			jmin = state->stamp_i - (adj_win - 1);
		else
			jmin = 0;
		jmin = MAX (1, jmin);
		state->poll_transition_th--;
	}
	else {
		/* ensure don't go past 1st stamp, and don't use 1st, as thnaive set to zero there */
		// Former code which requires to cast into signed variables.
		// jmin = MAX(1, state->stamp_i-state->offset_win+1);
		if ( state->stamp_i > (state->offset_win - 1 ))
			jmin = state->stamp_i - (state->offset_win - 1);
		else
			jmin = 0;
		jmin = MAX (1, jmin);
	}
	
	/* find history constraint */
	RTT_end 		= history_end(&state->RTT_hist);
	thnaive_end = history_end(&state->thnaive_hist);
	jmin = MAX(jmin, MAX(RTT_end, thnaive_end));

	for ( j = state->stamp_i; j >= jmin; j-- ) {
		/* Reassess pt errors each time, as RTThat not stable in warmup.
		 * Errors due to phat errors are small
		 * then add aging with pessimistic rate (safer to trust recent)
		 */
		RTT_tmp   = history_find(&state->RTT_hist, j);
		stamp_tmp = history_find(&state->stamp_hist, j);
		ET  = state->phat * ((double)(*RTT_tmp) - state->RTThat );
		ET += state->phat * (double)( stamp->Tf - stamp_tmp->Tf ) * phyparam->BestSKMrate;

		/* Per point bound error is simply ET in here */
		Ebound  = ET;

		/* Record best in window, smaller the better. When i<offset_win, bound
		 * to be zero since arg minRTT also in win. Initialise minET to first
		 * one in window.
		 */
		if ( j == state->stamp_i ) {
			minET = ET;
			jbest = j;
		} else {
			if ( ET < minET ) {
				minET = ET;
				jbest = j;
				Ebound_min = Ebound;    // could give weighted version instead
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



//	/* If a RTCreset, must adopt the most recent UTC available: normal quality
//	 * weighting or checks, and sanity checks, don't apply.
//	 * TODO: check settings for other variables not used in this branch, and for
//	 *		any other impacts of having the counter frozen, effectively an invisible jump backward
//	 *    check phat as well, other copies?  if change K, must change it IMMediately
//	 */
//	if (RTCreset) {
//		verbose(VERB_QUALITY, "i=%lu: RTCreset (warmup): setting to current stamp, "
//			"disabling window, quality may be poor until window refills.\n"
//			"      Jumping rAdclock by = %5.3lf [s]", state->stamp_i, - th_naive);
//
//		state->K -= th_naive;				// absorb current apparent error into K
//		thetahat = 0;						// error is zero wrt new K
//		//state->phat -= 2 * (state->thetahat - th_naive) / (double)(stamp->Ta + stamp->Tf);   // check this!!
//		state->thetahat = thetahat;
//		jbest = state->stamp_i;
//
//		/* Overwrite thetahats in window to ensure reset value is adopted.
//		 * This resets the already stored th_naive also. Retain th_naive variable
//		 * for verbosity and output to preserve original data and mark event.
//		 */
//		for ( j = state->stamp_i; j >= jmin; j-- )
//			*(double *)(history_find(&state->thnaive_hist, j)) = 0;
//
//		DEL_STATUS(rad_data, STARAD_OFFSET_QUALITY);	// since accepted stamp, even though quality likely poor
//		DEL_STATUS(rad_data, STARAD_OFFSET_SANITY);
//
//		PEER_ERROR(state)->Ebound_min_last = Ebound_min;
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
					"Eoffset_qual = %lg may be too large", state->stamp_i, minET, state->Eoffset_qual);
			thetahat = state->thetahat;
		} else {
			thetahat /= wsum;
			/* store est'd quality of new estimate */
			state->minET = minET;
			/* Record last good estimate of error bound.
			 * Also need to track last time we updated theta to do proper aging of
			 * clock error bound in warmup
			 */
			PEER_ERROR(state)->Ebound_min_last = Ebound_min;
			copystamp(stamp, &state->thetastamp);
		}
		
		/* if result looks insane, give warning */
		if ( fabs(state->thetahat - thetahat) > (state->Eoffset_sanity_min + state->Eoffset_sanity_rate * gapsize) ) {
			verbose(VERB_SANITY, "i=%lu: thetahat update fails sanity check: "
					"difference is: %5.3lg [ms], estimated error was  %5.3lg [ms]",
					state->stamp_i, 1000*(thetahat - state->thetahat), 1000*minET);
			state->offset_sanity_count++;
			ADD_STATUS(rad_data, STARAD_OFFSET_SANITY);
		} else
			DEL_STATUS(rad_data, STARAD_OFFSET_SANITY);

		/* update value of thetahat, even if sanity triggered */
		state->thetahat = thetahat;

	} else {
		verbose(VERB_QUALITY, "i=%lu: thetahat quality over offset window poor "
				"(minET %5.3lg [ms], thetahat = %5.3lg [ms]), repeating current value",
				state->stamp_i, 1000*minET, 1000*thetahat);
		ADD_STATUS(rad_data, STARAD_OFFSET_QUALITY);
	}

//	}	// RTCreset hack
	
	

// TODO should we check causality if state->thetahat has not been updated?
// TODO also behaviour different in warmup and full algo

	/* errTa - thetahat  should be -ve */
	errTa = (double)((long double)stamp->Ta * state->phat + state->K - (long double) stamp->Tb);
	if ( errTa > state->thetahat ) {
		verbose(VERB_CAUSALITY, "i=%lu: causality breach on C(Ta), errTa = %5.3lf [ms], "
				"thetahat = %5.3lf [ms], diff = %5.3lf [ms] ",
				state->stamp_i, 1000*errTa, 1000*state->thetahat, 1000*(errTa-state->thetahat));
	}

	/* errTf - thetahat  should be +ve */
	errTf = (double)((long double)stamp->Tf * state->phat + state->K - (long double) stamp->Te);
	if ( errTf < state->thetahat ) {
		verbose(VERB_CAUSALITY, "i=%lu: causality breach on C(Tf), errTf = %5.3lf [ms], "
				"thetahat = %5.3lf [ms], diff = %5.3lf [ms] ",
				state->stamp_i, 1000*errTf, 1000*state->thetahat, 1000*(errTf-state->thetahat));
	}

	/* Warmup to warmup is to pass offset_win */
	if ( (state->stamp_i < state->offset_win*2) || !(state->stamp_i%50) )
	{
		verbose(VERB_SYNC, "i=%lu: th_naive = %5.3lf [ms], thetahat = %5.3lf [ms], "
				"wsum = %7.5lf, minET = %7.5lf [ms], RTThat/2 = %5.3lf [ms]", 
				state->stamp_i, 1000*th_naive, 1000*thetahat, wsum,
				1000*minET, 1000*state->phat*state->RTThat/2.);
	}


	/* Fill output data structure to print internal local variables */
	errTa -= state->thetahat;
	errTf -= state->thetahat;
	output->errTa 			= errTa;
	output->errTf			= errTf;
	output->th_naive		= th_naive;
	output->minET			= minET;
	output->wsum			= wsum;
	stamp_tmp = history_find(&state->stamp_hist, jbest);
	output->best_Tf		= stamp_tmp->Tf;
}



void process_thetahat_full(struct bidir_algostate* state, struct radclock_data *rad_data,
		struct radclock_phyparam *phyparam, vcounter_t RTT,
		struct bidir_stamp* stamp, int qual_warning,  struct bidir_output *output)
{
	double thetahat;	// double ok since this corrects clock which is almost right
	double errTa = 0;	// calculate causality errors for correction of thetahat
	double errTf = 0;	// calculate causality errors for correction of thetahat
	double wj;			// weight of pkt i
	double wsum = 0;	// sum of weights
	double th_naive = 0;// thetahat naive estimate
	double ET = 0;		// error thetahat ?
	double minET = 0;	// error thetahat ?

	double *thnaive_tmp;
	double gapsize;		// size in seconds between pkts, tracks widest gap in offset_win
	//int gap = 0;		// logical: 1 = have found a large gap at THIS stamp  // not used

	index_t adj_win;					// adjusted window
	index_t j;				// loop index, needs to be signed to avoid problem when j hits zero
	index_t jmin  = 0;		// index that hits low end of loop
	index_t jbest = 0;		// record best packet selected in window

	double Ebound;				// per-stamp error bound on thetahat  (currently just ET)
	double Ebound_min = 0;	// smallest Ebound in offset win
	index_t st_end;			// indices of last pkts in stamp history
	index_t RTT_end;			// indices of last pkts in RTT history
	index_t RTThat_end;			// indices of last pkts in RTThat history
	index_t thnaive_end;		// indices of last pkts in thnaive history
	struct bidir_stamp *stamp_tmp;
	struct bidir_stamp *stamp_tmp2;
	vcounter_t *RTT_tmp;		// RTT value holder
	vcounter_t *RTThat_tmp;		// RTT hat value holder


	if ((stamp->Te - stamp->Tb) >= RTT*state->phat * 0.95) {
		verbose(VERB_SYNC, "i=%lu: Apparent server timestamping error, RTT<SD: "
				"RTT = %6.4lg [ms], SD= %6.4lg [ms], SD/RTT= %6.4lg.",
				state->stamp_i, 1000 * RTT*state->phat,
				1000 * (double)(stamp->Te-stamp->Tb),
				(double)(stamp->Te-stamp->Tb)/RTT/state->phat);
	}

	/* Calculate naive estimate at stamp i
	 * Also track last element stored in thnaive_end
	 */
	th_naive = (state->phat * ((long double)stamp->Ta + (long double)stamp->Tf) +
			(2 * state->K - (stamp->Tb + stamp->Te))) / 2.0;
	history_add(&state->thnaive_hist, state->stamp_i, &th_naive);

	/* Initialize gapsize
	 * Detect gaps and note large gaps (due to high loss)
	 * Default is no gap, if one, computed below
	 * gapsize is initialized here for this i, to localize big gaps
	 */
	gapsize = state->phat * (double)(stamp->Tf - state->stamp.Tf);

	/* gapsize is in [sec], but here looking for loss events */
	if ( gapsize > (double) state->poll_period * 4.5 ) {
		verbose(VERB_SYNC, "i=%lu: Non-trivial gap found: gapsize = %5.1lf "
				"stamps or %5.3lg [sec]", state->stamp_i,
				gapsize/state->poll_period, gapsize);
		/* In `big gap' mode, mistrust plocal and trust local th more */
		if (gapsize > (double) phyparam->SKM_SCALE) {
			//gap = 1;
			verbose(VERB_SYNC, "i=%lu: End of big gap found width = %5.3lg [day] "
					"or %5.2lg [hr]", state->stamp_i, gapsize/(3600*24), gapsize/3600);
		}
	}

	/* Calculate weighted sum */
	wsum = 0;
	thetahat = 0;

	/*
	 * Fix old end of thetahat window:  poll_period changes, offset_win changes,
	 * history limitations
	 */
	if (state->poll_transition_th > 0) {
		/* linear interpolation over new offset_win */
		adj_win = (state->offset_win - state->poll_transition_th) +
				(ceil)(state->poll_transition_th * state->poll_ratio);
		verbose(VERB_CONTROL, "In offset_win transition following poll_period "
				"change, [offset transition] windows are [%lu %lu]",
				state->offset_win, adj_win);

		// Former code which requires to cast into signed variables. This macro
		// is dangerous because it does not check the sign of the variables (if
		// unsigned rolls over, it gives the wrong result
		//jmin = MAX(2, state->stamp_i-adj_win+1);
		if (state->stamp_i > (adj_win - 1))
			jmin = state->stamp_i - (adj_win - 1);
		else
			jmin = 0;
		jmin = MAX(2, jmin);
		state->poll_transition_th--;
	}
	else {
		/* ensure don't go past 1st stamp, and don't use 1st, as thnaive set to
		 * zero there
		 */
		// Former code which requires to cast into signed variables. This macro
		// is dangerous because it does not check the sign of the variables (if
		// unsigned rolls over, it gives the wrong result
		// jmin = MAX(1, state->stamp_i-state->offset_win+1);
		if (state->stamp_i > (state->offset_win-1))
			jmin = state->stamp_i - (state->offset_win - 1);
		else
			jmin = 0;
		jmin = MAX (1, jmin);
	}
	/* find history constraint */
	st_end  	= history_end(&state->stamp_hist);
	RTT_end 	= history_end(&state->RTT_hist);
	RTThat_end 	= history_end(&state->RTThat_hist);
	thnaive_end = history_end(&state->thnaive_hist);

	jmin = MAX(jmin, MAX(st_end, MAX(RTT_end, MAX(RTThat_end, thnaive_end))));

	for (j = state->stamp_i; j >= jmin; j--) {
		/* first one done, and one fewer intervals than stamps
		 * find largest gap between stamps in window
		 */
		if (j < state->stamp_i - 1) {
			stamp_tmp = history_find(&state->stamp_hist, j);
			stamp_tmp2 = history_find(&state->stamp_hist, j+1);
			gapsize = MAX(gapsize, state->phat * (double) (stamp_tmp2->Tf - stamp_tmp->Tf));
		}

		/*
		 * Don't reassess pt errors (shifts already accounted for) then add SD
		 * quality measure (large SD at small RTT=> delayed Te, distorting
		 * th_naive) then add aging with pessimistic rate (safer to trust recent)
		 */
		RTT_tmp = history_find(&state->RTT_hist, j);
		RTThat_tmp = history_find(&state->RTThat_hist, j);
		stamp_tmp = history_find(&state->stamp_hist, j);
		ET  = state->phat * ((double)(*RTT_tmp) - *RTThat_tmp);
		ET += state->phat * (double) ( stamp->Tf - stamp_tmp->Tf ) * phyparam->BestSKMrate;

		/* Per point bound error is ET without the SD penalty */
		Ebound  = ET;

		/* Add SD penalty to ET
		 * XXX: SD quality measure has been problematic in different cases:
		 * - kernel timestamping with hardware based servers(DAG, 1588), punish
		 *   good packets
		 * - with bad NTP servers that have SD > Eoffset_qual all the time.
		 * Definitively removed on 28/07/2011
		 */
		//ET += stamp_tmp->Te - stamp_tmp->Tb;


		/* Record best in window, smaller the better. When i<offset_win, bound
		 * to be zero since arg minRTT also in win. Initialise minET to first
		 * one in window.
		 */
		if ( j == state->stamp_i ) {
			minET = ET; jbest = j; Ebound_min = Ebound;
		} else {
			if (ET < minET) { minET = ET; jbest = j; }
			if (Ebound < Ebound_min) { Ebound_min = Ebound; } // dont assume min index same here
		}
		/* Calculate weight, is <=1 Note: Eoffset initialised to non-0 value, safe to divide */
		wj = exp(- ET * ET / state->Eoffset / state->Eoffset); wsum += wj;

		/* Correct phat already used by difference with more locally accurate plocal */
		thnaive_tmp = history_find(&state->thnaive_hist, j);
		if (state->plocal_problem)
			thetahat += wj * (*thnaive_tmp);
		else
			thetahat += wj * (*thnaive_tmp - (state->plocal / state->phat - 1) *
				state->phat * (double) (stamp->Tf - stamp_tmp->Tf));
	}

	/* Check Quality and Calculate new candidate estimate
	 * quality over window looks good, use weights over window
	 */
	if (minET < state->Eoffset_qual) {
		DEL_STATUS(rad_data, STARAD_OFFSET_QUALITY);
		/* if wsum==0 just copy thetahat to avoid crashing (can't divide by zero)
		 * (this problem must be addressed by operator), else safe to normalise
		 */
		if (wsum == 0) {
			verbose(VERB_QUALITY, "i=%lu: quality looks good (minET = %lg) yet "
					"wsum=0! Eoffset_qual = %lg may be too large",
					state->stamp_i, minET,state->Eoffset_qual);
			thetahat = state->thetahat;
		}
		else {
			thetahat /= wsum;
			/* store est'd quality of new estimate */
			state->minET = minET;   // BUG  wrong place!  and is this minET_old?
		}
	} else {  // quality bad, forget weights (and plocal refinement) and lean on last reliable estimate
		//  BUG  where has gap code gone!!
		thetahat = state->thetahat;	// this will effectively suppress sanity check
		verbose(VERB_QUALITY, "i=%lu: thetahat quality very poor. wsum = %5.3lg, "
				"curr err = %5.3lg, old = %5.3lg, this pt-err = [%5.3lg] [ms]",
				state->stamp_i, wsum, 1000*minET, 1000*state->minET, 1000*ET);
		state->offset_quality_count++;
		ADD_STATUS(rad_data, STARAD_OFFSET_QUALITY);
	}

// TODO behaviour different in warmup and full algo, should check causality on
// TODO thetahat or state->thetahat?

	/* errTa - thetahat should be -ve */
	errTa = (double)((long double)stamp->Ta * state->phat + state->K -
			(long double) stamp->Tb);

	if (errTa > state->thetahat)
		verbose(VERB_CAUSALITY, "i=%lu: causality breach uncorrected on C(Ta), "
				"errTa = %5.3lf [ms], thetahat = %5.3lf [ms], diff = %5.3lf [ms]",
				state->stamp_i, 1000*errTa, 1000*thetahat, 1000*(errTa-thetahat));
	
	/* errTf - thetahat should be +ve */
	errTf = (double)((long double)stamp->Tf * state->phat + state->K -
			(long double) stamp->Te);

	if (errTf < state->thetahat)
		verbose(VERB_CAUSALITY, "i=%lu: causality breach uncorrected on C(Tf), "
				"errTf = %5.3lf [ms], thetahat = %5.3lf [ms], diff = %5.3lf [ms]",
				state->stamp_i, 1000*errTf, 1000*thetahat,1000*(errTf-thetahat));


	/* Apply Sanity Check 
	 * sanity also relative to duration of lockouts due to low quality
	 */
	gapsize = MAX(gapsize, state->phat * (double)(stamp->Tf - state->thetastamp.Tf) );
	/* if looks insane given gapsize, refuse update */
	if ((fabs(state->thetahat-thetahat) > (state->Eoffset_sanity_min +
				state->Eoffset_sanity_rate * gapsize)) || qual_warning) {
		if (qual_warning)
			verbose(VERB_QUALITY, "i=%lu: qual_warning received, following sanity check for thetahat",
				   	state->stamp_i);
		verbose(VERB_SANITY, "i=%lu: thetahat update fails sanity check."
				"diff= %5.3lg [ms], est''d err= %5.3lg [ms], sanity level: %5.3lg [ms] "
				"with total gapsize = %.0lf [sec]",
				state->stamp_i, 1000*(thetahat-state->thetahat), 1000*minET,
				1000*(state->Eoffset_sanity_min+state->Eoffset_sanity_rate*gapsize), gapsize);
		state->offset_sanity_count++;
		ADD_STATUS(rad_data, STARAD_OFFSET_SANITY);
	}
	else { // either quality and sane, or repeated due to bad quality
		state->thetahat = thetahat;  //  it passes! update current value
		if ( ( minET < state->Eoffset_qual ) && ( wsum != 0 ) ) {
// TODO check the logic of this branch, why thetahat update not in here as well
			copystamp(stamp, &state->thetastamp);
			// BUG??  where is "lastthetahat = i;" ?  ahh, seems never to have been used..
			/* Record last good estimate of error bound after sanity check */
			PEER_ERROR(state)->Ebound_min_last = Ebound_min;
		}
		DEL_STATUS(rad_data, STARAD_OFFSET_SANITY);
	}

	if ( !(state->stamp_i % (int)(6 * 3600 / state->poll_period)) )
	{
		verbose(VERB_SYNC, "i=%lu: th_naive = %5.3lf [ms], thetahat = %5.3lf [ms], "
				"wsum = %7.5lf, minET = %7.5lf [ms], RTThat/2 = %5.3lf [ms]", 
				state->stamp_i, 1000*th_naive, 1000*thetahat, wsum,
				1000*minET, 1000*state->phat*state->RTThat/2.);
	}

	/* Fill output data structure to print internal local variables */
	errTa -= state->thetahat;
	errTf -= state->thetahat;
	output->errTa 			= errTa;
	output->errTf 			= errTf;
	output->th_naive 		= th_naive;
	output->minET 			= minET;
	output->wsum 			= wsum;
	stamp_tmp = history_find(&state->stamp_hist, jbest);
	output->best_Tf 		= stamp_tmp->Tf;

}


/* =============================================================================
 * CLOCK SYNCHRONISATION ALGORITHM
 * =============================================================================
 */


/* This routine takes in a new bi-directional stamp and uses it to update the
 * estimates of the clock calibration.   It implements the full `algorithm' and
 * is abstracted away from the lower and interface layers.  It should be
 * entirely portable.  It returns updated clock in the global clock format,
 * pre-corrected.
 *
 * Offset estimation C(t) = vcount(t)*phat + K
 * theta(t) = C(t) - t
 */
int
process_bidir_stamp(struct radclock_handle *handle, struct bidir_algostate *state,
		struct bidir_stamp *input_stamp, int qual_warning,
		struct radclock_data *rad_data, struct radclock_error *rad_error,
		struct bidir_output *output)
{
	struct bidir_stamp *stamp;		// remember, here, stamp is not a stamp_t !
	struct radclock_phyparam *phyparam;
	struct radclock_config *conf;
	int poll_period;
	vcounter_t RTT;		// Current RTT (vcount units to avoid pb if phat bad */
	unsigned int warmup_winratio;
	unsigned int plocal_winratio;

	/*
	 * Error bound reporting. Error bound correspond to the last effective update of
	 * thetahat (i.e. it may not be the value computed with the current stamp).
	 * avg and std are tracked based on the size of the top window
	 * Only the ones needed to track top level window replacing values need to be
	 * static (until the window mechanism is rewritten to proper data structure to
	 * avoid statics).
	 * No need for a "_last", it is maintained outside the algo (as well as the
	 * current number)
	*/
	double error_bound;
	int phat_sanity_raised;

	JDEBUG

	stamp = input_stamp;		// why bother?  input_stamp is a local var anyway..
	phyparam = &(handle->conf->phyparam);
	conf = handle->conf;
	poll_period = conf->poll_period;

	/*
	 * Warmup and plocal window, gives fraction of Delta(t) sacrificed to near
	 * and far search windows.
	 */
	warmup_winratio = 4;
	plocal_winratio = 5;

	/*
	 * First thing is to react to configuration updates passed to the process.
	 * If the poll period or environment quality has changed, some key algorithm
	 * parameters have to be updated.
	 */
	if (HAS_UPDATE(conf->mask, UPDMASK_POLLPERIOD) ||
			HAS_UPDATE(conf->mask, UPDMASK_TEMPQUALITY)) {
		update_state(state, phyparam, poll_period, plocal_winratio);
	}

	/*
	 * It is the first stamp, we initialise the state structure, push the first
	 * stamp in history, initialize key algorithm variables: algo parameters,
	 * window sizes, history structures, states, poll period effects and return
	 */
	if (state->stamp_i == 0) {
		init_state(handle, phyparam, state, rad_data, stamp, plocal_winratio, poll_period);
		copystamp(stamp, &state->stamp);
		copystamp(stamp, &state->thetastamp); // thetastamp setting needs detailed revision
		state->stamp_i++;
 	
// TODO fixme, just a side effect
// Make sure we have a valid RTT for the output file
		RTT = MAX(1,stamp->Tf - stamp->Ta);

		output->best_Tf = stamp->Tf;

		/* Set the status of the clock to STARAD_WARMUP */
		ADD_STATUS(rad_data, STARAD_WARMUP);
		ADD_STATUS(rad_data, STARAD_UNSYNC);

		goto output_results;
	}



//	/* On second packet, i=1, let's get things started */
//	if ( state->stamp_i == 1 )
//	{
//		/* Set the status of the clock to STARAD_WARMUP */
//		verbose(VERB_CONTROL, "Beginning Warmup Phase");
//		ADD_STATUS(rad_data, STARAD_WARMUP);
//		ADD_STATUS(rad_data, STARAD_UNSYNC);
//	}


	/*
	 * Clear UNSYNC status once the burst of NTP packets is finished. This
	 * corresponds to the first update passed to the kernel. Cannot really do
	 * push an update before this, and not much after either. It should be
	 * driven by some quality metrick, but implementation is taking priority at
	 * this stage.
	 * This status should be put back on duing long gaps (outside the algo
	 * thread then), and cleared once recover from gaps or data starvation.
	 * Ideally, should not be right on recovery (i.e. the > test) but when
	 * quality gets good. 
	 */
	if (state->stamp_i == NTP_BURST) {
		DEL_STATUS(rad_data, STARAD_UNSYNC);
	}




	/* =============================================================================
	 * BEGIN SYNCHRONISATION
	 *
	 * First, Some universal processing for all stamps i > 0
	 * =============================================================================
	 */

	/* Current RTT - universal!
	 * Avoids zero or negative values in case of corrupted stamps
	 */
	RTT = MAX(1,stamp->Tf - stamp->Ta);

	/* Store history of basics
	 * [use circular buffer   a%b  performs  a - (a/b)*b ,  a,b integer]
	 * copy stamp i into history immediately, will be used in loops
	 */
	history_add(&state->stamp_hist, state->stamp_i, stamp);
	history_add(&state->RTT_hist,   state->stamp_i, &RTT);




	/* =============================================================================
	 * HISTORY WINDOW MANAGEMENT
	 * This should only be kicked in when we are out of warmup,
	 * but since history window way bigger than warmup, this is safe
	 * This resets history prior to stamp i
	 * Shift window not affected
	 * =============================================================================
	 */

	/* Initialize:  middle of very first window */
	if ( state->stamp_i == state->top_win/2 ) {
		/* reset half window estimate - previously  RTThat=next_RTThat */

		/* Make sure RTT is non zero in case we have corrupted stamps */
		// TODO: this should not happen since stamps would not be passed to the
		// algo ... remove the MAX operation
		state->next_RTThat = MAX(1, state->stamp.Tf - state->stamp.Ta);

		/*
		 * Initiate on-line algo for new pstamp_i calculation [needs to be in
		 * surviving half of window!] record next_pstamp_i (index), RTT, RTThat,
		 * point error and stamp
		 * TODO: jsearch_win should be chosen < ??
		 */
		state->jsearch_win = state->warmup_win;
		state->jcount = 1;
		state->next_pstamp_i = state->stamp_i;
		state->next_pstamp_RTThat = state->RTThat;
		state->next_pstamp_perr = state->phat*((double)(RTT) - state->RTThat);
		copystamp(stamp, &state->next_pstamp);
		/* Now DelTb >= top_win/2,  become fussier */
		state->Ep_qual /= 10;

		/* Background error bounds reinitialisation */
		PEER_ERROR(state)->cumsum_hwin = 0;
		PEER_ERROR(state)->sq_cumsum_hwin = 0;
		PEER_ERROR(state)->nerror_hwin = 0;

		verbose(VERB_CONTROL, "Adjusting history window before normal "
				"processing of stamp %lu. FIRST 1/2 window reached",
				state->stamp_i);
	}

	/* At end of history/top window */
	if ( state->stamp_i == state->top_win_half ) {
		/* move window ahead by top_win/2 so i is the first stamp in the 2nd half */
		state->top_win_half += state->top_win/2;
		/* reset RTT estimate - next_RTThat must have been reset at prior upward * shifts */
		state->RTThat = state->next_RTThat;
		/* reset half window estimate - prior shifts irrelevant */
		/* Make sure RTT is non zero in case we have corrupted stamps */
		// TODO: this should not happen since stamps would not be passed to the
		// algo ... remove the MAX operation
		state->next_RTThat = MAX(1, state->stamp.Tf - state->stamp.Ta);
		/* Take care of effects on phat algo
		* - begin using next_pstamp_i that has been precalculated in previous top_win/2
		* - reinitialise on-line algo for new next_pstamp_i calculation
		*   Record [index RTT RTThat stamp ]
		*/
		state->pstamp_i 		= state->next_pstamp_i;
		state->pstamp_RTThat	= state->next_pstamp_RTThat;
		state->pstamp_perr	= state->next_pstamp_perr;
		copystamp(&state->next_pstamp, &state->pstamp);
		state->jcount = 1;
		state->next_pstamp_i			= state->stamp_i;
		state->next_pstamp_RTThat	= state->RTThat;
		state->next_pstamp_perr		= state->phat*((double)(RTT) - state->RTThat);
		copystamp(stamp, &state->next_pstamp);

		/* Background error bounds taking over and restart all over again */
		PEER_ERROR(state)->cumsum 	= PEER_ERROR(state)->cumsum_hwin;
		PEER_ERROR(state)->sq_cumsum	= PEER_ERROR(state)->sq_cumsum_hwin;
		PEER_ERROR(state)->nerror 	= PEER_ERROR(state)->nerror_hwin;
		PEER_ERROR(state)->cumsum_hwin 		= 0;
		PEER_ERROR(state)->sq_cumsum_hwin	= 0;
		PEER_ERROR(state)->nerror_hwin 		= 0;

		verbose(VERB_CONTROL, "Total number of sanity events:  phat: %u, plocal: %u, Offset: %u ",
				state->phat_sanity_count, state->plocal_sanity_count, state->offset_sanity_count);
		verbose(VERB_CONTROL, "Total number of low quality events:  Offset: %u ", state->offset_quality_count);
		verbose(VERB_CONTROL, "Adjusting history window before normal processing of stamp %lu. "
				"New pstamp_i = %lu ", state->stamp_i, state->pstamp_i);
	}




	/* =============================================================================
	* GENERIC DESCRIPTION
	*
	* WARMUP MODDE
	* 0<i<warmup_win
	* pt errors are unreliable, need different algos 
	* RTT:  standard on-line 
	* upward shift detection:  disabled 
	* history window:  no overlap by design, so no need to map stamp indices via  [i%XX_win] 
	* phat:  use plocal type algo, or do nothing if stored value (guarantees value available ASAP), no sanity 
	* plocal:  not used, just copies phat, no sanity.
	* thetahat:  simple on-line weighted average with aging, no SD quality refinement (but non-causal warnings)
	* sanity checks:  switched off except for NAN check (warning in case of offset)
	*
	* FULL ALGO
	* Main body, i >= warmup_win
	* Start using full algos [still some initialisations left]
	* Start wrapping history vectors
	* =============================================================================
	*/

	collect_stats_state(state, stamp);
	if (state->stamp_i < state->warmup_win) {
		process_RTT_warmup(state, RTT);
		process_phat_warmup(state, RTT, warmup_winratio);
		process_plocal_warmup(state);
		process_thetahat_warmup(state, rad_data, phyparam, RTT, stamp, output);
	}
	else {
		process_RTT_full(state, rad_data, RTT);
		record_packet_j(state, RTT, stamp);
		phat_sanity_raised = process_phat_full(state, rad_data, phyparam, RTT,
				stamp, qual_warning);

		// XXX TODO stamp is passed only for quality warning, but plocal
		// windows exclude the current stamp!! should take into account the
		// quality warnings of the stamps actually picked up, no?
		// Check with, Darryl
		process_plocal_full(state, rad_data, plocal_winratio, stamp,
				phat_sanity_raised, qual_warning);

		process_thetahat_full(state, rad_data, phyparam, RTT, stamp, qual_warning, output);
	}



/* =============================================================================
 * END OF WARMUP INITIALISATION
 * =============================================================================
 */

	if (state->stamp_i == state->warmup_win - 1) {
		end_warmup_RTT(state, stamp);
		end_warmup_phat(state, stamp);
		end_warmup_plocal(state, stamp, plocal_winratio);
		end_warmup_thetahat(state, stamp);
		parameters_calibration(state);

		/* Set bounds on top window */
// TODO why is it done in here ??
		state->top_win_half = state->top_win - 1;

		verbose(VERB_CONTROL, "i=%lu: End of Warmup Phase. Stamp read check: "
				"%llu %22.10Lf %22.10Lf %llu",
				state->stamp_i, stamp->Ta,stamp->Tb,stamp->Te,stamp->Tf);

		/* Remove STARAD_WARMUP from the clock's status */
		DEL_STATUS(rad_data, STARAD_WARMUP);
	}



/* =============================================================================
 * RECORD LASTSTAMP
 * =============================================================================
 */
	copystamp(stamp, &state->stamp);

	print_stats_state(state);

	/*
	 * Prepare for next stamp.
	 * XXX Not great for printing things out of the algo (need to
	 * subtract 1)
	 */
	state->stamp_i++;



/* =============================================================================
 * OUTPUT 
 * =============================================================================
 */

output_results:

	/* We lock the global data to to ensure data consistency. Do not want shared
	 * memory segment be half updated and 3rd party processes get bad data.
	 * Also we lock the matlab output data at the same time
	 * to ensure consistency for live captures.
	 */
// TODO FIXME : locking should go away. Should update state data, then lock and
// copy to radclock_handle outside of here
	pthread_mutex_lock(&handle->globaldata_mutex);
	
	/* The next_expected field has to take into account the fact that ntpd sends
	 * packets with true period intervals [poll-1,poll+2] (see an histogram of 
	 * capture if you are not convinced). Also an extra half a second to be safe.
	 * Also, for very slow counters (e.g. ACPI), the first phat estimate can 
	 * send this value far in the future. Wait for i > 1
	 * TODO:  incorporate this into setting which is now in process_stamp
	 */
	//	if (state->stamp_i > 1)
	//		/* TODO: XXX Previously valid till was offset by 1.5s to allow for NTP's
	//		 * varying poll period when in piggy back mode
	//		 * rad_data->next_expected	= stamp->Tf + ((state->poll_period - 1.5) / state->phat);

	/* Clock error estimates.
	 * Applies an ageing from thetastamp to the current state, similar to gap recovery.
	 * Doesn't update estimates from their initialized value of zero on first stamp,
	 * since algo parameters can be 3 orders of magnitude out.
	 * As stamp_i already incremented here, it counts stamps as 1 2 3 TODO: revisit this
	 * nerror counts number of times stats collected, is  stamp_i-1 .
	 */
	if (state->stamp_i > 1) {
		//rad_data->ca_err			= state->minET;	// initialize to this stamp
		error_bound = PEER_ERROR(state)->Ebound_min_last +
			state->phat * (double)(stamp->Tf - state->thetastamp.Tf) * phyparam->RateErrBOUND;
		
		/* Update _hwin members of  state->peer_err  */
		PEER_ERROR(state)->cumsum_hwin += error_bound;
		PEER_ERROR(state)->sq_cumsum_hwin += error_bound * error_bound;
		PEER_ERROR(state)->nerror_hwin += 1;

		/* Update remaining members of  state->peer_err  */
		PEER_ERROR(state)->error_bound = error_bound;
		PEER_ERROR(state)->cumsum += error_bound;
		PEER_ERROR(state)->sq_cumsum += error_bound * error_bound;
		PEER_ERROR(state)->nerror += 1;
	}


	/* Fill the output structure, used mainly to fill the matlab file
	 * TODO: there is a bit of redundancy in here
	 */
	output->RTT				= RTT;
	output->phat			= state->phat;
	output->perr			= state->perr;
	output->plocal			= state->plocal;
	output->plocalerr		= state->plocalerr;
	output->K				= state->K;
	output->thetahat		= state->thetahat;
	output->RTThat			= state->RTThat;
	output->RTThat_new	= state->next_RTThat;
	output->RTThat_shift	= state->RTThat_shift;
	/* Values already set in process_thetahat_* */
	//	output->th_naive		= th_naive;
	//	output->minET			= minET;
	//	output->errTa			= errTa;
	//	output->errTf			= errTf;
	//	output->wsum			= wsum;
	//	stamp_tmp = history_find(&state->stamp_hist, jbest);
	//	output->best_Tf		= stamp_tmp->Tf;
	output->minET_last	= state->minET;			// but is already updated in state??
	output->status			= rad_data->status;

/* Need to rethink purpose of output structure.
 * Many field duplicate those in state, can we have pointers instead?
 * It is mainly for ascii file outputting, can we construct within the print fn
 * only just-in-time?  using values in state and rad_data?  and replace
 * exceptions with references to state and rad_data instead?
 * Ownerships unclear:
 *  [ I think best if output doesn't own anything, just copies for output ]
 *   status:  as above, rad_data seems to own (updated in situ from multiple places
 *          including here, but output just has an (incomplete?) copy made here
 *   leapsecond_*   output seems to own, rad_data gets a copy in process_stamp
 *					best to reverse this?  it is true that leapsec cant live in state.
 *   output->n_stamps  and  state->stamp_i  seem to be the same, no??
 * Does the output need mutex protection?  if not, remove it.
 */

	/* Unlock Global Data */
	pthread_mutex_unlock(&handle->globaldata_mutex);

	return (0);
}


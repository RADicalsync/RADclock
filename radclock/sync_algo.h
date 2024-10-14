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

#ifndef _SYNC_ALGO_H
#define _SYNC_ALGO_H

/*
 * Standard client-server NTP packet exchange.
 *
 *                Tb     Te        true event times:  ta < tb < te < tf
 *                 |      |           available TS's: Ta < Tf  [raw counter units]
 *  Server  ------tb------te-------                   Tb < Te  [s]
 *               /          \
 *              /            \
 *  Client  ---ta-------------tf---
 *             |               |
 *            Ta               Tf
 */



/*
 * Internal algo parameters and default values
 */


/*
 * Machine characteristics (interrupt latency and CPU oscillator stability)
 */
// [sec] natural TSing limitation: `maximum' interrupt latency
#define TS_LIMIT_EXCEL        0.000015
#define TS_LIMIT_GOOD         0.000015
#define TS_LIMIT_POOR         0.000015
// [sec] maximum timescale of validity of the Simple Skew Model (SKM)
#define SKM_SCALE_EXCEL       1024.0
#define SKM_SCALE_GOOD        1024.0
#define SKM_SCALE_POOR        512.0
// bound on rate error regardless of time scale
#define RATE_ERR_BOUND_EXCEL  0.0000001
#define RATE_ERR_BOUND_GOOD   0.0000005
#define RATE_ERR_BOUND_POOR   0.000001
// limit of meaningful accuracy of SKM rate
#define BEST_SKM_RATE_EXCEL   0.00000005
#define BEST_SKM_RATE_GOOD    0.0000002
#define BEST_SKM_RATE_POOR    0.000001
// Ratio defining offset based on TSLIMIT
#define OFFSET_RATIO_EXCEL    6
#define OFFSET_RATIO_GOOD     10
#define OFFSET_RATIO_POOR     10
// plocal quality
#define PLOCAL_QUALITY_EXCEL  0.0000008
#define PLOCAL_QUALITY_GOOD   0.0000008
#define PLOCAL_QUALITY_POOR   0.000002


/* Algo meta-parameters: common to all clocks
 * These are initialised (and/or reset) in the conf system.
 */
struct bidir_metaparam {
	/* base accuracy unit  TODO: rethink this */
	double TSLIMIT;
	/* counter model - values depend on temperature enviroment */
	double SKM_SCALE;
	double RateErrBOUND;
	double BestSKMrate;
	/* algo meta-parameters */
	int offset_ratio;             // used in setting of state->Eoffset
	double plocal_quality;        // used in setting of state->Eplocal_qual
	/* path meta-parameters */
	double path_scale;            // [s] comparison scale used in pref-server code
	double relasym_bound_global;  // used in pathpenalty metric
};


/*
 * Stamps structures, could be uni- or bi- directional. The generic stamp
 * structure also holds side info regarding network or the like.
 */
typedef enum {
	STAMP_UNKNOWN,
	STAMP_SPY,
	STAMP_NTP,    /* Handed by libpcap */
	STAMP_PPS,
} stamp_type_t;


struct unidir_stamp {
	vcounter_t stamp;
	/* Need some time information of some kind */
};


struct bidir_stamp {
	vcounter_t	Ta;    // vcount timestamp [counter value] of pkt leaving client
	long double	Tb;    // timestamp [sec] of arrival at server
	long double	Te;    // timestamp [sec] of departure from server
	vcounter_t	Tf;    // vcount timestamp [counter value] of pkt returning to client
};


// TODO this is very NTP centric
struct stamp_t {
	stamp_type_t type;
	uint64_t id;
	char server_ipaddr[INET6_ADDRSTRLEN];
	int ttl;
	int stratum;
	int LI;    // value of LI bits in response header, in {0,1,2,3}
	uint32_t refid;
	double rootdelay;
	double rootdispersion;
	union stamp_u {
		struct unidir_stamp ustamp;
		struct bidir_stamp  bstamp;
	} st;
};

/* Here x is a pointer to a stamp_t, returns a pointer to the tuple */
#define UST(x) (&((x)->st.ustamp))
#define BST(x) (&((x)->st.bstamp))


/* Holds all RADclock clock variables needed to be visible outside the algo.
	It is used to hold the variables output to analysis, as well as those
	transferred to rad_data.  Many of the algo-internal variables also appear in
	peer  [ so why duplicate ?? even have 3rd copy in rad_data for some ]
	Alternative:  eg for leap stuff, could have as statics within process_stamp,
	these would be the reference variables to be transferred into rad_data when ready.
 */
struct bidir_algooutput {

	/** Clock variables outside algo **/
	long int n_stamps;  // # of stamps with valid sID, and sane, received by process_stamp, fed to algo

	/* Leap second variables */
	int leapsec_total;    // sum of leap seconds seen since clock start
	int leapsec_next;     // value of the expected next leap second {-1 0 1}
	vcounter_t leapsec_expected;    // estimated vcount of next leap, or 0 if none
	
	/** Internal algo variables visible outside **/
	/* Per-stamp output */
	vcounter_t  RTT;       // value for last stamp (Not in state)
	double      phat;
	double      perr;
	double      plocal;
	double      plocalerr; // value for last stamp (Not in state)
	long double K;
	double      thetahat;
	vcounter_t  RTThat;
	vcounter_t  RTThat_new;
	vcounter_t  RTThat_shift;
	double      th_naive;
	double      minET;     // value for last stamp (Not in state)
	double      minET_last;
	double      pDf;
	double      pDb;
	double      wsum;
	vcounter_t  best_Tf;
	unsigned int status;    // Not in state
	// path penalty metrics
	double      pathpenalty;  // latest algo evaluation of path metric
	double      Pchange;      // windowed RTT BL change metric
	double      Pquality;     // EWMA-smoothed generalized quality impact metric

};


/* TODO this may not be too generic, but need it for cleanliness before
 * reworking the bidir_algostate structure
 */
struct algo_error {
	double Ebound_min_last;  // value at last true update
	double error_bound;      // value for last stamp
	long nerror;
	double cumsum;
	double sq_cumsum;
	long nerror_hwin;
	double cumsum_hwin;
	double sq_cumsum_hwin;
};


struct bidir_algostate {

	/* Unique stamp index (C notation [ 0 1 2 ..]) 136 yrs @ 1 stamp/[s] if 32bit */
	index_t stamp_i;

	struct bidir_stamp stamp;  // input bidir stamp (hence leap-free)

	/* Time Series Histories */
	history stamp_hist;
	history Df_hist;
	history Db_hist;
	//history Asym_hist;       // currently Asymhat not based on filtering a raw asym
	history Dfhat_hist;
	history Dbhat_hist;
	history Asymhat_hist;
	history RTT_hist;
	history RTThat_hist;
	history thnaive_hist;

	/* OWD */
	double Dfhat;              // Estimate of minimal Df
 	double next_Dfhat;         // Df estimate to be in the next half top window
	double Dfhat_shift;        // sliding window estimate for upward level shift detection
	double Dfhat_shift_thres;  // threshold in [s] for triggering upward shift detection
	double Dbhat;              // Estimate of minimal Db
 	double next_Dbhat;         // Db estimate to be in the next half top window
	double Dbhat_shift;        // sliding window estimate for upward level shift detection
	double Dbhat_shift_thres;  // threshold in [s] for triggering upward shift detection

	/* Path Asymmetry */
	double Asymhat;            // Estimate of underlying asymmetry

	/* Window sizes, measured in [pkt index] These control algorithm, independent of implementation */
	index_t h_win;             // top level window of held memory/history: must forget past
	index_t history_end;       // future stamp when top level history window half is advanced
	index_t warmup_win;        // warmup window, RTT estimation (indep of time and CPU, need samples)
	index_t shift_win;         // shift detection window size
	index_t shift_end;         // shift detection record of oldest pkt in shift window for RTT
	index_t shift_end_OWD;     // shift detection record of oldest pkt in shift window for OWD
	index_t plocal_win;        // local period estimation window based on SKM scale
	index_t plocal_end;        // oldest pkt in local period estimation window
	index_t offset_win;        // offset estimation, based on SKM scale (don't allow too small)
	index_t jsearch_win;       // window width for choosing pkt j for phat estimation

	int poll_period;            // Current polling period
	index_t poll_transition_th; // Number of future stamps remaining to complete new polling period transition (thetahat business)
	double poll_ratio;          // Ratio between new and old polling period after it changed
	index_t poll_changed_i;     // First stamp after change = index of last detected change

	/* Error thresholds, measured in [sec], or unitless */
	double Eshift;              // threshold for detection of upward level shifts (should adapt to variability)
	double Ep;                  // point error threshold for phat
	double Ep_qual;             // [unitless] quality threshold for phat (value after 1st window)
	double Ep_sanity;           // [unitless] sanity check threshold for phat
	double Eplocal_qual;        // [unitless] quality      threshold for plocal
	double Eplocal_sanity;      // [unitless] sanity check threshold for plocal
	double Eoffset;             // quality band in weighted theta estimate
	double Eoffset_qual;        // weighted quality threshold for offset,
	                            // choose with Gaussian decay in mind!! small multiple of Eoffset
	double Eoffset_sanity_min;  // was absolute sanity check threshold for offset (should adapt to data)
	double Eoffset_sanity_rate; // [unitless] sanity check threshold per unit time [sec] for offset

	/* Warmup phase */
	index_t wwidth;             // warmup width of end windows in pkt units (also used for plocal)
	index_t near_i;             // index of minimal RTT within near windows (larger i)
	index_t far_i;              // index of minimal RTT within far windows (smaller i)
 
	/* RTT (in vcounter units to avoid pb if phat bad)
	 * Records related to top window, and level shift */
	vcounter_t RTThat;             // Estimate of minimal RTT
	vcounter_t prevRTThat;         // previous estimate (immune to UpJump overwritting)
	vcounter_t next_RTThat;        // RTT estimate to be in the next half top window
	vcounter_t RTThat_shift;       // sliding window RTT estimate for upward level shift detection
	vcounter_t RTThat_shift_thres; // threshold in [vcount] units for triggering upward shift detection

	/* Oscillator period estimation */
	double phat;                   // period estimate
	double perr;                   // estimate of total error of phat [unitless]
	index_t jcount;                // counter for pkt index in jsearch window
	int phat_sanity_count;         // counters of error conditions

	/* Record of past stamps for oscillator period estimation */
	struct bidir_stamp pstamp;     // left hand stamp used to compute phat in full algo (1st half win)
	struct bidir_stamp next_pstamp;// the one to be in the next half top window
	index_t pstamp_i;              // index of phat stamp
	index_t next_pstamp_i;         // index of next phat stamp to be used
	double pstamp_perr;            // point error of phat stamp
	double next_pstamp_perr;       // point error of next phat stamp
	vcounter_t pstamp_RTThat;      // RTThat estimate recorded with phat stamp
	vcounter_t next_pstamp_RTThat; // RTThat estimate recorded for next phat stamp

	/* Plocal estimation */
	double plocal;                 // local period estimate
	double plocalerr;              // estimate of total error of plocal [unitless]
	int plocal_sanity_count;       // plocal sanity count
	int plocal_problem;            // something wrong with plocal windows

	/* Offset estimation */
	long double K;                 // Uncorrected clock origin alignment constant
	                               // must hold [s] since timescale origin, and at least 1mus precision
	double thetahat;               // Drift correction for uncorrected clock
	double minET;                  // Estimate of thetahat error
	struct bidir_stamp thetastamp; // Stamp corresponding to last update of thetahat
	int offset_quality_count;      // Offset quality events counter
	int offset_sanity_count;       // Offset sanity events counter

	/* Path Penalty [additional state required beyond RTThat] */
	vcounter_t rawstampgap; // accum'd gap between valid stamps (used outside algo)
	double Pbase;         	// EWMA-smoothed RTT BL
	double Pchange;         // exponentially depreciated TV of the RTT BL
	double Pquality_err;    // thetahat-aware "drift" error at last stamp
	double Pquality;        // EWMA-smoothed generalized quality impact metric

	/* Statistics to track error in the Absolute clock */
	struct algo_error algo_err;

	/* Statistics */
	int stats_sd[3];          // Stats on server delay (good, avg, bad)
// TODO should this be in source structure or peer structure?
//	struct timeref_stats stats;    // FIXME: this is a mess

};

#define	ALGO_ERROR(x)	(&(x->algo_err))



/* Structure containing RADclock algo input, state and output.
 * Per-server data is kept, the pointers point to dynamically allocated `arrays`
 * indexed by serverID: 0, 1,.. nservers-1
 * The matching queue holds stamps from all servers together.
 */
struct bidir_algodata {	
	struct stamp_t *laststamp;
	struct bidir_algostate *state;
	struct bidir_algooutput *output;

	/* Queue of RADstamps to be matched and processed, all servers combined */
	struct stamp_queue *q;
};


//#define OUTPUT(handle, x) ((struct bidir_algooutput*)handle->algo_output)->x
#define SOUTPUT(h,sID,x) ((struct bidir_algodata*)h->algodata)->output[sID].x
#define OUTPUT(h,x) (SOUTPUT(h,h->pref_sID,x))


/*
 * Functions declarations
 */
int RADalgo_bidir(struct radclock_handle *handle, struct bidir_algostate *state,
    struct bidir_stamp *input_stamp, int qual_warning,
    struct radclock_data *rad_data, struct radclock_error *rad_error,
    struct bidir_algooutput *output);

void init_stamp_queue(struct bidir_algodata *algodata);
void destroy_stamp_queue(struct bidir_algodata *algodata);

#endif

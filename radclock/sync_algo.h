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
	STAMP_NTP,        // bidir_stamp
	STAMP_NTP_INT,    // augmented with INTernal authoritative ts pair
	STAMP_NTP_PERF,   // augmented with authoritative Int and/or Ext ts pairs
	STAMP_PPS,
} stamp_type_t;

/* bidir_stamp augmented with authoritative timestamps */
//#define STAMP_NTP_PERF(x) ((x)->type == STAMP_NTP_INT || (x)->type == STAMP_NTP_EXT)


struct unidir_stamp {
	vcounter_t stamp;
	/* Need some time information of some kind */
};

struct bidir_stamp {
	vcounter_t  Ta;    // vcount timestamp [counter value] of pkt leaving client
	long double Tb;    // timestamp [sec] of arrival at server
	long double Te;    // timestamp [sec] of departure from server
	vcounter_t  Tf;    // vcount timestamp [counter value] of pkt returning to client
};

/* Authoritative client-side reference timestamp pair */
struct bidir_refstamp {
	long double Tout;  // [sec] outgoing packet (corresponding to Ta)
	long double Tin;   // [sec] incoming packet (corresponding to Tf)
};

/* This structure augments bidir_stamp with additional authoritative timestamps
 * enabling independent validation, or SHM.  The traditional example is external
 * timestamps taken by a DAG card tapping packets passing in or out of the host.
 */
struct bidir_stamp_perf
{
	struct bidir_stamp bstamp;
	/* Authoritative client-side timestamp pairs */
	struct bidir_refstamp IntRef;    // internal reference/comparison
	struct bidir_refstamp ExtRef;    // external reference
	//long double Tout;  // [sec] outgoing packet (corresponding to Ta)
	//long double Tin;   // [sec] incoming packet (corresponding to Tf)
};





 /*  TODO: relocate stamp_t, bidir_perfdata, bidir_stamp_perf,...  to other files,
  *    stamp_t to some general stamp level, where ?
  */


// TODO this is very NTP centric
struct stamp_t {
	stamp_type_t type;
	uint64_t id;
	char server_ipaddr[INET6_ADDRSTRLEN];
	int ttl;
	int stratum;
	int LI;           // value of LI bits in response header, in {0,1,2,3}
	uint32_t refid;
	double rootdelay;
	double rootdispersion;
	int auth_key_id;  // -1 for non auth NTP, otherwise valid key id
	union stamp_u {
		struct unidir_stamp     ustamp;
		struct bidir_stamp      bstamp;
		//struct bidir_stamp_perf pstamp;
	} st;
	struct bidir_refstamp IntRef;    // internal reference/comparison
	struct bidir_refstamp ExtRef;    // external reference
};

/* Macros to help directly access the relevant tuple in the union
 * Here x is a pointer to a stamp_t, returns a pointer to the tuple */
#define UST(x)  (&((x)->st.ustamp))
#define BST(x)  (&((x)->st.bstamp))
//#define PST(x)  (&((x)->st.pstamp))
//#define PSTB(x) (&((x)->st.pstamp.bstamp))
// Extract the RADstamp 4tuple from the right place regardless of stamp type
//#define BD_TUPLE(x) ( ((x)->type == STAMP_NTP_PERF) ? PSTB(x) : BST(x) )

//#define BD_TUPLE(res,x)              \
//do {                                 \
//	if ((x)->type == STAMP_NTP_PERF) \
//		res = PSTB(x);                 \
//	else                              \
//		res = BST(x);                  \
//} while (0)


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
	unsigned int status;   // Not in state
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

	int sID;    // my server ID

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
	double base_asym;          // base asymmetry known from conf file or calibration
	double Asymhat;            // Estimate of total tracked asymmetry change since start

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
	double Pbase;           // EWMA-smoothed RTT BL
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

#define	ALGO_ERROR(x) (&(x->algo_err))


/* State for BL tracking of scalar timeseries (ie RTT, Df or Db) */
struct BLtrack_state {

	/* Time Series Histories */
	history data_hist;
	history BL_hist;   // current BL estimate (values can be backdated)

	/* *** Detection parameters ****/
	/* Local */
	int ctrigger;
	int DHwin, UHwin;
	int Dinitwin, Uinitwin;
	/* Window */
	int Wwin;                 // shared window for now
	double DWthres, UWthres;  // (D,U) jump size threshold [s]

	/* Output Detection state */
	double BL;              // [s] Estimate of BL at current time

	/* Local Detection internal state */
	int Dpause, Upause;
	double local_ref;       // [s] sliding min ref level over Uinitwin
	index_t local_ref_end;  // record oldest point in window (init of 0 good)
	index_t first_lUdetect;
	index_t last_lUdetect;
	int bufferedU;

	/* Window Detection internal state */
	int DWpause, UWpause;
	double winmin;        // [s] sliding min over Wwin
	index_t winmin_end;   // record oldest point in window (init of 0 good)

};

/* State and parameters for the asymmetry calibration algo.
 * Calibration's stamp index stamp_i is relative to calibration start.
 * This variable is also used to manage stamp identity in BLtrack, as
 * BLtrack_state does not have a stamp counter member.
 * This follows the same convention as the RADclock algo, where bidir_algostate
 * hold the stamp_i member used, but not advanced by, all sub-algos.
 */
struct bidir_calibstate {

	int sID;    // my server ID

	index_t stamp_start;  // external stamp when calib activated
	index_t stamp_i;      // last stamp (relative to calib-start) processed

	double Df, Db;    // current values
	struct BLtrack_state Df_state, Db_state;

	/* Calibration parameters */
	int trim;        // safe distance from detections to avoid PE
	int minlength;   // short acceptable trimmed length for uA estimation

	/* Internal state */
	index_t lastICP;    // index of last ICP found so far
	double mDf, mDb;    // online min over next candidate ICZ

	/* Output state [details of best ICZ] */
	double Df_best, Db_best, uA_best;  // reference minimum/underlying values
	int N_best;
	index_t posn_best;  //

};


/* Unified state for SHM and RADperf assessment.
 * Although separate tasks requiring different data, some state is inherently
 * shared (eg stamp_i) and input PERFstamps are synchronous.
 * Terminology:
 *    PERF: pertains to the 8tuple PERF timestamp format jointly used
 * RADperf: pertains to using Ref timestamps to judge RADclock performance
 */
struct bidir_perfstate {

	int sID;    // my server ID

	index_t stamp_i;
	struct bidir_stamp_perf stamp;  // input bidir_perf stamp (hence leap-free)

	/* Window sizes, measured in [pkt index] */
	index_t warmup_win; // warmup window, RTT estimation (indep of time and CPU, need samples)
	index_t shift_win;  // shift detection window size
	index_t shift_end;  // shift detection record of oldest pkt in shift window for RTT


	/* ********* SHM variables: those not using RADclock timestamps ***********/

	/* Histories */
	history stamp_hist;

	/* OWD */
	double Dfhat;              // Estimate of minimal Df
	double Dfhat_shift;        // sliding window estimate for upward level shift detection
	double Dfhat_shift_thres;  // threshold in [s] for triggering upward shift detection
	double Dbhat;              // Estimate of minimal Db
	double Dbhat_shift;        // sliding window estimate for upward level shift detection
	double Dbhat_shift_thres;  // threshold in [s] for triggering upward shift detection

	/* Path Asymmetry */
	double Asymhat;            // Estimate of underlying asymmetry

	/* SA variables */
	int SA;           // ServerAnomaly: {0,1} = {no,detected} SA for this stamp
	long SA_total;    // total number of per-stamp SA detections


	/* ********* RADperf variables: those using RADclock timestamps ***********/
	/* Histories */

	/* rAdclock error estimation */
	double RADerror;

};

/* SHM variables needed to be visible outside the EXTREF thread */
struct SHM_output {
	/* Per-stamp SHM output */
	double auRTT;    // RTT measured using authoritative timestamps
	int SA;
};

/* RADclock perf variables needed to be visible outside the EXTREF thread */
struct RADperf_output {
	/* Per-stamp PERF output */
	double RADerror;
};


/* Structure containing RADclock algo input, state and output.
 * Per-server data is kept, the pointers point to dynamically allocated `arrays`
 * indexed by serverID: 0, 1,.. nservers-1
 * The matching queue holds stamps from all servers together.
 */
struct bidir_algodata {
	/* Queue of RADstamps to be matched and processed, all servers combined */
	struct stamp_queue *q;

	struct stamp_t *laststamp;
	struct bidir_algostate *state;
	struct bidir_algooutput *output;
};

/* Structure containing asym calibration per-server state.
 */
struct bidir_caldata {
	struct bidir_calibstate *state;
};


/* RADclock perf data, holding 8-tuple input and processing enabling :
 *  - SHM of RADclock's server [using the authoritative timestamps]
 *  - analysis of RADclock performance [assuming a trusted server]
 *  - needed queues to perform matching
 */
struct bidir_perfdata {
	/* Queue of PERFstamps to be processed. Must be first member. */
	struct stamp_queue *q;

	/* Buffer for fast dumping of sane popped RADstamps within PROC */
	int RADBUFF_SIZE;      // number of buffer elements
	struct stamp_t *RADbuff;
	index_t RADbuff_next;  // buffer index for next write, won't wrap

	struct stamp_t *laststamp;    // containing bidir_stamp_perf tuples
	struct bidir_perfstate *state;
	struct SHM_output *SHMoutput;
	struct RADperf_output *RPoutput;
	uint64_t ntc_status;    // trust summary for servers with NTC indices
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

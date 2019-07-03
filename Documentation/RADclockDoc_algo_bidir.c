RADclock V0.4 documentation.
Written by Darryl Veitch, 2019
================================

Convention to point within code:
	/dir/dir/file : functionorstructureinfile

Glossary:
 KV = Kernel Version  ( of the RADclock/FFclock modified kernel )
 IPC = Inter-Process Communication
 OS = Operating System
 SHM = SHared Memory
 FF = FeedForward
 FFcounter = a FF compatible counter
 algo = algorithm, the RADclock clock synchronization algorithm


==============================================================
Summary
=======

This documents the RADclock (algo)rithm itself and its interaction with the
remainder of the RADclock daemon code, and the FF kernel.
The source files are in RADclockrepo/radclock/radclock
	sync_bidir.c
The main associated header files are
	sync_algo.h
	sync_history.h

In addition there is related code in RADclockrepo/radclock/libradclock/ in
	..
and
	radclock-private.h



==============================================================
Introduction
============


/*
 * Structure representing the radclock parameters
 */
struct radclock_data {
	double phat;				// very stable estimate of long term counter period [s]
	double phat_err;			// estimated bound on the relative error of phat [unitless]
	double phat_local;		//	stable estimate on shorter timescale period [s]
	double phat_local_err;  // estimated bound on the relative error of plocal [unitless]
	long double ca;			// K_p - thetahat(t) - leapsectotal? [s]
	double ca_err;				// estimated error (currently minET) in thetahat and ca [s]
	unsigned int status;		// status word (contains 9 bit fields)
	vcounter_t last_changed;	// raw timestamp T(tf) of last stamp processed [counter]
	vcounter_t next_expected;	// estimated T value of next stamp, and hence update [counter]
	vcounter_t leapsec_expected;	// estimated vcount of next leap, or 0 if none
	int leapsec_total;				// sum of leap seconds seen since clock start
	int leapsec_next;					// value of the expected next leap second {-1 0 1}
};




Research papers describing the RADclock algorithm are ? and ? .


==============================================================
Algorithm Overview and Key Principles
============================

- clock model and terminology


==============================================================
Input and Output
============================


==============================================================
RADclock versus the algo
============================


- algo protection wrt leap seconds

- native  (leap free) Cabs versus UTC Cabs,  relation to radclock_data



==============================================================
Leap second treatment
============================

All leapsecond processing is contained within process_stamp(), which owns and sets
the rad_data member of the handle which defines the RADclock.  To be clear, the
RADclock absolute clock Ca is a UTC clock!

The algo itself sets peer variables, but does not update rad_data. Moreover it
operates natively, that is without any knowledge of leap seconds that have occurred since boot.




==============================================================
RTT Tracking Algorithm
============================


==============================================================
Difference Clock Algorithm
============================


==============================================================
pclocal Clock Algorithm
============================


================================q==============================
plocal Selection Control
============================

	  C_a(t) = p*T(t) + K_p  - thetahat  +  (T(t) - T(tlast))*(plocal-p)    (1)


This has now been sorted:  write text based on DISCUSSoion in RADclock_notepad


radclock.h:
typedef enum { RADCLOCK_LOCAL_PERIOD_ON, RADCLOCK_LOCAL_PERIOD_OFF }	radclock_local_period_t;











==============================================================
Absolute Clock Algorithm
============================


==============================================================
Coping with timecounter (tc) change
============================


KV>2 FreeBSD only:

			err = sysctlbyname("kern.timecounter.hardware", &hw_counter[0],


radclock-private.h:  clock.hw_counter    member  // used in pthread_dataproc.c only

pthread_raw.c:process_stamp
  - is after process_bidir_stamp  which is unfortunate
        (though insane_bidir_stamp will probably catch it )
	   - should be the first thing to do, wont be able to trust stamp, and need
		  to clear stamp queue of data that may never match.
		  
  - comments indicate a work in Progress
  - verbose says  "Reinitializing radclock"  but I see no re-start !

On kernel side, see FFclockDoc_FreeBSDv4 under  windup calls.



==============================================================
Warmup Phase
============================




==============================================================
States and Status
============================





==============================================================
Poll Period changes
============================

- restart/ rehash allows?
- algo still does right?
- how to connect with the adjusted_period  of TRIGGER and the starve_ratio idea?
  should the algo also adopt  adjusted_period  internally?


==============================================================
Configuration
============================

- initial

- under restart
   - init from kernel
   - changing pollperiod





#===================================  Import from  TSCclock/algonotes.txt ===
Raw Data Structure:
-------------------
 - Principal is to be as raw as possible, which by definition means as low in the
sync - user pkt process - user pkt read - kernel read - driver - NIC hierarchy as possible,
in order to test as much as the program as possible under replay conditions.
 - but, want to be as universal as possible, so data can be used to test different variants
now and in future, therefore need to stay OS independent, maybe physical layer ind also..
 - generic requirement (works for RTT or one-way paradigm) for pkt capture is 
   IP pkt and best TSC available.  Only want IP for network level checks, currently TTL.
 - possible other requirements are other TSC values or even SW stamps to try to bracket etc
 - legacy was:  IP in ether in BPF,  with TSC in BPF header.   was therefore specific to 
    all of that, with link layer, capture method details, as well as essential IP and TSC data.
 - want to support pcap, seems ideal soln, gives you IP support with pcap header, which has
   a tval field, perfect.  This tval is moreover taken from bpfheader under BSD, so under 
   BSD already does exactly the right thing.






Parameter Values:
----------------
- Values are chosen to avoid complexity which doesn't add anything.
- Order of adjustments is also important.
  -- warmup is made smaller than shift_win, so after warmup normal shift algo can apply without testing
  -- warmup is made smaller than plocal_win, so after warmup normal shift algo can apply without testing
  -- warmup window is forced to be smaller than hist_win/2, so warmup can forget history window completely
     (must force return in this case as the history variable used to define an arrray size currently)
  -- in fact hist_win/2 made to be bigger than warmup + shift, so get a full shift window after warmup
     before worrying about history.  Means pkt_j never falls inside shift_win and so is never subject to recalc.
- Outcome is:  warmup can ignore plocal and shift and history issues, history can ignore everthing else also


History window:
--------------

- simple in itself, issues are impact on other algos
- is updated in loop for stamp i, but actually occurs before it.  It is transparent to stamp i, 
  and the algo acts as if everything truly began at the new historn_begin, except how the current
  algo initialisations were obtained. 
- no impact on shift window as it can never stretch back far enough to be cut off when the window is
  changed. 


Shift window:
------------
- simple in itself, just a local window, no special treatment whatever, that is reserved
  for reactions of algos to it

RTT updates:
------------
- all three are simple and direct, no special branches.  
- Special stuff taken care of in 
 -- history update (where RRThat copies RTThat_new and RTThat_new resets from scratch) and 
 -- shift detection (where RTThat and RTThat_new are set to RTThat_sh, and presumed to be so
    starting from the shift detection point. 
- note that RTTs could be bad if physical or network layer bugs, eg RTT=0 if incoming stamp field read 
before it has been refreshed.  The best place to check for this is in the network level code. RTT is too
basic to mistrust in the algo (beyond a safety net of insuring it is >0 ).

Warmup:
-------
- Idea here is that RTThat is unreliable, so no point taking error estimates seriously, just do the best you can
- Force use of Delta(t), as this is deterministic and reliable.
- Turn off clever bits, trying to be clever in ignorance is not worth it. 
- Just need to provide initialisation. Even if phat bad, real algo should be able to improve it.
- Main role is to provide RTT history


phat:
-----

- Aim is not to track local phat, but to measure a value which is robust to queueing and TS errors, and
  which will track long term changes.  
- But, changes of the order of 0.1PPM can happen on scales not so far from the SKM scale. 
  Result is a need to measure rate on scales which guarantee
  negligible errors, but which otherwise tracks reasonably
  finely.  Tracking at scales this fine is not an Important end in itself, but is good since:
    - if don't track, differences can built eventually leading to jumps.  This is bad in itself, and also can ruin
      a simple sanity check.
    - local is desirable from the point of view that accuracy in phat is mainly (only?) needed to convert time
      differences anyway, over `local' scales.  If pushing the time scales of the difference clock to the max,
      using a `local value' (like an averaged plocal estimate) will be better than a week long average which
      could be 0.2PPM different.  This is the same logic which motivated plocal, but used for the difference
      clock, not for thetaest. 
    - still, main aim is STability and guaranteed not crazy, don't forget that..  Maybe plocal algo with
      large W is best for the main algo also.. except for level shifts. 


- in warmup, `growing window plocal algo' is conceptually simple, and works, it:
  -- guarantees growing Delt
  -- avoids pbs with errors in pt error values, by not using them, except to find `best local' qualitative no quant.
  -- doesn't try to be too clever when there is not enough data to be anyway
  -- coming out of warmup, initialisation just gives a good pkt_j, but not a reliable pt error for it,
     no Delt supplied for full algo, and error estimate perr also sus, though it includes warmup_win Delt,
     which is reasonable.

- main algo, do have background data, must use
 -- main difference is level shift - without that,  the simple model
    of rtt(k)+noise would work, and the warmup or original algo in the paper would work fine.   
 - Level shift means MUst evaluate estimate of error, but, must not forget wisdom of Delt  
 - new error evaluation method  (|point errs| + |base difference|)/Delt   
   is more robust than simpler ones, but can still be fooled by errors in error estimates. 
 - Must give priority to reliable DelT principle, whilst using error level to guard against level shift errors.
 - Don't be too fussy - lockout must be avoided
 - reinit of pkt_j according to `plocal principles' good:
   - forced refresh, consistent with history idea,
   - guarantees DelT>history_win/2 after first window  !!
   - guarantees get A value in limited time 
   - new error with base difference takes into account retrospective improvement in RTTmin
   - guarantees tracking at history_win time scales, safeguards lockout
 - Modifies pkt_i algo:  
   -- doesn't record last pki_i, not needed.  Are always looking at new i and assessing TOtal quality
   -- accepts if better than last error, OR if EXCelllent. Guards against lockout and errors in error estimates
   -- forces tracking at finer timescales than history_win, see above. 


thetahat:
---------

- no need to wait for large DelT here, warmup can be large wrt to SKM
  scale, suggests starting real algo before warmup end.
- however, still need different warmup algo, since (rarely used) `same th_i ok even after shift' logic
  applied to true shifts, for RTThat errors reassessment is necessary. Eg, 1st stamp will have perfect 
  quality, can't stick with that or it will always have a large impact even when new local min comes along.
- even more generally, true shifts are rare, but RTThat errors and `drift' are common.  Maybe need to make
  retrospective always: but would complicate algorithm. 
    - should relook at this, eg after down shifts effectively have
      warmup up again w.r.t. RTThat ! reassessment makes sense. Now added as option to calc_theta.
- thnaive values also depend on phat, which is also reassessable.  Don't bother with this as phat is
  good enough for a rough pt quality estimate provide i>0.  For i=0 however, hard to give a 
  meaningful value, so waste i=0 stamp for thetahat in order to simplify algo, even though 
  could be used by, eg, using the phat from i=1 or i=0 as well in a special branch for i=1. 
- remember to make shifts entirely transparent to algos:  shifts and histories should be taken into account
  by adjusting underlying RTThat or RTThat history values prior to entering algos.
- end up using stored rtt and RTThat values, could have just stored error, but then when taking into
  account shifts, would have to couple RTT updating and the algo...
- Gap detection 
  -- now continuously detected.  Threshold level only for reporting (and use much smaller value, since rare)
  -- gap now deemed to be relevant if it falls ANywhere inside SKM_win. Thus sanity does not have to think
     hard about exact position.  Important since related theta jump may easily occur elsewhere in the window
     away from the exact gap point - it is still expected to appear in due course
- Quality control refinement based on SD [stil bidir world] (if RTT good =>SD good, but Tb, Te can still be bad,
  eg SD>RTT if Te v-bad) Problem is that large SD may be fine, or may indicate terrible Te, can't always tell.
  To be safe, avoid large SD, but can't throw out too many pkts - RTT and bad Tf are independent!
  - test for SD>RTT and trucate th_naive to avoid non-causality [confident? of direct intervention inextreme case]
  - more typically, want to augment `quality system': add penalty for bad SD to point error via pterr += SD
    -- SD big, RTT big, maybe shouldn't do this (Te ok), but don't select RTT big anyway.
    -- SD small, RTT big, negligible impact plus don't select anyway
    -- SD small, RTT small: maybe shouldn't do it, maybe should, negligible impact anyway
    -- SD big, RTT small: key case, avoids trusting pkts which may have bad Te (and Tb?)
  Note for ideal server where Te~Tb, scheme does no harm, and does avoid nasty outliers.
  Could drop direct intervention since large penalty would stop it being influential anyway, probably...
- SD quality control cannot detect other 1-way type errors:  Tb and Tf can be bad together, currently only sanity detects this. 
- Attempts to detect causality and filter failed:  
  window averaging of thetahats makes causality assessments based on naive values fail. 
  Averaging even non-causal estimates still cancels variability and can work.  BUT:  causality
 outputs seem to show residual drift pb, which is greatly reduced if localp used. Seems `impossible' that they can be so
 bad without some serious error I am missing.... but need DAG to be be sure.. 


sanity:
-------
 - key principle is that it must be truly sanity - extreme events, and not become part of the normal algo.
 - it should be triggered only for true sanity events, not, for example, just because congestion is bad
   and therefore estimation difficult.
 - theta sanity now defined as an envelope,  const + rate-bound*DelT , with two constants. 
   --  Allows deviation from known hardware relative to time interval.  Worse time interval in SKM_win now
       monitored, this is gap detection with no arbitrary threshold, sanity is always made relative - much
       more robust.   
   -- Another form of time interval is time since last 'true' update, ie since a quality 
  estimate was accepted, not just a bad quality repeated one.  This is VERy important. Temporary
  lockout (in fact conservative `holding') due to bad quality is ok, as long as quality will be accepted as soon as it comes along again.
  But if sanity kicks in because drift has occurred in the meantime, then this temporary lockout becomes
  a true lockout..  Time interval sensitive sanity keeps it truly Sanity, so that it will Not be triggered
  even by very bad bad-quality lockouts - essential for high congestion scenarios.  It decouples the 
  sanity algo from the estimation algo in the right way.

To do:
------
 - reinstate gap based alternative bad quality code?  (to make really big gaps look better/recover faster) good idea?
 - fully automate calibration: need to make the following based on data:
     -- hardware parameters  (estimate Allan var + lots of robust addons)
          -- this is essential, sanity triggered if wrong!! and if not aircon'd, is always worse than 1e-7
     -- Eoffset_sanity_min
     -- Eshift
     -- Eoffset:  needs to be realistic to avoid killing ok samples if good data rare

Review of axioms (post Eric & Philip)
-------------------------------------
 Background is need to auto-configure, but estimation of hardware
 parameters not easy, given bad interrupt latency of modern hardware,
 can't even validate or get clean allandev. 

But, wondering if hardware params even used much.  When and where??
what is the best defn of RateErrBOUND?  based on medium scales?  isn't
it about `local rate' changes really?
Sanity:  yes definately, controls trigger points, make a big difference.
BestSKMrate:  does impact ageing of error - impacts if poll_period large -> more warnings..
-------

poll_period dependence:
-----------------------

- all through window sizes ( _win ) variables
  - exception is:  gapsize> (double)poll_period*4.5    but this is ok: verbose loss detector
    (however, gapsize > (double)(SKM_SCALE)  refinement behaviour will vary with poll_period)

- component algos themselves don't depend (RTT, phat, thetahat..), but
  - Which pkts are included does, 
  - algo state (value of i) also depends. In fact shouldn't beyond true Warmup_win = min # samples
  - history storage wrap does [ this handled now]
- pkt memory and accounting all based on unique pkt id, eg:
  - history_{begin,end}, history wrap, offset_win 
  - sometimes this is what you want directly:  
     lastshift = i-shift_win + 1;    this detects actual pkt id you want, if you are going for easy option
- three issues get mixed:  # of samples, time scale, pkt identification
- could get around by converting all to actual time windows:
  - ugly history[i].Tf everywhere
  - computationally intensive
  - sometimes i the natural unit -> still work to do
- possible to allow state to change: eg main -> warmup, warmup -> main
  - but plenty of issues, even main -> main hard, as lose TS control and transition period with 2 polls

- OPTIONS:
 - make everything true-time based and be done with it
   - make truetime during transition period, else pkt window ...
 - just make warmup duration time (plus minimum # samples) based
 - do one-time global fix up after change then continue
   - maybe smooth or freeze theta during this
 - give up and just restart, but with phat frozen in warmup
   - ditto, but extend faststart here, and otherwise

 - combine both:  CAlculate indices using time, then use established indices
   - means warmup doesn't change really!  but recalcuate index corresponding to it
   - make window loops go from i to endwin,  i same as before, endwin a function
   - the function can take a poll transition variable, and be the mechanism for dynamic window reduction

-----------
-----------

Two issues, 4 scenarios:
- running out of history:  check in loop, or in loop control.  Need totrack history end somehow  DONE
- using pkts with appropriate times:  if stop early, can prompt insideloop, else, need to fix up loop before enter
- decoupling of algo window variables, history you have, and size of history arrays DONE

** Poll period
Increase:  so will look too far back : 
Soln: inside loop, detect poll transition, check time and avoid going too far back, if min # samples
      present.  Also need to check pkts are there
      may need to resize history variable(s)   may run out of history  even if don't resize

Decrease:  won't look far enough back 
Soln: ?  check times before loop???


** Window change:  
Increase:  may need to resize history variable(s)   may run out of history even if don't resize
Decrease:  nothing to do, unless you want to size vector down..
 so now I've taken care of histories, nothing to do here??


History use:
------------

Key problems:
 - history in offset_win, plocal_win, shift_win easy to do on-line
 (circular buff), but if change windows, where will new data come from?
 Must either:  keep all (or a long way back)  OR accept transients
 - mapping of time to pkt index can be complex, may fill enlarged
 window but still not go far enough in TIMe
 - changing poll_period doesn't change some windows in time (eg
 offset_win), but may in future, and still involves history pbs

Relation to On-line algos:
 - if entirely on-line, then don't need any extra history (if extra variables defined as reqd)
 - but, can be on-line and yet use history if don't scan over it, ie,
   can use memory but not do heavy computation: is still on-line

OFF-line:
 min, min_slide ALWays used for RTThist
 - min used in:  
     WARMUP: init_plocal,  RTThat_sh init
     MAIN:   
 - min_slide:
     WARMUP: phat
     MAIN:   RTThat_sh, plocal

Remaining issues:
 - take care of time-relevant pkts pb in loops following poll_period change [DONE]
 - sort out whether warmup sacred or not DONE, warmup will flexibly save data and never try to go backwards in time
 recalculate
 - make min's work despite possible lack of history [warmup,  plocal-init.. ]
  shift_win:  DONE
  plocal:  DONE
 - fix lastpacket bit  DONE

WARMUP:   [ easier to store everything here!! ]

history:
phat: 
 - RTT in warmup_win for near_i, far_i (need to store!)
 - stamp for each  near_i far_i

plocal:
 - RTThist in warmup_win 

thetahat: 
 - thnaive, RTT in offset_win
 

MAIN:
history:
RTT: 
phat: 

plocal:
 - stamp for near_i, far_i
 - RTT for plocal_win, near_i, far_i
 - paused restart, or forced start after pause, if data cutoff of
 period poll pb. 
thetahat:
 - stamp, thnaive, RTT, RTThat in offset_win



-----------------------------------------------------------------------------------------------------------------
Algo descriptions:

struct TSCclockdata process_bidir_stamp(struct bidir_ts_tuple *stamp, int poll_period, FILE *matout_fd, int sync_saved, int sig_plocal) 

JOB: - processes a single stamp at a time
     - calculate new phat, C, thetahat candidates
     - return (phat,C-thetahat)    // C(t) = phat*TSC + C,   Ca(t) = C(t) - thetahat;
     - optional algo output to matout_fd

Algo Overview:
   Take in unique stamp i, store, calculate RTT = MAX(1,stamp->Tf - stamp->Ta);   
   (i=0)  
       - check window parameters, alter if necessary
       - initialise algo, generate: (phat,C,0) straight away.  
 
   (i<RTT_warmup_win)  // don't trust anything
       RTThat = MIN(RTThat, RTT);    // simple on-line algo
      - calculate (phat,C,theta_hat)
       - calculate phat
         - check quality
       - calculate plocal (defaults to phat)
       - calculate thetahat
         - check quality
   
    (i=RTT_warmup_win) // finish warmup, init main algo
         
    (i> ...)            // main algo,  turn off various refinements
       RTThat = MIN(RTThat, RTT);    // simple on-line algo
       - keep history window
       - track upward level shifts
       - calculate phat
          - check quality
          - if passed, check sanity
       - calculate plocal  // in selected
          - check quality
          - if passed, check sanity
      - calculate thetahat
          - check quality
          - if passed, check sanity  

    
     Output matlab output
 
     Return (phat,C-thetahat)


---------------------------
WARMUP

JOB:  - good RTTmin estimate
      - necessary initialisations for phat, thetahat
      - less guarantees and checks, but good estimates from the beginning
      - provide history necessary to allow error estimation in full-algo [essential to deal with level shifts].

ALGOs:
-------
 
RTT:  
 Algo:    RTThat = MIN(RTThat, RTT);    // simple on-line algo
 Issues:  don't trust RTThat in warmup
 
phat: 
 Algo:    - estimate is naive  phat: Delta(Server)/Delta(TSC)  between pkts  j .... i 
          - average of forward and backward estimate
          - [j,i] are best pkts in [near, far] windows  
          - windows are a fixed fraction of [0 i] interval (so grow with i)
          - each time phat actually update, C altered to compensate jump in C(t) [ ONly time C altered ]
  
 Issues:  - RTThat unreliable, error estimates also => ovoid error estimation/control
          - just let windows, baseline DelT grow, (deterministic and reliable)
          - Result very robust, but error unquantified
          - EXIT:   good j, and good phat for main algo, but no good perr

plocal:
 Algo:    - do nothing is not active, else just set to phat

thetahat:
 Algo:    - calculate theta naive values for each pkt: [ phat*(TSCa + TSCf) + 2C - (Tb + Te) ] / 2
          - get pt error ET_i  (remember best one)
          - weight wj = exp(-(ET/Eoffset)^2)
          - thetaest = normalised weighted sum
          - if (best one good) 
               accept   // but check for crazy stuff
            else
               repeat previous

 Issues:  - almost full algo, but: 
             -- window grows at beginning
             -- ET's not trusted, recalculated each time (RTThat could have changed) 
          - don't trust checks, so skip them (but report some warnings)
          - can't distinguish bad RTThat's from level shifts, so don't try
          - EXIT:  good thetahat and th_naive history, unless level shift happened.




-----------------------------------------
MAIN ALGO

JOB:  - deliver estimates of (phat,Ca)
      - implement top level history window to forget past (and cap memory and computation)
      - implement efficient and robust code to cope with changes in minRTT
      - add additional checks to ensure stable output regardless of circumstances
      - implement efficient sanity checking as a catch all - without interferring with normal operation
  
      - reduce dangerous dynamics by avoiding feedback, each part of algo
        [history, RTT, level shift, phat, plocal, thetahat ]
        acts transparently for the parts which follow, so they do not need to know state to operate
        [however, they do rely on previous parts having done the right thing!]


ALGOs:
-------
 
History:
 Algo    - maintain large window h_win (~1 week), at h_win/2, discard oldest half window
         - Two algorithm parts need adjustment at half window end:
           - RTT: on-line algo prepares replacement RTThat
           - phat:   on-line algo prepares replacement pkt_j and associated error and RTThat used
         - Init: - simple one at end of warmup
                 - first full one at first half window.  
                * - phat: decrease quality thresold a lot, become fussier
         - take opportunity to output some summary quality and summary stats
 Issues: - some recording of RTT values requires level shift algo to have applied corrections correctly

RTT:  
 Algo:    - update RTThat as before, and store RTThat values offset_win back (history of past state)
          - update RTTmin_new for new hist window
          - update RTTmin_sh over local shift window
 Issues:  - history assumed handled correctly already
          - RTTmin_sh just the min over a simple window, no state.
 
Shifts:
 Algo:    - only upward shift needs explicit code, downward is automatic
          - upward shift `detected' simply if  RTThat_sh > (RTThat + sh_thres) 
          - reactions needed for 3 parts of algo:  
            - RTT:  RTThat and RTThat_new reset, to RTThat_sh
            - phat: if on-line newpkt_j in shift window, recalculate its (error,RTT)
            - thetahat:  correct RTThat history back to shift point
 Issues:  - shift_win must be large to be conservative: must be certain of upward shift as consequences severe
          - close to no reaction needed for algos, as they incorporate resulting errors naturally if RTThat treated correctly


phat: 
 Algo:    - [j,i] chosen differently to warmup 
            - pkt_j chosen as best in wide window jsearch_win in far past (DelT > h_win/2 once past 1st h_win/2)
            - pkt_j recalculated on-line as part of history part 
          - Each i looked at with pkt_j constant (established in previous h_win/2)
            - if point quality not good, forget it, no update
             - if good, look at `pair quality', combining: both pt errors, `baseerr' (RTThat mismatch), DelT
             - if total error best yet, OR very small, accept
              - estimate calculated by averaging forward and backward naive as before
          - sanity checks
            - main test:   'impossible' relative difference from current value
            - triggered by qual_warning passed in from network layer
            - check NaN craziness
            - if pass, finally update phat
          - As before, C altered each time phat actually updated
          - Init:  -- pkt_j set as best in warmup_win
                   -- simple perr based on [j,i] from warmup phat algo calculated

 Issues:  - long history allows many samples for choice of [j,i], and very large DelT
          - complexity controlled by avoiding all (j,i) combs, using on-line precalculated fixed pkt_j and fast initial filtering on pkt_i
          - pkt_j algo also guarantees pkt_j found, in finite time, preserving large DelT
          - including baseerr incorporates level shifts and RTT estimation effects in one hit
          - very low error on pkt_j likely, large DelT guaranteed, so can wait for suitable i based on total quality
  

plocal:
 Algo:    - just as for phat in warmup, except DelT base fixed at plocal_win
          - near, far windows constant width,  fraction of plocal_win
          - error assessment based on RTT point quality, if not good, repeat past value
          - sanity checks
            - main test:   'impossible' relative difference from current value 
            - triggered by qual_warning passed in from network layer
            - check NaN craziness
            - if pass, finally update plocal
         - Init:  -- same algo, minus the quality assessment.
                  -- uses data from warmup window

 Issues:  - warmup window is big enough by design
          - must control DelT to control timescale: want LOcal p  estimate
          - hence, can't control quality, only check if ok.
          - unfortunately, local_win bigger than offset_win, to get enough samples
          - *** behaviour under level shifts (up And down) ok??  should add baseerr innovation here too?
      

thetahat:
 Algo:    - calculate theta naive values for each pkt as before 
             - apply safer alternative if RTT<SD
          - detect large gaps [gap parameter allows slightly different rule if gap large, helps fast recovery]
          - get pt error ET_i  (remember best one)
             - age point error using pessimistic rate error
             - add SD quality component
             - track worst gap in window (for use in sanity)
          - weight wj = exp(-(ET/Eoffset)^2)
          - thetaest = normalised weighted sum
             - if using_plocal,  correct for relative rate difference using it
          - if (best one good) 
               pass candidate thetahat_new
            else
               repeat previous (if big gap, emphasis new data more)
          - sanity checks
            - main test:   'impossible' relative difference from current value
            - triggered by qual_warning passed in from network layer
            - check NaN craziness
            - if pass, finally update
              - if also good quality, record as current quality most recent value
         - Init:  - initialise entire RTThat window to best RTT in warmup_win
                  - set last reliable estimate to current

 Issues:  - `lockout' avoided by decoupling sanity and quality checking
          - sanity checks relevant to age of last REliable estimate, and takes note of ANy gaps, to avoid lockout
    




------------------------------------------

Principles:
 - based on feedforward structure, not feedback. Each element designed
 to prepare ground for next, so next step can operate correctly using
 previous elements directly without knowledge of any extra `state'
 - Eg:  history operates before and independently of RTThat code (but may change it)
        RTThat operates before and independently of shift algo
        shift window operates independently of above, but fixes RTT if triggered.
        shift window operates independently of phat and thetahat
        phat   operates independently of thetahat
        plocal operates independently of thetahat
        thetahat is quasi-decoupled from phat, independent of shift algo

-------------------------------------------------
Constant Baseline
-----------------

RIPE implementation experience shows that an unwarranted shifting baseline in OWDs
is troubling. 

Three factors at work:
 - Server change (SC): change in server and/or route resulting in a change in both r and/or asymmetry 
   =>  jump in the TSCclock, and hence in all OWDs measured using it to any other host.
 - Host model failure (HM):  a host can enter into a rare state with
   lower min RTT. r_i=r+ positive noise still holds, but  
   => `effective' DeltaH is different, either adjust to make validation meaningful, or admit the TSCclock makes a constant error.
   => if level shift large, will result in persistent poor quality
   => To fix these need for reaction(s) to downward level shift, and hence detection.
 - Negative delays (ND):  Incorrect values of Delta in TSCclock hosts
   => delays to some boxes can be negative!   Likely when hosts closer to each other than to their servers.


SC:  Can preserve baseline by selecting new Delta to compensate for
jump. This needs data following shift to measure jump. On upward side, lots already
available using existing detection method. 

Issues for upward direction:
  Detection:
    - based on changes in r, NOT changes in Delta
      -- (i)   r may change without Delta (no need to compensate, but may add noisy estimate of zero!)
      -- (ii)  Delta may change without r (in which case no detection method available, but very unlikely)
  Reaction:
    - r:  already have
    - Delta:  measure jump delth in thetahat over known detection point, set Delta+=delth
      -- recalc thetahat from jump point.

Issues for downward direction:
  Detection:
      - based on changes in r, NOT changes in Delta, same four possibilities
  First need downward detector of r, this is simple and obvious, but new, note:
      - effectively RTThat starts anew, no pkts! need time to get
        reliable, need reassessment using best RTThat available for ET in theta algo
      - r Does change at that pt, so can measure asym changes there
      - so, detection is immediate and reliable, but measurement of new r takes time!

  For change in asym calc:
      - detection immediate, so thetaleft can be measured immediately
      - need to wait for right window to build up, so still have delay [but needed for r anyway]
      - since new r unreliable, may detect multiple down shift for one
        real one, end up fixing asym for only the last of these! 
        Under the assumption of isolated true shifts, need to remember
        first one. Need to define a downshiftwindow for this.

HM:  
Key pb is no direct access to HostRTT in algo, detection must still use RTT.
In many cases, clear HM events up to 50 mus are invisible in RTT and so do not determine Any of the RTT shift events.
All can do is invent a new class of upward shift, triggered by HM and maybe other route 'blip' events. Defn:
  - shift down from r to r', followed 'immediately' by shift up again to same level, ie:
     -- test in a blip window, starting at i=lastdownshift,  to see if above r 
  - much more specific than normal upshift detection as r,lastdownshift given
  - blip_win can be small, so even feasible to freeze thetahat until blip test over to maintain baseline on-line.
     -- maybe freeze always to avoid pkt at transition (whose pt
     quality will look great) creating problem, eg bad r estimate, or isolated asym change actually present in naive_theta
  - but real danger of it reversing normal RTThat improvements, especially
    under congestion, unless thres high, in which case would already be caught up normal upshift with a bit more patience..
    So in fact, trading off extra info enabling more reliable detection against blip window length and threshold, but won't perform magic. 
  - could be used to veto asym correction at end of offset window.. if looks like downshift was a fake. 


ND:
Moving from A=0 to A= -r:
  - estimates of -r will be too small, breaking the bound and causality!
  - won't stop -ve OWDs


-------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------

Linux wish list:
- Release support:  architecture to support automatic patch and pkg generation for new kernel versions
- Finish TODOs in the code [ but careful, only important ones ]
   - remove or isolate and identify GPL components
- General and efficient TSCclock mode support

- measurement of DeltaH 
- API for PPS (and other support for broadcast paradigm)
- Progress on influencing linux kernel community:
   - TSCclock modifications
   - stability of such given probable evolution
   - generic API to support diff & abs clocks (C libraries?)  [don't emphasise, keep specific]
- Dual CPU support
- Frequency change support


===========================================================================





=================================================================================
=================================================================================
=================================================================================
TODO:  Other major documentation topics:

 
====================================
Urgent

Package building
 - autohell guide, versioning, cheatsheet


RADclock server (NTP_SERV)



====================================
Can wait

RADclock Package Overview
	- include man pages

NTP packet generation (

 detail)


IEEE1588 support

VM support




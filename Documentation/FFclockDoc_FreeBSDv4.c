/*
 * Copyright (C) 2016-2020, Darryl Veitch <darryl.veitch@uts.edu.au>
 */

FFclock FreeBSD V4 documentation.
Written by Darryl Veitch
================================

Convention to point within code:
	/dir/dir/file : functionorstructureinfile

Glossary:
 KV = Kernel Version  ( of the RADclock/FFclock modified kernel )
 IPC = Inter-Process Communication
 OS = Operating System
 SHM = SHared Memory
 FF = FeedForward
 FB = FeedBack
 FFcounter = a FF compatible counter
 algo = algorithm, the RADclock clock synchronization algorithm
 tc = "timecounter"
		or the current timecounter structure variable
      or the current counter available in the current tc variable
 Ca = Absolute FF clock
 Cd = Difference FF clock



Works in Progress:
  - LERP:  the current algorithm, use of poll_period estimator, and the question of
     daemon versus kernel responsibility
	 Urgent, in that the FF clock that most will see will be a LERP version, so want it to be well
	 behaved in both abs and skew senses !  needs more thought !
  - clock error system:  confusion and incomplete and incorrect code both at FF, and daemon level
    But less urgent, since few will use this anyway !  much of it is advanced use.
	 Could clean up some aspects with a little thought, others may require much more work.
  - leapsecond, a lot of stuff missing, lots of questions, kernel<--> daemon  divide issues
    How does the fftimehands and system clock handle it now ???  important qn.
  - clock status:  barely used, not clear if fully consistent across daemon<-->FF divide .
    Linked to issue of passing next_expected or poll_period, or not
  - PPS:  does that code work??  what does it really do on the FF side ?? we dont have
    an S1 algo..  so how does FB handle S1 useing timehands?  that is the qn.
  - correct reaction to a tc change

==============================================================
Summary
=======

This documents the FFclock freeBSD kernel code, focussing on kernel version 2
and above.  The code includes the synthesis of a FF compatible counter, support
for a full featured FF system clock interfacing with a FF clock daemon,
in-kernel FF packet timestamping support with multiple variants (including one
which functions as a difference clock), and the provision of a universal FF/FB
system clock interface.
The main source files are in RADclockrepo/kernel/freebsd/FreeBSD-CURRENT/
	kern_ffclock.c
	kern_tc.c
	bpf.c
The main associated header files are
	_ffcounter.h
	timeffc.h
	bpf.h
	timetc.h  [ no FF changes ]

On the radclock library side, the main files in RADclockrepo/radclock/libradclock/ are
	radapi-base.c
	radapi-pcap.c
	{kclock,ffkernel,pcap}-fbsd.c
The main associated header files are
	kclock.h
	radclock.h							// external processes include this to use RADclock

In addition there is related code in RADclockrepo/radclock/radclock/ in
	radclock_main.c
	sync_bidir.c
with
	sync_algo.h


==============================================================
Introduction
============

The RADclock daemon, and libprocesses, fundamentally rely on two kinds of kernel support :
	- the synthesis of a FF counter (unless a reliable TSC was already available), and the
	  ability to read it from userland
	- an ability to kernel timestamp outgoing and incoming NTP packets using the
	  FF counter, and to access these from userland
The FFclock kernel code supports these but goes well beyond them in three ways.
It
	1) provides support for a system clock of FF type.
That is, it provides the functionality expected of a system clock, including monotonic
progression, but based on the principles of a FF clock with its separation of
Absolute and Difference time, built on top of an FF counter.
The FF clock is disciplined by a userland daemon algorithm via an API, and is not
RADclock specific (though clearly directly inspired by it).

	2) generalizes the system clock interface to be `universal`, that is capable of
being either FB or FF driven, with the ability to switch between (and compare) them,
provided suitable daemons are available and running.

	3) improves packet timestamping locations (within the bpf subsystem), in order to
improve efficiency, work within the new universal interface, and benefit from the
extra potential of the FF clock if selected.

All of the above has been integrated into the existing system clock support,
which is in turn integrated into the timecounter (tc) mechanism (both centred in kern_tc.c),
which manages the hardware timecounter wrapping issues for the FB system clock
and allows the selection of different hardware (time)counters.
A consequence of following the existing tc approach, combined with providing the
needs of a system clock, means that the FF implementation architecture does not follow the
linear expectation of: (FF system clock) on top of (FF clock) on top of (FF counter) .

As FFclock support is not yet `mainstream`, the FF code is written to be protected by the
FFCLOCK guard macro.  However, even if FFCLOCK is not defined, the non-FF portions of the
universal (Uni) code remain defined.

For a complete understanding of how the FFclock interacts with radclock as the
canonical FF daemon, the interactions between the two are also detailed.
Whereas RADclockDoc_library  documents the types and purposes of the
kernel <--> daemon interactions in general terms, here we give the detailed
technical connections to the algo, explaining why things are structured as they are,
and explain the ffclock kernel code.

The kernel versioning parameter lives in the pure FF file kern_ffclock.c
	static int ffclock_version = 3;     // declare value of KV, the kernel version of the FF code
and is accessible by a daemon via the FF-expanded sysctl tree.



==============================================================
FFCLOCK symbol and File Breakdown
======================================

Here we survey all files touched by FF code, whether it be new FF code, or modified
non-FF code.

The FFCLOCK guard macro controls whether FFclock support is available for compilation.
More than that, it protects almost all FF related code, so that if the symbol is not defined,
what you (almost) have is a kernel prior to the FF code enhancements.
The main exception is the universal sysclock (FF/FB) support which remain active (see below),
though with limited functionality if FFclock is FALSE.  The other exception is in
the generalized packet timestamping mechanism in bpf.

The main FF header file is  timeffc.h  and is included in all the key FF modifed files:
kern_{tc,ffclock}.c, subr_rtc.c, bpf.c .
It itself includes  _ffcounter.h which provides the kernel''s FF counter type:
typedef uint64_t ffcounter
The FF components in header files, including structure and macro definitions,
are in general unprotected, but with some exceptions.


The FFCLOCK symbol will be defined if a line containing :
options FFCLOCK
appears in the architecture specific conf file, at /usr/src/sys/amd64/conf  in the case of amd64.
If defined, this is then automatically noted in each of
?/opt_ffclock.h
	included in kern_{tc,ffclock}.c,  subr_rtc.c, bpf.c  	// used to convey knowledge of symbol
conf/options:
	FFCLOCK
conf/NOTES:
	options 	FFCLOCK

/* TODO:
  describe where option is set, content of  opt_ffclock.h
  explain connection to kern_ffclock.c:  FEATURE(ffclock, "Feed-forward clock support");
*/


To track the FF code and its compilation control we define the following:
	Fully Protected:		new pure FF functions, or defines, protected by FFCLOCK
	Protected:				old functions with new FF components protected by FFCLOCK
	Protected from:		old functions with some components excluded by FFCLOCK (protected by !FFCLOCK)
	Unprotected:			new FF components (in or out of functions) unprotected by FFCLOCK
	No FF content:			other function with no FF content listed for completeness

First consider the main FF files  kern_ffclock.c  and kern_tc.c .
Since the FFclock makes use of legacy system clock code, in particular tc code, but not
vice versa, excluding FF code is mainly trivial.

kern_ffclock.c:      [ the higher level pure-FF code ]
 Fully Protected:
	  Clock reading functions							// see section:  FF and system clock reading
		 ffclock_{abs,diff}time
		 ffclock_{bin,nano,micro}difftime
		 ffclock_[get]{bin,nano,micro}[up]time
	  syscalls												// see section:  Syscalls
		 sys_ffclock_{getcounter,{get,set}estimate}
	  sysctrl callbacks									// see section:  SYSCTL
		 sysctl_kern_sysclock_{active,available}
 Protected from:
	  syscalls
		 sys_ffclock_{getcounter,{get,set}estimate}	// dummys defined if #ifndef FFCLOCK


kern_tc.c				[ the lower level FF code integrated with legacy tc code]
 Fully Protected:
	  New FF functions:
		 ffclock_{init,reset_clock}
		 ffclock_{read_counter,windup,change_tc}
		 ffclock_{convert_{delta,diff,abs}}
	  New functions supporting the Universal FF/FB sysclock framework
		 fbclock_[get]{bin,nano,micro}[up]time      // new names for legacy functions
		 [get]{bin,nano,micro}[up]time				  // new definitions of these legacy names
		 [get]{bin,nano,micro}[up]time_fromclock    // switching wrappers

 Protected:
	  All FF additions to legacy functions:
		 tc_{init,getfrequency,setclock,windup}		// tc system
		 sysctl_kern_timecounter_{hardware,choice}	// systctl callbacks
		 pps_{ioctl,init,capture,event}              // pps system
		 inittimecounter                             // calls ffclock_init()
	  New Universal (FF/FB) functions that nonetheless allow FF disabling using FFCLOCK :
		 sysclock_{snap2bintime,getsnapshot}   	   // timestamp splitting: {raw+state,read clock}

 Unprotected:
	 	sysclock_{snap2bintime,getsnapshot}   		   // non-FF parts of these
		sysclock_active 										// universal sysclock:  records active clock

 No FF content		(neither direct nor indirect)
	  CPU tick handing system:
		 tc_cpu_ticks, cpu_tick_{calibration,calibrate}, set_cupticker, cpu{_tickrate,tick2usec}



The only other affected file which is not associated to packet capture is :
subr_rtc.c			[ Real Time Clock processing ]
 Protected:
   inittodr 											// simply calls ffclock_setto_rtc



=======================================================
Packet capture requires additional components to support FF and Universal timestamping

bpf.h
 Protected
 		# define BP 			// usage unclear
 Unprotected
 		New timestamp type macros: BPF_T_{[NO]FFC,*CLOCK, ...}
		New ffcounter members of  struct bpf_{x,}hdr

bpf.c
 Partially protected:
 		catchpacket	(declaration and defn)	// copying ffcount into hdr, new parameter to fn
 Protected from:
		bpfioctl  									// disable some checks on timestamp format
 Partially and UnProtected:
		bpf_{tap,mtap,mtap2} 					// new Uni sysclock split timestamping
 Protected by !BURN_BRIDGES
		struct bpf_hdr32							// 32-bit specific version of ffcounter enhanced bpf_hdr
 Unprotected
 		CTASSERT statement 						// keep eye on minimal size of bpf timestamp fields
		#define SET_CLOCKCFG_FLAGS				// macro `function' processing CLOCK related flags

/* TODO:  finish updating bpf part of doc */

=======================================================
Drivers need modifications to ensure timestamping occurs in the right place.
Here changes are not protected by a guard macro, instead a "\\FFCLOCK"  comment
flags them, but we describe them using the {Un}protected etc notation.

dev/e1000/if_em.c		// changes built on 9.0.0, quite a few other changes in 9.2, 10
  Unprotected
		em_xmit										// new argument allowing internal call to ETHER_BPF_MTAP
		em_{,mq}_start_locked               // call to new em_xmit
  Protected from:
		em_{,mq}_start_locked               // commenting out call to ETHER_BPF_MTAP
		em_initialize_receive_unit          // E1000_WRITE_REG: disable interrupt throttling setting with


Here changes are flagged by the "\\ben" instead of"\\FFCLOCK" :
dev/e1000/if_igb.c	 // changes built on 9.0.0   File missing from watson
  Exactly analogous changed to if_em.c, just applied to igb_xmit instead of em_xmit .


In addition, both if_{em,igb}.c have associated DTRACE code, flagged by "\\ben" :
		igb_rxeof					: just adds a DTRACE probe, not related to FF
		  SDT_PROBE(timestamp, if_igb, igb_rxeof, stamp, sendmp, 0, 0, 0, 0);

/* TODO:  finish updating this part of doc, and port to missing driver */


=======================================================
Automatically generated files
	init_sysent.c
	freebsd32_sysent.c
	freebsd32_{syscalls,systrace_args}.c
	          {syscalls,systrace_args}.c

	freebsd32_{syscall,proto}.h
	          {syscall,sysproto}.h

=======================================================
For completeness, other directly relevant files with no FF content
	kern_ntptime.c 				// NTPd API, includes {sys,kern}_adjtime
		ntp_update_second()		// called in tc_windup



==============================================================
Timecounter (tc) Mechanism: Overview
======================================

Unless stated otherwise, the functions and declarations here are found in
kern_tc.c and timeffc.h .

The timecounter mechanism was introduced to allow the system clock to be based on any
one of the available hardware counters.  Most such counters are only 32 bits wide, which
would wrap in only ~4.3s for a 1Ghz oscillator.
As a result they must be read regularly and transformed to a longer-lived form to be
useful for timekeeping.  It is natural in Unix OSes to do this through basing the system clock
architecture on the periodic interrupt cycle of frequency hz, which schedules system tasks
on a grid of width 1/hz seconds.  The interrupt cycle already has
links to platform hardware timing independent of the system clock itself.
The beginning of each cycle (or abusively, the cycle itself) is known as a `(hardclock) tick`,
not to be confused with the period of the underlying oscillator!
In modern processors hz=1000 typically, corresponding to an interval 1/hz = 1ms between ticks.
The variable kern/subr_param.c:tick  is the nominal number 1000000/hz of mus per tick.

The tc code is based on maintaining state which is updated on a roughly
periodic tc_tick grid.  The value of the global static int tc_tick  is set in inittimecounter().
It is linked to hz, to be morally equal (or just larger than) it to ensure that
tc-tick updates do not occur more frequently than hardclock ticks.
It is expressed however in the number of hardclock ticks per ms (see sysctl_kern_timecounter_tick),
and used as a `timeout` parameter in tc_ticktock().
For convenience, henceforth we use `tick` to refer to tc-ticks, and hardclock
ticks otherwise.

Events asynchronous to a tick relevant to the state are processed at those times.
Clock reading at some arbitrary time t is performed by accessing the state at the current tick,
and by interpolating from the tick start-time to t.  However as ticks occur
frequently, the state may be updated, perhaps several times, before calculations can complete.
To ensure that calculations can complete efficiently with consistent data, rather than
chasing the latest data in an expensive (perhaps infinite) loop, they are based on a
self-consistent snapshot of tick state.  To ensure it has adequate lifetime, a cyclic
buffer of tick-states is used so that tick updates can proceed without delay or concerns about
calculations in train, while (slightly) out of date, but still valid, snapshots
remain available for such calculations to complete.

The tick state data structure, designed to support the traditional FB system clock, is

struct timehands {
	struct timecounter	*th_counter; // tc used during this tick [ counter access + other tc data ]
	int64_t		th_adjustment;        // adjtime(2) adjustment: ns/s as a 32 binary fraction
	uint64_t		th_scale;          	 // period estimate as a 64bit binary fraction
	u_int	 		th_offset_count;   	 // counter value at tick-start (wraps)
	struct bintime		th_offset;      // uptime timestamp at tick-start (no overflow)
	struct bintime		th_bintime;		 // UTC bintime  version of th_offset		[ new in 11.2 ]
	struct timeval		th_microtime;   // UTC timeval  version of th_offset
	struct timespec	th_nanotime;    // UTC timespec version of th_offset
	struct bintime		th_boottime;	 // boottime value for this tick				[ new in 11.2 ]
   volatile u_int		th_generation;  // wrap-count to ensure tick state consistency
	struct timehands	*th_next;       // pointer to next timehands variable in cyclic buffer
};

The basic idea is that uptime timestamps are calculated based off the tick-start as :
	C(t) = th_offset + th_scale*∆(tc) ,
where nominally ∆(tc)= tc(t) - th_offset_count ,  the number of ticks since the
value of the counter recorded at tick-start.

The timehands variables are defined as externals in kern_tc.c :
	static struct timehands th{0,1,2,...,9};              // individual tick state containers
and the one holding the current tick is pointed to by :
	static struct timehands *volatile timehands = &th0;   // always points to current tick
with the circular buffer being created by linking the *th_next members in a loop.
Each timehands in the buffer has a generation variable, which records the number
of buffer-wraps it has seen (= # of times it has been overwritten), initialized to zero.

Also defined are the variables defining the uptime (time since boot) timestamps:
struct bintime boottimebin;		// UTC time of boot, bintime format
struct timeval boottime;			// UTC time of boot, timeval format
The convention here is that the native FB variables (eg th_offset) are uptime clocks.
To get UTC versions one must add boottime values to them.



The FFclock has been designed to work within the above mechanism, by adding FF
structures and functionality to it without materially altering the FB components.
Thus the FF clock is tick based and offers analogous operation to that of the FB clock,
and shares the same tick update triggering, but supports/creates an entirely separate clock family.
The FF version of timehands structure is

struct fftimehands {
	struct ffclock_estimate	cest;      // copy of kernel FF data from daemon used during this tick
	struct bintime		tick_time;       // UTC Ca timestamp at tick-start
	struct bintime		tick_time_lerp;  // monotonic version of UTC Ca at tick-start
	struct bintime		tick_time_diff;  // version of Ca which drifts, never jumps, approximating Cd
	struct bintime		tick_error;		  // bound in error for tick_time
	ffcounter		tick_ffcount;		  // FF counter value at tick-start (doesnt wrap!)
	uint64_t		   period_lerp;        // adjusted period used by monotonic version
	volatile uint8_t		gen;		     // wrap-count to ensure tick-data consistency
	struct fftimehands	*next;        // pointer to next fftimehands variable in cyclic buffer
};

The basic idea is that UTC timestamps are calculated based off the tick-start as :
	Ca(t) = tick_time + cest->period*∆(T) ,
where nominally ∆(T)= T(t) - tick_ffcount,  where T is the FFcounter which does not wrap.
This matches the formulation of equation (2) below for the RADclock absolute clock.
The actual implementation however is somewhat different, with ∆(T) being just the same
wrap-managed  ∆(tc) as above (more below).

The daemon''s FF clock algo is natively UTC, but with no leapsecond correction since launching, and
is not guaranteed to be monotonic, though it typically will be except (about 50% of the time)
during short intervals following updates.
Since most common uses of the system clock require and/or expect monotonicity,
a monotonic version must be provided.  The state for this is held in fftimehands, the idea being:
	CaMono(t) = tick_time_lerp + period_lerp*∆(T)
such that it is guaranteed to be monotonic, continuous at tick-start times, with bounded skew,
and close to Ca(t) near the end of ticks.  The definition of CaMono is
based on LinearintERPolation, and is contained in ffclock_windup.

In addition, it is desirable to provide difference-clock functionality.
To this end tick_time_diff is defined. It is a clock which begins as Ca(t), and updates
period estimates in the same way, but does not include offset corrections, and ignore leaps,
and never accepts corrective jumps for any reason.
It therefore drifts. Because new period values only operate inbetween FFdata updates,
calculating time differences over some interval is equivalent to using a
modified Cd, where the period used is an average of those over the interval, rather than the most recent.
It therefore achieves the elusive `difference clock via an absolute time call` holy grail.
As this clock can be user-selectable as BPF_T_FFDIFFCLOCK within the bpf timestamp type, it
enables applications requiring the difference clock to obtain it without the need to
modify existing code based on absolute timestamp code.

In summary, fftimehands provides five things:
  i) a way to define an FF counter via tick based updates to tick_ffcount and the existing ∆(tc)
  ii) a way to support safe tick-based reading of the FF clock Ca
  iii) the state needed to support a monotonic version of Ca
  iv) a way to obtain difference clock functionality from a (drifting) absolute `UTC` timestamp
  v) a fast way to obtain an FF timestamp with tick-accuracy: namely tick_time{_lerp}

A circular buffer is created in a similar way to timehands :
static struct fftimehands ffth[10];								// buffer
static struct fftimehands *volatile fftimehands = ffth;  // pointer to current fftimehands
where the initial circular linking is performed neatly by ffclock_init().
after initializing the entire array to zero using memset.

The cest member variable holds the data of the (kernel version of the) daemon''s FF clock.
It is important to note that it will be updated on a poll_period timescale set by
the daemon, whereas fftimehands will advance to fresh ticks independently of this,
and 1000''s of times more frequently.   Each member of the cest is initialized to zero by
ffclock_init(), including the update_ffcount variable, which remains zero unless
ffclock_setto_rtc() is called, or a true update pushed by the daemon, and is reset
to zero if the timecounter changes.

The timehands and fftimehands systems are separate, with separate generation variables,
however they update on ticks in a synchronous manner, because the timehands update
function: tc_windup, calls that for fftimehands: ffclock_windup.


/*
TODO:
  - consider a name change LERP -> MONOFF  though could be confused with MONOTONIC,
    and now the BPF_T_FFDIFFCLOCK is available, which is even more monotonic...
*/


==============================================================
FF algo data kernel <--> daemon translation
======================================

The FF code defines a data structure, ffclock_estimate, defined in timeffc.h
(compile `backup` in kclock.h using vcounter_t instead of ffcounter),
which contains the key information which the kernel needs
to record the state of, and interact with, a FF clock algorithm.
This includes being able to read such a clock when doing so
via the daemon is impossible (daemon dead) or too costly (within the kernel).
Essentially, this structure holds a kernel-specific version of the current
FF clock parameters  (historically called the `global data` in KV<2), which should be updated
whenever a new stamp is processed by the algo.

struct ffclock_estimate {
	struct bintime	update_time;	/* FFclock time of last update, ie Ca(tlast) */
	ffcounter	update_ffcount;	/* Counter value at last update */
	ffcounter	leapsec_expected;	/* Estimated counter value of next leap sec */
	uint64_t	period;					/* Estimate of current counter period [2^-64 s] */
	uint32_t	errb_abs;				/* Bound on absolute clock error [ns] */
	uint32_t	errb_rate;				/* Bound on relative counter period err [ps/s] */
	uint32_t	status;					/* Clock status */
	uint16_t	secs_to_nextupdate;	/* Estimated wait til next update [s] */
	int8_t	leapsec_total;			/* Sum of leap secs seen since clock start */
	int8_t	leapsec_next;			/* Next leap second (in {-1,0,1}) */
};

As the architypal FF algorithm, we will describe this data structure in relation
to the fields of the algo''s radclock_data structure.
The critical conversion functions between these two are in radapi-base.c :

//  radclock_data --> ffclock_estimate		[ FF <-- algo ]
void fill_ffclock_estimate(struct radclock_data *rad_data, *rad_err, struct ffclock_estimate *cest);
//  ffclock_estimate --> radclock_data		[ FF --> algo ]
void fill_radclock_data(struct ffclock_estimate *cest, struct radclock_data *rad_data);

Details of when these are called within the various kernel <--> daemon communications
are given in RADclockDoc_library.  Here we detail the meaning of the fields and
justify the mappings applied in these functions.

The translation is non-trivial, because
 - some radclock fields are not fully FF generic, and so need to be translated/removed
 - to support operations on ffclock_estimate in the kernel without floating point,
   fields are converted to binary and integer friendly forms
 - radclock_data is based on underlying parameters which can be used to construct/read the time,
   ffclock_estimate is oriented toward the fftimehands interface view: tick-start-time + ∆,
	needed to simultaneously support reading the FF clock, and the FF counter itself, and other features.
 - leap second fields are needed on both sides, but on the deamon side the algo
   is leapsec oblivious, this is handled in main, and is a work in progress.

On the daemon side, see RADclockDoc_algo_bidir for more details on Ca, and the defn
of bidir_peer in sync_algo.h for more parameter insights.


The defining RADclock data structure  radclock_data  is defined in radclock-private.h.
The global variable of this type is the rad_data member of the global clock_handle,
often accessed via the handle macro (defined in radclock_daemon.h):
#define RAD_DATA(x) (&(x->rad_data))

To understand radclock_data, recall that the underlying model for reading the
RADclock Absolute clock Ca at true time t is
						C_a(t) = p*T(t) + K_p  - thetahat  +  (T(t) - T(tlast))*(plocal-p)    (1)
						       = C_a(tlast) +  (T(t) - T(tlast))*plocal                       (2)
where T is the vcounter value,  tlast is the time of last update, K_p is a large constant setting
to origin to the UTC epoch relative to p=p, and plocal is a `local` estimate of the
counter Period which may default to the longer term p estimate.
The two forms, (1) and (2), are equivalent in terms of reading the clock, but not in terms of
1-1 correspondance of parameters. There is a one-parameter family of (p,K) values giving the
same C_a(tlast) value, so (p,K) cannot be inverted from C_a(tlast).
After each stamp is processed, the parameters (p,plocal,K,thetahat) are updated,
and stored in rad_data, along with some associated error estimates and metadata:

struct radclock_data {
	double phat;				// very stable estimate of long term counter period [s]
	double phat_err;			// estimated bound on the relative error of phat [unitless]
	double phat_local;		//	stable estimate on shorter timescale period [s]
	double phat_local_err;  // estimated bound on the relative error of plocal [unitless]
	long double ca;			// K_p - thetahat(t) - leapsectotal? [s]
	double ca_err;				// estimated error (currently minET) in thetahat and ca [s]
	unsigned int status;		// status word (contains 9 bit fields)
	vcounter_t last_changed;		// raw timestamp T(tf) of last stamp processed [counter]
	vcounter_t next_expected;		// estimated T value of next stamp, and hence update [counter]
	vcounter_t leapsec_expected;	// estimated vcount of next leap, or 0 if none
	int leapsec_total;				// sum of leap seconds seen since clock start
	int leapsec_next;					// value of the expected next leap second {-1 0 1}
};


/* Identical Fields   [ FF side  ~=?  algo side ] */
update_ffcount = last_changed
	However their types are given different names.
	_ffcounter.h:typedef uint64_t ffcounter;
	  radclock.h:typedef uint64_t vcounter_t;

status = status
	However their types (uint32_t and unsigned int) may not be identical.
   Values on the daemon side are radclock specific
   STARAD_{UNSYNC,WARMUP,KCLOCK,SYSCLOCK,STARVING,{PERIOD,OFFSET}_{QUALITY,SANITY}}
	In future, these should be mapped to more FF-generic values.
	The kernel has separate status system (undeveloped) including
	timeeff.c:FFCLOCK_STA_{UNSYNC,WARMUP}  and these interact and correspond:
	{STARAD,FFCLOCK_STA}:  _UNSYNC = {0x0001,1}  _WARMUP = {0x0002,2}

leapsec_{expected,total,next} == leapsec_{expected,total,next}
	However their integer types are potentially different.
	Respectively:   {ffcounter,int16_t,int8_t} <--> {vcounter_t,int,int}


/* Corresponding Fields [same except for format conversion (with negligible rounding error) ] */
uint64_t period == double phat_local
	period is stored as a binary fraction, just like the 64bit frac component of bintime format.
	In other words, an integer number of units of size 2^-{64} ~ 10^{-19}  seconds

uint32_t	errb_rate == double phat_local_err
   errb_rate stored as the integer number of Parts Per Trillion PPT (non-dimensional unit [ps]/[s])

errb_abs == double ca_err
   errb_abs stored as the integer number of ns, ie units of [ns], whereas ca_err is in [s]


/* Mapped Fields [ information carried over, but transformed as well as format converted ] */
phat 				a double  with no matching field on the FF   side, and
update_time 	a bintime with no matching field on the algo side.

FF <-- algo
	 update_time=Ca(tlast) (up to format conversion)
    Achieved according to eqn (1) above (with t=tlast) which makes use of phat.
    This corresponds to using (1) on the daemon side, and (2) on the FF side.
	 In this way, not only are K_p and thetahat combined on the FF side, but also phat.
	 This is why there is no phat member, period corresponding to plocal instead.
	 As the equations show, the two formulations are entirely equivalent, but the advantage
	 is that the FF side has fewer calculations to perform, fewer parameters to worry about,
	 and the format fits perfectly the per-tick (ff)timehands paradigm needed to
	 synthesize the FF counter by adopting the method used in timehands.

FF --> algo
	 ca = update_time - plocal*last_changed  (up to format conversion)  // hang on not plocal*(now-last_changed?) ?
	 This should be update_time - phat*last_changed  but phat is not available.
	 Since this is used in circumstances where the daemon is dead, using plocal is natural,
	 effectively it means that pbar = outofdate plocal going forward - close to optimal anyway


/* Incomplete Correspondances [ examples with at least one end partly missing ] */
? == phat_err
	FF <-- algo:	N/A: no phat field, phat value is encoded in update_time
	FF --> algo:	already setting phat=plocal both literally and in essence, so can
						just set phat_err = phat_local_err


uint16_t secs_to_nextupdate == vcounter_t next_expected
	FF <-- algo:	The kernel should know when an update is expected. Convert from the
	               update event timestamp to timeout to avoid need for (repetitive!) calculation.
						Status update code in ffclock_windup needs this.
	FF --> algo:	set to 0, as poll_period is owned by daemon.
						The kernel can never tell the daemon what the poll_period is,
						particularly if the daemon is dead.

 // TODO: check all of these are up to date,  and 'hang on' above, and make secs_to_nextupdate
 // secs_to_nextupdate bigger than 8 bits !! check implementation first will screw alignment...
 

==============================================================
Setting of FFclock data
======================================

We document the complete set of ways in which FFclock data can be set.
Initialization to 0 of all members of fftimehands, and hence all members of fftimehands.cest
for each timehand, is first performed in kern_tc:SYSINIT-->inittimecounter-->ffclock_init .

After this a cest in a current tick can be modified in one of three ways:
  - push `update` from the daemon
  - ffclock_setto_rtc (called by ?? --> subr_rtc.c:inittodr), perhaps several times during boot
      sets  {update_{time,ffcount},period,status} only   [ uses tc of current tick ]
      sets  global ffclock_updated = INT8_MAX;
	 [ fftimehand fields beyond cest:  none ]
  - by ffclock_change_tc (call by tc_windup() if needed)
      sets {period,status}, resets all other fields to zero
      sets  global ffclock_updated--;  to effectively ignore next update
	 [ fftimehand fields beyond cest:  {tick_ffcount, period_lerp} ]

A common scenario after boot is:
	ffclock_init;
	ffclock_change_tc  on the first invocation of  tc_windup->ffclock_windup via inittimecounter
		{period,status} = {nominal,FFCLOCK_STA_UNSYNC}
	no change for 100s of invocations of windup_ffclock
	ffclock_setto_rtc
		{update_{time,ffcount},period,status} = {passed ts,now,nominal,FFCLOCK_STA_UNSYNC}
	no change for 1000s of invocations of windup_ffclock
	daemon launched,  pulls current kernel value of cest
   first daemon push of rad_data
		setting;   all fields,  now secs_to_nextupdate>0

If the daemon is initializing, then fill_radclock_data will provide the very rough value
of period to the algo as a option for use, prior to any algo update push.

If the daemon was running but later ceases to push updates, then the value of
cest will likely never change.  This is detected in ffclock_windup by observing
that cest is old (update_ffcount) and late (secs_to_nextupdate).

Libprocesses in that scenario will be using this out of date kernel version via
the inversion routine fill_radclock_data, with inversion failures:
  - phat = plocal   (true phat lost)
  - next_expected = 0  ( signals kernel has no idea when next update coming )
but otherwise values are valid, in particular the leapsecond fields will work.


/* TODO:  look at whether update_{time} should also be set in ffclock_change_tc
   Look at changing name to ffclockData along with the ffclock_estimate --> ffclockData */



==============================================================
Global Variables   // integrate with FFCLOCK symbol section?
======================================

The global variables are all defined in kern_tc.c .
(there is an extern sysctl_kern_ffclock_ffcounter_bypass in timeffc.h [see sysctl],
defined in kern_ffclock.c).

The main variables supporting the timecounter mechanism are:

static struct timecounter dummy_timecounter = {
	dummy_get_timecount, 0, ~0u, 1000000, "dummy", -1000000
};
static struct timehands th{0,1,2,...,9};                 // used as a circular tick-data buffer
static struct timehands *volatile timehands = &th0;      // points to current tick within timehands array
struct timecounter *timecounter = &dummy_timecounter; 	// current tc structure
static struct timecounter *timecounters = &dummy_timecounter;  // points to last tc created
int tc_min_ticktock_freq = 1;	// slowest rate of windup calls to avoid th_offset_count wrappping

time_t time_second = 1;			// quick access to the current UTC    second updated by tc_windup
time_t time_uptime = 1;			// quick access to the current uptime second updated by tc_windup
struct bintime boottimebin;	// UTC time of boot, according to prevailing UTC at boot, bintime format
struct timeval boottime;		// UTC time of boot, timeval format



The complete set supporting the FFtimecounter extension is

struct ffclock_estimate ffclock_estimate;	// FF clock state variable
struct bintime ffclock_boottime;	/* FF boot time estimate. */
int8_t ffclock_updated;			/* New estimates are available. Note this is signed! special values: {..,-1,0,1,INT8_MAX} */
uint32_t ffclock_status;		/* Feed-forward clock status */
struct mtx ffclock_mtx;			/* Mutex on ffclock_estimate */

static struct fftimehands ffth[10];
static struct fftimehands *volatile fftimehands = ffth;  // pointer to current tick within ftimehands array


The division of FF clock labour across kern_tc.c and kern_ffclock.c is primarily about
access to the timecounter internals themselves:

kern_tc.c:
	- original timecounter code and FFtimecounter extensions
	- code that needs access to the timehands and fftimehands, and code to support that code
	- code that relies explicitly on data held per-tick
	- in particular
	    - the FFclock read function   ffclock_read_counter(&ffc)
		 - the three variants of the system FF clock (native, LERP, diff), based off
		   the underlying FFclock disciplined by the daemon.

kern_ffclock.c:   "if it doesnt need access to per-tick, then dont give it access"
   - code that doesnt need direct tick or timecounter access, that assumes the counter
	  can be read.  It relies on code in kern_tc.c, but not vice versa.
	  Hence it is about protecting the existing tc code by keeping kern_tc minimalist
	- many flavours of FF clock reading functions (but no explicit LERP actions)
	- support for the FF daemon  (syscalls)
	- other code at the FF clock level, not the FF counter level
	  - FF SYSCTL code
	  - FF/FB system clock selection

The system clock as such is spread across both of these. There is no clear separation
between the FF timecounter, the FF clock based on it, and the system clock functionality.



==============================================================
Timecounter (tc) Mechanism:  Detail
======================================

The operating principle of tick-state, held in {ff}timehands, is that it consists
of values that correspond to, and/or were recorded at, the beginning of a tick,
technically the location of the tc_delta raw timestamp call. This state
is to be used for any timestamp purposes initiated at any time during that tick.

The function which updates the tick state for the FB clock is
	static void tc_windup(void)
It is called by inittimecounter after initializing tc_tick,
by tc_setclock to process updated information on the true UTC time (including leaps?),
and most importantly, periodically once per tc_tick within tc_ticktock() .
All invocations are protected by static struct mtx tc_setclock_mtx .

Since tc_windup IS the function that updates timehands, it does not require
generation protection.   It does however set the generation of the next tick it
is preparing ("th") to zero, so that any calculation in-train pointing to that tick
will be aware it cannot be safely used. At the end of tc_windup the generation will
become valid, and those calculations can complete (after restarting using the updated tick data).

For the same of brevity, the following code summaries play fast and loose with
pointers versus *pointers, and other inconsistencies.

 tc_windup: simplified code:
 ---------------------------
   tho = timehands;                    // grab the current tick
	th = tho->th_next	..						// initialize next tick "th" to current tick values
	th->th_generation = 0;					// label this tick-data as in-progress, dont use!

	// raw timestamping
	delta = tc_delta(tho)					// take raw FB delta using tc of this tick
	ncount = timecounter->tc_get_timecount(timecounter);  // take one using globally current tc
	ffclock_windup(delta)					// fftimehands tick update including FFcounter read
	th_offset_count += delta				// add-in new delta, in wrap-safe manner

	// old tick data (and tc) must be used to update from start of old, to start of new
	th_offset += th_scale * delta			// done respecting binary representation limitations

	tho->th_counter->tc_poll_pps(tho->th_counter)	// if tc of PPS type, poll it

	// update UTC members
	update th_{micro,nano}time				// use 'magic' ntp_update_second processing, includes leap

	// process a changed timecounter
	if (th->th_counter != timecounter) {// if global tc different to one used in current tick
		disable hardware sleep if can
		th_counter = timecounter;
		th_offset_count = ncount
		tc_min_ticktock_freq	= ..			// reset tick frequency to ensure counter wraps caught
	   ffclock_change_tc(th,ncount)		// let the FF cope with this radical change
   }

	// update period=1/freq  estimate
	scale = (1 + th_adjustment)/th_counter->tc_frequency // rely on NTPd to adapt scale to new tc
																		  // code uses binary approximations

   th_generation = ++ogen					// announce this tick has (the newest) valid data (avoid special 0 value)
	
	// fill global convenience variables  (used in kern_ntptime.c)
	time_second									// universal update (switches on sysclock_active)
	time_uptime									//		"		"
	
	timehands = th;							// advance to the new tick  [ fftimehands already advanced ]
 ---------------------------


The timehands and fftimehands systems are separate, with separate generation variables,
however they update on ticks in a synchronous manner, because the timehands update
function: tc_windup, calls that for fftimehands: ffclock_windup, which we now describe.

Generation checking based on timehands is sufficent for fftimehands
checking, since if timehands tick state was not overwritten, then fftimehands state couldnt have been.
It is not necessary to use it instead of fftimehands generation, but is safer and
neater (particularly as ffclock_change_tc is a separate function called after ffclock_windup
within tc_windup which also advances fftimehands!) and ensures that {ff}timehands
are fully `in sync`, using the same timecounter on each (logically common) tick.

In the following, the pointer to the current tick is frequently omitted for brevity, hence
tick_ffcount instead of fftimehands->tick_ffcount etc .  Use is made of the globals
int8_t ffclock_updated;			/* New estimates are available.  Values used: {..,-1,0,1,INT8_MAX} */
uint32_t ffclock_status;		/* Feed-forward clock status Values used: {0,1} */

As the code is largely self-documenting, the summary here will describe mainly only the
main structure with comments.  Here `current tick` is the one pointed to by fftimehands.
The function prepares the next tick, which is not activated until the end when
fftimehands is advanced.

ffclock_windup(delta): simplified code:
---------------------------------------
	ffth = fftimehands->next					// next tick-state to prepare
	
	/* Calculate new tick-start using current tick values */
	tick_ffcount = tick_ffcount + delta 	// FF timestamp location in fact that of tc_delta

	/* No acceptable update in FFclock parameters to process */
  	if (ffclock_updated <= 0) {
  	  tick-state update based on copy or simple linear projection from previous tick.
	  Note all UTC members are updated using the CURRent tick data to project to new tick-start.

  	  /* If a normal tick update */
	  if (ffclock_updated == 0)
	    set FFCLOCK_STA_UNSYNC if FFdata too stale
   }
   
   /* Flag event where FFclock update entered next tick. Should be extremely rare, maybe impossible? */
	if (ffclock_estimate.update_ffcount >= ffth->tick_ffcount)
		"ffclock update slipped into next tick, will process on next call"  // no action needed, just flagging
		
		
	/* Acceptable FFdata update available */
	if (ffclock_updated > 0 &&  didnt slip into next tick) {  // if an update occurred during the current tick
 		/* update Ca members */
		tick_time = cest->update_time + ffdelta*period;  etc  		// recalculate directly off last update
	   tick_error = cest->errb_abs + cest->errb_rate*period*delta	// re-base from update:
		  // That is, all UTC members are updated using the new tick data to project from
		  // where the update occured in the previous tick, to new tick-start
		  // (ie a `rebase`, plus projection over less than 1 tick)
   
   	/* Lerp processing is more complex */
		if (ffclock_updated == INT8_MAX)	{	// update set by ffclock_setto_rtc
			tick_time_lerp = tick_time			// let the CaMono jump, since have reset radically
		else  // normal case
		   update it using period_lerp
		
		// record new disparity at tick-start between Ca and CaMono
		forward_jump = {0,1}  								// Ca jumps {down,up} wrt CaMono
		gap_lerp = abs(tick_time - tick_time_lerp)	// ≥0, sign carried by forward_jump
		
		// jump reset CaMono to Ca if in particular bad conditions
		if (UNSYNC in kernel but not in daemon, and in WARMUP)
			ffclock_boottime += gap_lerp 		// kill gap to avoid very slow CaMono -> Ca convergence
														// Do by absorbing into ffclock_boottime so Uptime variants dont jump, only UTC
			tick_time_lerp = tick_time			// jump lerp to make gap=0
		
 		/* Reset CaMono period for next pollperiod interval before the next FFdata update */
	 	ffth->period_lerp = cest->period;	   // start afresh with this update, no past dynamics
		gap_lerp = min(gap_lerp,secs_til_nextupdate*5000PPM) // cap rate of gap reduction to 5000PPM
	   period_lerp += gap_lerp/ffdelta        // adjust to reduce gap to zero over ffdelta cycles
   
		ffclock_status = cest->status;		// now kernel status acted on, override with update
		ffclock_updated = 0;		// signal that latest update is processed
	}
	
	fftimehands = ffth;		   // advance to the newly prepared tick-state

 ---------------------------


Complete breakdown of  ffclock_updated usage
-------------------------------------------
This is used to communicate the availability of fresh FFdata in the global
ffclock_estimate, both in the kernel and in the daemon.

 Set in:
   ffclock_init :    		 ->0
   ffclock_setto_rtc :      ->INT8_MAX
   ffclock_windup           ->0   if  > {0,INT8_MAX}  [ no action if <=0 ]
	sys_ffclock_setestimate	 ++  // new update available
	ffclock_change_tc   :	 --
 
 Tested in
    ffclock_windup          =={0,>0,INT8_MAX)

 Normal flow:
   init to 0
   update loop:
   	0->1 after daemon update
   	1->0 after update processed in ffwindup
 
 Reset flow:
   0
   0->INT8_MAX  after reset update
   INT8_MAX->0  after update processed in ffwindup
		
 tc change flow
	0
   0->-1  after tc change `update`    ( also INT8_MAX -> -1 )
	-1->0  after missed daemon update
	0->1   after next daemon update
	1->0   after update processed in ffwindup



==============================================================
Timecounter structure and hardware counters
======================================
This section describes the existing mechanism: no changes were made.

The tc abstraction of a hardware counter is defined in  sys/sys/timetc.h ,
where a tc is defined as a binary counter which:
  	 i) runs at a fixed, known frequency   [ ie is not scaled, divided, or power state influenced ]
	ii) has sufficient bits to not roll over in less than around max(2 msec, 2/HZ seconds)

typedef u_int timecounter_get_t(struct timecounter *); 	// counter reading function prototype
typedef void  timecounter_pps_t(struct timecounter *);	// PPS 	  reading function prototype

struct timecounter {
        timecounter_get_t       *tc_get_timecount; // ptr to counter reading fn, no masking
        timecounter_pps_t       *tc_poll_pps;		// checks for PPS events if relevant
        u_int                   tc_counter_mask;	// marks implemented bits
        uint64_t                tc_frequency;		// nominal frequency (hz) determined once only
        const char              *tc_name;				// name  [ used for identification ]
        int                     tc_quality;			// apriori quality to rank counters [-ve = only on request]
        u_int                   tc_flags;				// takes values in :
#define TC_FLAGS_C2STOP         1       					// Timer dies in C2+
#define TC_FLAGS_SUSPEND_SAFE   2       					// Timer functional across suspend/resume
        void                    *tc_priv;				// ptr to private parts
        struct timecounter      *tc_next;				// ptr to next tc
        timecounter_fill_vdso_timehands_t   *tc_fill_vdso_timehands;
        timecounter_fill_vdso_timehands32_t *tc_fill_vdso_timehands32;
};

Since the timecounter reading fns return u_int, the tc interface natively only supports
counters up to 32bits, even if the underlying hardware counter is wider.
FB: this is sufficient to keep track of a delta over a tc_tick, which is all that is
    needed for the C(t) = th_offset + th_scale*∆(tc)  in-tick tracking paradigm where ∆ is small
FF: the same ∆(tc) is convenient to use as in FB to maintain tick-state, but to support
	 the  Ca(t) = tick_time + cest->period*∆(T) paradigm  where T never wraps, a 64bit FFcounter T is
	 synthesized from the tc counter (this is done within ffclock_windup and ffclock_change_tc).
	 
The mask is used in counter reading to ensure that unimplemented bits are zeroed
so they will not interfere. Thus a counter living on b < 32 bits works within the
32bit container of the tc interface. This has nothing to do with the management of
wrapping or overflow of the tc counter, or associated variables.
The FFcounter is 64bits regardless of the original value of b.

At compile time, the tc_get_timecount() member is initialized to the dummy counter
which simply increments by 1 each time the fn is called.

For ARM processors, a static struct timecounter arm_tmr_timecount is defined in
/usr/src/sys/arm/arm/generic_timer.c .

For x86-like processes, the TSC family of timecounters are defined in
/usr/src/sys/x86/x86/tsc.c .   The basic counter reading fn is simply
static u_int tsc_get_timecount(struct timecounter *tc __unused)
{
        return (rdtsc32());	// return lower 32 bits of TSC register, fast!
}
where the low level register reading functions rdtsc32(), and rdtsc(), are
defined in relevant machine dependent files, such as sys/{amd64,i386}/include/cpufunc.h .


There are a number of sysctl INT variables controlling and monitoring TSC features,
which is complex and vendor dependent because of the need to insure invariance
wrt multicore and power management.  An important one is static int tsc_shift ,
which in  init_TSC_tc(void)  controls the maximum allowed frequency of a tc counter.
If the max is exceeded, the output of rdtsc32 is shifted until the frequency is low enough.
If this occurs, the TSC based tc counter takes the name "TSC-low" instead of "TSC",
and TSC itself will be unavailable within tc, though the full TSC is of course
still accessible via rdtsc().   The goal seems to be to ensure that
the tc does not progress too fast, which may cause tc_tick to be selected onerously small.
The value of the applied shift is stored in tc.tc_priv .

Example: the max frequency is 2^(32 - tsc_shift).  Hence with the default tsc_shift=1,
if a processor has freq 3Ghz > 2^31 = 2.147Ghz, it will be shifted by 1 to create a
tc counter called "TSC-low" of frequency freq/2 = 1.5Ghz.

The value of tsc_shift can be conveniently set to 0 to enable a "TSC" tc counter by adding
kern.timecounter.tsc_shift=0
to /boot/loader.conf  , provided freq does not exceed 2^32 = 4.295Ghz.
This takes effect after a reboot.  Supporting TSC at higher frequency would require
further work.



==============================================================
Raw Timestamping and Wrapping Management
======================================

Let th and ffth be the current tick states for timehands and fftimehands respectively.
The underlying function making counter reading possible for both FB and FF clocks is
tc_delta(th), which returns the number of tc counter increments since th->th_offset_count
was set.

static __inline u_int  tc_delta(struct timehands *th) {
	struct timecounter *tc;
   tc = th->th_counter;
   return ((tc->tc_get_timecount(tc) - th->th_offset_count) & tc->tc_counter_mask);
}

The issue of the possible wrapping of the counter since the last value recorded
in th_offset_count is taken care of automatically as follows.
In C, unsigned integer arithmetic (+,-,*) between variables of the same width b is
guaranteed to evaluate to a number, of width b, which is the correct modulo 2^b .
Hence even if the counter has overflowed so that  vcount < th_offset_count,
delta  = vcount - th_offset_count  will still be correct over the b bits.
The overflowed bit would be zero anyway because tc_tick has been chosen to
ensure it can wrap (and hence overflow) at most once between calls to tc_windup.
Thus delta = tc(delta) is a correct positive number regardless of overflow.

Similarity, if when the FB clock adds this (correct) delta to perform the update :
	th_offset += th_scale * delta
an overflow occurs, the bits held in th_offset are correct, simply with higher bits missing.
The update will therefore mirror the counter value (raw timestamp) read in the call to
tc_delta (which also has any overflow bits missing) which it precisely purpose of th_offset_count .

The FB clock uses tc_delta to calculate timestamps (see fbclock_binuptime),
as well as to advance th_offset_count itself during updates (tc_windup).
The FF clock uses exactly the same tc_delta(th) call to advance tick_ffcount
 *ffcount = ffth->tick_ffcount + tc_delta(th)  // add delta to tick-start
with the advantage that the overflow bits are retained.

Raw FFcounter timestamping is performed by ffclock_read_counter(*ffcount),
however this also uses tc_delta(th) to advance tick_ffcount.
Thus, counter reading for both FB and FF is based on the original tc wrapping
management provided by tc_delta working with timehands.

Note that, even if th becomes stale, timestamps will still be valid as tc_delta uses
the same generation as (th_offset_count, tick_ffcount), and also the same
choice of actual timecounter!, also recorded in th.
Furthermore (assuming a constant tc), working with old ticks does not even imply
poorer timestamps. If the FF data has not been updated, the result will be the same as
it is the same interpolation, just projected from an older starting time.
If the data has been updated, the parameters will be less than  tc_tick  out of
date typically.



==============================================================
Timecounter (tc) Mechanism:  Changing Timecounters
======================================

The global variable
struct timecounter *timecounter = &dummy_timecounter; 	// points to tc in use, initialized to dummy
records the current choice of tc and can change at any time.
In terms of tick-state however and its functionality, changes in timecounter
are picked up and processed in tc_windup, so that timestamping can be performed consistently within each tick,
possible since  timehands->th_counter  records which tc should be used for a given tick.

At boot, only the dummy timecounter is defined. Even before the system of timecounter tick
updates is initialized, it advances each time the counter is accessed (in fact around 1000 times, not sure where!).
Simplified code for the initialization of the tick update mechanism is

inittimecounter()
	tc_tick = max(1,hz)					// roughly speaking, maybe hz+1
   ffclock_init()							// create the cyclic buffer of fftimehands
	tc_windup(NULL);						// first call to tc_windup
			delta = tc_delta(th);		// operating on the current, first initialized tick-data which has tc = dummy
			if (th->th_counter != timecounter)  ncount = timecounter->tc_get_timecount(timecounter); // also read new counter
			ffclock_windup(delta);		// normal update with old counter, fftimehands advances
			if (th->th_counter != timecounter) {  // will enter as move from dummy
				printf("Changing tc counter on this tc-tick:  dummy = th_offset_count , current = ncount",..)
				ffclock_change_tc(th,ncount);	// fftimehands advances again, redoing the `same tick' with new counter
			}
			timehands = th;				// timehands advances

Thus the need to change timecounters is a generic one, because when inittimecounter is
called, timecounters have already been initialized (via tc_init()), any one of which
would have been seen as superior to dummy and selected as the current.
Thus the first call to tc_windup implies managing a timecounter change.
At other times a change in tc is a rare and relatively dramatic event.

During a tc change, each of {ff}timehands first prepare the new tick using the current
counter, then overwrite some of these fields as needed once the change is noted.
In the FF case, this in done using two separate advances of fftimehands, however it
should be thought of an update of the same tick, in terms of real-time tc-tick sequence.
It is done in this way for safety and so that ffclock_{windup,change_tc} can each
be neatly isolated within tc_windup using FFCLOCK.

Resetting to the new timecounter involves reinitializing the FFdata, and retaining the
clock values already calculated for the start of the tick. Thus the ncount value passed
in is using in origin-setting, but not as a delta.

The tricky task is the inital value given to the new counter.
Non-TSC counters:  here there is no available historical state to act on. The new
	FFcounter is simply initialized, exactly agree with that of timehands.
	Implicitly, this means that both counters have an origin which is ncount units
	in the past from the new tick-start.
TSC derived counters:  here the availability of rdtsc() (which has a fixed origin
   of some time shortly after boot (or power up?)), enables an historical origin to be
   accessed.  The FFcounter member ffth->tick_ffcount is selected to perfectly correspond
   to it, while naturally maintaining a perfect agreement with timehands over the
   lower 32bits.  This is also achieved in the case of "TSC-low" through accessing
   and applying the shift.

For TSC counters, the benefits of this origin setting are that tc reads are fully
consistent over time with rdtsc reads (shifted if needed), even if the counter
is changed multiple times:  each time it returns to TSC, the counter readings will
be on the same TSC timescale.

In ffclock_change_tc the ffclock_updated global is set to a special value to signal to ffclock_windup
to skip the next daemon update, as it may still be related to the old counter.


On the daemon side, a change in tc is a rare and dramatic event, and breaks the FF algo model,
requiring a full reset.
The current tc is accessible via
sysctlbyname("kern.timecounter.hardware", &hw_counter[0], ..)   // see if tc there or changed
and is recorded in the global  clock.hw_counter[32] , initialized in radclock_init_vcounter.
It is acted on in pthread_dataproc.c:process_stamp  just before the pushing of
a fresh parameter update to the kernel. If a change is seen, the update push is skipped
(hence ffclock_windup had earlier just advanced a standard tick, with no update)
and the algo reinitialized (however it is not clear if this is done properly).
For more details see the algo documentation.



// TODO: understand how code can detect if rdtsc and rdtsc32 not available
//  (or perhaps simply that TSC not available) so that related code in ffclock_change_tc can compile
 



==============================================================
FFcounter timestamping bypass mode
======================================

An invariant TSC counter has the key advantage of being accessible very rapidly
using rdtsc(), and doing from from either kernel or user context with no system call overhead.
The tc mechanism adds overhead both to recreate the higher bits of the TSC,
and to access timestamping using the tc-based TSC.  The goal of the bypass mechanism
is to allow a return to the minimal overhead world in the case of TSC, while
remaining compatible with the tc mechanism.  Other counters with the possibility
of fast direct access, particularly if wider than 32 bits, could readily be added
to the mechanism. Xen ClockSource is an example. Xen support is currently out of date
and not included in the bypass mode code.

As a kernel based mode, tc bypass mode is controlled by the Read/Write variable
(in kern_ffclock.c, declared as extern in timeff.c)
	int sysctl_kern_ffclock_ffcounter_bypass=0;		// default is off
with semantics:  bypass mode is  on/off  = {1,0}   (and not, {is,is not} supported) .
It should only be activated by a priviledged user, such as the daemon, and only
when the tc counter is "TSC"  (see algo documentation).
There is no issue in switching the mode multiple times:  the tc mechanism is unaffected,
only timestamping external to tick updating changes.
There is no testing of the correctness of the mode on the kernel side.
If the mode is set in error and the current timecounter is not TSC, timestamps
from an incompatible counter will be returned.

Once active, there are two different forms of the TSC counter: native via rdtsc, and the TSC tc.
These are different, but thanks to the origin alignment described above, they are
equivalent in terms of value, but differ in access latency.

There are two different locations where the bypass mode modifies raw timestamping.
These are the only places where the FFcounter is actually read.

ffclock_read_counter:   here rdtsc is called rather than tc_delta _+ tick processing
  [ currently only used in ffclock_setto_rtc ]

sysclock_getsnapshot:   here delta processing is still needed because of the tick-state
   recording architecture (the fn does much more than just read the raw counter),
   and the need to be tick-safe.  The delta is calculated in
   a more direct way than the usual tc_delta() call, but the overhead reduction is much less,
   and the timestamping still not quite that of a naive rdtsc() call placed as the first instruction.

Of course, any new code in kernel or user context can also invoke rdtsc() directly,
the resulting raw timestamp will be comparable with those obtained above.

Note: bypass mode could be extended to support TSC-low is needed, but the
increase in complexity does not seem well motivated. Expert users wishing to use
bypass will likely want the highest resolution anyway, or not need to worry about
compatibility with the tc TSC, and will be able to supply needed workarounds.



==============================================================
FF clock status setting
======================================

Here we describe the logic governing the interaction of the FFdata status taken from
the deamon, and the status of the FFclock itself, indicated by (timeeff.h) :
FFCLOCK_STA_{UNSYNC,WARMUP} .

The guiding principle here is that the daemon is the expert in assessing sync
accuracy and state, the FF clock should only mistrust or override it in extreme cases,
in particular when the daemon is unresponsive  (dead or no valid stamps arriving to the algo).


.....






==============================================================
FF and system clock reading
======================================

The FFclock has a number of variants which we describe here (see also the documentation
in timeffc.h).  For convenience we use the following to track input parameters.

	 T  = ffcounter ffcount;	 	// a raw counter timestamp or value
  0≤∆T = ffcounter* ffdelta;   	// the difference of counter units, a raw time interval
	 p  = uint64_t period;		 	// the FFclock counter period estimate
	 t  = struct bintime *bt    	// a time in seconds
	 ∆t = struct bintime *bt    	// a time difference in seconds
	 eb = struct bintime *bt    	// an absolute time error bound
FFdata = ffclock_estimate* cest  // FF clock data

Conceptually the organisation of these variants is as follows.
The FFdata holds the data defining a single dual (Ca,Cd) clock, which
is oblivious of leapseconds, is not guaranteed to be monotonic, should always be
read based on a fresh raw timestamp, and is expressed natively in bintime format.
To read it, one first requires raw FFcounter timestamps available from FFtimecounter mechanism:
void ffclock_read_counter(ffcounter *ffcount)

From here the difference clock is simple and exists in essentially a single form:
it calculates ∆t = ∆T*p given a suitable input ∆T.  Error reporting could be included,
but as kernel code is not even used to a difference clock yet, extra detail is unwarranted.

The absolute clock is more complex for four main reasons:
  - the need for a monotonic version of Ca expected of a system clock
  - the need to provide the expected uptime variant giving time since boot
  - leapseconds inclusion to track UTC
  - error reporting, as this exists for the FB (absolute) clock already

These needs live naturally at different levels in the code, which then controls
which file the functionality is added into. For example ffclock_convert_{abs,diff}
are defined in kern_tc because they need to be (see section on Global Variables),
though they are not used there, and LERP is accepted at the level of ffclock_abstime,
but actually handled by ffclock_convert_abs .
From low to high:
  - monotonic version:  			in effect a new clock, data for which lives in fftimehands
  - leapsecond and uptime:  		simple optional constants added to the base clocks
  - error reporting:					an option added into generic `full featured` calling functions
  - format conversion:				{binary,timespec,timeval} FF wrapper functions
  - FF/FB clock hiding: 			generic wrappers

Because of the risk of asynchronous updates, generation checking is used when clock data is accessed
  from fftimehands:   use fftimehands->gen   (even if want FFdata, is finer grained)
  from FFdata:			 use FFdata->update_ffcount
  
  
The actual functions are:

/*   Purpose               	function                            	  					defined in  */
	  ---------------------------------------------------------------------------------------
	  counter reading       ffclock_read_counter			  		    					kern_tc.c
	  counter conversion 	ffclock_convert_delta				    					kern_tc.c
	  low level FF:			ffclock_convert_{abs,diff}			    					kern_tc.c
	  ---------------------------------------------------------------------------------------
	  general FF workhorse:	ffclock_{abs,diff}time				    					kern_ffclock.c
	  FF specialized diff:	ffclock_{bin,nano,micro}difftime   						kern_ffclock.c
	  FF specialized abs :	ffclock_[get]{bin,nano,micro}[up]time   				kern_ffclock.c
	  ---------------------------------------------------------------------------------------
	 Absolute only:
	  FB specialized:			fbclock_[get]{bin,nano,micro}[up]time   				kern_tc.c
	  existing public names:        [get]{bin,nano,micro}[up]time					kern_tc.c
	  FF/FB switching    		     [get]{bin,nano,micro}[up]time_fromclock  	timeffc.h (inline)


/* Difference clock */
The hierarchy of calls is:

	ffclock_{bin,nano,micro}difftime(∆T, ∆t)  // system clock functions
		ffclock_difftime(∆T, ∆t, eb)				// semi-generic FF difference clock
			ffclock_convert_diff(∆T, ∆t)			// low level difference clock
				ffclock_convert_delta(∆T, p, ∆t) // simple multiplication

The ∆t ≥ 0 requirement is needed at each stage. In that sense these calls are not a
true generic difference clock. Contrast this to the RADclock library calls
radclock_{elapsed,duration} which each return long doubles and can be negative.

ffclock_convert_delta(∆T, p, ∆t)
	- simple subroutine performing ∆t = ∆T*p in binary, converting counter units to seconds
	- must have ∆t ≥ 0

ffclock_convert_diff(∆T, ∆t)
	- obtains p internally from FFdata (hence native, not LERP)
	- protection: fftimehands->gen
	- then just calls ffclock_convert_delta(∆T, p, ∆t)

ffclock_difftime(∆T, ∆t, eb)						// general FF Cd function independent of fftimehands
	- calls ffclock_convert_diff(∆T,∆t) with no modification
	- add error information if requested by non-null eb
	  - get from FFdata, protection:  FFdata.update_ffcount

ffclock_{bin,nano,micro}difftime(∆T, ∆t)
	- calls ffclock_difftime(∆T, ∆t, NULL)		// no error bound activated !
	- converts to {native binary,timespec,timeval}
	- thus ffclock_bindifftime(∆T, ∆t) == ffclock_convert_diff(∆T,∆t)



/* Absolute FF clock */
The absolute clock variants have an analogous structure but with more complexity as a function
of the flags (see timeffc.h)  FFCLOCK_{FAST,LERP,LEAPSEC,UPTIME}.

The hierarchy of calls is:
	ffclock_[get]{bin,nano,micro}[up]time(t)	// system clock functions
		ffclock_abstime(T, t, eb, flags)			// generic FF absolute clock
			ffclock_convert_abs(T, t, flags)		//	low level absolute clock
				ffclock_convert_delta(∆T, p, ∆t) // simple multiplication

ffclock_convert_abs(T, t, flags)			// reads basic FFclock, in native or LERP form
	- uses LERP to select between the native or LERP FFclock (needs fftimehands)
	- ffclock_convert_delta to generate delta to add to stored value at last tick
	- capable of handling both past and future T values  (effectively ∆t of either sign)
	- protection:  fftimehands->gen for entire calculation

ffclock_abstime(T, t, eb, flags)			// general FF Ca function independent of fftimehands
   - general function supporting all possible FF clock variants based on flags
		- FAST:  	use  ffclock_last_tick, else ffclock_convert_abs(ffclock_read_counter)
		- LERP:  	pass flags through for  ffclock_{last_tick,convert_abs}  to handle
		- LEAPSEC:  add in effect of leaps since boot into basic native clock to form UTC native
		- UPTIME:   subtract ffclock_boottime  to make times relative to boot
   - note LEAPSEC and UPTIME should not be used together
	- protection:   FFdata->update_ffcount  for update_ffcount retrieval only
   - add error information if requested by non-null eb [no gen-checking unlike ffclock_difftime]
	- algo native Ca(t) == ffclock_convert_abs(ffclock_read_counter(), t,0) // no LERP, no leap
	- UTC  native Ca(t) == ffclock_abstime(NULL, t, NULL, FFCLOCK_LEAPSEC ) // no LERP, leap added
	- FORMAT dimension hardwired to bintime

ffclock_[get]{bin,nano,micro}[up]time(t)
	- flags established based on function name and intention:
		- ALLways include LERP (top level functions, all need monotonicity)
	   - if get in name include FAST,   else not
		- if up  in name include UPTIME, else LEAPSEC
	- calls ffclock_abstime(NULL, t, NULL, flags)		// no error bounded activated !
	- converts to {native binary,timespec,timeval} as before
	- thus None of these functions allow the algo''s leap-free Ca clock to be read
	- designed to match existing  [get]{bin,nano,micro}[up]time  functionality:
		- explicit naming of  2x3x2 = 12 functions over {FAST,FORMAT,UPTIME} dimensions
		- native/LERP dimension not covered, LERP hardwired
		- no return of ffcounter




/* System Clock */
The existing clock names expected by the kernel, corresponding to the traditional FB-based
absolute time system clock, are [get]{bin,nano,micro}[up]time .
For backward compatibility these names still exist,  as do the original functions,
renamed with the prefix "fbclock_".  The functions [get]{bin,nano,micro}[up]time_fromclock
are now defined to switch between the FF and FB functions depending on the chosen
system clock paradigm, and the original names are wrappers for these.
In short, the original names will now use FF or FB under the hood.
Difference clocks only exist under FF, and there are no traditional names to map to.





/* TODO:
	- consider providing a sign argument to ffclock_convert_delta so that increments of either sign
	  can be accepted
	- make error checking consistent over ffclock_{diff,abs}time
	- provide checking to ignore LEAPSEC if UPTIME requested
	- ffclock_convert_diff  used gen-checking even though data comes from FFdata, fix this?
	- ffclock_difftime
		- fix conversion factor error
	   - protection wont ensure consistency, see notes  need/worth to protect all?
   - ffclock_abstime:
		-	revisit leap processing once leap system worked out
		- fix possible sign error on error bound (see notes)
		- fix conversion factor error
   - having LERP implemented within fftimehands instead of a form of FFdata means a
	  nasty complex failure of separation of clocks and the counter.  Instead of having
	  monoFF built on FF built on FFcounter,  monoFF is integrated into FFcounter!
	  It means that even a bypass counter has to be in timehands just to implement LERP !
	  It also means that if the FF clock had its own mono version, it would still have to
	  deal/nullify this one.
	  Can this be revisited?
*/


==============================================================
Syscalls:
======================================

The regular FFcode <--> userland interactions are achieved by syscalls for KV≥2.
The interactions via sysctl (next section) are to test/set more static parameters.
There are three syscalls. The relationship to functions mediating their use in
the daemon are summarized in RADclockDoc_library, repeated here:

syscall (defined in kernel)   linked userspace fn		   OS-dep wrapper {ffkernel,kclock}-{fbsd,,}.c
---------------------------------------------------------------------------------------------------
sys_ffclock_getcounter  		ffclock_getcounter(vcount) radclock_get_vcounter_syscall(clock,vc)
sys_ffclock_setestimate			ffclock_setestimate(cest)	set_kernel_ffclock(clock, cest)
sys_ffclock_getestimate			ffclock_getestimate(cest)	get_kernel_ffclock(clock, cest)

The syscalls, along with dummy versions returning ENOSYS if !FFCLOCK, and individual
user argument passing structures (if not already collected and included in sysproto.h),
are defined at the end of kern_ffclock.c .

ffclock_getcounter
   a wrapper to a call ffclock_read_counter(), passing the ffcounter result to userland,
	received as a vcounter_t.

sys_ffclock_{set,get}estimate
   {setting,getting} the global FFclock state variable cest of type ffclock_estimate.
	Access to  cest  is protected by ffclock_mtx, preventing potential multiple
	user processes from any inconsistent actions or consequences.
	Setting (updating) of the FF data is communicated to the ffclock_windup controlling
	fftimehands updating via the global  ffclock_updated++ (typically from 0 to 1).

Dont forget that the syscall code runs in kernel context, and so has access to any mutexes
defined there.


==============================================================
SYSCTL:
======================================

The SYSCTL tree associated with FF components is defined in kern_ffclock.c .
The provided controls are for the management of the Universal sysclock in general,
the FF system clock more specifically [see also next section], and tc level controls that
can impact the FF clock and the FF daemon, such as a change of tc.

_kern  							   ""   // defined above kern_tc.c somewhere
	-> _kern_sysclock	[NODE] 		"System clock related configuration"  // declared in timeffc.h
		- available [PROC]   			"List of available system clocks" [RO]
		- active    [PROC]  				"Name of the active system clock which is currently serving time"
		-> _kern_sysclock_ffclock [NODE]"Feed-forward clock configuration" // declared in timeffc.h
		   - version				[INT]			"Feed-forward clock kernel version" [RO]  // KV
		   - ffcounter_bypass	[INT]	  		"Use reliable hardware timecounter as the feed-forward counter"

The related variables are :
 timeffc.h
	 #define	SYSCLOCK_FBCK	0
	 #define	SYSCLOCK_FFWD	1
 kern_ffclock
	 static int ffclock_version = 3;     							// hardwired KV choice, here KV=3
	 static char *sysclocks[] = {"feedback", "feed-forward"};		// strings associated to FB/FF
	 #define	MAX_SYSCLOCK_NAME_LEN 16
	 #define	NUM_SYSCLOCKS (sizeof(sysclocks) / sizeof(*sysclocks)) // currently = 2
	 static int sysctl_kern_ffclock_ffcounter_bypass = 0;    	// default no counter bypass
 kern_tc.c
	 int sysclock_active;												// paradigm of active system clock

The callback procedures for the PROC leaves are
static int sysctl_kern_sysclock_available(SYSCTL_HANDLER_ARGS)
	- provides the names stored in *sysclocks
static int sysctl_kern_sysclock_active(SYSCTL_HANDLER_ARGS)
   - gets the string associated to sysclock_active  OR  sets value of sysclock_active

The macro SYSCTL_HANDLER_ARGS is defined in sysctl.h as
#define SYSCTL_HANDLER_ARGS struct sysctl_oid *oidp, void *arg1, intptr_t arg2, struct sysctl_req *req

/*
 * This describes the access space for a sysctl request.  This is needed
 * so that we can use the interface from the kernel or from user-space.
 */
struct sysctl_req {
        struct thread   *td;            /* used for access checking */
        int              lock;          /* wiring state */
        void            *oldptr;
        size_t           oldlen;
        size_t           oldidx;
        int             (*oldfunc)(struct sysctl_req *, const void *, size_t);
        void            *newptr;
        size_t           newlen;
        size_t           newidx;
        int             (*newfunc)(struct sysctl_req *, void *, size_t);
        size_t           validlen;
        int              flags;
};

int sysctlbyname(const char *sname,  void * oldp, size_t *oldlenp,   // current value and size
										 const void * newp, size_t newlen);		// new value and size


Those sysctl calls used by the daemon are accessed via sysctlbyname() and are
summarised in a section of RADclockDoc_library.  Only three calls are normally used:
sysctlbyname("kern.sysclock.ffclock.version",&version,&size_ctl,NULL,0)) // detect,store, report KV
sysctlbyname("kern.timecounter.hardware", &hw_counter[0], ..)            // on EACH stamp: see if tc there or changed
sysctlbyname("kern.sysclock.ffclock.ffcounter_bypass",&bypass_active,..) // get/set kernel bypass reading mode for TSC

Additional calls are made at startup within clock_init_live() and radclock_init_vcounter as a test of the
sysctl settings [To be removed, or moved to OS-dependent code].


For completeness, the tree components specific to the rest of the timecounter interface are
_kern
  - boottime [PROC] 	"System boottime"
  -> _kern_timecounter
	   - stepwarnings	[INT,RW]	  	"Log time steps"
	   - hardware   	[PROC,RW,RD]"Timecounter hardware selected"
	   - choice		 	[PROC] 		"Timecounter hardware detected"
	   - alloweddeviation [PROC] 	"Allowed time interval deviation in percents"  new in 11.2
	   - fast_gettime [PROC] 		"Enable fast time of day"
	   - tick			[INT,RD]	  	"Approximate number of hardclock ticks in a millisecond"
	==== from x86/cpu.h :
	   - invariant_tsc[INT,RDT]		"Indicates whether the TSC is P-state invariant"
	   - tsc_shift		[INT,RDT]		"Shift to pre-apply for the maximum TSC frequency"  // used in fclock_change_tc
	   - tsc_disabled	[INT,RDT]		"Disable x86 Time Stamp Counter"
	   - tsc_skip_calibration [INT,RDT]	"Disable TSC frequency calibration"
		- smp_tsc		[INT,RDT]		"Indicates whether the TSC is safe to use in SMP mode"
	   - smp_tsc_adjust [INT,RDT]	"Try to adjust TSC on APs to match BSP"
	====
	   -> _kern_timecounter_tc
			-> _kern_timecounter_tc_"tc_name"?  "timecounter description"
				- frequency		"timecounter frequency"
				- quality		"goodness of time counter"
				- counter		"current timecounter value"
				- mask			"mask for implemented bits"


and the bpf interface tree, with new node _net_bpf_tscfg, is
_net
  -> _net_bpf		"bpf sysctl"
      - maxinsns 			"Maximum bpf program instructions"
  		- zerocopy_enable	"Enable new zero-copy BPF buffer sessions"
  		- optimize_writers"Do not send packets until BPF program is set"
		-> _net_bpf_stats				"bpf statistics portal"
		-> _net_bpf_tscfg [static] 	"Per-interface timestamp configuration"
	       - default		  	[PROC]	"Per-interface system wide default timestamp configuration"
			 - ifp->if_xname 	[PROC]	"Interface BPF timestamp configuration"
		
where ifp->if_xname is created dynamically on a per-interface basis, and ifp
is the struct ifnet for the interface, defined in sys/net/if_var.h,
which also holds bpf_if .

/* TODO:
		- rename macros to  SYSCLOCK_{FF,FB}
		- verbosity and fixes?   to  sysctl_kern_sysclock_active
		- remove old KV2 if-level stuff (or flag as KV2)
*/


==============================================================
Universal (FF/FB) System Clock Support
======================================

The traditional system clock support, and timehands architecture, is FB oriented.
With FF implemented, the system now has a choice of paradigm.
The `FF code` includes an initial attempt to adopt a more general view, whereby
both FF and FB choices are always available (ie present, compiled, initialized,
continually updated, processing updates from a deamon, and ready for use),
and which is actually used to drive the system clock is (dynamically) selectable.
Of course, each clock requires its own active daemon, otherwise the corresponding
kernel clock will be in an unsynchronized state.

The clock paradigm in service is recorded by the global variable
int sysclock_active = SYSCLOCK_{FBCK,FFWD} = {0,1}
It is initialized to SYSCLOCK_FBCK when defined in kern_tc.c, and it can be get/set by
sysctl_kern_sysclock_active() :
	  get:	returns string  sysclocks[sysclock_active]
	  set:	sets sysclock to the value SYSCLOCK_{FB,FF}CK matching the passed input string
				(ie the index of the string in sysclocks)

The value of sysclock_active is used by the  _fromclock  timestamping functions to select
the right clock within the clock-agnostic wrapper functions [get]{bin,nano,micro}[up]time .


One of the attractive features of the FF clock is the ability to take a raw timestamp,
and to convert it to a timestamp in seconds at some later time.
Two functions in kern_fflock.c  aim to make this capability universal across FF and FB, by
splitting a traditional clock read function into the underlying steps of raw timestamping,
followed by timestamp assembly/reading.

 sysclock_getsnapshot(struct sysclock_snap *cs, int fast):
 		- capture FF and FB state, and counter values, enabling both FB and FF to be read later
 sysclock_snap2bintime(struct sysclock_snap *cs, struct bintime *bt,
                       int whichclock, int wantFast, int wantUptime, int wantLerp, int wantDiff)
     - from a captured state, read the specified flavour (UPTIME etc) of the specified clock family (FF/FB)

Splitting raw and final timestamping enables the following capabilities:
	- optimal timestamp location:  raw timestamps can be taken at the best place,
	  without having to know the needed timestamp flavour, in advance and without the
	  overhead of final timestamping(s)
   - no accuracy penalty due to the latency between the split, as all state is captured
	- flexibility: can convert according to ({NATIVE,LERP,DIFF},UPTIME,raw) requirements
	- efficiency:  only capture state once to support multiple timestamps, each with their own
		   ({NATIVE,LERP,DIFF},UPTIME,fast,active sysclock,raw)  preferences.
   - extends comparisons from earlier research to include the LERP version of the FF clock
	- access to the FFcounter values gives maximum flexibility, not all state necessarily needed
   - can use to form a snapshot database enabling flexible post-processing.
	  This would per-tick however, which is very expensive compared to the underlying FF daemon
	  parameter updates, so this is really just a conceptual advantage!
	 
Including both FB and FF simultaneously in the split enables:
	- the flexibility of clock choice when timestamping:  FF, FB, active sysclock,
	  in addition to other flavour choices, including a Difference clock, and allowing
	  user applications including libprocesses to see and compare against CaMono, which is
	  created in the kernel only.
	- fair selection of FF/FB clocks, as the raw timestamps are taken
	  together as `tick-safe` pairs.  In particular this supports a perfectly fair comparison
	  methodology of rival FF/FB deamons, based on truly back-to-back timestamping.
	- counter reading efficiency in that to read the FF counter, the underlying FB counter must be read anyway.
	  There is however some overhead in always storing both states per-tick.
	- the raw timestamp (common to both the FB and FF clocks) must be slightly delayed
	  in sysclock_getsnapshot, due to the need to safely capture the current tick-data,
	  to avoid having the raw ts potentially not occurring within the tick whose data is stored.


The snapshot is based on the current tick data, and so the FF parameters will be out of date
(ie from the previous daemon update) for ticks within which FFdata updates occur. If needed, this can be
overcome in userland by just using the captured FFcounter values, in conjunction with
the radclock API  (or even better, if the daemon is storing its own database
and this is available, then the exactly corrected parameters can be obtained irrespective of
any processing delays).

Although the above benefits are generic, they are particularly suited for use within
packet timestamping in the case where multiple processes may be filtering for the same
packets, but have different requirements.  The benefits are:
  - the time consuming raw timestamping is done once only
  - all tapping processes get the same raw data regardless of how they are scheduled,
    and derived timestamps are immune to any latency between raw and final timestamping
  - tapping processes have full freedom of timestamp flavour.
See the bpf section.


The snapshot data structure is
struct sysclock_snap {
	struct fbclock_info	fb_info;		// FB tick state needed to convert a counter to all time flavours
	struct ffclock_info	ff_info;		// FF  "        	"				"				"					"
	ffcounter		ffcount;				// FF counter read  (always defined, is filled if FFCLOCK)
	unsigned int		delta;			// FB counter read  (records tc_delta(th))
	int			sysclock_active;		// records current active sysclock
};
that is a state, and a raw timestamp, for each clock, plus a record of the current active clock.
The raw counter delta=tc_delta() need only be read once, as ffcount is derived from it.

The state structures
struct fbclock_info {
	struct bintime		error;			// from external time_esterror
	struct bintime		tick_time;		// tick-start time according to FB
	uint64_t		th_scale;				// counter period   (th_scale from current timehands)
	int		status;						// from external time_status
};
struct ffclock_info {
	struct bintime		error;			// estimate error at this counter read
	struct bintime		tick_time;		// tick-start time according to FF
	struct bintime		tick_time_lerp;// tick-start time according to FF (mono version)
	uint64_t		period;					// counter period	(from current fftimehands)
	uint64_t		period_lerp;			// counter period (mono version)
	int		leapsec_adjustment;		// total leap adjustment at this counter value
	int		status;						// current status
};
contain all those fields from timehands and fftimehands respectively that are essential
for clock reading within a tick at a fixed counter value.

The 4-byte flags used for flag processing in the {fb,ff}clock_[get]{bin,nano,micro}[up]time
hierarchy are not used in sysclock_snap2bintime:
#define	FBCLOCK_{FAST,UPTIME,MASK}						// lower 2 bytes, MASK = 0x0000ffff
#define	FFCLOCK_{FAST,UPTIME,MASK,LERP,LEAPSEC}	// upper 2 bytes, MASK = 0xffff0000
because bpf needs to separate the timestamping dimensions along different lines for efficiency,
backward compatibility, and to support the raw/final split.

As noted earlier, if FFCLOCK is not defined the FF components of the sysclock framework
are removed, but the framework remains, with correspondingly reduced functionality.
In that case sysclock_active = SYSCLOCK_FBCK , and cannot be altered as
sysctl_kern_sysclock_active  does not even exist.


/*
TODO:
  - _state, or _tickstate would be better than _info
  - I changed the th_offset name to tick_time to match ff language, so should do the same for
    th_scale -> period, then all info (should call it state) fields match, except ff has lerp versions 
	 as well + leapsec_adjustment
	 OR  revert to th_offset to keep FB unchanged, but can't keep this halfway house
  - align the constants SYSCLOCK_{FBCK,FFWD}  with the sysclocks strings more formally
  - consider lighter versions of this where only timestamps are provided?  or error fields skipped?
  		- address other TODOs under  fb_info in notes
*/




==============================================================
bpf and pcap
======================================

Modifications are needed to the BPF system, as it provides the timestamping of
any packets matching bpf filters, set by processes obtaining packets via pcap.

No actual changes are required to pcap. Further, the meaning of the timestamp
type _T_ flags is set in the kernel and understood by the daemon, pcap only passes
the bpf headers containing the required data between them, transparently.

The key structure at the attached hardware interface (if) level is (bpf.c, was in bpf.h) :

struct bpf_if {
#define	bif_next		bif_ext.bif_next		// emulated older member name
#define	bif_dlist	bif_ext.bif_dlist		//		"			"
	struct bpf_if_ext bif_ext;					/* public members */   	// hide some queue stuff
	u_int					bif_dlt;					/* link layer type */
	u_int					bif_hdrlen;				/* length of link header */
	struct ifnet		*bif_ifp;				/* corresponding interface */
	struct rwlock		bif_lock;				/* interface lock */	  	// was bif_mtx
	LIST_HEAD(, bpf_d) bif_wlist;				/* writer-only list */
	int					bif_flags;				/* Interface flags */
//	struct sysctl_oid *tscfgoid;		/* timestamp sysctl oid for interface */  // abandoned FF if-level interface
//	int tstype;								/* if-level timestamp interface */			//  	"					"
	struct bpf_if	**bif_bpf;					/* Pointer to pointer to us */
};

==========
In the abandonded FF if-level interface in 9.0 (hence applying to add descriptors), also had
static int bpf_default_tstype = BPF_TSTAMP_NORMAL;   // initialising system wide default
and had tstype taking values from the _TSTAMP_ flags
	BPF_TSTAMP_{DEFAULT,NONE,FAST,NORMAL,EXTERNAL}		// DEFAULT is new
which control timestamp options relevant for the if-level.
They were used in
	bpf_{tap,mtap{2}}
	bpf_tscfg_sysctl_handler		// the big one, get/set default and per-if settings
	bpfattach2							// initialization, including new bpf_if members
and had an associated branch in the bpf interface tree, with new node _net_bpf_tscfg :

_net
  -> _net_bpf		"bpf sysctl"
      - maxinsns 			"Maximum bpf program instructions"
  		- zerocopy_enable	"Enable new zero-copy BPF buffer sessions"
		-> _net_bpf_stats				"bpf statistics portal"
		-> _net_bpf_tscfg [static] 	"Per-interface timestamp configuration"
	       default	[PROC]	"Per-interface system wide default timestamp configuration"
		    ifp->xname? [PROC] "Interface BPF timestamp configuration"
		
==========

For a single interface there can be many descriptors correponding to different processes
wishing to monitor or receive packets. The key structure at this (d) level (sys/net/bpfdesc.h) is
struct bpf_d {
// needed in bpf_tap
   LIST_ENTRY(bpf_d) bd_next;      /* Linked list of descriptors */
   u_int64_t       bd_rcount;      /* number of packets received */
   u_int64_t       bd_fcount;      /* number of packets that matched filter */
	int           	 bd_tstamp;      /** select time stamping function **/
 	....
 // needed in catchpacket
	caddr_t         bd_sbuf;        /* store slot */
   caddr_t         bd_hbuf;        /* hold slot */
   int             bd_slen;        /* current length of store buffer */
   int             bd_hlen;        /* current length of hold buffer */
   int             bd_bufsize;     /* absolute length of buffers */
   u_int64_t       bd_dcount;      /* number of packets dropped */
   u_char          bd_state;       /* idle, waiting, or timed out */
	u_char          bd_compat32;    /* 32-bit stream on LP64 system */
	...
}

The key one here is bd_stamp, which specifies the timestamping type requested
on the given descriptor, using the _T_ timestamp type flags :

/* Time stamping flags */
// FORMAT flags 	[ mutually exclusive, not to be ORed ]
#define	BPF_T_MICROTIME	0x0000
#define	BPF_T_NANOTIME		0x0001
#define	BPF_T_BINTIME		0x0002
#define	BPF_T_NONE			0x0003	// relates to ts only, FFRAW independent
#define	BPF_T_FORMAT_MASK	0x0003
// FFRAW flag
#define	BPF_T_NOFFC			0x0000   // no FFcount
#define	BPF_T_FFC			0x0010   // want FFcount
#define	BPF_T_FFRAW_MASK	0x0010
// FLAVOR flags   [ can view bits as ORable flags ]
#define	BPF_T_NORMAL		0x0000	// UTC, !FAST
#define	BPF_T_FAST			0x0100   // UTC,  FAST
#define	BPF_T_MONOTONIC	0x0200	// UPTIME, !FAST
#define	BPF_T_MONOTONIC_FAST	0x0300// UPTIME,  FAST
#define	BPF_T_FLAVOR_MASK	0x0300
// CLOCK flags   [ mutually exclusive, not to be ORed ]
#define	BPF_T_SYSCLOCK		0x0000	// read current sysclock
#define	BPF_T_FBCLOCK		0x1000   // read FB
#define	BPF_T_FFCLOCK		0x2000   // read mono FF (standard reads are mono)
#define	BPF_T_FFNATIVECLOCK	0x3000	// read native FF
#define	BPF_T_FFDIFFCLOCK	0x4000	// read FF difference clock
#define	BPF_T_CLOCK_MASK	0x7000

// Extract FORMAT, FFRAW, FLAVOR, CLOCK  bits
#define	BPF_T_FORMAT(t)	((t) & BPF_T_FORMAT_MASK)
#define	BPF_T_FFRAW(t)		((t) & BPF_T_FFRAW_MASK)
#define	BPF_T_FLAVOR(t)	((t) & BPF_T_FLAVOR_MASK)
#define	BPF_T_CLOCK(t)		((t) & BPF_T_CLOCK_MASK)

// Used to vet descriptor passed to BPF via BIOCSTSTAMP ioctl
// In KV3, all components are independent, and either always meaningful, or
// not acted on if not meaningful (eg if !FFCLOCK, or value of CLOCK if requesting
// BPF_T_NONE   Hence checks reduce to ensuring no bits in undefined positions,
// and not ask for a FF clock that doesnt exist.
#ifdef FFCLOCK
#define	BPF_T_VALID(t)	( ((t) & ~(BPF_T_FORMAT_MASK | BPF_T_FFRAW_MASK | \
											  BPF_T_FLAVOR_MASK | BPF_T_CLOCK_MASK)) == 0 \
									&& BPF_T_CLOCK(t)<=BPF_T_FFDIFFCLOCK )
#else
#define	BPF_T_VALID(t)	( ((t) & ~(BPF_T_FORMAT_MASK | BPF_T_FFRAW_MASK | \
											  BPF_T_FLAVOR_MASK | BPF_T_CLOCK_MASK)) == 0 \
									&& BPF_T_CLOCK(t)<=BPF_T_FBCLOCK )
#endif

In KV=3 the four dimensions of the flags: FORMAT, FFRAW, FLAVOR, CLOCK  are completely
independent. There are two central innovations compared to the pre-FF kernel:
	i)  two kinds of timestamps: in seconds (ts) and in raw counter units (raw ts), that are simultaneously available
	ii) for ts, multiple clocks can be selected from, including the specialist FF clocks
For each dimension, the choices are:
 FORMAT:  (ts only)		specifies the standard resolution type, or to take no timestamp (NONE)
 FFAW:    (raw ts only) specifies if want it or not
 FLAVOR:  (ts only)		covers standard options of UTC versus UPTIME (time since boot), and
        FAST (use current tick-start time, dont bother taking a delta and interpolating intra-tick)
 CLOCK:	 (ts only)		select FB, or the type of FF desired, or adopt whatever the current
		  system clock is  (could be FBCLOCK or FFCLOCK)


Partial code breakdown of bpf timestamping
------------------------------------------

bpf_tap(struct bpf_if *bp, u_char *pkt, u_int pktlen) {

	/* Obtain state data and tc counter raw ts for both FF and FB clocks */
	sysclock_getsnapshot(&cs, 0);		// 0 to assume !FAST at if-level, so each d has a choice
	
	LIST_FOREACH(d, &bp->bif_dlist, bd_next) {
		++d->bd_rcount;
		
		slen = bpf_filter(d->bd_rfilter, pkt, pktlen, pktlen);
		if (slen != 0) {
			BPFD_LOCK(d);	// filter matches, acquire write lock
			d->bd_fcount++;
						
			/* Obtain ts if requested. [In bpf_mtap, add extra check that external ts not already taken] */
			if ( BPF_T_FORMAT(d->bd_tstamp)!=BPF_T_NONE ) {   // process BPF_T_NONE
				if ( (sysclock_active==SYSCLOCK_FBCK				// process BPF_T_CLOCK
						&& BPF_T_CLOCK(d->bd_tstamp)==BPF_T_SYSCLOCK)
						|| BPF_T_CLOCK(d->bd_tstamp)==BPF_T_FBCLOCK )
					sysclock_snap2bintime(&cs, &bt, SYSCLOCK_FBCK,	// process BPF_T_FLAVOR for FB case
						d->bd_tstamp & BPF_T_FAST,
						d->bd_tstamp & BPF_T_MONOTONIC, 0, 0);
				else
					sysclock_snap2bintime(&cs, &bt, SYSCLOCK_FFWD,  // process BPF_T_FLAVOR for the FF cases
						d->bd_tstamp & BPF_T_FAST,
						d->bd_tstamp & BPF_T_MONOTONIC,
						BPF_T_CLOCK(d->bd_tstamp)< BPF_T_FFNATIVECLOCK,
						BPF_T_CLOCK(d->bd_tstamp)==BPF_T_FFDIFFCLOCK );
			}	else
				bzero(&bt, sizeof(bt));
				
			ffcounter ffcount = 0;
			if (BPF_T_FFRAW(d->bd_tstamp) == BPF_T_FFC)		// process BPF_T_FFC
				catchpacket(d, pkt, pktlen, slen, bpf_append_mbuf, &bt, &cs.ffcount); // also processes BPF_T_NONE
			else
				catchpacket(d, pkt, pktlen, slen, bpf_append_mbuf, &bt, &ffcount);

			BPFD_UNLOCK(d);
		}
	}
}
	
	
The top level function bpf_tap (bpf_mtap{2} are analogous) processes the
if-level, then loops over all attached descriptors {d}.
For each d, if the pkt passes d''s filter, the _T_ flags (except FORMAT if it is not NONE)
are processed in order to take the appropriate ts and raw-ts timestamps, which are then passed
as arguments bt and ffcount to catchpacket().

For efficiency and flexibility reasons, the timestamping is performed using the
raw/final split described above. Thus sysclock_getsnapshot is called before the d-loop
to obtain the state once-only needed for all possible timestamp requests (thus FAST cant
be acted on at this level) whereas sysclock_snap2bintime is called separately for each d,
which can have any combination of the 4 _T_ dimensions.

The input to timestamp generation is d->bd_stamp, which carries the desired
BPF_T_{FORMAT,FFRAW,FLAVOR,CLOCK} flags given to pcap from the calling program.
In the radclock daemon,  d->bd_stamp  is get/set in pcap-fbsd.c:descriptor_{get,set}_tsmode
using   ioctl(pcap_fileno(p_handle), BIOC{G,S}TSTAMP, (caddr_t)(&bd_tstamp)).
Note that these ioctl commands are handled by bpf.c:bpfiotl(*dev, cmd, *data,.)
not by pcap as such.

The role of catchpacket is to store the packet and timestamps into a buffer structure for that d.
It first completes the flag processing by performing the FORMAT conversion specified in
d->bd_stamp, using the function
	bpf_bintime2ts(struct bintime *bt, struct bpf_ts *ts, int tstype) 	// process BPF_T_FORMAT  [only]
which returns the bt-inspired bpf-specific timestamp structure:

struct bpf_ts {
	bpf_int64	bt_sec;		/* seconds */
	bpf_u_int64	bt_frac;		// big enough to hold frac component of timeval, timespec, bintime, not itself bintime!
};

The final timestamps are then placed in the variable  hdr  of eXtended bpf_xhdr type (bpf.h) :
struct bpf_xhdr {
	struct bpf_ts	bh_tstamp;	// timestamp was timeval in obsolete bpf_hdr and bpf_hdr32
	ffcounter	bh_ffcounter;	// raw FFcounter timestamp  [new, in bpf_hdr{32} also]
	bpf_u_int32	bh_caplen;		/* length of captured portion */
	bpf_u_int32	bh_datalen;		/* original length of packet */
	u_short		bh_hdrlen;		/* length of bpf header (this struct plus bpf-style alignment padding) */
};

The above description actually pertains to the 64bit (BURN_BRIDGES) case. Otherwise,
the variable 	struct bpf_hdr hdr_old;  or  	struct bpf_hdr32 hdr32_old
is used instead of struct bpf_xhdr hdr.   Thus, still used, despite marked as obselete, is
	struct bpf_hdr {
		struct timeval	bh_tstamp;	/* time stamp */
		...
	}
where the header size depends on the size of timeval, which can be  2*32 or 2*64 depending on the arch!
and the explicitly 32bit version
	struct bpf_hdr32 {
	  struct timeval32 bh_tstamp;	/* time stamp */    // timeval32 is just a traditional 2*32bits
	  ...
used in the explicit 32bit support code, used in catchpackets extensively, protected by
#ifndef BURN_BRIDGES   		// ie  if keeping 32bit support.   BURN_BRIDGES=1 => abandon 32bit support

Also relevant here is the pcap header
struct pcap_pkthdr {
	struct timeval ts;		/* time stamp */
	bpf_u_int32 caplen;		/* length of portion present */
	bpf_u_int32 len;			/* length this packet (off wire) */
};
whose length will also depend on that of timeval.



======================================================
An important topic here is the alignment of headers, as this is essential to understand
how to successfully extract the required fields in the daemon later, based on what pcap
returns to it, since applications access bpf via pcap.

Essentially, bpf has its own alignment needs for headers, so need to override normal
pointer alignment of structures to conform to the BPF_ALIGNMENT granularity.

The struct bpf_if  has a member  u_int bif_hdrlen  carrying the fixed size of the link header.
This is set somewhere outside of bpf.c, by whatever function
calls bpfattach(*ifp, dlt, hdrlen), presumably once per interface only.

Within bpf.c,  bpf_hdrlen   calculates the header size that is used
internally by bpf, in particular the size pre-pended to the frame body in catchpacket.
// The part of the [bpfhdr][frame] = [bpfhdr][linkhdr][IP] which covers [bpfhdr][linkhdr]
// is also bpf-aligned, not just bpfhdr, because some bpf or pcap fn somewhere needs to grab that efficiently
static int bpf_hdrlen(struct bpf_d *d) :
	int hdrlen;
	hdrlen = d->bd_bif->bif_hdrlen;					// initialize using actual link hdr size
	# select correct  bpf_[x]hdr[32]  according to BURN_BRIDGES, COMPAT_FREEBSD32
	hdrlen += SIZEOF_BPF_HDR(struct bpf_hdr);		// add minimal size stripped of trailing padding from compiler alignment
	hdrlen = BPF_WORDALIGN(hdrlen);					// align according to bpf alignment rules (add padding back, but usually less)
	return (hdrlen - d->bd_bif->bif_hdrlen);		// remove the compiler aligned initial constant, remains bpf aligned

#define	SIZEOF_BPF_HDR(type)	\
(offsetof(type, bh_hdrlen) + sizeof(((type *)0)->bh_hdrlen))	// length stripped of trailing padding

/* From bpf.h:  Alignment macros.  BPF_WORDALIGN rounds up to the next even multiple of BPF_ALIGNMENT */
#define BPF_ALIGNMENT sizeof(long)
#define BPF_WORDALIGN(x) (((x)+(BPF_ALIGNMENT-1))&~(BPF_ALIGNMENT-1))


The daemon code that performs the header-size aware extraction is in pcap-fbsd.c
Being a library, pcap is not in the kernel, and it is not yet bpf_xhdr or bpf_ts compatible.
Thus although the FFcounter member is officially in all the bpf headers, there is no
pcap support to extract it painlessly, as its position within the header will vary
depending on the size of pcap header''s ts member.
The function that does the hard work is

	extract_vcount_stamp_v3(pcap_t *p_handle, const struct pcap_pkthdr *header, const unsigned char *packet, vcounter_t *vcount)
	
that relies on struct bpf_hdr_hack_v3  which mirrors  a struct bpf_hdr where bh_tstamp is a timeval.
This is convenient as the compiled daemon will know the size of timeval without the need for explicit testing and branching.
This function performs reliable tests to ensure that the timestamp being extracted is as expected.
Also in pcap-fbsd.c  is  ts_extraction_tester(p_handle, header, packet, vcount);  which
has extensive verbosity and tests to see what is really going on in the bpf -> pcap mapping.
More generally, the file has extensive comments describing all these issues in more detail.
See the algo Doc for the timestamp mode setting of the daemon.


// TODO: - what is the point of  #define BP ?   remove?




==============================================================
Device drivers
======================================



====================================
Drivers need modifications to use the new bpf functions, and to ensure timestamping
occurs in the right place.  Here changes are not protected by a guard macro, but
a "\\FFCLOCK"  comment flags them, but we describe them using the {Un}protected etc notation.

dev/e1000/if_em.c		// changes built on 9.0, quite a few other changes in 9.2, 10
  Unprotected
		em_xmit										// new argument allowing internal call to ETHER_BPF_MTAP
		em_{,mq}_start_locked               // call to new em_xmit
  Protected from:
		em_{,mq}_start_locked               // commenting out call to ETHER_BPF_MTAP
		em_initialize_receive_unit          // E1000_WRITE_REG: disable interrupt throttling setting with


Here changes are flagged by the "\\ben" instead of"\\FFCLOCK" :
dev/e1000/if_igb.c	 // changes built on 9.0.0   File missing from watson
  Exactly analogous changed to if_em.c, just applied to igb_xmit instead of em_xmit .


In addition, both if_{em,igb}.c have associated DTRACE code, flagged by "\\ben" :
		igb_rxeof					: just adds a DTRACE probe, not related to FF
		  SDT_PROBE(timestamp, if_igb, igb_rxeof, stamp, sendmp, 0, 0, 0, 0);






==============================================================
Leapsecond system  [ kernel, and kernel<-->daemon ]
======================================

The leapsecond system is independent of that used for the traditional FB clock.
All required leap second information is provided by the daemon, via the three
relevant members in rad_data, passed directly into ffclock_estimate:
	vcounter_t	leapsec_next;		/* Estimated counter value of next leap second. */
	int16_t		leapsec_total;		/* Sum of leap seconds seen since clock start. */
	int8_t		leapsec;				/* Next leap second (in {-1,0,1}). */

Whereas leapsec_total records the aggregate leap seen so far, leapsec_next allows
an upcoming leap to be taken into account immediately when timestamping.
The main place this is used is ffclock_abstime, where FFCLOCK_LEAPSEC
flags that we want to incorporate leap seconds, thereby converting the native
leap-free absolute FFclock into FFclock''s UTC clock.

	if ((flags & FFCLOCK_LEAPSEC) == FFCLOCK_LEAPSEC) {	// want UTC
		bt->sec -= cest.leapsec_total;	// subtracting means including leaps!
		if (ffc > cest.leapsec_next)		// if past expected imminent leap,
			bt->sec -= cest.leapsec;		// include it
		
For further details, see the leap section of RADclockDoc_algo_bidir.


## FB leap second system
 - how does it work?   could we piggyback?
 -
 
 
 
###########################################  from RADclockDoc_algo_bidir
==============================================================
Leap second treatment
============================

	int leapsec_total;				// sum of leap seconds seen since clock start
	int leapsec_next;					// value of the expected next leap second {-1 0 1}
	vcounter_t leapsec_expected;	// estimated vcount of next leap, or 0 if none coming


- Do need leap awareness in both
     daemon:  - gets UTC timestamps, must subtract out leaps for algo as usual
				  - gets LI bits
					 - best to send info to leap decision maker only, not take on this role!
				  - API: option to return time without leap info if asked?
	  kernel:  - reads leapsec file?  how coordinate with LI info to make final decision?
	           - how execute the jump at exactly the right time?
				  - how/why option to return time without leap info if asked

- ffclock_info.leapsec_adjustment logic: since counter set, can precalculate the correct
 leapsec information  ( same calculation as in ffclock_windup )
 PROVided this info doesnt change due to a FFdata update


Leap issues:
  - what to pass to algo, what else the kernel needs
  - how to handle historical conversions, ie if passed a vcounter before a leap, to a clock that already built in the leap..
  - how much in kernel compared to in daemon, duplicate functionality? complementary
  - do the leap fields in ffclock_estimate  get updated inbetween daemon updates?
    - nice to keep the daemon structure of updates only at stamps
	 - but what if daemon dies?  still want to pick up extra leaps to add into clock reading.
  - in bpf.c:bpf_bintime2ts   UTC is obtained by adding a constant to the monotonic (uptime)
    FB default, but this implies leaps must be built into  bootimebin !





==============================================================
PPS stuff:
======================================

/********* timepps.h ***********/
- has many structures and variables with FF components, ALWAys defined, eg
	136 struct pps_state {
	137         /* Capture information. */
	138         struct timehands *capth;
	139         struct fftimehands *capffth;  // breaks old interfaces, binaries?
	140         unsigned        capgen;
	141         unsigned        capcount;
	142 
	143         /* State information. */
	144         pps_params_t    ppsparam;
	145         pps_info_t      ppsinfo;
	146         pps_info_ffc_t  ppsinfo_ffc;  // and here
	147         int             kcmode;
	148         int             ppscap;
	149         struct timecounter *ppstc;
	150         unsigned        ppscount[3];
	151 };
- architecture seems to be:  augment structures, but write new 
  analogous functions if actually want to act on new FF parts.

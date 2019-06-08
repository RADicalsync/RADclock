FFclock FreeBSD V4 documentation.
Written by Darryl Veitch, 2018-19
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
 tc = timecounter,  the underlying hardware counter (and its access data structure)
 Ca = Absolute FF clock
 Cd = Difference FF clock


Main TODOs for basic compilation
  - new version of bpf.c ,  catchpackets, drivers


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
  - bypass versus passthrough, connection?  can the kernel use bypass??
  - proper behaviour for KV=3
  - correct reaction to a tc change

==============================================================
Summary
=======

This documents the FFclock freeBSD kernel code, focussing on kernel version 2
and above.  The code includes the synthesis of a FF compatible counter, support
for a full featured FF system clock interfacing with a FF clock daemon,
in-kernel FF packet timestamping support, and the provision of a universal FF/FB
system clock interface.
The main source files are in RADclockrepo/kernel/freebsd/FreeBSD-CURRENT/
	kern_ffclock.c
	kern_tc.c
	ntp.c
The main associated header files are
	_ffcounter.h
	timeffc.h
	ntp.h

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
RADclock specific.

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

As FFclock support is not yet mainstream, the FF code is written to be protected by the
FFCLOCK guard macro.  However, even if FFCLOCK is not defined, the non-FF portions of the
universal (Uni) code remain defined.

For a complete understanding of how the FFclock interacts with radclock as the
canonical FF daemon, the interactions between the two are also detailed.
Whereas RADclockDoc_library  documents the types and purposes of the
kernel <--> daemon interactions in general terms, here we give the detailed
technical connections to the algo, explaining why things are structured as they are.

The kernel versioning parameter lives in the pure FF file kern_ffclock.c
	static int ffclock_version = 3;     // declare value of KV, the kernel version of the FF code
and is accessible by a daemon via the FF-expanded sysctl tree.



==============================================================
FFCLOCK symbol and File Breakdown
======================================

Here we survey all files touched by FF code, whether it be new FF code, or modified
non-FF code.

The FFCLOCK guard macro controls whether FFclock support is available for compilation.
More than that, it protects almost all FF related code, so that the symbol is not defined,
what you (almost) have is a kernel prior to the FF code enhancements.
The main exception is the universal sysclock (FF/FB) support which remain active (see below),
though with limited functionality if FFclock is FALSE.  The other exception currently is
driver modifications, but this should change.

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


kern_tc.c				[ the lower higher level FF code integrated with legacy tc code]
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
   inittodr 											// simply calls ffclock_reset_clock



=======================================================
Packet capture requires additional components to support FF and Universal timestamping

bpf.h
 Protected
 		# define BP 			// usage unclear
 Unprotected
 		New macros: BPF_T_{FFCOUNTER,*CLOCK*} and BPF_T_{FB,FF}CLOCK_{MICRO,NANO,BIN}TIME_MONOTONIC
		New ffcounter members of  struct bpf_{x,}hdr

bpf.c
 Partially rotected:
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


=======================================================
Drivers need modifications to ensure timestamping occurs in the right place.
Here changes are not protected by a guard macro, but
a "\\FFCLOCK"  comment flags them, but we describe them using the {Un}protected etc notation.

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
The beginning of each cycle (or abusively, the cycle itself) is known as a `(hardclock) tick`.
In modern processors hz=1000 typically, corresponding to an interval 1/hz = 1ms between ticks.
The variable kern/subr_param.c:tick  is the nominal number 1000000/hz of mus per tick.

The tc code is based on maintaining state which is updated on a roughly
periodic tc_tick grid.  The value of tc_tick is set in inittimecounter() to be ≥1,
ensuring that tc-tick updates are at least as frequent as hardclock ticks.
It is expressed in units of the number of hardclock ticks per ms.
For convenience, henceforthe we use `tick` to refer to tc-ticks, and hardclock ticks
otherwise.

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
	struct bintime		th_bintime;		 // UTC timeval  version of th_offset		[ new in 11.2 ]
	struct timeval		th_microtime;   // UTC timeval  version of th_offset
	struct timespec	th_nanotime;    // UTC timespec version of th_offset
	struct bintime		th_boottime;	 // boottime value for this tick				[ new in 11.2 ]
   volatile u_int		th_generation;  // wrap-count to ensure tick state consistency
	struct timehands	*th_next;       // pointer to next timehands variable in cyclic buffer
};

The basic idea is that uptime timestamps are calculated based off the tick-start as :
	C(t) = th_offset + th_scale*∆(tc) ,
where nominally ∆(tc)= tc(t) - th_offset_count ,  using  th_counter->tc_counter_mask
to take wrapping of the tc into account.

The timehands variables are defined as externals in kern_tc.c :
	static struct timehands th{0,1,2,...,9};               // individual tick state containers
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
and shares the same tick update triggering, but supports/creates an entirely separate clock.
The FF version of timehands structure is

struct fftimehands {
	struct ffclock_estimate	cest;      // copy of kernel FF data from daemon used during this tick
	struct bintime		tick_time;       // UTC Ca timestamp at tick-start
	struct bintime		tick_time_lerp;  // monotonic version of UTC Ca at tick-start
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
based on LinearinTERpolation, and is contained in ffclock_windup.

In summary, fftimehands provides four things:
  i) a way to define an FF counter via tick based updates to tick_ffcount and the existing ∆(tc)
  ii) a way to support safe tick-based reading of the FF clock Ca
  iii) the state needed to support a monotonic version of Ca
  iv) a fast way to obtain an FF timestamp with tick-accuracy: namely tick_time{_lerp}

A circular buffer is created in a similar way to timehands :
static struct fftimehands ffth[10];								// buffer
static struct fftimehands *volatile fftimehands = ffth;  // pointer to current fftimehands
where the initial circular linking is performed neatly by ffclock_init().

The cest member variable holds the data of the (kernel version of the) daemon''s FF clock.
It is important to note that it will be updated on a poll_period timescale set by
the daemon, whereas fftimehands will advance to fresh ticks independently of this,
and 1000''s of times more frequently.

The timehands and fftimehands systems are separate, with separate generation variables,
however they update on ticks in a synchronous manner, because the timehands update
function: tc_windup, calls that for fftimehands: ffclock_windup .


/*
TODO:
  - consider a name change LERP -> MONOFF
  -
*/



==============================================================
FF algo data kernel <--> daemon translation
======================================

The FF code defines a data structure, ffclock_estimate, defined in timeffc.h
(backup in kclock.h), which contains the key information which the kernel needs
to record the state of, and interact with, a FF clock algorithm.
This includes being able to read such a clock when doing so
via the daemon is impossible (daemon dead) or too costly (within the kernel).
Essentially, this structure holds a kernel-specific version of the current
FF clock parameters  (called the `global data` in KV<2), which should be updated
whenever a new stamp is processed by the algo.

struct ffclock_estimate
{
	struct bintime	update_time;	/* FF clock time of last update, ie Ca(tlast). */
	vcounter_t	update_ffcount;	/* Counter value at last update. */
	vcounter_t	next_expected		/* Estimated time of next update [counter] */
	uint64_t	period;					/* Estimate of current counter period  [2^-64 s] */
	uint32_t	errb_abs;				/* Bound on absolute clock error [ns]. */
	uint32_t	errb_rate;				/* Bound on relative counter period error [ps/s] */
	uint32_t	status;					/* Clock status. */
	vcounter_t	leapsec_expected;	/* Estimated counter value of next leap second. */
	int16_t		leapsec_total;		/* Sum of leap seconds seen since clock start. */
	int8_t		leapsec_next;		/* Next leap second (in {-1,0,1}). */
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
   ffclock_estimate is oriented toward the fftimehands interface view: time@currenttick + ∆,
	needed to simultaneously support reading the FF clock, and the FF counter itself
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
	vcounter_t last_changed;// raw timestamp T(tf) of last stamp processed [counter]
	vcounter_t next_expected// estimated T value of next stamp, and hence update [counter]
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
	 As the equations show, the two formulations are entirely equivalent, but the avantage
	 is that the FF side has fewer calculations to perform, fewer parameters to worry about,
	 and the format fits perfectly the per-tick (ff)timehands paradigm needed to
	 synthesize the FF counter by adopting the method used in timehands.

FF --> algo
	 ca = update_time - plocal*last_changed  (up to format conversion)
	 This should be update_time - phat*last_changed  but phat is not available.
	 Since this is used in circumstances where the daemon is dead, using plocal is natural,
	 effectively it means that pbar = outofdate plocal going forward - close to optimal anyway


/* Incomplete Correspondances [ examples with at least one end partly missing ] */
? == phat_err
	FF <-- algo:	N/A: no phat field, phat value is encoded in update_time
	FF --> algo:	already setting phat=plocal both literally and in essence, so can
						just set phat_err = phat_local_err


vcounter_t next_expected == vcounter_t next_expected
	FF <-- algo:	Direct. The kernel should know when an update is expected.
						Status update code in ffclock_windup needs this.
	FF --> algo:	N/A: poll_period is owned by daemon.
						The kernel can never tell the daemon what the poll_period is,
						particularly if the daemon is dead.

/* Missing Correpondances */
leapsec_{next,total,} == ?
   These support the leapsec system.  The algo should not know about leap seconds, so it is
	consistent that there are no links from radclock_data here, however, the daemon
	needs to have leap awareness.  This is a complicated topic and is treated below.





/* TODO notes:
  - ffclock_estimate is a really poor name, but points to Ca storage instead of (phat,ca), I guess
	 more natural is ffclock_data
  - consider if add next_expected field on FF side, and if so, alter status update code in ffclock_windup to use it and remove  FFCLOCK_SKM_SCALE.  See notes.
*/





==============================================================
Global Variables   // integrate with FFCLOCK symbol section?
======================================

The global variables are all defined in kern_tc.c

The main variables supporting the timecounter mechanism are:

static struct timecounter dummy_timecounter = {
	dummy_get_timecount, 0, ~0u, 1000000, "dummy", -1000000
};
static struct timehands th{0,1,2,...,9};                 // used as a circular tick-data buffer
static struct timehands *volatile timehands = &th0;      // points to data from last tick
                                                         // is advanced by tc_windup
struct timecounter *timecounter = &dummy_timecounter; 	// hardware tc currently used
static struct timecounter *timecounters = &dummy_timecounter;  // points to last tc created
int tc_min_ticktock_freq = 1;		// slowest rate of windup calls to avoid th_offset_count wrappping

time_t time_second = 1;				// quick access to the current UTC    second updated by tc_windup
time_t time_uptime = 1;				// quick access to the current uptime second updated by tc_windup
struct bintime boottimebin;		// UTC time of boot, bintime format
struct timeval boottime;			// UTC time of boot, timeval format



The complete set supporting the FFtimecounter extension is

struct ffclock_estimate ffclock_estimate;	// FF clock state variable
struct bintime ffclock_boottime;	/* Feed-forward boot time estimate. */
int8_t ffclock_updated;			/* New estimates are available. */
uint32_t ffclock_status;		/* Feed-forward clock status. Values used: {0,1,INT8_MAX} */
struct mtx ffclock_mtx;			/* Mutex on ffclock_estimate. */

static struct fftimehands ffth[10];
static struct fftimehands *volatile fftimehands = ffth;  // pointer to fftimehands array


The division of FF clock labour across kern_tc.c and kern_ffclock.c is primarily about
access to the timecounter internals themselves:

kern_tc.c:
	- original timecounter code and FFtimecounter extensions
	- code that needs access to the timehands and fftimehands, and code to support that code
	- code that relies explicitly on data held per-tick
	- in particular
	    - the FFclock read function   ffclock_read_counter(&ffc)
		 - the two variants of the system FF clock (native and LERP), based off
		   the underlying FFclock disciplined by the daemon.

kern_ffclock.c:   "if it doesnt need access to per-tick, then dont give it access"
   - code that doesnt need direct tick or timecounter access, that assumes the counter
	  can be read.  It relies on code in kern_tc.c, but not vice versa.
	  Hence it is about protecting the FFtimecounter code by keeping kern_tc minimalist
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

The operating principle of tick-state, held in {ff}timehands, is that it is consists
of values that correspond to, and/or where recorded at, the beginning of a tick,
technically the location of the tc_delta raw timestamp call. This state
is to be used for any timestamp purposes initiated at any time during that tick.

The function which updates the tick state for the FB clock is
	static void tc_windup(void)
It is called by inittimecounter after initializing tc_tick,
by tc_setclock to process updated information on the true UTC time (including leaps?),
and most importantly, periodically once per tc_tick within tc_ticktock() .

Since tc_windup IS the function that updates timehands, it does not require
generation protection.   It does however set the generation of the next tick it
is preparing ("th") to zero, so that any calculation in-train pointing to that tick
will be aware it cannot be safely used. At the end of tc_windup the generation will
become valid, and those calculations can complete (using the latest tick data).

For the same of brevity, the following code summaries play fast and lose with
pointers versus *pointers, and other inconsistencies.

 tc_windup: simplified code:
 ---------------------------
   tho = timehands;                    // grab the current tick
	th = tho->th_next	..						// initialize next tick "th" to current tick values

	// raw timestamping
	delta = tc_delta(tho)					// take raw FB timestamp using tc of this tick
	ncount = timecounter->tc_get_timecount(timecounter);  // take one using globally current tc
	ffclock_windup(delta)					// fftimehands tick update including FFcounter read
	th_offset_count += delta				// add-in new delta, in wrap-safe manner

	// old tick data (and tc) must be used to update from start of old, to start of new
	th_offset += th_scale * delta			// done respecting binary representation limitations

	tho->th_counter->tc_poll_pps(tho->th_counter)	// if tc of PPS type, poll it

	// update UTC members
	update th_{micro,nano)time				// use 'magic' ntp_update_second processing, includes leap

	// process a changed timecounter
	if (th->th_counter != timecounter) {// if global tc different to one used in current tick
		disable hardware sleep if can
		th_counter = timecounter;
		th_offset_count = ncount
		tc_min_ticktock_freq	= ..			// reset tick frequency to ensure counter wraps caught
	   ffclock_change_tc(th)				// let the FF cope with this radical change
   }

	// update period=1/freq  estimate
	scale = (1 + th_adjustment)/th_counter->tc_frequency // rely on NTPd to adapt scale to new tc
																		  // code uses binary approximations

   th_generation = ++ogen					// announce this tick has valid data (avoid special 0 value)
	
	// fill global convenience variables  (used in kern_ntptime.c)
	time_second									// universal update (switches on sysclock_active)
	time_uptime									//		"		"
	
	timehands = th;							// advance to the new tick  [ fftimehands already advanced ]
 ---------------------------


The timehands and fftimehands systems are separate, with separate generation variables,
however they update on ticks in a synchronous manner, because the timehands update
function: tc_windup, calls that for fftimehands: ffclock_windup, which we now describe.

In the following, the pointer to the current tick is frequently omitted for brevity, hence
tick_ffcount instead of fftimehands->tick_ffcount etc .  Use is made of the globals
int8_t ffclock_updated;			/* New estimates are available. */
uint32_t ffclock_status;		/* Feed-forward clock status. Values used: {0,1,INT8_MAX} */

ffclock_windup(delta): simplified code:
---------------------------------------
	ffth = fftimehands->next				// point to next tick "ffth", set gen=0
	
	// initialize from current tick members that may require  ffclock_updated  processing later
	cest
	period_lerp
	tick_time
	tick_error							// propagate from last tick: error+= cest->errb_rate*period*delta

	// advance to start of next tick using current tick values: wont change even if ffclock_updated
	tick_time{_lerp}								// keep CaMono continuous
	tick_ffcount = tick_ffcount + delta 	// FF timestamp location in fact that of tc_delta

	// if no new FF data available, check this is ok
	if (ffclock_updated == 0)
  		 if cest->update too stale						// based on FFCLOCK_SKM_SCALE: needs changing
			ffclock_status |= FFCLOCK_STA_UNSYNC	//
// else ffclock_status = cest->status; see below
	
   ---------------------------------------------------
	// adopt and process FF data update occuring during previous tick  (and maybe just before here)
	if (ffclock_updated > 0) {
		cest = ffclock_estimate			// update normally in last tick, but could be after delta taken
		ffdelta = tick_ffcount - cest->update_ffcount;  // update to tick-start interval
																		// assumed +ve, ie update before delta

	 /* update Ca members */
		tick_time = cest->update_time + ffdelta*period  // recalculate directly off last update
	   tick_error = cest->errb_abs + cest->errb_rate*period*delta	// re-base from update:
		 
	 /* adjust CaMono continuity in extreme cases  */
		// INT8_MAX means update set from ffclock_reset, not deamon, because of change to RTC
		// sets tick_time to argument, other fields best effort,  cest.status=FFCLOCK_STA_UNSYNC
		if (ffclock_updated == INT8_MAX)
			tick_time_lerp = tick_time			// let the CaMono jump, since have reset radically
	   
		// record new disparity at tick-start between Ca and CaMono
		forward_jump = {0,1}  								// Ca jumps {down,up} wrt CaMono
		gap_lerp = abs(tick_time - tick_time_lerp)	// ≥0, sign carried by forward_jump
		
		// jump reset CaMono to Ca if in particular bad conditions
		if (UNSYNC in kernel but not in daemon, and in WARMUP)
			ffclock_boottime += gap_lerp 		// kill gap to avoid very slow CaMono -> Ca convergence
			tick_time_lerp = tick_time			// jump lerp to make gap=0
		ffclock_status = cest->status;		// now kernel status acted on, override with update
		
	/* readjust CaMono period */
	 	ffth->period_lerp = cest->period;	   // start afresh with this update, no past dynamics
	   ffdelta = next_expected - thisupdateFF // counter diff between last and next updates
		time_til_nextupdate = floor( ffdelta*period ) // corresponding #seconds [ estimated polling period ]
		gap_lerp = min(gap_lerp,time_til_nextupdate*5000PPM) // cap rate of gap reduction to 5000PPM
	   period_lerp += gap_lerp/ffdelta        // adjust to reduce gap to zero over ffdelta cycles
		
	 	ffclock_updated = 0;						// update processed
	}
   ---------------------------------------------------

   ffth->gen = ++ogen						// this tick has has valid data (avoid special 0 value)
	fftimehands = ffth;						// advance to the new tick
 ---------------------------


Consistency of both {ff}timehands should be checked via timehands->th_generation, and this
guards the entire windup procedure.



/* TODO
	- v-unlikely but possible that ffdelta is `negative` if update occurs after delta read
	  fix by checking for either sign (more complex but get update earlier), or discarding if negative (simpler and technically correct) and not resetting ffclock_updated
   - should fftimehands = ffth   be delay to align with timehands = th,  thereby 
	  allowing a tc change to be managed without an extra FF tick advance?
*/


==============================================================
Raw Timestamping
======================================

Let th and ffth be the current tick states for timehands and fftimehands respectively.
The underlying function making counter reading possible for both FB and FF clocks is
tc_delta(th), which returns the number of counter increments since th->th_offset_count
was set, taking wrapping into account.

static __inline u_int  tc_delta(struct timehands *th) {
	struct timecounter *tc;
   tc = th->th_counter;
   return ((tc->tc_get_timecount(tc) - th->th_offset_count) & tc->tc_counter_mask);
}

The FB clock uses this to calculate timestamps (see fbclock_binuptime),
and to advance th_offset_count itself during updates (tc_windup), paying attention to wrapping.
The FF clock uses exactly the same tc_delta(th) call to advance tick_ffcount
 *ffcount = ffth->tick_ffcount + tc_delta(th)  // add delta to tick-start
but with no wrapping concerns.   Raw FFcounter timestamping is performed by
ffclock_read_counter(*ffcount),  however this also uses tc_delta(th) to advance tick_ffcount.
Thus, counter reading for both FB and FF is based on the original tc wrapping
management provided by tc_delta working with timehands.

Note that, even if th becomes stale, timestamps will still be valid as tc_delta uses
the same generation as (th_offset_count, tick_ffcount), and also the same
choice of actual timecounter!, which is also recorded in th.
Furthermore (assuming a constant tc), working with old ticks does not even imply
poorer timestamps. If the FF data has not updated, the result will be the same as
it is the same interpolation, just projected from an older starting time.
If the data has been updated, the parameters will be less than  tc_tick  out of date typically.



==============================================================
Timecounter (tc) Mechanism:  Changing Timecounters
======================================

The hardware tc abstraction is defined in timetc.h, where a tc is defined as a
binary counter which:
  	 i) runs at a fixed, known frequency   [ ie is not scaled, divided, or power state influenced ]
	ii) has sufficient bits to not roll over in less than around max(2 msec, 2/HZ seconds)

typedef u_int timecounter_get_t(struct timecounter *); 	// counter reading function prototype
typedef void  timecounter_pps_t(struct timecounter *);	// PPS 	  reading function prototype

struct timecounter {
	timecounter_get_t  *tc_get_timecount;	// read the physical counter, no masking
	timecounter_pps_t  *tc_poll_pps;			// poll for PPS events, typically not needed
	u_int       tc_counter_mask;				// mask to remove unimplemented bits
	uint64_t    tc_frequency;					// nominal frequency (hz) determined once only
	char       *tc_name;							// name  [ used for identification ]
	int         tc_quality;						// apriori counter quality ranking [-ve =only on request]
	u_int       tc_flags;						// takes values in :
#define TC_FLAGS_C2STOP         1       // Timer dies in C2+
#define TC_FLAGS_SUSPEND_SAFE   2       // Timer functional across suspend/resume
	void       *tc_priv;							// pointer to private parts
	struct timecounter     *tc_next;			// next timecounter
};

Since the return type of timecounter_get_t is u_int, the implementation
reduce a physical counter to a maximum of 32 effective bits, even if (like the TSC)
it is natively longer. Correspondingly, the mask is only a u_int and (targetting
counters with fewer than 32 bits) and tc_delta returns a u_int.

The global variable
struct timecounter *timecounter = &dummy_timecounter; 	// points to tc in use
records the current choice of tc.  However changes in timecounter are picked up in
 tc_windup, so that timestamping can be performed consistently within each tick,
possible since  timehands->th_counter  records which tc should be used for a given tick.

A change in tc is a rare and dramatic event, and breaks the FF algo model,
requiring a full reset.

	Kernel side:
This is managed within tc_windup by ffclock_change_tc(), overwritting the
ffclock_windup already performed. It sets the FF data to trivial values, but
some of the fftimehands data is not right, also, it advances the FF tick, even
though this was already done!

	Daemon side:
The current tc is accessible via
sysctlbyname("kern.timecounter.hardware", &hw_counter[0], ..)   // see if tc there or changed
and is recorded in the global  clock.hw_counter[32] , initialized in radclock_init_vcounter.
It is acted on in pthread_rawdata.c:process_rawdata  just before the pushing of
a fresh parameter update to the kernel. If a change is seen, the update is skipped
(hence ffclock_windup had earlier just advanced a standard tick)
and the algo reinitialized (however it is not clear if this is done properly).


/* TODO
	- fix the tc change kernel actions, the extra fftick may have been deliberate, but is ugly
	- put in a better why to catch a tc and restart the daemon - it is a rare event, just do it
	- check that algo re-init is correct and complete if not doing a full restart
*/



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

Because of the risk of asynchronous updates, generation check is used when clock data is accessed
  from fftimehands:   use fftimehands->gen   (even if want FFdata, is finer grained)
  from FFdata:			 use FFdata->update_ffcount
  
  
The actual functions are:

/*   Purpose               function                              					defined in  */
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

	ffclock_{bin,nano,micro}time(∆T, ∆t)   	// system clock functions
		ffclock_difftime(∆T, ∆t, eb)				// semi-generic FF difference clock
			ffclock_convert_diff(∆T, ∆t)			// low level difference clock
				ffclock_convert_delta(∆T, p, ∆t) // simple multiplication

The ∆t ≥ 0 requirement is needed at each stage. In that sense these calls are not a
true generic difference clock. Constrast this to the RADclock library calls
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
	- calls ffclock_difftime(∆T, ∆t, NULL)		// no error bounded activated !
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
   - universal function supporting all possible FF clock variants based on flags
		- FAST:  	use  ffclock_last_tick, else ffclock_convert_abs(ffclock_read_counter)
		- LERP:  	pass flags through for  ffclock_{last_tick,convert_abs}  to handle
		- LEAPSEC:  add in effect of leaps since boot into basic clock to form UTC
		- UPTIME:   subtract ffclock_boottime  to make times relative to boot
   - note LEAPSEC and UPTIME should not be used together
	- protection:   FFdata->update_ffcount  for update_ffcount retrieval only
   - add error information if requested by non-null eb [no gen-checking unlike ffclock_difftime]
	- native Ca(t) == ffclock_convert_abs(ffclock_read_counter(), t,0) // no leap, just FFclock params
	- UTC     Ca(t) == ffclock_abstime(NULL, t, NULL, FFCLOCK_LEAPSEC ) // native with leap added

ffclock_[get]{bin,nano,micro}[up]time(t)
	- flags established based on function name and intention:
		- ALLways include LERP (top level functions, all need monotonicity)
	   - if get in name include FAST,   else not
		- if up  in name include UPTIME, else  LEAPSEC
	- calls ffclock_abstime(NULL, t, NULL, flags)		// no error bounded activated !
	- converts to {native binary,timespec,timeval} as before
	- thus None of these functions allow the native Ca clock to be read


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
   {setting,getting} the global FFclock state variable  ffclock_estimate
	Access to  ffclock_estimate  is protected by ffclock_mtx, preventing potential multiple
	user processes from any inconsistent actions or consequences.
	Setting (updating) of the FF data is communicated to the ffclock_winup controlling
	fftimehands updating via  the global  ffclock_updated++ (typically from 0 to 1).

Dont forget that the syscall code runs in kernel context, and so has access to any mutexes
defined there.


==============================================================
SYSCTL:
======================================

The SYSCTL tree associated with FF components is defined in kern_ffclock.c .
The provided controls are for the management of the Universal sysclock in general
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

Those sysctl calls used by the daemon are accessed via sysctlbyname() and are
summarised in a section of RADclockDoc_library.  Only three calls are used:
sysctlbyname("kern.sysclock.ffclock.version",&version,&size_ctl,NULL,0)) // detect,store, report KV
sysctlbyname("kern.timecounter.hardware", &hw_counter[0], ..)   		  // see if tc there or changed
sysctlbyname("kern.sysclock.ffclock.ffcounter_bypass",&passthrough_counter,..) // incomplete

For completeness, the tree components specific to the rest of the timecounter interface are
_kern
  - boottime  	"System boottime"
  -> _kern_timecounter
	   - stepwarnings		  "Log time steps"
	   - hardware   [PROC] "Timecounter hardware selected"
	   - choice		 [PROC] "Timecounter hardware detected"
	   - alloweddeviation [PROC] "Allowed time interval deviation in percents"  new in 11.2
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
		-> _net_bpf_stats					"bpf statistics portal"
		-> _net_bpf_tscfg [static] 	"Per-interface timestamp configuration"
	       bpf_tscfg_sysctl_handler	[PROC]	"Per-interface system wide default timestamp configuration"
			   children: bpf_if.tscfgoid 			"Interface BPF timestamp configuration"
		


/* TODO:
		- resolve bypass/passthrough confusion and complete code on kernel side
*/


==============================================================
Universal (FF/FB) System Clock Support
======================================

The traditional system clock support, and timehands architecture, is FB oriented.
With FF implemented, the system now has a choice of paradigm.
The `FF code` includes an initial attempt to adopt a more general view, whereby
both FF and FB choices are always available (ie present, compiled, initialized
and ready for use), and which is actually used to drive the system clock is (dynamically)
selectable.
Of course, each clock requires its own active daemon, otherwise the corresponding kernel
clock will be in an unsynchronized state.

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
 sysclock_snap2bintime(*cs, *bt, whichclock, flags)
		- from a captured state, read the specified flavour (UPTIME etc) of the specified clock (FF/FB)

Since _getsnapshot reads the counters, it MUst handle the fast flavour
(ie dont read the counter, just use the stored tick value) which _snap2bintime then
inherits.  All other flavours, such as LERP and UPTIME, can be performed by _snap2bintime on
demand in post-processing, building them into the returned bintime timestamp.

Splitting raw and final timestamping enables the following capabilities:
	- optimal timestamp location:  raw timestamps can be taken at the best place,
	  without having to know the needed timestamp flavour, in advance and without the
	  overhead of final timestamping(s)
   - no accuracy penalty due to the latency between the split, as all state is captured
	- flexibility: can convert according to (LERP,UPTIME,LEAPSEC,raw) requirements
	- efficiency:  only capture state once to support multiple timestamps, each with their own
		   (LERP,UPTIME,LEAPSEC,fast,active sysclock,raw)  preferences.
   - extends comparisons from earlier research to include the LERP version of the FF clock
	- access to the FFcounter values gives maximum flexibility, not all state necessarily needed
   - can use to form a snapshot database enabling flexible post-processing.
	  This would per-tick however, which is very expensive compared to the underlying FF daemon
	  parameter updates.
	 
Including both FB and FF simultaneously in the split enables:
	- the flexibility of clock choice when timestamping:  FF, FB, active sysclock,
	  in addition to other flavour choices
	- enables fair selection of FF/FB clocks, as the raw timestamps are taken
	  together as `tick-safe` pairs.  In particular this supports a perfectly fair comparison
	  methodology of rival FF/FB deamons, based on truly back-to-back timestamping.
and is efficient in that to read the FF counter, the underlying FB counter must be read anyway.
There is however some overhead in always storing both states per-tick.


The snapshot is based on the current tick data, and so the FF parameters will be out of date by a
fraction of a tick for ticks where parameter updates occur. If needed, this can be
overcome in userland by just using the captured FFcounter values, in conjunction with
the radclock API, or even better, if the daemon is storing its own database
and this is available, then the exactly corrected parameters can be obtained independently of
processing delays.

Although the above benefits are generic, they are particularly suited for use within
packet timestamping in the case where multiple processes may be filtering for the same
packets, but have different requirements.  The benefits are:
  - the time consuming raw timestamping is done once only  [or not all if using FAST]
  - all tapping processes get the same raw data reqardless of how they are scheduled,
    and derived timestamps are immune to any latency between raw and final timestamping
  - tapping processes have full freedom of timestamp flavour, with the exception of FAST
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
	int			status;					// from external time_status
};
struct ffclock_info {
	struct bintime		error;			// estimate error at this counter read
	struct bintime		tick_time;		// tick-start time according to FF
	struct bintime		tick_time_lerp;// tick-start time according to FF (mono version)
	uint64_t		period;					// counter period	(from current fftimehands)
	uint64_t		period_lerp;			// counter period (mono version)
	int			leapsec_adjustment;	// total leap adjustment at this counter value
	int			status;					// current status
};
contain all those fields from timehands and fftimehands respectively that are essential
for clock reading within a tick at a fixed counter value.

The 4-byte flags used in flag processing in sysclock_snap2bintime are:
#define	FBCLOCK_{FAST,UPTIME,MASK}					  // lower 2 bytes, MASK = 0x0000ffff
#define	FFCLOCK_{FAST,UPTIME,MASK,LERP,LEAPSEC}  // upper 2 bytes, MASK = 0xffff0000


As noted earlier, if FFCLOCK is not defined the FF components of the sysclock framework
are removed, but the framework remains, with correspondingly reduced functionality.
In that case sysclock_active = SYSCLOCK_FBCK , and cannot be altered as sysctl_kern_sysclock_active
does not even exist.


/*
TODO:
  - _state, or _tickstate would be better than _info
  - changed the th_offset name to tick_time to match ff language, so should do the same for
    th_scale -> period, then all info (should call it state) fields match, except ff has lerp versions 
	 as well + leapsec_adjustment
  - align the constants SYSCLOCK_{FBCK,FFWD}  with the sysclocks strings more formally
  - check to see if a generic timestamp call can be used to avoid repeating code across
    here and  tc_windup
  - fix BUG in fb_info error calc (see notes)
  - consider lighter versions of this where only timestamps are provided?  or error fields skipped?
*/




==============================================================
bpf and pcap
======================================

Modifications are needed to the BPF system, as it provides the timestamping of
any packets matching bpf filters, set by processes obtaining packets via pcap.

No actual changes are required to pcap, as the locations where timestamps are placed
are legal under KV≥2, no more hidding raw timestamps in padding!
Further, the meaning of the timestamp control flags is set in the kernel and understood by the
daemon, pcap only passes information along between them.

The key structure at the attached hardware interface (if) level is (bpf.h) :
struct bpf_if {
	LIST_ENTRY(bpf_if)	bif_next;	/* list of all interfaces */
	LIST_HEAD(, bpf_d)	bif_dlist;	/* descriptor list */
	u_int bif_dlt;							/* link layer type */
	u_int bif_hdrlen;						/* length of link header */
	struct ifnet *bif_ifp;				/* corresponding interface */
	struct mtx	bif_mtx;					/* mutex for interface */
	struct sysctl_oid *tscfgoid;		// new: timestamp sysctl oid for interface
	int tstype;								// new: if-level timestamp attributes
};
where tstype will take values from the _TSTAMP_ flags
	BPF_TSTAMP_{DEFAULT,NONE,FAST,NORMAL,EXTERNAL}		// DEFAULT is new
which control timestamp options relevant for the if-level.

For a single interface there can be many descriptors correponding to different processes
wishing to monitor or receive packets. The key structure at this (d) level (sys/net/bpfdesc.h) is
struct bpf_d {
// needed in bpf_tap
   LIST_ENTRY(bpf_d) bd_next;      /* Linked list of descriptors */
   u_int64_t       bd_rcount;      /* number of packets received */
   u_int64_t       bd_fcount;      /* number of packets that matched filter */
	int           	 bd_tstamp;      /*** select time stamping function ***/
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

and timestamp control is mediated though the _T_ flags :

// FORMAT flags
#define	BPF_T_{MICRO,NANO,BIN}TIME 0x000{0,1,2}  // standard formats
#define	BPF_T_NONE			0x0003
#define	BPF_T_FFCOUNTER	0x0004   // [ new ]  I want raw FF timestamp
#define	BPF_T_FORMAT_MAX	0x0004	  // largest format value
#define	BPF_T_FORMAT_MASK	0x0007   // 0000 0000 0000 0111
#define	BPF_T_NORMAL		0x0000   // = current SYSCLOCK, UTC with MICRO
// MONO flag  [ old ]
#define	BPF_T_MONOTONIC	0x0100	  // if want uptime clock [ translates to {FB,FF}CLOCK_UPTIME ]
#define	BPF_T_FLAG_MASK	0x0100   // 0000 0001 0000 0000
// CLOCK flags (for Universal sysclock, new)
#define	BPF_T_SYSCLOCK		0x0000	  // read current sysclock
#define	BPF_T_FBCLOCK		0x1000   // read FB
#define	BPF_T_FFCLOCK		0x2000   // read FF
#define	BPF_T_CLOCK_MAX	0x2000   // largest clock value
#define	BPF_T_CLOCK_MASK	0x3000	  // 0011 0000 0000 0000

// Extract FORMAT, MONO, CLOCK  flag bits
#define	BPF_T_FORMAT(t)	((t) & BPF_T_FORMAT_MASK)
#define	BPF_T_FLAG(t)		((t) & BPF_T_FLAG_MASK)
#define	BPF_T_CLOCK(t)		((t) & BPF_T_CLOCK_MASK)
// Explicitly named combinations of  (FORMAT,FLAG)  		 [ old ]
// explicitly named combinations of  (FORMAT,FLAG,CLOCK)  [ new ]
// (FORMAT,FLAG,CLOCK) combination valid if
#define	BPF_T_VALID(t) ...
 Meaning:  if FORMAT==NONE or FFCLOCK or
             (FORMAT standard) and (CLOCK is valid) and (no bits in illegal posns)
 

The top level function bpf_tap (bpf_mtap{2} are analogous) processes the
if-level using the _TSTAMP_ flags, then loops over all attached descriptors {d}.
For each d, if the pkt passes d''s filter, the _T_ level flags (except FORMAT)
are processing in order to take the appropriate timestamp(s), which are then passed
as arguments bt and ffcount to catchpacket().

For efficiency and flexbility reasons, the timestamping is performed using the
raw/final split described above. Thus sysclock_getsnapshot is called before the d-loop
to obtain the state once-only needed for all possible timestamp requests, whereas
sysclock_snap2bintime is called separately for each d.
Note that the BPF_TSTAMP_FAST flag is if-level and so is passed to
sysclock_getsnapshot, because if even a single descriptor is not fast, then
the counter must be read and its cost incurred for all.

The input to timestamp generation is d->bd_stamp, which carries the desired
BPF_T_{FORMAT,MONO,CLOCK} flags given to pcap from the calling program.
In the radclock daemon,  d->bd_stamp  is get/set in pcap-fbsd.c:descriptor_{get,set}_tsmode
using   ioctl(pcap_fileno(p_handle), BIOC{G,S}TSTAMP, (caddr_t)(&bd_tstamp))

The role of catchpacket is to store the packet and timestamps into a buffer structure for that d.
It first completes the flag processing by performing the FORMAT conversion specified in
d->bd_stamp, using the function
	bpf_bintime2ts(struct bintime *bt, struct bpf_ts *ts, int tstype)  ,
which returns the bt-inspired timestamp structure

struct bpf_ts {
	bpf_int64	bt_sec;		/* seconds */
	bpf_u_int64	bt_frac;		// big enough to hold frac component of timeval, timespec, bintime
};

The final timetamps are then placed in the eXtended bpf_xhdr structure (bpf.h) :
struct bpf_xhdr {
	struct bpf_ts	bh_tstamp;	// timestamp was timeval in obsolete bpf_hdr and bpf_hdr32
	ffcounter	bh_ffcounter;	// raw FFcounter timestamp  [new, in bpf_hdr{32} also]
	bpf_u_int32	bh_caplen;		/* length of captured portion */
	bpf_u_int32	bh_datalen;		/* original length of packet */
	u_short		bh_hdrlen;		/* length of bpf header (this struct plus alignment padding) */
};


The existence of an ffcounter member of bpf_xhdr (and bpf_hdr{32}) implies that this is KV=3 .
However, the current code is implementing the intent of KV=2 and more work is needed !
Some of the main issues are:
	- if BPF_T_FFCOUNTER,  bt is filled with ffcount and copied to header as such // KV=2 behaviour
	- if the FORMAT flags are taken to be mutually exclusive, then it is not possible
	  to both ask for a raw timestamp and specify the format of the bh_tstamp timestamp.
	- BPF_T_VALID is incompatible with the KV=3 goals
	- bpf.c should deal with the CLOCK flag but this is ignored
   - issue on daemon side:  the bpf_xhdr is used when d->bd_stamp = BPF_T_FFCOUNTER
	  (set when mode = RADCLOCK_TSMODE_RADCLOCK in descriptor_set_tsmode)
	  but struct bpf_hdr_hack_v3  used in  pcap-fbsd.c is based on bpf_hdr .
	  Is this because pcap binaries didnt understand bpf_xhdr back then?  should now.
	  Is this why the daemon sets  bd_tstamp = BPF_T_MICROTIME, to ensure the bpf_hdr branch in
	  catchpackets is followed ?
	- sysclock_getsnapshot  deals with UPTIME issues (via BPF_T_MONOTONIC) neatly in bpf_tap,
	  but in catchpackets  bpf_bintime2ts  also takes care of this [ makes sense if !FFCLOCK],
	  but if not, fails.  Could fix by, in FFLOCK case, calling bpf_bintime2ts
	  with   tstype & !BPF_T_MONOTONIC   so it wont act on the flag .
   - another reason why the processing in bpf_bintime2ts is somewhat superfluous:
	  It would be efficient if sysclock_getsnapshot handled FORMAT as well, particularly since
	  in the FAST and MONO case for FB, the FAST version of all formats are precalulated in the
	  timehands.
	    If the key goal of catchpacket is mainly storage, why not do all conversion in _tap, but
		 need to deal with compatibility issues with old header, which impacts storage issues...
	  
			


On the daemon side, the timestamp mode is set as :
pcap-fbsd.c:descriptor_set_tsmode
  RADCLOCK_TSMODE_{SYSCLOCK,FAIRCOMPARE}   bd_tstamp = BPF_T_MICROTIME
  RADCLOCK_TSMODE_RADCLOCK						 bd_tstamp = BPF_T_FFCOUNTER
  ioctl(pcap_fileno(p_handle), BIOCSTSTAMP, (caddr_t)&bd_tstamp) == -1)

Actually want something like
	RADCLOCK_TSMODE_DEAMON			bd_tstamp = BPF_T_FFCOUNTER | BPF_T_FBCLOCK | BPF_T_NANOTIME
	  - daemon wants the FFcounter, and the FB as well to enable external comparisons
	  - timeval is old, timespec is better
	RADCLOCK_TSMODE_READFF			bd_tstamp = BPF_T_FFCOUNTER | BPF_T_FFCLOCK | BPF_T_NANOTIME
	  - libprocess may want the raw timestamp, or the convenience of an immediate conversion to Ca(t)
	  - how to get FFCLOCK_{LEAPSEC,LEAP} preferences into  BPF_T_ language
	    At least bpf.c does include timeff.c  which includes these macros
		 - currently wouldnt want LERP, but would want LEAP
		 - could expand BPF_T_FFCLOCK  with BPF_T_FFLERPCLOCK, natural, it is a kind of clock
	    - leap is somewhat wierd, the native Ca is leap free for the algo, but what is this?
		   it is neither UTC, nor uptime, the kernel shouldnt know about this..  it is really
		   a daemon concept, but should just return with LEAP by default.
			Libprocesses can use the API if they want native Ca.

/*
TODO:
  - fix above bugs and issues with tsmodes and resulting actions
*/

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
All required leap second information is provided by the daemon, via the relevant
members passed into ffclock_estimate:
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

										


==============================================================
Other stuff
======================================




##########################
places to check  V4kernel code to RADclock connections:

Qn:  does this kernel support an old vcount hiding mode?

Plan of attack:

- Look into pcap aspects
   - currently there
	- in those final Julien patches he `hid` from me

- Find proper references to learn about some of the satellite timing elements to avoid confusion/ignorance/trying to do it all the hard way:
 	- hardware clock
	- time of day stuff
	- other timing facilities?
	- separating POSIX and ISO C compliance from other stuff, summarize
	- timecounter documentation!!  man pages at least
	- start with ALL references to `time` in UNIX book

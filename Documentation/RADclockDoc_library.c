/*
* Copyright (C) 2016-2020, Darryl Veitch <darryl.veitch@uts.edu.au>
*/

RADclock documentation.
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
 FFcounter = a FF compatible counter
 algo = algorithm, the RADclock clock synchronization algorithm


==============================================================
Summary
=======

This documents the RADclock library functionality and the inter-process communication
aspects of RADclock.  It includes the RADclock API which allows the full Absolute and
Difference clock features to be accessed by other applications,
the kernel-RADclock interactions, interaction with modified pcap for obtaining
raw kernel timestamps, and discusses the OS and kernel-version dependencies.
The source files are in RADclockrepo/radclock/libradclock
	radapi-base.c
	radapi-getset.c
	radapi-time.c
	radapi-pcap.c
	{kclock,ffkernel,pcap}-{fbsd,linux,none}.c
	logger.c
The main associated header files are
	radclock-private.h
	radclock.h							// external processes include this to use RADclock
	kclock.h

Examples of use are given in RADclockrepo/examples
	radclock_get_time.c				// using raw timestamping in userland
	radclock_capture_packet.c		// using raw timestamps delivered via modified pcap
	radclock_pps.c

In addition there is related code in RADclockrepo/radclock/radclock/ in
	radclock_main.c
	pthread_mgr.c
	pthread_dataproc.c
	virtual_machine.c
and
	pthread_mgr.h


==============================================================
Introduction
============

As a system running live, RADclock is comprised not only of the user threads
under the radclock process which underpin the algo operation, but also kernel
components, and the mechanisms for external processes to access and read a (RAD)clock.
In this document, to reduce overloading of the words `radclock` and `RADclock`,
we refer to this overall system by `RADclock`.  The radclock threaded code from
RADclockrepo/radclock/radclock, is referred to as the daemon code or simply the daemon.

By the RADclock library we mean the code in RADclockrepo/radclock/libradclock.
This library attempts to isolate the elements necessary to use RADclock.
To use RADclock an external `libprocess` must include radclock.h, the OS kernel
must have necessary RADclock or FFclock aware components, and the daemon must be running.

RADclock requires several kinds of IPC capabilities :
	FFcounter reading  [ typically from kernel, but can be direct if TSC (passthrough mode) ]
		- daemon:			needed by TRIGGER, NTP_SERV, MAIN (spy timestamping), and verbose()
		- libprocesses:   to timestamp arbitrary events
   Access to radclock parameters [owned by the daemon process]
	   - libprocesses:   		via a SHM segment (written to by daemon, read by libprocess)
		- kernel FFclock data:	pushed to kernel by daemon
   Access to kernel FFclock data
		- daemon:			pulled to initialize radclock parameters (clock_init_live, KV>1)
	   - libprocesses:   to get radclock parameters when SHM segment not available
   Access to raw packet timestamps make in kernel, via RAD/FFclock modified pcap
		- daemon:			to get raw timestamps and pkt data of NTP pkts for stamp creation
		- libprocesses:	to get raw timestamps and pkt data of monitored pkts
   Access to system clock control parameters, via the sysctl tree
	   - daemon:			to learn of KV value, set and learn of counter read options like bypass
		- kernel:			defines and mediates the relevant setting via the sysctl tree
	Standard system clock disciplining  [ optionally if conf->adjust_FBclock ]
		- daemon:			use system ntp_adjtime and settimeofday to slave system clock

Note that the RADclock adjust_FBclock option uses a traditional system clock
interface to perform traditional userland based discipling using RADclock,
in other words to allow the radclockd daemon to replace ntpd in that sense.
It should not be confused with a full FF based system clock, which if available
is not part of RADclock as such.  Such a clock however does rely on kernel FFclock
data updating, a task which the RADclock daemon fufills.


The following structure acts as a central `IPC hub` connecting the first five of the above needs.
It is defined within radclock_private.h  as it is the main structure passed to library
functions, but is publically opaque as it is not in radclock.h. Such `clock`
variables are created (and trivially initialized) using radapi-base.c:radclock_create .

For the daemon, the radclock_handle structure includes a clock variable :
	clock_handle->clock   		// struct radclock *clock
which is initialized within clock_init_live.
Libprocesses must define their own clock variable, initialized by library function
radclock_init.  For convenience in what follows in argument lists we often
write clock instead of struct radclock *clock .

struct radclock {			// variables of this type called `clock`

	/* Direct access pointers to kernel FFclock data */
	// Actual members set in kclock-{fbsd,linux}.c:init_kernel_clock
	union {
		struct radclock_impl_bsd 		bsd_data;		// only needed for KV=0,1
		struct radclock_impl_linux 	linux_data;
	};
	//  Convenient related OS-independent access macros
	 #ifdef linux
	 # define PRIV_DATA(x) (&(x->linux_data))
	 #else
	 # define PRIV_DATA(x) (&(x->bsd_data))
	 #endif


	/* IPC shared memory segment information for daemon<-->libprocess data sharing */
	int ipc_shm_id;				// unique key for segment
	void *ipc_shm;					// pointer to segment

	/* String identifying current hardware counter */
	// Currently used in FBSD only, set in radclock_init_vcounter using sysctlbyname
	char hw_counter[32];			// used to see if counter is a TSC or has changed
	
	/* KV: the version of the {FF/RAD}clock kernel modifications */
	// Interpretation of KV is OS dependent:   ( set to -1 if no support )
	//			FBSD:    0,1,2,3
	//			linux:	0,1,
	int kernel_version;			// records output of found_ffwd_kernel_version()

	/* Controls if algo's local counter period used when COMPuting time */
	// Used in in radapi-time.c: ffcounter_to_{abstime,difftime}_{shm,kernel} only.
	// Expert feature: average user can ignore, will get default=ON set in radclock_create
	// Can be altered using radapi-getset.c:radclock_{get,set}_local_period_mode
	radclock_local_period_t	 local_period_mode;	// values are RADCLOCK_LOCAL_PERIOD_{ON,OFF}

	/* pcap parameters for the packet stream being monitored */
	pcap_t *pcap_handle;			// daemon: NTP request/response;  libprocesses: whatever
	// Specifies the kind of timestamp you want from pcap data (details KV dependent)
	int tsmode;
	 
	 
	// syscall-based pushing of radclock parameters to kernel FFclock data for KV=0,1
	//  - currently never initialized (except to zero) !!  and never used
	//  - instead fixedpoint code performs kernel update using direct access pointers above
	int syscall_set_ffclock;	// appears in kclock-fbsd.c:set_kernel_ffclock, never used
	 
	
	// Read vcounter syscall direct link
	// Set in ffkernel-{fbsd,linux}.c:radclock_init_vcounter_syscall
	int syscall_get_vcounter;	//  FBSD: for KV>1, instead use syscall ffclock_getcounter()

	/* Generic function for reading of counter */
	// Set in radclock_init_vcounter:
	//				selecting rdtsc passthrough if available	(radclock_get_vcounter_rdtsc)
	//          else just linking to existing syscall		(radclock_get_vcounter_syscall)
	// In library radclock_get_vcounter() defined to access this opaquely
	int (*get_vcounter) (struct radclock *clock, vcounter_t *vcount);
};


In the sections below these members of clock will be revisited.

Although most public radclock.h declarations are intended for library function use,
some are also required by the daemon code directly.  A complete list is:

All						typedef uint64_t vcounter_t;		// included everywhere
main,sync_bidir, ..	STARAD_{UNSYNC,WARMUP,KCLOCK,.. }// RADclock status words
radclock_main.c:  	radclock_init_vcounter_syscall(clock);
							radclock_init_vcounter(clock);
several places			radclock_get_vcounter   			// generic counter read fn
stampinput-livepcap.c: radclock_{set,get}_tsmode		// ensure compatible mode to get vcount


The library has a logging system where messages are logged using logger.c:logger(),
which exploits the daemon''s verbosity system.  No special logfiles are required.
Details are provided in RADclockDoc_verbose.


==============================================================
OS dependence
======================================

What follows will ignore aspects related to the VM thread, which has particularly
complex OS dependence, though what is covered here is the foundation of it.

The main OS code dependency is controlled using the guard macros
# WITH_FFKERNEL_{LINUX,FBSD,NONE} 
only one of which is defined in the compile configuration.
This is achieved by including the relevant lines:
#undef/def WITH_FFKERNEL_FBSD
#undef/def WITH_FFKERNEL_LINUX
#undef/def WITH_FFKERNEL_NONE
in configure.in (the configuration input file for autoconf, automake and makeheader).

The macros protect all code in the respective library files
{ffkernel,kclock,pcap}-{fbsd,linux,none}.c
which carry the great bulk of OS dependency, and ideally would carry all.
Typically each of these files contain top level functions of the same name but
OS dependent implementations, and potentially other static OS-specific supporting functions.

The purpose of WITH_FFKERNEL_NONE is to allow dummy functions to be defined to ensure
compilation when kernel support doesnt exist. These should never be executed since
	clock->kernel_version = found_ffwd_kernel_version()
would return -1 (no support) when called in:
	daemon:			clock_init_live
	libprocess:		radclock_init
resulting in an early failure of live capture.

The macros would ideally only appear within the above library files.
One set of exceptions appear in pthread_dataproc.c, where
WITH_FFKERNEL_NONE protects dummy definitions of IPC functions, and
WITH_FFKERNEL_{LINUX,FBSD} maps NTP_ADJTIME to be the right function name.
In addition WITH_FFKERNEL_FBSD protects code that only exists under FBSD:
		pthread_dataproc.c: process_stamp   	deal with possible change of hw_counter


More generically controlled OS dependence is very rare:
  #ifdef linux						// once in radclock_private.h to define PRIV_DATA shortcut
  #if defined (__FreeBSD__)	// twice in kclock.h so bintime, struct ffclock_estimate defined



==============================================================
Overview of KV dependence (and history)
======================================

The (non static) functions in {ffkernel,kclock,pcap}-{fbsd,linux}.c  are all passed
a clock parameter, and handle KV dependence by switching on clock->kernel_version .
The supported version are
		 FBSD:   0,1,2,3
		LINUX:	0,1,2			// only the skeleton of V2 support in library, not in kernel

The capabilities and rationale for the KV naming across FreeBSD and LINUX roughly
mirror each other, however as there are many differences and details, we first describe
the more developed FBSD case, then quickly summarize that of LINUX.
For each, library functions hide the KV dependence as much as possible.


=====
FBSD
=====

KV = 0		This is derived from the original research-only oriented code.

The main features of KV = 0 are:
  - TSC only, no generic counter choice  (originally TSCclock, not RADclock)
  - no explicit in-kernel versioning
  - kernel data a full and simple copy of RADclock parameters - no changes or
    translations, clock reading in kernel worked using floating point for some reasons
  - most kernel modifications in bpf.c
	  - bpf handle required for access of parameters :  now stored in clock->bsd_data
	    This was the `globaldata` as it was how libprocesses could access radclock
	  - ioctl access to kernel data and get/set operations
  - pcap hack to hide raw timestamps in pcap header padding
	  - different `tsmodes` allowing choice of where/how timestamps were taken
  - thread FIXEDPOINT required to perform periodic extrapolated update of kernel
	 data (independent of stamp arrival rate) in case counter wraps, assuming
	 no floating point capability in the kernel.


The modified pcap code implemented a timestamp-mode (tsmode) attribute, with possible values,
in the language of the modern code, captured by the following type (defined in radclock.h) :
enum pktcap_tsmode {
	PKTCAP_TSMODE_NOMODE = 0,			// no FF support in pcap
	PKTCAP_TSMODE_SYSCLOCK = 1,		// leave normal pcap ts field, hide vcount in padded hdr
	PKTCAP_TSMODE_FFNATIVECLOCK = 2,		// fill ts with RADclock Abs clock, hide vcount in hdr
	PKTCAP_TSMODE_FAIRCOMPARE = 3  // as _SYSCLOCK, but ts and raw timestamps back-to-back
};
typedef enum pktcap_tsmode pktcap_tsmode_t ;

The point of _RADCLOCK was to make it more convenient for applications to use
RADclock by having the absolute timestamp pre-calculated in a standard field and
format (timeval). In situ calculation also ensured that the RADclock timeval
corresponded exactly to the captured raw vcount timestamp, using the correct
(ie current at that time) RADclock parameter values, assuming the kernel copy is up to date.

The point of _FAIRCOMPARE was facilitate a fair comparison of the system clock and RADclock.


----------------------------------
KV = 1

Being limited to the TSC was a serious restriction as it is not available on all
platforms, and for a time was very unreliable on systems with multiple cores
(1 TSC per core) and power management (changing clock rate).  It was desirable
to be able to exploit other reliable counters that the system might make
available, such as HPET. However, many of these were only 32bits wide.
The idea of a FeedForward counter was created which means wide enough to not
wrap even when of small period (<1ns) (64 bits was and remains sufficient),
and with monotonic advancement impervious to power management.
The existing FBSD timecounter interface, which exposed available counters to
userspace, was expanded to support FF-compatible versions of counters if needed
(expanding 32 bits counters to 64 using periodic updates, initially via the FIXEDPOINT thread).
The FFcounter code was designed to support the needs of FF clocks in general,
not only the RADclock. To support this the basic vcounter type was created,
defined in radclock.h:  typedef uint64_t vcounter_t;

KV=1	Added:
		- explicit versioning and the FFclock label for FFcounter related code
		- FFcounter extension to the timecounter interface allowing any available
		  counter to be FF-extended (if needed), accessed and used via a syscall
			- counter syscall available via  clock->syscall_get_vcounter
		- a passthrough mode allowing the TSC, if available, to be accessed directly
		  via rdtsc-like assembler macros as an option (just like in KV0), rather
		  than going through the slower userspace<->kernelspace  timecounter interface


----------------------------------
KV = 2

The limitations of KV1 included the inflexibility of accessing FFclock pcap data
and setting via bpf handles, and the complexity and overhead of requiring a
FIXEDPOINT thread. The next version was written with greater abstraction and
the library and daemon code was generalised to match this, with KV=0,1 support
retained for backward compatibility.

KV=2	Removed:
		- need for FIXEDPOINT thread
		- bpf based FFclock access for pcap (hence clock->bsd_data no longer used)
		- ioctl based access (regular communication now all via syscalls)
		- faircompare support:  _SYSCLOCK and _FAIRCOMPARE now treated in the same way,
		  getting the ts field from BPF_T_MICROTIME .
		  Later FAIRCOMPARE entirely as it was never acted on, not for any KV !
		  
		
		Added:
			- new semantics for vcount access (which breaks the description under KV=0 above):
			 the standard ts field is the only timestamp present, and is a timeval
			 or a vcount according to _SYSCLOCK or _RADCLOCK respectively.
			 Hence the daemon Must now use _RADCLOCK in order to access raw timestamps.
			
	      - syscalls for all FF functions:
syscall (kernel, kern_ffclock.c)  linked userspace fn		OS-dep wrapper {ffclock,kclock}-{fbsd,,}.c
---------------------------------------------------------------------------------------------------
sys_ffclock_getcounter  		ffclock_getcounter(vcount) radclock_get_vcounter_syscall(clock,vc)
sys_ffclock_setestimate			ffclock_setestimate(cest)	set_kernel_ffclock(clock, cest)
sys_ffclock_getestimate			ffclock_getestimate(cest)	get_kernel_ffclock(clock, cest)


Faircompare was removed because:
i)  this was a research feature not appropriate to the evolution of the kernel
    support towards a generic FF clock, and its use by an actual FF-based system clock.
ii) improvements to the kernel support meant that the timestamping was relatively
    optimised in all cases anyway and very fair.


----------------------------------
KV = 3

The breaking of the TSMODE semantics in KV2 was limiting and added complexity.
To properly support FF clocks in the kernel the central importance of the raw
timestamp should be make explicit. This led to KV3.

KV=3	Added:
	  - a new vcounter_t vcount member to the bpf header structure within bpf
	    (not just a hacked header structure used by RADclock)
	  - the interaction with the universal sysclock infrastructure and TSMODE needs some
	    sorting out.

For KV={2,3}, the definition is really the change in the bpf _T_ timestamp type
language, and the changes in functionality they then imply/impose.


----------------------------------
# HAVE_SYS_TIMEFFC_H		// defined in config.h generated by configure based on config.h.in
For KV>1 some of the KV dependence in the library is managed using this symbol.
This revolves around the availability of the FFclock header file sys/timeffc.h,
which is known through testing existence of the automatically
generated (by autoheader(configure.in)) associated kernel flag HAVE_SYS_TIMEFFC_H .
This header, and hence flag, is only defined when KV>1 .
The flag is used to :
					 		kclock.h:  ensure struct ffclock_estimate defined to allow compilation
{ffkernel,kclock,pcap}-fbsd.c:  include timeff.h if available
					 kclock-fbsd.c:  provide dummies if KV<2 for {set,get}_kernel_ffclock
				  ffkernel-fbsd.c:  provide dummy   if KV<2 for ffclock_getcounter




=====
LINUX
=====

KV = 0,1
Most of the comments made for FBSD hold in general terms here, except that instead of a bpf descriptor access is obtained through a linux generic netlink id: clock->linux_data

KV = 2
This version does not yet exist, but the groundwork is layed though the OS and KV dependency structure of the code.

Linux has a system for selecting an underlying counter analogous to timecounter, called
clocksource, which we also expand to allow FFclock compatible counters of
underlying physical counters.



==============================================================
IPC:	FFcounter reading
======================================
Main files:		radclock-read.c,	ffkernel-{,,,}.c

The generic counter-reading function (defined in radclock-read.c) is
int radclock_get_vcounter(radclock *clock, vcounter_t *vcount)
{
	return clock->get_vcounter(clock, vcount);
}
This wrapper function is publicly declared in radclock.h and is THe way in
which both the library and deamon code obtains a raw timestamp `vcount`.

The function pointer  clock->get_vcounter  is set in radclock_init_vcounter to
	radclock_get_vcounter_rdtsc		// wrapper for underlying TSC access
if passthrough is desired and available, otherwise
   radclock_get_vcounter_syscall		// wrapper for syscall access to existing counter

The FBSD KV dependence of the syscall access in radclock_get_vcounter_syscall is
	KV=0,1:	via clock->syscall_get_vcounter  // initialized in radclock_init_vcounter_syscall
	KV=2,3:	ffclock_getcounter					// no init, links to syscall: sys_ffclock_getcounter



==============================================================
IPC:	Access to pcap raw packet timestamps make in kernel
======================================
Main files:		radapi-cap.c,	pcap-{,,,}.c

Packets are made available via a call to pcap_loop in :
	daemon:		capture_raw_data             infinite loop via callback: fill_rawdata_pcap
	libprocess:	radclock_get_packet (public) 1 pkt via library callback: kernelclock_routine

Each callback uses the KV-generic extract_vcount_stamp to obtain the vcount value, by
switching on KV and taking into account the chosen tsmode, in order to call the
required KV-specific function.
Under FBSD these are  extract_vcount_stamp_v{1,2,3},  and make use of modified
headers struct bpf_hdr_hack_v{1,2,3} recorded in pcap-fbsd.c  .

The tsmode is set using the KV-generic public functions  radclock_{get,set}_tsmode:
	daemon:		livepcapstamp_init:  if FBSD & KV=2 _RADCLOCK, else _SYSCLOCK
	libprocess:	whatever
which are mainly wrappers for the OS-specific functions  descriptor_{get,set}_tsmode
which switch on KV and perform the actual work.

Only the latter functions contain the mapping between the daemon and FFkernel
enhanced _T_ language of bpf.c describing timestamp type :
  PKTCAP_TSMODE_{,,,}  <--> BPF_T_{,,,}
Thus descriptor_get_tsmode has verbosity detailing the BPF_T_ value obtained from
the kernel, but returns a PKTCAP_TSMODE_ value.



==============================================================
IPC:	Use of SYSCTL for FreeBSD, KV>1
======================================

The SYSCTL tree associated with FF components is summarized in FFclockDoc_FreeBSDv4 as:
_kern  							   ""   // defined above kern_tc.c somewhere
	-> _kern_sysclock	[NODE] 		"System clock related configuration"  // declared in timeffc.h
		- available [PROC]   			"List of available system clocks" [RO]
		- active    [PROC]  				"Name of the active system clock which is currently serving time"
		-> _kern_sysclock_ffclock [NODE]"Feed-forward clock configuration" // declared in timeffc.h
		   - version				[INT]			"Feed-forward clock kernel version" [RO]  // KV
		   - ffcounter_bypass	[INT]	  		"Use reliable hardware timecounter as the feed-forward counter"

as is the underlying timecounter (tc) tree :
_kern
  - boottime  	"System boottime"
  -> _kern_timecounter
	   - stepwarnings		  "Log time steps"
	   - hardware   [PROC] "Timecounter hardware selected"
	   - choice		 [PROC] "Timecounter hardware detected"
	   -> kern_timecounter_tc										
			-> kern_timecounter_tc_"tc_name"?  "timecounter description" 
				- frequency		"timecounter frequency"
				- quality		"goodness of time counter"
				- counter		"current timecounter value"
				- mask			"mask for implemented bits"

Access by the FF clock daemon is via sysctlbyname(), enabled (to be deprecated) by
#include <sys/sysctl.h>   It is used in the following places:

pthread_dataproc.c:process_stamp   and
virtual_machine.c:receive_loop_vm
		sysctlbyname("kern.timecounter.hardware", &hw_counter[0] // detect and react to tc change

ffkernel-fbsd.c:
 found_ffwd_kernel_version:
	sysctlbyname("kern.sysclock.ffclock.version",&version,&size_ctl,NULL,0)) // detect,store, report KV
 has_vm_vcounter:
	sysctlbyname("kern.sysclock.ffclock.ffcounter_bypass",&passthrough_counter,..) // missing in kernel
 radclock_init_vcounter:
	sysctlbyname("kern.sysclock.ffclock.ffcounter_bypass",&passthrough_counter,..)
	sysctlbyname("kern.timecounter.hardware", &hw_counter[0], ..)   // look to see if tc there






==============================================================
IPC:	Standard system clock disciplining
======================================
Main files:		pthread_dataproc.c

The RADclock facility for system clock disciplining from the daemon uses only
standard interfaces, namely the NTP_ADJTIME (actual name is OS dependent) and settimeofday
functions.   It is engaged optionally via conf->adjust_FBclock , with default:
	conf->adjust_FBclock = DEFAULT_ADJUST_FBCLOCK; 		// = BOOL_{OFF,ON}

The disciplining algorithm is contained within
	int update_FBclock(struct radclock_handle *handle)
and is a simple, conservative, heavily damped algorithm based on a 1 minute
calibration cycle of period adjustments involving userland raw timestamping
based bracketing of system clock reads (via read_clocks), which aims to slave the
system clock to RADclock, preserving monotonicity of the former.

This is a simple, non-optimized and non-ideal approach, motivated by the desire to make the system clock be basically well behaved by following RADclock, thereby avoiding the erratic behaviour of ntpd which can complicate experiments operationally (for example if the host houses a DAG card, it can confuse the `which second is which` setting of the card).
A second motivation was to provide, operationally, a means to replace nptd by RADclock without having to reengineer the system clock interface in any way, thereby allowing users to benefit from RADclock without having to do anything, in particular without having to compile in the RADclock library. This functionality was how in fact CAIDA was accessing RADclock.  However it still requires an FFkernel, currently, but not if a userland-only version of the daemon were developed. This will be superseded by kernels offering true FFclock support, though use of the difference clock will still require the API to be use directly in most cases, and app developers to be willing to make the effort.



==============================================================
IPC:	Access to radclock parameters:  Overview
======================================

Each time the RADclock algo (process_bidir_stamp) processes a stamp, the radclock
data and error stuctures held in  clock_handle->rad_{data,error},  with format :

struct radclock_data {
	double phat;
	double phat_err;
	double phat_local;
	double phat_local_err;
	long double ca;
	double ca_err;
	unsigned int status;
	vcounter_t last_changed;
	vcounter_t next_expected;
};

struct radclock_error
{
	double error_bound;
	double error_bound_avg;
	double error_bound_std;
	double min_RTT;
};

are updated.

The rad_data is essential to `tell time` using RADclock.  When running live, it
is made available externally to RADclock in the following two ways :

  1) API-access:  a copy is pushed to a shared memory segment (SHM), accessible by
						any `libprocess` which has compiled-in the RADclock library.

The library provides functions, declared in radclock.h and described below,
to access the rad_data fields, to transform them into different forms of time,
and to estimate their quality.
Use of the API enables the fullest possible use of RADclock capabilities.

  2) kernel-access:  a transformed copy, `kernel FFclock data`, is pushed to the
  							kernel, and can be read back from it.
							Under FBSD with KV>1 this is done via syscalls.

The kernel copy enables kernel processes to quickly access RADclock time without
kernelspace<->userspace overhead, and means RADclock based time will still be
available to kernel and other processes even if the RADclock daemon dies.
On launching a new daemon, the (more or less stale) kernel copy can even be used
to initialize clock_handle->rad_data .

On a system where the RADclock compatible FFclock kernel code implements a full
system clock interfacing to the RADclock daemon, then there is a third method,
which is simply to call traditional system clock calls (or FFclock extended versions)
which will be mapped to RADclock clock reading functions, based on on the kernel
copy (2) above.

These three copies of the current rad_data are kept roughly synchronous by calling
their update functions back-to-back within process_stamp :

 Simplified code when running live:  [ assuming relevant flags set ]
 ----------------------------------
	get_next_stamp(handle, stamp_source, &stamp);
	  leap second processing
	  process_bidir_stamp(handle, peer, &stamp_noleap, ..)); // update native RADclock
	  update handle->rad_data                 // update UTC RADabsclock
	update_ipc_shared_memory(handle)	    		// update SHM segment
	fill_ffclock_estimate(rad_data, rad_error, &cest); // convert to kernel-format cest
	set_kernel_ffclock(handle->clock, &cest); // update kernel FFclock data
	push_data_vm(handle)								// update VM store
	update_FBclock(handle);							// update kernel FBclock

There is no great problem if, for example, the SHM copy is accessed just before
process_bidir_stamp updates handle->rad_data, the error is not worse than an access
just before that stamp even arrived: in between stamps, rad_data does not update.


The form of the rad_data for these two different sharing purposes is different.
For API access the shared memory contains a variable of the following type
(radclock-private.h) :

struct radclock_shm {
	int version;				// currently = 1, not tested for anywhere
	int status;					// meant to be valid/invalid, currently unused
	int clockid;				// currently unused
	unsigned int gen;       // generation # to avoid inconsistencies
	size_t data_off;			// initially offset from top of structure to bufdata[0]
	size_t data_off_old;		// initially 		"			"			"		   bufdata[1]
	size_t error_off;			// initially offset from top of structure to buferr[0]
	size_t error_off_old;	// initially 		"			"			"		   buferr[1]
	struct radclock_data bufdata[2];	// initialized to	[ current; old ], then circular
	struct radclock_error buferr[2];	//		"					"						"
};

#define SHM_DATA(x)		((struct radclock_data *)((void *)x + x->data_off))
#define SHM_ERROR(x)	  ((struct radclock_error *)((void *)x + x->error_off))

which is a perfect copy of the current rad_data and rad_error, plus a record of the
previous values (accessible by pointer members), plus additional information.

In contrast, for FBSD KV>1 the kernel variable *cest (kclock.h) is defined as a structure :
struct ffclock_estimate
{
	struct bintime	update_time;	/* Time of last estimates update. */
	vcounter_t	update_ffcount;	/* Counter value at last update. */
	vcounter_t	next_expected		// Estimated T value of next stamp, and hence update [counter]
	vcounter_t	leapsec_next;		/* Counter value of next leap second. */
	uint64_t	period;					/* Estimate of counter period. */
	uint32_t	errb_abs;				/* Bound on absolute clock error [ns]. */
	uint32_t	errb_rate;				/* Bound on counter rate error [ps/s]. */
	uint32_t	status;					/* Clock status. */
	int16_t		leapsec_total;		/* All leap seconds seen so far. */
	int8_t		leapsec;				/* Next leap second (in {-1,0,1}). */

};

which does not quite preserve all the original rad_data and the additional
fields are more complex.  This is partly because the kernel FFclock data must offer generic
support to any FFclock, not only RADclock. In particular, the phat_local concept
and parameter is specific to RADclock.
Full details of the correspondance is provided in FFclockDoc_FreeBSDv4 under
"FF algo data kernel <--> daemon translation".



==============================================================
IPC:	Access to radclock parameters:  Access to kernel FFclock data
======================================
Main files:		kclock-{,,,}.c

Only the daemon sets the FFclock data. It is initialized OS-transparently:
	main
		clock_init_live
			init_kernel_clock		//  OS-dependent implementations, KV switching

To push the rad_data updates to the kernel there is no KV-independent function,
instead this is performed in process_stamp :

Simplified Code:
----------------
  if KV<2 									// need to use fixed point code and thread
	  update_kernel_fixed
		  set_kernel_fixedpoint(handle->clock, &fpdata))  // OS-dependent implementations
			  ioctl(PRIV_DATA(clock)->dev_fd, BIOCSRADCLOCKFIXED, fpdata);
  else
	  if HAVE_SYS_TIMEFFC_H_FBSD		//	FBSD, KV>1
	  		check for hw_counter change;
	  fill_ffclock_estimate				// transform rad_data to FFclock format
	  set_kernel_ffclock					// wrapper for syscall
		  ffclock_setestimate(cest)  	// links to syscall sys_ffclock_setestimate


Pulling FFclock data from the kernel is only well supported under FBSD, KV>1
in which case it is achieved by
	  get_kernel_ffclock					// wrapper for syscall
		  ffclock_getestimate(cest)  	// links to syscall sys_ffclock_getestimate

In other cases it could be achieved by using the older interfaces if needed.




==============================================================
IPC:	Access to radclock parameters:  SHM
======================================
Main files:		radapi-base.c

The SHM code is simple in that it has no kernel interactions, and hence no OS,KV
dependencies.

The SHM related members of clock are
	int ipc_shm_id;	// initialized to 0		in radapi-base.c:radclock_create()
	void *ipc_shm;		// initialized to NULL 	in radapi-base.c:radclock_create()

Also defined in  radclock_private.h  are the SHM relevant macros:
	#define RADCLOCK_RUN_DIRECTORY		"/var/run/radclock"
	#define IPC_SHARED_MEMORY			( RADCLOCK_RUN_DIRECTORY "/radclock.shm" )

The relevant configuration variable (config_mgr.c) is
	clock_handle->conf->server_ipc		= DEFAULT_SERVER_IPC;
and is one of the variables can be reset to on or off during restarts.
This variable is a misnomer as there is no server (or thread, except MAIN) as such
associated to the SHM underlying the IPC.

Note that the command line option -x turns the ipc server off, but there is no
option to turn it on. Thus for the IPC to work, it MUST be set on in the
conf file!



-------------------------------------
SHM: RADclock process side (internal)
-------------------------------------

The file radapi-base.c holds the core SHM create/initialize/destroy functions used
on the daemon side:

struct radclock * radclock_create(void)    // create clock, init to trivial values
int shm_init_writer(struct radclock *clock)// create shm segment for read/write
int shm_detach(struct radclock *clock)		 // detach segment from calling process

which make use of the following raw SHM functions from #include <sys/shm.h> :

            Called in
  ftok:		shm_init_{reader,writer}   create unique key given filename and user key
  shmget:   shm_init_{reader,writer}	returns shm id given key and access flags,
  													creates segment if doesnt already exist
  shmat:		shm_init_{reader,writer}   attach shm segment to process address space
  shmdt:		shm_detach						detach from process (just a wrapper for shmdt)
  shmctl:	shm_init_writer				control shm properties..
	 IPC_STAT:	fetch the properties shmid_ds data structure
	 IPC_SET:	set uid and gui permissions to match those of perm_flags
	 IPC_RMID:	remove segment (when all attached processes have exited) // Never used

Note that the `user key` input to ftok is hardwired to the arbitrary value of `a`.
It is never changed, to facilitate both the RADclock daemon and external processes being
able to locate and attach to the same segment.


 Simplified code in main running live:
 -------------------------------------
	create_handle
		radclock_create	// initializes clock.{ipc_shm,ipc_shm} to trivial values
   clock_init_live		// initialize kernel related clock members
		radclock_init_vcounter_syscall(clock)
		radclock_init_vcounter(clock)
		init_kernel_clock(clock)
	init_handle
		shm_init_writer	// creates shm segment if needed, and set permissions etc
   if restart
		rehash_deamon
				case conf->server_ipc = BOOL_ON  	shm_init_writer(clock)
				case conf->server_ipc = BOOL_OFF  	shm_detach(clock)
	shm_detach				// detach shm segment from RADclock


Note that the SHM segment, once created, is NEVER deleted/removed, not just under restarts, but even when RADclock exits completely. This is because external processes may still be attached to it. By keeping it alive these processes can continue to function (using the kernel copy fallback as described below), and once a new deamon is finally launched, it will pick up the same segment, thereby establishing the IPC without any need for signaling or other checks.
Libprocesses can always keep track of the freshness of the SHM data through the last_changed field of radclock_data.

Multiple independent and non-conflicting daemons are possible if distinguished by
different RADCLOCK_RUN_DIRECTORY values, which will result in separate SHM segments.


--------------------------------
SHM: libprocess side (external)
--------------------------------

For an external process `libprocess` to use RADclock it must include
#include <radclock.h>
Note that this declares a struct radclock but does not define it, the libprocess
does not know about its internal organisation of the clock type, it is an opaque type as
far as it is concerned.

It can create its private (rad)clock by calling:

	radclock_create		// create a clock structure, a private `radclock`
	radclock_init			// initializes clock
		radclock_init_vcounter_syscall(clock)
		radclock_init_vcounter(clock)
		shm_init_reader	// create/attach shm segment (read only)
   ...
	radclock_destroy		// destroy private clock, detach SHM

These create/initialize/destroy functions are again defined in radapi-base.c .
Note that shm_init_{reader,writer} behave symmetrically: each will create a segment if it does not already exist, or otherwise use the existing one. As explained above, this allows both private radclocks and daemons to come and go without risk of losing an ability to reconnect, and without any signalling complexities.




==============================================================
Library functions (declared in radclock.h)
================================================

The library functions are declared, with some documentation, in radclock.h .
For convenience in what follows in argument lists we write clock instead of
struct radclock *clock .

The most fundamental quantity is the definition of a feedforward (FF) compatible
counter, that is one that increases monotonically and is wide enough to avoid wrapping:
typedef uint64_t vcounter_t;

Next, STAtus words for RADclock are defined, which provide the big picture of
RADclock''s view of its status and the quality of its estimates:

#define STARAD_UNSYNC			0x0001	// not sync'ed (just started, server not reachable)
#define STARAD_WARMUP			0x0002	// in warmup phase, error estimates unreliable
#define STARAD_KCLOCK			0x0004	// RADclock kernel time is reliable
#define STARAD_SYSCLOCK			0x0008	// system clock (if RADclock controlled) is accurate
#define STARAD_STARVING			0x0010	// stamps lacking (pkts lost, server unavailable, match failure)
#define STARAD_PERIOD_QUALITY	0x0020	// RADclock period estimate is poor
#define STARAD_PERIOD_SANITY	0x0040	// consecutive period sanity checks triggered
#define STARAD_OFFSET_QUALITY	0x0080 	// RADclock absolute time poor (relatively speaking..)
#define STARAD_OFFSET_SANITY	0x0100	// consecutive offset sanity checks triggered


The functions relating to clock creation etc we have already seen from radapi-base.c:
	 struct radclock  *radclock_create(void);
	 int					radclock_init	 (clock);
	 void					radclock_destroy(clock);

and the other library function in that file is :
	int radclock_register_pcap(struct radclock *clock, pcap_t *pcap_handle);
which simply sets
	clock->pcap_handle = pcap handler   // record what packets our clock is timestamping



We now turn to those functions which are defined in radapi-getset.c .

Out of the 9 fields stored in the current rad_data within clock-ipc_shm,
7 functions are devoted to simply extracting them individually:
int radclock_get_last_changed	(clock, vcounter_t *last_vcount)	//			 = last_changed
int radclock_get_next_expected	(clock, vcounter_t *till_vcount)	// 	 = next_expected
int radclock_get_period			(clock, double *period)				// period = phat_local
int radclock_get_bootoffset			(clock, long double *offset)		// offset = ca
int radclock_get_period_error	(clock, double *err_period)		//			 = phat_err
int radclock_get_bootoffset_error	(clock, double *err_offset)		//			 = ca_err
int radclock_get_status			(clock, unsigned int *status)		// status = status

For each of these functions, if the SHM segment does not exist, the data is taken from
the kernel copy instead via:
		 get_kernel_ffclock(clock, &cest))			// get kernel copy
		 fill_radclock_data(&cest, &rad_data);		// extract rad_data from it
		 *desiredparam = rad_data.relevantfield
where cest is of type ffclock_estimate .
That that if the daemon has died, the SHM segment will exist and be used, but,
like the kernel copy, will not be updated.

The 2 missing fields are phat_local{,_err}, which are more for expert use, and
could not have been meaningfully filled from the kernel copy in any case.
NOTE:  currently radclock_get_{period,offset}_error  are effectively placeholders,
the value returned will not be correct.

Similarly, for each of the 4 fields stored in the current rad_error within the SHM
there is a dedicated extraction function:
int radclock_get_clockerror_bound		(clock, double *error_bound)
int radclock_get_clockerror_bound_avg	(clock, double *error_bound_avg)
int radclock_get_clockerror_bound_std	(clock, double *error_bound_std)
int radclock_get_min_RTT					(clock, double *min_RTT)
however in this case there is no fallback to kernel data (yet).

Finally, the functions (which currently are placeholders only) :
int radclock_set_local_period_mode(clock, radclock_local_period_t *local_period_mode)
int radclock_get_local_period_mode(clock, radclock_local_period_t *local_period_mode)
allow the selection of the local_period_mode (see algo documentation for details),
which uses
typedef enum { RADCLOCK_LOCAL_PERIOD_{ON,OFF} } radclock_local_period_t;
These functions allow a plocal preference to be specified on a per-radclock basis
(default is ON).



We now turn to those functions which are defined in radapi-time.c , which deal with
actually reading the Robust Absolute and Difference Clock (RADclock).

There are 4 library functions. Each of them are wrappers for underlying
functions (in radapi-time.c) which do the real work, in two flavours, depending on
whether the SHM segment exists, or if we need to fall back to the kernel copy.

The first set of two deal with the Absolute clock, and make use of
ffcounter_to_abstime_{shm,kernel}(clock, vcount, abstime)  .

int radclock_gettime(clock, long double *abstime)
	- returns current time based on a counter value it reads itself
int radclock_vcount_to_abstime(clock, const vcounter_t *vcount, long double *abstime)
	- returns time based on the counter value passed in

The second set of two deal with the Difference Clock, and make use of
ffcounter_to_difftime_{shm,kernel}(clock, vcount, vcount, duration)  .

int radclock_elapsed(clock, const vcounter_t *from_vcount,long double *duration)
	- returns time elapsed (now - from) from the passed value to a value it reads itself
int radclock_duration(clock, const vcounter_t *from_vcount, const vcounter_t *till_vcount, long double *duration)
	- returns time duration (till - from) based on the counter values passed in

In each case the time interval calculated can be negative, as is returned as a real.
Conversion to common time formats such at tval and timespec can be achieved using
conversion functions found elsewhere.

Note that the counter is read even in the radclock_{vcount_to_abstime,duration} cases,
in order to establish when the calculation is being performed.
This is to enable quality assessment, which depends on whether the parameters used
were up to date, somewhat out of date, or worse, with respect to the counter values
defining the time interval being calculated.

The quality assessment functions radapi-time.c:{raddata_quality,in_SKMwin} are not public,
but are called by ffcounter_to_abstime_{shm,kernel} and radclock_{elapsed,duration}.


The library included the 2 counter read functions defined in radclock-read.c :

inline vcounter_t radclock_readtsc(void);
int radclock_get_vcounter(clock, vcounter_t *vcount);

Here radclock_get_vcounter is just a wrapper for clock->get_vcounter(clock, vcount) .
It is needed because the the radclock type is opaque to the library.

The function radclock_readtsc is architecture dependent and its availability
depends also on kernel version, as described further below.  It is in the library
because, if such a TSC reading function is available it represents the best
(fastest) way to access a high resolution counter, which importantly avoids
system call overhead and works equally well from user or kernel space.
Of course for this to be useful, the daemon must also be using the TSC so the
rad_data shared via the SHM will be relevant to that choice of counter.


The above functions concerned timing of arbitrary events. When timestamping
packets using the RADclock kernel interface, and recovering via pcap, more is required.

The FFclock kernel code supports different variations on the form of packet timestamping
it makes (relevant for the daemon when obtaining its stamp data, and external applications
when timestamping arbitrary packets).  See the discussion earlier for more details.
The possible tsmodes, described above can be get/set with (radapi-pcap.c) :

int pktcap_set_tsmode(clock, pcap_t *p_handle, pktcap_tsmode_t mode);
int pktcap_get_tsmode(clock, pcap_t *p_handle, pktcap_tsmode_t *mode);

The following library function enables a single packet to be recovered :
int radclock_get_packet(clock, pcap_t *p_handle,
		struct pcap_pkthdr *header, unsigned char **packet, vcounter_t *vcount,
		struct timeval *ts);

An example of its use is given in radclock_capture_packet.c:main  :
	 /* Block until capturing the next packet */
	 ret = radclock_get_packet(clock, pcap_handle, &header,
										(unsigned char **) &packet, &vcount, &tv);



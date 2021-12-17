Copyright (C) 2021, Darryl Veitch <darryl.veitch@uts.edu.au>
This file is part of the RADclock program.

RADclock and FFclock documentation.  
Written by Darryl Veitch


Summary
=======

This documents how the RADclock software can be used, and how one can get
started.  It is less technical that the other documentation in
RADclockrepo/Documentation, though it still assumes some familiarity with time
keeping, though the installation instructions are self-contained.

A brief summary of the Documentation files is :

**RADclockDoc_threads**  
	The multi-threaded structure of the radclock daemon, and its global variables.

**RADclockDoc_library**
	RADclock library functionality and various inter-process and kernel communications.

**RADclockDoc_config**
	Configuration system of RADclock, including command line options.

**RADclockDoc_verbose**
	The verbosity system for the daemon generally, the library and the RADclock algo.

**RADclockDoc_algo_bidir**
	The RADclock algorithm itself [Doc under construction].

**RADclockDoc_guide**
	This document.

**FFclockDoc_FreeBSD**
	The FFclock FreeBSD kernel code, and connections to its canonical daemon: RADclock.

**FFclockDoc_Linux**
	The FFclock Linux kernel code, and connections to its canonical daemon: RADclock.
  
For users of RADclock, the most useful documents would be RADclockDoc_library
to understand the available library features, and RADclockDoc_config to understand
the configuration options and radclock.conf file (though the essentials of this
are covered below).  It is useful to view the documents (other than this one) with colour 
settings set to C syntax.

There are also the man pages in repo/man,  and in the case of FreeBSD, 
repo/kernel/freebsd/FreeBSD-XX/CurrentSource and in the system once installed :

 RADclock:		radclock.conf.5	radclock.8
 FFclockBSD:		ffclock.2	ffclock.4
 					ffclock_{getcounter,{get,set}estimate}.2     (  identical to ffclock.2  )


Research papers on RADclock can be found on Darryl Veitch''s webpage at
[http://138.25.60.68/~darryl/index.html]   .


Guide to Guide
===========

This guide has the following sections and subsections

## Introduction to RADclock

## RADclock installation FreeBSD
### FFCLOCK kernel 
#### Patching, Compiling, Installing
#### TSC versus TSC-low
#### Bypass mode
### RADclock daemon 
#### Installation
#### Controlling radclock when running as a daemon
#### Configuration file
#### Rotating the Log File

## RADclock installation Linux & Raspian
## Configuring RADclock

## Using RADclock
### Timestamping based on a raw timestamp (counter value)
### Timestamps obtained through standard system clock read functions
### Timestamps obtained via pcap, interpreted through radclock
### Timestamp arrives as an NTP client request packet, to be served by RADserver

## Using the RADclock API
## The Available Kernel Clocks, and the System Clock
## Packet timestamping [tsmode]
## Perfect Clock Comparison built in		[can skip at first reading ]


## Introduction to RADclock

The radclock is a software clock built on top of a suitable host hardware
counter, and a sequence of timestamp exchanges with a time server.
It is an implementation of the Robust Absolute and Difference clock
(RADclock), a clock synchronization algorithm based on a FeedForward
(FF) paradigm described in a suite of research papers by Darryl Veitch
and coauthors since 2002.  Its Absolute clock is capable of synchronizing
to Absolute time (UTC) with considerably higher accuracy, and reliability,
than ntpd.  Its Difference clock is capable of measuring time
differences to far higher accuracy and robustness, for example RTTs to
nanosecond accuracy, even over very noisy Internet paths.

The radclock binary communicates with a remote time-server using standard
NTP request response packets.  It relies on kernel patches to
access bpf-based in-kernel raw timestamps (counter reads) of these
packets.  If these are not available, it can _run dead_ to synthesize
time offline based on stored timestamp traces.

On FreeBSD systems with full FFclock support, the binary can run as a
daemon to synchronize the FFclock system clock which is integrated
into the kernel timecounter mechanism (the FFclock provides generic
support for a FeedForward based system clock, but was designed according
to RADclock principles), and optionally also the traditional FB
system clock.  Independently of this, it can also optionally serve time
to other processes in a number of ways using a library.  In the case
where the counter used is the TSC (Time Stamp Counter), radclock can take
advantage of a defined _kernel bypass mode_ when available, to access counter
reads without system call overhead.


## RADclock installation FreeBSD

The radclock daemon requires special support from the FreeBSD kernel.
FreeBSD 10.1 and later natively provide this via FFCLOCK support,
however that support was incomplete and is now outdated.
Updated FFCLOCK code is available here from FreeBSD 11.2, but
currently requires the kernel to be patched (this will go away when the changes
are merged upstream into FreeBSD from, hopefully, 13.0).
The kernel patch is provided with the radclock source tarball.

### FFCLOCK kernel

#### Patching, Compiling, Installing
Imagine you have downloaded radclock-0.4.0.tar.gz into /tmp .

	cd /tmp
	tar -xzf radclock-0.4.0.tar.gz
	cd radclock-0.4.0
	sudo Tools/update_FFkernelsource		# overwrite altered kernel files directly

Alternatively, the provided patch against the FreeBSD release versions can be
used. Eg in the case of FreeBSD-12.1 :

	cd /usr/src
	patch -Np0 < /tmp/radclock-0.4.0/kernel-patches/freebsd/FreeBSD-12.1/FFclock-FreeBSD-12.1.patch
	
We recommend the use of  update_FFkernelsource  as it will work directly on top of any other 
FFclock patched release that may already be installed.  It also installs the header files in  
/usr/include  to help in application recompilation if needed.

The above applies the needed updates to the system, man pages, and
installs the kernel configuration file :

	/usr/src/sys/amd64/conf/FFCLOCK   (in the case of amd64)
which simply adds the FFCLOCK option to those for the GENERIC kernel.

To compile the FFCLOCK kernel:

	cd /usr/src
	sudo make -j2 buildkernel KERNCONF=FFCLOCK
	sudo make   installkernel KERNCONF=FFCLOCK
	reboot

Alternatively, the script

	Tools/compile_FFkernel
does this for you, dumping the verbose output into build.log  .

If you are cautious, you can set up to fall back to the previous kernel in case
the new one fails:

	sudo make installkernel KERNCONF=FFCLOCK KODIR=/boot/FFtest
	nextboot -k FFtest   # try to boot /boot/FFtest, reverts to /boot/kernel if fails



#### TSC versus TSC-low		[ advanced topic, can be skipped! ]
Take a look at the counters available on your system with

	sysctl kern.timecounter.choice
If you see TSC-low rather than TSC, this is because the TSC counter has been deemed
to be too-high in frequency, and so it has been reduced (by some power of 2),
See FFclockDoc_FreeBSD for details.
(Note this is not a FFclock "feature", it has been in FreeBSD since around release 11.0)

This is undesirable if you want the highest possible resolution, or if you want to
take advantage of the FFCLOCK bypass mode (which RADclock does, if available).
To fix this, add   "kern.timecounter.tsc_shift=0"   to /boot/loader.conf
This takes effect after a reboot.
  

#### Bypass mode		[ advanced topic, can be skipped! ]
The idea of bypass mode is to allow, in the case where the chosen
counter is (an invariant) TSC, the use of the rdtsc() function to read the TSC
registers directly, rather than reading the re-synthesized version of the TSC
counter created within the FFclock-enabled timecounter mechanism.
These two versions of the TSC are equivalent in terms of value (thanks to
origin-setting code within the FFclock). The benefit is latency of access :
  FFkernel:  slightly lower on direct reads such as packet timestamping
  userland:  much lower as a syscall is avoided
  
Bypass mode is a FFkernel mode as it will affect the system for all users.
As described in  RADclockDoc_FreeBSD.txt, it defaults to Off (0) and can be
set to On (1) via :
	  sysctl  kern.sysclock.ffclock.ffcounter_bypass		# see current value
	  sysctl  kern.sysclock.ffclock.ffcounter_bypass=1		# turn on
	  
The daemon, and user radclocks, detect if bypass is activated, and set their
counter reading function to either use rdtsc, or read the timecounter''s TSC,
depending.



### RADclock daemon

Required packages [install prior to building the FF kernel if needed]

	gcc coreutils git
	autoconf-wrapper automake autotools libtool

Packages can be installed with

	pkg install pkg-name
	
#### Installation
	tar -xzf radclock-0.4.0.tar.gz
	cd radclock-0.4.0
	./configure

Make sure the output of the configure script indicates that the kernel support
has been detected.  If not, did you forget to reboot after compiling the kernel?
Without kernel support, radclock can still compile and run _dead_ on stored
data traces, but can't run live or act as a daemon for the system clock.

Now make and install the daemon.

	make
	sudo make install				# copies the executable to /usr/local/bin/radclock

You can quickly test radclock is working with

	radclock -h						# take a look at the command line options
	
and test it on the sample ascii input data provided :

	radclock -p 1  -s radclock-0.4.0/tests/Stratum1Stamps.dat

This file contains 1000 stamps (one per row) from a RADclock capture using the
high quality non-public stratum-1 server  ntp.melbourne.nmi.gov.au .


#### Controlling radclock when running as a daemon
To set this up, execute

	sudo Tools/install_startscript
which provides access to the service interface, and ensures the daemon will
start after boot.

Apart from the standard commands of stop, start, restart and status understood
by the service command, the radclock script supports:

	service radclock reload		# send a HUP to signal a reloading of the conf file
	service radclock rotate		# send a USR1 signal to close the logfile (see below)
The status command will display a line for each daemon thread.


#### Configuration file
When running as a daemon, radclock uses the configuration file

	/etc/radclock.conf

If the file does not exist, then one populated with default values will be
created, then radclock will exit.
Running a second time should be successful, however it is highly likely that you
will want to edit the file first! in particular to set parameters such as the
time_server, polling_period, and ntp_server.
[ See configuration section below ]

Many of the parameters can be altered without having to restart the daemon
(restart means stop, then start again from scratch).
To do this, edit the configuration file as desired, then use
	service radclock reload

See the radclock.conf(5) manpage for more details on configuration file use,
and radclock(8) for more details on command line options.


#### Rotating the Log File
The logfile is an important source of information for radclock. When run as
a daemon it is located at

	  /var/log/radclock.log
	  
This can become very large if radclock is run with -v, or much worse with -vv !
The RADclock logfile can be closed as above if running as a daemon, or in any
event by sending a SIGUSR1 signal:

	pkill -USR1 radclock

---
FreedBSD

To exploit this for automatic log file rotation, add the following line to /etc/newsyslog.conf

	/var/log/radclock.log    644  7   100  *  JC  /var/run/radclock/radclock.pid  30

Here 30 is the value of SIGUSR1 on FreeBSD. To check the value on your system, use
 kill -l USR1

---
Linux
Put in a dedicated entry into /etc/logrotate.conf



## RADclock installation Linux & Raspian

These distributions are out of date and so will not be detailed here.
Watch this space.  They will benefit immediately from the current version of
RADclock on the daemon side.



## Configuring RADclock

RADclock uses a file, with default name radclock.conf , to hold a human and
daemon readable record of configuration parameters.  A subset of these can be
overwritten on the command line. A list of all command line options can be
viewed with
	radclock -h
	
RADclock allows many of its subsystems to be switched on or off  [default is shown]:

	ipc_server = on			//	write to the SMS (Shared Memory Segment) to support user radclocks
	adjust_FFclock = on		// let the daemon discipline the system FFclock
	adjust_FBclock = off		// let the daemon discipline the system FBclock
	ntp_server = off  		// allow the RADserver to serve clients

Some options can only be controlled via the command line. When running as a daemon,
this implies modifying the command line string passed in the startup script.
Examples include
	- detailed output files, needed for daemon troubleshooting
	- upstream UDP port, if you want ntpd to run as well as the daemon, you need
	  to select a port other than 123.
	  
Other important parameters to keep in mind are
	time_server				// the remote server queried by the daemon
	polling_period				// the period separating the timing queries to time_server
	verbose_level				// this should normally be quiet, but more if testing

The time_server should be selected to be reliable and as close as possible
(in terms of RTT) to the host, and with stable routing.

Many of the parameters can be altered without having to restart the daemon
(restart means stop, then start again from scratch).
To do this, edit the configuration file as desired, then use
	service radclock reload

Important examples of on-the-fly changes are
	ntp_server					// to stop excessive incoming client traffic
	verbose_level				// may want to set it high for a period to get more
								// information if there is a problem, but not forever!
								
Another approach to verbosity control is to set it to quiet (which yields no output after
startup except serious errors), but to use the option to save raw output in pcap
format (-w on the command line, or via the configuration file). This file can
be replayed through any radclock offline with verbosity set to high, to obtain
detailed output identical to what the original radclock would have produced
(kernel interactions aside). It can be sent to the maintainer for analysis if
there is an issue that needs examination.

The configuration system of RADclock is documented in detail in RADclockDoc_config.txt.



## Using RADclock

There are several ways in which RADclock can be used. They are classified here
based on the timestamp source.

### Timestamping based on a raw timestamp (counter value)

The RADclock API can be used as described below, to exploit raw timestamps
to read either the Absolute or Difference clock.

The raw timestamps could have been obtained by
 - reading the counter from userland
 - provided externally
 - extracted via pcap [ see below ]
 
This approach delivers the highest possible flexibility and accuracy.
It requires the use of radclock library functions to set up.
[Whether the system clock is using an FF clock or not is not relevant here.]

RADclock Replay:		[ advanced topic, can be skipped! ]
If the daemon stamp input and/or output is being recorded, raw timestamps
can simply be stored and `replayed` by radclock offline at some later time.
Replay will deliver timestamps that are identical to those that would have
been obtained if a radclock were read in real time.
This can be advantageous when resources are limited and fast raw reads are
preferable.  The raw timestamps could even be replayed using a different
(for example upgraded) version of radclock.


### Timestamps obtained through standard system clock read functions

1)  If the system clock is set to use the FFclock class [see below], then the
timestamps will correspond to reads of FFclock: effectively a version of
RADclock which has been gently tweaked to ensure monotonicity.
The timestamp format will conform to the function specifications.
**This option is not available under Linux.**
 
This approach limits your options, it not as accurate as the underlying
RADclock (perhaps by as much as several ms, though the difference may often
be zero), and does not work if the system clock is set to be FB based.
It may be well suited to some situations where accuracy is less important
but monotonicity is critical.

** See warning below.

2)  If the system clock is set to use the FBclock class [see below], And the
daemon has the  adjust_FBclock = on  option on, then the
timestamps will correspond to reads of a clock which follows RADclock roughly,
but with some delay and smoothing.

This option is definately not recommended, this _radclockd_ replacement to
ntpd is a relatively simple design, whose goal was to provide a sane alternative
to an (often) insane ntpd.  It will never do anything crazy, but is has
heavily damped convergence time, is constrained by the FB-based interface to
the kernel (in particular it only offers Absolute time), and will have an
accuracy well below that of the underlying RADclock.
See the "IPC:	Standard system clock disciplining" section of
RADclockDoc_library.txt for more details.

** If this option is used, ntpd should not be running, otherwise two daemons
will be fighting each other!
	   
	   
### Timestamps obtained via pcap, interpreted through radclock

This option relates, of course, only to the timestamping of packets.
This timestamping occurs in the kernel and so has much lower latency _noise_
compared to userland packet timestamping, and should be taken advantage of.
The daemon uses this form.   There are two ways this can be used.

1)   using radclock library functions, pcap data can be used to access underlying
	bpf data on the packet, allowing a raw timestamp of the packet arrival to
	be obtained.  This can then be used with the API as above.
2)	pcap also returns a normal timestamp. By setting the BIOCSTSTAMP on the
	capture to the desired timestamping mode (tsmode, which is passed to bpf),
	this timestamp can be chosen to be one of several FF or FB variants
	[see below].
		
For packet capture applications, it may be convenient to use
the pcap option, setting an appropriate tsmode to get the kernel to conveniently
convert the raw timestamp capture in the kernel to an appropriate RADclock read.

Existing application code will be able to run largely as normal, however the
desired tsmode will have to be set, and depending on the mode, radclock library
functions or a few lines of custom code may be needed to interpret the
timestamp correctly.
[This is partly because pcap is behind the times, and does not even support the
existing bpf capabilities correctly, let alone the FF enhanced ones.]
  
  
### Timestamp arrives as an NTP client request packet, to be served by RADserver.

The daemon also runs an NTP server, delivering timestamps obeying the NTP
protocol as follows:

	incoming rec timestamp:  a RADclock Absolute timestamp
	outgoing xmt timestamp:  generated using the RADclock difference clock

The raw timestamps of the RADclock server are currently make in userland in
the daemon. Kernel timestamps are being investigated.

Timestamps received by clients of RADserver therefore contain normal absolute
timestamps, but the interval  xmt - rec  is highly reliable and accurate as
it is generated using the difference clock.

Currently the RADserver only responds to CLIENT mode requests. It does not
respond to other modes or implement the entirety of the NTPv4 standard.
As it does not respond to control packets nor react or generate kiss codes,
it cannot be exploited in known NTP protocol attacks.



## Using the RADclock API

The API allows programs to define _user radclocks_, enabling radclock reading
functionality based on direct access to the RADclock parameters maintained by
the daemon, via a Shared Memory Segment (SMS) written to securely by the daemon.

The library includes a suite of functions to enable both the Absolute and
Difference forms of the R-A-D-clock to be exploited.  The suite is described in
repo/libradclock/radclock.h , and described in detail in RADclockDoc_library.
Using the API gives the widest possible set of options to exploit RADclock
capabilities.

If the daemon has died, the radclock library functions will continue to obtain
radclock parameters from the SMS, however they will fall increasingly out of date.
If the daemon is restarted, these will be refreshed seemlessly, there is
nothing to re-initialize on the user radclock side.

If the SMS itself has died for some reason (unlikely), the function will switch
to the FF parameters held in the kernel, again transparently to the user.
Without a working daemon however, such parameters will become stale over time.
The library functions incorporate error bound estimates on absolute
timestamps which take into account the events being timestamped, in relation to
the staleness of the data.

The program  repo/examples/radclock_get_time.c  (which along with the daemon
is built by make), provides an example of how a user radclock can be created
and used, both as an Absolute and Difference clock.
These examples deal with the case where raw timestamps (counter readings) are
either provided externally, or obtained directly by reading the counter from
userland via the library function radclock_get_vcounter(,,) .


## The Available Kernel Clocks, and the System Clock

[This option is not available under Linux, except for packet timestamping, with some limitations]

The FF-aware kernel expands the system clock or _SYSclock_ architecture.
There are now two classes of clocks: FBclock (feedback based) and FFclock
(feedforward based), and they are both always active.
Which is selected to actually Be the system clock  (which means: used by the
suite of standard kernel clock read functions) can be seen by

	sysctl -a kern.sysclock			# takes values in {"FBclock", "FFclock"}
and set by

	sysctl -a kern.sysclock="FFclock"    # set system clock to read from the FF class

The available clocks are:

  FB class:   FB             :  Absolute clock with an interface matched to a FB based daemon
  FF class:   FFnativeclock  :  corresponds to the Absolute clock provided by the daemon
				  FFclock		  :  carefully adjusted version of FFnativeclock to ensure monotonicity
				  FFdiffclock	  :  version of FFnativeclock equivalent to the daemons''s Difference clock
				  
Required daemons:

	FB class:  typically ntpd  [ alternatives exist ]
                radclock daemon [if configuration parameter  adjust_FBclock  is set ]
               
	FF class:  radclock daemon [if configuration parameter  adjust_FFclock  is set ]
				
Note that the FFclock daemon directly synchronizes FFnativeclock. The other
FFclocks are created within the kernel, based off FFnativeclock.

	FFclock:  this is the FF clock underlying standard system clock read functions,
	          because they normally assume or require monotonic behaviour.
	FFdiffclock:  this effectively enables, from within the kernel, the RADclock
			    difference clock to be used, even though it returns a normal timestamp.
			    This is because it is constructed to avoid all absolute clock corrections.

All three FFclock are initialized to the same value (an approximate UTC time
shortly after boot). FFdiffclock will drift over time.


** WARNING:  in systems where the counter may be paused, for example during
	sleep or suspend states, the hard resetting of the RTC (realtime clock)
	of the system can cause system slow downs when sysclock is set to FFclock.
	In such cases it is recommended that the sysclock be let on its default value
	of FBclock.   This does not prevent the FFclock from being used for timestamping
	or applications !
	



## Packet timestamping [tsmode]

The daemon, and user radclocks, benefit from the FFclock-aware kernel timestamping
built into the bpf subsystem as part of the FF kernel patches. The timestamp
flags controlling the type of timestamp requested has been considerably expanded
to cover four mutually exclusive dimensions:
		FORMAT, FFRAW, FLAG, CLOCK
which can be independently selected.

FORMAT:  do you want the timestamp returned to have mus, ns, or maximum resolution?
FFRAW:   do you want the raw timestamp to be returned (in addition to a traditional one)?
FLAG:  	do you want UTC or Uptime?
			do you want "accurate but slow", or "faster but less accurate"
			[ fast means snapped to the kernel interrupt cycle grid` (10ms granularity) ]
CLOCK:   which kind of clock?  FB?  FF{native,diff,}clock?

To avoid the need to deal with low level details the following presets are defined
(see comment block in radclock.h for more details)

	enum pktcap_tsmode {
		PKTCAP_TSMODE_NOMODE = 0,			// no FF support in pcap, or very early versions
		PKTCAP_TSMODE_SYSCLOCK = 1,		// get raw, plus normal timestamp from sysclock
		PKTCAP_TSMODE_FBCLOCK = 2,			//                "                    FBclock
		PKTCAP_TSMODE_FFCLOCK = 3,			//                "                    FFclock (mono)
		PKTCAP_TSMODE_FFNATIVECLOCK = 4,	//                "                    FFclock (native)
		PKTCAP_TSMODE_FFDIFFCLOCK = 5,	//                "                    FF difference clock
		PKTCAP_TSMODE_RADCLOCK = 6,		//    "   , plus RADclock timestamp (userland) [expert use]
		PKTCAP_TSMODE_CUSTOM = 100,		// direct accept a customised tsmode input
	};

The nominal choice here is FFNATIVECLOCK.  This selects the FF kernel clock
that corresponds directly to the daemon''s RADclock, returns a raw timestamp also
for use with the RADclock API, and conveniently provides a UTC timestamp in
timeval format (microsecond resolution) based on that raw timestamp.

The program  repo/examples/radclock_capture_packet.c  (also built by make),
provides an example of how a radclock can be created and used to timestamp a
set of packets, using different tsmodes presets, or custom tsmodes.
Read on for more detail.


## More detail  [ can skip at first reading ]
From bpf.h  (and the bpf and pcap section of RADclockDoc_FreeBSD.txt) ,
the definition of the tsmodes is as follow :

	/* Time stamping flags */
	// FORMAT flags 	[ mutually exclusive, not to be ORed ]
	#define	BPF_T_MICROTIME	0x0000
	#define	BPF_T_NANOTIME		0x0001
	#define	BPF_T_BINTIME		0x0002
	#define	BPF_T_NONE			0x0003	// relates to ts only, FFRAW independent
	#define	BPF_T_FORMAT_MASK	0x0003
	// FFRAW flag
	#define	BPF_T_NOFFC			0x0000   //   no raw timestamp
	#define	BPF_T_FFC			0x0010   // want raw timestamp
	#define	BPF_T_FFRAW_MASK	0x0010
	// FLAG flags   [ can view bits as ORable flags ]
	#define	BPF_T_NORMAL		0x0000	// UTC, !FAST
	#define	BPF_T_FAST			0x0100   // UTC,  FAST
	#define	BPF_T_MONOTONIC	0x0200	// UPTIME, !FAST
	#define	BPF_T_MONOTONIC_FAST	0x0300// UPTIME,  FAST
	#define	BPF_T_FLAG_MASK	0x0300
	// CLOCK flags   [ mutually exclusive, not to be ORed ]
	#define	BPF_T_SYSCLOCK		0x0000	// read current sysclock
	#define	BPF_T_FBCLOCK		0x1000   // read FB
	#define	BPF_T_FFCLOCK		0x2000   // read mono FF (standard reads are mono)
	#define	BPF_T_FFNATIVECLOCK	0x3000	// read native FF
	#define	BPF_T_FFDIFFCLOCK	0x4000	// read FF difference clock
	#define	BPF_T_CLOCK_MASK	0x7000

For example the FFNATIVECLOCK preset above correspond to :

	bd_tstamp = BPF_T_MICROTIME | BPF_T_FFC | BPF_T_NORMAL | BPF_T_FFNATIVECLOCK ;

On the other hand an application specializing in RTT measurement may select

	bd_tstamp = BPF_T_NANOTIME | BPF_T_FFC | BPF_T_NORMAL | BPF_T_FFDIFFCLOCK ;
	
as it needs the raw counter for use with the RADclock API, and it is
convenient to also obtain what the (FFclock version of the RADclock''s)
Difference clock reads directly, and can expect this to be accurate to sub-nanosecond,
so nanosecond resolution is appropriate (returns a timeval format, but this can
be cast to a timespec format).

  
#### Linux limitations

Some of the above options are not supported, even for packet timestamping. In particular
BPF_T_{NANO,BIN}TIME, and BPF_T_SYSCLOCK. While the FAST options are supported,
they do not provide real benefits and should not be used.



## Perfect Clock Comparison built in

Regardless of the current value of the SYSclock, pcap captures can be set using
any allowed tsmodes.  Thus for example one user radclock may read the FB clock,
and other a FF clock.  Alternatively one may read the FFnativeclock (effectively
the RADclock Absolute clock) selecting microsecond resolution,
and another FFdiffclock  (effectively the RADclock Difference clock) selecting
bintime resolution (this would provide RTTs to better then ns resolution,
and ns level accuracy as well!).

The new FF bpf packet stamping architecture has another important feature.
If a packet is set to be tapped by a number of different processes, each tap
will deliver exactly the same timestamp. More precisely, a common raw
timestamp is taken, as well and both FF and FB clock state,
and these are used to deliver the timestamps to all listening processes.
The details specified by the respective tsmodes are obeyed, but the underlying
timestamp is shared.
 
This means that if many processes are listening for the same packet, their
timestamps do not depend on the order in which they are processed or added to
the tap: if their tsmodes are the same, their returned timestamps will be
identically equal regardless of system load.

Expert uses :
	- enables perfect `back-to-back clock comparisons for the purpose of clock
	  algorithm evaluation.
	- the shared raw timestamp can be used as a foolproof unique key for pkt
	  matching across measurement applications

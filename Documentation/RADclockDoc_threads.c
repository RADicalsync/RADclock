/*
* Copyright (C) 2016-2020, Darryl Veitch <darryl.veitch@uts.edu.au>
*/

RADclock documentation.
Written by Darryl Veitch
===============================

Convention to point within code:
	/dir/dir/file : functionorstructureinfile

Glossary:
 UPD = Updated Parameter Data
 rd = raw data
 rdb = raw data bundle
 algo = algorithm, the RADclock clock synchronization algorithm


==============================================================
Summary
=======

This documents the running of radclock as a process, including its multi-threaded
structure and flow of execution, reaction to UNIX signals, and the structure of main.
The source files are in RADclockrepo/radclock/radclock/
The main ones are
	radclock_main.c
	pthread_mgr.c
	pthread_trigger.c
	pthread_ntpserver.c
	pthread_dataproc.c
	rawdata.c
The main associated header files are
	pthread_mgr.h
	radclock_daemon.h
   sync_algo.h


==============================================================
Introduction
============

If running from trace data, there is only a single, default MAIN thread.
If running from a live source then MAIN creates additional threads.
The main rationale is the asynchronous nature of many of the underlying operations,
such as the sending of packets, their receipt and processing, and the serving of
timing requests.

If running from a live source MAIN always creates two additional core threads:

	Data Processing (DATA_PROC): 	which processes captured raw data,
whereas the MAIN thread is responsible for the capture of the raw data, based on
what pcap passes up from the kernel packet capture.

   NTP Client (TRIGGER):  which generates periodic NTP requests toward the remote server.
The name stems from the fact that it is the passage of packets, either outgoing or
incoming, which triggers the raw timestamping and data capture, which in turn
triggers everything else. If no packets flow, then on the whole nothing happens.

Two other threads are started by MAIN depending on optional settings:
	RADclock Server (NTP_SERV):  which serves external NTP timing requests from RADclock
and
	Virtual machine RADclock (VM_UDP_SERV): which serves RADclock clock parameters
		 to guest OSes over UDP.

Finally, in old RADclock kernel versions (version<2), a thread is needed to
periodically update the kernel version of the RADclock `globaldata`, ie parameters,
due to a lack of floating point capability in the kernel.
	Kernel fixedpoint updater (FIXEDPOINT):	deprecated.

Codes for all threads except MAIN are defined in pthread_mgr.h :
#define PTH_{NONE,DATA_PROC,TRIGGER,NTP_SERV,FIXEDPOINT,VM_UDP_SERV} = {0,1,2,3,4,5}


The thread functions are the `main` executing code of the thread.  They are:
In radclock_main.c :
	int main(int argc, char *argv[])
In pthread_mgr.c :
	void *thread_trigger(void *c_handle)
	void *thread_data_processing(void *c_handle)
	void *thread_fixedpoint(void *c_handle)
In pthread_ntpserver.c
	void *thread_ntp_server(void *c_handle)
In virtual_machine.c
	void *thread_vm_udp_server(void *c_handle)

The structure of each thread program follows the following template, to which
thread-specific code is added :

  init_thread_signal_mgt();  							// block existing signals
  initialize thread
  while ( STOP flag not set for this thread )   // loop to do work until thread STOPped
		  sleep
		  do the work for this round
  pthread_exit(NULL)										// terminate




==============================================================
Run and SYNCTYPE Modes
============================

The value (see radclock_daemon.h) of

	handle->run_mode = RADCLOCK_SYNC_{NOTSET,DEAD,LIVE}

is set in main according to the opinion of stampinput.c:is_live_source(handle),
as a function of the input modes recorded in the configuration, with logic:
if either ascii or pcap (trace) input selected,   // an error if both
	run as dead
else
	run live.
Knowledge of run_mode then allows the critical choice of running as a single
thread, or multi-threaded, to be taken in main.


RADclock has several different SYNCTYPE modes, defined in config_mgr.h :
#define SYNCTYPE_{SPY,PIGGY,NTP,1588,PPS,VM_UDP,XEN,VMWARE} = {0,1,2,3,4,5,6,7}
and corresponding raw data types (where relevant) are defined in rawdata.h :
 RD_{UNKNOWN,TYPE_{SPY,NTP,1588,PPS}} .

These modes combine scenarios involving the fundamental type of the data :
	NTP:  Bidirectional NTP packet		// usual input, either dead or alive
	PPS:	Unidirectional PPS				// timestamp PPS input in kernel
	1588:	PTP (IEEE-1588 protocol)		// capture and timestamps of PTP pkts
and its source :
	RADclock NTP client (NTP)				// usual live mode, using TRIGGER to send pkts
	existing NTP packets (PIGGY)			// listen in (piggyback) on an exiting NTP flow
	existing system clock (SPY) 			// using the system clock as a sneaky local server
and special needs under virtualization :
	virtualized (VM_UDP,XEN,VMWARE)		// serve RADclock time, or clock reading only

and as such involve considerations which are a function of run_mode, and if running
live, concern multiple threads.  As a result the logic is complex but it is driven again
by configuration choices.  Many of the choices (eg 1588, PPS) are works in progress, and
not all combinations are currently supported.

Note that under SYNCTYPE_PIGGY, TRIGGER still runs in the same way, except that
the thread work is just to sleep, rather than to create, send, wait for, and test,
NTP packets.

The relevant default in config_mgr.h is
#define DEFAULT_SYNCHRO_TYPE		SYNCTYPE_NTP

Strong SYNCTYPE dependence is found in particular in
config_mgr.h, pthread_trigger.c, rawdata.c, stampinput.c, virtual_machine.c, and main.




==============================================================
Global variables
============================

The global variable, defined at the top of radclock_main.c :
	struct radclock_handle *clock_handle;
points to all the information needed to be shared across the process, and to other
processes via the RADclock API library.
The structure definition below is from radclock_daemon.h.
It employs other structures many of which are defined in radclock-private.h .

struct radclock_handle {

	/* Library radclock structure */
	struct radclock *clock;
	
	/* Clock data, the real stuff */
	struct radclock_data rad_data;
	/* Clock error estimates */
	struct radclock_error rad_error;
	/* Virtual Machine management */
	struct radclock_vm rad_vm;

	/* Protocol related stuff on the client side (NTP, 1588, ...) */
	struct radclock_ntp_client		*ntp_client;
	/* Protol related stuff (NTP, 1588, ...) */
	struct radclock_ntp_server		*ntp_server;

	/* Raw data capture buffer for libpcap */
	struct raw_data_queue *pcap_queue;
	/* Raw data capture buffer for 1588 error queue */
	struct raw_data_queue *ieee1588eq_queue;

	/* Common data for the daemon */
	int is_daemon;
	radclock_runmode_t 		run_mode;
	/* UNIX signals */
	unsigned int unix_signal;

	/* Output file descriptors */
	FILE* stampout_fd;
	FILE* matout_fd;

	/* Thread related */
	pthread_t threads[8];
	int	pthread_flag_stop;
	pthread_mutex_t globaldata_mutex;
	int wakeup_checkfordata;
	pthread_mutex_t wakeup_mutex;
	pthread_cond_t wakeup_cond;

	/* Configuration */
	struct radclock_config *conf;

	/* Algo output */
	radclock_syncalgo_mode_t syncalgo_mode;
	void *algo_output; 	/* Defined as void* since not part of the library */
								// sync_algo.h:bidir_output  structure
	/* Stamp source */
	void *stamp_source; /* Defined as void* since not part of the library */
	/* Synchronisation Peers. Peers are of different nature (bidir, unidir) will cast */
	void *active_peer;   /* Internal algo variables */
	
};

The global clock_handle is almost never used directly, instead access is controlled
though passing it as a parameter, where internal to functions its copy goes under the name
`handle`. Apart from main, the exceptions are the callback functions signal_handler
and fill_rawdata_spy as they cannot take the extra parameter.

The only other global variable is verbose_data, needed by the verbosity system
and available everywhere via an extern declaration in verbose.h.
It is protected by alarm_{mutex,cwait} as discussed below.



==============================================================
Main and Basic Execution Flow
============================

RADclock is launched from the command line or a shell script.
To begin a description of the flow of execution, consider this summary of main.

 Simplified code:
 ----------------
	register signal handlers				// for SIG{HUP,TERM,USR1,USR2}, see below
	initialize verbosity system       	// all verbosity mediated by verbose(), see RADclockDoc_verbose
	initialize configuration system   	// includes command line parsing, see RADclockDoc_config

	clock_handle = create_handle(conf, is_daemon); // create global handle variable
	handle = clock_handle;                         // make local copy of pointer
	set_logger((logger_verbose_bridge))	// initialize logging
	daemonize(,)								// fork/exit check unique, open logfiles, close stdin etc

	if (!is_live_source(handle))   		// depends on selected inputs recorded in handle->conf
		handle->run_mode = RADCLOCK_SYNC_DEAD;
	else
		handle->run_mode = RADCLOCK_SYNC_LIVE;
		clock_init_live(clock)				// find KV, initialize kernel related clock members

	init_handle(handle);						// creates data source, and SHM, fully initialize verbosity

	if RADCLOCK_SYNC_DEAD
		init_peer_stamp_queue(&peer);		// init radclock algo data
	   while (1)
			err = process_stamp			// consume data until no more
	   }
	else // RADCLOCK_SYNC_LIVE
		while (starting or restarting)
		  err = start_live(handle)   		// starts/joins threads, infinite data capture loop
		  if err==0 							// exit from start_live due to SIGHUP,
		  		rehash_daemon()		      // process param file updates ready for restart
	 	}

	verbosity
	cleanup
 ----------------

The critical configuration parameter here is handle->run_mode.
If running live, radclock can run both as a deamon or not, but this is less important.
The only real difference is the existence of a controlling terminal, and the location
and naming of log files.

The key function regarding threads is start_live. It starts up the threads using
the start_thread_{DATA_PROC,,,,} functions defined in pthread_mgr.c, and calls
	rawdata.c:capture_raw_data,
which calls pcap_loop(), thereby entering an infinite rawdata production
loop under the control of MAIN. Meanwhile DATA_PROC is now active. It calls rawdata.c:process_stamp
in an infinite loop to consume raw data as it becomes available.

If in infinite loop in start_live exits due to an error or SIGTERM signal (exit code err=1),
then it soft-signals the threads to terminate themselves using the  handle->pthread_flag_stop
flags, and then uses pthread_join() on each of them so they return to MAIN on death in sequence.
Control then returns to main and RADclock exits.

If start_live exits due to a SIGHUP (exit code err=0), this signals that RADclock
should accept new parameter setting and continue on or `restart`.
In that case PROC_DATA will be kept alive as it contains the RADclock clock history.
However it is best to terminate the other threads, as before, to enable a clean re-start.
On exit rehash_daemon() reparses the configuration file and manages any updates,
fixing any incompatibilities. Control then passes back to start_live which restarts
the other threads and reenters the data production loop.

Both SIGTERM and SIGHUP are caught by main.c:signal_handler, which simply calls
source_breakloop (mapped to livepcapstamp_breakloop in this case) which is a wrapper
for pcap_breakloop who actually handles the signals, causing pcap_loop and hence
start_live to exit.  A more complete description of all UNIX signal handling is
given below.



==============================================================
Threading and mutexes
============================

A standardized interface for thread implementation is POSIX Threads (Pthreads),
which is a set of C-function library calls.

A complete list of the calls used by RADclock (declared in thread.h) is:

// Management: creation
int pthread_create					(pthread_t*, const pthread_attr_t, funcptr,(void *)otherparam)
int pthread_join						(pthread_t , void **)  // block on thread until it terminates
int pthread_exit						(void *)					  // does not release related mutexes
// Management: attribute control
int pthread_attr_init				(pthread_attr_t*)
int pthread_attr_setdetachstate	(pthread_attr_t*, int)
int pthread_attr_getschedparam	(const pthread_attr_t*, struct sched_param*)
int pthread_attr_setschedparam	(pthread_attr_t*, const struct sched_param*)
int pthread_attr_setschedpolicy	(pthread_attr_t*, int)
// Condition threads
int pthread_cond_init 			(pthread_cond_t*, pthread_condattr_t)
int pthread_cond_signal       (pthread_cond_t*, pthread_t)
int pthread_cond_wait			(pthread_cond_t*, pthread_mutex_t* )  // unlocks mutex during blocking wait, locks when wakes up on signal !
int pthread_cond_destroy		(pthread_cond_t*);
// Mutexes
int pthread_mutex_init			(pthread_mutex_t*, const pthread_mutexattr_t* )
int pthread_mutex_destroy		(pthread_mutex_t*)
int pthread_mutex_lock			(pthread_mutex_t*) 			// is a blocking function
int pthread_mutex_unlock		(pthread_mutex_t*)


A complete list of thread related global variables and mutexes defined in RADclock is:

/* Thread-related part of radclock_handle */
pthread_t threads[8];				// dont ask what these really are, threads are an opaque type !
int	pthread_flag_stop;			// used to signal thread(s) to stop
pthread_mutex_t globaldata_mutex;// protects handle->rad_data
int   wakeup_checkfordata;			// used to weakly signal between TRIGGER and DATA_PROC
pthread_mutex_t wakeup_mutex;		// protects wakeup_cond signalling
pthread_cond_t  wakeup_cond;		// used to strongly signal between TRIGGER and DATA_PROC

/* All mutexes in code */
handle->{globaldata,wakeup}_mutex			// direct members of *clock_handle
handle->{pcap,ieee1588eq}_queue->rdb_mutex// inside raw_data_queue members
verbose_data.vmutex 								// external defn verbose.c;  extern decl everywhere
alarm_mutex											// external defn pthread_trigger.c; decl client_ntp.c


The following table maps out mutex usage, giving what the mutexes are used to protect,
the functions they are used in, and within which threads.

mutex    			Data it protects      	Thread,  	fns involved,  	notes
------------------------------------------------------------------------------------------------
	 wakeup_mutex  handle->wakeup_cond		MAIN			pthread_mutex_{init,destroy}
					 handle->wakeup_checkfordata	TRIGGER		thread_trigger(), 		pthread_cond_signal
													 	DATA_PROC	thread_data_processing, pthread_cond_wait

globaldata_mutex  handle->rad_data 			DATA_PROC	updates at end of sync_bidir()
														VM_UDP_SERV updates in receive_vm_udp()
																		reads   in thread_vm_udp_server()

	    rdb_mutex	rdbs in rdb queues		MAIN			insert_rdb_in_list(),  protects insertion
		 												DATA_PROC	free_and_cherrypick(), protects freeing

			 vmutex  verbose_data		  		All			only verbose(), but called everywhere
			 			daemon log file

	  alarm_mutex	alarm_cwait				   MAIN		catch_alarm(), protects pthread_cond_signal()
													 	TRIGGER	ntp_client(),  protects pthread_cond_wait()
------------------------------------------------------------------------------------------------

We now expand on each of the above.

#  rdb_mutex: Safety of raw data access between MAIN (insertion), and DATA_PROC (freeing)
This is the most important example in that it involves, in a sense, the bulk of RADclock code
including the algo.  On the other hand it is simple because it does not involve actual
signaling, only asynchronous data protection.
A simplified flow of execution from the dual top level infinite loops,
toward the low level rdb insertion and freeing functions is given below.
The rd queue, protected by rdb_mutex, is the data structure which
effectively allows the two threads to interact (see RADclockDoc_stamps).


Source File 			   	Function                     		Returns         Comment
--------------------------------------------------------------------------------
pthread_mgr.c 				  thread_data_processing 									 [ infinite loop ]
pthread_dataproc.c     		process_stamp							rad_data			 [ calls algo ]
stampinput.c   			    get_next_stamp						stamp           [ fn pointer ]
stampinput-livepcap.c	     livepcapstamp_get_next			stamp           [ wrapper ]
create_stamp.c 					get_network_stamp					stamp				 [ generic taking callback ]
stampinput-livepcap.c		  		get_packet_livepcap			radpcap_packet_t [writes to .raw]
rawdata.c									deliver_rawdata_pcap		radpcap_packet_t, vcount
rawdata.c										free_and_cherrypick  rdb

Consume data from rd queue ^  	[DATA_PROC thread]
============================================================================================
Produce live data to rd queue v	[MAIN thread]

rawdata.c										insert_rdb_in_list	rdb
rawdata.c									fill_rawdata_pcap			rdb				[ pcap_loop callback ]
rawdata.c 								pcap_loop 						pcap_hdr, pkt  [ infinite loop ]
rawdata.c							capture_raw_data
radclock_main.c 				  start_live
radclock_main.c 				 main

In the case of dead input there is no raw data capture at all, and so nothing below
the level of get_packet_livepcap above runs, and there is no rd queue.
For example in the case of raw tracefile input, get_packet_tracefile is called instead
of get_packet_livepcap, which obtains the packet data directly at the radpcap_packet_t level,
so that process_stamp, which is now called by main, drives both the data production
and consumption.


#  wakeup_mutex:	Coord''n of raw data prod''n in TRIGGER with its consumption in DATA_PROC
The problem is how to tell DATA_PROC that new data is available without polling somehow.
It is TRIGGER that has the inside information, since it owns the socket that
sent the packet and hence knows when packets return from it, or should have.
One expects that when this userland return has occurred, pcap has also seen it in
the kernel and passed its copy to MAIN, who has already inserted it into the rd queue.
The idea then is for TRIGGER to signal DATA_PROC when it has seen data, and then to check that
DATA_PROC has picked it up.  The latter is achieved by toggling handle->wakeup_checkfordata .
The signaling occurs within the core infinite while loops in thread_trigger and
thread_data_processing (back to back in pthread_mgr.c).
A partial flow of control on the TRIGGER side is:

pthread_mgr.c		 thread_trigger  [infinite loop, sig to thread_data_processing infinite loop]
pthread_trigger.c	   trigger_work  [ just calls the mode and source specific work function ]
client_ntp.c			  ntp_client  [ if SYNCTYPE_NTP: tries to get a single valid stamp  ]
pthread_trigger.c		  dummy_client[ if SYNCTYPE_PIGGY: just sleeps ]

More details below.


#	 globaldata_mutex:  Safety of access of RADclock clock data handle->rad_data
This provides locking to prevent simultaneous access of global clock data.

#	 vmutex:  Safety of access of verbose_data and log files written to by verbose.
Since verbose() is available everywhere and is used by all threads, the risk is high.

#  alarm_mutex:	Passing of a SIGALRM caught in MAIN to TRIGGER for action.
See Unix Signalling below for more details.




==============================================================
(Unix) Signaling
============================

The only signals involved are the following:
	SIGALRM: [alarm clock timer expired]			Need periodic timers for spy_loop and ntp_client
	SIGHUP:	[controlling terminal closed]			Used to trigger config file reload & restart
	SIGTERM:	[request termination or process] 	Used to shutdown RADclock and terminate.
	SIGUSR1:	[user defined  							Used to signal closing a logfile.
	SIGUSR2:	[user defined]  							Just a placeholder currently.

The catching of SIGHUP and SIGTERM signals (only) are recorded in handle->unix_signal .

Signals are sent to a process, and could potentially be received by any thread.
For simplicity, signals are blocked using pthread_sigmask(,,) at the creation of
each thread, so that all signals will be caught by MAIN.  This is achieved by a call to
pthread_mgr.c:init_thread_signal_mgt  at the start of each start_thread_{,,,,} function.
Thus all of the defined signal callback functions mentioned below:
	signal_handler  	for HUP, TERM, USR1, USR2
	fill_rawdata_spy  for ALRM
	catch_alarm			for ALRM
are caught by MAIN, even if they were registered and set up in other threads.


Code related to signals appears in the following functions only :

radclock_main.c :
  main
  		SIG{HUP,TERM,USR1,USR2}		// sigaction registration near start
  signal_handler()
  		HUP:		set handle->unix_signal = SIGHUP	;   source_breakloop (pass to pcap_breakloop)
		TERM:		set handle->unix_signal = SIGTERM;   source_breakloop (pass to pcap_breakloop)
		USR1:		; close verbose logfile (under vmutex protection like all verbosity)
		USR2: 	; placeholder
  start_live()
  		TERM:		set if get err=-1 from capture_raw_data (abuse since not necc a real SIGTERM)
		HUP:		clear stop flag for DATA_PROC, and avoid joining on it
		
pthread_mgr.c : init_thread_signal_mgt()
		Block signals, called by all start_thread_{,,,,} functions

rawdata.c : spy_loop()
		HUP,TERM:	exits on either
		ALRM:			get periodic timestamps
						handler:  fill_rawdata_spy(int sig)

client_ntp.c : ntp_client{_init}():
		ALRM:			NTP pkt sender timer
						handler:  pthread_trigger.c:catch_alarm(int sig)


Note that SIGALRM signals are sent by the system via the alarm timer functions.
These are defined in pthread_trigger.c :
	{set,assess}_ptimer() ;  using timer_{settime,gettime,create}	// if  HAVE_POSIX_TIMER
	{set,assess}_itimer() ;  using {set,get}itimer  					// if !HAVE_POSIX_TIMER
The assess_ calls are only used by ntp_client().

These timing calls are set up in the appropriate thread (MAIN,TRIGGER) for
(spy_loop,ntp_client) but the signals are still caught by MAIN as above.


All the uses of signals above concern the MAIN thread with the exception of
the periodic alarm used by ntp_client under the TRIGGER thread.
Hence the callback, catch_alarm(), called under MAIN, must communicate back to TRIGGER.
It does this as follows :
	 void catch_alarm(int sig)
	 {
		 pthread_mutex_lock(&alarm_mutex);
		 pthread_cond_signal(&alarm_cwait);
		 pthread_mutex_unlock(&alarm_mutex);
	 }
where the signal is caught by matching alarm_mutex protected wait code in ntp_client() :
	pthread_cond_wait(&alarm_cwait, &alarm_mutex);
where alarm_{cwait,mutex} are defined in pthread_trigger.c .




==============================================================
Trigger Thread Detail
============================

The original idea of TRIGGER was that it would be a dumb packet sender only, with some
retry intelligence only, completely independent of PROC.  However for efficiency reasons,
this evolved into a much tighter coordination, where they takes turns. The benefit is that
each can sleep most of the time, and that in the common case (if timeouts and retries are
calibrated appropriately), the process is simple:
  TRIGGER
    - sleeps waiting for next grid point alarm,
    - wakes on alarm, creates and sends a response
    - wait a bit to try to get a reply, succeeds, typically it matches
    - tells PROC it is finished this this grid point  (successfully or not)
  PROC
    - wakes up, tries to extract data from the queue
    - processes it, typically it will see the new full stamp available, will get it
    - tries to get more, fails  [ this part is inefficient: has to fail many times]
    - hands control back to TRIGGER, blocks
A third benefit is that per-grid point actions, like starvation checking, can be performed
in PROC where they are more natural, without running the risk of being performed very late
in a batch if PROC stayed blocked until a new stamp arrived.
The downside is that they do not run truly independently.
However they run independently enough to ensure robustness, if their coordination fails,
nothing too bad will happen, no packets will be lost. The worst that can happen is
that a stamp which is ready but slightly delayed may now be delayed by 1 grid point
before reaching the algo.

Details:  there is a division of work between TRIGGER and DATA_PROC.
When TRIGGER is performing its work, including waiting (within ntp_client) for the
next grid point alarm so it may attempt to send the next NTP request, it holds
wakeup_mutex until it is finished dealing with the current grid point (successfully or not).
After releasing it, it enters a signal-then-sleep loop until wakeup_checkfordata is released.
Conversely, when DATA_PROC hold the lock it attempts to consume data (within
process_stamp), and will not stop until there is no data left.
Thus is is normal for process_stamp to complain about a lack of data regularly, and TRIGGER
cannot generate new data until after process_stamp has given up and
wakeup_checkfordata is released.
In fact, DATA_PROC holds the lock essentially all the time.
The toggle at the end of the while does allow TRIGGER to regain control.
It also releases it within the pthread_cond_wait call, allowing TRIGGER (who
has advanced from the signal-then-sleep loop at that point, and is blocking on
pthread_mutex_lock, to proceed.














==============================================================
DATA_PROC Thread Detail
============================

==============================================================
NTP_SERV Thread Detail
============================

==============================================================
VM_UDP_SERV Thread Detail
============================

//
//  radclock_mapout.c   version 0.3.3
//  
//
//  Created by Darryl Veitch on 21/02/2017.
//
//


# In gitrepo/radclock:

-rw-r--r--   1 darryl  admin  17224  2 Dec 16:38:13 2013 configure.in
-rw-r--r--   1 darryl  admin    134  2 Dec 16:38:13 2013 README
-rw-r--r--   1 darryl  admin     79  2 Dec 16:38:13 2013 NEWS
-rw-r--r--   1 darryl  admin   3544  2 Dec 16:38:13 2013 Makefile.am
-rw-r--r--   1 darryl  admin   4886  2 Dec 16:38:13 2013 ChangeLog
-rw-r--r--   1 darryl  admin   1269  2 Dec 16:38:13 2013 COPYING
-rw-r--r--   1 darryl  admin   2520  2 Dec 16:38:13 2013 AUTHORS
-rwxr-xr-x   1 darryl  admin    156  2 Dec 16:38:14 2013 version.sh*
drwxr-xr-x   5 darryl  admin    170 29 Nov 16:27:13 2015 tests/
drwxr-xr-x   4 darryl  admin    136 29 Nov 16:27:13 2015 redhat/
drwxr-xr-x  38 darryl  admin   1292 29 Nov 16:27:13 2015 radclock/
drwxr-xr-x   5 darryl  admin    170 29 Nov 16:27:13 2015 python-module/
drwxr-xr-x  18 darryl  admin    612 29 Nov 16:27:13 2015 python-gui/
drwxr-xr-x   5 darryl  admin    170 29 Nov 16:27:13 2015 man/
drwxr-xr-x  24 darryl  admin    816 29 Nov 16:27:13 2015 libradclock/
drwxr-xr-x   6 darryl  admin    204 29 Nov 16:27:13 2015 kernel/
drwxr-xr-x   6 darryl  admin    204 29 Nov 16:27:13 2015 examples/
drwxr-xr-x  11 darryl  admin    374 29 Nov 16:27:13 2015 debian/
drwxr-xr-x  20 darryl  admin    680 29 Nov 16:27:13 2015 attic/
drwxr-xr-x  13 darryl  admin    442 29 Nov 16:27:13 2015 .git/



########################################################################
# In radclock, .h files

-rw-r--r--  1 darryl  admin   8572  2 Dec  2013 config_mgr.h
-rw-r--r--  1 darryl  admin   4111  2 Dec  2013 create_stamp.h
-rw-r--r--  1 darryl  admin   6003  2 Dec  2013 fixedpoint.h
-rw-r--r--  1 darryl  admin   4227  2 Dec  2013 jdebug.h
-rw-r--r--  1 darryl  admin   2227  2 Dec  2013 misc.h
-rw-r--r--  1 darryl  admin   1722  2 Dec  2013 ntohll.h
-rw-r--r--  1 darryl  admin   6921  2 Dec  2013 proto_ntp.h
-rw-r--r--  1 darryl  admin   2899  2 Dec  2013 pthread_mgr.h
-rw-r--r--  1 darryl  admin   5612  2 Dec  2013 radclock_daemon.h
-rw-r--r--  1 darryl  admin   3424  2 Dec  2013 rawdata.h
-rw-r--r--  1 darryl  admin   2777  2 Dec  2013 stampinput.h
-rw-r--r--  1 darryl  admin   2556  2 Dec  2013 stampinput_int.h
-rw-r--r--  1 darryl  admin   1768  2 Dec  2013 stampoutput.h
-rw-r--r--  1 darryl  admin  10984  2 Dec  2013 sync_algo.h
-rw-r--r--  1 darryl  admin   2565  2 Dec  2013 sync_history.h
-rw-r--r--  1 darryl  admin   2418  2 Dec  2013 verbose.h


## jdebug.h		[ no jdebug.c,  all done with macros here, separate to verbose system ]

#ifdef WITH_JDEBUG     [ where is this defined? seems outside radclock config stuff ]
  extern struct radclock_handle *clock_handle; // seems corrected from struct radclock earlier

  static inline char *pthread_id()   // useful summary of threads, why is this fn not in thread code?
	 - BUG: VM thread missing


  // externs defined in main to support macros:
  extern struct rusage jdbg_rusage;		// tracks resource usage
  long int jdbg_memuse = 0;					// tracks total memory use I think

  // Defines big printout macros:
  JDEBUG      fprintf(stdout,"format", pthread_id(), __FILE__, __LINE__, __FUNCTION__)  // ~63 times
    - purpose?  just to track if fn is being entered, and by expected thread?
  JDEBUG_STR(_format,...)   same, + extra args and format for them   // used twice only
  JDEBUG_RUSAGE( , )  + some kind of system resource use stats, relies on jdbg_rusage. // used once

   // I think this is his method to track memory leaks
  JDEBUG_MEMORY(_op, _x)    // version for __FreeBSD__ and other
      same + malloc memory info:       // used 62 times
		  _op=JDBG_MALLOC  gives size of x, and increments total ever malloc'd' // 28 times
		     =JDBG_FREE          "     "   ,     decrements     "      "          // 34 times
#else
   define them as blank names to allow compilation, since macros are always in
	source code, this switches the system off at no cost once compiled.
#endif




## verbose.h     [ note no mention of JDEBUG ]

#define DAEMON_LOG_FILE	"/var/log/radclock.log"    // default if running live
#define BIN_LOG_FILE	"radclock.log"               // default if running dead

Defines standard syslog (in syslog.h)  (for daemon?) and radclock logging/verbosity codes:
	LOG_{EMERG,ALERT,ERR,WARNING,NOTICE} = {0,1,2,3,4,5}  // used {0,2,133,68,66} times
	VERB_{DEFAULT,QUALITY,CAUSALITY,SANITY,CONTROL,SYNC,DEBUG} = {10,11,12,13,14,15, 20}

struct verbose_data_t {
	struct radclock_handle *handle;
	int is_daemon;
	int is_initialized; 		// marks when radclock initialized? ie when running live
	int verbose_level;      // overall level to control which VERB_ messages printed
	char logfile[250];
	FILE* fd;
	pthread_mutex_t vmutex;
};


// declares verbose data, and all verbose fns, as externs so all who include this can use
extern struct verbose_data_t verbose_data;
extern void verbose(int facility, const char* format, ...);
extern void   set_verbose(struct radclock_handle *handle, int verbose_level,int initialized);
extern void unset_verbose();
extern int    get_verbose_level();   // only ever used via VERB_LEVEL
#define VERB_LEVEL get_verbose_level()   
  - was only used twice, in pthread_dataproc.c:{update_FBclock,process_stamp}
  - now I used in insertandmatch_halfstamp
  - not worth it, should remove probably



## verbose.c

/* Global seen my main for access to mutex */
// I think is saying that is declared as extern at top of main, hence acts as a global there
// also decl as extern in verbose.h, so will end up everywhere anyway, not a BUG, but should remove.
struct verbose_data_t verbose_data;	 // external defined here, used in verbose.c, radclock_main.c

void set_verbose(struct radclock_handle *handle, int verbose_level, int initialized)
  - full init routine, called by main
  - learns about deamon from handle, obviously important to know..
  - sets logfiles names using DAEMON_LOG_FILE or BIN_LOG_FILE if no value in handle
  - Doesnt set the logfile pointer
void unset_verbose()
  - called by main, resets most things, but
  - BUG:  why not logfile or is_initialized ?
int get_verbose_level()
	{	return verbose_data.verbose_level;  }    // only ever used via VERB_LEVEL
  - hmm, need this to condition code on verb level, but virtually not used, results in wasted
    code as only the printing out (via verbose()) actually acting on the verb level, eg at
	 end of insert_stamp_queue (now insertandmatch_halfstamp) - I changed it, works.
  - Aah! but can access verbose_data.verbose_level directly, dont need VERB_LEVEL to get it, and this is what is done in verbose() itself and in main!
static void verbose_open()
  - this is the only way that the file can be opened and verbose_data.fd set
  - set_verbose does not set fd !
  - only used by verbose(), uses different file opening code depending on is_deamon

static const char *months[12] = {"Jan", "Feb", "Mar", ....,  "Aug", "Sep", "Oct", "Nov", "Dec"};
  - used in verbose() only


// Main fn called everywhere
void verbose(int facility, const char* format, ...)
  - format wierdly named, as for radclock this is just the message you pass, but it can have
  formatting commands: this follows syslog(priority, *format) convention
  - are the optional extra args (needed if format has variables) ever used??

  - almost all of fn protected by verbose_data.vmutex, because others may write to daemon log file?
    It does access handle members, but only for read, but maybe want to protect all reads to
	 handle->rad_data

  - if file pointer member not defined, then does so via verbose_open().
    Note set_verbose does not do this!
  - allocates debug memory `str` used to take the passed string populated by vsnprintf() I think
  - black magic is obtaining enough memory to write message to str variable
  - strange  <stdarg.m>, allowing I think 	va_list arg;  definition and vsnprintf,
        both only ever included and used here and in logger.c:logger().
		  Seems to be about populating str in a very careful way.
		  Lots of JDEBUG here, obviously this writting to log files is harder than it seems, and had to pull in this stdarg technology to get it working safely

  - maps LOG_ and VERB_ numbers to "customize" string, eg LOG_ERR -> "ERROR: ", VERB_DEBUG -> "Debug: "
  - populates prepend string in ctime_buf, string is:
	 - RADclock Init-  before radclock started  (eg during config file read)
	 - Once initialized, if live, puts system data and time stamp
	 - if dead, put "Replay"

 Basic output string is:
 	fprintf(verbose_data.fd, "%s: %s%s\n", ctime_buf, "customize", str);
 Switch on facility argument :
   VERB_DEBUG				suppressed if verbose_level=0,1   ; falls back to stderr if file fails
   VERB_anything else        "                 "       0   (but VERB_DEFAULT has no customize str)
   default   not controlled, allows anything in ignoring verbose_level, also no customize str;
	  - goal was to catch the LOG_{ERR,WARNING,NOTICE} values in one hit
	  - but also allows anything else in, and passes to logfile regardless of verbose_level
	  - sends to syslog if daemon, else stderr
	  - could use to add adhoc debugging comments instead of generic fprintf, but you can aleady do that by using VERB_DEFAULT and verbose_level>0  .

Confusion:  VERB_DEBUG has nothing to do with JDEBUG system
   VERB_DEBUG:  just a level in normal verbosity system for radclock running
	JDEBUG:      His actual macro based system to debug code (esp memory leaks), all sent to stdout.

WATCHOUT: - if verbose_level set low during a live run, then even if set higher during replay,
		      cant of course get higher level verbosity for code below rd level.
          - VERB_DEBUG set up as a `finer detail` setting as it should be, but he
			   has also put some `impossible events` in this category !  they should
				be at the MOST verbose end, since if they happen, sth wrong, yet rare, so
				no downside to always reporting them. Should be LOG_{WARNING,ERR}
				depending on whether impact thought to be {0,high/unknown}, since these will bypass the verbose level.

// In main have configuration hack I dont understand:
  	/* Little hack to deal with parsing of long options in the command line */
	if (conf->verbose_level > 0)
		SET_UPDATE(param_mask, UPDMASK_VERBOSE);

// also look in update_data for some verbose setting/checking




########## from radclock library

The logger system is the library version of verbosity, which is hidden to users.
It support three message types:
	logger(RADLOG_{ERR,WARNING,NOTICE}, msg)
which are mapped behind the scenes by logger_verbose_bridge() to
	verbose(LOG_{ERR,WARNING,NOTICE}, msg)

The library doesnt need to know about the internal algo related VERB_ types.

## logger.h   // this is verbatim, nothing else in the file
 - included in most libradclock/.c files
 - not used in radclock files except in main, as it sets up logging and
   defines  logger_verbose_bridge which gets mapped to logger()

 
#define RADLOG_ERR 			3
#define RADLOG_WARNING 		4
#define RADLOG_NOTICE 		5

void logger(int level, char *fmt, ...);

int set_logger(void (*logger_funcp)(int level, char *message));
//=========================================


## logger.c 
#define MAX_MSG 1024
static void (*priv_logger)(int level, char *message);

static void default_log(int level, char *message)
	{ fprintf(stderr, "[libradclock] %s\n", message);}
   - basic err log format

int set_logger(void (*logger_funcp)(int level, char *message))
 - sets generic logging fn name to the one given in the argument

void logger(int level, char *fmt, ...)
{
	char buf[MAX_MSG];
	va_list arg;
	va_start(arg, fmt);
	vsnprintf(buf, MAX_MSG, fmt, arg);
	va_end(arg);
	if (priv_logger)
		priv_logger(level, buf);
	else
		default_log(level, buf);
}
  - wrapper around calling the simple default_log printout, or a private one if available.




##  Initialization of system:  from radclock_main.c:

static void logger_verbose_bridge(int level, char *msg)
    Used only here in this file (not in any .h).
    Looks like it implements logging by using verbose(,)
	 Set to generic logger name by set_logger(logger_verbose_bridge)

 Complete map of verbose and logger usage:
 In main.c above main :
   - #include <syslog.h> ,  "logger.h"
	- local fn logger_verbose_bridge() defined
	- daemonize() defined
	   - uses syslog fns:  setlogmask, openlog, syslog
	- init_handle() defined
	   - set_verbose(handle, handle->conf->verbose_level, 1);  //initializes verbose system fully
		- set_logger(logger_verbose_bridge);   // makes priv_logger() point to logger_verbose_bridge, that way library fn logger() can access it
   - note VERB_ not used anywhere in file as it is algo specific, only LOG_


 In main itself
   - very early (just after signals) all members of verbose_data manually initialized to trivial values
	  BUG:  is_initialized is left out!  should be set to 0? 1?  something!
   - little hack to update options mask for verbose_level if needed.
   - first use of verbose a bit later:
	  verbose(LOG_ERR, "Could not create clock handle");
	    - Execution within verbose given only trivial init so far:
		   - notices fd not set, so calls verbose_open to open file and set .fd
			  - verbose_open
			        is_deamon=0 which is correct as not yet deamonized
					  open with 'w' permission, appropriate for first time opened
					  But logfile member still NULL, so will fail !
		     - if fd still NULL, will output to stderr instead
			  - but if a LOG_ message, writes to fd if can, but always writes using syslog()
			    if live, else stderr.


	- set_verbose(handle, handle->conf->verbose_level, 0);  // not trivial init
	     - this Will set a logfile, even if only a default one
	  set_logger(logger_verbose_bridge);  // initialize so logger() works properly
   - next call to verbose will again call verbose_open, but this time will work
   - daemonize and parse conf file
	- set_verbose(handle, handle->conf->verbose_level, 0);  // still is_initialized=0 !
	  set_logger(logger_verbose_bridge);   //  I guess do again since logfile name could change
   - init_handle()    //
	    - calls set_verbose yet again, but finally with is_initialized=1
		 - calls set_logger yet again, why??

   .....
   - closelog()
	- unset_verbose

   TODO:  streamline this to avoid 4 different initializations!








########################################################################

## radclock_daemon.h

- daemon refers to the whole RADclock process, also known as `RADclock` !
  So this file is I think what may need to be shared to other processes about the daemon.
  Hence it defines the key global variable type (struct radclock_handle) and other
  structure enabling understanding of thread level parameters, like:
    - runmode type
	 - struct radclock_ntp_{client,server}
	 - struct radclock_vm
	 - struct radclock_handle  + convenience macros to manipulate its members
  since high level calls tend to use these structure as the key (or only) parameters,
  so need to understand them.  So this is really about understanding the types used
  in IPC communication.

- defines VC_FMT just like in radclock.h, apparently this is the right location
- main purpose is defn of struct radclock_handle, the types it includes, and macros for fast acces
    - is the central point to get any data you want about radclock
    - includes ptr to stamp source variable, holding input data ptr and fnpointers
      void *stamp_source; /* Defined as void* since not part of the library */
    - how does compiler know about all these structures?  assuming other .h included already?

- also vm stuff
- note that struct radclock defined in libradclock/radclock_private.h, not here

- HISTORY:
    late change renaming of ntp sending stuff to be more generic client_data --> ntp_client etc
      this achieves a TODO
    late change replacing rdbs with a new kind of struct raw_data_queue  (inspired by need
      to be general enough to cover 1588? ), makes the global rdb_mutex obsolete as well.

#define NTP_CLIENT(x) (x->ntp_client)
#define RAD_DATA(x) (&(x->rad_data))  //  etc


// many of these structures defined in radclock-private.h
// Being a central info point, has both internal and library stuff.
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


********HISTORY:  was this before
	/* Protocol related stuff on the client side (NTP, 1588, ...) */
	struct radclock_client_data *client_data;
	/* Protocol related stuff (NTP, 1588, ...) */
	struct radclock_ntpserver_data *server_data;
	/* Raw data capture buffer */
	struct raw_data_bundle *rdb_start;
	struct raw_data_bundle *rdb_end;
    pthread_mutex_t rdb_mutex;	// XXX arbiter between insert_rdb_in_list
								// and free_and_cherry_pick
   								// XXX Should not need a lock, but there is
								// XXX quite some messing around if hammering
								// XXX NTP control packets
*********

	/* Common data for the daemon */
	int is_daemon;
	radclock_runmode_t 		run_mode;

	/* UNIX signals */
	unsigned int unix_signal;

	/* Output file descriptors */
	FILE* stampout_fd;
	FILE* matout_fd;

	/* Threads */
	pthread_t threads[8];		/* TODO: quite ugly implementation ... */
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

	/* Stamp source */   // Dont really understand this..
	void *stamp_source; /* Defined as void* since not part of the library */

	/* Synchronisation Peers. Peers are of different nature (bidir, oneway) will
	 * cast
	 */
	void *active_peer;  // Pointer to the stamp queue, I think
	
};


ntp pkt sender stuff   [ sender code was in pthread_trigger.c ]
 - needs to keep info from outgoing client side and incoming server side to match
 - late change here:  old names were radclock_{client,ntpserver}_data
    which was bad as wasnt clear were talking about ntp pkt generation (still unclear
    in fact, still ambiguous wrt the radclock server )  but are these names inspired
    by ntpd?

struct radclock_ntp_client {
                int socket;
                struct sockaddr_in s_to;
                struct sockaddr_in s_from;
};
struct radclock_ntp_server {
                int burst;
                uint32_t refid;
                unsigned int stratum;
                double serverdelay;     /* RTThat to the server we sync to */
                double rootdelay;       /* Cumulative RTThat from top of stratum hierarchy */
                double rootdispersion;/* Cumulative clock error from top of stratum hierarchy */
};




## create_stamp.h
- defines bpf and radpcap pkt/buffer sizes;  some useful though obscure comments
- some PPS support

The following are almost the only typedefs in the entire code:
typedef struct linux_sll_header_t
  - defines linux_sll_header used to create modified frame header for raw saving

typedef struct radpcap_packet_t {
    void *header;	/* Modified BPF header with vcount */
    void *payload;  /* Packet payload (ethernet) */
    void *buffer;
    size_t size;	/* Captured size */
    u_int32_t type;	/* rt protocol type */
    struct sockaddr_storage ss_if;	/* Capture interface IP address */
} radpcap_packet_t;

get_network_stamp(*handle, userdata, get_packet( , , ),
                  struct stamp_t *stamp, struct timeref_stats *stats);
	- declared here so .h can be included where this fn needed?



## stampinput_int.h        int = ?    internal?
- somewhat obscure:  only defines two structures:
    - stampsource, which hasnt got much in it, only:
        - pointer to `private data`, whatever that is
        - stampsource_def variable: just way to access `method` fns
        - some stats
    - stampsource_def; defines 6 fnpointers, all having a stampsource `self` parameter!
        - init, get_next_stamp, source_breakloop, destroy, update_{filter,dumpout}
- also defines #define INPUT_OPS(x) x->def, used only in stampoutput.c
- all files that include this also include stampinput.h


### stampinput.h
- bad name, primarily about stamp sources, not stamps themselves, except
  int get_next_stamp(*handle, struct stampsource *source,struct stamp_t *stamp);
- declares most of the functions with names similar to the fnptr types declared in stampsource_def
- new fn is     int is_live_source(struct radclock_handle *handle);
  used only in radclock_main.c and stampinput.c
    - this is the key function that determines the running mode given input params !!
- only fns that include this but notstampinput_int.h  are
  pthread_{mgr,rawdata},  why bother separating?  what are we hiding and why?
  but this file doesnt include stampinput_int, legal syntax?






## sync_algo.h   [ here focus on non-algo aspects ]

typedef enum {
    STAMP_UNKNOWN,
    STAMP_SPY,
    STAMP_NTP,		/* Handed by libpcap */
    STAMP_PPS,
} stamp_type_t;

/* struct holding info extracted from captured data, NTP centric */
struct stamp_t {
    stamp_type_t type;
    int qual_warning;	/* warning: route or server changes, server problem */
    uint64_t id;        // what is this?
    char server_ipaddr[INET6_ADDRSTRLEN];
    int ttl;
    int stratum;
    int leapsec;
    uint32_t refid;
    double rootdelay;
    double rootdispersion;
    union stamp_u {
        struct unidir_stamp ustamp;   /* this is just a counter */
        struct bidir_stamp  bstamp;   /* traditional algo fourtuple */
    } st;
};

#define UST(x) (&((x)->st.ustamp))
#define BST(x) (&((x)->st.bstamp))

void init_peer_stamp_queue(struct bidir_peer *peer);
void destroy_peer_stamp_queue(struct bidir_peer *peer);



## rawdata.h

// mirrored stamp_type_t from sync_algo.h, with s/RD_TYPE/STAMP/, except 1588
typedef enum { 
	RD_UNKNOWN,
	RD_TYPE_SPY,
	RD_TYPE_NTP,		/* Handed by libpcap */
	RD_TYPE_1588,
	RD_TYPE_PPS,
} rawdata_type_t;


 Actual data structures needed for different cases
    rd_spy_stamp:  use this for Spy capture mode
    rd_pcap_pkt:   use for libpcap oriented capture mode (NTP and PIGGY) //used to be rd_ntp_pkt
    raw_data_bundle:   holds all pkt data and some locking info
    raw_data_queue:    holds rdb queue stuff  // HISTORY: members were directly in handle before

  int capture_raw_data(struct radclock_handle *handle);
  int deliver_rawdata_pcap(*handle,struct radpcap_packet_t *pkt, .. *vcount);
  int deliver_rawdata_spy(struct radclock_handle *handle, struct stamp_t *stamp);

#define RD_PKT(x) (&((x)->rd.rd_pkt))



## misc.h

# used to be in sync_algo.h
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

static inline void UTCld_to_timeval(long double *time, struct timeval *tv)
/* Subtract two timeval */
static inline void subtract_tv(struct timeval *delta, struct timeval tv1, struct timeval tv2)








########################################################################
########################################################################
########################################################################

## In libradclock, .h files
-rw-r--r--  1 darryl  admin   3579  2 Dec  2013 libradclock/kclock.h
-rw-r--r--  1 darryl  admin   1046  2 Dec  2013 libradclock/local-genetlink.h
-rw-r--r--  1 darryl  admin   1704  2 Dec  2013 libradclock/logger.h   // see later
-rw-r--r--  1 darryl  admin   5152  2 Dec  2013 libradclock/radclock-private.h
-rw-r--r--  1 darryl  admin   2717  2 Dec  2013 libradclock/radclock-timepps.h
-rw-r--r--  1 darryl  admin  14248  2 Dec  2013 libradclock/radclock.h



# radclock-timepps.h
Seems to be RADclock versions of existing standard functions.

## radclock.h
#ifndef _RADCLOCK_H

- defines RADclock stampinput_int
    - includes only time.h, pcap.h, stdint.h
    - unusually verbose comments for fn definitions! but not perfect
- defines
    radclock main and algo status constants like STARAD_WARMUP
    - typedef uint64_t vcounter_t;
    - VC_FMT "lu" or "llu" depending

typedef enum { RADCLOCK_LOCAL_PERIOD_ON, RADCLOCK_LOCAL_PERIOD_OFF }
		radclock_local_period_t;
  - no enumeration tag  (ie a name for this type) but since adding a typedef, can use it again that way


// kernel modes for pcap based timestamps
enum radclock_tsmode {
	RADCLOCK_TSMODE_NOMODE = 0,
	RADCLOCK_TSMODE_SYSCLOCK = 1,
	RADCLOCK_TSMODE_RADCLOCK = 2,
	RADCLOCK_TSMODE_FAIRCOMPARE = 3
};
typedef enum radclock_tsmode radclock_tsmode_t ;

// From radapi.c have related variables are fns mapping desired
// userland  radclock_tsmode_t mode to the best available kernel tsmode :
// No idea why the K_ modes shouldnt be enums also..
#define K_RADCLOCK_TSMODE_SYSCLOCK		0x0001  /* return SW timeval and raw vcounter */
#define K_RADCLOCK_TSMODE_RADCLOCK		0x0002  /* return timeval based on RADclock and raw vcounter */
#define K_RADCLOCK_TSMODE_FAIRCOMPARE  0x0003  /* return SW timeval, and vcounter read just before SW in global data */




- creates, destroys, initialization, get and set routines for a radclock
- change mode commands:  still supports RADCLOCK_TSMODE_FAIRCOMPARE = 3
- radclock specific time conversion functions
- defines kernel mode strings and get/set functions
    plocal
    RADCLOCK_UPDATE_mode   [ get params from kernel or not ]
- radclock reading options  [ which use the update mode inside no doubt ]
    gettimeofday, conversions, differences
    most in both timeval and fp (floating point) versions
- functions seem to all be defined in various libradclock files
    - probably is a summary of all library fns except a few internal workhorse one, to check
- need to clarity what is
    `global data'
    clock cant be reached
- bugs/problems :
    - a couple of typos in comments
    - many claims that errors and RTTs etc are long double, but are double
    - not clear what the radclock `offset' is'



## radclock-private.h

- defines many structures used in radclock_handle: radclock, radclock_{data,error,shm}
   - no fucking documentation on what the parameters are, or anything, of course
   - why are these here in a library routine, if they are private to radclock?
     why does the library need these things? but not radclock_handle  ??

struct radclock_data {
	double phat;
	double phat_err;
	double phat_local;
	double phat_local_err;
	long double ca;         // currently leapsectotal put in here!!
	double ca_err;
	unsigned int status;
	vcounter_t last_changed;
	vcounter_t valid_till;
};



/* IPC stuff:  IPC using shared memory */
#define RADCLOCK_RUN_DIRECTORY		"/var/run/radclock"    // needed for daemon
#define IPC_SHARED_MEMORY			( RADCLOCK_RUN_DIRECTORY "/radclock.shm" )



// This is a low level thing carrying shm and OS and local mode and pcap stuff and
// getcounter functions only, no radclock data.
// Seems to be all the kernel interaction stuff wrapped up.
// Not to be confused with radclock_handle from radclock_daemon.h ! which has
// everything to do with a radclock, and include a member of radclock type
// A strange name really.  Should be called: radclock_IPChub  or  radclock_accesshub
// Is used primarily by external processes using the API, it is their private clock,
// only the SHM is shared.  When he writes, `a radclock` this is what he means.
struct radclock {

	/* System specific stuff */
	union {   // values set in libradclock/kclock-{fbsd,linux}.c:init_kernel_clock
		struct radclock_impl_bsd 	bsd_data;
		struct radclock_impl_linux 	linux_data;
	};
//  kernel based switching defined lower down to set this on compilation:
//  BUG:  clunky, so many different ways to determine kernel aspects..
#ifdef linux
# define PRIV_DATA(x) (&(x->linux_data))
#else
# define PRIV_DATA(x) (&(x->bsd_data))
#endif

	/* IPC shared memory */
	int ipc_shm_id;
	void *ipc_shm; 			// why is this void* instead of radclock_shm* ?

	/* Description of current counter */
	char hw_counter[32];
	int kernel_version;

	radclock_local_period_t	local_period_mode;

	/* Pcap handler for the RADclock only */
	pcap_t *pcap_handle;
	int tsmode;

	/* Syscalls */
	int syscall_set_ffclock;	// used only in FBSD set_kernel_ffclock KV=0,1
	int syscall_get_vcounter;

	/* Read Feed-Forward counter */
	int (*get_vcounter) (struct radclock *clock, vcounter_t *vcount);
};


- defines lower level init and get functions interfacing with pcap and vcounter
  and the kernel.  This includes extract_vcount_stamp(,,,,) which understands
  how the vcount is hidden in the packet, and is the bridge to the lower level
  rd code in radclock/rawdata.c .
  	- connection is that rawdata.c:fill_rawdata_ntp  calls extract_vcount_stamp


static inline void  counter_to_time(*rad_data, *vcount, long double *time)
   - just Ca(vcounter) returning a long double, using classic method:
	  C_a(t) = p*T(t) + K_p  - thetahat  +  (T(t) - T(tlast))*(plocal-p)    (1)

	- uses plocal always instead of reading local_period_mode !
   aah, maybe plocal double variable always exists, is just set to phat of plocal mode if off, that way never need logic for floating calculations.
	- has a form of generation protection using last_changed
	- is this THE method of reading Ca within the daemon ?
		YES!  because this is the only one that builds in leapseconds via the leapaware ca in rad_data.
		Other manual readings within the algo are leap oblivious and hence wont return UTC
		Need to be clear:  the RADclock Cabs=Ca is a UTC clock !
	- Used in:
	     daemon:  client.ntp.c  pthread_{ntpserver,rawdata}, stampinput-livepcap, verbose.c
       library:  radapi-base.c:fill_ffclock_estimate    // see BUG comments above
   - Note:  this function does not look through algo-parameter history to perform replay.
	   It simply tries to read the most-current Ca() based on a fixed input vcount, an input
		which could be far in the past or future.
   - Should be called ReadRADclock_Cabs  or counter_to_Cabs


- bugs/problems :
    - broken comments in      int found_ffwd_kernel_version(void);
    - duplicate comment a bit further down




# kclock.h   [ as in Kernel clock ]   // covers all OSes
// I think this is minimum shared definitions from the kernel code itself that are
// needed (at least in some cases)   So are these fns defined in the kernel as well?

  - ensures bintime defined   // checks __FreeBSD__ in the process
    - note bintime sec field is signed, but frac unsigned
  - ensures struct ffclock_estimate  defined
  		- need it for the fns here taking cest argument
		- gets it from sys/timeffc.h  if  __FreeBSD__ and  HAVE_SYS_TIMEFFC_H defined


// Declares :
int get_kernel_ffclock(struct radclock *clock, struct ffclock_estimate *cest);
int set_kernel_ffclock(struct radclock *clock, struct ffclock_estimate *cest);
void fill_ffclock_estimate(struct radclock_data *rad_data,   *rad_err, *cest);
void fill_clock_data(struct ffclock_estimate *cest, struct radclock_data *rad_data);


// kernel fp code for  KV<2 compatibility, note about deprecating it
// not present in kernel CURRENT, not surprisingly
struct radclock_fixedpoint
int set_kernel_fixedpoint(struct radclock *clock, struct radclock_fixedpoint *fp);




########################################################################
########################################################################
########################################################################
##  libradclock   c files



########################################################################
## kclock-fbsd.c

// Protected with WITH_FFKERNEL_FBSD

int init_kernel_clock(struct radclock *clock)
  - KV=0,1:  cycles through all possible bpf%d names until we find one that opens
  	- on success sets  clock->bsd_data->dev_fd   [ via PRIV_DATA defn, see below ]
	- this is the dirty old trick: to get your hands on global data to have to open a bpf..
  - KV>1:  no need to do anything, have syscalls instead

#ifdef HAVE_SYS_TIMEFFC_H   // else dummy defns returning 0 (success), but BUG: should do more
// HAVE_SYS_TIMEFFC_H defined, or not, in config.h
// these fns exist since KV=2, and are cest based, so should only be called for KV>=2
   BUG:  if available, kclock.h will already have included timeffc.h, here would include a 2nd time !
	int get_kernel_ffclock(struct radclock *clock, struct ffclock_estimate *cest)
		- wraps ffclock_getestimate syscall  // comment:  FreeBSD system call
		- BUG: sanity verbosity wrong, bad data is NOT ignored
	int set_kernel_ffclock(struct radclock *clock, struct ffclock_estimate *cest)
		- wraps setting syscall
			 KV=0,1:  syscall(clock->syscall_set_ffclock, cest);
			 	- only time  clock->syscall_set_ffclock  used!
				- BUG: cases for KV=0,1 will never execute, whole fn needs KV>=2
					- consistent with that, this is only place clock->syscall_set_ffclock ever used, but in
					  fact this variable never set !  (except trivially)
					- this is an exercise in attempted pointless uniformity I think,

				but how IS global data set
					  in KV=0,1?  done via PRIV_DATA(clock)  in set_kernel_fixedpoint
		    KV=2,3:	 ffclock_setestimate(cest)


//  Actual Globaldata (kernel FFdata) setting
	 process_stamp
	 	if KV<2
			update_kernel_fixed
				set_kernel_fixedpoint(handle->clock, &fpdata))
					ioctl(PRIV_DATA(clock)->dev_fd, BIOCSRADCLOCKFIXED, fpdata);
	   else
			if HAVE_SYS_TIMEFFC_H_FBSD  check for hw_counter change;	// FBSD, KV>1
			fill_ffclock_estimate				// transform rad_data to FFclock format
			set_kernel_ffclock					// system call for KV>1
				ffclock_setestimate(cest)  	// links to syscall sys_ffclock_setestimate



# Support for VK = 0,1 :

// these are no longer used, even for VK=0,1, I think
// #define BIOCSRADCLOCKDATA	_IOW('B', FREEBSD_RADCLOCK_IOCTL, ..radclock_data) // set global data
// #define BIOCGRADCLOCKDATA	_IOW('B', FREEBSD_RADCLOCK_IOCTL, struct radclock_data)  // get
#define BIOCSRADCLOCKFIXED	_IOW('B', FREEBSD_RADCLOCK_IOCTL + 4, ) // also in pcap-fbsd.c

int set_kernel_fixedpoint(struct radclock *clock, struct radclock_fixedpoint *fpdata)
  - KV=0,1:   ioctl(PRIV_DATA(clock)->dev_fd, BIOCSRADCLOCKFIXED, fpdata);

 sys_ffclock_getcounter			[static]
 sys_ffclock_setestimate		[static]
 sys_ffclock_getestimate		[static]



########################################################################
## kclock-linux.c

// Protected with WITH_FFKERNEL_LINUX


# More complex lookup of RADclock related kernel syscalls etc. Need:
int init_kernel_clock(struct radclock *handle)
  - recall clock->linux_data->radclock_gnl_id
	 sets as:  PRIV_DATA(handle)->radclock_gnl_id = resolve_family(RADCLOCK_NAME);
  - simple fn with no KV switch, uses resolve_family to do hard work


static int resolve_family(const char *family_name)
  - Resolve a generic netlink id from a family name
  - comment that maybe dont need this anymore
  - returns  ret = nlmsg_parse(reply_hdr,GENL_HDRLEN,attrs,CTRL_ATTR_MAX,NULL);
  - relies on some RADclock enumeration types being defined, used in kernel(s)?

static int radclock_gnl_receive(int radclock_gnl_id, const struct sockaddr_nl *who,
		struct nlmsghdr *n, void *into)

// XXX this one is not used anymore ... remove?
static int radclock_gnl_get_attr(int radclock_gnl_id, void *into)
static int radclock_gnl_set_attr(int radclock_gnl_id, int id, void *from)

// Placeholders, since aim inside at KV>2 (for consistency with FBSD versions, but
// Linux kernels so far only 0,1
int get_kernel_ffclock(struct radclock *clock, struct ffclock_estimate *cest)
int get_kernel_ffclock(struct radclock *clock, struct ffclock_estimate *cest)
	- some commented code via PRIV_DATA(clock)->radclock_gnl_id instead of syscalls

// Highly analogous to FBSD version, with key line changed:
int set_kernel_fixedpoint(struct radclock *clock, struct radclock_fixedpoint *fpdata)
  - KV=0,1:   radclock_gnl_set_attr(PRIV_DATA(handle)->radclock_gnl_id,
				RADCLOCK_ATTR_FIXEDPOINT, fpdata);



########################################################################
## kclock-none.c

// Protected with WITH_FFKERNEL_NONE

Placeholders which return (1)  (error) for each of
	init_kernel_clock(struct radclock *clock)
	get_kernel_ffclock
	set_kernel_ffclock
	set_kernel_fixedpoint






########################################################################
Kernel interaction functions `API`
// KV = kernel version
// ckv = clock->kernel_version
###############

## Elements defined in kernel

#ifdef __FreeBSD__  			// limited use, see below
#ifdef linux					// limited use, see below

HAVE_ flags:   // I suspect these are auto-generated by GCC compilation of kernel
 - flag denotes has been seen by kernel, is available, doesnt mean is already included !!
 - for all kernel header guard macros, generate flag by:
    append "HAVE" and drop trailing "_" if any
   EG: timeffc.h  guard macro  _SYS_TIMEFF_H_ . If available then HAVE_SYS_TIMEFFC_H  defined

 Used in library:
  FBSD:
	HAVE_SYS_TIMEFFC_H 				// due to presence of FF code but not generated by it
	HAVE_RDTSC
	HAVE_MACHINE_CPUFUNC_H
  LINUX:
  	HAVE_RDTSCLL_ASM
	HAVE_RDTSCLL_ASM_X86
	HAVE_RDTSCLL_ASM_X86_64
	HAVE_LINUX_GENETLINK_H
	HAVE_PCAP_ACTIVATE

 Used in daemon:
 	HAVE_POSIX_TIMER
	HAVE_LIBRT
	HAVE_IFADDRS_H


## Elements Defined in FF Kernel Code:
sys/sys/timeffc
	- if available, is included to include defn of  struct ffclock_estimate
	- since is the one thing from FF kernel code that is included, it is special, see mess below.



================================================================================
## OS switches for library or radclock code  // not kernel symbols


# WITH_FFKERNEL_{LINUX,FBSD,NONE}    // due to library, not kernel, set in compile configure?
Protects code in :
  library:
  		{ffkernel,kclock,pcap}-{fbsd,linux,none}.c    // all code

  radclock: 		// all this stuff should be move to the OS dependent part of library
  		pthread_dataproc.c,	// limited stuff at top, dummy fns if NONE, else define NTP_ADJTIME()
									// _FBSD:  two related places dealing with hw_counter
		stampinput-livepcap.c// two places if _FBSD and KV=2, see below
		virtual_machine.c		// big chunks of stuff, complicated


#ifdef HAVE_SYS_TIMEFFC_H
- not defined in radclock/ nor in library, nor in V0.4 kernel code or summary
- present in RADclockrepo/config.h{,.in} , but still not sure of actual origin
- means that our kernel file  sys/timeffc.h  is available I think
- if not defined, then this support not there, fns that rely on it return dummy success (0)
- provides a check if kernel support actually available, if not, doesnt even define

	- FBSD only:  {ffkernel,kclock,pcap}-fbsd.c,  nowhere else
		- guards
				inclusion of sys/timeffc.h   // only try to include if available
				defn of non-dummy {set,get}_kernel_ffclock  ( KV>=2 fns ) // though these fns KV aware
		- doesNOT guard found_ffwd_kernel_version, so can get a version though in fact some of it missing


Qns:
	- if already have WITH_FFKERNEL_ control, plus KV aware code, why need HAVE_SYS_TIMEFFC_H ? isnt that the same as _FBSD and KV=2,3 anyway?  or is it because the KV might be right but the .h hasnt be read in or something?
	ANS:  this is for the following situations:
		- if for some reason it is not available though it should be [ incomplete kernel patch? ]
		- to allow compilation without kernel support (but still use WITH_FFKERNEL_FBSD)
			- found_ffwd_kernel_version will detect at runtime, so wont try to actually execute dummy fns




## Other in library     // keep track of whether kernel related stuff well isolated

#ifdef linux:
  radclock_private.h :    // v-natural to put here since is a convenience for struct radclock
	 #ifdef linux				// resolve union structure shortcut in struct radclock
	 # define PRIV_DATA(x) (&(x->linux_data))
	 #else
	 # define PRIV_DATA(x) (&(x->bsd_data))
   // Actual members of clock are set in kclock-{fbsd,linux}.c:init_kernel_clock
	// this is generic, works for all KV's, is just: special point for bsd/linux is ...


#ifdef __FreeBSD__
	kclock.h						// no problem, this is a (OS,kernel) specific file


## Other in Daemon

#ifdef __FreeBSD__
   jdebug.h						// doesnt matter, anything goes in debug



================================================================================
Kernel-specific setup mainly found in the library in
	{ffkernel,kclock,pcap}-{fbsd,linux,none}.c

Here we summarize use of this set up:

#  ckv:  interesting variable as doesnt state what OS, so not enough to select
#        right action by itself, will have to switch on OS also manually, or, 
#			call fns that have already been made OS-generic  (this is the approach followed for KV>=2,
#			where KV has meaning for all OSes.
ckv appears in :
	radclock_main.c:
		clock_init_live:  ckv = found_ffwd_kernel_version();  // only place called
								if ckv>=2  get_kernel_ffclock(clock, &cest);
	   start_live:			if ckv<2  start_thread_FIXEDPOINT
		
	pthread_dataproc.c
		process_stamp:	if ckv<2 update_kernel_fixed
								#ifdef WITH_FFKERNEL_FBSD  
									check if HWcounter changed, if so restart algo [but history not cleared?]
									
	stampinput-livepcap.c
		livepcapstamp_init:  #ifdef WITH_FFKERNEL_FBSD  // TODO here about KV mess
										if ckv=2 set RADCLOCK_TSMODE_RADCLOCK, else _SYSCLOCK
								   #else 
										set RADCLOCK_TSMODE_SYSCLOCK
										
		get_packet_livepcap:  #ifdef WITH_FFKERNEL_FBSD   // and if LInux??
										if ckv=2  Create pcap timestamp from daemon, pass back
										
	virtual_machine.c:	leaving for now




## Other kernel stuff:



# KV summary:
// key resources:   ffkernel-{fbsd,linux,none}.c: found_ffwd_kernel_version
// KV values are sync'd between FBSD and LINUX roughly speaking. In particular for
//   KV>=1 generic names are supposed to give an OS-independent interface and features, I think.
FBSD
  -1 or <0 :   no RADclock or FFclock support
   0 :		pre-ffclock, no explicit versioning, no timecounter, no passthrough
			need to open bpf to access globaldata
			need fixedpt
			ioctl
				#define BIOCSRADCLOCKTSMODE	_IOW('B', FREEBSD_RADCLOCK_IOCTL + 2, int8_t)
				#define BIOCGRADCLOCKTSMODE	_IOR('B', FREEBSD_RADCLOCK_IOCTL + 3, int8_t)
				ioctl(pcap_fileno(p_handle), BIOCSRADCLOCKTSMODE, (caddr_t)&kmode)
				ioctl(pcap_fileno(p_handle), BIOCGRADCLOCKTSMODE, (caddr_t)kmode)
				ioctl(PRIV_DATA(clock)->dev_fd, BIOCSRADCLOCKFIXED, fpdata);
			sysctl:
				"net.bpf.bpf_radclock_tsmode"
   1 :		ffclock, uses timecounter, versioning supported, passthrough
		   still need to open bpf to access globaldata (bit surprising, check this)
			still need fixedpt
			ioctl
				ioctl(pcap_fileno(p_handle), BIOCSRADCLOCKTSMODE, (caddr_t)&kmode)
				ioctl(pcap_fileno(p_handle), BIOCGRADCLOCKTSMODE, (caddr_t)kmode)
			sysctl:
				"kern.ffclock.version"
				"kern.timecounter.passthrough"
		   syscalls:
				syscall(clock->syscall_set_ffclock, cest);

	2 :		// assumes kernel CURRENT code (descriptor_get_tsmode uses _F{F,B}CLOCK )
	 		eliminate need for FIXEDPOINT thread
			use extra syscalls instead of bpf opening to access globaldata
			faircompare mode eliminated, RADCLOCK_TSMODE_FAIRCOMPARE -> _SYSCLOCK
			bpf header changed from usual timeval + `hidden` vcount to only one ts (union)
			ioctl
				ioctl(pcap_fileno(p_handle), BIOCSTSTAMP, (caddr_t)&bd_tstamp)
			syscalls:
				"kern.sysclock.ffclock.version"
				"kern.sysclock.ffclock.ffcounter_bypass"
	3 :		// /* Version 3 is version 2 with the extended BPF header */
	 		// This version all about enhancements at pcap level, not generic vcounter stuff.
			bpf header changed to two official timestamp fields:  system clock and vcount

						
LINUX
  -1 or <0 :   no RADclock or FFclock support
   0 :		pre-ffclock, no explicit versioning, no clocksource, no passthrough
		   need to open bpf to access globaldata?
	 		need fixedpt
			radclock_gnl_set_attr(PRIV_DATA(handle)->radclock_gnl_id, ..)
			fd = fopen ("/proc/sys/net/core/radclock_default_tsmode", "r");
			syscalls:
				"?"
   1 :		ffclock, using clocksource, versioning supported, passthrough
			fd = fopen ("/sys/devices/system/ffclock/ffclock0/version", "r");
		   need fixedpt
			radclock_gnl_set_attr(PRIV_DATA(handle)->radclock_gnl_id, ..)
			syscalls:
				"?"
	2 :		doesnt yet exist explicitly
	 		fd = fopen ("/sys/devices/system/ffclock/ffclock0/version", "r");
			
			

			
RADCLOCK_TSMODE_{RADCLOCK,SYSCLOCK,FAIRCOMPARE}				// pcap interface attribute
	- set to SYSCLOCK in all cases except BSD with KV==2
	- logic is I think that in other cases have no choice, always had pcap timestamp being from the sysclock, but now can get RADclock directly. This is preferable for the daemon since the in-kernel conversion used to read that CA value used the rad_data appropriate (we hope) to the raw timestamp. If calculated later in the daemon, may have to look up (ie a mini `replay`) the right params for that raw value which is not supported, I dont think.
	- no, this explanation is clever but the real reason is that for BSD with KV==2 this is the only
	  way to get the counter - see notes elsewhere.
	- of course in research sysclock-RADclock comparison work, would want _SYSCLOCK, or FAIRCOMPARE, but seems to be hardwired now, how to get what you ask for? need to do it as an external radclock?

	- if KV=2, must select RADCLOCK else there is no vcount!
	- QN:  which clock members can be different in an external clock from the deamons copy?






# From Lastest Full FBSD Kernel code:    //  matches calls under FBSD:KV=2 above

Syscalls:     // if undef FFCLOCK, these return(ENOSYS), but not used in daemon or library code
kern_ffclock.c :
 sys_ffclock_getcounter			[static]
 sys_ffclock_setestimate		[static]
 sys_ffclock_getestimate		[static]
- Convention whereby syscall "sys_name" becomes a function "name" in userland.
			
SYSCTL tree:   [ underscores put between all new levels of nodes and variables ]
_kern
	-> _kern_sysclock
		- available;  "List of available system clocks" [RO]
		- active      "Name of the active system clock which is currently serving time"
		-> kern_sysclock_ffclock										
		   - version			"Feed-forward clock kernel version" [RO]
		   - ffcounter_bypass	"Use reliable hardware timecounter as the feed-forward counter"

















########################################################################
## ffkernel-fbsd.c			// connection to underlying kernel support
// Protected with WITH_FFKERNEL_FBSD

Only radclock includes are:
#include <radclock.h>
#include "radclock-private.h"
#include "logger.h"


// This stuff is about reading a vcounter, and therefore how to connect to the
// FFcounter support, which in version >1 (true?) became integrated into
// the timecounter mechanism.
// Ultimately it is about selecting and setting  clock->syscall_get_vcounter,
// which is either a syscall, or a passthrough call to rdtsc if desired and possible.

int found_ffwd_kernel_version (void) // returns kernel version, -1 if no support

int has_vm_vcounter(struct radclock *clock)	// passthrough mode stuff in vm world

static inline uint64_t rdtsc(void) 				// defines this is not already defined
// Then define this wrapper   (same thing appears in radclock-read.c  )
inline vcounter_t radclock_readtsc(void) {
	return (vcounter_t) rdtsc();
}
// Extra wrapper to match the prototype of get->vcounter to which it (may be) assigned
inline int radclock_get_vcounter_rdtsc(struct radclock *handle, vcounter_t *vcount)
{
	*vcount = radclock_readtsc();
	return 0;
}


int radclock_init_vcounter_syscall	(struct radclock *clock)
  - try to create  get_vcounter  syscall
  - clock->syscall_get_vcounter = stat.data.intval; 	// record its registration
  - FBSD: for KV>1 instead get ffclock_getcounter()  linked to sys_ffclock_getcounter syscall for us by libc

int radclock_get_vcounter_syscall	(struct radclock *clock, vcounter_t *vcount)
  - wrapper to select relevant syscall method depending on KV:
	 0,1:		syscall(clock->syscall_get_vcounter, vcount);
	 2,3:		ffclock_getcounter(vcount);

int radclock_init_vcounter				(struct radclock *handle)
  - actually makes the connection to the right underlying counter call
  - tries to determine if passthrough (ie using rdtsc) can work
		clock->get_vcounter = &radclock_get_vcounter_rdtsc;     // if it can
      clock->get_vcounter = &radclock_get_vcounter_syscall    // else init to syscall

  Logic:             // needs some tidying up
  	passthrough = 0   // BUG:  should be =1, (never set to 1 in code)
   Check kernel for (KV dependent) passthrough flag
	  if fail,
	  		passthrough = 0
	  else
			get timecounter we have, put in handle->hw_counter, and RADLOG_NOTICE it
   if passthrough = 0
	 	use radclock_get_vcounter_syscall
   else  // BUG means this wont execute since passthrough always=0 !
		if TSC
			use radclock_get_vcounter_rdtsc
	 	else
			use radclock_get_vcounter_syscall

	verbose compatibility check: "Passthrough mode in ON but the timecounter does not support it!!"



// libprocesses
	radclock_create		// create a clock structure, a private `radclock`
	radclock_init			// initializes clock
		radclock_init_vcounter_syscall(clock)
		radclock_init_vcounter(clock)





########################################################################
## ffkernel-linux.c			// connects get_vcounter to underlying kernel support

// Protected with WITH_FFKERNEL_LINUX


All function calls the same as fbsd versions!  just the details on internals that
are different.

Effectively is documentation on all kernel version differences, see KV summary.




########################################################################
## ffkernel-none.c

// Protected with WITH_FFKERNEL_NONE

Placeholders which return error for each of
	found_ffwd_kernel_version					// returns -1
	has_vm_vcounter								// returns -ENDOENT
	radclock_init_vcounter_syscall			// returns  0
	radclock_init_vcounter						// returns  0





########################################################################
## radapi-pcap.c
// Deals with getting packet data and associated vcount out of pcap, and the
// setting of the desired tsmode to do so.
// No (OS,KV) dependence.  That is relegated to the pcap-{fbsd,linux,none}.c  code.
Only radclock includes are:
#include <radclock.h>
#include "radclock-private.h"
#include "logger.h"


// is this imitating the syscall naming convention of sys_name becoming name() in userland?
#define K_RADCLOCK_TSMODE_SYSCLOCK		0x0001  // return SW timeval and raw vcounter
#define K_RADCLOCK_TSMODE_RADCLOCK		0x0002  // return timeval based on RADclock and raw vcounter
#define K_RADCLOCK_TSMODE_FAIRCOMPARE  0x0003  // return SW timeval, vcounter read just before SW
 - these arent copies of some kernel #defines, they dont exist here except to help us, in theory,
  distinguish between what we ask for and available kernel values
 - seems to be a failed exercise in attempted generality, these are not really used in any real sense.
 - only real point could be if the kernel used different integers, this is then just a mapping
  function between the daemon and kernel side integers for exactly the same modes.
  I guess if the kernel ever needed to change the integers (likely?? is this subject to the vagaries of BPF_T_ rewritting?), this would restrict any needed changes in the daemon to fixing this mapping.  But this is only needed if you ever just naively copy the integers over, if instead
  use logic, no need to ever do this.

Basic story:   they are set up to be separate to radclock_tsmode_t
but are used as if they are identical, not even  used consistently ( see radclock_get_tsmode notes just below), and are not understood by the kernel either (only integers 1 2 3 are ever passed to kernel), so this is really just just have two forms of the same thing in the library.  Combined with faircompare being retired anyway, this is just a complicated mess - best eliminated.

// Recall related userland modes from  radclock.h :
enum radclock_tsmode {
	RADCLOCK_TSMODE_NOMODE = 0,			// no FF support in pcap
	RADCLOCK_TSMODE_SYSCLOCK = 1,		// leave normal pcap ts field, hide vcount in padded hdr
	RADCLOCK_TSMODE_RADCLOCK = 2,		// fill ts with RADclock Abs clock, hide vcount in hdr
	RADCLOCK_TSMODE_FAIRCOMPARE = 3  // as _SYSCLOCK, but ts and raw timestamps back-to-back
};
typedef enum radclock_tsmode radclock_tsmode_t ;
 - The function radapi-pcap.c:radclock_set_tsmode translates between these before
actually setting the values in the kernel using the OS specific descriptor_set_tsmode(kmode) .
But inside descriptor_set_tsmode it is not the K_RADCLOCK_TSMODE_* which are used !
relying on the fact that the two sets are 1-1 in value and correspond
 - TODO:  eliminate these probably or  BUG:  make them K_ modes internally  except those
   K_ defined in radapi-pcap.c  (currently used only there) arent even visible inside pcap-fbsd.c  , right?


int radclock_set_tsmode(clock, pcap_t *p_handle, radclock_tsmode_t mode)
	- as just said, translates directly from mode=RADCLOCK_TSMODE_{,,} to kmode=K_RADCLOCK_TSMODE_{,,},
	  then just calls  descriptor_set_tsmode
	- no KV dependence here, which means for high KV where RADCLOCK_TSMODE_FAIRCOMPARE
	  deprecated, will still need to understand  kmode=3=K_RADCLOCK_TSMODE_FAIRCOMPARE in the kernel,
	  and map it to K_RADCLOCK_TSMODE_SYSCLOCK instead !
	  - I guess it makes sense that the daemon shouldnt have to know how these concepts are understood by the kernel.
	    - Ahh! but in radclock_get_tsmode that is exactly what is expected when the daemon has to
		 translate in the other direction!
int radclock_get_tsmode(clock, pcap_t *p_handle, radclock_tsmode_t mode)
   - calls  descriptor_get_tsmode, then just assumes numerical codes for mode and kmode agree !
	  TODO:  should kmode also be an enum then?

- BUG: bizarre thing is that clock->tsmode  is an int, not a radclock_tsmode_t,
    hence the comment I guess:  //TODO align enum with kernel modes
	 Actually this probably means to do the switching instead of just assuming the numbers agree.


// need to wrap these up so can pass all params kernelclock_routine needs to pcap_loop
struct routine_priv_data
{
	struct radclock *clock;
	pcap_t *p_handle;
	struct pcap_pkthdr *pcap_header;
	unsigned char *packet;
	vcounter_t *vcount;
	struct timeval *ts;
	int ret;
};


void kernelclock_routine(u_char *user, const struct pcap_pkthdr *phdr, const u_char *pdata)
  - is the callback for pcap_loop
  - via a local ptr `data`, fills following routine_priv_data members of *user:
  	  pcap_header, packet, ts, and vcount and ret (using extract_vcount_stamp)
  - doesnt fill:  p_handle,


// dont pass a routine_priv_data structure to this since is in the public library (radclock.h)
// This function only gets you a single packet, not that practical? ok for simple demo though.
// Comment about `no choice but to pass clock', but dont see the pb, every fn in radclock.h passes clock!
int radclock_get_packet( struct radclock *clock, 
						pcap_t *p_handle, 
						struct pcap_pkthdr *header, 
						unsigned char **packet, 
						vcounter_t *vcount, 
						struct timeval *ts)
{
	- NOTE: when he says `user` here he means an external radclock, not the old Swedish user-TSCclock !
	



########################################################################
## pcap-fbsd.c
// This is the file that reveals how the vcounter was hidden differently in the bpf header
// for each of:
// 	 KV=0,1  :   normal timeval +  vcount hidden in padding
//		 KV=2 	:	only one timestamp, normal or raw, at al time, in union structure selected somehow
//		 KV=3		:	two official timestamps:  timeval and vcounter
//                timeval abstracted to bpf_ts in CURRENT, not due to us?

The three functions   extract_vcount_stamp_v{1,2,3} all get the vcount via: *vcount= hack->vcount;
the trick is just to get hack to point to the right place, taking into account the
actual header size given the header hack in question (v1,2,3) and the associated padding issues.


// Protected with WITH_FFKERNEL_FBSD


/* Deprecated IOCTL based BFP modifications stuff for FF support Versions 0, 1 */
#define BIOCSRADCLOCKTSMODE	_IOW('B', FREEBSD_RADCLOCK_IOCTL + 2, int8_t)
#define BIOCGRADCLOCKTSMODE	_IOR('B', FREEBSD_RADCLOCK_IOCTL + 3, int8_t)


int descriptor_set_tsmode(struct radclock *handle, pcap_t *p_handle, int kmode)
	- KV=0,1 uses ioctl  BIOCSRADCLOCKTSMODE
	- KV=2,3 sets _SYSCLOCK and _FAIRCOMPARE to the same:  BPF_T_MICROTIME
			else BPF_T_FFCOUNTER
int descriptor_get_tsmode(struct radclock *handle, pcap_t *p_handle, int *kmode)
	- KV=0,1 uses ioctl  BIOCSRADCLOCKTSMODE
	- KV=2,3:  uses BPF_T_{CLOCK_MASK,FBCLOCK,FFCLOCK}  masks from FFmodfied bpf.h
      to select RADCLOCK_TSMODE_
		// first place where CURRENT kernel code assumed! and there are TODOs

/* Big comment on word alignment issues and the interaction with the hacked bpf_header */

// What TS modes do we now Want from userland?  [assume KV=3, KV=2 is so limiting..]
 Daemon (implies FFCLOCK=TRUE):   'just get me the FFcounter'  // other timestamp not actually used
    - get the counter in FFcounter field
	 - CLOCK:  use normal sysclock?  but now potentially FF
	 		     ** make FB deliberately to support compareclock.m, the daemon already has FF
				  make FF to support comparison with LERP, or kernel based Ca sanity checking?
				  In future could add extra modes to use daemon as research engine
	 - FORMAT: ask for NANOTIME   (tval is crap, bintime not natural or needed in userland)
	 - call this RADCLOCK_TSMODE_FFcounterandFBsysclock or RADCLOCK_TSMODE_DEAMON (more generic)
	 	- modern form of old RADCLOCK_TSMODE_SYSCLOCK  [ those old name does still make sense in Uni world, but is misleading, may imply that FF is supposed to be the SYSCLOCK
	 - obtain via radclock_set_tsmode
	     - passing above name to it
		  - mapping it to BPF_T_FFCOUNTER | BPF_T_FBCLOCK_NANOTIME_MONOTONIC  or manually to
		       BPF_T_FFCOUNTER | BPF_T_FBCLOCK | BPF_T_NANOTIME
				oops:   - cant have two FORMAT flags !!
				        - if dont include BPF_T_MONOTONIC explicitly, what will happen?? always need?

 Libprocesses:  'get me counter, also return Ca(t) for convenience'
	 - get the counter in FFcounter field
	 - CLOCK: ask for FF deliberately to support easy FFclock reading without need for
	 			 API post-conversion, SHM, or parameter database concerns (delayed conversion)
	 - FORMAT: ask for NANOTIME
	 - FLAG:   dont ask for LERP  (again, could add extra options)
	 - call this RADCLOCK_TSMODE_FFcounterandFFsysclock or RADCLOCK_TSMODE_READSYSFF
	 	- enchanced form of old RADCLOCK_TSMODE_RADCLOCK [ old name ok I guess]
    - in radclock_set_tsmode map it to
	    BPF_T_FFCOUNTER | BPF_T_FFCLOCK | BPF_T_NANOTIME    // same problem as above




enum radclock_tsmode {
	RADCLOCK_TSMODE_NOMODE = 0,			// no FF support in pcap
	RADCLOCK_TSMODE_SYSCLOCK = 1,		// leave normal pcap ts field, hide vcount in padded hdr
	RADCLOCK_TSMODE_RADCLOCK = 2,		// fill ts with RADclock Abs clock, hide vcount in hdr
	RADCLOCK_TSMODE_FAIRCOMPARE = 3  // as _SYSCLOCK, but ts and raw timestamps back-to-back
};





// This is identical to RADclock FBSD 8.1 bpf.h defn except vcount was vcount_stamp
// So this is what is used in KV=1
// included here with a different name to ease compilation
struct bpf_hdr_hack_v1 {
	struct timeval bh_tstamp;		/* time stamp */
	bpf_u_int32 	bh_caplen;		/* length of captured portion */
	bpf_u_int32 	bh_datalen;		/* original length of packet */
	u_short			bh_hdrlen;		/* length of bpf header (this struct plus* alignment padding) */
	u_short			padding;			/* padding to align the fields */
	vcounter_t		vcount;			/* raw vcount value for this packet */
};



static inline int extract_vcount_stamp_v1(pcap_t *p_handle, const struct pcap_pkthdr *header,*pkt,*vcount)
	- based on bpf_hdr_hack_v1 and local to ensure alignment is taken care of, plus checks
	- used also for KV=0, difference between KV 0,1 lie elsewhere
	

// Based off pcap_pkthdr but this time commented out, but probably the KV=2 hack:
//		idea is to have normal TS or raw in standard ts field, depending, but not both
//    (no need to hide vcount in padding).  How does this work with _SYSCLOCK?
//struct bpf_hdr_hack_v2 {
//	union {
//		struct timeval tv;	/* time stamp */
//		vcounter_t vcount;
//	} bh_ustamp;
//	bpf_u_int32 caplen;		/* length of captured portion */
//	bpf_u_int32 len;			/* original length of packet */
//};

static inline int extract_vcount_stamp_v2(pcap_t *p_handle, const struct pcap_pkthdr *header,*pkt,*vcount)
	- just return header->ts
	- only works for RADCLOCK_TSMODE_RADCLOCK, for KV=2 _SYSCLOCK does not have a vcount to grab!
	  maybe this was shortlived, did most data capture with KV=1
	
	  


// TODO could use system include from bpf.h
struct bpf_hdr_hack_v3 {
	struct timeval bh_tstamp;	/* time stamp */
	vcounter_t vcount;			/* raw vcount value for this packet */
	bpf_u_int32 bh_caplen;		/* length of captured portion */
	bpf_u_int32 bh_datalen;		/* original length of packet */
	u_short bh_hdrlen;			/* length of bpf header (this struct plus alignment padding) */
};

- even this v3 hack uses a timeval, it is not a bpf_xhdr type, much less the experimental
one adding the extra FF member from  V4/bpf.c:
struct bpf_ts {
	bpf_int64	bt_sec;		/* seconds */
	bpf_u_int64	bt_frac;	/* fraction */
};
struct bpf_xhdr {
	struct bpf_ts	bh_tstamp;	/* time stamp */
	ffcounter	bh_ffcounter;	/* feed-forward counter stamp */
	bpf_u_int32	bh_caplen;	/* length of captured portion */
	bpf_u_int32	bh_datalen;	/* original length of packet */
	u_short		bh_hdrlen;	/* length of bpf header (this struct
					   plus alignment padding) */
};

what is going on??  Note however commented out bpf_hdr_hack_v2 which has a vcount member !
extract_vcount_stamp_v2  is in fact a hackk that doesnt even use a   bpf_hdr_hack_v2
because in fact using a standard header, just hiding a vcounter in it.
However, bpf_hdr_hack_v3  DOES correspond to the FF-enhanced Obsolete header above.



	
static inline int extract_vcount_stamp_v3(pcap_t *p_handle, const struct pcap_pkthdr *header,*pkt,*vcount)
	-


int extract_vcount_stamp(struct radclock *clock, pcap_t *p_handle,
		const struct pcap_pkthdr *header, const unsigned char *packet,
		vcounter_t *vcount)
	- calls the right version of vcount hiding depending on KV:
	  0,1:  extract_vcount_stamp_v1(clock->pcap_handle, header, packet, vcount);
	  2  :  if RADCLOCK_TSMODE_RADCLOCK     // This version supports a single timestamp at a time
	  				extract_vcount_stamp_v2
			  else
					error  //  why?  vcount is there regardless, no??

	  3  :  if RADCLOCK_TSMODE_RADCLOCK
	  				extract_vcount_stamp_v2   // this confuses me, header wont be of v3 type !
			  else
					extract_vcount_stamp_v3

 - need to look at kernel code to sort this out, should become clear, drop complex
   reverse eng!
   Key qn is what is put in standard timestamp field in _RADCLOCK
	

 
 

=================================================
## Recall from pcap.h

struct pcap_pkthdr {
  struct timeval ts;			/* time stamp */
  bpf_u_int32 caplen;		/* length of portion present */
  bpf_u_int32 len;			/* length this packet (off wire) */
};

Qn:  what is the difference between this and the bpf_hdr ? and between the timestamp fields?


=================================================
## bpf.h  pcap header defn:

# From modified CURRENT BSD kernel:
struct bpf_xhdr {
        struct bpf_ts   bh_tstamp;      /* time stamp */   // used to be struct timeval
        ffcounter       bh_ffcounter;   /* feed-forward counter stamp */   // properly supported field
        bpf_u_int32     bh_caplen;      /* length of captured portion */
        bpf_u_int32     bh_datalen;     /* original length of packet */
        u_short         bh_hdrlen;      /* length of bpf header (this struct plus alignment padding) */
};

# FBSD 8.1 kernel    //  KV=1 je crois

#ifdef RADCLOCK
struct bpf_hdr {
        struct timeval  bh_tstamp;      /* time stamp */
        bpf_u_int32     bh_caplen;      /* length of captured portion */
        bpf_u_int32     bh_datalen;     /* original length of packet */
        u_short         bh_hdrlen;      /* length of bpf header (this struct plus alignment padding) */
        u_short         padding;        /* padding to align the fields */
        vcounter_t      vcount_stamp;   /* raw virtual timecounter timestamp for this packet */
};
#else   // original
struct bpf_hdr {
        struct timeval  bh_tstamp;      /* time stamp */
        bpf_u_int32     bh_caplen;      /* length of captured portion */
        bpf_u_int32     bh_datalen;     /* original length of packet */
        u_short         bh_hdrlen;      /* length of bpf header (this struct plus alignment padding) */
};
#endif /* RADCLOCK */

 
 
 
 
########################################################################
## pcap-linux.c

// Protected with WITH_FFKERNEL_LINUX



########################################################################
## pcap-none.c

// Protected with WITH_FFKERNEL_NONE

Placeholders which return (1)  (error) for each of
	descriptor_set_tsmode
	descriptor_get_tsmode
	extract_vcount_stamp



########################################################################
## pcap-logger.c

Relocated after verbose.c  notes.






########################################################################
## radapi-base.c

Lots of shm initialization stuff, most of the file defines


struct radclock * radclock_create(void)
	- create radclock structure, init to trivial values
	- used in create_handle:   handle->clock = radclock_create()


/* Initialise shared memory segment.
 * IPC mechanism to access radclock updated clock parameters and error estimates.*/
int shm_init_reader(struct radclock *clock)
	- initialises  clock->ipc_shm_id
	- clock->ipc_shm = shmat(clock->ipc_shm_id, NULL, SHM_RDONLY);  // read only !
	- called in radclock_init only, but this fn never called by RADclock, used externally only
	- uses shmget which will create segment if not yet existing. This shouldnt occur, but might,
	 and allows a user clock to be created which will still work (using kernel data), and when
	 a deamon is finally launched, it will happily pick up this same shm already created.
	 Alternative would be to say: " sorry, cant create your RADclock since daemon not running", which is
	 like the API equivalent of a system clock refusing to return a value just because ntpd is not running..

int shm_init_writer(struct radclock *clock)  function
	- called in  radclock_main:init_handle() , and rehash_daemon, if running live
	- creates then closes the shm_fd file immediately !  yet now exists, do ftok on it
	- is segment already exists, then shmget call will fail, and have to set modes etc
	  correctly using shmctl calls.

int shm_detach(struct radclock *clock)
 	- pure wrapper around shmdt
	- comment about not removing the SHM id
		"Best is to have the shared memory created once, reused and never deleted."

// Initialise what is common to radclock and other apps that have a clock
int radclock_init(struct radclock *clock)
	- radclock_create performs a trivial init
	- tackles proper init of pointers to shared resources:  [ should be called radclock_init_shared() ?]]
		- SHM
		- kernel interface:
				vcounter_syscall  (ie    clock->syscall_get_vcounter via radclock_init_vcounter_syscall)
				vcounter 			(ie fn-pointer clock->get_vcounter via radclock_init_vcounter)
				syscall_set_ffclock   BUG:  is missing  clock->syscall_set_ffclock  never set except to 0
		 
	- avoids elements that arent clock dependent:  [ but why, couldnt include here? ]
		- local_period_mode:  can be chosen freely for any clock
		- private data
		- hw_counter


void radclock_destroy(struct radclock *clock)
	- nextBUG: uses shmdt directly instead of shm_detach - inconsistent

int radclock_register_pcap(struct radclock *clock, pcap_t *pcap_handle)
	- just:  	clock->pcap_handle = pcap_handle;
     to support public opacity of clock,
	- daemon doesnt use this, instead sets using:
	  stampinput-livepcap.c:livepcapstamp_init :handle->clock->pcap_handle = LIVEPCAP_DATA(source)->live_input;
	  set by: LIVEPCAP_DATA(source)->live_input = open_live(handle, LIVEPCAP_DATA(source));


/* Build the data structure to pass to the kernel */
void fill_ffclock_estimate(struct radclock_data *rad_data,
									struct radclock_error *rad_err, struct ffclock_estimate *cest)
	 - explains <<63 trick at last, as a technique of getting   p * 2^64
	   Is relying on implied casting to an integer throwing away fractional part.
	 - Want to *2^64 because will put it into an integer field
	   with interpretation of units of 2^-64, so extracting from there is like * 2^-64  .
		So premultiplying will cancel this:  2^64 * 2^-64 = 1
	 - is relying on implicit rounding or just truncation when casting in errb_{abs,rate}, should really
	   make it explicit
	 - [fixed] BUG:  update_time is set using the generic  counter_to_time(rad_data, vcount, *time)
	   which reads native Ca(t).  But! it is fed a vcount of last_changed, so generic function
		(which interpolates from last stamp time to input vcount) is superfluous. But it also
		uses last_changed as an update checker. If in fact an algo update occurs mid-stream,
	   then bizarrely the function will look back in time to read Ca at the OLd value.
		But what we want here is simply to reflect the most recent update !
		Instead, should drop the interpolation and just calculate at the current stamp.
		You could then check for changes if you want, and get the new one if it updates.
	    But, why do this update checking for update_time, but not for other members?
		 if it were updated, vcount would be out of date.  It is trying to respect the original last_changed,
		 by returning a time corresponding to it (even if calculated with newer data), but the rest of
		 the parameters wont respect that original update, end up with inconsistency, which is what this was
		 supposed to avoid !
		 Better to protect the entire fn with a check. Then the call to counter_to_time would
		 work, but better to do the calculation direct in situ., clearer.
	   - rationale for the checking may be its use in virtual_machine.c,  where maybe the call can be delayed.
		  In normal daemon, the fn is only called once in process_stamp following a stamp update,
		  so nothing can in fact change while fill_ffclock_estimate  runs, so not needed at all.
	- [fixed] first cest->leapsec should be cest->leapsec_next ;
	- [Fixed] 1e9 seems too small (integer<1 in many cases given phat errors often ~1e-10).
	  Should be [ps]/[s] ,  hence multiply by 1e12, not 1e9 in fill_ffclock_estimate.
	  This would match (cancel) the factor of 2^64/1e12 claimed in kern_ffclock.c:ffclock_abstime
	  even though code used /1e6 by mistake.


	 

/* Build radclock_data from kernel estimate */
void fill_clock_data(struct ffclock_estimate *cest, struct radclock_data *rad_data)
	- name changed to fill_radclock_data
	
	- other way around: use kernel data to get clock data in rad_data format
   - plocal and phat set to the same thing  - makes sense if only ever used as a warmup initialization, otherwise no, but  cant invert to get plocal anyway
   - called in:  main :  clock_init_live
					  external process:   radclock_get_*   ffcounter_to_{difftime,abstime}_kernel
	  but the LOCAL_PERIOD mode used in the _kernel routines must have a real plocal to be meaningful, but it is impossible, so a waste of time currently, but good to be future proof if a phat member added to ffclock_estimate in future, or if phat selected differently somehow.
   - Hence BUG:  filling of ca is wrong because cant invert from current fields:
	    Ca(tlast) = phat*T(tlast) + ca :  know T(tlast) and Ca(tlast), but both phat and ca missing, so cant invert .  Currently sets  ca = Ca(tlast):  a big error even for the clock_init_live application.  !!   Instead update_time - phat*last_changed ,  using plocal as a surrogate
		  Since this is used in circumstances where the daemon is dead, using plocal is natural,
		  effectively it means that pbar = outofdate plocal going forward - close to optimal anyway
	- but does copy over status verbatim, and last_changed = cest->update_ffcount
	- old printout of mine commented out here :-)
	- Note sometimes uses 1LLU, sometimes 1LL, both should work.
	- TODO:  name should be  fill_radclock_data for symmetry and to indicate is radclock specific




########################################################################
## Error system

	The err fields in radclock_data are the original current-stamp errors for each parameter.
	The radclock_error structure (with global variable handle.rad_error) is newer, focussed
	on Ca, and providing more time-series like metrics, calculated in process_bidir_stamp
	using on-line algorithms.  Further, the algo parameter holding structure bidir_peer
	has a member structure peer_error, which tries to be comprehensive and aims to centralize
	all errors but is currently just a first cut placeholder.
	
	All this is done in sync_bidir.c , particularly at the end where the visible (handle.radclock_error)
	and internal (peer_error)  members are set, based on the value of Ebound_min_last
	which is a renaming of the last updated Ebound, set currently to  minET; in
	process_thetahat_{warmup,full}
	
	Design question here is why store the per-stamp  minET in rad_data at all?   it is
	just one of many low-level per-stamp parameters, and doesnt correspond to the error of the
	thetahat finally accepted for that stamp.  For that you need to access the lasttamp and apply aging.
	This is exactly what error_bound currently does, of course.
	Qn is:  should error_bound  be put into rad_data instead of having this whole new rad_error structure?
	        Or, strip out errors from rad_data, and use rad_error for that.
	Current soln:   make ca_err = error_bound  so it is meaningful.
			- that way fill_clock_data   is mapping between  cest and rad_data only, simpler
			- question of if need rad_error can be addressed later.
	






########################################################################

### radapi-getset.c

## SET functions:

/* Specifies if local refinement of hardware counter period should be used when computing time*/
int radclock_set_local_period_mode(struct radclock *clock, radclock_local_period_t *local_period_mode)
	- not sure if want this facility - if you mistrust plocal, maybe better ways to avoid it
	- aah, never used in fact, default value of ON set in radclock_create:
	  	clock->local_period_mode = RADCLOCK_LOCAL_PERIOD_ON;
     but if a knowledgable user want to mistrust it, then have fn to act on opaque clock struct
	  to switch it off.  Eg as researchers, we may want to switch it off in an application.
	- this allows plocal to be ignored at the point of use, thereby enabling plocal algo branch to always be one without penalty. Does the algo still have its own plocal chooser?? seems not, there is no associated config parameter anyway.

## GET functions:

// TODO: should kill this? plocal is always used.
int radclock_get_local_period_mode(struct radclock *clock,
		radclock_local_period_t *local_period_mode)



# Functions with common template:    // could this be converted to a method?

int radclock_get_desiredparam(struct radclock *clock, *desiredparam)
{
	 if (clock->ipc_shm)  // if IPC active
		 get desired quantity from it, using SHM_{DATA,ERROR} macros below
		 protect access with generation trick
	 else 		// assume daemon dead, use copy in kernel instead (may be stale!)
		 get_kernel_ffclock(clock, &cest))		// get it
		 fill_clock_data(&cest, &rad_data);		// extract rad_data from it
		 *desiredparam = rad_data.relevantfield
	 return 0 on success, else 1
}

// Template applies to:
int radclock_get_last_stamp	(struct radclock *clock, vcounter_t *last_vcount)//	= last_changed
int radclock_get_till_stamp	(struct radclock *clock, vcounter_t *till_vcount)// 	= valid_till
int radclock_get_period			(struct radclock *clock, double *period)			// period = phat
int radclock_get_offset			(struct radclock *clock, long double *offset)	// offset = ca
int radclock_get_period_error	(struct radclock *clock, double *err_period)		//			 = phat_err
int radclock_get_offset_error	(struct radclock *clock, double *err_offset)		//			 = ca_err
int radclock_get_status			(struct radclock *clock, unsigned int *status)	// status = status

 - these get 7 out of 9 radclock_data fields, missing are phat_local{,_err}
 - raises question of naming of cest fields, why so different?  and role of plocal
 - currently radclock_get_{period,offset}_error  are effectively placeholders,
the value returned will not be correct since fill_clock_data has a bug.

// functions where kernel based answer is missing: needs implementation and more kernel support
// TODO:  check if kernel v0.4 has this already or not
int radclock_get_clockerror_bound		(struct radclock *clock, double *error_bound)
int radclock_get_clockerror_bound_avg	(struct radclock *clock, double *error_bound_avg)
int radclock_get_clockerror_bound_std	(struct radclock *clock, double *error_bound_std)
int radclock_get_min_RTT					(struct radclock *clock, double *min_RTT)

  - these recover all 4 fields of radclock_err instead of radclock_data within shm, with same names

// These macros from radclock-private.h are used to get the required SHM data
#define SHM_DATA(x)		((struct radclock_data *)((void *)x + x->data_off))
#define SHM_ERROR(x)	  ((struct radclock_error *)((void *)x + x->error_off))






########################################################################

### radapi-time.c

#define OUT_SKM	1024
  - seems like a nasty hack
  - aah, used only in raddata_quality wrt when now is already beyond valid:
    valid already encodes SKMscale, OUT_SKM is like a tolerance beyond it..

- used the shm (generation protected) via clock->ipc_shm, but no new shm stuff to learn here

int raddata_quality(vcounter_t now, vcounter_t last, vcounter_t valid, double phat)
  - is all about comparison against valid, which already encodes SKMscale
      - really?  seem to be just about poll_period in fact.
  - uses OUT_SKM to flag if now too far beyond valid (softer than believing in valid literally)
  - strangely, is an independent system to in_skm, even though both really about comparing now to valid..  Need to integrate them somehow.  This could make better sense if
      in fact valid only about poll_period .
	  - Ca fns:  uses this fn only
	  - Cd fns:  used this fn and in_skm

  - function that tries to access conditions, so that know if difference clock can be used etc
  - used by ffcounter_to_{abstime,difftime}_{shm,kernel}  fns directly,
    and hence all other fns here indirectly, to return quality measure.
  - big TODOs  and FIXME here, this is rather experimental, flagged as such needs fixing.  Clearly reliant on noone actually using the API...
  - dont understand issue with thread as per comment
  - considers case then now<last, but returns inappropriate  `bad error=3` value
  		- should use the old member of shm?  or just interpolate backward?
  - issue with handling vm issues compared to straight RADclock, maybe separate completely?
  - BUG: comments on other functions given return values based on what this fn returns, that they
    are wrong..   Return =1 not even possible right now.
  - one problem is that the arguments dont give full context in all cases, making quality inference difficult


static inline int ffcounter_to_abstime_shm	(struct radclock *clock, vcounter_t vcount,long double *time)
static inline int ffcounter_to_abstime_kernel(struct radclock *clock, vcounter_t vcount, long double *time)
  - TODO:  use of local_period_mode  illogical:  used in a _kernel routine even though raddata derived from cest, and so plocal just set to pbar anyway. I guess this is for completeness, so these routines do what they can, given they cant be certain what fill_clock_data does, makes sense, but need a comment about this.

static inline int ffcounter_to_difftime_shm	(struct radclock *clock, vcounter_t from_vcount,
		vcounter_t till_vcount, long double *time)
static inline int ffcounter_to_difftime_kernel(struct radclock *clock, vcounter_t from_vcount,
		vcounter_t till_vcount, long double *time)
 - TODO/Bug:  similar but different issue regarding local_period_mode.
     _shm dirn: This time the mode is not used!  plocal always used instead. No harm done in that if plocal defaults to pbar, but if not, no user ability to switch it off.  This is inconsistent, but I guess the point is that the argument for switching off is stronger for Ca than Cd, and dont want to have two different switches..  not clear wot to do here.  An advanced user can always bypass this fn and do what s/he wants.
     _kernel dirn, as before, plocal used, but is in fact just pbar from fill_clock_data
	  
 - calculates   from - till  , could be -ve
 - result variable `time` should be called `duration` or sth, stupid, and till should be to
 - note that these functions dont check SKMscale and that should be checked before call, and yet,
 in_skm is checked only After in radclock_{elapsed,duration}
 - returns Cd between {from,till}_vcount
 		- no order assumed
		- uses plocal
 - the call to radclock_get_vcounter only used to fuel raddata_quality()
   One could of course be asked to calculate the time between two raw stamps using
	data which is far remove from them in time..
 - his comment:  // TODO Stupid performance penalty, but needs more thought
	Ahh, one reason why this is stupid is that when called from radclock_{elapsed,duration}
	radclock_get_vcounter has only just been called anyway, but here we are calling it again!
	need to get around this stupidity !




static inline int in_skm(*clock, const vcounter_t *past_count, const vcounter_t *vc)
	- BUG:  now gets overwritten if vc=NULL !
	- used in diffclock routines  radclock_{elapsed,duration} only, not in the underlying
	  ffcounter_to_difftime_{shm,kernel}
	- should be called within_SKMscale()

//  These fns wrappers calling the _shm,_kernel versions of needed fns
int radclock_gettime(struct radclock *clock, long double *abstime)
	- nextBUG: comment is wrong, return long double, not timeval !  looks like cutnpaste error
	- 1024 is hardwired !!  should at least have used the #define OUT_SKM at top,
	  but better, need to access actual config value !
   - returns  1 if input past_count and vc (or current value if vc was NULL)



int radclock_vcount_to_abstime(struct radclock *clock, const vcounter_t *vcount,long double *abstime)

int radclock_elapsed(struct radclock *clock, const vcounter_t *from_vcount,long double *duration)
  - measures duration between the from input and a raw made inside the fn

int radclock_duration(struct radclock *clock, const vcounter_t *from_vcount, const vcounter_t *till_vcount, long double *duration)
  - measures duration = till - from


Both of these use in_skm to see if Cd applicable but is just returning an error if it is not, instead of reverting to Ca and issuing a warning.  In this case quality is ignored, instead of switching to a Ca evaluation of quality after reverting to that.



########################################################################
## radclock-read.c			// Defines generic read tsc and get vcounter fns:


#if defined(__APPLE__)
  set #define rdtsca(val) to be what it should be depending on arch,
  then define an inline fn  radclock_readtsc  to return it.

  - and if not apple?  dont need it??

// this fn declared in API header radclock.h
// just a wrapper to call the reading fn already attached to the (rad)clock
int radclock_get_vcounter(struct radclock *handle, vcounter_t *vcount)
{
	return handle->get_vcounter(handle, vcount);
}
  -handle->get_vcounter  initialized in ffkernel-fbsd.c:radclock_init_vcounter
  - should use `clock` as the dummy variable, not handle.
  - this is THE way that the counter is read
	 - daemon:  ONly occurence of "get_vcounter" is in string "radclock_get_vcounter"
	 - library:	only way read, but of course lots of setting up stuff, eg
	 	ffkernel-fbsd.c:		handle->get_vcounter = &radclock_get_vcounter_syscall;











########################################################################
RADclockrepo/examples

## radclock_get_time.c

/* Needed for accessing the RADclock API */
#include <radclock.h>

- SHM hidden inside functions, no explicit mention
- fails to call radclock_destroy at end, naughty naughty
- breaks convention of calling a variable of radclock type `clock`, instead
  calls is clock_handle, which is what the daemon calls its radclock_handle variable ! bad idea





## radclock_capture_packet.c

/* Needed for accessing the RADclock API */
#include <radclock.h>

- as above for bad convention on clock variable name

- fails to call radclock_destroy at end, or pcap tidyup



## radclock_pps.c

Doesnt use the API.























########################################################################
########################################################################
########################################################################
########################################################################
## radclock   c files


########################################################################

## stampoutput.c
All about outputing to ascii files:   .dat and .out

int open_output_stamp(struct radclock_handle *handle)
  - ascii specific file output function
  - tests if exists first and renames if so
  - creates, put in header block, hardwired to "version: 3"

void close_output_stamp(struct radclock_handle *handle)
  - flushes then closes in .dat case

int open_output_matlab(struct radclock_handle *handle)
  - ascii specific file output function
  - tests if exists first and renames if so
  - creates, put in header block
  - file written to in process_bidir_stamp

void close_output_matlab(struct radclock_handle *handle)
  - flushes then closes in .out case

void print_out_files(struct radclock_handle *handle, struct stamp_t *stamp)
  - writes out a 5tuple  (id + 4tuple) line to handle->stampout_fd   if selected
  		- strangely gets the radclock info from OUTPUT instead of the canonical version
		  (in peer or RAD_DATA) but it shouldnt matter
  - writes out a line to handle->matout_fd   if selected
  - stupid name, should be:  print_output_toasciifiles
  - called in process_stamp after call to algo




## stampinput.c
# these are defined elsewhere, this is where true fns mapped to methods
extern struct stampsource_def ascii_source;
extern struct stampsource_def livepcap_source;
extern struct stampsource_def filepcap_source;
extern struct stampsource_def spy_source;

- uses  #define INPUT_OPS(x) x->def    defined in stampinput_int.h
- defines precisely the fns declared in stampinput.h
    is_live_source
    create_source
  These ones all just wrappers for calling `methods` in stampsource_def !
    get_next_stamp
    source_breakloop
    destroy_source
    update_filter_source
    update_dumpout_source

The function  struct stampsource *create_source(struct radclock_handle *handle)
    - creates and rtns a stampsource variable, with its stampsource_def set to
      the right source fns (using INPUT_OPS). It knows which via handle->run_mode
    - called only by init_handle (called by main) in radclock_main.c:
            stamp_source = create_source(handle);





############ STAMP INPUT CODE ################
##  Takes into account the different needs according to the stamp input type,
##      ie  ascii;  tracefile;  livepcap;  spy
##  Ends up defining the functions (assigning fnpointers) and data assembled into
##  variable ?  of stampsource type, used in ?
##


## stampinput-ascii.c   [ good one to start, is simplest and still has my code ]

Warning do these need updating !!??  Actually can remove, see asciistamp_get_next
#define NTPtoUTC_OFFSET  2272060800lu  // [sec] since NTP base epoch [..
#define NTPtoUNIX_OFFSET 2208988800lu  // [sec] since NTP base epoch, currently!

- #define ASCII_DATA(x) ((struct ascii_data *)(x->priv_data))
  Here x will be a stampsource var in practice, only ascii part is the casting
  So its `get data from a place that doesnt know what it is, then tell it what it is`
  I guess otherwise stampsource would have to have an enum field, why not?
  Instead, a stamp_t stamp variable, is often passed which has a stamp_type_t member
  STAMP_{UNKNOWN,SPY,NTP,PPS} which contains this info.

struct ascii_data
{
    FILE *fd;
};
 - data structure used only in functions in this file

Special fns used for this type only
- int skip_commentblock(FILE *fd, const char commentchar)
- static FILE *open_timestamp(char* sync_in_ascii) /* opens,checks,skips comments */

- defines specific functions to fill stampsource_def member of stampsource variable
- Assignment and defn of external variable ascii_source at end of file
    used in stampinput.c:    extern struct stampsource_def ascii_source;
struct stampsource_def ascii_source =
{
    .init 				= asciistamp_init,
    .get_next_stamp 	= asciistamp_get_next, // this gets one line, hence a bidir stamp
    .source_breakloop 	= asciistamp_breakloop,
    .destroy 			= asciistamp_finish,
    .update_filter 		= asciistamp_update_filter,
    .update_dumpout 	= asciistamp_update_dumpout,
};


Bugs/Questions
- in asciistamp_get_next:
    can remove hack
    comment on better input format:  just change to stamp->type = stamp.type



## stampinput-tracefile.c       [ also pretty simple ]

#define TRACEFILE_DATA(x) ((struct tracefile_data *)(x->priv_data))

struct tracefile_data {
    pcap_t *trace_input;	/* Input trace */
    uint32_t data_link;	/* Link layer type (stored in dump file) */
    struct sockaddr_storage ss_if;
};

Special fns used for this type only
 static int get_packet_tracefile(*handle, void *userdata, radpcap_packet_t **packet_p)
	- uses only generic libpcap fns for extraction, as vcount already legally hidden in SSL
	- uses a local packet variable just to avoid confusing ** I think
    - uses local data variable for same reason?
    - dont understand why it assigns *packet_p = packet; at end, does it still point to the beginning of the buffer = beginning of the header?
    - data actually obtained by pcap_next_ex( , , ) 

 tracefilestamp_get_next
 	- based on get_network_stamp(,,,,) from create_stamp.c, where 3rd dummy argument is
      get_packet( , , ), assigned to get_packet_tracefile just above when called, a kind of circular calling....
    - finally called via err = get_packet(handle, userdata, &packet) in get_network_stamp

Finally fill struct stampsource_def tracefilesource_source with tracefilesource_{init,.}



Recall from pcap.h
	struct pcap_pkthdr {
		struct timeval ts; /* time stamp */
		bpf_u_int32 caplen; /* length of portion present */
		bpf_u_int32 len; /* length this packet (off wire) */
	};
Check out http://www.tcpdump.org/pcap.html  for a nice reminder of pcap stuff.





## stampinput-livepcap.c       [uses create_stamp.h  ]

Dont know what this is, cant find HAVE_IFADDRS_H in radclock/ or libradclock/
#ifdef HAVE_IFADDRS_H
# include <ifaddrs.h>
#else
# error Need ifaddrs.h or go back to using the ioctl
#endif

struct livepcap_data
{
    pcap_t *live_input;
    pcap_dumper_t *trace_output;
    struct sockaddr_storage ss_if;
};

Special fns used for this type only [worries about where to put vcounter in SLL headers

- int insert_sll_header(radpcap_packet_t *packet)
  - goal is to use a Linux SLL header to hold vcounter, replaces original linklayer header
  - resulting modified header is a radclock-internal storage format to give some MAC layer
    independence, doesnt relate to how get vcounter from kernel
  - doubles as a format compatible with pcap file saving
  
  - treats different DLTs, fixing etherlen for special cases,
  - DLT_EN10MB covers all Eth types:  http://www.tcpdump.org/linktypes.html
  - puts proper SLL fields in (8 bytes), leaving 8 byte addr field to hold vcounter
                       

Questions:
	- memcpy(dst,src,len)
                                
                                     
void set_vcount_in_sll(radpcap_packet_t *packet, vcounter_t vcount)
 - puts vcount into addr field of SSL header structure, from packet->payload, NOT
   in header as written here:
 typedef struct radpcap_packet_t {
	void *header;	/* Modified BPF header with vcount */
	void *payload;  /* Packet payload (ethernet) */    // ie, the frame (up to snaplen)
	void *buffer;
    
	size_t size;	/* Captured size */
	u_int32_t type;	/* rt protocol type */
	struct sockaddr_storage ss_if;	/* Capture interface IP address */
 } radpcap_packet_t;


static int get_packet_livepcap(*handle, void *userdata, radpcap_packet_t **packet_p)
  - gets the raw pkt data from pcap, and vount `separately`, via
    	err = deliver_rawdata_pcap(handle, packet, &vcount);   // was _rawdata_ntp
    but actual pkt reads not in here, see rawdata.c
    
  - inserts SLL header into packet, then vcount into it
  - saves header and payload to raw file via standard pcap routine :
  		pcap_dump((u_char *)traceoutput, packet->header, packet->payload);
  - [fixed] comment is wrong, this is not a callback to get_bidir_stamp, but rather to
    get_network_stamp as we see just below
  - Has KV=2 stuff!  need to fix wrt KV=3 and new FF code
  		- origin of the `CHECK ME` verbose
  - err codes:
    return(1) directly from deliver_rawdata_pcap
    // comment says data buffer empty, BUG: but from deliver_rawdata_pcap could be wrong RD_TYPE also
    else return(0)   // if get data
    

                   
Familar BFP stuff, but upgraded!
- int get_interface(char* if_name, char* ip_addr)
- int get_address_by_name(char* addr, char* hostname)
- int build_BPFfilter(*handle, char *fltstr, int maxsize,char *hostname, char *ntp_host)
- static pcap_t *open_live(*handle, struct livepcap_data *ldata)
               
               
===================
Define fnpointer fns (bit complicated)
static int livepcapstamp_get_next(struct radclock_handle *handle, struct stampsource *source,
	                              struct stamp_t *stamp)
  - as before just wraps generic fn, passing specific callback defined here:
	err = get_network_stamp(handle, (void *)LIVEPCAP_DATA(source),
                            get_packet_livepcap, stamp, &source->ntp_stats);
  - if fills in a couple of stamp fields, like stamp->{type,qual_warning},
    but why when get_network_stamp could have done it??  aah, this is
    get_network_stamp  doing it..
  - err returned is directly inherited from get_network_stamp()


- finally define  struct stampsource_def livepcap_source variable by assigning
	function pointers to workhorse functions here




## stampinput-spy.c

Seems to be an almost empty placeholder for some special kind of live capture.
Main info on this is in rawdata.c where is defined
 deliver_rawdata_spy(struct radclock_handle *handle, struct stamp_t *stamp)
 void fill_rawdata_spy(int sig)
 int spy_loop(struct radclock *clock_handle)

No special functions, and almost empty fnpointer fns used to define and declare
    struct stampsource_def spy_source

Best guess is this is the new way to implement Stratum_1.5 :
    - seems to get stamps by listening to sysclock periodically
    - but is it fully working yet, or is this work in progress? check V0.4




########################################################################

## create_stamp.c   [ ie stamp_t variable, not just 4tuple=bidir_stamp, but server info ]
Functions that do the real work of getting pkts, checking them, extracting
the needed info, and the queueing and stamp retrieval routines.

HISTORY:  this is where my name suddenly disappears from the main master to their
private versions.. !


// This is the element in the stamp queue, basically a stamp_t
struct stq_elt {               // elt = element?
	struct stamp_t stamp;
	struct stq_elt *prev;
	struct stq_elt *next;
};

struct stamp_queue {            // the queue itself, access to it and its size
	struct stq_elt *start;
	struct stq_elt *end;
	int size;
};

/* To prevent allocating heaps of memory if stamps are not paired in queue */
#define MAX_STQ_SIZE	20

long double NTPtime_to_UTCld(l_fp ntp_ts)   // to be obsoleted I think??

radpcap_packet_t * create_radpcap_packet()
void destroy_radpcap_packet(radpcap_packet_t *packet) // neat, all pts set to NULL
int check_ipv4(struct ip *iph, int remaining)
    - BUG: what is the "U" in test?? unsigned int =4 ?  should be 40 like with ipv6?
int check_ipv6(struct ip6_hdr *ip6h, int remaining)

get_valid_ntp_payload(radpcap_packet_t *packet, struct ntp_pkt **ntp,
    struct sockaddr_storage *ss_src, struct sockaddr_storage *ss_dst,int *ttl)
  - important comments and history about formats
  - backward compatible on old data formats, data lost anyway!
  - here using fact that DLT type field correctly set, so can switch on that
  - some IPv6 support in new SLL code, but still not perfect apparently
  - return NTP pkt, but what are the ss_{src,dst} params for??
  
 
Routines for extracting vcount from data returned by pcap
 int get_vcount_from_etherframe(radpcap_packet_t *packet, vcounter_t *vcount)
  	- original hack, timestamp actually hidden in pcap_hdr despite name
 int get_vcount_from_sll(radpcap_packet_t *packet, vcounter_t *vcount)
 	- works for both methods 2 (with libtrace) and 3 (modern)
 int get_vcount(radpcap_packet_t *packet, vcounter_t *vcount)
	- generic version to hide details, nice


Stamp-peer queue routines
 - about storing the algos per stamp data, I used to use lots of circular buffers
 - why is this here?  nothing to do with stamp creation, it is algo level, but
   main prog will need it to output to .out no doubt
 void init_peer_stamp_queue(struct bidir_peer *peer)
 	- why use calloc not malloc??
    - names reflect fact that doesnt create queue, it creates and puts it into
      a struct bidir_peer directly
 void destroy_peer_stamp_queue(struct bidir_peer *peer)   // HISTORY: late bug fix?
 
 
Check storage size right across IPv4 or IPv6
 int compare_sockaddr_storage(struct sockaddr_storage *first, struct sockaddr_storage *second)
 	- called compare, but main fn is to copy second arg to first
    - only used in this file
 int is_loopback_sockaddr_storage(struct sockaddr_storage *ss)
 int bad_packet_client(struct ntp_pkt *ntp, struct sockaddr_storage *ss_if,
		struct sockaddr_storage *ss_dst, struct timeref_stats *stats)
    - I think this is about stopping RADclock collecting pkts sent to it as a
      radclock server, or similar symptom
 int bad_packet_server(struct ntp_pkt *ntp, struct sockaddr_storage *ss_if,
		struct sockaddr_storage *ss_src, struct timeref_stats *stats)
    - oops, seems to throw out server pkts with LEAP_NOTINSYNC set , good for radclock
	   if these are LI=3 set in the middle of nowhere,
	   NOT GOOD for SHM experiments !  need to disable for SHM !
        
        
		  
================================================================
Create stamps  [ ie stamp_t variable, not just 4tuple=bidir_stamp, but server info ]
NTP pkt timestamp reminder: in server reply: (org,rec,xmt) -> (Ta,Tb,Te)

int insertandmatch_halfstamp(struct stamp_queue *q, struct stamp_t *new, int mode)
	 - DV modified April 2017, for detail, see new comment block and comments, begins:
/*
 * Insert a client or server halfstamp into the stamp queue, perform duplicate
 * halfstamp detection and client request<-->server reply matching, and trim 
 * excessive queue if needed. 
 * The queue allows many client requests to be buffered while waiting for their
 * replies, and can cope with both reordering, and loss, of both client and
 * server packets.
 */
    - old code relied on keeping queue in id-order and inserting in right place
	 	- bad idea since id not guaranteed to be in order
		- several bugs. `justbeforetheendMattversion` fixed them maybe, but cost of v-high complexity
		- rewritten to make id for matching only, no order assumed or used
			- also returned -1 on duplicate! was passed up all the way to thread_data_processing/main
             and caused radclock to exit !!  why?? I changed to LOG_WARNING
		  - was called insert_stamp_queue
	 - fn actually called by push_stamp_{client,server}, inside update_stamp_queue
    - it _ bidir NTP specific even though is a queue of stamp_t, no spy support :
        - could be called insert_and_match_NTPbidirstamp_queue
        - could be made generic to handle 1588 maybe..
        - is spy supported anyway by bypassing the queue completely since no matching needed?
	 - could integrate with get_network_stamp to avoid repeated work.
	   Currently need to update more than once trying to get a full stamp if live, but still,
		once you get a fullone, then you exit immediately then basically call get_network_stamp
		immediately..  still, nice to have the id, and temporal, code separated.
     

 int push_client_halfstamp(struct stamp_queue *q, struct ntp_pkt *ntp, vcounter_t *vcount)
	 - creates local stamp var, use to store halfstamp info until call to insert_stamp_queue
    - DV modified: was called push_stamp_client
    - ** this is where the unique stamp id extracted ! sysclock outgoing in xmt field
      In old get_bidir_stamp code (now called get_network_stamp) this was
      key = ((u_int64_t)ntp->xmt.int_part) << 32 | ntp->xmt.fraction; // ~Ta

 int push_server_halfstamp(struct stamp_queue *q, struct ntp_pkt *ntp,
		vcounter_t *vcount, struct sockaddr_storage *ss_src, int *ttl)
	 - creates local stamp var, use to store halfstamp info until call to insert_stamp_queue
	 - DV modified:  was called push_stamp_server
    - ** this is where the unique stamp id extracted !  Here same ts is in org field
    
	 
 int update_stamp_queue(struct stamp_queue *q, radpcap_packet_t *packet,
		struct timeref_stats *stats)
    - aah, this is the higher level one, grabs a radpcap_packet_t from either dirn,
      gets the vcount and NTP payload from it, finds out if NTP pkt mode is Client or Server,
      the call relevant bad_packet_{,} sanity checks then push fns to insert it into the queue
    - this is where we go from radpcap_packet_t level to stamp level effectively
    - err codes:
  		 -1:  error from get_vcount, and then return(-1)
		  0:  success in getting, vetting, and pushing halfstamp
		  1:  error in any of getting,vetting, pushing, or not even an NTP pkt
              ie failure in putting this pkt in.
        Note typical case is get err directly from push_stamp_ which inherits directly from insert_stamp_queue

 int get_fullstamp_from_queue_andclean(struct stamp_queue *q, struct stamp_t *stamp)
 	 - DV modified April 2017, for detail, see new comment block and comments
	 	- rewritten from scratch
	   - was called  get_stamp_from_queue
	   - old code assumed order based on id, and had other bugs
    
	 
	 
	 
   
int get_network_stamp(struct radclock_handle *handle, void *userdata,
	  int (*get_packet)(struct radclock_handle *, void *, radpcap_packet_t **),
	  struct stamp_t *stamp, struct timeref_stats *stats)

 - DV modified April 2017:
 	- rationalising comments esp on error codes, fixed other comments
	- fixed memory leak potential with packet
	- put live logic into a if else for clarity
		  
 - top level get stamp fn, generic for live or dead pcap, and also uses get_packet
   callback which is also set to the right source (ascii, live/dead pcap)
 - Note TODOs at end that will need attending to.
 - live/dead dependance:
     - hence some paths not followed on replay!  but! actual stamp functions are
	    all agnostic, they will be tested on replay using same pkts, its just
		 that WHen they become available will be different.
     - if live apparently woken up by trigger thread, which justifies the short term usleep code
	  
 - Top level function that almost does everything
   	  get_packet(handle, userdata, &packet);    // getting new packets
      	 err = deliver_rawdata_pcap(handle, packet, &vcount);  //  rtn 1 if buffer empty
      stats->ref_count++;
      case dead
         update_stamp_queue(peer->q, packet, stats);  // converting to stamps, putting in queue
      case live
         // multiple attempts to catch an outgoing, then the incoming
         update_stamp_queue(peer->q, packet, stats);  // converting to stamps, putting in queue
      
      get_stamp_from_queue(peer->q, stamp);        // if err=0, getting a full stamp out.
      verbosity:
      	stamp queue popping
        NTP server sanity change
        
 - err codes:  [ get translation here, since now err={0,1} mean sth specific wrt stamp finding,
                 so actual strange errors get mapped to err =-1 ]
   -1:  if get err from get_packet (which only has err = 1), and then return(-1)
        or if get err=-1 from update_stamp_queue,  will exit radclock !!
    0:  got fullstamp, is ready
    1:  inserted halfstamp         :  this is clunky, this is a normal output, yet it is
        classed as an `error`, why not return something better directly?

 					 
	BUG:	refid string not coming out right, some fields have a - in front.  Tried using %u instead of %i but created nonsense... reverted
					 

Err type checking:
 get_packet:   1: no rbd data or error  ( see get_packet_livepcap comments above )
               0: got data
 
 update_stamp_queue:  1: direct from get_valid_ntp_payload  [ "not an NTP packet" ], or invalid mode, or output of any of bad_packet_*, push_stamp_*
                      0: output of any of bad_packet_*, push_stamp_*
                      












########################################################################

## rawdata.c       [ rdb = raw data bundle ]

This file now pretty well understood.

This handles getting low level raw data in different source cases and putting it
into a list, called the raw data queue, and getting it out again.

inline void insert_rdb_in_list(struct raw_data_queue *rq, struct raw_data_bundle *rdb)
  - rq is the rd queue itself
  - this is time critical:  need to get raw data available ASAP.
  - no documentation on role/difference of the start versus start->next,
  - insertion is by the head, ie at start, and rdb->next means the next closer to head!
    and this field = NULL at the head itself.
  - called by fill_rawdata_{ntp,spy} only
  - lock confusion:  comment on lock free is confusing, mutex IS used!! corresponding comment
    in free_and_cherrypick, claims is lock free since pcap_loop adding to the list,
    but we see here that lock is used..  Is it that this code has a lock, but the
    datastructure ITSelf is not locked somehow??  Look at comment in radclock_daemon,
    looks like locks added later due to some NTP control pkt issue, and comments not up to date.
  - comment about chronological order:  simply means I think that insert from head,
    remove from tail, so that extraction will be delivered in the same order as
    arrival (FIFO).  If packets arrived out of order, deal with that elsewhere.
 	
    
void fill_rawdata_ntp(u_char *c_handle, const struct pcap_pkthdr *pcap_hdr, const u_char *packet_data)
	- callback fn for pcap_loop call within capture_raw_data()
    - defines the rdb to hold new data, then puts in list using insert_rdb_in_list
    - pcap data: mapping from what is passed to it from pcap_loop to rdb:
    		pcap_hdr    --> rd_pcap.pcap_hdr
        packet_data	--> rd_pcap.buf
    - vcount data: obtained by call to the lower level library fn extract_vcount_stamp
      (see radclock-private.h ).
	 - HISTORY: semantically related function deliver_rawdata_pcap  used to be called
	   deliver_rawdata_ntp, but is more pcap oriented, hence the name change.
		To match this, changing this fn name to fill_rawdata_pcap  (Mar 2019)


void fill_rawdata_spy(int sig)
	- fill a spy stamp by just reading the counter and sysclock using gettimeofday
    - data placed in rd_spy member of rdb but type is tellingly rd_spy_stamp
    
int spy_loop(struct radclock *clock_handle)
	- named by analogy with pcap_loop
	- sets up a usleep every second to check to see if need to collect new spy data
    - good place to learn about signals

int capture_raw_data( struct radclock *clock_handle)
    - uses relevant _loop function when possible, provides errors for other modes
	- finally where the pkt is actually read, using pcap_loop in infinite loop, with callback fill_rawdata_ntp to grab the data bundles (rdb)
    - useful comment here (at last...)
    - SYNCTYPE_{PIGGY,NTP} treated in same way (case statement falls through!)
      since processing pkts the same regardless of who sends them.
	 - return codes:
	 	  -1:  if various errors occur (eg RADCLOCK_SYNC_NOTSET,  or if pcap_loop returns this)
		  -2:	 if pcap_loop returns this (due to call to pcap_breakloop)
		   0:  if in 'other' cases and no error, currently basically spy



struct raw_data_bundle* free_and_cherrypick(struct raw_data_queue *rq)
    - this pops the head of the raw data list, after first removing stuff that
      has already been read.  But, who would read but not take off? maybe just
      a fail safe, always take off the one youve just read, but may as well do a clean
      each time to be safe. Not time critical: raw data already recorded.
      Note read==1 if already read
    - insert_rdb_in_list handles case of empty list, of course, but here list is
      never left empty (after first rd obviously), which is main reason why stuff
      that is already read can still be in here!
      Comment claims that clearing it could confuse pcap_loop, no idea why.
      Comment near end clears up most of this but not all.
    - ahhh, is this all designed to work with multiple sources simultaneously?? a list
      with a mix of different source types?  see comment above deliver_rawdata_pcap.
      If so, another reason why could have more than one read already.
      Dont think so though, since free_and_cherrypick is agnostic, so when someone
      calls it, will deliver the next one, not unneccesary of desired source type.

    - goes through all elements except first from end up, clearing out if
      already read, until first one that is not yet read, if any.
        - any already read ones higher up would be cleared out next time (shouldnt be..)
    	- not sure why it frees the buf but not other fields before freeing entire rdb
    - bothers clearing rdb_tofree->next = NULL that gets freed just after anyway,
      because OS keeps track of any pointers pointing to alloated memory,
      wont let you kill them unless there is noone left looking?? Check!!
      Maybe just a general safety rule that never leave a ptr that should be dead or
      unused pointing to anything but NULL
    - lock confusion: see comments under insert_rdb_in_list
    - works for any rd type, currently just RD_SPY and RD_PKT
	


int deliver_rawdata_spy(struct radclock *clock_handle, struct stamp_t *stamp)
	- simple eg of how to transfer rd out of rd buffer list, into stamp structure


int deliver_rawdata_pcap( struct radclock *clock_handle, struct radpcap_packet_t *pkt, vcounter_t *vcount)
	- gets raw data from list via  rdb = free_and_cherrypick(rq);
	- finally, this is where the radpcap_packet_t variable actually filled
    	- first puts [ pcap hdr ; frame ] into pkt->buffer
		- then sets other ptrs:  header is same as buffer, payload = frame
        - size is size of [ pcap hdr ; frame ] together
    - mapping from data in rd_pcap member of rdb to radpcap_packet_t  structure is therefore:
    	- pcap_hdr --> header
          buf      --> payload
        Seems to be an unneccesary duplication somehow...
    
          

Remember:
- typedef struct radpcap_packet_t {
    void *header;	/* Modified BPF header with vcount */
    void *payload;  /* Packet payload (ethernet) */ //stupid name, this is ethernet frame
    void *buffer;         // points to start of everything, ie [header; frame]
    size_t size;	/* Captured size */  // bad name, is size of everything, ie of buffer
    u_int32_t type;	/* rt protocol type */
    struct sockaddr_storage ss_if;	/* Capture interface IP address */
} radpcap_packet_t;

struct raw_data_bundle {
	struct raw_data_bundle *next;	/* Next buddy */  // seems to be the next oldest
	int read;						/* Have I been read? i.e. ready to be freed? */
	rawdata_type_t type;			/* If we know the type, let's put it there */
	union rd_t {
		struct rd_pcap_pkt rd_pkt;
		struct rd_spy_stamp rd_spy;
	} rd;
};

struct rd_pcap_pkt {
	vcounter_t vcount;				/* vcount stamp for this buddy */
	struct pcap_pkthdr pcap_hdr;	/* The PCAP header */
	void* buf;						/* Actual data, contains packet */
};

// rdb queue itself
struct raw_data_queue {
	struct raw_data_bundle *rdb_start;
	struct raw_data_bundle *rdb_end;
	// XXX arbiter between insert_rdb_in_list and free_and_cherry_pick.	Should
	// not need a lock, but there is quite some messing around if hammering NTP control packets
	pthread_mutex_t rdb_mutex;
};



########################################################################



## client_ntp.c   // no .h file  // HISTORY:  new fn, content was previously in pthread_trigger.c
This is all about the RADclock NTP pkt sending code.
It does everything, the stack does nothing except transport the pkt.
Of course our kernel code gives us a raw TS as it passes, but is entirely independent.

#define NTP_MIN_SO_TIMEOUT 995000		/* 995 ms, not 100 ms */  // VK change I think

#ifdef HAVE_POSIX_TIMER
timer_t ntpclient_timerid;
#endif
extern pthread_mutex_t alarm_mutex;
extern pthread_cond_t alarm_cwait;


// TODO Ugly as hell
static long double last_xmt = 0.0;
   - records last xmt value to check within create_ntp_request if different
      or not (hence if key will be unique)

int ntp_client_init(struct radclock_handle *handle)
	- sets up SIGALRM for periodic wakeups for pkt sending, via catch_alarm(int sig)
	 which uses pthread_cond_signal(&alarm_cwait) to communicate with ntp_client()
	- seems to set timers to be poll_period/2, but is this too early?? or is this
	  THE setting of a periodic timer, so need to set to that to avoid missing pkts regardless of phase as the timer drifts ?
	- Unrelated (I think) to timer (set to NTP_MIN_SO_TIMEOUT) to avoid blocking for lost pkts
	- sets first alarm in 0.5s, then every poll_period.  Dont know why 0.5 is good.
	  In fact no pkt will be be sent until this poll_period interval is revised anyway
	  in ntp_client due to BURST and algo poll control considerations.

int create_ntp_request(struct radclock_handle *handle, struct ntp_pkt *pkt,
		struct timeval *xmt)
   - creates the NTP request pkt
   - pkt->reftime field populated in userland using RADclock !
   - pkt->xmt            "          "      "    "  a bit after, with uniqueness check
    based on comparing with last time
   
static int unmatched_ntp_pair(struct ntp_pkt *spkt, struct ntp_pkt *rpkt)
   - compares usual key (outgoing timestamp)
   - // HISTORY, wasnt static before
   
int ntp_client(struct radclock_handle *handle)
   - this is the NTP client pkt sender code, top function
	- its goal is not just to send a pkt and consume it on return, it tries to get
	  a successful matched pair, by making multiple attempts, and checking for key match.
	  This is the idea of having a lightweight yet not bad matching code at the sending
	  level, entirely independent from the real stamp matching, because if it fails, not critical,
	  and also it is inherently different, because you can do sth about a match failure by trying again.
	  It is just good enough to get some intelligence into period selection, and lost pkt avoidance.
   - implements initial burst by adjusting period
	
   - has burst at start if handle->ntp_client->burst set >0
	  - initialized to NTP_BURST in pthread_dataproc.c
	  - results in adjusted_period = BURST_DELAY, which controls timers
	    - this is how implement burst ! just reduce period, then do as normal
	 	 - will get the shorter adjusted_period for the first Burst pkts
	  - unimplemented option for algo to also modulate adjusted_period via starve_ratio variable
	  - need more logic here to avoid burst overlapping with normal packets when poll_period small,
	    eg with poll_period=1 shouldnt have bursts, need logic to adjust when initializing.
	    Should incorporate RTT in safe way, eg avoiding in warmup ?
	    Ditto for avoiding bursts?  or just work with adjusted_period and treat in uniform framework
	
  Before pkt sending attempt loop:
     - reset timer interval to adjusted_period [s]  (same timer)
     - then wait til get message (from alarm handler capturing signal) that next alarm has
     gone off:    pthread_cond_wait(&alarm_cwait, &alarm_mutex);
     - note decoupling, the alarms are just alarms, they dont cause sleeping, hvae to
     do that separately
	
  Enter attempt loop, which tries to get a successful stamp in `attempt` attempts
	- used to be 3, but VK set to 1 I think
	- this is NOT the burst code but is done for all sending attempts, including pkts in a burst
	- it is the new approach to try to get around losses by retrying everytime, very important
	  if poll_period large to maintain minimum stamp rate for algo
	
	  
	- each attempt waits for returning pkt by blocking on socket
	  - if get no reply (timeout), move to next attempt
	  - if get sth break out of attempt loop
			if not a match verbose(LOG_WARNING, "NTP client got a non matching pair. Increase socket timeout?");
			
	  - Dont understand:  if times out, or if get non-matching pair, then what happens to the pkt if it ever returns?  even if it is stored in a queue, if recvfrom only ever pops one, then wont that lead to long sequences of non-matching events?
	      Need to understand sendto and recvfrom better.
  
	 - has adaptive timeout code at end to ensure timeout <= adjusted_period / 3
	   Should include RTT: see above .
		
	The matching code is in unmatched_ntp_pair() and is based on the usual key, but lighter code !
	 	- do retries generate copies? I dont think so:
	 		- each attempt calls create_ntp_request() again, will get new key
	 		- is this disabled in experiment versions??
	 		- printout to find out !!
   
// TRIGGER: partial flow of control
pthread_mgr.c					thread_trigger				[ infinite loop, signal DATA_COND, usleep]
pthread_trigger.c				 trigger_work  						[ doesnt do much ]
client_ntp.c					  ntp_client                     [ tries to get a valid stamp ]















   
########################################################################
#####################  process thread summary

Recall that whereas a process is a unit of resources, a thread is a unit of
scheduling and execution.

A standardized interface for thread implementation is POSIX Threads (Pthreads),
which is a set of C-function library calls.

## pthread_mgr.h    // Defines thread codes and declares all thread fns

// these codes used to index into handle->threads[] array
#define PTH_NONE				0
#define PTH_DATA_PROC		1	// main radclock code, processing rdb, stamps, algo, reporting
										// after algo runs: update ipc, kernel clock data, adjust sysclock
#define PTH_TRIGGER			2	// NTP client, ie radclock's NTP pkt sender and receiver
#define PTH_NTP_SERV			3	// NTP server:  ie, serving the requests of external clients
#define PTH_FIXEDPOINT		4	// deprecated fixed point precision for kernel stuff?
#define PTH_VM_UDP_SERV		5	// VM stuff
'

  - PTH_{}_STOP flags
  - start functions:  start_thread_{,,,,,}(struct radclock_handle *handle);
  - init functions:         thread_{,,,,} (struct radclock_handle *handle);
       - fixedpoint one in .c instead, because more private?
		 - extra fns:
		 		trigger_work
				process_stamp !!  why need to declare here?
  - more init funcs (no explanation as to difference from others)
     	void init_thread_signal_mgt();
		int trigger_init(struct radclock_handle *handle);

	

// late addition was this:   allow compilation on Darwin?
/* Posix Timers and Alarm routines */
void catch_alarm(int sig);
#ifdef HAVE_POSIX_TIMER 
int set_ptimer(timer_t timer, float next, float period);
int assess_ptimer(timer_t timer, float period);
#else
int set_itimer(float next, float period);
int assess_itimer(float period);
#endif

Should look at
#include <pthread.h>




## radclock/pthread_mgr.c

Main thread management code, functions to get threads up and running.
They all seem to have sleeps, either explicit or hidden.
Important comments here but full picture not explained.


void init_thread_signal_mgt()     // called by trigger, data_proc, fixedpt, others
	- simple thing to avoid existing signals inherited from main (but future ones?)
	- aah, confusing comment, does not mean inherit specific signal instances, it means inherit list of signal types it will listen to.
	- point of this fn is to block ALL signals, and it is called in ALL thread-work functions.
	  This means that any signal delivered to process will go to main thread only, for simpler
	  centralised processing.
	
	
// Template of all thread routines:
		- init_thread_signal_mgt();  							// block existing signals
		- handle = (struct radclock_handle *)c_handle;  // type-correct copy of global with standard name
		- initialize thread
		- while ( STOP flag not set for this thread )   // loop to do work until thread STOPped
				sleep
				do the work for this round
	 	- pthread_exit(NULL)		// terminate verbosely


// in fact in virtual_machine.c, included here for completeness
void *thread_vm_udp_server(void *c_handle)
  - more complex as have to open sockets, bind, set timeouts
  - uses globaldata_mutex to protect work

// in fact in pthread_ntpserver.c, included here for completeness
void *thread_ntp_server(void *c_handle)
  - much more complex as have to open sockets, bind, create packets, set timeouts..
  - no mutex, no extra signalling
  - comment on no need for extra thread for connectionless sockets, not sure what the pt is


// Extra stuff beyond template:
void *thread_trigger(void *c_handle)
  - if error in init stop ALL, not just trigger
  - in while
  		- if get wakeup_checkfordata == 1  'signal':
		    - not yet processed, so signal DATA_PROC again
			 - usleep(50)  // later sleep comment outdated
			   - if DATA_PROC grabbed the mutex really fast, then maybe already processed
				  in the time it takes to get back to the top of the while loop here, so wont sleep !
			 	- or, could still be unprocessed when return from sleep, but thats ok, just get even
			     more raw data, when eventually DATA_PROC runs, will have a larger block of data to process
		- lock wakeup_mutex
		    - call trigger_work [from pthread_trigger.c] to do work
			 	  work is to get raw data via ntp_client(), which has some blocking
				  as it makes attempts for a successive outgoing,incoming match, but WIll return
		    - set wakeup_checkfordata == 1  // data ready for you!
	   	 - signal DATA_PROC again with pthread_cond_signal(&handle->wakeup_cond)
	   - unlock wakeup_mutex
		  [ now DATA_PROC can wake, and it will see that data is available ]
	 
	 
// This is where main routine process_stamp runs if RADCLOCK_SYNC_LIVE
// Extra stuff beyond template:
void *thread_data_processing(void *c_handle)
  - init is: init_peer_stamp_queue(&peer) // this is almost a full RADclock init?  neat..
  - in while
    - all protected by wakeup_mutex
  	 - weaksignal TRIGGER that are busy by setting handle->wakeup_checkfordata = 0
	 - check again if have STOP flag, not sure why, though has big comment
    - pthread_cond_wait(&handle->wakeup_cond, &handle->wakeup_mutex);  //


  Simplified Code:
    init_peer_stamp_queue(&peer);  			// creates stamp queue:
    handle->active_peer = (void*) &peer;	// give to handle
	 while (not PTH_DATA_PROC_STOP)		// main RADclock data consumption loop!
	     // confusing comment, the `release it' is hidden inside normal pthread_cond_wait behaviour
	 	  lock wakeup_mutex
	 	  handle->wakeup_checkfordata = 0; // tell TRIGGER did wake, lock, available stamps processed
		  unlock if PTH_DATA_PROC_STOP set in interim to avoid a pthread_cond_wait() can never wake from
		  pthread_cond_wait(&handle->wakeup_cond, &handle->wakeup_mutex);  // wait signal from TRIGGER
		  // BUG: wrong comment! something -> nothing
		  do {
				err = process_stamp(handle, &peer);   // process next raw, maybe stamp, if available
		  } while (err == 0);   // loop until all available stamps processed, exit when no more (err=1)
	 
	 destroy_peer_stamp_queue(&peer);
 
  
// Recall process rawdata return codes:  [ for a single call, processing a single stamp ]
	  -1: nasty pb, exit
	   0: success, got a stamp and processed it  (being caught by sanity counts as processed)
		1: no error, but no stamp to process


  
void *thread_fixedpoint(void *c_handle)
	- nothing beyond template, no mutexes, no signaling since no other thread competing




// All similar, use
//		pthread_create    (fills entry of threads[] array, passes thread 'main' function )
//		thread_attr_{init,setdetachstate}
//    All set to be joinable
int start_thread_NTP_SERV		(struct radclock_handle *handle)
int start_thread_VM_UDP_SERV	(struct radclock_handle *handle)
int start_thread_DATA_PROC		(struct radclock_handle *handle)
int start_thread_TRIGGER		(struct radclock_handle *handle)
  - more complex one, access its scheduling and adjust to max priority and FIFO
int start_thread_FIXEDPOINT	(struct radclock_handle *handle)







## pthread_trigger.c		// PTH_TRIGGER
This is about the regular triggering of clock inputs, and then status updates.
It is written for all cases:  SYNCTYPE_{SPY,PIGGY,PPS,1588,NTP}
but only serious work is for NTP, ie the RADclock NTP client (NTP pkt sending code)
Use dummy periodic trigger for 1588.

pthread_mutex_t alarm_mutex;
pthread_cond_t alarm_cwait;

// NTP client declarations  ( from client_ntp.c )
int ntp_client_init(struct radclock_handle *handle);
int ntp_client(struct radclock_handle *handle);

int dummy_client()
  - does nothing but wake up every 500ms, currently used only for SYNCTYPE_1588
  
// Generic triggering and timer stuff - don't get this.  Used in pthread_mgr.c
void catch_alarm(int sig)
#ifdef HAVE_POSIX_TIMER
	 int set_ptimer(timer_t timer, float next, float period)
	 int assess_ptimer(timer_t timer, float period)
#else /* ! HAVE_POSIX_TIMER */
	 int set_itimer(float next, float period)
	 int assess_itimer(float period)
#endif

Timer stuff in sys/sys/time.h  :
 388 struct itimerval {
  389         struct  timeval it_interval;    /* timer interval (period)*/
  390         struct  timeval it_value;       /* current value (gap til next) */
  391 };
int setitimer (int which, const struct itimerval *new, struct itimerval *old)
int getitimer (int which, struct itimerval *old)
unsigned int alarm (unsigned int seconds)    // set alarm to expire in seconds [s]
  - implemented using setitimer with (value,interval) = (seconds,0)

  
https://www.gnu.org/software/libc/manual/html_node/Setting-an-Alarm.html
 - Timers get created once per process. You have to retgset them to give them new deadlines.
 	- when they expire, SIGARLM will be sent to process
 	- think of as a countdown, the period just means when one expires, automatically
 	  set a new one it_interval into the future - a new countdown.  The current
 	  alarm is always the time remaining til the next alarm.
 - period (it_interval) period of periodic alarms, if set to zero, alarm only sent once
 - next   (it_value)		gap from now til first alarm: if set to zero, alarm disabled
 - {set,get}itimer come from BSD, alarm comes from POSIX.1

Timers:
  set_itimer(next, period)
    - just a wrapper to call setitimer(ITIMER_REAL, &itv, NULL) with
           next mapped to it_value      tval          and
			  period         it_interval   tval
         
  set_ptimer(timer_t timer, float next, float period)
    - analogous, but format is timespec not tval
         
  assess_itimer(period)
    - calls getitimer(ITIMER_REAL, &itv);  extract  itnext and itperiod  as float
    - if (itperiod != period)
			return (set_itimer(itnext, itperiod));
	// [fixed] BUG: should change to period otherwise never does anything (except reset origin?)!

  assess_ptimer(timer_t timer, float period)
    - analogous, but this time gets it right:
    -	if (ptperiod != period)
			err = set_ptimer(timer, ptnext, period);


 - assess_ functions used to reset timers following a change in sending poll_period
   controlled by a subsampling parameter called starve_ratio set in ntp_client.
	This is supposed to be controlled by the algo but this was never done?
	I think the idea was that sending rate should be reduced if the server cant handle it.
	Or, perhaps this was supposed to be a nicer way to modulate the actual poll_period, under algo control, without having to do brutal restarts.  Need to check algo code.
	Anyway, this was also the beginning of long-wanted flexibility to allow sending rate
	flexibility for optimization (dangerous but desirable)

int trigger_work(struct radclock_handle *handle)
  - does very little except call correct real-work fn depending on conf->synchro_type)
  		- for SYNCTYPE_NTP is ntp_client()
  - set status to starving if last stamp more than skm_scale/2 (ADD_STATUS(handle, STARAD_STARVING);)
  // TODO: here about going further and moving from starvation to UNSYNC if too bad, and back
  	- Important link here about status indpendent of pkt arrivals, must expand:
  	   - logic is:   if ?SKMscale/2 since last update , set STARAD_STARVING and LOG_WARNING
  	   - note this is done after ntp_client returns, so does it ONly rtn when if has found a match?
  	     that would make it roughly the same as `have new stamp`, but not the same..
  	- need a defn of starvation, refers to returning pkts or successful stamps?
  		- get_next_stamp is the custodian of no stamps coming in, will run even if algo never
  		  does, so why need sth here?  normally trigger is supposed to be dumb. Anything
  		  it can learn (no returning pkt, no match) the stamp queue can learn as well. Or would get_next_stamp not run?  Dont want 2 places tracking starvation.
  		- even if detection capability unique here, starvation is a fn of poll_period, of the
  		 number missed, not just time, similar issue to next_expected, should use same logic
		- STARAD_STARVING only used in trigger_work, and radclock_main as an init
		  - comment claims defn is quality input, but probably should just be stamp input per-se,
		    already have quality measures.

TRIGGER TODO:
  - fix infinite loop/sanity check bug I found in create_ntp_request
     - reqires buildworld to test, leave for now  Verbosity is in place to trap it
  - make retry tuning and limiting smarter
  		- already a fn of RTT via timeout in a fairly safe way, but stupid
  		- currently adjusting timeout, and checking maxattempts, on every pkt, too much work !
  		  still, only on VERB_DEBUG do we print it..
  		- need a static (or owned by pthread_mgr and passing through) target parameter variable, which is only tested and reset when needed?
  
TRIGGER TODO DONE:
	- // BUG s fixed
		- in assess_itimer   (itperiod --> period in set_itimer call)
		- in verbosity in NTP reception, got id from xmt not org
		- had usleep in get_network_stamp even if on the last attempt
		- starvation defn was deleting status on every pkt
	- check timestamps put into packets:  ref and Xmit, ensure uniqueness of xmit
	   > uniqueness of nonce code added, new verbosity etc and comments about
	      create_ntp_request ensuring uniqueness  [ if counter resolution low, this causes deliberate errors, but can track such cases with verbosity and invert if needed ]
		> removed tval intermediary (as per old comment) and created misc.h:UTCld_to_NTPtime() instead to do it directly, tested it.
		  Makes it super unlikely that the +1 to enforce uniqueness will ever be needed/cause a problem.
		> renamed  timeld_to_timeval --> UTCld_to_timeval  in the process to make it clearer.  This is still needed as the fn requires
		  to return  tval, also used elsewhere.
		  // TODO:  in pthread_ntpserver.c, also need to upgrade to UTCld_to_NTPtime and remove tval dependence
		  // TODO:  use of my new fn in the server thread
  	- ensure BURST and retry dont collide
		> BURST just influences adjusted_period, everything then in terms of that
	- make BURST and poll_period consistent
		> use min, to avoid stupid case of initial BURST being less intense that normal polling!
		  and also, since BURST=2 normally, if poll<BURST then want to disable BURST anyway since not needed, this achieves that neatly.
	- created  NTP_INTIAL_SO_TIMEOUT  so it could be large to catch first pkt
	  before RTT even measurable
	- set NTP_MIN_SO_TIMEOUT 150000  which works on my VM-on-Mac
	- understand timers:  difference between socket TO, and alarms   [must read up sendto, recvfrom]
		> mainly done. The periodic alarm is used to generate the NTP request grid
		  The TO is quite separate, it just determines the spacing between retries
		  of a Single grid point, if they are allowed to occur.
	- eliminate error Ive been seeing
		> Not an error, it is the natural result of get_network_stamp within process_stamp
		  having exhausted all raw data available, with no more forthcoming, as
		  TRIGGER is blocked until process_stamp exits and resets wakeup_checkfordata
		  See notes in RADclockDoc_threads.
		  Have added more verbosity in get_network_stamp to track progress and different exit possibilities:
		     - with a fullstamp  ( and say how many attempts it took )
		     - run out of attempts
		     		- with no data at the end [typical case]]
		     		- with data (but obviously another lonely half-stamp)
		  Also added verbosity to check rdb insertions and deletions, all working nicely,
		  now commented out.
		Reset maxattempts from 10 to 20 (must be even number!) for slow VM-on-Mac
		Hmm, this is important, if miss it, wont see that stamp for another full grid pt!!
		so the next time round, pick one up on first attempt, then have a go for the current one.
		The pattern you need to see is, each time PROC runs:
		       i) find full stamp   [ every time!]
		       ii) see the algo run on it
		       iii) return to get_network_stamp  and see it run out of attempts with no data
			- need to test this for different conditions to tune maxattempts !!
  - UTC -> NTP timestamp conversion, frac field uses:
     	pkt->reftime.l_fra = htonl(reftime.tv_usec * 4294967296.0 / 1e6); //  ie * 2^32 *1e6
         This removes the #mus encoding of tv_usec, replaces it with number of 2^-32 units,
         ie a 32bit `bintime'   Consistent with NTP --> UTC code in   NTPtime_to_UTCld()
       - same conversion used for all NTP pkt timestamp fields in client_ntp.c, or pthread_ntpserver
      > reorganised all that, putting NTP<-->UTC conversion fns in misc.h, where other tval conversion lived
			> had to reorg the .h files throughout workspace to make it work:  misc.h after proto_ntp.h,  also removed misc.h in some cases where not needed .  Interesting reflextions on how/when .h inclusions work.
			  Ultimately could depend on order of compilation. To be save, in All files where the fns in misc.h could be called, should include proto_ntp.h ahead of misc.h
			 // TODO:  work out why/if they should be static inline, if not, then shouldn't be in a .h file, but why?
  - starvation defn
   	- localize in one place if can, remove from trigger if can
   	- scan all places where this status is used, start to track this for all RADclock status bits
   	> clarified defn as being a validstamp-gap thresholding only, measured in #missingstamps,
   	  regardless of reason (networking or server down, match failure, insane stamp).
   	  So this is a pure input-to-clock measure, not a clock quality measure with SKMscale etc, which is
   	  already covered by other metrics, in particular the sync status bit
   	  Hence, the status and starvation bits are only partially correlated.
   	> fixed defn as above, improved logic to avoid superfluous re-setting of bits
   	> moved to process_stamp.  This is thread-neutral, as it is called whether Alive or Dead.
   	> threading issues:   key aspect of starvation is tracking events invisible inter-algo, as they occur when valid stamps do not.  Pros and cons of basing this in:
   	   TRIGGER:
   	      - natural to ensure checking after every grid point attempt, regardless of success.
   	      - right place is in trigger_work as before [inside actual threading management, as reqd]
   	      - but a bit silly as recheck before have even looked at the stamp attempt just completed
   	      - not natural wrt origial trigger ethos: this should be about trying to send stamps on a grid only, not about success of the clock and setting status bits.  On the other hand, as a sender, it is natural here to note how many failure you get
   	      - code wont run at all if running DEAD
			PROC:
			 	- natural as it is the seat of radclock intelligence, can integrate with other stamp tests, and status bit setting later on, raddata updating in general
			 	- but in a original trigger ethos, PROC would not even run if there was no data. Currently however it DOES, in fact it runs after TRIGGER has finished with each grid pt, even if there is not raw data at all for it to pick up, or some but not a full stamp.
			 	    If this ever changes, would again have to put the code in trigger
			Opted for PROC:
				- updated code, new comments, smarter placement of DEL_STATUS so only runs exactly when it
				  should by defn. Placement of ADD_STATUS so it is set in both no-stamp and insane-stamp scenarios.
				- can only run if Live, otherwise cant read the counter and meaningless anyway
	- ntp_client may return even though no data was found (all attempts failed),
     yet the flag wakeup_data_ready is still set.  Should be called:
     wakeup_trigger_donewithgridpt  or  wakeup_datahasbeensent
	  > changed name to wakeup_checkfordata
			

int trigger_init(struct radclock_handle *handle)




## pthread_ntpserver.c	// a.k.a. PTH_NTP_SERV
The RADclock server, ie serving external client packet by radclock as a server.

 - look at system clock comments in config_mgr.c:write_config_file, I think this mechanism  part of the dirty hack to sync the sys clock by RADclock with damping. It is not the full RADclock kernel sysclock?  Unclear since the discussion of IPC also provides kernel&user access to RADclock via the API but without any kernel takeover - I this this comment is outdated.
 
   
static inline void build_timestamp_tval(struct radclock_handle *handle,
		const struct radclock_data *rdata, vcounter_t vcount, struct timeval *tv)
  - calls counter_to_time to return Ca as a long double, then
    UTCld_to_timeval(&time, tv);      // see misc.h
  - seems to be no daemon-internal Cd function here or anywhere!  and library fns
    ffcounter_to_difftime_{shm,kernel}   not used either
  - who needs this?  just read the clock then convert

void *thread_ntp_server(void *c_handle)
	- all incoming modes ignored except MODE_CLIENT
	- BUG:  used Ca via build_timestamp_tval instead of Cd to define xmt timestamp !  annoying.
	



## fixedpoint.c	// a.k.a. PTH_FIXEDPOINT
Deprecated: old way of pushing clock updates to the kernel with no fp support,
needed to wake up often enough to avoid overflow of 32 bit counter.
Could be useful to get simpler idea of how kernel updates done, or for dirty
Linux patches?

Still used for handle->clock->kernel_version < 2  .
Started within start_live in main with call to start_thread_FIXEDPOINT.




## pthread_dataproc.c      // a.k.a. PTH_DATA_PROC

Stupid naming:  this thread effectively has two names:  DATA_PROC and rawdata.
Needs to be DATA_PROC as it does the raw consuming part, but much more.
The file itself is top level stamp processing stuff!  essentially the PROC main
	> (was pthread_rawdata.c  ) changed to pthread_dataproc.c  on June 8 2019 via git

Has kernel specific timer stuff:
#ifdef WITH_FFKERNEL_{NONE,FBSD,LINUX}

int update_ipc_shared_memory(struct radclock_handle *handle)
  - update procedure for rad_data (same story for rad_error ) :
  		- first copy new data into `old` position
		- then swap old and current pointers, so current points to new data copy
		- this implements a two-element circular buffer
		- benefits are I guess:
			- swapping pointers instead of structures is faster
		   - if read before finished, the current pointer will still point to
			  useful data (not the old one, but the current just about to be replaced.)
		   - but why not just store a single `i` integer and use shm->bufdata[i%2] ?  so much simpler!
			  set it up so i is always current and so i+1 is old, easy.  Ahh but the calling program would have to understand the i internals to be able to use it..
	 - Generation Control:
	   // Lock-free safe access code without underlying atomic support
		// Can't use mutex here as no shared address space, and besides, can't control what
		// other processes do, could interrupt out of critical section and hang,
		// so mutex never released, and global data never updated !
	     generation = shm->gen;// save current value
	 	  shm->gen = 0 			// signal inside pointer-swapping `critical section`:
								   // makes  !shm->gen  TRUE, tested in numerous fns in radapi-getset.c
									// makes fns try again if found to be in middle
	     generation++			// increase BEfore exit to signal a new update & avoid ambiguity
		     - In fact not quite so simple, actually use this to cover special case:
			  if (generation++ == 0)   // generation at max value on entry
					generation = 1;			 // skip over reserved value of generation=0
		  shm->gen = generation; // signal finished update and flag what update we are at
		// This works because after entry, guaranteed that (generation,gen) pair different to before:
	 	//   - either (generation_old,0),  or  (generation_new,0), or (generation_new,generation_new)
		// If calling fn sees pb, repeat until good - then know you have the latest info as well.
		
  - use generations instead of locking, but who are the competitors?
  		- DATA_PROC calls this,  is it the kernel also pushing in some cases ?
		- who can call the fns that use this in radapi-{time,getset}.c
		- I guess any other PROcess wanting to use the API, kernel or other


void read_clocks(struct radclock_handle *handle, struct timeval *sys_tv,
	struct timeval *rad_tv, vcounter_t *counter)
  - attempt to read system clock and vcounter simultaneously in userland with bracketing
  - makes up to 5 attempts to get good bracket
  - used in update_FBclock only  (but could be useful more generally)
  		- but not make static for VM reason, see comments
  - something to do with needing this less hidden than ideal to support VMs
  - should be called read_sysandRAD_fromuserland

int update_FBclock(struct radclock_handle *handle)
  - is this part of the dirty algo to create a RADclock based sysclock without kernel
    support? with the conservative low adjustment heuristic RADclock-->sysclock feedback?
    what was that called??  Main use case was when RADclock was great, wanted to
    ensure systemclock benefitted instead of having crazy ntpd.  BUT!  also seems to be
    the way that CAIDA was using it, which was why it defaulted to BOOL_ON, that is how
    they used it without needed to alter Scamper..

  - has dedicated config parameter conf->adjust_FBclock
  - HISTORY (May 20 2019)  was update_system_clock  with conf parameter  adjust_system_clock.
     Changed to make it explicit that this is the wierd  FB daemon functionality, and
     changed the default to off plus comments to make it clear is not the main deal,
	  and changed logic (and some organisation _+ comments) in
	  pthread_dataproc:process_stamp()  to make it an
	  independent feature only, no link to FFclock kernel updating.
	  ** No need for explicit adjust_FFclock  parameter, if you are live, then you are
	  using an FF kernel by defn, and all FFkernels (even KV=0), have global data
	  to be updated.
  - is OS/kernel aware, uses #ifdef WITH_FFKERNEL_NONE, but almost all always defined.
  - note STA_NANO:  looks like Pi will be ok with KERN_RES set to 1e6
  - statics:
  	static vcounter_t sys_init;           // records raw time origin of syncing
	static struct timeval sys_init_tv;
	static int next_stamp;                // records end of calibration period
  - BUG:  uses ntp_adjtime  once instead of macro NTP_ADJTIME
      
   - Rough algo:
  	  - avoids adjustment when in NTP_BURST phase
      - at end of burst
      		- jump sysclock to Ca
      		- initialize other sysclock variables over 60sec calibration period
              next_stamp is the projected stamp number when period complete
            - useful but woeful comments here..
      - do nothing over calibration period, just return 0
      - right after calibration
      	    - update sysclocks tx structure with crude RADclock based estimates
            - tell it is is synchronized etc
            - not sure what "make up for frantic run" is for", but is a one-off
      - normal updates
			- just pass RADclock offsets, otherwise let kernel PLL do it


int insane_bidir_stamp(struct stamp_t *stamp, struct stamp_t *laststamp)
  - this is the current form of the "network layer" sanity checking
  - called only once, in process_stamp, after get_next_stamp called
  - sets qual_warning if stamp survives but spots a problem
  - pkt classed as bad and killed if  (this applies to spy as well)
    - stamps are of different types
    - identical  [ identical IDs are possible, but 4tuples super unlikely ]
	 - at least one 0 raw stamp (impossible for NTP?)
	 - overlapping stamps!!
	 - non-increasing Ta   !   [ now should be impossible with NTP, but nice to check ]
	 - crazy non-causal raw stamps [ I remember this now. ]
	 - impossibly low HOSTRTT:   BUG:  update for Pis STC
   - nice to have this already in place to check crazy, and exclude if you want
	- Will very likely catch most cases of a mid-stamp change in timecounter, which will
	almost certainly induce either a huge causality violation, or a huge RTT:  should add huge RTT test (no hard discarding them anyway..)  Where huge = say >10s,  but 3s would be fine (largest reasonable true RTT)
	  though algo also could handle these, but still, it is a network level problem,so ..
	   - only way to do better would be to explicity test for the identity of the tc, how could the daemon do this?
	 

process_stamp(struct radclock_handle *handle, struct bidir_peer *peer)
  Core of main program functionality, called in an infinite loop by main if
  handle->run_mode == RADCLOCK_SYNC_DEAD, otherwise gets called inside
  pthread_mgr.c:thread_data_processing  .
  Has struct ffclock_estimate cest;  // if defined separately if kernel not supportive
	
  Originally called process_rawdata.  This did match capture_raw_data
  name under MAIN, the producer, however the name was stupid as it is clearly a fn whose role
  it to process a stamp through the algo, and update radclock state based on it.
  It does not get raw until the calls go all the way down to free_and_cherrypick !
  It is even run on every grid pt (when live) and so is the right place to update the clock more generally,
  so could be called   updateclockforthisgridpt  ...
	
  Calls:
	err = get_next_stamp(handle, (struct stampsource *)handle->stamp_source, &stamp);
	if insane_bidir_stamp(&stamp, &laststamp) return
	handle->algo_output->n_stamps++  // tracks # stamps send to algo
	leap second stuff 					// All leapsec processing here, both for CaUTS and CaNative
	
	process_bidir_stamp(handle, peer, &stamp_noleap, stamp.qual_warning);
	update_ipc_shared_memory(handle)	// if RADCLOCK_SYNC_LIVE & server_ipc & status !STARAD_UNSYNC
	set_kernel_ffclock(handle->clock, &cest);  // if RADCLOCK_SYNC_LIVE and WITH_FFKERNEL_FBSD
	
	update_FBclock(handle)   	// if RADCLOCK_SYNC_LIVE and adjust_FBclock
	
	print_out_files(handle, &stamp);
	verbose output periodically (if selected)
   return codes:  [ for a single call, processing a single stamp ]
	  -1: nasty pb, exit
	   0: success, got a stamp and processed it  (being caught by sanity counts as processed)
		1: no error, but no stamp to process




########################################################################
Globals   [ doesnt cover macros ]
###############

Variable								File where defined		Extern in..
------------------------------------------------------------------------------
clock_handle						radclock_main.c			rawdata.c (needed for spy) jdebug.h
verbose_data						verbose.c					everywhere via verbose.h
alarm_mutex, alarm_cwait		pthread_trigger.c			client_ntp.c

Even in radclock_main.c  clock_handle is always passed or copied to a local variable,
always (I think) called handle, rather than used directly.
Exceptions:
	- in main for essential create and NULLing operations
	- static void signal_handler(int sig): // because not allowed to take another argument
	- void fill_rawdata_spy(int sig):      // another callback only allowed one argument
	- once/twice in rehash_deamon, but this is a BUG
	
Apart from anything else, this is a discipline making it easy to track all actual
references to the global. You dont want to have lots of local variables called
the same thing, and you want to limit direct access to the minimum on principle.
That way (almost) everything works via parameters (which are themselves copies).
 
Sometimes a handle argument is passed as void*, in which case it is called c_handle,
and inside the fn a local is defined to get the right type, and call it handle for consistency.  Eg:
void  fill_rawdata_ntp(u_char *c_handle, const struct pcap_pkthdr *pcap_hdr,..
{  ...	handle = (struct radclock_handle *) c_handle;  }



########################################################################


## radclock_main.c   [ contains main program ]

/* Globals */
struct radclock_handle *clock_handle;        /* global pointer, main has local copy "handle"
															/* pts to structure created as static by create_handle
extern struct verbose_data_t verbose_data;   /* defined in verbose.c
															/* declared as extern here and in verbose.h only */
															/* but extern in verbose.h anyway, no need for this */
#ifdef WITH_JDEBUG
    long int jdbg_memuse = 0;
    struct rusage jdbg_rusage;
#endif


- almost all fns static here:  clearly tracks global state of radclock_main

static int rehash_daemon(struct radclock_handle *handle, uint32_t param_mask)
  - ahh! this does not rehash some other daemon, it IS the daemon to 'rehash' parameter config
  - is there No connection to RADcock deamon?  if so, change name
  
  Allows changed parameters in config file to be reread and acted on on the fly
   - In fact only ever called with param_mask=UPDMASK_NOUPD
	  - is passed by value so it remains =UPDMASK_NOUPD on next call
	  - this is just initializion of a local mask variable.
	  
  Begin with config_parse
   - param_mask modified throughout by CLEAR_UPDATE calls (but no SET_UPDATE)
	- on exit saved to global:  conf->mask = param_mask;   used to allow sync_algo
	  to handle remaining actions following conf file change
	  
	- param_mask cleared of some values (why not, already dealt with), not others, why?
	
  Params that can be acted on:
   server_ipc:  	on or off    not a thread? threated like one here syntactically
	server_ntp:  	on or off    uses handle
	server_vm_udp: on or off    uses handle (passed copy of clock_handle ptr) and
									    BUG: global clock_handle directly
	UPDMASK_SYNC_OUT_ASCII:		close then reopen (reopen will trigger file saving)
	UPDMASK_CLOCK_OUT_ASCII:	close then reopen (reopen will trigger file saving)
	UPDMASK_SYNC_OUT_PCAP:		close using update_dumpout_source method, not sure if reopens..
	UPDMASK_{SYNCHRO_TYPE,SERVER_NTP,TIME_SERVER,HOSTNAME} call update_filter_source method
				why is SYNCHRO_TYPE included?  already dealt with higher
	
	BUG:  threads for server_ntp and server_vm_udp restarted here, but this will be
			done straight after in start_live anyway, since conf variables updated.
			Logically, rehash should just do reparsing and consistency, leave restarting to start_live
		In fact for these, either it was on before but now you want it to stop (so kill but no REstart),
		or it was off before and so want to start it (ok could be a REstart).  Check logic here.
		
		If we want an outside program to do this (like the NTC Controller), then want to edit the conf
		file, then send the signal to the instance.  Either that, or just a special signal we havent used yet. Still, if we have a mechanism already..  may also want to change poll or sth else when transitioning in/out of public-serving mode.
		
			
  conf->mask = param_mask;

  Other params that can be reread but dealt with elsewhere or not needing action:
  		 left to algo:		TEMPQUALITY, POLLPERIOD   (result is call to update_peer)
	 	 no specific action:		VERBOSE, adjust_FBclock
		    (update_data inside config_parse updates these )
		 no action at all:		DELTA_HOST, DELTA_NET:  in fact these appear nowhere
		    BUG: names should be DEFAULT_ASYM_{HOST,NET} ? these are acted on in update_data()

		  
  config_print(LOG_NOTICE, conf);
	

  Looks like verbosity level, pollperiod can be acted on!
  Lots of IPC issues to deal with here
  
  All threads except data proc will be killed and restarted, so rely on clock_init_live to avoid
stupid start from scratch (this is before algo gets a chance) but what about history?
  Important header comments arent clear


static void logger_verbose_bridge(int level, char *msg)
  see verbose/logger section above

static void signal_handler(int sig)
    initial triage of signals into those source_breakloop handles and other user defined ones.

static int daemonize(const char* lockfile, int *daemon_pid_fd)
	 This is called by main before other threads launched.
	 Good comments here, but to understand, need to read up on deamons, most
	 stuff here is standard daemon only but I dont get it.
	 
	 In main:
	 	#define DAEMON_LOCK_FILE ( RADCLOCK_RUN_DIRECTORY "/radclock.pid" )
		const char *pid_lockfile = DAEMON_LOCK_FILE;
	 - actually forks (copies process) then kills original immediately ! radical, but standard..
	 - opens and sets log files, enables `levels`:
	    	openlog ("radclock ", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);
      seems to be standard daemon thing, not RADclock
		
    - care taken to avoid multiple instances running as a daemon_pid_fd
	   - should check again to see if this is about only have one special daemon being able to set global data, and potentially influence the FFsystemclock
	 - why would you deamonize before even checking (in main) if running live ?


static struct radclock_handle * create_handle(struct radclock_config *conf, int is_daemon)
    - Good record of all aspects to consider when creating
    - sets default values for global radclock_data (defined in radlock_private.h)
    - record of initial burst of NTP pts behaviour (uses existing? ntp parameter NTP_BURST)
    - creates pcap and 1588 raw data queues // HISTORY, all rdb buffer stuff moved in here
    - creates and initialised to zero all members of algo_output

static int clock_init_live(struct radclock *clock, struct radclock_data *rad_data)
    - tries to improve default radclock_data by reading from kernel version>=2
	 	- this is before algo even has a chance to look at it - need to check how/when it
		  would overwrite this initialization
    - multiple cvounter initializations:  should look into why

static int init_handle(struct radclock_handle *handle)
    - more non-trivials initialization that can only be done once stuff ready..
	 	- if live, creates SHM with shm_init_writer(handle->clock);
		- creates data source via create_source
		- opens .dat and .out output files
    - stupid name!  makes it sound like it will init entire handle, but only does
      some things, and already relies on handle->run_mode being set.

int start_live(struct radclock_handle *handle)
	 - stupid name, is not just an init routine, it has the infinite data capture loop,
	   and controls threads dying !!  Only join calls are here.
		Call it start_and_join_threads ?   start_capture_join ?   livecapture_starttojoin
	 - is one of the best places commenting on overall thread execution logic
	 
	 - exits immediately if no server specified: NTP client would spin..
    - starts up threads
        start_thread_trigger		//   NTP pkt sending client
		  start_thread_DATA_PROC	//	  main data processing, if not a VM slave
		  		 - is called on every rehash, so some things may already started
        start_thread_NTP_SERV		// if running a RADclock NTP server
        start_thread_VM_UDP_SERV	// if running VM server for guests
		  start_thread_FIXEDPOINT  // if kernel version < 2
		  
    - calls err = capture_raw_data(handle);  main data loop ! see rawdata.{h,c}
	   On exit:
			err =  0:   only happens for spy mode I think
	      err = -1:		end input or other error, set handle->unix_signal = SIGTERM to 'signal' internally
			err = -2:		rehash case, verbose this, but no specific action needed except save Data Proc
	 
	   Either way:
		 - set STOP flag for all threads, except for Data Proc if HUP !    (bad implementation)
		 - joins threads {NTP_serv,trigger,fixedpt}, and notes when they return (and hence are dead)
		   joins Data Proc if not HUP
	 	 - BUG:?  no join for VM
		 - BUG:?  why does it make sense to restart NTP_serv?
		 - in rehash case, rehash will restart them  (debate in rehash_daemon as to where have joins)
		 
	 - exit codes: 1: will die case:  either error from start_thread_{,,,} or TERM
						0:	HUP  case, will be rehashed
						   or got 0 from capture_raw_data (eg spy mode), rehash?


// Recall return codes for capture_raw_data:
	 	  -1:  if various errors occur (eg RADCLOCK_SYNC_NOTSET,  or if pcap_loop returns this)
		  -2:	 if pcap_loop returns this (due to call to pcap_breakloop)
		   0:  if in 'other' cases and no error, currently basically spy




MAIN
  Some initializions, but primarily reading command line options,
  Register signal handlers
  Setup verbosity
  daemonize
  	- BUG  chmod to 00755 not 0755  ?
  THEN the config system.
  
  
  // Config system:
	conf = (struct radclock_config *) malloc(sizeof(struct radclock_config));
	config_init(conf);
	param_mask = UPDMASK_NOUPD;  // initialize to 0, meaning none updated
	for each command line argument:
	  record that have updated into param_mask using SET_UPDATE(param_mask, UPDMASK_..)
  	  update actual conf->param member
  
	sort out deamon status  (is_deamon so far set via online params)
	
	// Set params according to config file, but ignore those recorded in param_mask
   config_parse(handle->conf, &param_mask, handle->is_daemon))
   correct any incompatible configuration params
	config_print(LOG_NOTICE, handle->conf);       // record actual config used
   param_mask = UPDMASK_NOUPD; 		/* Reinit the mask that counts updated values */
	//===========================
	
  
  - clock_handle = create_handle(conf, is_daemon); // global handle variable
  - handle = clock_handle;                         // local copy used instead, but why??
    // I think to make all instances of direct use easily findable, and others to obey
	 // a common convention.

	if (!is_live_source(handle))   // finds out by looking at conf variable
		handle->run_mode = RADCLOCK_SYNC_DEAD;
	else
		handle->run_mode = RADCLOCK_SYNC_LIVE;

  	err = init_handle(handle);   // stupid name

  - reveals single read and process thread in case of replay
  - if replay,
        calls process_stamp in infinite loop, exits if error
    else
        // enter main data capture loop
		  err = start_live(..)   // starts threads, infinite capture loop, stops them on signal or error
		  if err==0 // stop was due to SIGHUP,
		  		rehash to process param file updates, then go back to start_live
		  
  - after exit, output verbose stuff and cleanup





########################################################################
## config_mgr.h        // configuration stuff !

Is included in almost all radclock/.c files

#define DAEMON_CONFIG_FILE		"/etc/radclock.conf"
#define BIN_CONFIG_FILE			"radclock.conf"

/* Trigger / Sync Protocol configuration */
#define SYNCTYPE_{SPY,PIGGY,NTP,1588,PPS,VM_UDP,XEN,VMWARE} = {0,1,2,3,4,5,6,7}
  - dont appear in config_mgr.c !
  - what are these really?  mainly about selecting source type
  
/* Server Protocol configuration */
#define BOOL_{OFF,ON} = {0,1}
  - dont appear in config_mgr.c !

#define VM_SLAVE(val)  ((val->conf->synchro_type == SYNCTYPE_VM_UDP) ||  ...
#define VM_MASTER(val) ((val->conf->server_vm_udp == BOOL_ON) || \   ...


/* Default configuration values */
#define DEFAULT_VERBOSE				1
#define DEFAULT_SYNCHRO_TYPE		SYNCTYPE_NTP	// Protocol used
.....
#define DEFAULT_NTP_POLL_PERIOD 	16				// 16 NTP pkts every [sec]
#define DEFAULT_TIME_SERVER		"ntp.cubinlab.ee.unimelb.edu.au"
.....
#define DEFAULT_SYNC_IN_PCAP		"sync_input.pcap"
...
#define DEFAULT_VM_UDP_LIST			"vm_udp_list"

  - 21 default labels, given values from SYNCTYPE_ or BOOL_ above, or numbers, or strings
  - these are used in config_mgr.c to set defaults
  	- does this give a full set of things that are given defaults?
  - gives access to defaults whenever needed throughout code
  - modelled off NTP default of DEFAULT_NTP_PORT defined in proto_ntp.h (from ntp.h)

/* Definition of keys for configuration file keywords */
#define CONFIG_UNKNOWN			0
#define CONFIG_RADCLOCK_VERSION 1
...
#define CONFIG_VM_UDP_LIST		63

  - These are just the arbitrary codes for config param types using in config_mgr.c
  - here the order is logical by category, why is the order different when defining keys[]  in config_mgr.c ?  (each have 30 values, encoded over [0:63] )

/* Pre-defined description of temperature environment quality */
#define CONFIG_QUALITY_{POOR,GOOD,EXCEL,UNKWN} = {0,1,2,3}
  - stupid names, no mention of temperature
  - not used inside keys[], instead used inside temp_quality[] which is another _key array.
  - an overall quality rating summarising the physparams (see get_temperature_config)
 


/* 27 "UDP..  " masks to control reload of configuration parameters */
// UPD = Updated Parameter Data?
#define UPDMASK_NOUPD			0x0000000
#define UPDMASK_POLLPERIOD		0x0000001
....
#define UPDMASK_PID_FILE		0x0800000
#define UPD_NTP_UPSTREAM_PORT	0x1000000
#define UPD_NTP_DOWNSTREAM_PORT	0x2000000

// Confusing dummy variable names here, since used like:  HAS_UPDATE(*mask, UPDMASK_VERBOSE) )
// with mask typically = param_mask initialized to UPDMASK_NOUPD
#define HAS_UPDATE(val,mask)	((val & mask) == mask)
  - true iff val has a 1 in mask position
  - hence if val=UPDMASK_NOUPD=0x0, always returns 0=false
#define SET_UPDATE(val,mask)	(val |= mask) 
  - updates 'val' mask with the new specific mask 'mask.
  - used to record a new mask into the cumulative val variable (called as param_maks)
#define CLEAR_UPDATE(val,mask)	(val &= ~mask)
  - clear specific 'mask' out of the cumulative val variable



// Global structure used to keep track of config params, used mostly by signal handlers
// This is the key `conf' parameter passed to config_{init,parse,print}
// Every element here (including all 6 phyparam members) printed by config_print, except mask
struct radclock_config {
		u_int32_t mask;						/* Update param mask */
	/* Runnning defaults */
		char 	conffile[MAXLINE]; 			/* Configuration file path */
		char 	logfile[MAXLINE]; 			/* Log file path */
	char 	radclock_version[MAXLINE]; /* Package version id */
	int 	verbose_level; 				/* debug output level */
	int 	poll_period; 				/* period of NTP pkt sending [sec] */
	struct 	radclock_phyparam phyparam; /* Physical and temperature characteristics */ 
	int 	synchro_type; 				/* multi-choice depending on client-side protocol */
	int 	server_ipc; 				/* Boolean */
	int 	server_ntp;					/* Boolean */
	int 	server_vm_udp;					/* Boolean */
	int 	server_xen;						/* Boolean */
	int 	server_vmware;					/* Boolean */
	int 	adjust_FBclock;		/* Boolean */
	double 	phat_init;					/* Initial value for phat */
	double 	asym_host;					/* Host asymmetry estimate [sec] */
	double	asym_net;					/* Network asymmetry estimate [sec] */
	//
		int     ntp_upstream_port;                      /* NTP Upstream port */
		int     ntp_downstream_port;                    /* NTP Downstream port */
	char 	hostname[MAXLINE]; 			/* Client hostname */
	char 	time_server[MAXLINE]; 		/* Server name */
	//
	char 	network_device[MAXLINE];	/* physical device string, eg xl0, eth0 */ 
	char 	sync_in_pcap[MAXLINE];	 		/* read from stored instead of live input */
	char 	sync_in_ascii[MAXLINE]; 		/* input is a preprocessed stamp file */
	char 	sync_out_pcap[MAXLINE]; 		/* raw packet Output file name */
	char 	sync_out_ascii[MAXLINE]; 		/* output processed stamp file */
	char 	clock_out_ascii[MAXLINE];  	/* output matlab requirements */
	char 	vm_udp_list[MAXLINE];  		/* File containing list of udp vm's */
};

  - 28 Members, or 33 if phyparam is expanded
  - grouped according to variable type? , but are ordered according to category
  in init config_mgr.c:config_init definition, and differently again in keys[],
  stupid since .h is the more public file, need the most structure here.
  TODO:  review config_init order and alter keys[] and radclock_config to match
  
  - Members almost the same as those of keys[] :
  		- missing from keys (indented above): 	mask;  {conf,log}file;  ntp_{up,down}stream_port
			- seems strange to include CONFIG_TIME_SERVER but not ports
			- mask only ever set to
			      UPDMASK_NOUPD  in  config_init
			      UPDMASK_NOUPD  in  pthread_dataproc.c:process_stamp
					param_mask     in  rehash_daemon
		     and read in
				   process_bidir_stamp
		  So after read in process_bidir_stamp, no need to clear or reset, next call to rehash_daemon will set it to new value as needed, then read by process_bidir again.
			  
		- missing from radconf_config: 			CONFIG_TEMPQUALITY;  CONFIG_UNKNOWN
			- temperature quality already factored into choice of physparams, (and may not be defined though physparams always is) but would be nice to record..
			- CONFIG_UNKNOWN is just a way to match an empty string to a trivial code (0)
			
  - question:  what about other params in config_mgr.{c,h} not included here?
  
// Declares these defined below
void config_init(struct radclock_config *conf);
int config_parse(struct radclock_config *conf, u_int32_t *mask, int is_daemon);
void config_print(int level, struct radclock_config *conf);




################################################
## config_mgr.c        // configuration stuff !

 BUG?  functions in this file return 0 on failure and 1 on success !

// config file uses many parameters structures as `key = value'.
// Nothing to do with stamp matching key, is about associating useful meaningful string to
// `codes' pointing to a parameter type
struct _key {
	const char *label;
	int keytype;
};


static struct _key keys[] = {
	{ "radclock_version",		CONFIG_RADCLOCK_VERSION},
	{ ...

The keytype/keywords here are not values (eg the actual radclock version), they
are just arbitrary distinct codes (defined in config_mgr.h) representing the configuration parameter `type'.
Presumably so that a single configuration integer parameter can be passed around, and depending on its value (which can be tested for), the type of parameter in question, and its string description, can be known).
  - slightly different to list of config params in struct radclock_config *conf), and why is the order so different?  confusing.


	
// options labels
// just a quick way to convert coded values to strings, used only in config_mgr.c
static char* labels_bool[] = { "off", "on" };
  - nowhere is it said that these map to {0,1}
  - implicit link given only in printed lines from write_config_file
static char* labels_verb[] = { "quiet", "normal", "high" };
  - nowhere is it said that these map to {1,2,3}, no clear connection to verbose.{c,h}
  - implicit link given only in printed lines from write_config_file
static char* labels_sync[] = { "spy", "piggy", "ntp", "ieee1588", "pps","vm_udp", "xen", "vmware" };

static struct _key temp_quality[] = { "poor", "good", "excellent", ""} = CONFIG_QUALITY_{POOR,..}
  - poor names, no mention of temp in them, reason to limit number of characters?


int get_temperature_config(struct radclock_config *conf)
 - only used twice inside config_mgr.c:write_config_file
 - this fn defines the meaning of the CONFIG_QUALITY_{POOR,GOOD,EXCEL,UNKWN} param types
 - basically, this is a single quantifier, more convenient than the 6 inside physparams
 - currently rather trivial, just reflecting the `all poor' or `all good' etc structure.
 - is preparing for more generality but seems like overkill.  Instead, could make these params the origin of the the temp concept, then set physparams accordingly. But, this fn only visible here, is used as a convenience almost, whereas physparams is more global


void write_config_file(FILE *fd, struct _key *keys, struct radclock_config *conf)
  - used twice only, both in config_mgr:config_parse
  - writes a (default?) config file
  		- a default file if input conf=NULL
		- else uses and complements values from conf in the cases:
			- where conf gives trivial values like ""
			- where fields missing from conf:  CONFIG_TEMPQUALITY
			- where there is only one answer anyway: CONFIG_RADCLOCK_VERSION
			- * does not assume conf is the output from config_init, could be fuller
		   - but printouts disabled lines (prefixed by #) in a number of cases:
		      _HOSTNAME, _TIME_SERVER, _VM_UDP_LIST
				expert temp except for CONFIG_QUALITY_UNKWN
				CONFIG_NETWORKDEV, _SYNC_*,  _CLOCK_OUT_ASCII
			
  - format is strict, each line starts either with a #, or an [a-z] or "" from a key[] label
			
  - writes out only to file, doesnt set anything! but the printed file will later be read in.

  - structure based on going through 30 members of keys[] systematically, sauf CONFIG_UNKNOWN
  		- the order (given on left below) is different to orders in keys[], config_init, etc..
  		- all 21 DEFAULT_ labels mentioned
  - Relationship to active variables and config_init achieved manually through the printf statements, no automatic mapping!
  - comments here a valuable resource, only place where various things explained
  
  - Temperature_quality:
     - sets to GOOD defaults if no conf, otherwise, overwrites _EXCEL case to be _GOOD, and populates the 'expert' case, ie _UNKWN
  	  - Unkwn is used to indicate an 'expert config' ie not fitting into the simple cases, and is output in write_config_file as unkwn = good  , implying trust of expert
	  - experimental code:  seems to print out the actual physparams values instead of just the CONFIG_TEMPQUALITY overall label, but what sets physparams in the first place??
	   Temp code seems stupidly complex for such a simple thing..
	  
	    
  
  
static struct _key keys[] = {
1	{ "radclock_version",		CONFIG_RADCLOCK_VERSION},
2	{ "verbose_level",			CONFIG_VERBOSE},
3	{ "synchronisation_type",	CONFIG_SYNCHRO_TYPE},
7	{ "ipc_server",				CONFIG_SERVER_IPC},
12	{ "ntp_server",				CONFIG_SERVER_NTP},
8	{ "vm_udp_server",			CONFIG_SERVER_VM_UDP},
10	{ "xen_server",				CONFIG_SERVER_XEN},
11	{ "vmware_server",			CONFIG_SERVER_VMWARE},
13	{ "adjust_FBclock",	CONFIG_ADJUST_FBCLOCK},
4	{ "polling_period", 		CONFIG_POLLPERIOD},
14	{ "temperature_quality", 	CONFIG_TEMPQUALITY},
 15	{ "ts_limit",				CONFIG_TSLIMIT},
 16	{ "skm_scale",				CONFIG_SKM_SCALE},
 17	{ "rate_error_bound",		CONFIG_RATE_ERR_BOUND},
 18	{ "best_skm_rate",			CONFIG_BEST_SKM_RATE},
 19	{ "offset_ratio",			CONFIG_OFFSET_RATIO},
 20	{ "plocal_quality",			CONFIG_PLOCAL_QUALITY},
21	{ "init_period_estimate",	CONFIG_PHAT_INIT},
22	{ "host_asymmetry",			CONFIG_ASYM_HOST},
23	{ "network_asymmetry",		CONFIG_ASYM_NET},
5	{ "hostname",				CONFIG_HOSTNAME},
6	{ "time_server",			CONFIG_TIME_SERVER},
24	{ "network_device",			CONFIG_NETWORKDEV},
25	{ "sync_input_pcap",		CONFIG_SYNC_IN_PCAP},
26	{ "sync_input_ascii",		CONFIG_SYNC_IN_ASCII},
27	{ "sync_output_pcap",		CONFIG_SYNC_OUT_PCAP},
28	{ "sync_output_ascii",		CONFIG_SYNC_OUT_ASCII},
29	{ "clock_output_ascii",		CONFIG_CLOCK_OUT_ASCII},
9	{ "vm_udp_list",			CONFIG_VM_UDP_LIST},
30	{ "",			 			CONFIG_UNKNOWN} // Must be the last.
};

  - CONFIG_UNKNOWN Used in find_key_label() and match_key only, just to catch when you've fallen through to the end because nothing has matched

// Key manipulation functions (should be put together in .c, they arent
// inconsistent naming, have `find' and 'match' when in both cases are `getting'
const char* find_key_label(struct _key *keys, int codekey)
  - workhorse used throughout write_config_file, and nowhere else
  - given the key code/type, returns the matching label
  - uses for (;;), relies on being passed start of *keys[] array to scan all entries

int match_key(struct _key *keys, char* keylabel)
  - used once in config_parse only !
  - returns keytype corresponding from input key string

int extract_key_value(char* c, char* key, char* value)
  - used once in config_parse only !
  - extract key label (not code!) and corresponding value from line of parsed configuration file.
  - strncpy(key, c, strchr(c,'=')-c);  // set key = part of line to left of "="
  - says copy key but is really copying label,
  - BUG:  key is not the label but the label with " " appended, based on code
  - code for getting the value seems to kill space at end several times.. anyway this is all
    overkill since write_config_file creates clean lines anyway

int check_valid_option(char* value, char* labels[], int label_sz)
  - workhorse used throughout update_data, and nowhere else
  - checks if input label value is present in label array, returns its posn in array,
    which is in fact its coded value.
	 Bad name, since 'value' is not a parameter value, it is the associated label string.
  - position takes value in 0,1,.. label_sz,  if value absent, returns -1
  - dummy variables have generic names as used for label_verb etc as well as keys[]
  

// Global data, but also really ugly to make it a parameter.
int have_all_tmpqual = 0; /* To know if we run expert mode */  tmp=temp?
  - used in update_data for all physparams members and CONFIG_TEMPQUALITY, and once in config_parse
  - have_all_tmpqual = 1 means using overall CONFIG_TEMPQUALITY to set in the traditional
  way.  = 0 means set them individually, that requires an `expert to determine
 

/* Update global data with values retrieved from the configuration file */
int update_data (struct radclock_config *conf, u_int32_t *mask, int codekey, char *value) { 
  - used in config_parse to update parameter data extracted from each line in config file
  - on input mask conveys which parameters to ignore, ie not to take an update from file
    - usually set to UPDMASK_NOUPD=0x0  so that all params will be updated
	 - exception is after command line parsing, to avoid overwritting command line inputs
	 - on output, record if the input parameter was actually changed
 
  - for each possible input key:
      use HAS_UPDATE to avoid any updates recorded in mask [ if possible command line param ]
	   retrieve parameter value from value parameter
		  - uses ival=check_valid_option() in labels_verb, bool, _SYNCHRO_TYPE  cases
	   use SET_UPDATE to record if parameter actually changed
		  - no need to check for -1 here, if disagreement for any reason, will update
		perform sanity check and provide DEFAULT_ fallback and verbosity for failure
		  - not possible for variables like CONFIG_TIME_SERVER where you cant know it
	   actually update conf->parameter

   - phyparam individual entries are different
	  - each one guarded by the 'not overall CONFIG_TEMPQUALITY' boolen: have_all_tmpqual == 0
	    "Be sure we don't override an overall temperature setting"
		 so cant set expert is overall already set
	  - no HAS_UPDATE since these not available for individual entries
	  - SET_UPDATE instead for the overall UPDMASK_TEMPQUALITY, but not yet clear what changed it to !

	  - CONFIG_TEMPQUALITY a separate beast
	    - sets have_all_tmpqual = 1;  to indicate traditional overall setting in effect
		 
	   

// These 3 are in .h, as they are called outside, should change order here to reflect?
void config_init(struct radclock_config *conf)
  - called near beginning of main only
  - provides initialization for all 28 conf members except verbose_level
  - DEFAULT_ values
    - 11 of 21 used.
	 - 10 not used are DEFAULT_VERBOSE, all 7 input/output, and 2 network (all set to "")
  - Other defaults:
	 - separate DEFAULT_NTP_PORT for {up,down}stream_port
	 - {conffile, logfile} set to ""
	 - radclock_version = PACKAGE_VERSION  /* PACKAGE_VERSION defined with autoconf tools */
    - phyparam values set to GOOD settings  (defined in sync_algo.h)
  
 	
/* Reads config file line by line, retrieve (key,value) and update global data */
int config_parse(struct radclock_config *conf, u_int32_t *mask, int is_daemon)
  - called in radclock_main.c:{rehash_deamon,main} only
  - not a great name, doesnt just parse but creates, parses and updates, then
    checks consistency.  Maybe  config_build
  - on exit, mask preserves the input 'cant update' and all the 'did update' params

  
Simplified code:
   populate conf->{conffile,logfile} if not already present
	BUG:  does conffile twice instead of logfile
	 
	if conffile missing,
	 	call  write_config_file(fd, keys, NULL);  to create it
  
	if conffile already there (eg just created it), then:
	 have_all_tmpqual = 0; //ugly   // allows expert temp values to be accepted?
    for each line in conffile
	    extract the (key label, actual parameter value) pair printed in file
		 get corresponding codekey  (ie keytype)
		 update_data(conf, mask, codekey, value);
	 hence conffile format used as an API to alter conf variable, and as the place
	 where the mapping between param values and key codes is accessible
  
	In case an old conffile was read for an old package, call
      write_config_file(fd, keys, conf);
	  to ensure radclock_version (and anything else?) up to date, then if is_daemon
	  set conf->sync_in_{ascii,pcap} = ""  for consistency
	 
    Other possible consistency conditions violated checked in main after config_parse call.
	 
  
  
void config_print(int level, struct radclock_config *conf)
  - called in radclock_main.c:{rehash_deamon,main} only
  - straightforward hardwired printing of every element of conf (including all 6 phyparam members), except mask, but not in order of config_init of course
  








########################################################################
## proto_ntp.h   [ no matching .c ]

# Trimed down adaptation of ntp.h and ntp_fp.h to suit the need of RADclock all in one file.

Some of the most useful parts to remember are:

/* NTP protocol parameters.  See section 3.2.6 of the specification */
#define	NTP_VERSION	((u_char)4) /* current version number */
#define	DEFAULT_NTP_PORT	123	/* included for non-unix machines */

/* Poll interval parameters */
#define	NTP_MINPOLL	4	/* log2 min poll interval (16 s) */
#define NTP_BURST	8	/* packets in burst */


/* Values for peer mode and packet mode. Only the modes through
 * MODE_BROADCAST and MODE_BCLIENT appear in the transition
 * function. MODE_CONTROL and MODE_PRIVATE can appear in packets,
 * but those never survive to the transition function. */
#define	MODE_UNSPEC	0	/* unspecified (old version) */
#define	MODE_ACTIVE	1	/* symmetric active mode */
#define	MODE_PASSIVE	2	/* symmetric passive mode */
#define	MODE_CLIENT	3	/* client mode */
#define	MODE_SERVER	4	/* server mode */
#define	MODE_BROADCAST	5	/* broadcast mode */
/*
 * These can appear in packets
 */
#define	MODE_CONTROL	6	/* control mode */
#define	MODE_PRIVATE	7	/* private mode */
/*
 * This is a madeup mode for broadcast client.
 */
#define	MODE_BCLIENT	6	/* broadcast client mode */


/*
 * NTP packet format.  The mac field is optional.  It isn't really
 * an l_fp either, but for now declaring it that way is convenient.
 * See Appendix A in the specification.
 *
 * Note that all u_fp and l_fp values arrive in network byte order
 * and must be converted (except the mac, which isn't, really).
 */
struct ntp_pkt {
	uint8_t		li_vn_mode;	/* leap indicator, version and mode */
	uint8_t		stratum;	/* peer stratum */
	uint8_t		ppoll;		/* peer poll interval */
	int8_t		precision;	/* peer clock precision */
	uint32_t	rootdelay;	/* distance to primary clock */
	uint32_t 	rootdispersion;	/* clock dispersion */
	uint32_t	refid;		/* reference clock ID */
	l_fp		reftime;	/* time peer clock was last updated */
	l_fp		org;		/* originate time stamp */
	l_fp		rec;		/* receive time stamp */
	l_fp		xmt;		/* transmit time stamp */

#define	LEN_PKT_NOMAC	12 * sizeof(u_int32_t) /* min header length */
#define	LEN_PKT_MAC	LEN_PKT_NOMAC +  sizeof(u_int32_t)
#define MIN_MAC_LEN	3 * sizeof(u_int32_t)	/* DES */
#define MAX_MAC_LEN	5 * sizeof(u_int32_t)	/* MD5 */

	/*
	 * The length of the packet less MAC must be a multiple of 64
	 * with an RSA modulus and Diffie-Hellman prime of 64 octets
	 * and maximum host name of 128 octets, the maximum autokey
	 * command is 152 octets and maximum autokey response is 460
	 * octets. A packet can contain no more than one command and one
	 * response, so the maximum total extension field length is 672
	 * octets. But, to handle humungus certificates, the bank must
	 * be broke.
	 */
#ifdef OPENSSL
	uint32_t	exten[NTP_MAXEXTEN / 4]; /* max extension field */
#else /* OPENSSL */
	uint32_t	exten[1];	/* misused */
#endif /* OPENSSL */
	uint8_t	mac[MAX_MAC_LEN]; /* mac */
};



/* Dealing with stratum.  0 gets mapped to 16 incoming, and back to 0 * on output */
#define	STRATUM_REFCLOCK 	0 /* default stratum */
#define	STRATUM_REFPRIM 	1 /* stratum 1 */
#define	STRATUM_UNSPEC		16 /* unspecified */





########################################################################
## sync_history.h

Provides types to replace my circular buffer code with a more generic version,
allowing items of arbitrary size to obey the circular logic, with some of the
housekeeping variables kept internally instead of through dedicated variables.
But still perhaps need external parameters to track current state, not sure - lets
see if this ends up being concise or not. In practice, only needed it for simple
types anyway, but now perhaps need to manipulate many more complex types.


typedef unsigned long int index_t;

typedef struct sync_hist
{
	void *buffer;				/* data buffer */
	void *buffer_end;			/* end of data buffer */
	unsigned int buffer_sz;		/* buffer size (max number of elements) */
	unsigned int item_count; 	/* current number of items in the buffer */
	size_t item_sz;				/* size of each item in the buffer */
	void *head;					/* pointer to head */
	void *tail;					/* pointer to tail */
	index_t curr_i;				/* Record of the global index of last item added */ 
} history;

-- (buffer,buffer_end) mark the extremes of the storage (dont change)
-- (head,tail) mark the extremes of where the data is
	- head is where new data is inserted, is always available, tail is oldest
	- previously 'i' was given externally, and was head for All circular variables.

Then all fns defined here are in 1-1 correspondence with those in sync_history.c

One nice thing is that things we keep histories for now explicit in sync_algo.h :
/* Histories */
	history stamp_hist;
	history RTT_hist;
	history RTThat_hist;
	history thnaive_hist;



## sync_history.c

int history_init(history *hist, unsigned int buffer_sz, size_t item_sz);
  - malloc memory to get reqd number of items
  - head and tail initialised to buffer

void history_free(history *hist);
  - obv
  - BUG: comment that this is never used..

void history_add(history *hist, index_t i, const void *item);
  - item corresponding to stamp i is inserted at head, then move head forward,
  circulates back to buffer (ie start of memory block) if at end
  - force tail forward first if history full, making room for head insertion at same point
  
index_t history_end(history *hist);
  - returns number of items in history
  - stupid name!  maybe historical wrt my old code
  - BUG:  comments claims will return wrong answer if history empty, must check
  
void *history_find(history *hist, index_t index);
  - gets item corresponding to stamp index, item version of history(i%hissize)
  - returns void I guess to keep it generic wrt item type
  - stupid name, should be history_retrieve()
  
int history_resize(history *hist, unsigned int buffer_sz, unsigned long int index);
  - remake of the old resize capability, applied to a single history only
  - keep semantic of indexing using global stamp index i
  - didnt check all details, seems unnecessarily complex
  
# These functions are my original ones partially mapped to new data structures
# Generality pointless here, efficiency lost.
index_t history_min(history *hist, index_t j, index_t i);
  -
index_t history_min_slide(history *hist, index_t index_curr,  index_t j, index_t i);
  -
vcounter_t history_min_slide_value(history *hist, index_t min_curr,  index_t j, index_t i);
  -










########################################################################
## sync_bidir.c

static void set_algo_windows(struct radclock_phyparam *phyparam, struct bidir_peer *p ) 
static void adjust_warmup_win(index_t i, struct bidir_peer *p, unsigned int plocal_winratio) 

static void init_plocal(struct bidir_peer *peer, unsigned int plocal_winratio, index_t i)
static void init_errthresholds( struct radclock_phyparam *phyparam, struct bidir_peer *peer) 
static void print_algo_parameters(struct radclock_phyparam *phyparam, struct bidir_peer *peer)

void init_peer( struct radclock_handle *handle, struct radclock_phyparam *phyparam,
				struct bidir_peer *peer, struct bidir_stamp *stamp, 
				unsigned int plocal_winratio, int poll_period)

void update_peer(struct bidir_peer *peer, struct radclock_phyparam *phyparam, int poll_period, 
					unsigned int plocal_winratio )

void end_warmup_RTT( struct bidir_peer *peer, struct bidir_stamp *stamp)
void end_warmup_phat(struct bidir_peer *peer, struct bidir_stamp *stamp)
void end_warmup_plocal(struct bidir_peer *peer, struct bidir_stamp *stamp, unsigned int plocal_winratio)
void end_warmup_thetahat(struct bidir_peer *peer, struct bidir_stamp *stamp)
void parameters_calibration( struct bidir_peer *peer)
void collect_stats_peer(struct bidir_peer *peer, struct bidir_stamp *stamp)
void print_stats_peer(struct bidir_peer *peer)

void process_RTT_warmup(struct bidir_peer *peer, vcounter_t RTT)
void process_RTT_full (struct bidir_peer *peer, vcounter_t RTT)
double compute_phat (struct bidir_peer* peer,struct bidir_stamp* far, struct bidir_stamp* near)
int process_phat_warmup (struct bidir_peer* peer, vcounter_t RTT,unsigned int warmup_winratio)

void record_packet_j (struct bidir_peer* peer, vcounter_t RTT, struct bidir_stamp* stamp)
int process_phat_full (struct bidir_peer* peer, struct radclock_handle* handle,
	struct radclock_phyparam *phyparam, vcounter_t RTT,
	struct bidir_stamp* stamp, int qual_warning)

void process_plocal_warmup(struct bidir_peer* peer)
int process_plocal_full(struct bidir_peer* peer, struct radclock_handle* handle,
		unsigned int plocal_winratio, struct bidir_stamp* stamp,
		int phat_sanity_raised, int qual_warning)

    
void process_thetahat_warmup(struct bidir_peer* peer, struct radclock_handle* handle,
		struct radclock_phyparam *phyparam, vcounter_t RTT,
		struct bidir_stamp* stamp)
    
void process_thetahat_full (struct bidir_peer* peer, struct radclock_handle* handle,
		struct radclock_phyparam *phyparam, vcounter_t RTT,
		struct bidir_stamp* stamp, int qual_warning)
    
    
int process_bidir_stamp(struct radclock_handle *handle, struct bidir_peer *peer,
		struct bidir_stamp *input_stamp, int qual_warning)

    
  - mutex stuff:  Only globaldata_mutex used and have TODO FIXME to remove threads completely from here
	  - appears near end only to protect updating of all radclock algo related handle members
	  - but why protect, who else would alter this info? answer, VM stuff
	  - PEER_ERROR used here, defined in sync_algo.h, all other such macros in radclock_daemon.h, since only this fn uses it
    
	 
 - on first stamp, goto output_results;  so has to do some canonical actions itself, like copystamp etc
   ugly, whereas they are also done later for i>0  . stupid, lame.
   - also stamp 0  is not added to peer->stamp_hist normally, but in init_peer .
	
     
	 
	 
TODO:
  - ca member of radclock_data  poorly named, could use K like in sync_ToN paper, but in
    fact is K-thetahat, so deserves a different name
    Also, K is called the old C inside  sync_algo.h:struct bidir_peer,  should change to K,
	 C is already very overloaded - start with this.
	 
  - reconsider passing in a peer variable  - means that all the algo internals are
    known to main even though it shouldnt, why?  just to avoid a few statics?
	 True it is convenient if you end up with a growing list of exceptions that main needs to know, but are there any??
	 
  - check/fix wierdness about Stamp-peer queue routines  in create_stamp.c above
  - PLOCAL_{STOP,START,RESTART}  in sync_algo.h no longer seem to be used, nothing in conf, nor in the algo.   Instead,
  plocal seems to be on all the time, with the radclock_{get,set}_local_period_mode library reading options allowed it to be ignored or not at clock reading time, with a separate preference per-radclock   Need to double check that in fact no longer exists, and check if this is what I want.  Is true that per-radclock option only has meaning if in fact on all the time, and this is an expert option anyway.  When would u not want it on?
      maybe in a stripped down IoT system where the whole plocal algo is pointless overhead (or maybe even more important there..)
		in terms of the code for the copies made, no need to be informed of whether plocal running, just combine p and plocal in the same way, if plocal=p, no harm done. To control it under the scenes,  we need is a way to tell the algo itself whether to store a plocal being calculated into the plocal field, or instead to just put pbar there. [of course, the old way of not running it at ALL will save some resources, but will mean delays and errors if suddenly want to switch it on.]
		  Ahh, still one more issue, if have option to switch off, will also keep the ability to change on the fly?   it does add a lot of complexity to allow plocal algo to transition through a warmup when the algo overall is out of warmup..
		    Maybe a simpler compromise:  allow a conf option to have it or not, but NO changin on the fly, if you want to change mind, have to restart.  Advantages:
			   - supports lower resource option if needed, can turn the fucker off
				- if on, doesnt stop reading option from effectively disabling it.
		      - only downside is cant change on the fly, but who is ever going to need that?
		Neat implementation would just be to have
		  if conf_plocalon process_plocal_full, else plocal = pbar;
	 TODO:  remove process_plocal_warmup, just put the code in directly, simpler
	 
  - in ffcounter_to_abstime_shm  etc, plocal only used from last stamp to the present, seems local, since K is linked to pbar, but is that right?
  
  

	 
	 
	 
###################################### Leapsecond tracking:

// sync_algo.h
struct bidir_output
    	int leapsectotal;		// bidir radclock output, ok, algo output, no
		
struct bidir_peer {
		int LI
	- appears to be set to the value for the last stamp, just like TTL, stratum, refid,
	  done in create_stamp.c:get_network_stamp
	  Comment on why store this a 2nd time, as is already in ??
	  to
		
// radclock_daemon.h
struct radclock_handle {
		void *algo_output    // not just algo output, is radclock output .. algo suggests `peer` only
		
// radclock_main.c:create_handle()
handle->algo_output = (void*) malloc(sizeof(struct bidir_output));
memset(handle->algo_output, 0, sizeof(struct bidir_output));  // initialises leapsectotal

Hence:  leapsecondtotal always accessed as
    ((struct bidir_output *)handle->algo_output)->leapsectotal
// should remplace with  OUTPUT(handle, leapsectotal)

process_bidir_stamp:    // only mention of "leap"
  	/* We don't want the leapsecond to create a jump in post processing of data,
	 * so we reverse the operation performed in get_bidir_stamp (now get_network_stamp). With this
	 * implementation this will not have an impact on the matlab output file
	 */
	RAD_DATA(handle)->ca -= ((struct bidir_output *)handle->algo_output)->leapsectotal;
    
	   - but this comment wrong, get_network_stamp doesnt do anything, I think meant to say
		  process_stamp which is called just before
      - should be, outside algo, but in terms of storing it directly in globaldata..
		  is it currenty the algos job to
      - doesnt impact ascii data output directly, but since .xmt stamp generated
        by RADclock, it will impact the matching key and hence stamp creation order,
        unless leapsectotal disabled anyway
				
#  Concept of   radclock `bootime`  and hence uptime
  daemon:
  		- neither concept appears, though all needed raw info is of course there
		- An uptime implies a long time interval, so must be measured using the abs clock, cant
		  evaluate the current clock at 0, that is using the difference clock !
		  Hence to calculate it must read the clock at bootime, when clock doesnt even exist !
		  Could wait until clock ok, then say at SKMscale, look back to retrospectively read at
		  vcount=0, but not so accurate.  Plus, the clock could be restarted well after boot..
		  Really here you do need to take an external reference,
		  like the first Ta or some RTC thing, and use that.  This leads to the kernel concept of
		  a bootime variable.
		- the leapsecond issue is separate. The raw info there is available  (current leapsectotal
		  now, knowing it was 0 at boot) so this can be inverted in principle.
			
			
		- closest thing is Ca(0) = ca   giving a bootime which will vary with ca=K-thetahat
		  as thetahat varies.   But ca also has leapsectotal built in:  ca=K-thetahat-leapsecondtotal
		  So Ca(0) is the UTC time, as currently understood, of boot, not the UTC at the time.
		- rad_data doesnt have leapsecondtotal  so cant invert, yet cest can!
		  so cest is more complete in this sense than rad_data !  not good !
		- in fact bootime mainly interesting in order to get uptime
		- this is not avaiable via the radclock api:   but could be obtained by
		  augmenting rad_data with leapsecondtotal, since leapsecondtotal
		  at boot time =0   To support properly, would have to define new functions, or
		  incorporate new uptime flags, into library functions.

	FFclock:
		- ffclock_bootttime global used in ffclock_abstime, sysclock_snap2bintime, tc_windup
		  to support UPTIME variants:
			if ((flags & FFCLOCK_UPTIME) == FFCLOCK_UPTIME) {
				bintime_sub(bt, &ffclock_boottime);
		- but what is done with leapsecondtotal also crucial to get right final result
		- thus ffclock_binuptime  uses  FFCLOCK_UPTIME  but Not  FFCLOCK_LEAPSEC
		  which makes sense
		- ffclock_boottime  only set in :
				ffclock_reset_clock    whereas it should come from daemon though from the daemon, it
				   would change on every stamp ...
		- this is a pure FF thing trying to emulate the bootime approach for the FB, it does not come
		  from the daemon, but in some ways it should, especially if the daemon starts supporting it too.
		  Otherwise you again get the situation where the clock is somehow split between sysclock code and
		  the daemon, a two-headed monster, not the right idea.
		- BUG:  cest->update_time    currently uses ca, hence has leaps added, is UTC
		    But it is used in ffclock_abstime  as if it is the leap-free version of Cabs
		  

				
		
		
#  Leapseconds and reading Ca :
 Rad_data has no leadsecond field, and since the SHM is just Rad_data, the same is true of
 all libprocess radclock Ca timestamping.
 Approach is to built it into rad_data.ca ,  since this is already a lumped constant = K - thetahat,
 and it reduces computation:  the leap addition only one once for all reads, and if you store a
 (p,ca,Tlast,plocal) database, you get the right leapsecond factor as well.
 
 On the other hand if you want uptime you will have to do the extra work or removing the leapsec_total
 from ca .   Since uptime is less frequently needed, makes sense to built in as a default, but should
 name it better:   ca = Ca(0) = K-thetahat-leapsecondtotal,  so is "caUTC"
 
 
 Question is how has responsibility for adding it in and dealing with these issues, the algo
 fills rad_data, but it shouldnt know about leaps.
 
 So there are different sources of rad data in a way, some with leaps in, others not:
 	rad_data:   (including SHM copy and push to kernel):  leapseconds added into ca
	algo_output:  has (C,thetahat, leapsectotal), so info there, but no composed "ca"
	active_peer:  peer variable: has stamp (last stamp processed) , phat, C, but not leapsectotal
 this is a mess... no clear ownership

    
# Main excecution flow is in process_stamp :

 	- all commented out:
switch ( PKT_LEAP(ntp->li_vn_mode) ) {		// ntp not defined  already in stamp.leapsec
	case LEAP_ADDSECOND:
		((struct bidir_output *)handle->algo_output)->leapsectotal+=1;
		verbose(LOG_WARNING, "Leap second change!! leapsecond total is now %d",  ..
		break;
	case LEAP_DELSECOND:
		((struct bidir_output *)handle->algo_output)->leapsectotal-=1;
		verbose(LOG_WARNING, "Leap second change!! leapsecond total is now %d",  ...
		break;
	case LEAP_NOTINSYNC:
	case LEAP_NOWARNING:
	default:
		break;
}
// Remove total detected leapseconds from UNIX timestamps taken
// from server if clock jumps back, this brings it forward again
//BST(stamp)->Tb += ((struct bidir_output *)handle->algo_output)->leapsectotal;
//BST(stamp)->Te += ((struct bidir_output *)handle->algo_output)->leapsectotal;
 		
	
Simplified code:
  laststamp = stamp; 		// store current stamp in its original form
  if  firststamppastLeap(relevant info) {
	  leapsectotal++
	  
  }
  //  Always apply, to remove all leaps from all server stamps [this part was correct]
  //  here stamp is a local, saved in lastst
  BST(stamp)->Tb += leapsectotal
  BST(stamp)->Te += leapsectotal
  
  // pass leap-cleaned version of bidir stamp to algo
  process_bidir_stamp(handle, peer, BST(&stamp), stamp.qual_warning);
		stamp = *input_stamp  // local pointer to passed stamp  // why? ask Peter
 		copystamp(stamp, &peer->stamp);   // copy to peer variable, this is what used to be laststamp
													 // note full bidir stamp history not kept
	 
	 
	 
	 
// PKT_LEAP defined in proto_ntp.h:
PKT_LEAP(li_vn_mode)	((u_char)(((li_vn_mode) >> 6) & 0x3))   // extract LI bits
- used to get the LI bits and store in stamp.leapsec  in
  create_stamp.c:push_server_halfstamp

// radapi-base.c:fill_ffclock_estimate  :


Seems to be nothing else, it is all `TODO` and currently it is wrong.`TODO`
except leapsectotal never actually incremented, so algo will see the jump
in server timestamps and go crazy, but shouldnt modify the SW stamps..


# Communication of leap info
 - extracted from packets in ...
 - stored per-stamp in stamp_t stamp.leapsec  variable in queue   // should call member LI !
 
 
# Big picture:
Three key issues:
  1)  Algo protection:  to reliably protect the algo from leaps, must update leapsectotal
  		when  Tf > t*  for the first time, then block relapse for any reason
			- not ESsential if an error made, algo will reject based on sanity, will self-protect
			- could be done with reasonable heuristics not needing full approach, see below
			- in common case, first stamp past leap will be way past, should be easy in principle
			- if trust server, could also add extra check to see if server jumped as expected, but tricky,
			  what if didnt jump, decide to follow it?  no, radclock philosophy is to NOT follow
			  servers into stupidity.  So what the server does is irrelevant, must know when
			  a leap is coming, then decide to jump yourself.
  2) UTC Ca correctness at algo updates
  			- failing to update leapsectotal in time means the UTC clock in rad_data is wrong, and
			  will stay wrong for at least one more poll_period.  Too early and it was wrong
			  for poll_periods on that side
			- so although missing the leap by a stamp or two ok for algo protection, NOT ok for
			  final Ca clock
  3) UTC Ca correctness inbetween algo updates
		- even assuming the updates are perfect, the UTC Ca will be wrong over [t*, next Tf]
		- same true of kernel version and libprocesses of course
	 	- is a new kind of interpolation pb.  Not just incrementing using local counter,
		  now need to know in advance when leap happens, add sth in mid-stamp, which is
		  outside of the algo, and even outside of the daemon currently given it is triggered
		  on pkt arrivals !
		  	- will need to see that one is coming up, then set timer to change leapsectotal
			  at the right time, and repush it to kernel and IPC.
			  - alternative is this temporary leap stuff like in the kernel, where you have
			    a predicted vcount for the event, then include it when it happens. Devil is in the
				 detailed interaction between the on-the-leap stuff and leapsectotal
			- Nice advantage is that if can do this right, then when next pkt
			  comes in, no need for any new work, leapsectotal is already good !
			- may be that existing elements were well on way to nice soln, but never finished.
		- problem needs to be solved both by the daemon and FFclock
		- here maybe info can flow from kernel -> daemon, if kernel gets leap info from
		existing leap mechanism:  need to see how cest leap members set !  I think it is
		by the daemon, which is an issue as lots of work to make that work,
		and a waste of existing kernel mechanisms
		- in some ways, this is a training wheel problem for the CaMono in daemon versus kernel issue
		
		
Heuristic for 1)   (assume +ve leap for simplicity)

	Qn:  are the functions giving time+date as a fn of UTC seconds aware of leaps?
			in advance??  ever??
		  Would be much easier if knew the integer for 1st sec of new month, even if didnt know if
		  a leap or not, but need to know if it knows !
			Even this heuristic relies on knowing which second corresponds to the end of the month!!
			
			
//=========






########################################################################

## virtual_machine.c
Lots of stuff in here..  with Tim

globaldata_mutex:  protects competition with DATA_PROC thread, wrt handle->rad_data  data:
   DATA_PROC:			updates at end of sync_bidir()
	VM_UDP_SERV:		updates in receive_vm_udp (dont understand)
							reads   in thread_vm_udp_server


static int receive_vm_udp(struct radclock_handle *handle)
 - uses globaldata_mutex


void * thread_vm_udp_server(void *c_handle)
 - VM thread main function
 - uses globaldata_mutex




########################################################################
## Note archive
         
==================================
Stamp queue temporal ordering definition, and associated cleaning out stategies,
once target fullstamp (with Ta* Tf* ) found.

Idea:  order based on trusted temporal ordering of bpf delivered raw timestamps, ie vounts
Choose a particular vcount field to Define temporal order of stamps.
Scan queue, and remove all stamps that are older wrt this  [ie in queue longer = smaller value]

pp = poll period
ooo = out of order
pplb = per-packet load balancing

Ta based:
  Pros:  - natural and original defn
         - natural fit to main motivation of queue: buffering burst while waiting for replies
         - correctness immune to reordering events beyond host outgoing, in particular network
         - get natural clean out of client halfstamps who will never get a match (missing s)
         
  Cons:  - discards c-halfstamps whose replies come `late` (ooo), losing an otherwise valid
           (though overlapping) stamp, subsequently generating orphan server halfstamp
           [though such a stamp is of lower quality than the other overlapping one, so ok to drop]
           In other words, vagaries of reordering sets arrival order and so which stamps get dropped
         - persistent network reordering, eg pplb with small pp => regular stamp loss
         - server halfstamps cant be cleared by order defn, need extra rule, eg:
           remove if Tf<Tf* of current fullstamp
             - since s-halfstamps are wierd, and unlikely to find the missing c, discard is ok
             - Maybe in SHM this can happen due to outgoing overload, should monitor

  Summary: natural order and logic, and natural supplementary rule for s-halfstamps cleanout
           (no stale queues) but means that ooo => lost stamps.


Tf based:
  Pros:  - natural in that this is The order in which fullstamps arrive, and are seen by algo
  		   - wont discard c-halfstamps, if their replies finally arrive, will get the stamps!
         - correctness immune to reordering events before host incoming, eg network or busy host
         
  Cons:  - vagaries of network reordering, incl pplb, set order defn, eg simple burst generates a congestion and route dependent reordering, loses natural logic of `ordered attempts`
         - clean out only targets two kind of events, both `impossible`:
            - where Tf smaller even though arrives later
            - s-halfstamps who will never get match [ c must have been sent some time ago, but lost]
         - c-halfstamps cant be cleared by rule, would stay in queue forever until overflow
           Extra rule not so obvious, Ta<Ta*  would have same drawback as above
           Need timeout of some kind, and would be used continuously since missing replies common
           
  Summary:  natural in terms of data arrival order, and avoids losing stamps due to reordering,
            but at cost of treating wierd case as the common case, and queues full of stale data.
            
            
Conclusion:   Ta is simpler and better.  If ooo becomes endemic need to think again, otherwise
              this covers all bases.
            
Note:  duplicates are separate to this, ordering is not the issue, and all duplicates should
       simply be discarded. By defn they will not generate a full stamp as they have no effect,
       so no other implications, unless it is happening all the time in which case we have a
       real problem..  However, should they be treated as a fatal error or just discarded?
            
	    All this code can do in enforce a reasonable defn of stamp order.
		 Larger qn is whether algo itself can handle overlapping pkts. Need to know this,
       as with much faster polling with 1588 work, may become common. From memory,
       is mainly fine by construction, but may be cases where it is a problem.
												 
      
 Compared to my original code
     	- allows match even if s arrives before c [previously s discarded, then c overwritten]
        	- could happen in bursts with busy OS making rd arrive out of order,
              maybe common in SHM experiments with high load, probably why did this
        - allows burst of c's to be buffered until their s`s arrive, can match them all,
           THIs is probably the original reason for the queue - just a client buffer.
        - allows match even if s pkts arrive out of order [previously older c pkt
           would already be overwritten, so lose older stamp]

================================================================




########################################################################

C reminder sheet:

- case statements fall through !
- break:     exits any loop (for, while, do while) from any position, and case to exit switch
				With for, no final loop increment is performed, just exit
- continue:  jumps back to start of any loop. If case of for, increment is done
- false=0, true is anything else
	hence if err = fn(),   then fn needs to return zero on success
- free(ptr)  frees memory pointed to by ptr,
  ptr = newvalue;  // after also have to reset prt to desired new value, often NULL.
- for (;;) will loop forever unless break, return (or goto) forces exit
- returned error code convention:
	- 0 is success, else failure (ie TRUE) Thus call it 'err', so can use:  if (err) then..
	- pass up, let each fn add its own failure message knowing its context
	  passing up sth too complex would generate geometrically increasing complexity
- adding an integer i to a pointer ptr in fact jumps the address in units of the size of the
  object, ie i+ptr  really points to  ptr + i*sizeof(*ptr)
  Nice example in kern_tc.c:ffclock_init


Scope Rules:
- function arguments
	- functions ALWAYS pass arguments by value
	- to get by-reference functionality, pass a pointer to the data to be changed.
     [ inside the function this pointer will be a copy, but will point to the same data ]
	- eg: if an argument is a pointer that itself needs changing, pass a pointer to this pointer.

- file scope:
	- if declared outside functions in some file.
	- visible between their declaration and the end of the file
	- automatically gives them external linkage - they are global
		- but still need (extern) declarations in other files as compiler needs to know the type.
	  	- usual method: put the extern decl in a .h and include that wherever needed
	- add static keywork to kill external linkage (scope will just be the file)
	
- #define:
   - not scope in a true sense, but effectively has scope from point of defn to end of file
	  To remove its effect lower down in a file, must undef it.
	- limited to file, but of course if a header file and included elsewhere..
	- must ensure is only set once, can test for it, undef it then redef it to ensure this
	  They are dangerous.
	
- extent:  that is lifetime. Can be local or static (eternal)
- static keyword:
	- used to restrict the visibility of a function or variable to its translation unit.
   - Functions :
		- just means keep the fn in file scope, dont give it external linkage
		- put it on both defn and decl
		- on decl, just means will find defn later in the File
   - Variables :
		- is always on a defn
		- also means has static extent
		- is initialized automatically or manually only once
	 	- so when used on a variable defn in a function, scope doesnt change
		  (was local, stays local), but lifetime increased. Initialization occurs once only.
		  
   - Always use static in .c files unless you need to reference the object from a different .c module.
  	- never use static in .h files, because you will create a different object every time it is included.

   - use to effectively define functions using inline, eg from kernel_summary_notepad:
		Note the _fromclock fns are declared as static inline, which means whenever
timeffc.h is included, these "functions" are expanded like a #define, but with
type checking, so that are not really new fns, so ok to define them in a .h file. 
Since they are static, any such inclusion/expansion is sealed from any other, 
avoiding any confusing about multiple definitions.

 - casting:
   - has higher precedence than * / %
   - more complex egs:
     ((struct bidir_peer*)(handle->active_peer))->poll_period   // need extra brackets


- extern keyword:
	- indicates variable has external linkage and is defined elsewhere
	- indicates variable has static extent
	- indicates function has external linkage and is defined elsewhere, just as for variables, but the keyword is not needed, it is understood to be external anyway

- global variables
  - no global keyword.
  - Any variable with file scope has external linkage and is globally available as above.
  - omitting extern in a file doesnt mean excluding the global from that file !
    on the contrary if it compiles, it just confirms that the global is not used there.
	 Hence:  control use of globals by minimizing files that need externs for them.
	 If it doesnt compile, then the global is used and must add extern to get it to compile.

- Initializing variables  [ needs more work ]
	- externs (sth in static or global scope) are initialized automatically to 0
	- variables in fns are not
	- in general should always initialize explicitly but
		- not in the extern case?  may be wasteful?
		- when just about to call myfn(&var)  since its job is to catch a returned value,
		  any initialized value is confusing as it will be overwritten by myfn by design anyway, hence stupid
	 	- when it is meaningless as it must be set elsewhere:  here initializing would only
		  prevent the compiler from warning you that it was never set to a value elsewhere.

- typedef
	- think of as a shorthand alias for some constructed type, rather than defining a type, a bit more natural
	- eg:

- header file inclusions
  - order matters as h files use definitions that must be declared at least in advance
  - re-ordering of ..

# printf conversion characters:
z:  For integer types, causes printf to expect a size_t-sized integer argument.
j:	For integer types, causes printf to expect a intmax_t-sized integer argument.
t:	For integer types, causes printf to expect a ptrdiff_t-sized integer argument.
q:  For integer types, causes printf to expect a 64-bit (quad word) integer argument.
    Commonly found in BSD platforms.
i:  should be synonymous with %d for printf





### Library Functions   [ ISO C, POSIX, POSIX extensions ]


===============================================================================
## ISO C Standard


# assert.h

	 void assert(int expression)
	  - if expresssion zero, then
		  prints to stderr: "Assertion failed:  expression, file _FILE__, line __LINE__"
		  abort
	  - if NDEBUG defined before assert.h included, then assert macro is ignored


# string.h    // http://www.cplusplus.com/reference/cstring/strncpy/
	 const char * strchr ( const char * str, int character );
			 char * strchr (       char * str, int character );
	  - Returns a pointer to the first occurrence of character in the C string str.

	 char * strncpy ( char * destination, const char * source, size_t num );
	  - Copies the first num characters of source to destination.

	 int strcmp ( const char * str1, const char * str2 );
	  - compares the C string str1 to the C string str2.
	  - returns (<,=,>) 0 if str1 (<,=,>) str2.
	  
	 memcpy(dst,src,len)



# stdlib.h

# math.h

# stdarg.h

# signal.h

# time.h

clock_t clock(void)   // returns processor time (measured in HZ ticks since execution)

# syslog.h   //  Single Unix Specification    is this ISO in fact?
// http://pubs.opengroup.org/onlinepubs/7908799/xsh/syslog.h.html

Defines the #define LOG_{EMERG,...} levels of the LOG facility used by the daemon

Defines fns:
 
void  closelog(void);
void  openlog(const char *, int, int);
int   setlogmask(int);
void  syslog(int priority, const char *message, ... /* argument */);





===============================================================================
## POSIX.1

# sys/types.h		// primitive system data types
	clock_t		counter of clock ticks (for process time)
	timer_t		counter of seconds of calendar time
	size_t		sizes of objects (such as strings) unsigned
	time_t      an integer suitable to hold a whole second component (I think it is here..)

# sys/times.h		// process times




===============================================================================
## POSIX XSI extension headers


# sys/time.h		// time types
  - definitions of bintime, bintime manipulation, etc
  - see kernel_summary_notepad for summary


# sys/timeb.h		// additional time and date defns



===============================================================================
## Header file conventions

Once-only header trick:
	- known also as  wrapper #ifndef, controlling macro or guard macro
	- normally system .h files prepend _, user .h files shouldnt










RADclock V0.4 documentation.
Written by Darryl Veitch, 2018
===============================

Convention to point within code:
	/dir/dir/file : functionorstructureinfile

Glossary:
 algo = algorithm, the RADclock clock synchronization algorithm


==============================================================
Summary
=======

This documents the verbosity, logging and debug systems of radclock.
The source files are in RADclockrepo/radclock/radclock/
The main ones are
	verbose.c
	main.c
	config_mgr.c
The main associated header files are
	verbose.h
	config_mgr.h
	jdebug.h

Also covered here is the RADclock library''s logging system which is built on
top of the daemon''s system
The source and header files under RADclockrepo/radclock/libradclock are
	logger.{h,c}



==============================================================
Introduction
============

The verbosity system provides the means to embed the generation of informative
messages about program execution throughout the code, and send them to log file(s).
The system in fact supports three different messaging needs:
    Messages from the daemon suitable for the standard syslog/error interfaces	LOG_{}
	 Messages from the daemon to track the performance of radclock algo internals	VERB_{}
	 Simpler messaging needed for `libprocesses` using radclock							RADLOG_{}

In addition, there is a completely separate debug system.
Whereas the verbosity system is about providing informative output on runtime state and
conditions and is part of normal operation, the debug system is based on
additional code placed in targeted locations, based on macros, and is
compiled in only when detailed debugging is required.



==============================================================
Verbosity System Basics
==============================================================

The underlying system is provided in verbose.{c,h}, whose main function is
void verbose(int facility, const char* format)
where format contains the desired message to be printed.
The facility argument allows different messaging treatments to be given according
to two classes (facilities): VERB and LOG.


VERB relates to messages specific to the RADclock algo, and takes levels
#define VERB_{DEFAULT,QUALITY,CAUSALITY,SANITY,CONTROL,SYNC,DEBUG} = {10,11,12,13,14,15, 20}
For these the configuration parameter conf->verbose_level controls whether
they will be printed or not.
Three values are supported:
	0:  `Off`    no VERB messages will be printed
	1:  `On`    all VERB messages will be printed except VERB_DEBUG (default)
  >1:  `Max`   all VERB messages will be printed

VERB messages are printed to a radclock specific logfile, with run_mode dependent defaults:
#define DAEMON_LOG_FILE	"/var/log/radclock.log"      // if live And daemonized
#define BIN_LOG_FILE		"radclock.log"               // otherwise
which can be overridden via conf->logfile,  and messages are printed to stderr
if the file is not available.


LOG relates to messages specific to the RADclock process generally, and takes expected levels
#DEFINE	LOG_{EMERG,ALERT,ERR,WARNING,NOTICE} = {0,1,2,3,4,5}
which map to standard labels, defined in syslog.h, within the syslog system.
Other LOG_ values will also be accepted, and in fact any non VERB_ value will
be treated as a LOG_ level and be printed in the same way.
For LOG the configuration parameter conf->verbose_level is ignored.

LOG messages are also printed to a radclock logfile as above, if available.
In addition however they are also printed to standard logging locations, depending
on run_mode:
  RADCLOCK_SYNC_LIVE   via the standard syslog() function.
	 This system is initialized in main.c:daemonize() using
		setlogmask (LOG_UPTO (LOG_DEBUG));		// allow all levels
		openlog ("radclock ", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON); // LOG_DAEMON facility
	 and shutdown in main using closelog()
  RADCLOCK_SYNC_DEAD   stderr  (since in that case there will be an associated terminal)


An abuse of the above VERB/LOG division is allowed to support convenient runtime
verbosity control of non-VERB messages. Namely, verbose_level values above 1 can be used
to activate arbitrary additional verbosity, including that written using
verbose(LOG_*) or logger(RADLOD_*) levels.  For example in clock_init_live:
if ( VERB_LEVEL>1 ) printout_FFdata(&cest);   // printout_FFdata uses logger(RADLOG_NOTICE


In all cases, the actual string printed is as follows :
	 fprintf(destination, "%s: %s%s\n", ctime_buf, customize, str);
where str is the desired message, customize a string associated to the facility
level (defined to be blank if not recognized), and ctime_buf is a string taking values
	"-RADclock Init-"   			initially (before daemonization or conf file reparsing)
	"Replay"           			if RADCLOCK_SYNC_DEAD
	A RADclock UTC timestamp  	if RADCLOCK_SYNC_LIVE



Access to the system is provided by inclusion of verbose.h, which defines
extern struct verbose_data_t verbose_data;    // global defined in verbose.c

struct verbose_data_t {
	struct radclock_handle *handle;  // give the system access to radclock data
	int is_daemon;          // shortcut to find out if running live
	int is_initialized; 		// records if radclock is initialized, ie if run_mode known
	int verbose_level;      // verbosity level controlling which VERB_ messages printed
	char logfile[250];			// name of logfile when known
	FILE* fd;               // pointer to logfile
	pthread_mutex_t vmutex; // protects all code in verbose()
};


Also defined are
  external functions {get,set}_verbose()
  get_verbose_level()   accessed via   #define VERB_LEVEL get_verbose_level()




==============================================================
RADclock library Logging System
==============================================================

The logger system is the RADclock library version of the daemon''s LOG facility,
which is itself hidden to users. It is exposed in logger.{h,c}, and defines the function
	void logger(int level, char *fmt, ...)
which supports three message types:
	logger(RADLOG_{ERR,WARNING,NOTICE}, msg)
which are mapped behind the scenes by static function main.c:logger_verbose_bridge() to
	verbose(LOG_{ERR,WARNING,NOTICE}, msg)

The function set_logger is used to point a generic library logging fn name (priv_logger) to
logger_verbose_bridge in order to hide the latter from the library.

Note that this is a LOG facility, not a VERB facility, and so messages dont need
a special logfile, they are sent to syslog or stderr as described above
for verbose(LOG_ , msg) messages.

Within library functions verbose is not available, logger must be used!



==============================================================
Initialization of System
==============================================================

This is performed in main, and is somewhat complex because of the need to make
logging available very early, before critical issues like run_mode have been
determined, or the conf file parsed (which could influence the logfile filename,
and the value of verbose_level).

The initialization starts with direct assignments within main, then repeated use of
	 set_verbose(handle, handle->conf->verbose_level);
	 set_logger(logger_verbose_bridge);
within main and finally via a call to init_handle, which also calls set_verbose, this
time setting is_initialized to 1 (this is the only place where this occurs).

Set_verbose sets most members of verbose_data, but NOT fd, only verbose() does
this via a call to verbose_open().

In all cases a call to verbose will work.  It will try to open a logfile
but if it fails, it will fall back to outputting to stderr if needed.
All LOG messages will therefore be logged, but VERB messages will depend on verbosity level.

Prior to the initialization, needed verbosity is achieved by direct use of
fprintf to stdout  (for example in command line parameter parsing), or
stderr, and the output is the first to appear.

In command line parsing, each occurrence of option "v" increases verbose_level
by one, eg  -vvv results in verbose_level=3 .


==============================================================
Verbosity Configuration
==============================================================

In config_mgr.h is defined
#define DEFAULT_VERBOSE			1	// used only in config_mgr.c:{write_config_file(),update_data()}
#define CONFIG_VERBOSE			10 // code used to map to "verbose_level" string in keys[]

The daemon-wide verbosity level is recorded in conf->verbose_level however unlike
all other conf members it is not initialized by config_init().
Instead, this is performed by update_data() within config_parse() which gets the value from
the config file and overwrites with the command line value if present.

Verbosity level can be altered by reparsing of the config file via config_parse
(see RADclockDoc_config) and will be reset to DEFAULT_VERBOSE if an error.



==============================================================
Debug Support
==============================================================

This `system` is a set of macros defined in jdebug.h, protected by the guard symbol
#ifdef WITH_JDEBUG      //  symbol defined (or not) in config.h
and are defined as blank values if WITH_JDEBUG not defined so that macros calls
can remain in the code but be inactive if required.

The macros are shorthand for crafted fprintf statements designed to help keep track
of resource allocation of the different threads.  They are supported by
	static inline char * pthread_id()		// returns what thread is running
	extern struct rusage jdbg_rusage;		// tracks resource usage
	long int jdbg_memuse = 0;					// tracks total memory use (I think)


Macros:

  JDEBUG      fprintf(stdout,"format", pthread_id(), __FILE__, __LINE__, __FUNCTION__)
        							// seems to track if a fn is being entered, and by expected thread
  JDEBUG_STR(_format,...)  // same, + extra args and format for them
  JDEBUG_RUSAGE( , )  		// system resource use stats, relies on jdbg_rusage

  // macro to track memory leaks
  JDEBUG_MEMORY(_op, _x)    // version for __FreeBSD__ and other
      same + malloc memory info:
		  _op=JDBG_MALLOC  gives size of x, and increments total ever malloc'd'
		     =JDBG_FREE          "     "   ,     decrements     "      "


Examples of use are found throughout the code.



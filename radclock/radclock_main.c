/*
 * Copyright (C) 2006-2012, Julien Ridoux and Darryl Veitch
 * Copyright (C) 2013-2020, Darryl Veitch <darryl.veitch@uts.edu.au>
 * All rights reserved.
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

// TODO we probably don't need all these includes anymore
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <net/ethernet.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "../config.h"
#include "radclock.h"
#include "radclock-private.h"
#include "kclock.h"

#include "radclock_daemon.h"
#include "logger.h"
#include "verbose.h"
#include "sync_history.h"
#include "sync_algo.h"
#include "create_stamp.h"
#include "config_mgr.h"
#include "pthread_mgr.h"
#include "rawdata.h"
#include "stampinput.h"
#include "stampinput_int.h"
#include "stampoutput.h"
#include "proto_ntp.h"
#include "jdebug.h"

#include <sys/sysctl.h>

/* Default PID lockfile (-P overrides this) */
#define DAEMON_LOCK_FILE ( RADCLOCK_RUN_DIRECTORY "/radclock.pid" )



/* Globals */

/* RADclock handler */
struct radclock_handle *clock_handle;

/* Verbose data contains pthread_mutex */
extern struct verbose_data_t verbose_data;

/* Debug */
#ifdef WITH_JDEBUG
long int jdbg_memuse = 0;
struct rusage jdbg_rusage;
#endif




/*************************** Helper Routines ******************************/

/*** Guide to input parameters of radclock ***/
static void usage(void) {
	fprintf(stderr, "usage: radclock [options] \n"
		"\t-x do not serve radclock time/data (to kernel or processes)\n"
		"\t-d run as a daemon\n"
		"\t-c <filename> path to alternative configuration file\n"
		"\t-l <filename> path to alternative log file\n"
		"\t-i <interface>\n"
		"\t-n <hostname> the host running radclock\n"
		"\t-t <hostname> the timeserver radclock will query\n"
		"\t-p <poll_period> [sec] default is DEFAULT_NTP_POLL_PERIOD=16\n"
		"\t-r <filename> read raw sync input from pcap file (\"-\" for stdin)\n"
		"\t-s <filename> read sync input from ascii file (header comments and "
				"extra columns skipped)\n"
		"\t-w <filename> write raw sync output to file (modified pcap format)\n"
		"\t-a <filename> write sync output to file (ascii)\n"
		"\t-o <filename> write radclock algo output to file (ascii)\n"
		"\t-P <filename> write pid lockfile to file\n"
		"\t-U <port_number> NTP upstream port\n"
		"\t-D <port_number> NTP downstream port\n"
		"\t-v -vv verbose\n"
		"\t-V print version\n"
		"\t-h this help message\n"
		);
	exit(EXIT_SUCCESS);
}









/*-------------------------------------------------------------------------*/
/************************ Daemon(-like) Routines ***************************/
/*-------------------------------------------------------------------------*/

/*
 * Reparse the configuration file when receiving SIGHUP, and take
 * special initialization actions if needed, and print out updated parameters.
 * Parameter changes handled directly by PROC:
 * 	UPDMASK_ADJUST_FFCLOCK
 * 	UPDMASK_ADJUST_FBCLOCK
 * Parameter changes handled by the algo only:
 * 	UPDMASK_DELTA_HOST
 * 	UPDMASK_DELTA_NET
 * 	UPDMASK_POLLPERIOD
 * 	UPDMASK_TEMPQUALITY
*/

// TODO : Reload of individual physical parameters is not handled
static int
rehash_daemon(struct radclock_handle *handle, uint32_t param_mask)
{
	struct radclock_config *conf;
	int err;
	
	JDEBUG

	conf = handle->conf;

	verbose(LOG_NOTICE, "Rereading configuration and acting on allowed changes");
	/* Parse the configuration file, update all conf values */
	if (!(config_parse(conf, &param_mask, handle->is_daemon, &handle->nservers))) {
		verbose(LOG_ERR, " Error: Rehash of configuration file failed");
		return (1);
	}

	if (HAS_UPDATE(param_mask, UPDMASK_SYNCHRO_TYPE))
		verbose(LOG_WARNING, " It is not possible to change the type of client "
				"synchronisation on the fly!");

	//XXX Should check we have only one input selected
	if (HAS_UPDATE(param_mask, UPDMASK_NETWORKDEV) ||
			HAS_UPDATE(param_mask, UPDMASK_SYNC_IN_PCAP) ||
			HAS_UPDATE(param_mask, UPDMASK_SYNC_IN_ASCII))
	{
		verbose(LOG_WARNING, " It is not possible to change the type of input "
				"on the fly!");
		verbose(LOG_WARNING, " Parameter is parsed and saved but not taken "
				"into account");
		CLEAR_UPDATE(param_mask, UPDMASK_NETWORKDEV);
		CLEAR_UPDATE(param_mask, UPDMASK_SYNC_IN_PCAP);
		CLEAR_UPDATE(param_mask, UPDMASK_SYNC_IN_ASCII);
	}

	if (HAS_UPDATE(param_mask, UPDMASK_VERBOSE)) {
		set_verbose(handle, conf->verbose_level, 1);
		//verbose(LOG_NOTICE, " Verbose level reset to %d", conf->verbose_level);
	}
		
// TODO: The old naming convention for server IPC could be changed for clarity.
// Would require an update of config file parsing.
	if (HAS_UPDATE(param_mask, UPDMASK_SERVER_IPC)) {
		switch (conf->server_ipc) {
		case BOOL_ON:
			err = sms_init_writer(handle->clock);
			if (err)
				return (1);
			verbose(LOG_NOTICE, " IPC Shared Memory ready and updating");
			break;
		case BOOL_OFF:
			verbose(LOG_NOTICE, " IPC Shared Memory no longer updating");
			sms_detach(handle->clock);		// detach segment, but do not destroy!
			break;
		}
	}

	if (HAS_UPDATE(param_mask, UPDMASK_SERVER_NTP)) {
		switch (conf->server_ntp) {
		case BOOL_ON:
			verbose(LOG_NOTICE, " RADserver will be activated");
			//start_thread_NTP_SERV(handle);   // now done in start_live
			break;
		case BOOL_OFF:
			handle->pthread_flag_stop |= PTH_NTP_SERV_STOP;
			verbose(LOG_NOTICE, " RADserver has been shut down");
			break;
		}
	}

	if (HAS_UPDATE(param_mask, UPDMASK_SERVER_VM_UDP)) {
		switch (conf->server_vm_udp) {
		case BOOL_ON:	
			//start_thread_VM_UDP_SERV(handle);
			break;
		case BOOL_OFF:
			clock_handle->pthread_flag_stop |= PTH_VM_UDP_SERV_STOP;
// TODO should we join the thread in here ... requires testing
//			pthread_join(clock_handle->threads[PTH_VM_UDP_SERV], &thread_status);
			break;
		}
	}

	/* Management of output files */
	if (HAS_UPDATE(param_mask, UPDMASK_SYNC_OUT_ASCII)) {
		close_output_stamp(handle);
		open_output_stamp(handle);
		CLEAR_UPDATE(param_mask, UPDMASK_SYNC_OUT_ASCII);
	}

	if (HAS_UPDATE(param_mask, UPDMASK_CLOCK_OUT_ASCII)) {
		close_output_matlab(handle);
		open_output_matlab(handle);
		CLEAR_UPDATE(param_mask, UPDMASK_CLOCK_OUT_ASCII);
	}

	if (HAS_UPDATE(param_mask, UPDMASK_SYNC_OUT_PCAP)) {
		err = update_dumpout_source(handle, (struct stampsource *)handle->stamp_source);
		if (err != 0) {
			verbose(LOG_ERR, " Things are probably out of control. Bye !");
			exit (1);
		}
		CLEAR_UPDATE(param_mask, UPDMASK_SYNC_OUT_PCAP);
	}


	/* Change the filter on the open BPF device */
	if (HAS_UPDATE(param_mask, UPDMASK_SYNCHRO_TYPE) ||
			HAS_UPDATE(param_mask, UPDMASK_SERVER_NTP) ||
			HAS_UPDATE(param_mask, UPDMASK_TIME_SERVER) ||
			HAS_UPDATE(param_mask, UPDMASK_HOSTNAME))
	{
		err = update_filter_source(handle, (struct stampsource *)handle->stamp_source);
		if (err != 0)  {
			verbose(LOG_ERR, " Things are probably out of control. Bye !");
			exit (1);
		}
		CLEAR_UPDATE(param_mask, UPDMASK_TIME_SERVER);
		CLEAR_UPDATE(param_mask, UPDMASK_HOSTNAME);
	}

	/*  Print configuration actually used */
	config_print(LOG_NOTICE, conf, handle->nservers);

	/* Push param_mask into the config so that the algo sees it,
	 * since only algo related thing should be remaining
	 */
	conf->mask = param_mask;

	return (0);
}


static void
logger_verbose_bridge(int level, char *msg)
{
	switch (level) {
	case RADLOG_ERR:
		verbose(LOG_ERR, msg);
		break;
	case RADLOG_WARNING:
		verbose(LOG_WARNING, msg);
		break;
	case RADLOG_NOTICE:
		verbose(LOG_NOTICE, msg);
		break;
	}
}


/**
 * Signal handler function
 */
static void
signal_handler(int sig)
{
	switch(sig){

// TODO fix this commment
	/*
	 * We caught a SIGHUP, if the algo is processing data, we delay the
	 * configuration update to keep data consistent.  Other possiblity is that
	 * we are blocked on pcap capture function. We then force leaving the
	 * blocking state and the capture exit with a specific error code to goto
	 * rehash call.  Warning: this is valid as long as the pcap capture and
	 * pcap_breakloop calls are made from the same thread. If we fork the
	 * capture loop from the main(), this will not work anymore. See man
	 * pcap_breakloop
	 */
	case SIGHUP:
		clock_handle->unix_signal = SIGHUP;
		source_breakloop(clock_handle,
				(struct stampsource *)clock_handle->stamp_source);
		verbose(LOG_NOTICE, "SIGHUP scheduled after packet processing.");
		break;

	/*
	 * First of all raise our exit flag and break loop (blocking function for
	 * live input
	 */
	case SIGTERM:
		clock_handle->unix_signal = SIGTERM;
		source_breakloop(clock_handle,
				(struct stampsource *)clock_handle->stamp_source);
		break;

	/* user signal 1 */
	case SIGUSR1:
		verbose(LOG_NOTICE, "SIGUSR1 received, closing log file.");
		if (verbose_data.fd != NULL) {
			pthread_mutex_lock(&(verbose_data.vmutex));
			fclose(verbose_data.fd);
			verbose_data.fd = NULL;
			pthread_mutex_unlock(&(verbose_data.vmutex));
		}
		break;

	/* user signal 2 */
	case SIGUSR2:
		break;
	}
}


/*
 * Function that fork the process and creates the running daemon
 */
static int
daemonize(const char* lockfile, int *daemon_pid_fd)
{
	/* Scheduler */
	struct sched_param sched;

	/* Process ID, Session ID, Lock file */
	pid_t pid, sid;
	char* str = (char*) malloc(20 * sizeof(char));
	JDEBUG_MEMORY(JDBG_MALLOC, str);

	/* If already a daemon */
	if( getppid() == 1 ) {
		verbose(LOG_NOTICE, "Already a daemon");
		return (0);
	}

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}

	/* If we got a good PID, then we can exit the parent process. */
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* Change the file mode mask to the complement of mode 750 */
	umask(027);

	/* Open any logs here. Allow all levels */
	setlogmask (LOG_UPTO (LOG_DEBUG));
	openlog ("radclock ", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);

	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		syslog (LOG_ERR, "sid error");
		exit(EXIT_FAILURE);
	}

	/* Mutual exclusion of concurrent daemons */
	*daemon_pid_fd = open(lockfile, O_RDWR|O_CREAT, 0640);

	if (*daemon_pid_fd <= 0) {
		verbose(LOG_ERR, "Cannot open lock file");
		exit(EXIT_FAILURE);
	}

	/* The first instance locks the pid file */
	if ( lockf(*daemon_pid_fd, F_TLOCK,0) < 0) {
		verbose(LOG_ERR, "Cannot lock. Is another instance of radclock running?");
		exit(EXIT_FAILURE);
	}

	/* Record pid to lockfile (write is a no buffering function) */
	sprintf(str, "%d\n", getpid());
	write(*daemon_pid_fd, str, strlen(str));

	/* Change the current working directory */
	if ((chdir("/")) < 0) {
		/* Log the failure */
		syslog (LOG_ERR, "chdir error");
		exit(EXIT_FAILURE);
	}

	/* Close out the standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* Boost our scheduler priority. Here I assume we have access to the
	 * Posix scheduling API ... and if not?
	 */
#ifdef HAVE_LIBRT
	sched.sched_priority = sched_get_priority_max(SCHED_FIFO);
	if (sched_setscheduler(0, SCHED_FIFO, &sched) == -1)
		verbose(LOG_ERR, "Could not set scheduler priority");
#endif

	JDEBUG_MEMORY(JDBG_FREE, str);
	free(str);

	return (1);
}


static struct radclock_handle *
create_handle(struct radclock_config *conf, int is_daemon)
{
	struct radclock_handle *handle;
	handle = malloc(sizeof(struct radclock_handle));
	if (!handle)
		return (NULL);

	handle->clock = radclock_create();
	handle->conf= conf;
	handle->is_daemon = is_daemon;

	/* Multiple server management */
	handle->nservers = 0;
	handle->pref_sID = 0;
	handle->lastalarm_sID = -1;
	
	handle->run_mode = RADCLOCK_SYNC_NOTSET;
	strcpy(handle->hostIP, "");

	/* Output files */
	handle->stampout_fd = NULL;
	handle->matout_fd = NULL;

	/* Thread related */
	handle->pthread_flag_stop = 0;
	handle->wakeup_checkfordata = 0;
	pthread_mutex_init(&(handle->globaldata_mutex), NULL);
	pthread_mutex_init(&(handle->wakeup_mutex), NULL);
	pthread_cond_init(&(handle->wakeup_cond), NULL);

	handle->syncalgo_mode = RADCLOCK_BIDIR; // hardwired, as yet not really used
	handle->stamp_source = NULL;

	/* Raw data queues */
	handle->pcap_queue = (void*) malloc(sizeof(struct raw_data_bundle));
	JDEBUG_MEMORY(JDBG_MALLOC, handle->pcap_queue);
	memset(handle->pcap_queue, 0, sizeof(struct raw_data_queue));
	pthread_mutex_init(&(handle->pcap_queue->rdb_mutex), NULL);
	handle->pcap_queue->rdb_start = NULL;
	handle->pcap_queue->rdb_end = NULL;

	handle->ieee1588eq_queue = (void*) malloc(sizeof(struct raw_data_bundle));
	JDEBUG_MEMORY(JDBG_MALLOC, handle->ieee1588eq_queue);
	memset(handle->ieee1588eq_queue, 0, sizeof(struct raw_data_queue));
	pthread_mutex_init(&(handle->ieee1588eq_queue->rdb_mutex), NULL);
	handle->ieee1588eq_queue->rdb_start = NULL;
	handle->ieee1588eq_queue->rdb_end = NULL;

	/* Handle structure for per-server data, and generic stamp queue */
	struct bidir_algodata *algodata;
	algodata = malloc(sizeof *algodata);
	handle->algodata = (void*) algodata;	// enduring copy of ptr to algodata data
	init_stamp_queue(algodata);

	/* Initialize all servers to trusted. */
	handle->servertrust = 0;

	return (handle);
}


/* Allocate space, and initialize, `arrays' of structures for ns RADclocks. */
static void
init_mRADclocks(struct radclock_handle *handle, int ns)
{
	int s;
	struct bidir_algodata *algodata;

	handle->nservers = ns;	// just in case, should already be true
	
	/* RADclock data. Initialize all to zero then override some members */
	handle->rad_data = calloc(ns,sizeof(struct radclock_data));
	for (s=0; s<ns; s++) {
		handle->rad_data[s].phat 			= 1e-9;
		handle->rad_data[s].phat_local 	= 1e-9;
		handle->rad_data[s].status 		= STARAD_UNSYNC | STARAD_WARMUP;
	}

	/* Clock error bound. All members initialized to zero */
	handle->rad_error = calloc(ns,sizeof(struct radclock_error));

	/* NTP client data */
	handle->ntp_client = calloc(ns,sizeof(struct radclock_ntp_client));

	/* NTP server data */
	handle->ntp_server = calloc(ns,sizeof(struct radclock_ntp_server));
	for (s=0; s<ns; s++) {
		SNTP_SERVER(handle,s)->burst = NTP_BURST;	// burst at startup, like ntpd
		SNTP_SERVER(handle,s)->stratum = STRATUM_UNSPEC;
	}

	/* Initialize remaining members of algodata */
	algodata = handle->algodata;
	algodata->laststamp = calloc(ns,sizeof(struct stamp_t));
	algodata->output = calloc(ns,sizeof(struct bidir_algooutput));
	algodata->state = calloc(ns,sizeof(struct bidir_algostate));

	return;
}




static int
clock_init_live(struct radclock *clock, struct radclock_data *rad_data,
	struct radclock_error *rad_error)
{
	struct ffclock_estimate cest;
	struct radclock_data rd;
	struct radclock_error rde;
	int err;

	JDEBUG

	/* Detect the version of the kernel support we are running on */
	clock->kernel_version = found_ffwd_kernel_version();
	if (clock->kernel_version < 0) {
		verbose(LOG_ERR, "No Feed-Forward kernel support - cannot run live");
		return (1);
	}

	/* Establish the link allowing raddata <--> FFdata exchange */
	err = init_kernel_clock(clock);
	if (err)
		return (1);
	verbose(LOG_NOTICE, "Feed-Forward clock support initialised");


	/* Attempt to retrieve FFdata from the kernel
	 * Provide verbose checking of raddata <--> FFdata correspondance
	 * Use to overwrite the naive default set by radclock_create.
	 * ** Currently suppressed due to RTC reset complications making such initialization
	 *    dangerous.
	 */
	err = get_kernel_ffclock(clock, &cest);
	if (err != 1) {		// if this kernel supports FFdata getting

		if (err < 0) {
			verbose(LOG_ERR, "Failed to get initial ffclock data from kernel");
			return (1);
		}
		verbose(LOG_NOTICE, "Initial retrieve of FFclock data successful");
		if ( VERB_LEVEL > 1 )
			printout_FFdata(&cest);

		/* Sanity check warnings when FFdata not fully set */
		if ((cest.update_time.sec == 0) || (cest.period == 0)) {
			logger(RADLOG_WARNING, "kernel FFdata is uninitialized!");
			printout_FFdata(&cest);
		}

		/* Debug code: check FFdata <--> rad_data mapping inverts as expected */
		if ( VERB_LEVEL > 2 ) {
			fill_radclock_data(&cest, &rd);
			verbose(LOG_NOTICE, "  Conversion to raddata :");
			printout_raddata(&rd);
			fill_ffclock_estimate(&rd, &rde, &cest);
			verbose(LOG_NOTICE, "  Inversion back to FFdata :");
			printout_FFdata(&cest);
		}

		/* Override default rad_data value with current kernel version */
		verbose(LOG_NOTICE, "  NOT Setting central rad_data to mirror FFdata");
//		memcpy(&rad_data,&rd,sizeof(rd));
//		verbose(LOG_NOTICE, "  Setting central rad_data to mirror FFdata");
//		if ( VERB_LEVEL > 1 )
//			printout_raddata(rad_data);
	}

	err = radclock_init_vcounter_syscall(clock); // null if KV>1
	if (err)
		return (1);

	err = radclock_init_vcounter(clock);
	if (err < 0)
		return (1);


	/* Get information about FF status from kernel
	 * This is a hack, need to move to kern dep code, or just ultimately remove
	 */
	#define	MAX_SYSCLOCK_NAME_LEN 16
//	int inttopass, intold;
//	char currname[32];		// maps to 'old' input
//	char nametopass[32];
//	char nameavail[32];
//	size_t sn;
//	size_t si = sizeof inttopass;
//
//	verbose(LOG_NOTICE, "clock_init_live:  testing sysctl interface");

//	// Check current hardware counter
//	sn = sizeof(currname);
//	err = sysctlbyname("kern.timecounter.hardware", &currname[0], &sn, NULL, 0);
//	verbose(LOG_NOTICE, "\t Hardware counter: %s [sn = %u]", currname, strlen(currname));
//
//	strlcpy(nametopass,"HPET",32);
//	verbose(LOG_NOTICE, "\t Trying to change counter to %s", nametopass);
//	err = sysctlbyname("kern.timecounter.hardware", NULL, &sn, nametopass, strlen(nametopass));
//
//	// Check current hardware counter
//	sn = sizeof(currname);  	// reset, as on return sn is set to the actual length of currname
//	err = sysctlbyname("kern.timecounter.hardware", &currname[0], &sn, NULL, 0);
//	verbose(LOG_NOTICE, "\t Hardware counter: %s [sn = %u]", currname, strlen(currname));

	
//	err = sysctlbyname("kern.sysclock.ffclock.version", &intold, &si, NULL, 0);
//	verbose(LOG_NOTICE, "\t FF Kernel version: %d", intold);
//	intold = 5;
//	inttopass = 6;
//	verbose(LOG_NOTICE, "  FFcounter bypass state: (old,new) = (%d,%d)", intold, inttopass); // init values
//	err = sysctlbyname("kern.sysclock.ffclock.ffcounter_bypass", &intold, &si, NULL, 0); // get current from kern
//	verbose(LOG_NOTICE, "  FFcounter bypass state: (old,new) = (%d,%d)", intold, inttopass); // rtns intold =0
//	err = sysctlbyname("kern.sysclock.ffclock.ffcounter_bypass", NULL, &si, &inttopass, si); // set to this
//	err = sysctlbyname("kern.sysclock.ffclock.ffcounter_bypass", &intold, &si, NULL, 0); // get current
//	verbose(LOG_NOTICE, "  FFcounter bypass state: (old,new) = (%d,%d)", intold, inttopass); // was set as expected
//
	
/* Migrated this to ffkernel-fbsd.c to have KV protection */
//	sn = sizeof(nameavail);
//	err = sysctlbyname("kern.sysclock.available", &nameavail[0], &sn, NULL, 0);
//	verbose(LOG_NOTICE, "\t Available sysclocks: %s", nameavail);
//
//	/* Get the active sysclock */
//	sn = sizeof(currname);
//	err = sysctlbyname("kern.sysclock.active", &currname[0], &sn, NULL, 0);
//	verbose(LOG_NOTICE, "\t Active sysclock: %s [sn = %u]", currname, strlen(currname));

//	/* Change to feed-forward */
//	sn = sizeof(nametopass);
//	strlcpy(nametopass,"feed-forward",sn);  // userland doesnt understand SYSCLOCK_{FFWD,FBCK}
//	verbose(LOG_NOTICE, "\t Trying to change sysclock to: %s", nametopass);
//	err = sysctlbyname("kern.sysclock.active", NULL, NULL, nametopass, strlen(nametopass));
//
//	// Check afterward if has changed
//	sn = sizeof(currname);
//	memset(currname,0,sn); currname[0] = '\0';
//	err = sysctlbyname("kern.sysclock.active", &currname[0], &sn, NULL, 0);
//	verbose(LOG_NOTICE, "\t Active sysclock: %s [sn = %u]", currname, strlen(currname));

//	// Change it back !
//	sn = sizeof(currname);
//	strlcpy(nametopass,"feedback",sn);  // userland doesnt understand SYSCLOCK_{FFWD,FBCK}
//	verbose(LOG_NOTICE, "\t Trying to change sysclock to: %s", nametopass);
//	err = sysctlbyname("kern.sysclock.active", NULL, NULL, nametopass, strlen(nametopass));
//
//
//	// Check afterward if has changed
//	sn = sizeof(currname);
//	memset(currname,0,sn); currname[0] = '\0';
//	err = sysctlbyname("kern.sysclock.active", &currname[0], &sn, NULL, 0);
//	verbose(LOG_NOTICE, "\t Active sysclock: %s [sn = %u]", currname, strlen(currname));
//


	/* Get the value of the interface's timestamp default setting */
	// TODO: drop this once clear that if-level ts types won't be reappearing
	//       Only works on BSD anyway, will break under Linux
//	if (clock->kernel_version == 2) {
//		sn = sizeof(nameavail);
//		err = sysctlbyname("net.bpf.tscfg.default", &nameavail[0], &sn, NULL, 0);
//		verbose(LOG_NOTICE, "\t Timestamp default configuration for interfaces: %s", nameavail);
//	}
	
	/* Get the value of our interface's timestamp setting */
//	sn = sizeof(nameavail);
//	err = sysctlbyname("net.bpf.tscfg.em0", &nameavail[0], &sn, NULL, 0);
//	verbose(LOG_NOTICE, "\t em0's current configuration: %s",
//				clock_handle->conf->network_device, nameavail);	// using global handle!
	
	/* End sysctl verbosity hack */

	return (0);
}



/*
 * radclock process specific init of the clock_handle
 */
static int
init_handle(struct radclock_handle *handle)
{
	/* Input source */
	struct stampsource *stamp_source;
	int err, s;

	JDEBUG

	/* Clock has been init', set the pointer to the clock */
	set_verbose(handle, handle->conf->verbose_level, 1);
	set_logger(logger_verbose_bridge);

	if (handle->run_mode == RADCLOCK_SYNC_LIVE) {

		/* Initial status words */
		// TODO: more should be set here?  and in dead case?
		for (s=0; s < handle->nservers; s++) {
			ADD_STATUS(&handle->rad_data[s], STARAD_STARVING);
		}

		/* Initialise IPC shared memory segment */
		if (handle->conf->server_ipc == BOOL_ON) {
			err = sms_init_writer(handle->clock);
			if (err)
				return (1);
			verbose(LOG_NOTICE, "IPC Shared Memory ready");
		}
	}

	/* Open input file from which to read TS data */
	if (!VM_SLAVE(handle)) {
		stamp_source = create_source(handle);
		if (!stamp_source) {
			verbose(LOG_ERR, "Error creating stamp source, exiting");
			exit(EXIT_FAILURE);
		}

		/* Hang stamp source on the handler */
		handle->stamp_source = (void *) stamp_source;
	}

	/* Open output files */
	open_output_stamp(handle);
	open_output_matlab(handle);

	return (0);
}



int
start_live(struct radclock_handle *handle)
{
	/* Threads */
	void* thread_status;

	int have_fixed_point_thread = 0;
	int err;

	JDEBUG

	/*
	 * Handle first time run. If no time_server specified while we produce
	 * packets, we would be a nasty CPU hog. Better avoid creating problems and
	 * exit with an error message
	 */
	if ((handle->conf->synchro_type == SYNCTYPE_NTP) ||
			(handle->conf->synchro_type == SYNCTYPE_1588)) {
		if (strlen(handle->conf->time_server) == 0) {
			verbose(LOG_ERR, "No time server specified on command line "
					"or configuration file, attempting suicide.");
			return (1);
		}
	}

	/* TRIGGER
	 * This thread triggers the processing of data. It could be a dummy sleeping
	 * loop, an NTP client, a 1588 slave  ...
	 */
	if (!VM_SLAVE(handle)) {
		err = start_thread_TRIGGER(handle);
		if (err < 0)
			return (1);
	}

	/* PROC
	 * In the case of a restart following a SIGHUP triggering a rehash, PROC
	 * is still running, so simply clear the flag.
	 */
	if (handle->unix_signal == SIGHUP)
		handle->unix_signal = 0;
	else {
		if (VM_SLAVE(handle) || VM_MASTER(handle)) {
			err = init_vm(handle);
			if (err < 0){
				verbose(LOG_ERR, "Failed to initialise VM communication");
				return (1);
			}
		}
		if (!VM_SLAVE(handle)) {
			err = start_thread_DATA_PROC(handle);
			if (err < 0)
				return (1);
		}
	   
	}

	/* NTP_SERV */
	switch (handle->conf->server_ntp) {
	case BOOL_ON:
		err = start_thread_NTP_SERV(handle);
		if (err < 0)
			return (1);
		break;
	case BOOL_OFF:
	default:
		break;
	}

	/* VM_UDP_SERV */
	switch (handle->conf->server_vm_udp) {
	case BOOL_ON:
		err = start_thread_VM_UDP_SERV(handle);
		if (err < 0)
			return (1);
		break;
	case BOOL_OFF:
	default:
		/* do nothing */
		break;
	}


	/*
	 * To be able to provide the RADCLOCK timestamping mode, we need to refresh
	 * the fixed point data in the kernel.  That's this guy's job.
	 * XXX Update: with kernel version 2, the overflow problem is taking care of
	 * by the kernel. The fixedpoint thread is deprecated and should be removed
	 * in the future
	 */
	if ((handle->run_mode == RADCLOCK_SYNC_LIVE) &&
			(handle->clock->kernel_version < 3)) {
		err = start_thread_FIXEDPOINT(handle);
		if (err < 0)
			return (1);
		have_fixed_point_thread = 1;
	}
	else
		have_fixed_point_thread = 0;


	/*
	 * Main infinite capture loop, it does not return until the end of
	 * input or if we explicitely break it
	 * TODO: a unique source is assumed !!
	 */
	err = capture_raw_data(handle);



	if (err == -1) {
		handle->unix_signal = SIGTERM;	// some abuse here, not a true signal
		verbose(LOG_NOTICE, "Reached end of input");
	}
	if (err == -2 && handle->unix_signal == SIGHUP)
		verbose(LOG_NOTICE, "Breaking current capture loop for rehash");


	/*
	 * pcap_break_loop() has been called or end of input. In both cases kill the
	 * threads. If we rehash, they will be restarted anyway.
	 */
	verbose(LOG_NOTICE, "Send killing signal to threads. Wait for stop message.");
	handle->pthread_flag_stop = PTH_STOP_ALL;

	/* Do not stop PROC in HUP case (owns sync algo state) */
	if (handle->unix_signal == SIGHUP)
		handle->pthread_flag_stop &= ~PTH_DATA_PROC_STOP;

	if (handle->conf->server_ntp == BOOL_ON) {
		pthread_join(handle->threads[PTH_NTP_SERV], &thread_status);
		verbose(LOG_NOTICE, "NTP server thread is dead.");
	}

	pthread_join(handle->threads[PTH_TRIGGER], &thread_status);
	verbose(LOG_NOTICE, "Trigger thread is dead.");

	if (have_fixed_point_thread) {
		pthread_join(handle->threads[PTH_FIXEDPOINT], &thread_status);
		verbose(LOG_NOTICE, "Kernel fixedpoint thread is dead.");
	}
	
	/* Join on TERM since algo has been told to die */
	if (handle->unix_signal != SIGHUP) {
		pthread_join(handle->threads[PTH_DATA_PROC], &thread_status);
		verbose(LOG_NOTICE, "Data processing thread is dead.");
		/* Reinitialise flags */
		handle->pthread_flag_stop = 0;
		verbose(LOG_NOTICE, "Threads are dead.");
		return (1);
	}
	else
		handle->pthread_flag_stop = 0;

	return (0);
}






/*-------------------------------------------------------------------------*/
/********************************* main ************************************/
/*-------------------------------------------------------------------------*/

int
main(int argc, char *argv[])
{
	struct radclock_handle *handle;
	struct radclock_config *conf;
	int is_daemon = 0;

	/* File and command line reading */
	int ch;
	
	/* Mask variable used to know which parameter to update */
	uint32_t param_mask = 0;

	/* PID lock file for daemon */
	int daemon_pid_fd 		= 0;

	/* Initialize PID lockfile to a default value */
	const char *pid_lockfile = DAEMON_LOCK_FILE;

	/* Misc */
	int err;

	/* turn off buffering to allow results to be seen immediately if JDEBUG*/
	#ifdef WITH_JDEBUG
	setvbuf(stdout, (char *)NULL, _IONBF, 0);
	setvbuf(stderr, (char *)NULL, _IONBF, 0);
	#endif

	/*
	 * Register Signal handlers. We use sigaction() instead of signal() to catch
	 * signals. The main reason concerns the SIGHUP signal. In Linux, the
	 * syscalls are restarted as soon as the signal handler returns. This
	 * prevent pcap_breakloop() to do its job (see pcap man page). Using
	 * sigaction() we can overwrite the default flag to prevent this behavior
	 */
	sigset_t block_mask;
	sigfillset (&block_mask);
	struct sigaction sig_struct;


	sig_struct.sa_handler = signal_handler;
	sig_struct.sa_mask = block_mask;
	sig_struct.sa_flags = 0;

	sigaction(SIGHUP,  &sig_struct, NULL); /* hangup signal (1) */
	sigaction(SIGTERM, &sig_struct, NULL); /* software termination signal (15) */
	sigaction(SIGUSR1, &sig_struct, NULL); /* user signal 1 (30 on FreeBSD) */
	sigaction(SIGUSR2, &sig_struct, NULL); /* user signal 2 (31 on FreeBSD) */


	/* Initialise verbose data to defaults */
	verbose_data.handle = NULL;
	verbose_data.is_daemon = 0;
	verbose_data.is_initialized = 0;
	verbose_data.verbose_level = 0;
	verbose_data.fd = NULL;
	strcpy(verbose_data.logfile, "");
	pthread_mutex_init(&(verbose_data.vmutex), NULL);


	/* Management of configuration options */
	conf = (struct radclock_config *) malloc(sizeof(struct radclock_config));
	JDEBUG_MEMORY(JDBG_MALLOC, conf);
	memset(conf, 0, sizeof(struct radclock_config));

	/*
	 * The command line arguments are given the priority and override possible
	 * values of the configuration file But the configuration file is parsed
	 * after the command line because we need to know if we are running a daemon
	 * or not (configuration file is different if we run a daemon or not). Use
	 * the param_mask variable to indicate which values have to be updated from
	 * the config file
	 */

	/* Initialize the physical parameters, and other config parameters. */
	config_init(conf);

	/* Init the mask we use to signal configuration updates */
	param_mask = UPDMASK_NOUPD;

	/* Reading the command line arguments */
	while ((ch = getopt(argc, argv, "dxvhc:i:l:n:t:r:w:s:a:o:p:P:U:D:V")) != -1)
		switch (ch) {
		case 'x':
			SET_UPDATE(param_mask, UPDMASK_SERVER_IPC);
			conf->server_ipc = BOOL_OFF;
			SET_UPDATE(param_mask, UPDMASK_ADJUST_FFCLOCK);
			conf->adjust_FFclock = BOOL_OFF;
			SET_UPDATE(param_mask, UPDMASK_ADJUST_FBCLOCK);
			conf->adjust_FBclock = BOOL_OFF;
			break;
		case 'c':
			strcpy(conf->conffile, optarg);
			break;
		case 'd':
			is_daemon = 1;
			break;
		case 'l':
			strcpy(conf->logfile, optarg);
			break;
		case 'n':
			if (strlen(optarg) > MAXLINE) {
				fprintf(stdout, "ERROR: parameter too long\n");
				exit (1);
			}
			SET_UPDATE(param_mask, UPDMASK_HOSTNAME);
			strcpy(conf->hostname, optarg);
			break;
		case 'p':
			SET_UPDATE(param_mask, UPDMASK_POLLPERIOD);
			if ( atoi(optarg) < RAD_MINPOLL ) {
				conf->poll_period = RAD_MINPOLL;
				fprintf(stdout, "Warning: Poll period too small, set to %d\n",
					conf->poll_period);
			}
			else
				conf->poll_period = atoi(optarg);
			if ( conf->poll_period > RAD_MAXPOLL ) {
				conf->poll_period = RAD_MAXPOLL;
				fprintf(stdout, "Warning: Poll period too big, set to %d\n",
						conf->poll_period);
			}
			break;
		case 't':
			if (strlen(optarg) > MAXLINE) {
				fprintf(stdout, "ERROR: parameter too long\n");
				exit (1);
			}
			SET_UPDATE(param_mask, UPDMASK_TIME_SERVER);
			strcpy(conf->time_server, optarg);
			break;
		case 'i':
			if (strlen(optarg) > MAXLINE) {
				fprintf(stdout, "ERROR: parameter too long\n");
				exit (1);
			}
			SET_UPDATE(param_mask, UPDMASK_NETWORKDEV);
			strcpy(conf->network_device, optarg);
			break;
		case 'r':
			if (strlen(optarg) > MAXLINE) {
				fprintf(stdout, "ERROR: parameter too long\n");
				exit (1);
			}
			SET_UPDATE(param_mask, UPDMASK_SYNC_IN_PCAP);
			strcpy(conf->sync_in_pcap, optarg);
			break;
		case 'w':
			if (strlen(optarg) > MAXLINE) {
				fprintf(stdout, "ERROR: parameter too long\n");
				exit (1);
			}
			SET_UPDATE(param_mask, UPDMASK_SYNC_OUT_PCAP);
			strcpy(conf->sync_out_pcap, optarg);
			break;
		case 's':
			if (strlen(optarg) > MAXLINE) {
				fprintf(stdout, "ERROR: parameter too long\n");
				exit (1);
			}
			SET_UPDATE(param_mask, UPDMASK_SYNC_IN_ASCII);
			strcpy(conf->sync_in_ascii, optarg);
			break;
		case 'a':
			if (strlen(optarg) > MAXLINE) {
				fprintf(stdout, "ERROR: parameter too long\n");
				exit (1);
			}
			SET_UPDATE(param_mask, UPDMASK_SYNC_OUT_ASCII);
			strcpy(conf->sync_out_ascii, optarg);
			break;
		case 'o':
			if (strlen(optarg) > MAXLINE) {
				fprintf(stdout, "ERROR: parameter too long\n");
				exit (1);
			}
			SET_UPDATE(param_mask, UPDMASK_CLOCK_OUT_ASCII);
			strcpy(conf->clock_out_ascii, optarg);
			break;
		case 'P':
			if (strlen(optarg) > MAXLINE) {
				fprintf(stdout, "ERROR: parameter too long\n");
				exit (1);
			}
			SET_UPDATE(param_mask, UPDMASK_PID_FILE);
			pid_lockfile = optarg;
			break;
		case 'v':
			SET_UPDATE(param_mask, UPDMASK_VERBOSE);
			conf->verbose_level++;	// -vvv results in  verbose_level = 3
			break;
		case 'U':
			SET_UPDATE(param_mask, UPD_NTP_UPSTREAM_PORT);
			conf->ntp_upstream_port = atoi(optarg);
			break;
		case 'D':
			SET_UPDATE(param_mask, UPD_NTP_DOWNSTREAM_PORT);
			conf->ntp_downstream_port = atoi(optarg);
			break;
		case 'V':
			fprintf(stdout, "%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
			break;
		case 'h':
		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	/* Little hack to deal with parsing of long options in the command line */
	if (conf->verbose_level > 0)
		SET_UPDATE(param_mask, UPDMASK_VERBOSE);


	/* Create the radclock handle */
	clock_handle = create_handle(conf, is_daemon);
	if (!clock_handle) {
		verbose(LOG_ERR, "Could not create clock handle");
		return (-1);
	}
	handle = clock_handle;


	/*
	 * Have not parsed the config file yet, so will have to do it again since it
	 * may not be the right settings. Handles config parse messages in the right
	 * log file though. So far clock has not been sent to init, no syscall
	 * registered, pass a NULL pointer to verbose.
	 */
	set_verbose(handle, handle->conf->verbose_level, 0);
	set_logger(logger_verbose_bridge);
	
	/* Daemonize now, so that we can open the log files and close connection to
	 * stdin since we parsed the command line
	 */
	if (is_daemon) {
		struct stat sb;
		if (stat(RADCLOCK_RUN_DIRECTORY, &sb) < 0) {
			if (mkdir(RADCLOCK_RUN_DIRECTORY, 0755) < 0) {
				verbose(LOG_ERR, "Cannot create %s directory. Run as root or "
						"(!daemon && !server)", RADCLOCK_RUN_DIRECTORY);
				return (1);
			}
		}
		/* Check this everytime in case something happened */
		chmod(RADCLOCK_RUN_DIRECTORY, 00755);

		if (!(daemonize(pid_lockfile, &daemon_pid_fd))) {
			fprintf(stderr, "Error: did not manage to create the daemon\n");
			exit(EXIT_FAILURE);
		}
	}

	/*
	 * Retrieve configuration from the config file (write it down if it does not
	 * exist) That should be the only occasion when get_config() is called and
	 * the param_mask is not positioned to UPDMASK_NOUPD !!!  Only the
	 * parameters not specified on the command line are updated.
	 * TODO: in case file doesnt exist, handle->conf not updated to the values
	 *    written. Eg, Time server is left blank, requiring a 2nd run to pick up.
	 */
	if (!config_parse(handle->conf, &param_mask, is_daemon, &handle->nservers))
		return (0);

	/* Knowing the number of servers, create space for corresponding RADclocks */
	init_mRADclocks(handle, handle->nservers);

	
	/*
	 * Now that we have the configuration to use (verbose level), let's
	 * initialise the verbose level to the correct value
	 */
	set_verbose(handle, handle->conf->verbose_level, 0);
	set_logger(logger_verbose_bridge);

	/* Check for incompatible configurations and correct them */
	if (( handle->conf->synchro_type == SYNCTYPE_SPY ) ||
		( handle->conf->synchro_type == SYNCTYPE_PIGGY ))
	{
		if (handle->conf->server_ntp == BOOL_ON) {
			verbose(LOG_ERR, "Configuration error. Disabling NTP server "
					"(incompatible with spy or piggy mode).");
			handle->conf->server_ntp = BOOL_OFF;
		}
		if ( handle->conf->adjust_FBclock == BOOL_ON ) {
			verbose(LOG_ERR, "Configuration error. Disabling adjust FBclock "
					"(incompatible with spy or piggy mode).");
			handle->conf->adjust_FBclock = BOOL_OFF;
		}
	}
	
	/* Confirmation output of the final configuration used */
	config_print(LOG_NOTICE, handle->conf, handle->nservers);
	verbose(LOG_NOTICE, "Configuration parsing found %d servers", handle->nservers);

	/* Reinit the mask that counts updated values */
	param_mask = UPDMASK_NOUPD;


	// TODO extract extra checks from is_live_source and make an input fix 
	// function instead, would be clearer
	// TODO the conf->network_device business is way too messy


	/*
	 * Need to know if we are replaying data or not. If not, no need to create
	 * shared global data on the system or open a BPF. This define input to the
	 * init of the radclock handle
	 */
	if (!is_live_source(handle))
		handle->run_mode = RADCLOCK_SYNC_DEAD;
	else
		handle->run_mode = RADCLOCK_SYNC_LIVE;

	/* Setup kernel interactions: init clock handle and private data */
	if (handle->run_mode == RADCLOCK_SYNC_LIVE) {
		err = clock_init_live(handle->clock, RAD_DATA(handle), RAD_ERROR(handle));
		if (err) {
			verbose(LOG_ERR, "Could not initialise the RADclock");
			return (1);
		}
	}

	/* Create and initialize the source, create SMS */
	err = init_handle(handle);
	if (err) {
		verbose(LOG_ERR, "Radclock process specific init failed.");
		return (1);
	}

	/*
	 * Now 2 cases. Either we are running live or we are replaying some data.
	 * If we run live, we will spawn some threads and do some smart things.  If
	 * we replay data, no need to do all of that, we access data and process it
	 * in the same thread.
	 */
	if (handle->run_mode == RADCLOCK_SYNC_DEAD) {

		while (1) {
			err = process_stamp(handle);
			if (err < 0)
				break;
		}
	}
	/*s
	 * We loop in here in case we are rehashed. Threads are (re-)created every
	 * time we loop in
	 */
	else {
		while (err == 0) {
			err = start_live(handle);	// SIG{HUP,TERM} map to err={0,1}
			if (err == 0) {
				if (rehash_daemon(handle, param_mask))
					verbose(LOG_ERR, "SIGHUP - Failed to rehash daemon !!.");
			}
		}
	}

	/* These final stats based on the preferred clock only
	 * TODO: look into making the stats a separate structure. Could be much
	 *       easier to manage
	 */
	long int n_stamp;
	unsigned int ref_count;
	n_stamp = OUTPUT(handle, n_stamps);
	ref_count = ((struct stampsource*)(handle->stamp_source))->ntp_stats.ref_count;
	verbose(LOG_NOTICE, "%u NTP packets captured", ref_count);
	verbose(LOG_NOTICE,"%ld missed NTP packets", ref_count - 2 * n_stamp);
	verbose(LOG_NOTICE, "%ld valid timestamp tuples extracted", n_stamp);


	/* Close output files */
	close_output_stamp(handle);

	/* Print out last good phat value */
	verbose(LOG_NOTICE, "Last estimate of the clock source period: %12.10lg",
			RAD_DATA(handle)->phat);

	/* Say bye and close syslog */
	verbose(LOG_NOTICE, "RADclock stopped");
	if (is_daemon)
		closelog ();
	unset_verbose();

	/* Free the lock file */
	if (is_daemon) {
		write(daemon_pid_fd, "", 0);
		lockf(daemon_pid_fd, F_ULOCK, 0);
	}

	// TODO:  all the destructors have to be re-written
	destroy_source(handle, (struct stampsource *)(handle->stamp_source));


	/* Clear thread stuff */
	pthread_mutex_destroy(&(handle->globaldata_mutex));
	pthread_mutex_destroy(&(handle->wakeup_mutex));
	pthread_cond_destroy(&(handle->wakeup_cond));

	/* Detach IPC shared memory if were running as IPC server. */
	if (handle->conf->server_ipc == BOOL_ON)
		sms_detach(handle->clock);

	/* Free the clock handle members and itself. */
	free(handle->conf->time_server);
	free(handle->rad_data);
	free(handle->rad_error);
	free(handle->ntp_client);
	free(handle->ntp_server);
	pthread_mutex_destroy(&(handle->pcap_queue->rdb_mutex));
	pthread_mutex_destroy(&(handle->ieee1588eq_queue->rdb_mutex));
	free(handle->pcap_queue);
	free(handle->ieee1588eq_queue);
	free(((struct bidir_algodata*)handle->algodata)->laststamp);
	free(((struct bidir_algodata*)handle->algodata)->output);
	free(((struct bidir_algodata*)handle->algodata)->state);
	destroy_stamp_queue((struct bidir_algodata*)handle->algodata);
	free(handle);
	handle = NULL;
	clock_handle = NULL;

	exit(EXIT_SUCCESS);
}


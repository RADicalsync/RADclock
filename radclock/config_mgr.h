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

#ifndef _CONFIG_MGR_H
#define _CONFIG_MGR_H

#include "radclock-private.h"


#define DAEMON_CONFIG_FILE    "/etc/radclock.conf"
#define BIN_CONFIG_FILE       "radclock.conf"


#define	RAD_MINPOLL 1      // min poll interval [s]
#define	RAD_MAXPOLL 1024   // max poll interval [s]

/* Global setting for use of plocal refinement for in-daemon timestamping */
//# define PLOCAL_ACTIVE  1   //  {0,1} = {inactive, active}


/* Define max size for command line and configuration file parameters */
#define MAXLINE 250


/*
 * Trigger / Sync Protocol configuration
 */
#define SYNCTYPE_SPY    0
#define SYNCTYPE_PIGGY  1
#define SYNCTYPE_NTP    2
#define SYNCTYPE_1588   3
#define SYNCTYPE_PPS    4
#define SYNCTYPE_VM_UDP 5
#define SYNCTYPE_XEN    6
#define SYNCTYPE_VMWARE 7

/*
 * Server Protocol configuration
 */
#define BOOL_OFF 0
#define BOOL_ON  1


/*
 * Virtual Machine environmnent
 */
#define VM_SLAVE(val) ((val->conf->synchro_type == SYNCTYPE_VM_UDP) || \
	    (val->conf->synchro_type == SYNCTYPE_XEN) || \
	    (val->conf->synchro_type == SYNCTYPE_VMWARE))

#define VM_MASTER(val) ((val->conf->server_vm_udp == BOOL_ON) || \
	    (val->conf->server_xen == BOOL_ON) || \
	    (val->conf->server_vmware == BOOL_ON))

/* 
 * Default configuration values 
 */
#define DEFAULT_VERBOSE          1
#define DEFAULT_SYNCHRO_TYPE     SYNCTYPE_NTP  // Protocol used
#define DEFAULT_SERVER_IPC       BOOL_ON       // Update the clock
#define DEFAULT_IS_OCN           BOOL_OFF    // Defines if this server is an OCN
#define DEFAULT_IS_CN            BOOL_OFF    // Defines if this server is an CN
#define DEFAULT_SERVER_TELEMETRY BOOL_OFF    // Creates telemetry cache files in /radclock
#define DEFAULT_SERVER_SHM       BOOL_OFF    // Defines if SHM is active
#define DEFAULT_SERVER_NTP       BOOL_OFF    // Don't act as a server
#define DEFAULT_SERVER_VM_UDP    BOOL_OFF    // Don't Start VM servers
#define DEFAULT_SERVER_XEN       BOOL_OFF
#define DEFAULT_SERVER_VMWARE    BOOL_OFF
#define DEFAULT_ADJUST_FFCLOCK   BOOL_ON       // Normally a FFclock daemon !
#define DEFAULT_ADJUST_FBCLOCK   BOOL_OFF      // Not normally a FBclock daemon
#define DEFAULT_NTP_POLL_PERIOD  16            // 16 NTP pkts every [s]
#define DEFAULT_PHAT_INIT        1.e-9
#define CONFIG_PLOCAL_QUALITY    36
#define DEFAULT_PATH_SCALE       (5*3600)      // [s] in conf, but EXCluded from conf file
#define DEFAULT_ASYM_HOST        0.0           // [mus]
#define DEFAULT_ASYM_NET         0.0           // [mus]
#define DEFAULT_HOSTNAME         "platypus2.tklab.feit.uts.edu.au"
#define DEFAULT_TIME_SERVER      "tock.une.edu.au"
#define DEFAULT_NETWORKDEV       "em0"
#define DEFAULT_SYNC_IN_PCAP     "/etc/sync_input.pcap"
#define DEFAULT_SYNC_IN_ASCII    "/etc/sync_input.ascii"
#define DEFAULT_SYNC_OUT_PCAP    "/etc/sync_output.pcap"
#define DEFAULT_SYNC_OUT_ASCII   "/etc/sync_output.ascii"
#define DEFAULT_CLOCK_OUT_ASCII  "/etc/clock_output.ascii"
#define DEFAULT_SHM_DAG_CLIENT   "10.0.0.3"
#define DEFAULT_VM_UDP_LIST      "vm_udp_list"
#define DEFAULT_PUBLIC_NTP       BOOL_OFF
#define DEFAULT_PUBLIC_NTP_LIMIT  50  // public response limit over a DEFAULT_PUBLIC_NTP_PERIOD
#define DEFAULT_PUBLIC_NTP_PERIOD 10  // window for public response measurement [sec]
#define DEFAULT_PUBLIC_NTP_HASH_BUCKETS 100 // # hash buckets splitting the IP range
// If only 1 bad IP, 1/PUBLIC_NTP_HASH_BUCKETS of the IP range would be rejected

/*
 *  Definition of keys for configuration file keywords
 */
#define CONFIG_UNKNOWN          0
#define CONFIG_RADCLOCK_VERSION 1
/* Generic stuff */
#define CONFIG_VERBOSE         10
#define CONFIG_SERVER_IPC      11
#define CONFIG_SERVER_TELEMETRY 12
//#define CONFIG_          13
#define CONFIG_SYNCHRO_TYPE    13
#define CONFIG_SERVER_NTP      14
#define CONFIG_ADJUST_FFCLOCK  15
#define CONFIG_ADJUST_FBCLOCK  16
#define CONFIG_SERVER_SHM      17
#define CONFIG_SHM_DAG_CLIENT  18
/* Clock parameters */
#define CONFIG_POLLPERIOD      20
//#define CONFIG_            21
#define CONFIG_PHAT_INIT       22
#define CONFIG_ASYM_HOST       23
#define CONFIG_ASYM_NET        24
/* Environment */
#define CONFIG_TEMPQUALITY     30
#define CONFIG_TSLIMIT         31
#define CONFIG_SKM_SCALE       32
#define CONFIG_RATE_ERR_BOUND  33
#define CONFIG_BEST_SKM_RATE   34
#define CONFIG_OFFSET_RATIO    35
#define CONFIG_PLOCAL_QUALITY  36
/* Network Level */
#define CONFIG_HOSTNAME        40
#define CONFIG_TIME_SERVER     41
#define CONFIG_NTC             42
/* I/O definitions */
#define CONFIG_NETWORKDEV      50
#define CONFIG_SYNC_IN_PCAP    51
#define CONFIG_SYNC_IN_ASCII   52
#define CONFIG_SYNC_OUT_PCAP   53
#define CONFIG_SYNC_OUT_ASCII  54
#define CONFIG_CLOCK_OUT_ASCII 55
/* Virtual Machine stuff */
#define CONFIG_SERVER_VM_UDP   60
#define CONFIG_SERVER_XEN      61
#define CONFIG_SERVER_VMWARE   62
#define CONFIG_VM_UDP_LIST     63
/* radclock type */
#define CONFIG_IS_OCN          70
#define CONFIG_IS_CN           71
/* NTP serving type */
#define CONFIG_PUBLIC_NTP      80


/*
 * Pre-defined description of temperature environment quality
 * CONFIG_QUALITY_UNKWN has to be defined with the highest values to parse
 * the config file correctly
 */
#define CONFIG_QUALITY_POOR    0
#define CONFIG_QUALITY_GOOD    1
#define CONFIG_QUALITY_EXCEL   2
#define CONFIG_QUALITY_UNKWN   3


/*
 * Masks to reload the configuration parameters
 */
#define UPDMASK_NOUPD           0x0000000
#define UPDMASK_POLLPERIOD      0x0000001
//#define UPDMASK_              0x0000002
#define UPDMASK_TEMPQUALITY     0x0000004
#define UPDMASK_ASYM_HOST       0x0000008
#define UPDMASK_ASYM_NET        0x0000010
#define UPDMASK_SERVER_IPC      0x0000020
#define UPDMASK_SYNCHRO_TYPE    0x0000040
#define UPDMASK_SERVER_NTP      0x0000080
#define UPDMASK_ADJUST_FFCLOCK  0x0000100
#define UPDMASK_ADJUST_FBCLOCK  0x0000200
#define UPDMASK_HOSTNAME        0x0000400
#define UPDMASK_TIME_SERVER     0x0000800
#define UPDMASK_VERBOSE         0x0001000
#define UPDMASK_NETWORKDEV      0x0002000
#define UPDMASK_SYNC_IN_PCAP    0x0004000
#define UPDMASK_SYNC_IN_ASCII   0x0008000
#define UPDMASK_SYNC_OUT_PCAP   0x0010000
#define UPDMASK_SYNC_OUT_ASCII  0x0020000
#define UPDMASK_CLOCK_OUT_ASCII 0x0040000
#define UPDMASK_SERVER_VM_UDP   0x0080000
#define UPDMASK_SERVER_XEN      0x0100000
#define UPDMASK_SERVER_VMWARE   0x0200000
#define UPDMASK_VM_UDP_LIST     0x0400000
#define UPDMASK_PID_FILE        0x0800000
#define UPD_NTP_UPSTREAM_PORT   0x1000000
#define UPD_NTP_DOWNSTREAM_PORT 0x2000000
#define UPDMASK_SERVER_TELEMETRY 0x4000000
#define UPDMASK_NTC             0x8000000
#define UPDMASK_SERVER_SHM      0x20000000
#define UPDMASK_SHM_DAG_CLIENT  0x40000000
#define UPDMASK_IS_OCN          0x80000000
#define UPDMASK_IS_CN           0x100000000
#define UPDMASK_PUBLIC_NTP      0x200000000

#define HAS_UPDATE(val,mask)   ((val & mask) == mask)
#define SET_UPDATE(val,mask)   (val |= mask)
#define CLEAR_UPDATE(val,mask) (val &= ~mask)


/* NTC Command&Control assigns NTC nodes unique ids for NTC-global consistency.
 * The NTC_id are allocated as follows:
 *  CN:  0
 *  ICN: 1,2,...  ICN_MAX   (currently ICN_MAX = MAX_NTC/2 -1)
 *  OCN: ICN_MAX+1 ... MAX_NTC
 *  non-NTC servers or uninitialized:  -1
 * If #nodes>64 the implementation based on uint64_t and refid abuse will break.
 *
 * In `flag form` status words, the (s+1)-th LSB corresponds to ntc_id=s .
 * The ICN_MASK is used to null the bits in ntc_id status words
 * corresponding to the OCNs (used in in-band signalling ICN status to OCNs)
 */
#define MAX_NTC 32
#define ICN_MASK (~(~0ULL << MAX_NTC/2))	// 000..0011...111  null OCN bits
/* Convert the NTC_id of an OCN into a `OCN_id' starting at 1, or -1 if not an OCN */
#define OCN_ID(h) (h >= MAX_NTC/2 ? h - MAX_NTC/2 + 1 : -1 )
struct NTC_Config {
	int 	id;
	char 	domain[MAXLINE];
};


/* Global structure used to keep track of the config parameters Mostly
 * used by signal handlers The fields correspond to the parameters
 * of the get_config function.
 */
struct radclock_config {
	u_int32_t mask;                    // Update param mask
	char conffile[MAXLINE];            // Configuration file path
	char logfile[MAXLINE];             // Log file path
	char radclock_version[MAXLINE];    // Package version id
	int verbose_level;                 // debug output level
	int poll_period;                   // period of NTP pkt sending [s]
	struct bidir_metaparam metaparam;  // Physical characteristics
	int synchro_type;                  // multi-choice depending on client-side protocol
	int server_ipc;                    // Boolean
	int server_telemetry;              // Boolean
	int server_shm;                    // Boolean
	int server_ntp;                    // Boolean
	int server_vm_udp;                 // Boolean
	int server_xen;                    // Boolean
	int server_vmware;                 // Boolean
	int is_ocn;                        // Boolean
	int is_cn;                         // Boolean
	int public_ntp;                    // Boolean - whether radclock responds to public NTP requests
	int adjust_FFclock;                // Boolean
	int adjust_FBclock;                // Boolean
	double phat_init;                  // Initial value for phat
	double asym_host;                  // Host asymmetry estimate [s]
	double asym_net;                   // Network asymmetry estimate [s]
	int ntp_upstream_port;             // NTP Upstream port
	int ntp_downstream_port;           // NTP Downstream port
	char hostname[MAXLINE];            // Client hostname
	char *time_server;                 // Server names, concatenated in MAXLINE blocks
	int *time_server_ntc_mapping;      // Maps timer_server id to NTC ids, non NTC servers get -1
	int *time_server_ntc_indexes;      // Lists the NTC server indexes relative to the time_server ordering.
	    // Eg given time_servers A B C. If A and C are NTC servers then this would be 0,2
	int time_server_ntc_count;         // The number of time_servers that are NTC servers
	char network_device[MAXLINE];      // physical device string, eg xl0, eth0
	char sync_in_pcap[MAXLINE];        // read from stored instead of live input
	char sync_in_ascii[MAXLINE];       // input is a preprocessed stamp file
	char sync_out_pcap[MAXLINE];       // raw packet Output file name
	char sync_out_ascii[MAXLINE];      // output processed stamp file
	char clock_out_ascii[MAXLINE];     // output matlab requirements
	char vm_udp_list[MAXLINE];         // File containing list of udp VM's
	char shm_dag_client[MAXLINE];      // IP address of SHM DAG client
	struct NTC_Config ntc[MAX_NTC];    // NTC definition
};



/* Initialise the configuration of the radclock daemon */
void config_init(struct radclock_config *conf);

/* Parse a configuration file */
int config_parse(struct radclock_config *conf, u_int32_t *mask, int is_daemon, int *ns);

/* Reads config file line by line, retrieve (key,value) and update global data.
 * Only parses select inforation from config. Doesn't attempt to reread all.
 * This allows avoids the need to shutdown and re-setup running threads.
 * This should be allowed to run without interferring with live radclock running threads
 */
int light_config_parse(struct radclock_config *conf, u_int32_t *mask, int is_daemon, int *ns);

/* Output the config in config to verbose using level */
void config_print(int level, struct radclock_config *conf, int ns);


#endif

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

#include <arpa/inet.h>

#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <syslog.h>
#include <sys/stat.h>

#include "../config.h"
#include "radclock.h"
#include "radclock-private.h"

#include "radclock_daemon.h"
#include "proto_ntp.h"
#include "sync_history.h"
#include "sync_algo.h"
#include "verbose.h"
#include "config_mgr.h"
#include "jdebug.h"



/* The configuration file lines follow the template : 
 * key = value
 */


/** Basic structure definition containing label and corresponding index */
struct _key {
	const char *label;
	int keytype;
};



/* Definition of the keys used in the conf file */
static struct _key keys[] = {
	{ "radclock_version",		CONFIG_RADCLOCK_VERSION},
	{ "verbose_level",			CONFIG_VERBOSE},
	{ "synchronization_type",	CONFIG_SYNCHRO_TYPE},
	{ "ipc_server",				CONFIG_SERVER_IPC},
	{ "telemetry_server",		CONFIG_SERVER_TELEMETRY},
	{ "shm_server",				CONFIG_SERVER_SHM},
	{ "shm_dag_client",			CONFIG_SHM_DAG_CLIENT},
	{ "ntp_server",				CONFIG_SERVER_NTP},
	{ "public_ntp",				CONFIG_PUBLIC_NTP},
	{ "vm_udp_server",			CONFIG_SERVER_VM_UDP},
	{ "xen_server",				CONFIG_SERVER_XEN},
	{ "vmware_server",			CONFIG_SERVER_VMWARE},
	{ "adjust_FFclock",			CONFIG_ADJUST_FFCLOCK},
	{ "adjust_FBclock",			CONFIG_ADJUST_FBCLOCK},
	{ "polling_period",			CONFIG_POLLPERIOD},
	{ "temperature_quality", 	CONFIG_TEMPQUALITY},
	{ "ts_limit",				CONFIG_TSLIMIT},
	{ "skm_scale",				CONFIG_SKM_SCALE},
	{ "rate_error_bound",		CONFIG_RATE_ERR_BOUND},
	{ "best_skm_rate",			CONFIG_BEST_SKM_RATE},
	{ "offset_ratio",			CONFIG_OFFSET_RATIO},
	{ "plocal_quality",			CONFIG_PLOCAL_QUALITY},
	{ "init_period_estimate",	CONFIG_PHAT_INIT},
	{ "host_asymmetry",			CONFIG_ASYM_HOST},
	{ "network_asymmetry",		CONFIG_ASYM_NET},
	{ "hostname",				CONFIG_HOSTNAME},
	{ "time_server",			CONFIG_TIME_SERVER},
	{ "network_device",			CONFIG_NETWORKDEV},
	{ "sync_input_pcap",		CONFIG_SYNC_IN_PCAP},
	{ "sync_input_ascii",		CONFIG_SYNC_IN_ASCII},
	{ "sync_output_pcap",		CONFIG_SYNC_OUT_PCAP},
	{ "sync_output_ascii",		CONFIG_SYNC_OUT_ASCII},
	{ "clock_output_ascii",		CONFIG_CLOCK_OUT_ASCII},
	{ "vm_udp_list",			CONFIG_VM_UDP_LIST},
	{ "ntc",					CONFIG_NTC},
	{ "is_ocn",					CONFIG_IS_OCN},
	{ "is_tn",					CONFIG_IS_TN},
	{ "",			 			CONFIG_UNKNOWN} // Must be the last one
};

/* Definition of the options labels
 * Order matters !
 * TODO make this a bit more robust to bugs with enums already partly defined
 */
static char* labels_bool[] = { "off", "on" };
static char* labels_verb[] = { "quiet", "normal", "high" };
static char* labels_sync[] = { "spy", "piggy", "ntp", "ieee1588", "pps",
	"vm_udp", "xen", "vmware" };



/** Modes for the quality of the temperature environment 
 * Must be defined in the same sequence order as the CONFIG_QUALITY_*  values
 */
static struct _key temp_quality[] = {
	{ "poor",      CONFIG_QUALITY_POOR},
	{ "good",      CONFIG_QUALITY_GOOD},
	{ "excellent", CONFIG_QUALITY_EXCEL},
	{ "",          CONFIG_QUALITY_UNKWN}  // Must be the last one
};



/*
 * Initialise the structure of global data to default values.  Avoid weird
 * configuration and a basis to generate a conf file if it does not exist.
 */
void
config_init(struct radclock_config *conf)
{
	JDEBUG

	/* Init the mask for parameters */
	conf->mask = UPDMASK_NOUPD;

	/* Runnning defaults */
	strcpy(conf->conffile, "");
	strcpy(conf->logfile, "");
	strcpy(conf->radclock_version, PACKAGE_VERSION);
	conf->server_ipc        = DEFAULT_SERVER_IPC;
	conf->server_telemetry  = DEFAULT_SERVER_TELEMETRY;
	conf->synchro_type      = DEFAULT_SYNCHRO_TYPE;
	conf->server_ntp        = DEFAULT_SERVER_NTP;
	conf->adjust_FFclock    = DEFAULT_ADJUST_FFCLOCK;
	conf->adjust_FBclock    = DEFAULT_ADJUST_FBCLOCK;
	conf->server_shm        = DEFAULT_SERVER_SHM;
	conf->public_ntp        = DEFAULT_PUBLIC_NTP;

	strcpy(conf->shm_dag_client, "");

	
	
	/* Virtual Machine */
	conf->server_vm_udp     = DEFAULT_SERVER_VM_UDP;
	conf->server_xen        = DEFAULT_SERVER_XEN;
	conf->server_vmware     = DEFAULT_SERVER_VMWARE;

	/* Clock parameters */
	conf->poll_period              = DEFAULT_NTP_POLL_PERIOD;
	conf->metaparam.TSLIMIT        = TS_LIMIT_GOOD;
	conf->metaparam.SKM_SCALE      = SKM_SCALE_GOOD;
	conf->metaparam.RateErrBOUND   = RATE_ERR_BOUND_GOOD;
	conf->metaparam.BestSKMrate    = BEST_SKM_RATE_GOOD;
	conf->metaparam.offset_ratio   = OFFSET_RATIO_GOOD;
	conf->metaparam.plocal_quality = PLOCAL_QUALITY_GOOD;
	conf->metaparam.path_scale     = DEFAULT_PATH_SCALE;  // in conf, but EXCluded from conf file
	conf->metaparam.relasym_bound_global = DEFAULT_RELASYM_BOUND_GLOBAL; // "
	conf->phat_init                = DEFAULT_PHAT_INIT;
	conf->asym_host                = DEFAULT_ASYM_HOST;
	conf->asym_net                 = DEFAULT_ASYM_NET;

	/* Network level */
	strcpy(conf->hostname, "");
	conf->time_server = calloc(1,MAXLINE);  // always need at least one string
	conf->ntp_upstream_port	  = DEFAULT_NTP_PORT;
	conf->ntp_downstream_port = DEFAULT_NTP_PORT;

	/* Input/Output files and devices. Must set to empty string to not confuse
	 * anything. Only one option can be specified at a time (either from the
	 * conf file or the command line.
	 */
	strcpy(conf->network_device, "");
	strcpy(conf->sync_in_pcap, "");
	strcpy(conf->sync_in_ascii, "");
	strcpy(conf->sync_out_pcap, "");
	strcpy(conf->sync_out_ascii, "");
	strcpy(conf->clock_out_ascii, "");
	strcpy(conf->vm_udp_list, "");

	conf->time_server_ntc_mapping = 0; // Flag mapping as uninitialised
	conf->time_server_ntc_count = 0;

	// Clear all NTC server data
	for (int i =0; i < MAX_NTC; i++)
	{
		conf->ntc[i].id = -1;
		conf->ntc[i].domain[0] = 0;
	}

}



/** For a given key index, lookup for the corresponding label */
const char* find_key_label(struct _key *keys, int codekey) 
{
	for (;;) {	
		if (keys->keytype == CONFIG_UNKNOWN) {
			verbose(LOG_ERR, "Did not find a key while creating the "
					"configuration file.");
			return NULL;
		}
		if (keys->keytype == codekey) {
			return keys->label;
		}
		keys++;	
	}
	return NULL;
}


int get_temperature_config(struct radclock_config *conf)
{
	if ( (conf->metaparam.TSLIMIT == TS_LIMIT_POOR)
	  && (conf->metaparam.RateErrBOUND == RATE_ERR_BOUND_POOR)
	  && (conf->metaparam.SKM_SCALE == SKM_SCALE_POOR)
	  && (conf->metaparam.BestSKMrate == BEST_SKM_RATE_POOR)
	  && (conf->metaparam.offset_ratio == OFFSET_RATIO_POOR)
	  && (conf->metaparam.plocal_quality == PLOCAL_QUALITY_POOR))
			return CONFIG_QUALITY_POOR;

	else if ( (conf->metaparam.TSLIMIT == TS_LIMIT_GOOD)
	  && (conf->metaparam.RateErrBOUND == RATE_ERR_BOUND_GOOD)
	  && (conf->metaparam.SKM_SCALE == SKM_SCALE_GOOD)
	  && (conf->metaparam.BestSKMrate == BEST_SKM_RATE_GOOD)
	  && (conf->metaparam.offset_ratio == OFFSET_RATIO_GOOD)
	  && (conf->metaparam.plocal_quality == PLOCAL_QUALITY_GOOD))
			return CONFIG_QUALITY_GOOD;

	else if ( (conf->metaparam.TSLIMIT == TS_LIMIT_EXCEL)
	  && (conf->metaparam.RateErrBOUND == RATE_ERR_BOUND_EXCEL)
	  && (conf->metaparam.SKM_SCALE == SKM_SCALE_EXCEL)
	  && (conf->metaparam.BestSKMrate == BEST_SKM_RATE_EXCEL)
	  && (conf->metaparam.offset_ratio == OFFSET_RATIO_EXCEL)
	  && (conf->metaparam.plocal_quality == PLOCAL_QUALITY_EXCEL))
			return CONFIG_QUALITY_EXCEL;

	else	
			return CONFIG_QUALITY_UNKWN;
}


/* Write a default configuration file
 * If at least one time_server is already configured, it is necessary to
 * pass in the number  ns  of them.
 */
void
write_config_file(FILE *fd, struct _key *keys, struct radclock_config *conf, int ns)
{

	fprintf(fd, "##\n");
	fprintf(fd, "## This is the default configuration file for the RADclock\n");
	fprintf(fd, "##\n\n");

	/* PACKAGE_VERSION defined with autoconf tools */
	fprintf(fd, "#----------------------------------------------------------------------------#\n");
	fprintf(fd, "# Package version. Do not modify this line.\n");
	fprintf(fd, "%s = %s\n", find_key_label(keys, CONFIG_RADCLOCK_VERSION), PACKAGE_VERSION);
	fprintf(fd, "#----------------------------------------------------------------------------#\n");
	fprintf(fd, "\n\n");

	/* Verbose */
	fprintf(fd, "# Verbosity level of the radclock daemon.\n");
	fprintf(fd, "#\tquiet : only errors and warnings are logged\n");
	fprintf(fd, "#\tnormal: adaptive logging of events\n");
	fprintf(fd, "#\thigh  : include debug messages (is very high, expert use)\n");
	if (conf == NULL)
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_VERBOSE), labels_verb[DEFAULT_VERBOSE]);
	else
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_VERBOSE), labels_verb[conf->verbose_level]);


	fprintf(fd, "\n\n\n");
	fprintf(fd, "#----------------------------------------------------------------------------#\n");
	fprintf(fd, "# Synchronization Client parameters\n");
	fprintf(fd, "#----------------------------------------------------------------------------#\n");
	fprintf(fd, "\n");


	/* Specify the type of synchronization to use*/
	fprintf(fd, "# Specify the type of underlying synchronization used.\n"
			"# Note that piggybacking requires an ntp daemon running and is then\n"
			"# incompatible with the RADclock serving clients over the network or \n"
			"# adjusting the system FBclock. Piggybacking disables these functions.\n");
	fprintf(fd, "#\tpiggy   : piggybacking on running ntp daemon\n");
	fprintf(fd, "#\tntp     : RADclock uses NTP protocol\n");
	fprintf(fd, "#\tieee1588: RADclock uses IEEE 1588 protocol - NOT IMPLEMENTED YET\n");
	fprintf(fd, "#\tpps     : RADclock listens to PPS - NOT IMPLEMENTED YET\n");
	fprintf(fd, "#\tvm_udp  : RADclock communicates with master radclock for "
			"clock data over udp\n");
	fprintf(fd, "#\txen     : RADclock communicates with master radclock for "
			"clock data over xenstore\n");
	fprintf(fd, "#\tvmware  : RADclock communicates with master radclock for "
			"clock data over vmci\n");
	if (conf == NULL)
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SYNCHRO_TYPE),
				labels_sync[DEFAULT_SYNCHRO_TYPE]);
	else
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SYNCHRO_TYPE),
				labels_sync[conf->synchro_type]);


	/* Poll Period */
	fprintf(fd, "# The polling period specifies the time interval at which\n");
	fprintf(fd, "# the requests for time are sent to the server (value in seconds)\n");
	if (conf == NULL)
		fprintf(fd, "%s = %d\n\n", find_key_label(keys, CONFIG_POLLPERIOD), DEFAULT_NTP_POLL_PERIOD);
	else
		fprintf(fd, "%s = %d\n\n", find_key_label(keys, CONFIG_POLLPERIOD), conf->poll_period);


	/* Hostname */
	fprintf(fd, "# Hostname or IP address (uses lookup name resolution).\n");
	fprintf(fd, "# Automatic detection will be attempted if not specified.\n");
	if ( (conf) && (strlen(conf->hostname) > 0) )
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_HOSTNAME), conf->hostname);
	else
		fprintf(fd, "#%s = %s\n\n", find_key_label(keys, CONFIG_HOSTNAME), DEFAULT_HOSTNAME);


	/* NTP server */
	fprintf(fd, "# Time server answering the requests from this client.\n");
	fprintf(fd, "# Can be a host name or an IP address (uses lookup name resolution).\n");
	if ( (conf) && (strlen(conf->time_server) > 0) )
		for (int s=0; s<ns; s++)
			fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_TIME_SERVER),
				conf->time_server + s*MAXLINE );
	else
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_TIME_SERVER), DEFAULT_TIME_SERVER);


	fprintf(fd, "\n\n\n");
	fprintf(fd, "#----------------------------------------------------------------------------#\n");
	fprintf(fd, "# Synchronization Server parameters\n");
	fprintf(fd, "#----------------------------------------------------------------------------#\n");
	fprintf(fd, "\n");


	/* Serve other processes and kernel update */
	fprintf(fd, "# IPC server.\n");
	fprintf(fd, "# Serves time to the Shared Memory Segment for use by user radclocks.\n");
	fprintf(fd, "#\ton : Start service - makes RADclock available to other programs\n");
	fprintf(fd, "#\toff: Stop service  - useful when replaying traces\n");
	if (conf == NULL)
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SERVER_IPC),
				labels_bool[DEFAULT_SERVER_IPC]);
	else
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SERVER_IPC),
				labels_bool[conf->server_ipc]);


	/* Saves telemetry data to disk /radclock */
	fprintf(fd, "# Telemetry server.\n");
	fprintf(fd, "# Saves telemetry data to /radclock .\n");
	fprintf(fd, "#\ton : Start service - Generates telemetry data\n");
	fprintf(fd, "#\toff: Stop service  - No Telemetry Data\n");
	if (conf == NULL)
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SERVER_TELEMETRY),
				labels_bool[DEFAULT_SERVER_TELEMETRY]);
	else
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SERVER_TELEMETRY),
				labels_bool[conf->server_telemetry]);

	/* Defines if this server is serving public NTP requests */
	fprintf(fd, "# Public NTP server.\n");
	fprintf(fd, "# Defines if this server is serving public NTP requests .\n");
	fprintf(fd, "#\ton : Start service - Replies to public NTP requests\n");
	fprintf(fd, "#\toff: Stop service  - Refuses public NTP requests\n");
	if (conf == NULL)
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_PUBLIC_NTP),
				labels_bool[DEFAULT_PUBLIC_NTP]);
	else
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_PUBLIC_NTP),
				labels_bool[conf->public_ntp]);




	/* Defines if this server is an OCN */
	fprintf(fd, "# is_ocn.\n");
	fprintf(fd, "# Defines if this server is an OCN - cannot be on with is_tn .\n");
	fprintf(fd, "#\ton : Start service - Turns on OCN services\n");
	fprintf(fd, "#\toff: Stop service  - Disables OCN services\n");
	if (conf == NULL)
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_IS_OCN),
				labels_bool[DEFAULT_IS_OCN]);
	else
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_IS_OCN),
				labels_bool[conf->is_ocn]);


	/* Defines if this server is an TN */
	fprintf(fd, "# is_tn.\n");
	fprintf(fd, "# Defines if this server is an TN - cannot be on with is_ocn .\n");
	fprintf(fd, "#\ton : Start service - Turns on TN services\n");
	fprintf(fd, "#\toff: Stop service  - Disables TN services\n");
	if (conf == NULL)
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_IS_TN),
				labels_bool[DEFAULT_IS_TN]);
	else
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_IS_TN),
				labels_bool[conf->is_tn]);

	/* Active SHM  */
	fprintf(fd, "# SHM enabled.\n");
	fprintf(fd, "# Runs server health monitoring module .\n");
	fprintf(fd, "#\ton : Start service - Runs SHM module\n");
	fprintf(fd, "#\toff: Stop service  - Deactivates SHM module\n");
	if (conf == NULL)
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SERVER_SHM),
				labels_bool[DEFAULT_SERVER_SHM]);
	else
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SERVER_SHM),
				labels_bool[conf->server_shm]);

	/* Define SHM DAG Client IP */
	fprintf(fd, "# SHM DAG Client IP.\n");
	fprintf(fd, "# Requires that DAG packets can only be received by this IP .\n");
	if (conf == NULL)
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SHM_DAG_CLIENT),
				DEFAULT_SHM_DAG_CLIENT);
	else
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SHM_DAG_CLIENT),
				conf->shm_dag_client);


	/* *
	 * Virtual Machine Servers 
	 * */
	fprintf(fd, "# VM UDP server.\n");
	fprintf(fd, "# Serves clock data to radclock clients over the network.\n");
	fprintf(fd, "#\ton : runs a VM UDP server for guests\n");
	fprintf(fd, "#\toff: no server running\n");
	if (conf == NULL)
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SERVER_VM_UDP),
				labels_bool[DEFAULT_SERVER_VM_UDP]);
	else
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SERVER_VM_UDP),
				labels_bool[conf->server_vm_udp]);

	// TODO rename this to something other than list.. since it is now multipurpose
	fprintf(fd, "# File containing list of VM UDP clients in the case of "
			"VM UDP server\n");
	fprintf(fd, "# Name or IP address of master radclock in the case of "
			"synctype = vm_udp\n");
	if ( (conf) && (strlen(conf->vm_udp_list) > 0) )
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_VM_UDP_LIST),
				conf->vm_udp_list);
	else
		fprintf(fd, "#%s = %s\n\n", find_key_label(keys, CONFIG_VM_UDP_LIST),
				DEFAULT_VM_UDP_LIST);

	fprintf(fd, "# XEN server.\n");
	fprintf(fd, "# Serves clock data to radclock clients over the xenstore.\n");
	fprintf(fd, "#\ton : runs a XEN server for guests\n");
	fprintf(fd, "#\toff: no server running\n");
	if (conf == NULL)
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SERVER_XEN),
				labels_bool[DEFAULT_SERVER_XEN]);
	else
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SERVER_XEN),
				labels_bool[conf->server_xen]);

	fprintf(fd, "# VMWARE server.\n");
	fprintf(fd, "# Serves clock data to radclock clients over a VMCI socket.\n");
	fprintf(fd, "#\ton : runs a VMWARE server for guests\n");
	fprintf(fd, "#\toff: no server running\n");
	if (conf == NULL)
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SERVER_VMWARE),
				labels_bool[DEFAULT_SERVER_VMWARE]);
	else
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SERVER_VMWARE),
				labels_bool[conf->server_vmware]);


	/* Specify the type of synchronization server we run */
	fprintf(fd, "# NTP server.\n");
	fprintf(fd, "# Serves time to radclock clients over the network.\n");
	fprintf(fd, "#\ton : runs a NTP server for remote clients\n");
	fprintf(fd, "#\toff: no server running\n");
	if (conf == NULL)
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SERVER_NTP), labels_bool[DEFAULT_SERVER_NTP]);
	else
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SERVER_NTP), labels_bool[conf->server_ntp]);


	/* Adjust the system FFclock */
	fprintf(fd, "# System FFclock.\n"
				"# Let the RADclock daemon adjust the system FFclock. The main usecase for disabling\n"
				"# the push of radclock updates to the FFclock is to allow a second radclock\n"
				"# to run (eg for experimental purposes) without competing with an\n"
				"# existing radclock already operating as the FFclock's daemon.\n"
				"#\ton : adjust the system FFclock\n"
				"#\toff: does not adjust the FFclock\n");
	if (conf == NULL)
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_ADJUST_FFCLOCK), labels_bool[DEFAULT_ADJUST_FFCLOCK]);
	else
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_ADJUST_FFCLOCK), labels_bool[conf->adjust_FFclock]);


	/* Adjust the system FBclock */
	fprintf(fd, "# System FBclock.\n"
				"# Let the RADclock adjust the system FBclock, make sure no other FB synchronization\n"
				"# daemon is running, especially the ntp daemon.\n"
				"# Note that this feature relies on standard kernel calls to adjust the time and\n"
				"# is completely different from the IPC server provided. It is also independent \n"
				"# if the system FFclock functionality. This feature is essentially\n"
				"# provided to maintain FB system time as an alternative to ntpd when the latter\n"
				"# is causing problems, or when access to RADclock absolute time is needed on older\n"
				"# systems whose calling code cannot be changes.\n"
				"# If you care about synchronization, either use the FFclock, or turn the IPC\n"
				"# server on and use the libradclock API.\n"
				"# Also note that system clock time causality may break since the system clock will\n"
				"# be set on RADclock restart. The system clock will tick monotonically afterwards.\n"
				"#\ton : adjust the system FBclock\n"
				"#\toff: does not adjust the FBclock (to use with NTP piggybacking)\n");
	if (conf == NULL)
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_ADJUST_FBCLOCK), labels_bool[DEFAULT_ADJUST_FBCLOCK]);
	else
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_ADJUST_FBCLOCK), labels_bool[conf->adjust_FBclock]);



	fprintf(fd, "\n\n\n");
	fprintf(fd, "#----------------------------------------------------------------------------#\n");
	fprintf(fd, "# Environment and Tuning parameters\n");
	fprintf(fd, "#----------------------------------------------------------------------------#\n");
	fprintf(fd, "\n");

	/* Temperature */
	fprintf(fd, "# Temperature environment and hardware quality.\n");
	fprintf(fd, "# Keywods accepted are: poor, good, excellent.\n"	);
	fprintf(fd, "# This setting overrides temperature and hardware expert mode (default behavior). \n"	);
	if (conf == NULL)
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_TEMPQUALITY ), temp_quality[CONFIG_QUALITY_GOOD].label);
	else {
		/* There is an existing configuration file */
		switch (get_temperature_config(conf)) { 
		case CONFIG_QUALITY_POOR:
			fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_TEMPQUALITY ), 
					temp_quality[CONFIG_QUALITY_POOR].label);
			break;
		case CONFIG_QUALITY_GOOD:
			fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_TEMPQUALITY ), 
					temp_quality[CONFIG_QUALITY_GOOD].label);
			break;
		case CONFIG_QUALITY_EXCEL:
			fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_TEMPQUALITY ), 
					temp_quality[CONFIG_QUALITY_EXCEL].label);
			break;
		/* We have an existing expert config */
		case CONFIG_QUALITY_UNKWN:
		default:
		fprintf(fd, "#%s = %s\n\n", find_key_label(keys, CONFIG_TEMPQUALITY ), 
				temp_quality[CONFIG_QUALITY_GOOD].label);
		}
	}

	/* Temperature expert mode */
	fprintf(fd, "# EXPERIMENTAL.\n");
	fprintf(fd, "# Temperature environment and hardware quality - EXPERT.\n");
	fprintf(fd, "# This settings are over-written by the %s keyword.\n", find_key_label(keys,CONFIG_TEMPQUALITY));
	if (conf == NULL) {
		fprintf(fd, "#%s = %.9lf\n", find_key_label(keys, CONFIG_TSLIMIT), TS_LIMIT_GOOD);
		fprintf(fd, "#%s = %.9lf\n", find_key_label(keys, CONFIG_SKM_SCALE), SKM_SCALE_GOOD);
		fprintf(fd, "#%s = %.9lf\n", find_key_label(keys, CONFIG_RATE_ERR_BOUND), RATE_ERR_BOUND_GOOD);
		fprintf(fd, "#%s = %.9lf\n", find_key_label(keys, CONFIG_BEST_SKM_RATE), BEST_SKM_RATE_GOOD);
		fprintf(fd, "#%s = %d\n", find_key_label(keys, CONFIG_OFFSET_RATIO), OFFSET_RATIO_GOOD);
		fprintf(fd, "#%s = %.9lf\n", find_key_label(keys, CONFIG_PLOCAL_QUALITY), PLOCAL_QUALITY_GOOD);
		fprintf(fd, "\n"); 
	} else {
		switch (get_temperature_config(conf)) { 
		case CONFIG_QUALITY_POOR:
		case CONFIG_QUALITY_GOOD:
		case CONFIG_QUALITY_EXCEL:
			fprintf(fd, "#%s = %.9lf\n", find_key_label(keys, CONFIG_TSLIMIT), TS_LIMIT_GOOD);
			fprintf(fd, "#%s = %.9lf\n", find_key_label(keys, CONFIG_SKM_SCALE), SKM_SCALE_GOOD);
			fprintf(fd, "#%s = %.9lf\n", find_key_label(keys, CONFIG_RATE_ERR_BOUND), RATE_ERR_BOUND_GOOD);
			fprintf(fd, "#%s = %.9lf\n", find_key_label(keys, CONFIG_BEST_SKM_RATE), BEST_SKM_RATE_GOOD);
			fprintf(fd, "#%s = %d\n", find_key_label(keys, CONFIG_OFFSET_RATIO), OFFSET_RATIO_GOOD);
			fprintf(fd, "#%s = %.9lf\n", find_key_label(keys, CONFIG_PLOCAL_QUALITY), PLOCAL_QUALITY_GOOD);
			fprintf(fd, "\n"); 
			break;
		/* We have an existing expert config */
		case CONFIG_QUALITY_UNKWN:
		default:
			fprintf(fd, "%s = %.9lf\n", find_key_label(keys, CONFIG_TSLIMIT), conf->metaparam.TSLIMIT);
			fprintf(fd, "%s = %.9lf\n", find_key_label(keys, CONFIG_SKM_SCALE), conf->metaparam.SKM_SCALE);
			fprintf(fd, "%s = %.9lf\n", find_key_label(keys, CONFIG_RATE_ERR_BOUND), conf->metaparam.RateErrBOUND);
			fprintf(fd, "%s = %.9lf\n", find_key_label(keys, CONFIG_BEST_SKM_RATE), conf->metaparam.BestSKMrate);
			fprintf(fd, "%s = %d\n", find_key_label(keys, CONFIG_OFFSET_RATIO), conf->metaparam.offset_ratio);
			fprintf(fd, "%s = %.9lf\n", find_key_label(keys, CONFIG_PLOCAL_QUALITY), conf->metaparam.plocal_quality);
			fprintf(fd, "\n"); 
		}
	}


	/* Phat init */
	fprintf(fd, "# For a quick start, the initial value of the period of the counter (in seconds). \n");
	if (conf == NULL)
		fprintf(fd, "%s = %lg\n\n", find_key_label(keys, CONFIG_PHAT_INIT), DEFAULT_PHAT_INIT);
	else
		fprintf(fd, "%s = %lg\n\n", find_key_label(keys, CONFIG_PHAT_INIT), conf->phat_init);


	/* Delta Host */
	fprintf(fd, "# Estimation of the asym within the host (in seconds). \n");
	if (conf == NULL)
		fprintf(fd, "%s = %lf\n\n", find_key_label(keys, CONFIG_ASYM_HOST), DEFAULT_ASYM_HOST);
	else
		fprintf(fd, "%s = %lf\n\n", find_key_label(keys, CONFIG_ASYM_HOST), conf->asym_host);

	/* Delta Network */
	fprintf(fd, "# Estimation of the network asym (in seconds). \n"	);
	if (conf == NULL)
		fprintf(fd, "%s = %lf\n\n", find_key_label(keys, CONFIG_ASYM_NET), DEFAULT_ASYM_NET);
	else
		fprintf(fd, "%s = %lf\n\n", find_key_label(keys, CONFIG_ASYM_NET), conf->asym_net);



	fprintf(fd, "\n\n\n");
	fprintf(fd, "#----------------------------------------------------------------------------#\n");
	fprintf(fd, "# Input / Output parameters\n");
	fprintf(fd, "#----------------------------------------------------------------------------#\n");
	fprintf(fd, "\n");
	
	/* Network device */
	fprintf(fd, "# Network interface.\n");
	fprintf(fd, "# Specify a different interface (xl0, eth0, ...)\n");
	fprintf(fd, "# If none, the RADclock will lookup for a default one.\n");
	if ( (conf) && (strlen(conf->network_device) > 0) )
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_NETWORKDEV), conf->network_device);
	else
		fprintf(fd, "#%s = %s\n\n", find_key_label(keys, CONFIG_NETWORKDEV), DEFAULT_NETWORKDEV);


	/* RAW Input */
	fprintf(fd, "# Synchronization data input file (modified pcap format).\n");
	fprintf(fd, "# Replay mode requires a file produced by the RADclock.\n");
	if ( (conf) && (strlen(conf->sync_in_pcap) > 0) )
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SYNC_IN_PCAP), conf->sync_in_pcap);
	else
		fprintf(fd, "#%s = %s\n\n", find_key_label(keys, CONFIG_SYNC_IN_PCAP), DEFAULT_SYNC_IN_PCAP);

	/* Stamp Input */
	fprintf(fd, "# Synchronization data input file (ascii format).\n");
	fprintf(fd, "# Replay mode requires a file produced by the RADclock.\n");
	if ( (conf) && (strlen(conf->sync_in_ascii) > 0) )
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SYNC_IN_ASCII), conf->sync_in_ascii);
	else
		fprintf(fd, "#%s = %s\n\n", find_key_label(keys, CONFIG_SYNC_IN_ASCII), DEFAULT_SYNC_IN_ASCII);

	/* RAW Output */
	fprintf(fd, "# Synchronization data output file (modified pcap format).\n");
	if ( (conf) && (strlen(conf->sync_out_pcap) > 0) )
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SYNC_OUT_PCAP), conf->sync_out_pcap);
	else
		fprintf(fd, "#%s = %s\n\n", find_key_label(keys, CONFIG_SYNC_OUT_PCAP), DEFAULT_SYNC_OUT_PCAP);

	/* Stamp output */
	fprintf(fd, "# Synchronization data output file (ascii format).\n");
	if ( (conf) && (strlen(conf->sync_out_ascii) > 0) )
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_SYNC_OUT_ASCII), conf->sync_out_ascii);
	else
		fprintf(fd, "#%s = %s\n\n", find_key_label(keys, CONFIG_SYNC_OUT_ASCII), DEFAULT_SYNC_OUT_ASCII);

	/* Matlab */
	fprintf(fd, "# Internal clock data output file (ascii format).\n");
	if ( (conf) && (strlen(conf->clock_out_ascii) > 0) )
		fprintf(fd, "%s = %s\n\n", find_key_label(keys, CONFIG_CLOCK_OUT_ASCII), conf->clock_out_ascii);
	else
		fprintf(fd, "#%s = %s\n\n", find_key_label(keys, CONFIG_CLOCK_OUT_ASCII), DEFAULT_CLOCK_OUT_ASCII);

	/* Specifies NTC servers to be used */
	fprintf(fd, "# NTC ids.\n");
	fprintf(fd, "# %s = NTC_ID domain. NTC_ID should remain constant and never re-used.\n", find_key_label(keys, CONFIG_NTC));
	int written_ntc_ids = 0;
	if ( conf )
	{
		for (int i = 0; i<MAX_NTC; i++)
			if ( strlen(conf->ntc[i].domain) > 0 )
			{
				fprintf(fd, "%s = %s %d\n", find_key_label(keys, CONFIG_NTC), conf->ntc[i].domain, conf->ntc[i].id);
				written_ntc_ids ++;
			}
	}

}


/** Extract the key label and the corresponding value from a line of the parsed configuration file */
int extract_key_value(char* c, char* key, char* value) {

	char *ch;

	// Look for first character in line
	while ((*c==' ') || (*c=='\t')) { c++; }

	// Check if character for a config parameter
	if ((*c=='#') || (*c=='\n') || (*c=='\0')) { return 0; }

	// Identify the separator, copy key and clean end white spaces 
	strncpy(key, c, strchr(c,'=')-c);
	ch = key;
	while ((*ch!=' ') && (*ch!='\n') && (*ch != '\t')) { ch++; }
	*ch = '\0';

	// Identify first character of the value
	c = strchr(c,'=');
	ch = c;
	while ((*ch=='=') || (*ch==' ') || (*ch=='\t')) { ch++; }
	c = ch;

	// Remove possible comments in line after the value '#' 
	while ((*ch!='\0') && (*ch!='#')) {ch++;}
		*ch = '\0';

	// Remove extra space at the end of the line if any
	ch = c + strlen(c) - 1 ;
	while ((*ch==' ') || (*ch=='\t') || (*ch=='\n')) { ch--; }
	*(ch+1) = '\0';

	// Copy final string
	strncpy(value, c, strlen(c)+1);

	return 1;
}


/** Match the key index from the key label */
int match_key(struct _key *keys, char* keylabel) {
	for (;;) {	
		if (keys->keytype == CONFIG_UNKNOWN) {
			verbose(LOG_WARNING, "Unknown key in config file: %s", keylabel);
			return 0;
		}
		if (strcmp(keys->label, keylabel) == 0) 
			return keys->keytype;
		keys++;	
	}
	return 1;
}


int check_valid_option(char* value, char* labels[], int label_sz) 
{
	int i;
	for ( i=0; i<label_sz; i++ )
	{
		if ( strcmp(value, labels[i]) == 0 )
			return i;
	}
	return -1;
}





// Global data, but also really ugly to make it a parameter.
int have_all_tmpqual = 0; /* To know if we run expert mode */




/* Update global data with values retrieved from the configuration file
 * This function counts the number of time servers being configured, and passes
 * this back for storage as handle->nservers .
 */
static int
update_data(struct radclock_config *conf, u_int32_t *mask, int codekey, char *value, int *nf, int ns, int light_update)
{
	// The mask parameter should always be positioned to UPDMASK_NOUPD except
	// for the first call to config_parse() after parsing the command line
	// arguments

	int ival		= 0;
	double dval		= 0.0;
	int iqual		= 0;
	struct _key *quality = temp_quality;
	char sval[MAXLINE];
	// Additionnal input checks: codekey and value
	if ( codekey < 0) {
		verbose(LOG_ERR, "Negative key value from the config file");
		return 0;
	}

	if (value == NULL || strlen(value)==0) {
		verbose(LOG_ERR, "Empty value from the config file");
		return 0;
	}

	// Currently the only light update option is the CONFIG_PUBLIC_NTP
	if ( light_update && codekey != CONFIG_PUBLIC_NTP) {
		return 0;
	}


switch (codekey) {
	
	case CONFIG_RADCLOCK_VERSION:
		strcpy(conf->radclock_version, value);
		break;


	case CONFIG_VERBOSE:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_VERBOSE) ) 
			break;
		ival = check_valid_option(value, labels_verb, 3);
		// Indicate changed value
		if ( conf->verbose_level != ival )
			SET_UPDATE(*mask, UPDMASK_VERBOSE);
		if ( (ival<0) || (ival>2)) {
			verbose(LOG_WARNING, "verbose_level value incorrect. Fall back to default.");
			conf->verbose_level = DEFAULT_VERBOSE;
		}
		else
			conf->verbose_level = ival;
		break;


	case CONFIG_SERVER_IPC:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_SERVER_IPC) ) 
			break;
		ival = check_valid_option(value, labels_bool, 2);
		// Indicate changed value
		if ( conf->server_ipc != ival )
			SET_UPDATE(*mask, UPDMASK_SERVER_IPC);
		if ( ival < 0)
		{
			verbose(LOG_WARNING, "ipc_server value incorrect. Fall back to default.");
			conf->server_ipc = DEFAULT_SERVER_IPC;
		}
		else
			conf->server_ipc = ival;
		break;

	case CONFIG_SERVER_TELEMETRY:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_SERVER_TELEMETRY) ) 
			break;
		ival = check_valid_option(value, labels_bool, 2);
		// Indicate changed value
		if ( conf->server_telemetry != ival )
			SET_UPDATE(*mask, UPDMASK_SERVER_TELEMETRY);
		if ( ival < 0)
		{
			verbose(LOG_WARNING, "telemetry_server value incorrect. Fall back to default.");
			conf->server_telemetry = DEFAULT_SERVER_TELEMETRY;
		}
		else
			conf->server_telemetry = ival;
		break;

	case CONFIG_PUBLIC_NTP:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_PUBLIC_NTP) ) 
			break;
		ival = check_valid_option(value, labels_bool, 2);
		// Indicate changed value
		if ( conf->public_ntp != ival )
			SET_UPDATE(*mask, UPDMASK_PUBLIC_NTP);
		if ( ival < 0)
		{
			verbose(LOG_WARNING, "public_ntp value incorrect. Fall back to default.");
			conf->public_ntp = DEFAULT_PUBLIC_NTP;
		}
		else
			conf->public_ntp = ival;
		break;

	case CONFIG_IS_OCN:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_IS_OCN) ) 
			break;
		ival = check_valid_option(value, labels_bool, 2);
		// Indicate changed value
		if ( conf->is_ocn != ival )
			SET_UPDATE(*mask, UPDMASK_IS_OCN);
		if ( ival < 0)
		{
			verbose(LOG_WARNING, "is_ocn value incorrect. Fall back to default.");
			conf->is_ocn = DEFAULT_IS_OCN;
		}
		else
			conf->is_ocn = ival;
		break;


	case CONFIG_IS_TN:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_IS_TN) ) 
			break;
		ival = check_valid_option(value, labels_bool, 2);
		// Indicate changed value
		if ( conf->is_tn != ival )
			SET_UPDATE(*mask, UPDMASK_IS_TN);
		if ( ival < 0)
		{
			verbose(LOG_WARNING, "is_tn value incorrect. Fall back to default.");
			conf->is_tn = DEFAULT_IS_TN;
		}
		else
			conf->is_tn = ival;
		break;

	case CONFIG_SERVER_SHM:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_SERVER_SHM) ) 
			break;
		ival = check_valid_option(value, labels_bool, 2);
		// Indicate changed value
		if ( conf->server_shm != ival )
			SET_UPDATE(*mask, UPDMASK_SERVER_SHM);
		if ( ival < 0)
		{
			verbose(LOG_WARNING, "shm_server value incorrect. Fall back to default.");
			conf->server_shm = DEFAULT_SERVER_SHM;
		}
		else
			conf->server_shm = ival;
		break;

	case CONFIG_SHM_DAG_CLIENT:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_SHM_DAG_CLIENT) ) 
			break;
		if ( strcmp(conf->shm_dag_client, value) != 0 )
			SET_UPDATE(*mask, UPDMASK_SHM_DAG_CLIENT);
		strcpy(conf->shm_dag_client, value);

		break;

	case CONFIG_SYNCHRO_TYPE:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_SYNCHRO_TYPE) ) 
			break;
		ival = check_valid_option(value, labels_sync, 8);
		// Indicate changed value
		if ( conf->synchro_type != ival )
			SET_UPDATE(*mask, UPDMASK_SYNCHRO_TYPE);
		if ( ival < 0) {
			verbose(LOG_WARNING, "synchro_type value incorrect. Fall back to default.");
			conf->synchro_type = DEFAULT_SYNCHRO_TYPE;
		}
		else
			conf->synchro_type = ival;
		break;


	case CONFIG_SERVER_NTP:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_SERVER_NTP) ) 
			break;
		ival = check_valid_option(value, labels_bool, 2);
		// Indicate changed value
		if ( conf->server_ntp != ival )
			SET_UPDATE(*mask, UPDMASK_SERVER_NTP);
		if ( ival < 0 ) {
			verbose(LOG_WARNING, "ntp_server parameter incorrect. Fall back to default.");
			conf->server_ntp = DEFAULT_SERVER_NTP;
		}
		else
			conf->server_ntp = ival;
		break;
	
	case CONFIG_SERVER_VM_UDP:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_SERVER_VM_UDP) ) 
			break;
		ival = check_valid_option(value, labels_bool, 2);
		// Indicate changed value
		if ( conf->server_vm_udp != ival )
			SET_UPDATE(*mask, UPDMASK_SERVER_VM_UDP);
		if ( ival < 0 ) {
			verbose(LOG_WARNING, "vm_udp_server parameter incorrect. Fall back to default.");
			conf->server_vm_udp = DEFAULT_SERVER_VM_UDP;
		}
		else
			conf->server_vm_udp = ival;
		break;

	case CONFIG_SERVER_XEN:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_SERVER_XEN) ) 
			break;
		ival = check_valid_option(value, labels_bool, 2);
		// Indicate changed value
		if ( conf->server_xen != ival )
			SET_UPDATE(*mask, UPDMASK_SERVER_XEN);
		if ( ival < 0 ) {
			verbose(LOG_WARNING, "xen_server parameter incorrect. Fall back to default.");
			conf->server_xen = DEFAULT_SERVER_XEN;
		}
		else
			conf->server_xen = ival;
		break;

	case CONFIG_SERVER_VMWARE:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_SERVER_VMWARE) ) 
			break;
		ival = check_valid_option(value, labels_bool, 2);
		// Indicate changed value
		if ( conf->server_vmware != ival )
			SET_UPDATE(*mask, UPDMASK_SERVER_VMWARE);
		if ( ival < 0 ) {
			verbose(LOG_WARNING, "vmware_server parameter incorrect. Fall back to default.");
			conf->server_vmware = DEFAULT_SERVER_VMWARE;
		}
		else
			conf->server_vmware = ival;
		break;


	case CONFIG_ADJUST_FFCLOCK:
		// If value specified on the command line
		if (HAS_UPDATE(*mask, UPDMASK_ADJUST_FFCLOCK))
			break;
		ival = check_valid_option(value, labels_bool, 2);
		// Indicate changed value
		if (conf->adjust_FFclock != ival)
			SET_UPDATE(*mask, UPDMASK_ADJUST_FFCLOCK);
		if (ival < 0) {
			verbose(LOG_WARNING, "adjust_FFclock parameter incorrect."
					"Fall back to default.");
			conf->adjust_FFclock = DEFAULT_ADJUST_FFCLOCK;
		}
		else
			conf->adjust_FFclock = ival;
		break;


	case CONFIG_ADJUST_FBCLOCK:
		// If value specified on the command line
		if (HAS_UPDATE(*mask, UPDMASK_ADJUST_FBCLOCK))
			break;
		ival = check_valid_option(value, labels_bool, 2);
		// Indicate changed value
		if (conf->adjust_FBclock != ival)
			SET_UPDATE(*mask, UPDMASK_ADJUST_FBCLOCK);
		if (ival < 0) {
			verbose(LOG_WARNING, "adjust_FBclock parameter incorrect."
					"Fall back to default.");
			conf->adjust_FBclock = DEFAULT_ADJUST_FBCLOCK;
		}
		else
			conf->adjust_FBclock = ival;
		break;


	case CONFIG_POLLPERIOD:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_POLLPERIOD) )
			break;
		ival = atoi(value);
		// Indicate changed value
		if ( conf->poll_period != ival )
			SET_UPDATE(*mask, UPDMASK_POLLPERIOD);
		if ((ival<RAD_MINPOLL) || (ival>RAD_MAXPOLL)) {	
			verbose(LOG_WARNING, "Poll period value out of [%d,%d] range (%d). Fall back to default.",
				   	ival, RAD_MINPOLL, RAD_MAXPOLL);
			conf->poll_period = DEFAULT_NTP_POLL_PERIOD;
		}
		else {
			conf->poll_period = ival;
		}
		break;


	case CONFIG_TSLIMIT:
		/* Be sure we don't override an overall temperature setting */
		if (have_all_tmpqual == 0) { 
			dval = strtod(value, NULL);
			// Indicate changed value
			if ( conf->metaparam.TSLIMIT != dval )
				SET_UPDATE(*mask, UPDMASK_TEMPQUALITY);
			if (dval<0)  {
				verbose(LOG_WARNING, "Using ts_limit value out of range (%f). Fall back to default.", dval);
				conf->metaparam.TSLIMIT = TS_LIMIT_GOOD;
			}
			else {
				conf->metaparam.TSLIMIT = dval;
			}
		}
		break;
		
	case CONFIG_SKM_SCALE:
		/* Be sure we don't override an overall temperature setting */
		if (have_all_tmpqual == 0) { 
			dval = strtod(value, NULL);
			// Indicate changed value
			if ( conf->metaparam.SKM_SCALE != dval )
				SET_UPDATE(*mask, UPDMASK_TEMPQUALITY);
			if (dval<0)  {
				verbose(LOG_WARNING, "Using skm_scale value out of range (%f). Fall back to default.", dval);
				conf->metaparam.SKM_SCALE = SKM_SCALE_GOOD;
			}
			else {
				conf->metaparam.SKM_SCALE = dval;
			}
		}
		break;
		
	case CONFIG_RATE_ERR_BOUND:
		/* Be sure we don't override an overall temperature setting */
		if (have_all_tmpqual == 0) { 
			dval = strtod(value, NULL);
			// Indicate changed value
			if ( conf->metaparam.RateErrBOUND != dval )
				SET_UPDATE(*mask, UPDMASK_TEMPQUALITY);
			if (dval<0)  {
				verbose(LOG_WARNING, "Using rate_error_bound value out of range (%f). Fall back to default.", dval);
				conf->metaparam.RateErrBOUND = RATE_ERR_BOUND_GOOD;
			}
			else {
				conf->metaparam.RateErrBOUND = dval;
			}
		}
		break;
	
	case CONFIG_BEST_SKM_RATE:
		/* Be sure we don't override an overall temperature setting */
		if (have_all_tmpqual == 0) { 
			dval = strtod(value, NULL);
			// Indicate changed value
			if ( conf->metaparam.BestSKMrate != dval )
				SET_UPDATE(*mask, UPDMASK_TEMPQUALITY);
			if (dval<0)  {
				verbose(LOG_WARNING, "Using best_skm_rate value out of range (%f). Fall back to default.", dval);
				conf->metaparam.BestSKMrate = BEST_SKM_RATE_GOOD;
			}
			else {
				conf->metaparam.BestSKMrate = dval;
			}
		}
		break;

	case CONFIG_OFFSET_RATIO:
		/* Be sure we don't override an overall temperature setting */
		if (have_all_tmpqual == 0) { 
			ival = atoi(value);
			// Indicate changed value
			if ( conf->metaparam.offset_ratio != ival )
				SET_UPDATE(*mask, UPDMASK_TEMPQUALITY);
			if (ival<=0)  {
				verbose(LOG_WARNING, "Using offset_ratio value out of range (%f). Fall back to default.", ival);
				conf->metaparam.offset_ratio = OFFSET_RATIO_GOOD;
			}
			else {
				conf->metaparam.offset_ratio = ival;
			}
		}
		break;

	case CONFIG_PLOCAL_QUALITY:
		/* Be sure we don't override an overall temperature setting */
		if (have_all_tmpqual == 0) { 
			dval = strtod(value, NULL);
			// Indicate changed value
			if ( conf->metaparam.plocal_quality != dval )
				SET_UPDATE(*mask, UPDMASK_TEMPQUALITY);
			if (dval<0)  {
				verbose(LOG_WARNING, "Using plocal_quality value out of range (%f). Fall back to default.", dval);
				conf->metaparam.plocal_quality = PLOCAL_QUALITY_GOOD;
			}
			else {
				conf->metaparam.plocal_quality = dval;
			}
		}
		break;

	
	case CONFIG_TEMPQUALITY:
		/* We have an overall environment quality key word */
		have_all_tmpqual = 1;
		for (;;) {
			if (quality->keytype == CONFIG_QUALITY_UNKWN) {
				verbose(LOG_ERR, "The quality parameter given is unknown");
				return 0;
			}
			if (strcmp(quality->label, value) == 0) {
				iqual = quality->keytype;
				break;
			}
			quality++;
		}
		switch (iqual) {
			case CONFIG_QUALITY_POOR:
				// Indicate changed value
				if ( (conf->metaparam.TSLIMIT - TS_LIMIT_POOR) != 0)
					SET_UPDATE(*mask, UPDMASK_TEMPQUALITY);
				conf->metaparam.TSLIMIT 		= TS_LIMIT_POOR;
				conf->metaparam.SKM_SCALE 		= SKM_SCALE_POOR;
				conf->metaparam.RateErrBOUND 	= RATE_ERR_BOUND_POOR;
				conf->metaparam.BestSKMrate 	= BEST_SKM_RATE_POOR;
				conf->metaparam.offset_ratio 	= OFFSET_RATIO_POOR;
				conf->metaparam.plocal_quality	= PLOCAL_QUALITY_POOR;
				break;
			case CONFIG_QUALITY_GOOD:
				// Indicate changed value
				if ( (conf->metaparam.TSLIMIT - TS_LIMIT_GOOD) != 0 )
					SET_UPDATE(*mask, UPDMASK_TEMPQUALITY);
				conf->metaparam.TSLIMIT 		= TS_LIMIT_GOOD;
				conf->metaparam.SKM_SCALE		= SKM_SCALE_GOOD;
				conf->metaparam.RateErrBOUND 	= RATE_ERR_BOUND_GOOD;
				conf->metaparam.BestSKMrate 	= BEST_SKM_RATE_GOOD;
				conf->metaparam.offset_ratio 	= OFFSET_RATIO_GOOD;
				conf->metaparam.plocal_quality	= PLOCAL_QUALITY_GOOD;
				break;
			case CONFIG_QUALITY_EXCEL:
				// Indicate changed value
				if ( (conf->metaparam.TSLIMIT - TS_LIMIT_EXCEL) != 0 )
					SET_UPDATE(*mask, UPDMASK_TEMPQUALITY);
				conf->metaparam.TSLIMIT 		= TS_LIMIT_EXCEL;
				conf->metaparam.SKM_SCALE 		= SKM_SCALE_EXCEL;
				conf->metaparam.RateErrBOUND 	= RATE_ERR_BOUND_EXCEL;
				conf->metaparam.BestSKMrate 	= BEST_SKM_RATE_EXCEL;
				conf->metaparam.offset_ratio 	= OFFSET_RATIO_EXCEL;
				conf->metaparam.plocal_quality	= PLOCAL_QUALITY_EXCEL;
				break;
			default:
				verbose(LOG_ERR, "Quality parameter given is unknown");
				break;
		}	
		break;


	case CONFIG_PHAT_INIT:
		dval = strtod(value, NULL);
		if ( (dval<0) || (dval==0) || (dval>1)) {
			verbose(LOG_WARNING, "Using phat_init value out of range (%f). Falling back to default.", dval);
			conf->phat_init = DEFAULT_PHAT_INIT;
		}
		else {
			conf->phat_init = dval;
		}
		break;


	case CONFIG_ASYM_HOST:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_ASYM_HOST) ) 
			break;
		dval = strtod(value, NULL);
		// Indicate changed value
		if ( conf->asym_host != dval )
			SET_UPDATE(*mask, UPDMASK_ASYM_HOST);
		if ( (dval<0) || (dval>1)) {
			verbose(LOG_WARNING, "Using host_asymmetry value out of range (%f). Falling back to default.", dval);
			conf->asym_host = DEFAULT_ASYM_HOST;
		}
		else {
			conf->asym_host = dval;
		}
		break;


	case CONFIG_ASYM_NET:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_ASYM_NET) ) 
			break;
		dval = strtod(value, NULL);
		// Indicate changed value
		if ( conf->asym_net != dval )
			SET_UPDATE(*mask, UPDMASK_ASYM_NET);
		if ( (dval<0) || (dval>1)) {
			verbose(LOG_WARNING, "Using network_asymmetry value out of range (%f). Falling back to default.", dval);
			conf->asym_net = DEFAULT_ASYM_NET;
		}
		else {
			conf->asym_net = dval;
		}
		break;


	case CONFIG_HOSTNAME:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_HOSTNAME) ) 
			break;
		if ( strcmp(conf->hostname, value) != 0 )
			SET_UPDATE(*mask, UPDMASK_HOSTNAME);
		strcpy(conf->hostname, value);
		break;


	/* There may be multiple time_server lines in the conf file.
	 * Space is already allocated for the first, if more encountered, more is made
	 * dynamically. If this is called during rehash, more may already be allocated.
	 * The allocated memory is organised as nf concatenated blocks,
	 * each of MAXLINE chars, off a common origin pointed to by conf->time_server .
	 * Here nf tracks the number of servers parsed so far this time, ns the number
	 * last time (or ns=0 when starting).
	 */
	case CONFIG_TIME_SERVER:

		verbose(VERB_DEBUG, "found server nf = %d (ns = %d)", *nf, ns);

		// If value specified on the command line, replace first server in config
		if ( *nf == 0 && HAS_UPDATE(*mask, UPDMASK_TIME_SERVER) ) {
			//verbose(LOG_DEBUG, " Adopting CMDline specified %s as server %d", conf->time_server, *nf);
			*nf = 1;
			break;
		}

		// From file, process potentially multiple lines
		char *this_s;		// start address of string for this server
		if (*nf < ns) {	// if storage already there 
			this_s = conf->time_server + (*nf)*MAXLINE;
			if ( strcmp(this_s, value) != 0 )
				SET_UPDATE(*mask, UPDMASK_TIME_SERVER);
		} else {			// must extend allocation, and record new origin
			conf->time_server = realloc(conf->time_server,(*nf+1)*MAXLINE);
			this_s = conf->time_server + (*nf)*MAXLINE;
			SET_UPDATE(*mask, UPDMASK_TIME_SERVER);
		}
		strcpy(this_s, value);
		for (int s=0; s<*nf; s++)
			verbose(LOG_WARNING, "Time server          : %d %s", s, conf->time_server + s*MAXLINE);
		//verbose(LOG_DEBUG, " Adopting %s as server nf = %d (ns = %d)", this_s, *nf, ns);
		(*nf)++;
		break;


	case CONFIG_NTC:
		// If value specified on the command line

		// Config line can accept one or two arguements (can optionally exclude id)
		iqual = sscanf(value, "%s %d", sval, &ival);
		if (! (iqual == 2 && ival >= 0 && ival < MAX_NTC ) )
			ival = -1;
		if (iqual == 1)
		{
			ival = 0;
			for (iqual =0; iqual< MAX_NTC; iqual++)
				if (conf->ntc[iqual].domain[0] == 0)
				{
					ival = iqual;
					break;
				}
		}

		if (0 <= ival && ival < MAX_NTC)
		{
			SET_UPDATE(*mask, UPDMASK_NTC);
			strcpy(conf->ntc[ival].domain, sval);
			conf->ntc[ival].id = ival;
		}
		break;

	case CONFIG_NETWORKDEV:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_NETWORKDEV) ) 
			break;
		if ( strcmp(conf->network_device, value) != 0 )
			SET_UPDATE(*mask, UPDMASK_NETWORKDEV);
		strcpy(conf->network_device, value);
		break;


	case CONFIG_SYNC_IN_PCAP:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_SYNC_IN_PCAP) ) 
			break;
		if ( strcmp(conf->sync_in_pcap, value) != 0 )
			SET_UPDATE(*mask, UPDMASK_SYNC_IN_PCAP);
		strcpy(conf->sync_in_pcap, value);
		break;


	case CONFIG_SYNC_IN_ASCII:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_SYNC_IN_ASCII) ) 
			break;
		if ( strcmp(conf->sync_in_ascii, value) != 0 )
			SET_UPDATE(*mask, UPDMASK_SYNC_IN_ASCII);
		strcpy(conf->sync_in_ascii, value);
		break;


	case CONFIG_SYNC_OUT_PCAP:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_SYNC_OUT_PCAP) ) 
			break;
		if ( strcmp(conf->sync_out_pcap, value) != 0 )
			SET_UPDATE(*mask, UPDMASK_SYNC_OUT_PCAP);
		strcpy(conf->sync_out_pcap, value);
		break;


	case CONFIG_SYNC_OUT_ASCII:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_SYNC_OUT_ASCII) ) 
			break;
		if ( strcmp(conf->sync_out_ascii, value) != 0 )
			SET_UPDATE(*mask, UPDMASK_SYNC_OUT_ASCII);
		strcpy(conf->sync_out_ascii, value);
		break;


	case CONFIG_CLOCK_OUT_ASCII:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_CLOCK_OUT_ASCII) ) 
			break;
		if ( strcmp(conf->clock_out_ascii, value) != 0 )
			SET_UPDATE(*mask, UPDMASK_CLOCK_OUT_ASCII);
		strcpy(conf->clock_out_ascii, value);
		break;


	case CONFIG_VM_UDP_LIST:
		// If value specified on the command line
		if ( HAS_UPDATE(*mask, UPDMASK_VM_UDP_LIST) ) 
			break;
		if ( strcmp(conf->vm_udp_list, value) != 0 )
			SET_UPDATE(*mask, UPDMASK_VM_UDP_LIST);
		strcpy(conf->vm_udp_list, value);
		break;

	default:
		verbose(LOG_WARNING, "Unknown CONFIG_* symbol.");
		break;
}
return 1;
}


/*
 * Reads config file line by line, retrieve (key,value) and update global data\
 * Only parses select inforation from config. Doesn't attempt to reread all data
 * This allows avoids the need to shutdown and resetup running threads.
 * This should be allowed to run without interferring with live radclock running threads
 */
int light_config_parse(struct radclock_config *conf, u_int32_t *mask, int is_daemon, int *ns)
{
	struct _key *pkey = keys;
	int codekey=0;
	int nf;		// count time servers found
	char *c;
	char line[MAXLINE];
	char key[MAXLINE];
	char value[MAXLINE];
	FILE* fd = NULL;

	/* Check input */
	if (conf == NULL) {
		verbose(LOG_ERR, "Configuration structure is NULL.");
		return 0;
	}
	
	/* Config and log files */
	if (strlen(conf->conffile) == 0)
	{
		if ( is_daemon )
			strcpy(conf->conffile, DAEMON_CONFIG_FILE);
		else
			strcpy(conf->conffile, BIN_CONFIG_FILE);
	}

	if (strlen(conf->logfile) == 0)
	{
		if ( is_daemon )
			strcpy(conf->logfile, DAEMON_LOG_FILE);
		else
			strcpy(conf->logfile, BIN_LOG_FILE);
	}

	/* The file can't be opened. Ether it doesn't exist yet or I/O error.
	 */
	fd = fopen(conf->conffile, "r");
	if (!fd) {
		verbose(LOG_ERR, "Configuration file doesn't exist upon light config reparse.");
		return 0;
	}

	while ((c=fgets(line, MAXLINE, fd))!=NULL) {

		// Start with a reset of the value to avoid mistakes
		strcpy(value, "");

		// Extract key and values from the conf file
		if ( !(extract_key_value(c, key, value) ))
			continue;
	
		// Identify the key and update config values
		codekey = match_key(pkey, key);

		// This line is not a configuration line
		if (codekey < 0)
			continue;

		/* Update in case we actually retrieved a value, update nf */
		if ( strlen(value) > 0 )
			update_data(conf, mask, codekey, value, &nf, *ns, 1);
	}
	fclose(fd);

	return 1;
}


/*
 * Reads config file line by line, retrieve (key,value) and update global data
 */
int config_parse(struct radclock_config *conf, u_int32_t *mask, int is_daemon, int *ns)
{
	struct _key *pkey = keys;
	int codekey=0;
	int nf;		// count time servers found
	char *c;
	char line[MAXLINE];
	char key[MAXLINE];
	char value[MAXLINE];
	FILE* fd = NULL;

	/* Check input */
	if (conf == NULL) {
		verbose(LOG_ERR, "Configuration structure is NULL.");
		return 0;
	}
	
	/* Config and log files */
	if (strlen(conf->conffile) == 0)
	{
		if ( is_daemon )
			strcpy(conf->conffile, DAEMON_CONFIG_FILE);
		else
			strcpy(conf->conffile, BIN_CONFIG_FILE);
	}

	if (strlen(conf->logfile) == 0)
	{
		if ( is_daemon )
			strcpy(conf->logfile, DAEMON_LOG_FILE);
		else
			strcpy(conf->logfile, BIN_LOG_FILE);
	}



	/* The file can't be opened. Ether it doesn't exist yet or I/O error.
	 * Note: this writes the file, but doesn't update handle->conf with the
	 * values in it!
	 */
	fd = fopen(conf->conffile, "r");
	if (!fd) {

		// Modify umask so that the file can be read by all after being written
		umask(022);
		verbose(LOG_NOTICE, "Did not find configuration file: %s.", conf->conffile);
		fd = fopen(conf->conffile, "w+");
		if (!fd) {
			verbose(LOG_ERR, "Cannot write configuration file: %s. ", conf->conffile);
			umask(027);
			return 0;
		}

		write_config_file(fd, keys, NULL, 1);	// single server only
		*ns = 1;

		fclose(fd);
		verbose(LOG_NOTICE, "Writing configuration file.");
		//verbose(LOG_NOTICE, "    Time server          : %s", conf->time_server);

		// Reposition umask
		umask(027);
		return 1;
	}

	// The configuration file exists, parse it and update default values
	have_all_tmpqual = 0; // ugly
	nf = 0;
	while ((c=fgets(line, MAXLINE, fd)) != NULL) {

		// Start with a reset of the value to avoid mistakes
		strcpy(value, "");

		// Extract key and values from the conf file
		if ( !(extract_key_value(c, key, value) ))
			continue;

		// Identify the key and update config values
		codekey = match_key(pkey, key);

		// This line is not a configuration line
		if (codekey < 0)
			continue;

		/* Update in case we actually retrieved a value, update nf */
		if ( strlen(value) > 0 )
			update_data(conf, mask, codekey, value, &nf, *ns, 0);
	}
	fclose(fd);
	/* Trim any memory containing old servers, and update handle
	   TODO: works in rehash case when must remember former ns value, and also servers? to remap databases? */
	if (nf < *ns)
		conf->time_server = realloc(conf->time_server, nf*MAXLINE);
	*ns = nf;



	/* Ok, the file has been parsed, but the version may be outdated. Since
	 * we just parsed the configuration, we can produce an up-to-date version.
	 */
	if ( strcmp(conf->radclock_version, PACKAGE_VERSION) != 0 ) {

		// Modify umask so that the file can be read by all after being written
		umask(022);
		fd = fopen(conf->conffile, "w");
		if ( !fd ) {
			verbose(LOG_ERR, "Cannot update configuration file: %s.", conf->conffile);
			umask(027);
			return 0;
		}
		write_config_file(fd, keys, conf, *ns);
		fclose(fd);
		umask(027);

		// Adjust version
		strcpy(conf->radclock_version, PACKAGE_VERSION);
		verbose(LOG_NOTICE, "Updated the configuration file to the current package version");
	}

	/* Check command line arguments and config file for exclusion. 
	 * - If running as a daemon, refuse to read input raw or ascii file
	 */
	if ( is_daemon ) {
		if ( (strlen(conf->sync_in_ascii) > 0) || ( strlen(conf->sync_in_pcap) > 0) )
			verbose(LOG_WARNING, "Running as a daemon. Live capture only.");
		// Force the input to be a live device
		strcpy(conf->sync_in_ascii,"");
		strcpy(conf->sync_in_pcap,"");
	}

	if (conf->is_tn && conf->is_ocn) {
		verbose(LOG_ERR, "Configuration cannot have both is_tn and is_ocn enabled.");
		exit(1);
	}

	/* Set the mapping between time_server index and NTC server ids
	 * Mapping value is -1 if there is no match
	 */

	if (conf->time_server_ntc_mapping) {
		free(conf->time_server_ntc_mapping);
		free(conf->time_server_ntc_indexes);
		conf->time_server_ntc_mapping = 0;
		conf->time_server_ntc_indexes = 0;
	}

	conf->time_server_ntc_mapping = malloc(sizeof(int)*(*ns));
	conf->time_server_ntc_count = 0;
	// Match time_servers with NTCs, Non matches get a value of -1
	for (int time_server_id = 0; time_server_id < *ns; time_server_id++)
	{
		conf->time_server_ntc_mapping[time_server_id] = -1; // Set mapping initially to -1
		char *this_s = conf->time_server + time_server_id * MAXLINE;
		for (int NTC_id = 0; NTC_id < MAX_NTC; NTC_id ++)
		{
			if (strcmp(conf->ntc[NTC_id].domain, this_s) == 0) { // If match is found set the NTC id
				conf->time_server_ntc_mapping[time_server_id] = conf->ntc[NTC_id].id;
				conf->time_server_ntc_count += 1;
				conf->time_server_ntc_indexes = realloc(conf->time_server_ntc_indexes, sizeof(int)*conf->time_server_ntc_count);
				conf->time_server_ntc_indexes[conf->time_server_ntc_count -1] = time_server_id;
				verbose(LOG_INFO, "Configuration matched time_server:%s mapping for NTC:%d",
				    this_s, conf->ntc[NTC_id].id);
			}
		}
	}


	return 1;
}




/* Print configuration to logfile */
void config_print(int level, struct radclock_config *conf, int ns)
{
	verbose(level, "RADclock - configuration summary");
	verbose(level, "radclock version     : %s", conf->radclock_version);
	verbose(level, "Server TN            : %s", labels_bool[conf->is_tn]);
	verbose(level, "Server OCN           : %s", labels_bool[conf->is_ocn]);
	verbose(level, "Configuration file   : %s", conf->conffile);
	verbose(level, "Log file             : %s", conf->logfile);
	verbose(level, "Verbose level        : %s", labels_verb[conf->verbose_level]);
	verbose(level, "Client sync          : %s", labels_sync[conf->synchro_type]);
	verbose(level, "Server IPC           : %s", labels_bool[conf->server_ipc]);
	verbose(level, "Server Telemetry     : %s", labels_bool[conf->server_telemetry]);
	verbose(level, "Server SHM           : %s", labels_bool[conf->server_shm]);
	verbose(level, "SHM DAG Client       : %s", conf->shm_dag_client);
	verbose(level, "Server NTP           : %s", labels_bool[conf->server_ntp]);
	verbose(level, "Server VM_UDP        : %s", labels_bool[conf->server_vm_udp]);
	verbose(level, "Server XEN           : %s", labels_bool[conf->server_xen]);
	verbose(level, "Server VMWARE        : %s", labels_bool[conf->server_vmware]);
	verbose(level, "Upstream NTP port    : %d", conf->ntp_upstream_port);
	verbose(level, "Downstream NTP port  : %d", conf->ntp_downstream_port);
	verbose(level, "Adjust system FFclock: %s", labels_bool[conf->adjust_FFclock]);
	verbose(level, "Adjust system FBclock: %s", labels_bool[conf->adjust_FBclock]);
	verbose(level, "Polling period       : %d", conf->poll_period);
	verbose(level, "TSLIMIT              : %.9lf", conf->metaparam.TSLIMIT);
	verbose(level, "SKM_SCALE            : %.9lf", conf->metaparam.SKM_SCALE);
	verbose(level, "RateErrBound         : %.9lf", conf->metaparam.RateErrBOUND);
	verbose(level, "BestSKMrate          : %.9lf", conf->metaparam.BestSKMrate);
	verbose(level, "offset_ratio         : %d", conf->metaparam.offset_ratio);
	verbose(level, "plocal_quality       : %.9lf", conf->metaparam.plocal_quality);
	verbose(level, "path_scale           : %.9lf", conf->metaparam.path_scale);
	verbose(level, "Initial phat         : %lg", conf->phat_init);
	verbose(level, "Host asymmetry       : %lf", conf->asym_host);
	verbose(level, "Network asymmetry    : %lf", conf->asym_net);
	verbose(level, "Host name            : %s", conf->hostname);
	for (int s=0; s<ns; s++)
		verbose(level, "Time server          : %d %s", s, conf->time_server + s*MAXLINE);
	verbose(level, "Interface            : %s", conf->network_device);
	verbose(level, "pcap sync input      : %s", conf->sync_in_pcap);
	verbose(level, "ascii sync input     : %s", conf->sync_in_ascii);
	verbose(level, "pcap sync output     : %s", conf->sync_out_pcap);
	verbose(level, "ascii sync output    : %s", conf->sync_out_ascii);
	verbose(level, "ascii clock output   : %s", conf->clock_out_ascii);

	for (int i=0; i<MAX_NTC; i++)
		if ( strlen(conf->ntc[i].domain) > 0 )
			verbose(level, "NTC                  : %d %s", conf->ntc[i].id, conf->ntc[i].domain);

}

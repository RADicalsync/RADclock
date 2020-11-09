/*
 * Copyright (C) 2006-2011 Julien Ridoux and Darryl Veitch
 * Copyright (C) 2013-2020, Darryl Veitch <darryl.veitch@uts.edu.au>
 *
 * This file is part of the radclock program.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */


/*
 * This program illustrate the use of functions related to capture network
 * traffic and producing kernel timestamps based on the RADclock.
 *
 * The RADclock daemon should be running for this example to work correctly.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
 
//#include <pcap.h>			// includes <net/bpf.h>
#include <net/bpf.h>			// not needed if just using tsmode presets in radclock.h

/* RADclock API and RADclock packet capture API */
#include <radclock.h>		// includes <pcap.h>

/* For testing, but is outside the library, don't need for basic use */
#include <radclock-private.h>
#include "kclock.h"              // struct ffclock_estimate, get_kernel_ffclock

#define BPF_PACKET_SIZE   108


void
usage(char *progname)
{
	fprintf(stdout, "%s: [-v] [-L] [-i <interface>] -o <filename> [-t <tsmode preset>]"
	" [-c <custom tsmode code>] [-f <pkt filter string>] \n"
					, progname);
	fflush(stdout);
	exit(-1);
}


pcap_t *
initialise_pcap_device(char * network_device, char * filtstr)
{
	pcap_t * phandle;
	struct bpf_program filter;
	char errbuf[PCAP_ERRBUF_SIZE];  /* size of error message set in pcap.h */

	/* pcap stuff, need to get access to global RADclock data */
	/* Use pcap to open a bpf device */
	if (network_device == NULL) {
		//if network device has not been specified by user
		if ((network_device = pcap_lookupdev(errbuf)) == NULL) {
			/* Find free device */
			fprintf(stderr,"Failed to find free device, pcap says: %s\n",errbuf);
			exit(EXIT_FAILURE);
		}
		else
			fprintf(stderr, "Found device %s\n", network_device);
	}

	/* No promiscuous mode, timeout on BPF = 5ms */
	phandle = pcap_open_live(network_device, BPF_PACKET_SIZE, 0, 5, errbuf);
	if (phandle == NULL) {
		fprintf(stderr, "Open failed on live interface, pcap says: %s\n", errbuf);
		exit(EXIT_FAILURE);
	}

	/* No need to test broadcast addresses */
	if (filtstr == NULL) {
		fprintf(stdout, "Using default filter:  port 123\n");
		if (pcap_compile(phandle, &filter, "port 123", 0, 0) == -1) {
			fprintf(stdout, "pcap filter compiling failure, pcap says: %s\n", pcap_geterr(phandle));
			exit(EXIT_FAILURE);
		}
	} else {
		fprintf(stdout, "using provided filter:  %s\n", filtstr);
		if (pcap_compile(phandle, &filter, filtstr, 0, 0) == -1) {
			fprintf(stdout, "pcap filter compiling failure, pcap says: %s\n", pcap_geterr(phandle));
			exit(EXIT_FAILURE);
		}
	}
	if (pcap_setfilter(phandle,&filter) == -1 )  {
		fprintf(stderr, "pcap filter setting failure, pcap says: %s\n",
				pcap_geterr(phandle));
		exit(EXIT_FAILURE);
	}
	return phandle;
}


/* This routine interprets the contents of the timeval-typed ts field from
 * the pcap header according to the BPF_T_FORMAT options, and converts the
 * timestamp to a long double.
 * The options where the tv_usec field have been used to store the fractional
 * component of time (MICROTIME (mus, the original tval format), and
 * and NANOTIME (ns)) both fit within either 32 or 64 bits tv_usec field,
 * whereas BINTIME can only work in the 64 bit case.
 */
static void
ts_format_to_double(struct timeval *pcapts, int tstype, long double *timestamp)
{
	static int limit = 0;					// limit verbosity to once-only
	//long double two32 = 4294967296.0;	// 2^32
	
	*timestamp = pcapts->tv_sec;
	
	switch (BPF_T_FORMAT(tstype)) {
	case BPF_T_MICROTIME:
		if (!limit) fprintf(stdout, "converting timestamp as a microtime\n");
		*timestamp += 1e-6 * pcapts->tv_usec;
		break;
	case BPF_T_NANOTIME:
		if (!limit) fprintf(stdout, "converting timestamp as a nanotime\n");
		*timestamp += 1e-9 * pcapts->tv_usec;
		break;
	case BPF_T_BINTIME:
		if (!limit) fprintf(stdout, "converting timestamp as a bintime\n");
		if (sizeof(struct timeval) < 16)
			fprintf(stderr, "Looks like timeval frac field is only 32 bits, ignoring!\n");
		else {
//			*timestamp += ((long double)((long long unsigned)pcapts->tv_usec))/( two32*two32 );
			bintime_to_ld(timestamp, (struct bintime *) pcapts);	// treat the ts as a bintime
		}
		break;
	}
	
	limit++;
}



int
main (int argc, char *argv[])
{

	/* The user radclock structure */
	struct radclock *clock;
	radclock_local_period_t	 lpm = RADCLOCK_LOCAL_PERIOD_OFF;

	/* Pcap */
	pcap_t *pcap_handle = NULL; /* pcap handle for interface */
	char *network_device = NULL; /* points to physical device, eg xl0, em0, eth0 */

	/* Captured packet */
	struct pcap_pkthdr header;           /* The header that pcap gives us */
	const u_char *packet;                /* The actual packet */
	vcounter_t vcount;
	struct timeval tv;
	long double currtime;
	int ret;
	char * output_file = NULL;
	FILE * output_fd = NULL;
	char * filtstr = NULL;

	/* SHM */
	struct radclock_shm *shm;
	unsigned int gen = 0;
	
	/* Packet timestamp capture mode. Defaults are specified here, each can be
	 * overwritten by a command line argument.
	 */
	pktcap_tsmode_t tsmode = PKTCAP_TSMODE_FFNATIVECLOCK;
	u_int custom = 0x3012;	// like PKTCAP_TSMODE_FFNATIVECLOCK but returns bintime

	/* Misc */
	int verbose_flag = 0;
	int ch;
	long count_pkt = 0;
	long count_pkt_null = 0;
	long count_err_ns = 0;

	long double tvdouble=0 , cdiff, frac;

	/* parsing the command line arguments */
	while ((ch = getopt(argc, argv, "vo:i:t:c:Lf:")) != -1)
		switch (ch) {
		case 'o':
			output_file = optarg;
			break;
		case 'v':
			verbose_flag = 1;
			break;
		case 'i':    //  interface to monitor for reference TSs if not default
			network_device = optarg;
			break;
		case 't':    //  tsmode selection amoung presets, to override default
			tsmode = (pktcap_tsmode_t) strtol(optarg,NULL,10);	// base10
			break;
		case 'c':    //  custom bpf tsmode selection, to override default
			custom = (u_int) strtol(optarg,NULL,16);	// base16
			break;
		case 'L':    //  local period mode to ON, else the default is OFF
 			lpm = RADCLOCK_LOCAL_PERIOD_ON;
			fprintf(stdout, "Activating plocal refinement if available.\n");
 			break;
		case 'f':    //  bpf filter string to pass to pcap
			filtstr = optarg;
			break;
		default:
			usage(argv[0]);
	}

	if ( !output_file ) {
		usage(argv[0]);
		return 1;
	}


	/* Initialise the pcap capture device */
	pcap_handle = initialise_pcap_device(network_device, filtstr);

	/* Initialize the clock handle */
	clock = radclock_create();
	if (!clock) {
		fprintf(stderr, "Couldn''t create a radclock");
		return -1;
	}
	printf("------------------- Initializing your radclock  ----------------\n");
	radclock_init(clock);   // sets clock->ipc_shm  here
	radclock_set_local_period_mode(clock, &lpm);
	printf("----------------------------------------------------------------\n");

	radclock_register_pcap(clock, pcap_handle);
	
	
	
	/* tsmode is typically set by use of the pktcap_tsmode presets described in
	 * radclock.h   The classic choice is PKTCAP_TSMODE_FFNATIVECLOCK, which
	 * uses the native FFclock.  This is the normal radclock, which is the most
	 * accurate, but has small jumps after radlock daemon updates and so is not
	 * strictly monotonic. It returns a UTC timestamp limited to microsecond
	 * resolution (timeval format), as well as the raw counter timestamp that
	 * the radclock API can act on.
	 * Alternative presets can be set on the command line using -t .
	 *
	 * If the preset is PKTCAP_TSMODE_CUSTOM (using -t 100), then instead of a
	 * preset the detailed bpf level tsmode is set directly via `custom'.
	 * The default value of custom, 0x3012, is similar to FFNATIVECLOCK, except
	 * the timestamp format is upped to bintime, giving maximum resolution.
	 * Alternative values can be set on the command line using -c .
	 */
	printf("------------------- Setting the packet tsmode ------------------\n");
	//custom = BPF_T_MICROTIME | BPF_T_FFC | BPF_T_NORMAL | BPF_T_FFNATIVECLOCK;	// this is PKTCAP_TSMODE_FFNATIVECLOCK
	//custom = BPF_T_NANOTIME  | BPF_T_FFC | BPF_T_NORMAL | BPF_T_FFNATIVECLOCK;  // same but upping to ns resolution
	pktcap_set_tsmode(clock, pcap_handle, tsmode, custom);
	
	printf("------------------- Checking what it was finally set to ---------\n");
	pktcap_get_tsmode(clock, pcap_handle, &tsmode);
	printf("----------------------------------------------------------------\n");


	

	/* Open output file to store output */
	if ((output_fd = fopen(output_file,"w")) == NULL) {
		fprintf(stderr, "Open failed on stamp output file- %s\n", output_file);
		exit(-1);
	} else {  /* write out comment header describing data saved */
		fprintf(output_fd, "%% Log of packet timestamps\n");
		fprintf(output_fd, "%% column 1: RAW vcount underlying the timestamp\n");
		fprintf(output_fd, "%% column 2: Time - Absolute time from kernel using chosen clock\n");
		fprintf(output_fd, "%% column 3: Time - RADclock Absolute clock at that raw timestamp\n");
		fprintf(output_fd, "%% column 4: diff:  kernel - radclock [s] \n");
		fprintf(output_fd, "%% column 5: Fractional part of diff expressed in mus\n");
		fprintf(output_fd, "%% column 6: Generation number of shm used\n");
		fflush(output_fd);
	}



	/* Will timestamp whatever NTP packets appear no port 123.
	 * If the daemon is the only process sending such pkts out, you should see
	 * pairs of packets every pollperiod seconds (the request, and its response)
	 */
	fprintf(stdout, "Starting sniffing packets using supplied filter \n");


	/* Collect and store both timestamps for each pkt */
	while (1) {

		/* Block until the next packet, return the raw and ts timestmaps */
		ret = radclock_get_packet(clock, pcap_handle, &header,
				(unsigned char **) &packet, &vcount, &tv);
				
		if (ret) {
			fprintf(stderr, "WARNING: problem getting packet\n");
			return 0;
		}

		/* Quickly check the current RADdata generation within the SHM */
		shm = (struct radclock_shm *)clock->ipc_shm;
		if (shm)
			gen = shm->gen;
		else
			fprintf(stderr," Warning, SHM is down.\n");
			
			
		/* Read the corresponding UTC time (with maximum resolution as a long
		 * double `currtime'), based on the given vcount raw timestamp, from
		 * your radclock.
		 */
		radclock_vcount_to_abstime(clock, &vcount, &currtime);
		
		/* Convert tv to double for comparison */
		ts_format_to_double(&tv, custom, &tvdouble);
		cdiff = (currtime - tvdouble);
		frac = cdiff - (int) cdiff;
		
		/* Output the kernel's absolute timestamp, the raw, radclocks's abs time */
		fprintf(output_fd, "(%llu)  %ld.%.6llu  %.9Lf (diff %3.9Lf %3.1Lf mus) (shmgen: %u) \n",
							(long long unsigned) vcount,
							tv.tv_sec, (long long unsigned)tv.tv_usec,
							currtime,
							cdiff, 1e6*frac,
							gen);
							
		fflush(output_fd);
		
		
		if (verbose_flag) {
			fprintf(stdout, "(%llu)  %ld.%.6llu  %.9Lf (diff %3.9Lf %3.1Lf mus) (shmgen: %u) \n",
							(long long unsigned) vcount,
							tv.tv_sec, (long long unsigned)tv.tv_usec,
							currtime,
							cdiff, 1e6*frac,
							gen);
		}
		
		/* Collect some statistics */
		count_pkt++;
		if ( fabs(1e9*frac) > 1 ) count_err_ns++;		// keep track of #errors worse than 1ns

		if (verbose_flag) {
			if ( count_pkt%4 == 0 ) {
				fprintf(stdout, "Number of packets sniffed : %ld  \t #(null, ns-err) = (%ld %ld) \n",
									count_pkt, count_pkt_null, count_err_ns);
				fflush(stdout);
			}
		} else {
			fprintf(stdout, "\r Number of packets sniffed : %ld  \t #(null, ns-err) = (%ld %ld)",
									count_pkt, count_pkt_null, count_err_ns);
			fflush(stdout);
		}
	}
}

/*
 * Copyright (C) 2020, Darryl Veitch <darryl.veitch@uts.edu.au>
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
 
#include <pcap.h>			// includes <net/bpf.h>
//#include <net/bpf.h>			// not needed if just using tsmode presets in radclock.h

/* RADclock API and RADclock packet capture API */
#include <radclock.h>		// includes <pcap.h>

/* For testing, but is outside the library */
#include <radclock-private.h>
#include "kclock.h"              // struct ffclock_estimate, get_kernel_ffclock

#define BPF_PACKET_SIZE   108


void
usage(char *progname)
{
	fprintf(stdout, "%s: [-v] [-L] [-i <interface>] -o <filename> [-t <tsmode preset>]"
	" [-c <custom tsmode code>]  [-f <pkt filter string>] \n"
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
// wrong arguments, must fix		if ((network_device = pcap_findalldevs(&phandle, errbuf)) == NULL) {
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



int
main (int argc, char *argv[])
{

	/* The user radclock structure */
	const int nc = 6;		// number of radclocks to compare
	struct radclock *clock, *c[nc];
	radclock_local_period_t	 lpm = RADCLOCK_LOCAL_PERIOD_OFF;

	/* Pcap */
	pcap_t *pcap_handle = NULL; /* pcap handle for interface */
	pcap_t *ph[nc];
	char *network_device = NULL; /* points to physical device, eg xl0, em0, eth0 */
	int i;
	
	/* Captured packet */
	struct pcap_pkthdr header;           /* The header that pcap gives us */
	const u_char *packet;                /* The actual packet */
	vcounter_t v[nc];
	struct timeval tv;
	long double  t[nc], ti[nc];
	char * output_file = NULL;
	FILE * output_fd = NULL;
	char * filtstr = NULL;

	/* SMS */
	//struct radclock_sms *sms;

	/* FFdata */
	//struct ffclock_estimate cest;
	
	
	/* packet timestamp capture mode */
	//pktcap_tsmode_t tsmode;
	// detailed bpf level tsmode used only if tsmode = PKTCAP_TSMODE_CUSTOM
	//u_int custom = 0x3012;	// like PKTCAP_TSMODE_FFNATIVECLOCK but returns bintime

	/* Misc */
	int verbose_flag = 0;
	int ch;
	long count_pkt = 0;


	/* parsing the command line arguments */
	while ((ch = getopt(argc, argv, "vo:i:t:c:lf:")) != -1)
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
			//tsmode = (pktcap_tsmode_t) strtol(optarg,NULL,10);	// base10
			break;
		case 'c':    //  custom bpf tsmode selection, to override default
			//custom = (u_int) strtol(optarg,NULL,16);	// base16
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


	/* Initialize the vector of clocks */
	for (i=0; i<nc; i++)	{
	
		printf("--------------- Initializing radclock %d  ----------------\n", i);
		clock = radclock_create();
		if (!clock) {
			fprintf(stderr, "Couldn''t create a radclock");
			return -1;
		}
		radclock_init(clock);
		radclock_set_local_period_mode(clock, &lpm);
		
		/* Initialise the dedicated pcap capture device */
		pcap_handle = initialise_pcap_device(network_device, filtstr);
		radclock_register_pcap(clock, pcap_handle);

		c[i] = clock;
		ph[i] = pcap_handle;
	
	}

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
	u_int cus[nc];
	// FreeBSD choice
	cus[0] = 0x3012;			// FFnative; UTC !FAST; bintime
	cus[1] = 0x2012;			// FFmono;   UTC !FAST; bintime
	cus[2] = 0x0012;			// SYSclock; UTC !FAST; bintime
	cus[3] = 0x1012;			// FBclock;  UTC !FAST; bintime
	cus[4] = 0x2212;			// FFmono;   Upt !FAST; bintime
	cus[5] = 0x1212;			// FBclock;  Upt !FAST; bintime
	// Linux choice
	cus[0] = 0x3011;			// FFnative; UTC !FAST; nanotime
	cus[1] = 0x2011;			// FFmono;   UTC !FAST;    "
	cus[2] = 0x0011;			// SYSclock; UTC !FAST;    "
	cus[3] = 0x1011;			// FBclock;  UTC !FAST;    "
	cus[4] = 0x2211;			// FFmono;   Upt !FAST;    "
	cus[5] = 0x1211;			// FBclock;  Upt !FAST;    "


	printf("------------------- Setting the packet tsmodes ------------------\n");
	for (i=0; i<nc; i++)	{
		pktcap_set_tsmode(c[i], ph[i], PKTCAP_TSMODE_CUSTOM, cus[i]);
//		printf("------------------- Checking what it was finally set to ---------\n");
//		pktcap_get_tsmode(c[i], ph[i], &tsm[i]);
//		printf("----------------------------------------------------------------\n");
	}




	/* Open output file to store output */
	if ((output_fd = fopen(output_file,"w")) == NULL) {
		fprintf(stderr, "Open failed on stamp output file- %s\n", output_file);
		exit(-1);
	} else {  /* write out comment header describing data saved */
		fprintf(output_fd, "%% Log of packet comparison timestamps, relative to capture FF origin.\n");
		fprintf(output_fd, "%% column 1: RAW timestamp\n");
		fprintf(output_fd, "%% column 2: FFclock UTC\n");
		fprintf(output_fd, "%% column 3: FBclock UTC\n");
		fprintf(output_fd, "%% column 4: diff:  FF - FB \n");
		fprintf(output_fd, "%% column 5: FF Uptime\n");
		fprintf(output_fd, "%% column 6: FB Uptime\n");
		fprintf(output_fd, "%% column 7: diff:  FF - FB \n");
		fflush(output_fd);
	}


	/* Will timestamp whatever NTP packets appear no port 123.
	 * If the daemon is the only process sending such pkts out, you should see
	 * pairs of packets every pollperiod seconds (the request, and its response)
	 */
	fprintf(stdout, "\n Starting sniffing packets using supplied filter \n");


	/* Provide an initial dump to all clocks to provide the origin baseline,
	 * then compare FF and FB side by side relative to adjusted origins given below
	 */
	printf("----------- Timestamp read check for baseline ---------\n");
	for (i=0; i<nc; i++)	{
		radclock_get_packet(c[i], ph[i], &header, (unsigned char **) &packet, &v[i], &tv);
		ts_format_to_double(&tv, cus[i], &ti[i]);		// convert tv to ld using chosen tsmode
		fprintf(output_fd,	" (%llu)	%3.9Lf [s]\n", (long long unsigned)v[i], ti[i]);
		fprintf(stdout, 		" (%llu)	%3.9Lf [s]\n", (long long unsigned)v[i], ti[i]);
	}
	if ( ! (v[0] == v[1] && v[2] == v[3] && v[0] == v[3]) )	printf(" raw timestamps differ !! \n");
	//vi = v[0];
				
	printf("-------------------------------------------------------\n");
	fprintf(stdout, " (raw)\t\t   UTC:  FF   FFmono   SYS    FB     (FFmono-FF SYS-FF FB-FF) \t   UP:  FFmono  FB  (FB-FFmono) \n");
	printf("-------------------------------------------------------\n");

	while (1) {

		for (i=0; i<nc; i++)	{
			radclock_get_packet(c[i], ph[i], &header, (unsigned char **) &packet, &v[i], &tv);
			ts_format_to_double(&tv, cus[i], &t[i]);		// convert tv to ld using chosen tsmode
			/* Make relative to FF values on initial packet to improve readability */
			if (i<4)
				t[i] -= ti[0];		// UTC origin set to first nativeFF value
			else
				t[i] -= ti[4];		// UPtime origin set to first monoFF values
			}
		
		/* Check bpf timestamping architecture:  all taps should get same raw timestamp */
		if ( ! (v[0] == v[1] && v[2] == v[3] && v[0] == v[3]) )
				printf(" raw timestamps differ !! \n");
				
		/* Compare FF to FB, both as UTC, and as Uptime clocks */
		fprintf(output_fd, " %llu %3.4Lf %3.4Lf %3.4Lf %3.4Lf %3.3Lf %3.4Lf %3.4Lf "
								 "   %3.4Lf %3.4Lf %3.5Lf\n",
					(long long unsigned)(v[0]), 	t[0], t[1], t[2], t[3], t[1]-t[0], t[2]-t[0], t[3]-t[0],
															t[4], t[5], t[5]-t[4] );
		fflush(output_fd);

		if (verbose_flag || count_pkt < 2)
			fprintf(stdout, " [%llu] \t %3.4Lf %3.4Lf %3.4Lf %3.4Lf (%3.3Lf [ms] %3.4Lf %3.4Lf) \t"
								 	 " | %3.4Lf %3.4Lf (%3.5Lf)\n",
					(long long unsigned)(v[0]), 	t[0], t[1], t[2], t[3], 1000*(t[1]-t[0]), t[2]-t[0], t[3]-t[0],
															t[4], t[5], t[5]-t[4] );
		
		
		/* Collect some statistics */
		count_pkt++;
		
		if (!verbose_flag && count_pkt >= 2) {
			fprintf(stdout, "\r Number of packets sniffed : %ld", count_pkt);
			fflush(stdout);
		}
	}
	
}

/*
 * Copyright (C) 2020 The RADclock Project (see AUTHORS file)
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
 * This program illustrate the use of functions related to the capture of network
 * traffic, and producing kernel timestamps based on the RADclock daemon via the
 * associated kernel FFclock.
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
 
#include <pcap.h>               // includes <net/bpf.h>
#include <net/bpf.h>            // not needed if just using tsmode presets in radclock.h

/* RADclock API and RADclock packet capture API */
#include <radclock.h>           // includes <pcap.h>

/* For testing, but is outside the library */
#include <radclock-private.h>
#include "kclock.h"             // struct ffclock_estimate, get_kernel_ffclock

#define BPF_PACKET_SIZE   108
#define PCAP_TIMEOUT   15       // [ms]  Previous value of 5 caused huge delays

/* bpf based KV detection */
#define	BPF_KV             2.	// Define symbol, with default value
#ifdef	BPF_T_FFC           	// KV≥3 detection
#define	BPF_KV             3.
#endif
#ifdef	BPF_T_SYSC         	    // KV3bis detection
#define	BPF_KV             3.5
#endif


void
usage(char *progname)
{
	fprintf(stdout, "%s: [-v] [-L] [-i <interface>] -o <filename> [-t <tsmode preset>]"
	    " [-c <custom tsmode code>]  [-f <pkt filter string>] \n", progname);
	fflush(stdout);
	exit(-1);
}


/* Use pcap to open a bpf device
 * If a device not specified by caller, then look for one.
 */
pcap_t *
initialise_pcap_device(char * network_device, char * filtstr)
{
	pcap_t * phandle;
	struct bpf_program filter;
	pcap_if_t *alldevs = NULL;
	char errbuf[PCAP_ERRBUF_SIZE];  /* size of error message set in pcap.h */

	/* Look for available device if needed */
	if (network_device == NULL) {
		//if ((network_device = pcap_lookupdev(errbuf)) == NULL) { // deprecated
		if (pcap_findalldevs(&alldevs, errbuf) == -1) {
			printf("Error in pcap_findalldevs: %s\n", errbuf);
			exit(EXIT_FAILURE);
		}
		if (alldevs == NULL) {
			fprintf(stderr,"Failed to find free device, pcap says: %s\n", errbuf);
			exit(EXIT_FAILURE);
		} else {
			network_device = alldevs->name;
			fprintf(stderr, "Found device %s\n", network_device);
		}
	}

	/* No promiscuous mode */
	phandle = pcap_open_live(network_device, BPF_PACKET_SIZE, 0, PCAP_TIMEOUT, errbuf);
	if (phandle == NULL) {
		fprintf(stderr, "Open failed on live interface, pcap says: %s\n", errbuf);
		exit(EXIT_FAILURE);
	}
	if (alldevs)
		pcap_freealldevs(alldevs);    // network_device no longer needed

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
		fprintf(stderr, "pcap filter setting failure, pcap says: %s\n", pcap_geterr(phandle));
		exit(EXIT_FAILURE);
	}
	return phandle;
}




/* The main goal of this program is to check that the FFclock normal timestamp
 * conversions, agree with conversions made using a radclock in userland.
 * Also enables the tsmode to be varied to test different branches.
 * Performs other checks, including comparing the parameters held in the sms
 * with those held as global FFdata, and using an older set to see that that is
 * the cause of mismatches.  Thus this tests the FF code, as well as whether
 * the FFdata and sms RADdata are in sync.
 *
 * NOTE on sniffing of the daemon's NTP packets:
 * i) In this case we are sniffing the same pkts used to form stamps fed to the daemon.
 * The update_ffcount member of the FFdata is none other than the response raw
 * timestamp Tf, so that it is in fact possible for a raw timestamp T of a pkt,
 * to be interpreted using FFdata with the same update_ffcount=T, in which case
 * update_gap = 0  in the printouts below.
 *  Of course this is only possible if
 *  in fact the SAMe raw pkt timestamp is given to both listening applications.
 *  Happily this is occurring in Linux, and also occurs in FreeBSD thanks to the
 *  FFclock bpf snapshot system.
 * However this is unlikely, as it can only occur if this T reached the daemon,
 * was fully processed, and the FFdata update pushed, all before T is translated
 * to a normal timestamp in the kernel for the sniffing application.
 * Typically, T is translated using the FFdata before this, which will
 * be a poll-period behind.  Hence in the printouts below  update_gap_sec  is
 * typically very close to poll_period.
 * ii) If looking at dev.c verbosity, see the two sets of identical outputs
 *     one from radclock, one from the sniffer.
 *
 * test_FFclocks -i ens33 -o test.out  -v                         // listen using defaults  (port 123)
 * test_FFclocks -i ens33 -o test.out  -v         -t 100          // default custom filter of 3012
 * test_FFclocks -i ens33 -o test.out  -v -f icmp                 // use ICMP so can control traffic with ping
 * test_FFclocks -i ens33 -o test.out  -v -f icmp -t 100 -c 3011  // nanotime: hope to see error go sub 1mus --> sub 1ns
 * test_FFclocks -i ens33 -o test.out  -v -f icmp -t 100 -c 3013  // test if Format=NONE obeyed
 * test_FFclocks -i ens33 -o test.out  -v -f icmp -t 100 -c 3111  // test if FAST makes a difference
 * test_FFclocks -i ens33 -o test.out  -v -f icmp -t 100 -c 4111  // watch drift of FFdiff
 * test_FFclocks -i ens33 -o test.out  -v         -t 100 -c 3013  // test with complexities of daemon pkts sniffed
 *
 */
int
main (int argc, char *argv[])
{

	/* The user radclock structure */
	struct radclock *clock;
	radclock_local_period_t	 lpm = RADCLOCK_LOCAL_PERIOD_OFF;

	/* Pcap */
	pcap_t *pcap_handle = NULL;  /* pcap handle for interface */
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

	/* SMS */
	struct radclock_sms *sms;
	vcounter_t RADgen = 0;
	unsigned int gen = 0;

	/* FFdata */
	struct ffclock_estimate cest;
	vcounter_t FFgen = 0;

	/* packet timestamp capture mode */
	pktcap_tsmode_t tsmode = PKTCAP_TSMODE_FFNATIVECLOCK;
	// detailed bpf level tsmode used only if tsmode = PKTCAP_TSMODE_CUSTOM
	u_int custom = 0x3012;	// like PKTCAP_TSMODE_FFNATIVECLOCK but returns bintime

	/* Misc */
	int verbose_flag = 0;
	int ch;
	long gen_diff;
	long update_gap;
	double update_gap_sec;
	long count_pkt = 0;
	long count_pkt_null = 0;
	long count_gen = 0;
	long count_err_ns = 0;

	long double tvdouble=0, cdiff, frac;

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
			tsmode = (pktcap_tsmode_t) strtol(optarg,NULL,10);	// base10
			break;
		case 'c':    //  custom bpf tsmode selection, to override default
			custom = (u_int) strtol(optarg,NULL,16);	// base16
			break;
		case 'l':    //  local period mode to ON, else the default is OFF
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


	/* Initialize the clock handle */
	clock = radclock_create();
	if (!clock) {
		fprintf(stderr, "Couldn''t create a radclock");
		return -1;
	}
	printf("------------------- Initializing your radclock  ----------------\n");
	radclock_init(clock);   // sets clock->ipc_sms  here, but never again, if dies?  can refresh? test run PID ??
	radclock_set_local_period_mode(clock, &lpm);
	printf("----------------------------------------------------------------\n");

	/* Initialise the pcap capture device */
	pcap_handle = initialise_pcap_device(network_device, filtstr);
	radclock_register_pcap(clock, pcap_handle);

	/* bpf KV check */
	fprintf(stderr, "\n KV bpf version detected as: %3.1f \n\n", BPF_KV);

	/* The bpf tstype is typically set by use of the pktcap_tsmode presets described in
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
	//custom = BPF_T_MICROTIME | BPF_T_FFC | BPF_T_NORMAL | BPF_T_FFNATIVECLOCK;  // emulated PKTCAP_TSMODE_FFNATIVECLOCK
	//custom = BPF_T_NANOTIME  | BPF_T_FFC | BPF_T_NORMAL | BPF_T_FFNATIVECLOCK;  // same but upping to ns resolution
	pktcap_set_tsmode(clock, pcap_handle, tsmode, custom);

	printf("------------------- Checking what it was finally set to ---------\n");
	pktcap_get_tsmode(clock, pcap_handle, &tsmode);
	printf("----------------------------------------------------------------\n");

	// tsmode now set.  Reuse variable custom as a tstype argument for  ts_format_to_double  below
//	if (tsmode == PKTCAP_TSMODE_FFNATIVECLOCK)
//		custom = 0x3010;	// since in this case, the default custom value is not activated and shouldn't apply
//		custom = BPF_T_MICROTIME | BPF_T_FFC | BPF_T_NORMAL | BPF_T_FFNATIVECLOCK;	// not available in any .h ?
	fprintf(stdout, "custom now = 0x%04x\n", custom);


	/* Open output file to store output */
	if ((output_fd = fopen(output_file,"w")) == NULL) {
		fprintf(stderr, "Open failed on stamp output file- %s\n", output_file);
		exit(-1);
	} else {  /* write out comment header describing data saved */
		fprintf(output_fd, "   Shared raw    [FFgen-raw,[s],gen_diff]        bpf tv      "
		    "      radclock currtime       (     currtime  -  tv      ) \n");
//		fprintf(output_fd, "%% Log of packet timestamps\n");
//		fprintf(output_fd, "%% column 1: Time - Absolute time from kernel using chosen clock\n");
//		fprintf(output_fd, "%% column 2: RAW vcount underlying the timestamp\n");
//		fprintf(output_fd, "%% column 3: Time - RADclock Absolute clock at that raw timestamp\n");
//		fflush(output_fd);
	}

	/* We do a bit of warm up to heat the IPC socket on slow systems */
//	for (ret = 0; ret < 5; ret++ ) {
//		radclock_get_last_changed(clock, &vcount);
//		//fprintf(stdout, "(%llu) \n",	(long long unsigned) vcount );
//		sleep(1);
//	}



	/* Will timestamp whatever NTP packets appear on port 123.
	 * If the daemon is the only process sending such pkts out, you should see
	 * pairs of packets every pollperiod seconds (the request, and its response)
	 */
	//fprintf(stdout, " size of long double is %d\n", sizeof(long double));
	fprintf(stdout, "Starting sniffing packets using supplied filter \n");


	/* Collect and store both timestamps for each pkt */
	while (1) {

		/* Block until the next packet, return the raw and ts timestamps */
		ret = radclock_get_packet(clock, pcap_handle, &header,
		    (unsigned char **) &packet, &vcount, &tv);

		/* Quickly check the current FFdata generation */
		get_kernel_ffclock(clock, &cest);
		FFgen = cest.update_ffcount;
		
		/* Quickly check the current RADdata generation within the SMS */
		sms = (struct radclock_sms *)clock->ipc_sms;
		if (sms) {
			RADgen = SMS_DATA(sms)->last_changed;
			gen = sms->gen;
		} else
			fprintf(stderr," Warning, SMS is down.\n");

		if (ret) {
			fprintf(stderr, "WARNING: problem getting packet\n");
			return 1;
		}

		/* Read the corresponding UTC time (with maximum resolution as a long
		 * double `currtime'), based on the given vcount raw timestamp, from
		 * your radclock.
		 */
		radclock_vcount_to_abstime(clock, &vcount, &currtime);

		/* Perform check of conversion error due to RAD-->FF-->RAD representation
		 * of absolute time, as a `best possible' baseline for comparison of
		 * RAD versus FF-via-pcap timestamp.
		 * This tests representation issues only, not the FFclock or RAD timestamps.
		 */
		struct bintime bt;
		long double timetest;
		ld_to_bintime(&bt, &currtime);
		bintime_to_ld(&timetest, &bt);
		if ( currtime - timetest != 0 )
			fprintf(stderr, "ld conversion error found:  %3.6Lf [ns] \n", 1e9*(currtime - timetest));


		/* Generation checks and fix */
		gen_diff   = (signed long)(FFgen - RADgen);	// check that the two versions of RADdata nominally used agree
		update_gap = (signed long)(vcount - FFgen);	// gap between the vcount and the Current FFdata
//		if (update_gap == 0)			// turns out they were all indeed the same
//			fprintf(stdout, " Testing gen_diff in all dirns: vcount: %llu, FFgen: %llu, RADgen: %llu\n",
//				(long long unsigned)vcount, (long long unsigned)FFgen, (long long unsigned)RADgen);
		update_gap_sec = update_gap * SMS_DATA(sms)->phat;		// value in [sec]
		/* FFdata has been updated since the version used to create tv and vcount */
		if (update_gap < 0)  // unlikely, if happens, will see -ve update_gap printed below
			read_RADabs_UTC(SMS_DATAold(sms), &vcount, &currtime, PLOCAL_ACTIVE);  // overwrite from old SMS to find a match

		/* Convert tv to double for comparison */
		ts_format_to_double(&tv, custom, &tvdouble);    // custom currently ignored if Linux
		cdiff = (currtime - tvdouble);
		frac = cdiff - (int) cdiff;

		/* Based on the same raw, output the kernel's absolute timestamp, and radclocks's abs time */
		fprintf(output_fd, "(%llu) [%ld %1.4lf %ld] %ld.%.6llu  %.9Lf (diff %3.9Lf %3.1Lf mus %3.3Lf ns) (smsgen: %u) \n",
		    (long long unsigned) vcount, update_gap, update_gap_sec, gen_diff,
		    tv.tv_sec, (long long unsigned)tv.tv_usec,
		    currtime, cdiff, 1e6*frac, 1e9*frac,
		    gen);
//		fprintf(output_fd,  "%ld.%.6d %llu %.9Lf\n", tv.tv_sec, (int)tv.tv_usec,
//				(long long unsigned)vcount, currtime);
		/* Repeat with old SMS to see if get a match there if spot a problem, note gen missed at end so u can c it*/
		if ( fabs(1e9*frac) > 1 ) {		// 1ns trigger
//		if ( fabs(1e6*frac) > 1 ) {		// 1mus trigger hack
			count_err_ns++;
			read_RADabs_UTC(SMS_DATAold(sms), &vcount, &currtime, PLOCAL_ACTIVE);
			cdiff = (currtime - tvdouble);		// *** this overwrites the results seen in verbose output! but without the OLDCHECK flagging it !
			frac = cdiff - (int) cdiff;
			fprintf(output_fd, "(%llu) [%ld %1.4lf %ld] %ld.%.6llu  %.9Lf (diff %3.9Lf %3.1Lf mus %3.3Lf ns) OLDCHECK\n",
			    (long long unsigned) vcount, update_gap, update_gap_sec, gen_diff,
			    tv.tv_sec, (long long unsigned)tv.tv_usec,
			    currtime, cdiff, 1e6*frac, 1e9*frac);
		}
		fflush(output_fd);


/***************      Verbose does to stdout    ***********/
		if (verbose_flag) {
			if ( count_pkt%16 == 0 )
				fprintf(stdout, "   Shared raw        FFgen      [FFgen-raw,[s],gen_diff]    bpf tv      "
				    "  radclock currtime       (     currtime  -  tv      ) \n");
			fprintf(stdout, "(%llu) %llu [%ld %1.4lf %ld] %ld.%.6llu  %.9Lf (diff %3.9Lf %3.1Lf mus %3.3Lf ns) (smsgen: %u) \n",
			    (long long unsigned) vcount, (long long unsigned) FFgen, update_gap, update_gap_sec, gen_diff,
			    tv.tv_sec, (long long unsigned)tv.tv_usec,
			    currtime, cdiff, 1e6*frac, 1e9*frac,
			    gen);
//			/* Repeat with old SMS to see if get a match there if spot a problem */
////			if ( fabs(1e9*frac) > 1 ) {		// 1ns trigger
//			if ( fabs(1e6*frac) > 1 ) {		// 1mus trigger
//					//count_err_ns++;
//					read_RADabs_UTC(SMS_DATAold(sms), &vcount, &currtime, PLOCAL_ACTIVE);
//					cdiff = (currtime - tvdouble);
//					frac = cdiff - (int) cdiff;
//					fprintf(stdout, "(%llu) [%ld %1.4lf %ld] %ld.%.6llu  %.9Lf (diff %3.9Lf %3.1Lf mus %3.3Lf ns) \n",
//							(long long unsigned) vcount, update_gap, update_gap_sec, gen_diff,
//							tv.tv_sec, (long long unsigned)tv.tv_usec,
//							currtime, cdiff, 1e6*frac, 1e9*frac);
//			}
		}

		/* Collect some statistics */
		count_pkt++;
		if ( gen_diff != 0 ) count_gen++;
		
		if (verbose_flag) {
			if ( count_pkt%16 == 0 ) {
				fprintf(stdout, "Number of packets sniffed : %ld  \t #(null, badgen, ns-err) = (%ld %ld %ld) \n",
				    count_pkt, count_pkt_null, count_gen, count_err_ns);
				fflush(stdout);
			}
		} else {
			fprintf(stdout, "\r Number of packets sniffed : %ld  \t #(null, gen-err, ns-err) = (%ld %ld %ld)",
			    count_pkt, count_pkt_null, count_gen, count_err_ns);
			fflush(stdout);
		}
	}
}

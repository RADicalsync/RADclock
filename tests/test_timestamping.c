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

#include "../config.h"

#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include "radclock.h"
#include "radclock-private.h"
#include "logger.h"

#define BPF_PACKET_SIZE   170
#define PCAP_TIMEOUT   15       // [ms]  Previous value of 5 caused huge delays


int
main(int argc, char **argv)
{
	struct radclock *clock;
	struct bpf_program fp;	/* The compiled filter expression */
	pktcap_tsmode_t tsmode;
	pcap_t *phandle;
	//char fltstr[120];
	char errbuf[PCAP_ERRBUF_SIZE];	/* Error string */
	char *if_name;
	int err;


	clock = radclock_create();
	radclock_init(clock);

	/* Look for and open a PCAP device */
	pcap_if_t *alldevs = NULL;
	if (pcap_findalldevs(&alldevs, errbuf) == -1) {
		printf("Error in pcap_findalldevs: %s\n", errbuf);
		exit(EXIT_FAILURE);
	}
	if (alldevs == NULL) {
		fprintf(stderr, "Failed to find free device, pcap says: %s\n", errbuf);
		exit(EXIT_FAILURE);
	} else {
		if_name = alldevs->name;
		fprintf(stderr, "Found device %s\n", if_name);
	}

	/* No promiscuous mode */
	if ((phandle = pcap_open_live(if_name, BPF_PACKET_SIZE, 0, PCAP_TIMEOUT, errbuf)) == NULL) {
		fprintf(stderr, "Open failed on live interface, pcap says: %s\n", errbuf);
		return (1);
	}
	pcap_freealldevs(alldevs);		// if_name no longer needed



	/* No need to test broadcast addresses */
	err = pcap_compile(phandle, &fp, "port 123", 0, 0);
	if (err == -1) {
		fprintf(stderr, "pcap filter compiling failure, pcap says: %s\n",
			pcap_geterr(phandle));
		return (1);
	}

	/* Set filter on pcap handler */
	err = pcap_setfilter(phandle, &fp);
	if (err == -1 ) {
		fprintf(stderr, "pcap filter setting failure, pcap says: %s\n",
			pcap_geterr(phandle));
		return (1);
	}

	tsmode = PKTCAP_TSMODE_SYSCLOCK;
	err = pktcap_set_tsmode(clock, phandle, tsmode, 0);
	if (err == -1 ) {
		fprintf(stderr, "FAILED: pktcap_set_tsmode SYSCLOCK\n");
		return (1);
	} else
		fprintf(stderr, "SUCCESS: pktcap_set_tsmode SYSCLOCK\n");


	tsmode = PKTCAP_TSMODE_RADCLOCK;
	err = pktcap_set_tsmode(clock, phandle, tsmode, 0);
	if (err == -1 ) {
		fprintf(stderr, "FAILED: pktcap_set_tsmode RADCLOCK\n");
		return (1);
	}
		fprintf(stderr, "SUCCESS: pktcap_set_tsmode RADCLOCK\n");

	tsmode = PKTCAP_TSMODE_FFNATIVECLOCK;
	err = pktcap_set_tsmode(clock, phandle, tsmode, 0);
	if (err == -1 ) {
		fprintf(stderr, "FAILED: pktcap_set_tsmode FFNATIVECLOCK\n");
		return (1);
	}
		fprintf(stderr, "SUCCESS: pktcap_set_tsmode FFNATIVECLOCK\n");

	err = pktcap_get_tsmode(clock, phandle, &tsmode);
	if (err == -1 ) {
		fprintf(stderr, "pktcap_get_tsmode failed\n");
		return (1);
	}

	return (0);
}

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

#ifndef _RAWDATA_H
#define _RAWDATA_H



/**
 * Raw data element.
 * A container for raw data being timestamped by the kernel and passed
 * to us. Example are a NTP packet captured via LibPcap or a PPS
 * timestamped.
 * Structure is designed to be versatile enough to handle raw data of
 * different nature in a doubled chain list.
 * IMPORTANT: the design is lock free. I could have created a mutex to
 * ensure the producer/consumer don't mess up, but the point of this 
 * buffer chain is to ensure low level capture function (e.g. the 
 * callback passed to pcap_loop()) returns as fast as possible. So we 
 * don't want to block and wait for the mutex to be unlocked.
 */

typedef enum { 
	RD_UNKNOWN,
	RD_TYPE_SPY,
	RD_TYPE_NTP,		/* Handed by libpcap */
	RD_TYPE_1588,
	RD_TYPE_PPS,
} rawdata_type_t;


/*
 * Raw data structure specific to the SYP capture mode.
 * So far looks like a bidir stamp, but may change in the future.
 */
struct rd_spy_stamp {
	vcounter_t Ta;
	struct timeval Tb;
	struct timeval Te;
	vcounter_t Tf;
};


/*
 * Raw data structure specific to NTP and PIGGY capture modes.
 * Very libpacp oriented.
 */
struct rd_pcap_pkt {
	vcounter_t vcount;				/* vcount stamp for this buddy */
	struct pcap_pkthdr pcap_hdr;	/* The PCAP header */
	void* buf;						/* Actual data, contains packet */
};


/*
 * Raw data bundle. Holds actual raw_data and deals with link list
 * and light locking
 */
struct raw_data_bundle {
	struct raw_data_bundle *next;	// next points toward the Head
	int read;							// Have I been read? i.e. ready to be freed?
	rawdata_type_t type;			/* If we know the type, let's put it there */
	union rd_t {
		struct rd_pcap_pkt rd_pkt;
		struct rd_spy_stamp rd_spy;
	} rd;
};

struct raw_data_queue {
	struct raw_data_bundle *rdb_start;
	struct raw_data_bundle *rdb_end;

	// XXX arbiter between insert_rdb_in_list and free_and_cherry_pick.	Should
	// not need a lock, but there is quite some messing around if hammering NTP
	// control packets
	pthread_mutex_t rdb_mutex;
};

#define RD_PKT(x) (&((x)->rd.rd_pkt))
#define RD_SPY(x) (&((x)->rd.rd_spy))



int capture_raw_data(struct radclock_handle *handle);

int deliver_rawdata_pcap(struct radclock_handle *handle,
		struct radpcap_packet_t *pkt, vcounter_t *vcount);

int deliver_rawdata_spy(struct radclock_handle *handle, struct stamp_t *stamp);

#endif

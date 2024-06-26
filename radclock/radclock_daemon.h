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

#ifndef _RADCLOCK_DAEMON_H
#define _RADCLOCK_DAEMON_H


#ifdef VC_FMT
#undef VC_FMT
#endif
#if defined (__LP64__) || defined (__ILP64__)
#define VC_FMT "lu"
#else
#define VC_FMT "llu"
#endif



// TODO : should provide methods for modify this?
typedef enum {
	RADCLOCK_SYNC_NOTSET,
	RADCLOCK_SYNC_DEAD,
	RADCLOCK_SYNC_LIVE,
} radclock_runmode_t;

typedef enum {
	RADCLOCK_UNIDIR,
	RADCLOCK_BIDIR,
} radclock_syncalgo_mode_t;


struct radclock_handle;



// TODO: This network protocol code could be factored?

/* NTP Protocol related stuff */
struct radclock_ntp_client {
	int socket;
	struct sockaddr_in s_to;
	struct sockaddr_in s_from;
};

struct radclock_ntp_server {
	int burst;
	uint32_t refid;
	unsigned int stratum;
	double minRTT;          // RTThat to the server we sync to
	double rootdelay;       // Cumulative RTThat from top of stratum hierarchy
	double rootdispersion;  // Cumulative clock error from top of stratum hierarchy
};



/* Virtual machine environment data
 * Mode run in, push and pull struct radclock_data
 */
struct radclock_vm
{
	/* A handle to the xenstore */
	void *store_handle;

	/* Socket for vm_udp push / pull (the vm_server maintains its own socket) */
	int sock;
	struct sockaddr_in server_addr;
};

#define VM_MAGIC_NUMBER    31051978
#define VM_REQ_RAD_DATA    1
#define VM_REQ_RAD_ERROR   2

struct vm_request {
	unsigned int magic_number;
	unsigned int version;
	unsigned int request_type;
};

struct vm_reply {
	unsigned int magic_number;
	unsigned int version;
	unsigned int reply_type;
	union {
		struct radclock_data rad_data;
		struct radclock_error rad_error;
	};
};



struct radclock_handle {

	/* Library radclock structure */
	struct radclock *clock;

	/* Points to an array of RADclock parameter sets, one per server */
	struct radclock_data *rad_data;
	/* Corresponding RADclock error estimate sets */
	struct radclock_error *rad_error;

	/* Virtual Machine management */
	struct radclock_vm rad_vm;

	/* Protocol related state on the daemon client side (NTP case) */
	struct radclock_ntp_client *ntp_client;
	/* Protocol related state on the daemon's server side (NTP case) */
	struct radclock_ntp_server *ntp_server;
	
	/* Raw data capture buffer for libpcap */
	struct raw_data_queue *pcap_queue;
	/* Raw data capture buffer for 1588 error queue */
	struct raw_data_queue *ieee1588eq_queue;

	/* Common data for the daemon */
	int is_daemon;
	radclock_runmode_t run_mode;
	char hostIP[16];    // IP address of the host

	/* UNIX signals */
	unsigned int unix_signal;    // for recording of HUP and TERM
	struct FIFO *alarm_buffer;   // buffer for sIDs of packet SIGALRMs
	
	/* Output file descriptors */
	FILE* stampout_fd;
	FILE* matout_fd;

	/* Threads */
	pthread_t threads[8];
	int pthread_flag_stop;
	pthread_mutex_t globaldata_mutex;

	/* Configuration */
	struct radclock_config *conf;

	/* Algo output */
	radclock_syncalgo_mode_t syncalgo_mode;
	//void *algo_output;   // Defined as void* since not part of the library

	/* Stamp source */
	void *stamp_source;    // Defined as void* since not part of the library

	/* Points to an array of Synchronisation algodata, one per server */
	void *algodata;

	/* Multiple server management */
	int nservers;         // number of servers
	int pref_sID;         // ID ("array" index) of preferred RADclock
	vcounter_t pref_date; // raw time of last pref change

	/* Server trust status word: 1 bit per server, denoting
	 *   0: server is trusted
	 *   1: an issue has been detected, use with caution
	 * Mask to access server sID's bit is  mask = (1ULL << sID)
	 */
	uint64_t servertrust;

};


/* These macros facilitate access to the data for the s-th server.
 *    SMACRO(h,s,..)    macro where the s the ID of the desired server
 *     MACRO(h,..  )    macro where s = h->pref_sID, the preferred server
 */
#define SNTP_CLIENT(h,s) (&(h->ntp_client[s]))
#define SNTP_SERVER(h,s) (&(h->ntp_server[s]))
#define NTP_CLIENT(h) (SNTP_CLIENT(h,h->pref_sID))
#define NTP_SERVER(h) (SNTP_SERVER(h,h->pref_sID))

#define SRAD_DATA(h,s)  (&(h->rad_data[s]))      // ptr to data of server s
#define SRAD_ERROR(h,s) (&(h->rad_error[s]))
#define RAD_DATA(h)  (SRAD_DATA(h,h->pref_sID))  // ptr to data of preferred s
#define RAD_ERROR(h) (SRAD_ERROR(h,h->pref_sID))

#define RAD_VM(h) (&(h->rad_vm))

/* New: based on r pointing to the desired rad_data */
#define ADD_STATUS(r,y) ((r)->status = (r)->status | y )
#define DEL_STATUS(r,y) ((r)->status = (r)->status & ~y )
#define HAS_STATUS(r,y) (((r)->status & y) == y )


/* Functions that handle the push and pull of virtual machine data */
int init_vm(struct radclock_handle *handle);
int push_data_vm(struct radclock_handle *handle);
int receive_loop_vm(struct radclock_handle *handle);
int update_ipc_shared_memory(struct radclock_handle *handle);
void read_clocks(struct radclock_handle *handle, struct timeval *sys_tv,
    struct timeval *rad_tv, vcounter_t *counter);

#endif

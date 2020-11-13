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
	RADCLOCK_SYNC_LIVE } radclock_runmode_t;

typedef enum {
	RADCLOCK_UNIDIR,
	RADCLOCK_BIDIR} radclock_syncalgo_mode_t;


struct radclock_handle;



// TODO: This network protocol code could be factored?

/*
 * NTP Protocol related stuff
 */
struct radclock_ntp_client {
	int socket;
	struct sockaddr_in s_to;
	struct sockaddr_in s_from;
};

struct radclock_ntp_server {
	int burst;
	uint32_t refid;
	unsigned int stratum;
	double minRTT; 			/* RTThat to the server we sync to */
	double rootdelay;			/* Cumulative RTThat from top of stratum hierarchy */
	double rootdispersion;	/* Cumulative clock error from top of stratum hierarchy */
};



/*
 * Virtual machine environment data
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

#define VM_MAGIC_NUMBER		31051978
#define VM_REQ_RAD_DATA		1
#define VM_REQ_RAD_ERROR	2

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

	/* Clock data, the real stuff */
	struct radclock_data rad_data;

	/* Clock error estimates */
	struct radclock_error rad_error;

	/* Virtual Machine management */
	struct radclock_vm rad_vm;

	/* Protocol related state on the daemon client side (NTP case) */
	struct radclock_ntp_client		*ntp_client;
	
	/* Protocol related state on the daemon's server side (NTP case) */
	struct radclock_ntp_server		*ntp_server;
	
	/* Raw data capture buffer for libpcap */
	struct raw_data_queue *pcap_queue;

	/* Raw data capture buffer for 1588 error queue */
	struct raw_data_queue *ieee1588eq_queue;

	/* Common data for the daemon */
	int is_daemon;
	radclock_runmode_t 		run_mode;
	char hostIP[16];		// IP address of the host

	/* UNIX signals */
	unsigned int unix_signal;

	/* Output file descriptors */
	FILE* stampout_fd;
	FILE* matout_fd;

	/* Threads */
	pthread_t threads[8];		/* TODO: quite ugly implementation ... */
	int	pthread_flag_stop;
	pthread_mutex_t globaldata_mutex;
	int wakeup_checkfordata;
	pthread_mutex_t wakeup_mutex;
	pthread_cond_t wakeup_cond;

	/* Configuration */
	struct radclock_config *conf;

	/* Algo output */
	radclock_syncalgo_mode_t syncalgo_mode;
	void *algo_output; 	/* Defined as void* since not part of the library */

	/* Stamp source */
	void *stamp_source; /* Defined as void* since not part of the library */

	/* Synchronisation Peers. Peers are of different nature (bidir, oneway) will
	 * cast
	 */
	void *active_peer;
	
};


#define NTP_CLIENT(x) (x->ntp_client)
#define NTP_SERVER(x) (x->ntp_server)
#define RAD_DATA(x) (&(x->rad_data))
#define RAD_ERROR(x) (&(x->rad_error))
#define RAD_VM(x) (&(x->rad_vm))

#define ADD_STATUS(x,y) (RAD_DATA(x)->status = RAD_DATA(x)->status | y )
#define DEL_STATUS(x,y) (RAD_DATA(x)->status = RAD_DATA(x)->status & ~y )
#define HAS_STATUS(x,y) ((RAD_DATA(x)->status & y ) == y )


/*
 * Functions which handle the push and pull of virtual machine data
 */
int init_vm(struct radclock_handle *handle);
int push_data_vm(struct radclock_handle *handle);
int receive_loop_vm(struct radclock_handle *handle);
int update_ipc_shared_memory(struct radclock_handle *handle);
void read_clocks(struct radclock_handle *handle, struct timeval *sys_tv,
	struct timeval *rad_tv, vcounter_t *counter);
#endif

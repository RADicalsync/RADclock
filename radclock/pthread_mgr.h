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

#ifndef _PTHREAD_MGR_H
#define _PTHREAD_MGR_H


/*
 * Pthread description for their IDs
 * Check we have enough space allocated in radclock structure
 * for the pthread IDs
 */
#define PTH_NONE			0
#define PTH_DATA_PROC		1
#define PTH_TRIGGER			2
#define PTH_NTP_SERV		3
#define PTH_FIXEDPOINT		4
#define PTH_VM_UDP_SERV		5
#define PTH_TELEMETRY_CON	6
#define PTH_SHM_CON			7


/*
 * Flags to signal threads they have to commit suicide
 */
#define PTH_DATA_PROC_STOP		0x0000001
#define PTH_TRIGGER_STOP		0x0000010
#define PTH_NTP_SERV_STOP		0x0000100
#define PTH_FIXEDPOINT_STOP		0x0001000
#define PTH_VM_UDP_SERV_STOP	0x0010000
#define PTH_TELEMETRY_CON_STOP	0x0100000
#define PTH_SHM_CON_STOP		0x1000000
#define PTH_STOP_ALL			(PTH_DATA_PROC_STOP | PTH_TRIGGER_STOP | \
		PTH_FIXEDPOINT_STOP | PTH_NTP_SERV_STOP | PTH_VM_UDP_SERV_STOP | \
		PTH_TELEMETRY_CON_STOP | PTH_SHM_CON_STOP)


/*
 *  Threads starters
 */
int start_thread_DATA_PROC(struct radclock_handle *handle);
int start_thread_TRIGGER(struct radclock_handle *handle);
int start_thread_NTP_SERV(struct radclock_handle *handle);
int start_thread_VM_UDP_SERV(struct radclock_handle *handle);
int start_thread_FIXEDPOINT(struct radclock_handle *handle);
int start_thread_TELEMETRY_CON(struct radclock_handle *handle);
int start_thread_SHM(struct radclock_handle *handle);


/*
 * Threads starters init functions
 */
void* thread_data_processing(void *c_handle);
void* thread_trigger(void *c_handle);
void* thread_ntp_server(void *c_handle);
void* thread_vm_udp_server(void *c_handle);
void* thread_telemetry_consumer(void *c_handle);
void* thread_shm(void *c_handle);


int trigger_work(struct radclock_handle *handle);
int process_stamp(struct radclock_handle *handle, struct bidir_peer *peer);


/*
 * Threads initialisation
 */
void init_thread_signal_mgt();
int trigger_init(struct radclock_handle *handle);


/*
 * Posix Timers and Alarm routines
 */
void catch_alarm(int sig);
#ifdef HAVE_POSIX_TIMER 
int set_ptimer(timer_t timer, float next, float period);
int assess_ptimer(timer_t timer, float period);
#else
int set_itimer(float next, float period);
int assess_itimer(float period);
#endif


#endif

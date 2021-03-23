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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <net/ethernet.h>

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <fcntl.h>

#include "../config.h"
#include "radclock.h"
#include "radclock-private.h"
#include "radclock_daemon.h"
#include "verbose.h"
#include "sync_history.h"
#include "sync_algo.h"
#include "pthread_mgr.h"
#include "proto_ntp.h"
#include "misc.h"
#include "jdebug.h"
#include "config_mgr.h"

struct dag_cap {
	long double cn_send_xmit_ts;
	long double cn_send_xmit_dag_ts;
	long double cn_rcv_xmit_dag_ts;
    l_fp server_reply_org;
    struct in_addr ip;
};

void 
find_matching_shm_packets(struct dag_cap dag_msg, struct radclock_shm_ts * SHM_stamps, int queue_size)
{
    uint64_t id = ((uint64_t) ntohl(dag_msg.server_reply_org.l_int)<<32) |((uint64_t) ntohl(dag_msg.server_reply_org.l_fra));
    char server_ipaddr[INET6_ADDRSTRLEN];
    strcpy( server_ipaddr, inet_ntoa(dag_msg.ip));
    // verbose(LOG_INFO, "Attempting match on ip %s", server_ipaddr);
    for (int i =0; i < queue_size; i++)
    {
        // if (SHM_stamps[i].id == id && strcmp(server_ipaddr, SHM_stamps[i].server_ipaddr) != 0)
        //     verbose(LOG_INFO, "Mis match on ip %s %s ", server_ipaddr, SHM_stamps[i].server_ipaddr);

        if (SHM_stamps[i].id == id && strcmp(server_ipaddr, SHM_stamps[i].server_ipaddr) == 0)
        {
            // Make temp copy of variable incase it gets overriden while reading
            struct radclock_shm_ts radclock_shm_ts_cpy = SHM_stamps[i];

            // Double check that the data didn't change while making temp copy
            if (radclock_shm_ts_cpy.id == id && strcmp(server_ipaddr, radclock_shm_ts_cpy.server_ipaddr) == 0)
            {
                // We are now working on data in local function scope so we don't need to worry about thread race conditions

                verbose(LOG_INFO, "Found matching SHM packet from DAG capture %lu ICN:%d %s", radclock_shm_ts_cpy.id, radclock_shm_ts_cpy.icn_id, server_ipaddr);
                return;
            }
        }
    }
    verbose(LOG_INFO, "No match found matching SHM packet from DAG capture %lu (%d)", SHM_stamps[0].id, queue_size);

}

/*
 * Integrated thread and thread-work function for SHM module
 */
void 
thread_shm(void *c_handle)
{

    int socket_desc, c, new_socket;
    struct sockaddr_in server, client;
    struct dag_cap dag_msg;
    struct radclock_handle *handle = (struct radclock_handle *) c_handle;
	unsigned int socklen = sizeof(struct sockaddr_in);

    if (!handle->conf->is_cn)
    {
		verbose(LOG_WARNING, "SHM: Disabled - This radclock must set is_cn to on");
        return;
    }

    socket_desc = socket(AF_INET , SOCK_DGRAM , 0);
    if (socket_desc == -1)
	{
		verbose(LOG_ERR, "SHM: Could not create socket");
        return ;
	}

	/* 
	 * Eliminates "ERROR on binding: Address already in use" error. 
	 */
	int optval = 1;
	setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR,
		   (const void *)&optval, sizeof(int));

    // int flags = fcntl(socket_desc, F_GETFL, 0) | O_NONBLOCK;
    // if (fcntl(socket_desc, F_SETFL, flags) == 1)
    // {
	// 	printf("SHM: Failed to set socket to non blocking");
    // }

    //Prepare the sockaddr_in structure
    bzero((char *)&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons( 5671 );
	
	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		verbose(LOG_ERR, "SHM: bind socket failed");
        return ;
	}
    
    struct sockaddr_in client_add;
    while (1)
    {
        // Connected to DAG system
        int ret = recvfrom(socket_desc,
        &dag_msg, sizeof(struct dag_cap), MSG_WAITALL,
        &client_add, &socklen);
        if (ret == sizeof(struct dag_cap) )
        {
            if (strcmp(handle->conf->shm_dag_client, inet_ntoa(client_add.sin_addr)) == 0)
                find_matching_shm_packets(dag_msg, handle->SHM_stamps, handle->SHM_stamp_write_id);
            else
            {
                verbose(LOG_WARNING, "SHM packet received from unknown IP - Potiential attacker (%s)\n", inet_ntoa(client_add.sin_addr));    
            }                    
        }
    }

    if (new_socket>=0)
        close(new_socket);
    if (socket_desc>=0)
        close(socket_desc);

}


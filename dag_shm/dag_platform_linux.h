/*
 * LEGAL NOTICE: This source code: 
 * 
 * a) is a proprietary to Endace Technology Limited, a New Zealand company,
 *    and its suppliers and licensors ("Endace"). You must not copy, modify,
 *    disclose or distribute any part of it to anyone else, except as expressly
 *    permitted in the software license agreement provided with this
 *    software, or with the prior written authorisation of Endace Technology
 *    Limited; 
 *   
 * b) may also be part of inventions that are protected by patents and 
 *    patent applications; and   
 * 
 * c) is (C) copyright to Endace, 2013. All rights reserved, except as
 *    expressly granted under the software license agreement referred to
 *    in clause (a) above.
 *
 */


/*
 * Copyright (c) 2005 Endace Technology Ltd, Hamilton, New Zealand.
 * All rights reserved.
 *
 * This source code is proprietary to Endace Technology Limited and no part
 * of it may be redistributed, published or disclosed except as outlined in
 * the written contract supplied with this product.
 *
 * $Id: a60fa8e  from: Fri Nov 13 16:58:02 2015 +1300  author: Anthony Coddington $
 */

#ifndef DAG_PLATFORM_LINUX_H
#define DAG_PLATFORM_LINUX_H

#if defined(__linux__)

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif /* HAVE_CONFIG_H */

/* POSIX headers. */
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/un.h>
#include <regex.h>
#include <semaphore.h>
#include <syslog.h>

#if defined(HAVE_LINUX_IF_TUN_H)
#include <linux/if_tun.h>
#endif

/* C Standard Library headers. */
#include <inttypes.h>
/*C++ has its own bool so we don't need this if using a C++ compiler. */
#ifndef __cplusplus
#include <stdbool.h>
#endif


#ifndef PRIu64
#define PRIu64 "llu"
#endif /* PRIu64 */

#ifndef PRId64
#define PRId64 "lld"
#endif /* PRId64 */

#ifndef PRIx64
#define PRIx64 "llx"
#endif /* PRIx64 */

#ifndef INLINE
#define INLINE inline
#endif /* INLINE */


/* libedit header default. */
#ifndef HAVE_EDITLINE
#define HAVE_EDITLINE 0
#endif /* HAVE_EDITLINE */


/* Byteswap code. */
#include <byteswap.h>

/* Check IP checksum (for IP packets). */
#define IN_CHKSUM(IP) ip_sum_calc_linux((uint8_t *)(IP))


#include <linux/types.h>

#ifndef s6_addr
#include <linux/in6.h>
#endif /* s6_addr */


/* Routines. */
uint16_t ip_sum_calc_linux(uint8_t* buff);


#endif /* __linux__ */

#endif /* DAG_PLATFORM_LINUX_H */ 

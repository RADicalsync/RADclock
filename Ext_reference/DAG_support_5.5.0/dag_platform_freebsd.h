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
 * $Id: a6bd287  from: Tue Aug 13 20:56:07 2013 +0000  author: Stephen Donnelly $
 */

#ifndef DAG_PLATFORM_FREEBSD_H
#define DAG_PLATFORM_FREEBSD_H

#if defined(__FreeBSD__)

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif /* HAVE_CONFIG_H */

/* CAUTION: FreeBSD is much pickier about header file ordering than Linux! */

/* POSIX headers. */
#include <sys/types.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/un.h>
#include <regex.h>
#include <semaphore.h>
#include <syslog.h>

/* C Standard Library headers. */
#include <inttypes.h>
#include <stdbool.h>


#ifndef PRIu8
#define PRIu8 "u"
#endif /* PRIu8 */

#ifndef PRId8
#define PRId8 "d"
#endif /* PRId8 */

#ifndef PRIu32
#define PRIu32 "u"
#endif /* PRIu32 */

#ifndef PRId32
#define PRId32 "d"
#endif /* PRId32 */

#ifndef PRIu64
#define PRIu64 "llu"
#endif /* PRIu64 */

#ifndef PRId64
#define PRId64 "lld"
#endif /* PRId64 */

#ifndef PRIx64
#define PRIx64 "llx"
#endif /* PRIx64 */

#if (__FreeBSD__ == 4)
#ifndef PRIxPTR
#define PRIxPTR "x"
#endif /* PRIxPTR */
#endif /* FreeBSD 4 */

#ifndef INLINE
#define INLINE inline
#endif /* INLINE */


/* libedit header default. */
#ifndef HAVE_EDITLINE
#define HAVE_EDITLINE 0
#endif /* HAVE_EDITLINE */


/* Byteswap code. */
#if defined(BYTESWAP)
#include <byteswap.h>
#else
#include <machine/endian.h>
#if defined(__byte_swap_int)
#define bswap_64(x) __bswap64(x)
#else
#define bswap_64(x)                               \
    (__extension__                                \
     ({ union { __extension__ uint64_t __ll;      \
     uint32_t __l[2]; } __w, __r;                 \
     __w.__ll = (x);                              \
     __r.__l[0] = __byte_swap_long (__w.__l[1]);  \
     __r.__l[1] = __byte_swap_long (__w.__l[0]);  \
     __r.__ll; }))
#endif /* __byte_swap_int */

/* redefine __byte_swap_long fnction if not defined for free bsd 6*/
#ifndef __byte_swap_long
#define __byte_swap_long __bswap32
#endif

//define bswap_32(x) __byte_swap_long(x)
/* added custome bswap_32 function because some freeBSD 64 bit version of bsd having trouble with it*/
uint32_t bswap_32(uint32_t x);

#endif /* BYTESWAP */

#include <math.h>
#if (__FreeBSD_version < 803000)
#define log2(x) (log(x) / M_LN2) 
#endif /* log2 */

/* Check IP checksum (for IP packets). */
#include <machine/in_cksum.h>

uint16_t ip_sum_calc_freebsd(uint8_t* buff);
#define IN_CHKSUM(IP) ip_sum_calc_freebsd((uint8_t *)IP)

#endif /* __FreeBSD__ */


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* DAG_PLATFORM_FREEBSD_H */ 

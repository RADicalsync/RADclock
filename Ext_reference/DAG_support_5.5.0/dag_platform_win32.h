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

#ifndef DAG_PLATFORM_WIN32_H
#define DAG_PLATFORM_WIN32_H

#if defined(_WIN32)

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <getopt.h>
#include <io.h>
#include <sys/stat.h>
#include <ipchecksum.h>
#include <strmisc.h>
#include <strtok_r.h>
#include <timeofday.h>
#include <wintypedefs.h>
#include <ethertype.h>
#include <stdbool.h>
#include <regex.h>

extern char* optarg;
extern int optind;
extern int opterr;

/* definition of IP header version 4 as per RFC 791 */
typedef struct
{ 
  u_char ip_ver_hl;      /* version and header length */ 
  u_char ip_tos;         /* type of service */ 
  short ip_len;          /* total length */ 
  u_short ip_id;         /* identification */ 
  short ip_off;          /* fragment offset field */ 
  u_char ip_ttl;         /* time to live */ 
  u_char ip_p;           /* protocol */ 
  u_short ip_cksum;      /* checksum */ 
  struct in_addr ip_src; /* source address */ 
  struct in_addr ip_dst; /* destination address */ 

} iphdr;

typedef struct
{
  unsigned short source;
  unsigned short dest;
  unsigned short len;
  unsigned short check;

} udphdr;

struct sockaddr_un {
  unsigned short sun_family;              /* address family AF_LOCAL/AF_UNIX */
  char	         sun_path[108]; /* 108 bytes of socket address     */
};


#ifndef PRIu64
#define PRIu64 "I64u"
#endif /* PRIu64 */

#ifndef PRId64
#define PRId64 "I64d"
#endif /* PRId64 */

#ifndef PRIx64
#define PRIx64 "I64x"
#endif /* PRIx64 */

#ifndef PRIxPTR
#define PRIxPTR "p"
#endif /* PRIx64 */

#ifndef PRId32
#define PRId32 "d"
#endif /* PRId32 */

#ifndef PRIu32
#define PRIu32 "u"
#endif /* PRIu32 */

#ifndef PRIu8
#define PRIu8 "u"
#endif /* PRIu8 */

// The fscanf macros for signed integers are:
#define SCNd8       "d"
#define SCNi8       "i"
#define SCNdLEAST8  "d"
#define SCNiLEAST8  "i"
#define SCNdFAST8   "d"
#define SCNiFAST8   "i"
#define SCNd16       "hd"
#define SCNi16       "hi"
#define SCNdLEAST16  "hd"
#define SCNiLEAST16  "hi"
#define SCNdFAST16   "hd"
#define SCNiFAST16   "hi"
#define SCNd32       "ld"
#define SCNi32       "li"
#define SCNdLEAST32  "ld"
#define SCNiLEAST32  "li"
#define SCNdFAST32   "ld"
#define SCNiFAST32   "li"
#define SCNd64       "I64d"
#define SCNi64       "I64i"
#define SCNdLEAST64  "I64d"
#define SCNiLEAST64  "I64i"
#define SCNdFAST64   "I64d"
#define SCNiFAST64   "I64i"
#define SCNdMAX     "I64d"
#define SCNiMAX     "I64i"

// The fscanf macros for unsigned integers are:
#define SCNo8       "o"
#define SCNu8       "u"
#define SCNx8       "x"
#define SCNX8       "X"
#define SCNoLEAST8  "o"
#define SCNuLEAST8  "u"
#define SCNxLEAST8  "x"
#define SCNXLEAST8  "X"
#define SCNoFAST8   "o"
#define SCNuFAST8   "u"
#define SCNxFAST8   "x"
#define SCNXFAST8   "X"

#define SCNo16       "ho"
#define SCNu16       "hu"
#define SCNx16       "hx"
#define SCNX16       "hX"
#define SCNoLEAST16  "ho"
#define SCNuLEAST16  "hu"
#define SCNxLEAST16  "hx"
#define SCNXLEAST16  "hX"
#define SCNoFAST16   "ho"
#define SCNuFAST16   "hu"
#define SCNxFAST16   "hx"
#define SCNXFAST16   "hX"
#define SCNo32       "lo"
#define SCNu32       "lu"
#define SCNx32       "lx"
#define SCNX32       "lX"
#define SCNoLEAST32  "lo"
#define SCNuLEAST32  "lu"
#define SCNxLEAST32  "lx"
#define SCNXLEAST32  "lX"
#define SCNoFAST32   "lo"
#define SCNuFAST32   "lu"
#define SCNxFAST32   "lx"
#define SCNXFAST32   "lX"

#define SCNo64       "I64o"
#define SCNu64       "I64u"
#define SCNx64       "I64x"
#define SCNX64       "I64X"
#define SCNoLEAST64  "I64o"
#define SCNuLEAST64  "I64u"
#define SCNxLEAST64  "I64x"
#define SCNXLEAST64  "I64X"
#define SCNoFAST64   "I64o"
#define SCNuFAST64   "I64u"
#define SCNxFAST64   "I64x"
#define SCNXFAST64   "I64X"
#define SCNoMAX     "I64o"
#define SCNuMAX     "I64u"
#define SCNxMAX     "I64x"
#define SCNXMAX     "I64X"

#define __inline__

#ifndef INLINE 
#define INLINE 
#endif /* INLINE */

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif /* STDOUT_FILENO */

#ifndef __func__
#define __func__     __FUNCTION__
#endif

#define MAX_OPEN_FILES	WIN_MAX_OPEN_FILES

/* libedit header default. */
#ifndef HAVE_EDITLINE
#define HAVE_EDITLINE 0
#endif /* HAVE_EDITLINE */

#ifndef  ETIMEDOUT 
#define ETIMEDOUT 110 /* Connection timed out */
#endif

/* Byteswap code. */
#if defined(BYTESWAP)
#include <byteswap.h>
#else
ULONGLONG bswap_64(ULONGLONG x);
UINT32 bswap_32(UINT x);


#endif /* BYTESWAP */

/* Check IP checksum (for IP packets). */
#define IN_CHKSUM(IP) ip_sum_calc_win32((uint8_t *)(IP))
#define STDIN_FILENO (fileno(stdin))

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* Routines. */
uint16_t ip_sum_calc_win32(uint8_t* buff);
void* reallocf(void *ptr, size_t size);
int32_t mrand48(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _WIN32 */

#endif /* DAG_PLATFORM_WIN32_H */ 

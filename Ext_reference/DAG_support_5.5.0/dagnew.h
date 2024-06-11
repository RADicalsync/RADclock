/*
 * Copyright (c) 2004-2014 Endace Technology Ltd, Hamilton, New Zealand.
 * All rights reserved.
 *
 * $Id: 78a9f0d  from: Thu Mar 13 23:55:34 2014 +0000  author: Stephen Donnelly $
 */

#ifndef DAGNEW_H
#define DAGNEW_H

/* DAG tree headers. */
#include "dagioctl.h"

typedef struct dagpbm
{
	volatile uint32_t   memaddr;      /* 0x00 */
	volatile uint32_t   memsize;      /* 0x04 */
	volatile uint32_t   burstthresh;  /* 0x08 */
	volatile uint32_t   segsize;      /* 0x0c */
	volatile uint32_t   wrsafe;       /* 0x10 */
	volatile uint32_t   redzone;      /* 0x14 */
	volatile uint32_t   curaddr;      /* 0x18 */
	volatile uint32_t   cs;           /* 0x1c */
	volatile uint32_t   capacity;     /* 0x20 */
	volatile uint32_t   underover;    /* 0x24 */
	volatile uint32_t   pciwptr;      /* 0x28 */
	volatile uint32_t   bursttmo;     /* 0x2c */
	volatile uint32_t   build;        /* 0x30 */
	volatile uint32_t   maxlevel;     /* 0x34 */
	volatile uint32_t   segaddr;      /* 0x38 */

} dagpbm_t;

/* PBM mkI structure. */
typedef struct dagpbm_mkI
{
	volatile uint32_t   mem_addr;
	volatile uint32_t   mem_size;
	volatile uint32_t   burst_threshold;
	volatile uint32_t   drop_counter;
	volatile uint32_t   limit_pointer;
	volatile uint32_t   limit_count;
	volatile uint32_t   record_pointer;
	volatile uint32_t   control_status;
	volatile uint32_t   unused_0;
	volatile uint32_t   unused_1;
	volatile uint32_t   burst_timeout;
	volatile uint32_t   unused_2;
	volatile uint32_t   unused_3;
	volatile uint32_t   unused_4;
	
} dagpbm_mkI_t;

/* PBM mkII structure. */
typedef struct dagpbm_mkII
{
	volatile uint32_t control_status;
	volatile uint32_t burst_threshold;
	volatile uint32_t burst_timeout;
	volatile uint32_t unused[13];

} dagpbm_mkII_t;

/* PBM mkII per stream register block */
typedef struct dagpbm_mkII_stream_block
{
	volatile uint32_t stream_status;
	volatile uint32_t mem_addr;
	volatile uint32_t mem_size;
	volatile uint32_t record_pointer;
	volatile uint32_t limit_pointer;
	volatile uint32_t limit_count;
	volatile uint32_t drop_counter;
	volatile uint32_t unused[9];
	
} dagpbm_mkII_stream_block_t;

typedef enum pbmcs
{
	DAGPBM_UNPAUSED = 0x00,
	DAGPBM_PAUSED   = 0x01,
	DAGPBM_AUTOWRAP = 0x02,
	DAGPBM_FLUSH    = 0x04,
	DAGPBM_BYTESWAP = 0x08,
	DAGPBM_SAFETY   = 0x10,
	DAGPBM_WIDEBUS  = 0x20,
	DAGPBM_SYNCL2R  = 0x40,
	DAGPBM_REQPEND  = 0x80,
	DAGPBM_LARGE    = 0x100,
	DAGPBM_DEAD     = 0x200
} pbmctrl_t;

typedef struct daggpp
{
	volatile uint32_t control;   /* 0x00 */
	volatile uint32_t reserved;  /* 0x04 */
	volatile uint32_t padword;   /* 0x08 */
	volatile uint32_t snaplen;   /* 0x0c */
	
} daggpp_t;


/*
 * SAFETY_WINDOW is the number of bytes we maintain of difference between
 * the read pointer (curaddr) and the limit pointer (wrsafe) so this last
 * one can never catch the first one.
 */
#define SAFETY_WINDOW 8

#if (defined(__APPLE__) && defined(__ppc__))
enum
{
	kDAGUserClientOpen,
	kDAGUserClientClose,
	kDAGNumberOfMethods
};

enum
{
	kDAGUserClientMmapIOM,
	kDAGUserClientMmapBuffer
};
#endif /* Mac OS X */


#define DAGN(X)      (((X)&0xf000)>>12)
#define DAG8(X)      (((X)&0xf000)==0x8000)
#define ETHERDAG(X)  (((X)&0x000f)==0x000e)

#endif /* DAGNEW_H */

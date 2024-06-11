/*
 * Copyright (c) 2004-2014 Endace Technology Ltd, Hamilton, New Zealand.
 * All rights reserved.
 *
 * $Id: 709901f  from: Fri Jun 12 17:19:23 2015 +1200  author: Alexey Korolev $
 */

#ifndef DAGIOCTL_H
#define DAGIOCTL_H

/* Version 5.3 */
#define DAG_API_VERSION_CODE 0x00050500

/*
 * Device types
 */
typedef enum dag_devtype
{
	DAGMINOR_MEM = 0,
	DAGMINOR_IOM = 1,
	DAGMINOR_MAX = 2
}dag_devtype_t;

/* DAG CARD related definitios and structures*/
#define DAG_MAX_BOARDS 64	/* The maximum number of dag cards in the system */

#define DAG_STREAM_SPACE 36
/* Possible values for card flags */
#define DAG_CARD_HW		0x1
#define DAG_CARD_VDAG		0x2
#define DAG_CARD_GUEST		0x4
#define	DAG_MEM_CONTIG		0x8
#define	DAG_MEM_MMIO		0x10
#define	DAG_DRB_MMIO		0x20
#define DAG_ID_SIZE 20
typedef struct dag_card_inf {
	uint32_t		id;			/* DAG device number 0 to DAG_MAX_BOARDS*/
	uint32_t		flags;		/* Flags */
	uint32_t		iom_size;	/* iom size */
	uint16_t		device_code;	/* PCI device ID */
	uint8_t 		brd_rev;		/* Card rev ID from PCI conf space */
	char			bus_id[DAG_ID_SIZE];	/* PCI bus ID string */
	int8_t			bus_node;				/* PCI bus NUMA node */
	char			reserved[28];			/* reserved for future needs */
} dag_card_inf_t;

/* DAG RESET alternative codes */
/*
        DAGRESET_V1_FULL = 0,
        DAGRESET_V2_RUDE = 1,
        DAGRESET_V2_LINK_DOWN = 2,
        DAGRESET_V2_HOT_BURN = 3
*/
/* DAG RESET codes */
typedef enum reset_method
{
	DAGRESET_FULL    = 0,
	DAGRESET_REBOOT  = 1,  
	DAGRESET_GEORGE  = 2, // LINK DOWN
	DAGRESET_DAVE    = 3, // HOT BURN
        DAGRESET_PAUL    = 4  // PCI gen 3 stuff
} reset_method_t;


typedef struct dag_card_reset
{
        /* Note for ROM v1: Stable and Current images stand for 0 and 1 image numbers respectively */
        int            img_nr;
        reset_method_t method;
} dag_card_reset_t;

/* DAG STREAM related definitios and structures*/

/* The maximum streams of 256 is requred by HSBM */
#define DAG_STREAM_MAX 256

#define DAG_STREAM_NODE_BIND   0x01
#define DAG_STREAM_NODE_PREF   0x02
#define DAG_STREAM_NODE_AUTO   0x04
#define DAG_STREAM_32BIT       0x08
#define DAG_STREAM_USE_POOL    0x10
#define DAG_STREAM_LARGE       0x20
#define DAG_STREAM_INDEPENDENT 0x40

typedef struct dag_memreq
{
	uint64_t base;
	uint64_t size;
	uint32_t stream;
	uint32_t node;
	uint32_t flags;
}dag_memreq_t;

/* This is the mode of operation of a vDAG in daemon/server mode. */
#define DAG_REVERSE_MODE 1
/* This is the normal mode of operation of a Hardware DAG card. */
#define DAG_NORMAL_MODE 0
#define DAG_LOCK_MODE_MAX (2)

#define DAG_LOCK_OP_RELEASE (0)
#define DAG_LOCK_OP_ACQUIRE (1)

typedef struct dag_stream_lock
{
	uint32_t stream;
	uint16_t mode;
	uint16_t set;
}dag_stream_lock_t;


/* DUCK related definitios and structures*/

/* Bitfields for DAG_IOSETDUCK, Set_Duck_Field parameter */
#define DAGDUCK_CLEAR_STATS	0x0001
#define DAGDUCK_HEALTH_THRESH	0x0002
#define DAGDUCK_PHASE_CORR	0x0004
#define DAGDUCK_RESET		0x0008

typedef struct duckinf
{
	int64_t		Phase_Correction;
	uint64_t	Last_Ticks;
	uint64_t	Last_TSC;
	time_t		Stat_Start, Stat_End;
	uint32_t	Crystal_Freq;
	uint32_t	Synth_Freq;
	uint32_t	Resyncs;
	uint32_t	Bad_Pulses;
	uint32_t	Worst_Freq_Err, Worst_Phase_Err;
	uint32_t	Health_Thresh;
	uint32_t	Pulses, Single_Pulses_Missing, Longest_Pulse_Missing;
	uint32_t	Health, Sickness;
	int32_t		Freq_Err, Phase_Err;
	uint32_t	Set_Duck_Field;
} duckinf_t;

typedef struct duck_irq_time
{
	uint32_t    nsec;
	uint32_t    sec;
} duck_irq_time_t;


typedef struct dag_netdev_inf
{
	int16_t idx;
	int16_t stream_rx;
	int16_t stream_tx;
	int16_t port;
	int16_t timing_port;
	int16_t ptp_state;
	int16_t ptp_index;
	char    mac[6];
	char    name[16];
}dag_netdev_inf_t;

typedef struct dag_stream_stat
{
	int16_t stream;
	int16_t stat;
}dag_stream_stat_t;


/* Structure which keeps detailed info about actual memory 
allocation */
#define DAG_MAX_NODES 16
typedef struct dag_poolinfo
{
    uint32_t pool;
    uint64_t base;
    uint64_t size;
    uint32_t free;
    uint32_t node;
} dag_poolinfo_t;

/* DAG IOCTLS defenitions */
/* All IOCTLs _must_ at least have a short description in comments. */

#if defined(__SVR4) && defined(__sun)

#define DAGIOC_CARD_RESET		0
#define DAGIOC_CARD_DUCK		1
#define DAGIOC_CARD_IRQTIME		2
#define DAGIOC_CARD_GETINFO		3
#define DAGIOC_CARD_SETINFO		4
#define DAGIOC_CARD_LOCK		5
#define DAGIOC_STREAM_ALLOC		6
#define DAGIOC_STREAM_FREE		7
#define DAGIOC_STREAM_GETINFO	        8
#define DAGIOC_STREAM_LOCK		9
#define DAGIOC_STREAM_STAT		10

#else

#define DAG_IOC_MAGIC  'd'
#define DAGIOC_CARD_RESET		_IOW(DAG_IOC_MAGIC, 0, dag_card_reset_t)
#define DAGIOC_CARD_DUCK		_IOWR(DAG_IOC_MAGIC, 1, duckinf_t)
#define DAGIOC_CARD_IRQTIME		_IOR(DAG_IOC_MAGIC, 2, duck_irq_time_t)
#define DAGIOC_CARD_GETINFO		_IOR(DAG_IOC_MAGIC, 3, dag_card_inf_t)
#define DAGIOC_CARD_SETINFO		_IOW(DAG_IOC_MAGIC, 4, dag_card_inf_t)
#define DAGIOC_CARD_LOCK		_IOW(DAG_IOC_MAGIC, 5, uint32_t)
#define DAGIOC_CARD_CONFLOCK		_IOW(DAG_IOC_MAGIC, 6, uint32_t)
#define DAGIOC_CARD_GETVERSION 		_IOR(DAG_IOC_MAGIC, 16, uint32_t)
#define DAGIOC_CARD_SETVERSION 		_IOWR(DAG_IOC_MAGIC, 17, uint32_t)
#define DAGIOC_CARD_RESTORE_PCICONF     _IO(DAG_IOC_MAGIC, 18)
#define DAGIOC_CARD_SET_PHCOFFSET       _IOW(DAG_IOC_MAGIC, 19, int64_t)
#define DAGIOC_CARD_GET_PHCOFFSET       _IOR(DAG_IOC_MAGIC, 20, int64_t)
#define DAGIOC_CARD_GET_PHCINDEX        _IOR(DAG_IOC_MAGIC, 21, uint32_t)


#define DAGIOC_STREAM_ALLOC _IOWR(DAG_IOC_MAGIC, 7, dag_memreq_t)
#define DAGIOC_STREAM_FREE _IOW(DAG_IOC_MAGIC,  8, uint32_t)
#define DAGIOC_STREAM_GETINFO _IOWR(DAG_IOC_MAGIC, 9, dag_memreq_t)
#define DAGIOC_STREAM_LOCK _IOW(DAG_IOC_MAGIC, 10, dag_stream_lock_t)
#define DAGIOC_STREAM_STAT _IOWR(DAG_IOC_MAGIC, 11, dag_stream_stat_t)
#define DAGIOC_NETDEV_CREATE _IOWR(DAG_IOC_MAGIC, 12, dag_netdev_inf_t)
#define DAGIOC_NETDEV_FREE _IOW(DAG_IOC_MAGIC, 13, dag_netdev_inf_t)
#define DAGIOC_NETDEV_INFO _IOWR(DAG_IOC_MAGIC, 14, dag_netdev_inf_t)
#define DAGIOC_API_VERSION _IOR(DAG_IOC_MAGIC, 15, uint32_t)

#endif

#define DAG_IOC_MAXNR 22

#endif

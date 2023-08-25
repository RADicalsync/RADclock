/*
 * Copyright (c) 2003-2014 Endace Technology Ltd, Hamilton, New Zealand.
 * All rights reserved.
 *
 * $Id: 8373ae3  from: Thu Nov 19 14:06:37 2015 +1300  author: Alexey Korolev $
 */

#ifndef DAGREG_H
#define DAGREG_H


#if defined(__linux__)

#ifndef __KERNEL__
#include <inttypes.h>  /* The Linux kernel has its own stdint types. */
#else
#include <linux/kernel.h>
#endif /* __KERNEL__ */

#elif defined(__FreeBSD__)

#ifndef _KERNEL
#include <inttypes.h>
#include <stdlib.h>
#else
#include <sys/param.h>
#if (__FreeBSD_version >= 700000)
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/stddef.h>
#elif (__FreeBSD_version >= 504000)
#include <inttypes.h>
#else
#include <sys/inttypes.h>  /* So does the FreeBSD kernel. */
#endif
#endif /* _KERNEL */

#elif defined(__NetBSD__)

#ifndef _KERNEL
#include <inttypes.h>
#else
#include <sys/param.h>
#include <sys/inttypes.h>  /* So does the NetBSD kernel. */
#endif /* _KERNEL */

#elif (defined(__APPLE__) && defined(__ppc__))

#include <stdint.h>

#elif defined(_WIN32)

#include <wintypedefs.h>

#elif defined (__SVR4) && defined (__sun)

#include <sys/types.h>

#else
#error Compiling on an unsupported platform - please contact <support@endace.com> for assistance.
#endif /* Platform-specific code. */


#define DAG_REG_MAX_ENTRIES 256

#define DAG_REG_37T_XSCALE_RESET (0x15c)
#define DAG_REG_ADDR_START 0xffa5
#define DAG_REG_ADDR_END   0xffff

typedef struct dag_reg_hw
{
#if defined(__ppc__) || defined(__POWERPC__)

	/* Big-endian layout. */
	uint32_t  addr    : 16;
	uint32_t  module  : 8;
	uint32_t  version : 4;
	uint32_t  flags   : 4;
	
#else

	/* Little-endian layout. */
	uint32_t  addr    : 16;
	uint32_t  module  : 8;
	uint32_t  flags   : 4;
	uint32_t  version : 4;
	
#endif /* Endianness. */
} dag_reg_hw_t;

typedef enum dag_reg_flags
{
	DAG_REG_FLAG_TABLE = 0x8,
	DAG_REG_FLAG_ARM   = 0x4
	
} dag_reg_flags_t;

/* This enumeration defines the meaning of the version field of table start entries */
typedef enum dag_reg_start_ver
{
	DAG_REG_TABLE_NORM = 0x00,
	DAG_REG_TABLE_PORT = 0x01,
	DAG_REG_TABLE_TIMING = 0x02
} dag_reg_start_ver_t;

typedef struct dag_reg
{
	uint32_t  addr    : 16;
	uint32_t  module  : 8;
	uint32_t  version : 4;
	uint32_t  flags   : 4;
	dag_reg_start_ver_t type;
	uint32_t  index;
} dag_reg_t;

/* this enumeration must match the value of the firmware enumeration table */
typedef enum dag_reg_module
{
	DAG_REG_START = 0x00,
	DAG_REG_GENERAL = 0x01,
	DAG_REG_DUCK = 0x02,
	DAG_REG_PBM = 0x03,
	DAG_REG_GPP = 0x04,
	DAG_REG_SONET = 0x05,
	DAG_REG_ROM = 0x06,
	DAG_REG_PTX = 0x07,
	DAG_REG_FPGAP = 0x08,
	DAG_REG_UART = 0x09,
	DAG_REG_BTX = 0x0A,
	DAG_REG_ARM = 0x0B,
	DAG_REG_PM5355 = 0x0C,
	DAG_REG_VSC9112 = 0x0D,
	DAG_REG_ICS1893 = 0x0E,
	DAG_REG_SONIC_3 = 0x0F,
	DAG_REG_SONIC_D = 0x10,
	DAG_REG_MINIMAC = 0x11,
	DAG_REG_S19205 = 0x12,
	DAG_REG_PM5381 = 0x13,
	DAG_REG_MAR = 0x14,
	DAG_REG_HDIM = 0x15,
	DAG_REG_MIPF = 0x16,
	DAG_REG_COPRO = 0x17,
	DAG_REG_ARM_RAM = 0x18,
	DAG_REG_ARM_PCI = 0x19,
	DAG_REG_ARM_X1 = 0x1A,
	DAG_REG_ARM_X2 = 0x1B,
	DAG_REG_ARM_X3 = 0x1C,
	DAG_REG_ARM_PHY = 0x1D,
	DAG_REG_ARM_EEP = 0x1E,
	DAG_REG_PM3386 = 0x1F,
	DAG_REG_CAM_MAINT = 0x20,
	DAG_REG_CAM_SEARCH = 0x21,
	DAG_REG_COPRO_BIST = 0x22,
	DAG_REG_BIST_DG = 0x23,
	DAG_REG_COPRO_COUNTERS = 0x24,
	DAG_REG_SAR_CTRL = 0x25,
	DAG_REG_MSA300_RX = 0x26,
	DAG_REG_MSA300_TX = 0x27,
	DAG_REG_PPF = 0x28,
	DAG_REG_ATX = 0x29,
	DAG_REG_QDR = 0x2A,
	DAG_REG_ATM_ARMOUR = 0x2B,
	DAG_REG_BOPT_GEN = 0x2C,
	DAG_REG_BOPT_RESET = 0x2D,
	DAG_REG_SONIC_E1T1 = 0x2E,
	DAG_REG_E1T1_CTRL = 0x2F,
	DAG_REG_TERF64 = 0x30,
	DAG_REG_PP64 = 0x31,
	DAG_REG_DISP64 = 0x32,
	DAG_REG_DP83865 = 0x33,
	DAG_REG_TAP = 0x34,
	DAG_REG_SMB = 0x35,
	DAG_REG_E1T1_HDLC_DEMAP = 0x36,
	DAG_REG_E1T1_HDLC_MAP = 0x37,
	DAG_REG_E1T1_ATM_DEMAP = 0x38,
	DAG_REG_E1T1_ATM_MAP = 0x39,
	DAG_REG_THRESHMODE = 0x3a,
	DAG_REG_PL3_ARMOUR = 0x3b,
	DAG_REG_RC = 0x3c,
	DAG_REG_RAWCAPTYPE = 0x3d,
	DAG_REG_RAW_TX = 0x3e,
	DAG_REG_SAM_MAINT = 0x3f,
	DAG_REG_MV88E1111 = 0x40,
	DAG_REG_AD2D_BRIDGE = 0x41,
	DAG_REG_BIST = 0x42,
	DAG_REG_FAILSAFE = 0x43,
	DAG_REG_BPFI = 0x44, 
	DAG_REG_IDT_TCAM = 0x45,
	DAG_REG_AMCC = 0x46,
	DAG_REG_XRT79L72 = 0x47,
	DAG_REG_RAW_SMBBUS = 0x48,
	DAG_REG_RAW_MAXDS3182 = 0x49,
	DAG_REG_STEERING = 0x4a,
	DAG_REG_BITHDLC_DEMAP = 0x4b,
	DAG_REG_SRGPP = 0x4c,
	DAG_REG_IXP = 0x4d,
	DAG_REG_HMM = 0x4e,
	DAG_REG_STATS = 0x4f,
	DAG_REG_DROP = 0x50,
	DAG_REG_RANDOM_CAPTURE = 0x51,
	DAG_REG_VSC8476 = 0x52,
	DAG_REG_XGMII = 0x53,
	DAG_REG_HLB = 0x54,
	DAG_REG_OC48_DEFRAMER = 0x55,
	/* 0x56 ? */
	DAG_REG_AMCC3485 = 0x57,
	DAG_REG_DSMU = 0x58,
	DAG_REG_DPA = 0x59,
	/* 0x5a ? */
	/* 0x5b ? */
	DAG_REG_COUNT_INTERF = 0x5C,
	/* 0x5d ? */
	/* 0x5e ? */
	/* 0x5f ? */
	/* 0x60 ? */
	/* 0x61 ? */
	DAG_REG_INFINICLASSIFIER = 0x62,
	/* 0x63 ? */
	DAG_REG_XFP = 0x64,
	DAG_REG_VSC8479 = 0x65,
	DAG_REG_SONET_PP = 0x66,
	/* 0x67 ? */
	DAG_REG_XGE_PCS = 0x68,
	DAG_REG_CAT = 0x69,
	DAG_REG_STREAM_FTR = 0x6A,
	/* 0x6b ? */
	DAG_REG_ROM_SERIAL_NUMBER = 0x6c,
	DAG_REG_INFI_FRAMER = 0x6D,
	DAG_REG_INFINICAM = 0x6E,
	DAG_REG_43GE_LASER_CONTROL = 0x6F,
	DAG_REG_GTP_PHY = 0x70,
	DAG_REG_MEMTX = 0x71,
	DAG_REG_DUCK_READER = 0x72,
	/* 0x73 ? */
	DAG_REG_VSC8486 = 0x74,
        /* 0x75 ? */  
	/* 0x76 ? */
	DAG_REG_QT2225_FRAMER = 0x77,
        /* 0x78 ? */
	DAG_REG_BFSCAM = 0x79,
	DAG_REG_PATTERN_MATCH = 0x7A,
	DAG_REG_SONET_CHANNEL_DETECT = 0x7B,
	DAG_REG_SONET_CHANNEL_CONFIG_MEM = 0x7C,
	DAG_REG_SONET_RAW_SPE_EXTRACT = 0x7D,
	DAG_REG_RESET_STRATEGY = 0x7e,
	/* 0x7f ? */
	DAG_REG_IRIGB = 0x80,
	DAG_REG_TILE_CTRL = 0x82,  
	DAG_REG_PHY_MOD = 0X83,
	DAG_REG_SCRATCH_PAD=0x85,
	DAG_REG_SDH_DECHAN = 0x84,
	DAG_REG_CLOCK_SWITCH = 0x86,
	DAG_REG_PMA = 0x8a,
	DAG_REG_PCS = 0x8b,
	DAG_REG_MAC = 0x8c,
	DAG_REG_VCXO = 0x91,
	DAG_REG_CARD_INFO    = 0x8D,
	DAG_REG_RAW_CAPTURE  = 0x8e,
	DAG_REG_FORMAT_SELECTOR = 0x8f,
	DAG_REG_TIMESTAMP_CONVERTER = 0x92,
	DAG_REG_INDEPENDENT_STREAM = 0x94,
	DAG_REG_PORT_IDENTIFIER = 0x95,
	DAG_REG_INTERNAL_STATUS = 0x96,
	DAG_REG_PROVENANCE_MODULE = 0x97,
	DAG_REG_TX_STREAM_FEATURES = 0x9A,
	DAG_REG_TX_BEHAVIOUR		= 0x9B,
	DAG_REG_SOURCE_ID			= 0x9C,
	DAG_REG_END = 0xFF
} dag_reg_module_t;

#define DAG_REG_ADDR(X)  ((X).addr)
#define DAG_REG_VER(X)   ((X).version)
#define DAG_REG_TYPE(X)  ((X).type)
#define DAG_REG_INDEX(X) ((X).index)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined(__KERNEL__) || defined(_KERNEL)
int dag_reg_find(char *iom, dag_reg_module_t module, dag_reg_t *result, int max_results);
#else
int dag_reg_init(int dagfd);
int dag_reg_find(int dagfd, dag_reg_module_t module, dag_reg_t result[DAG_REG_MAX_ENTRIES]);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* DAGREG_H */

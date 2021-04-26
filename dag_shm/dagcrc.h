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
 * Copyright (c) 2004-2005 Endace Technology Ltd, Hamilton, New Zealand.
 * All rights reserved.
 *
 * This source code is proprietary to Endace Technology Limited and no part
 * of it may be redistributed, published or disclosed except as outlined in
 * the written contract supplied with this product.
 *
 * $Id: a6bd287  from: Tue Aug 13 20:56:07 2013 +0000  author: Stephen Donnelly $
 */

#ifndef DAGCRC_H
#define DAGCRC_H

#ifndef _WIN32
/* C Standard Library headers. */
#include <inttypes.h>
#else /* _WIN32 */
#include <wintypedefs.h>
#endif /* _WIN32 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum dagcrc_mode {
	DAGCRC_MODE_CONT = 0,
	DAGCRC_MODE_START = 1,
	DAGCRC_MODE_END = 2,
	DAGCRC_MODE_FULL = 3
} dagcrc_mode_t;

typedef enum dagcrc_crcs {
	DAGCRC_ETH = 0,
	DAGCRC_IB_INV = 0,
	DAGCRC_AAL5 = 1,
	DAGCRC_PPP32 = 2,
	DAGCRC_CRC32C = 3,
	DAGCRC_PPP16 = 4,
	DAGCRC_IB_VAR = 5,
	DAGCRC_ATM_HEC = 6,

	DAGCRC_CRC_MAX = DAGCRC_ATM_HEC
} dagcrc_crcs_t;

/* New public interfaces */

/* Pre-generate a table from the types above */
int dagcrc_gen_table(dagcrc_crcs_t crc_type);

/* Do a specific CRC calculation over a buffer. Types and modes are as above. */
uint32_t dagcrc_do_crc(uint32_t crc, const void *buf, unsigned int length, dagcrc_crcs_t crc_type, dagcrc_mode_t mode);



/* Legacy interfaces */

/* Ethernet 32-bit CRC. */
void dagcrc_make_ethernet_table_r(void);
uint32_t dagcrc_ethernet_crc32_r(uint32_t crc, const uint8_t* buf, int len);

/* Infiniband 16-bit VCRC */
void dagcrc_make_infiniband_vcrc_table_r(void);
uint16_t dagcrc_infiniband_vcrc16_r(uint32_t crc, const uint8_t* buf, int len);

/* infiniband 32 bit ICRC */
uint32_t
dagcrc_infiniband_icrc32(uint32_t crc, const uint8_t* buf1, int len1, const uint8_t* buf2, int len2);
/* PPP 16 and 32-bit CRCs. */
#define PPPINITFCS16 0xffff  /* Initial FCS value */
#define PPPGOODFCS16 0xf0b8  /* Good final FCS value */

#define PPPINITFCS32 0xffffffff   /* Initial FCS value */
#define PPPGOODFCS32 0xdebb20e3   /* Good final FCS value */

void dagcrc_make_ppp_fcs16_table(void);
/* Note no legacy dagcrc_make_ppp_fcs32_table() function*/

/* Users calling this fn on a full packet must pass fcs = PPPINITFCS16 on start */
uint16_t dagcrc_ppp_fcs16(uint16_t fcs, const uint8_t* cp, int len);

/* Users calling this fn on a full packet must pass fcs = PPPINITFCS32 on start */
uint32_t dagcrc_ppp_fcs32(uint32_t fcs, const uint8_t* cp, int len);

void generate_crc8_crc10_table(void);
uint8_t dagcrc_atm_hec8( uint8_t crc_accum, const char *pdata, int len );
uint16_t dagcrc_atm_crc10( uint16_t crc_accum, const char *pdata, int len );

/* ATM AAL5 32-bit CRC. */
#define CRC_INIT   0xffffffffL
#define CRC_PASS   0xC704DD7BL

void dagcrc_make_aal5_table(void);

/* Users calling this fn on a full packet must pass crc_accum = CRC_INIT on start */
uint32_t dagcrc_aal5_crc(uint32_t crc_accum, const char *data_blk_ptr, int data_blk_size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DAGCRC_H */

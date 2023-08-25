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
 * Copyright (c) 2006-2006 Endace Technology Ltd, Hamilton, New Zealand.
 * All rights reserved.
 *
 * This source code is proprietary to Endace Technology Limited and no part
 * of it may be redistributed, published or disclosed except as outlined in
 * the written contract supplied with this product.
 *
 * $Id: e62a228  from: Wed Aug 6 23:21:28 2014 +0000  author: Nuwan Gunasekara $
 */

#ifndef DAGERF_H
#define DAGERF_H

/* Endace headers. */
#include "dag_platform.h"


typedef enum tt
{
	TT_ERROR = -1,
	TT_ATM = 0,
	TT_ETH = 1,
	TT_POS = 2
	
} tt_t;

typedef struct legacy_atm_
{
	int64_t  ts;
	uint32_t crc;
	uint32_t header;
	uint32_t pload[12];
	
} legacy_atm_t;

typedef struct legacy_eth_
{
	int64_t  ts;
	uint16_t wlen;
	uint8_t  dst[6];
	uint8_t  src[6];
	uint16_t etype;
	uint32_t pload[10];
	
} legacy_eth_t;

typedef struct legacy_pos_
{
	int64_t  ts;
	uint32_t slen;
	uint16_t loss;
	uint16_t wlen;
	uint32_t pload[12];
	
} legacy_pos_t;

typedef struct tcp_ip_counter_
{
	int64_t ts;
	uint8_t type;
	uint8_t flags;
	uint16_t rlen;
	uint32_t tcp_ip_flag;
	uint8_t pload[1];
	
} tcp_ip_counter_t;

/* MC HDLC Error flags */
enum
{
	MC_HDLC_FCS = 0x01,
	MC_HDLC_SHORT = 0x02,
	MC_HDLC_LONG = 0x04,
	MC_HDLC_ABORT = 0x08,
	MC_HDLC_OCTET = 0x10,
	MC_HDLC_LOST = 0x20
};

/* MC ATM Error flags */
enum
{
	MC_ATM_LOST = 0x01,
	MC_ATM_HECC = 0x02,
	MC_ATM_OAMCRC = 0x04,
	MC_ATM_OAM = 0x08
};

/* MC AAL Error Flags */
enum
{
	MC_AAL_1ST_CELL = 0x10,
	MC_AAL_LEN_ERROR = 0x80
};

/* MC AAL2 Error Flags */
enum
{
	MC_AAL2_MAAL = 0x40,
	MC_AAL2_1ST_CELL = 0x20
};

/* MC AAL5 Error Flags */
enum
{
	MC_AAL5_CRC_CHECK = 0x10,
	MC_AAL5_CRC_ERROR = 0x20,
	MC_AAL5_LEN_CHECK = 0x40,
	MC_AAL5_LEN_ERROR = 0x80,
	MC_AAL5_1ST_CELL = 0x10
};

typedef struct ipv4_header {
#if defined (BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN)
	uint8_t		version:4;
	/* The minimum value for this field is 5 (RFC 791), which is a length of 5x32 = 160 bits = 20 bytes.
	 * Being a 4-bit value, the maximum length is 15 words (15x32 bits) or 480 bits = 60 bytes. */
	uint8_t		ihl:4; /*Internet Header Length*/
	uint8_t		dscp:6;
	uint8_t		ecn:2;
#else /* assumes LITTLE_ENDIAN */
	/* The minimum value for this field is 5 (RFC 791), which is a length of 5x32 = 160 bits = 20 bytes.
	 * Being a 4-bit value, the maximum length is 15 words (15x32 bits) or 480 bits = 60 bytes. */
	uint8_t		ihl:4; /*Internet Header Length*/
	uint8_t		version:4;
	uint8_t		ecn:2;
	uint8_t		dscp:6;
#endif

	uint16_t	total_len;
	uint16_t	identification;
#if defined (BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN)
	uint16_t	flags:3;
	uint16_t	fragment_offset:13;
#else /* assumes LITTLE_ENDIAN */
	uint16_t	fragment_offset:13;
	uint16_t	flags:3;
#endif
	uint8_t		ttl;
	uint8_t		protocol;
	uint16_t	header_checksum;
	uint8_t	source_ip[4];
	uint8_t	dst_ip[4];
} __attribute__ ((packed)) ipv4_header_t;

/* structure definitions for NBP trailers and ts */
typedef struct cpacket_triler
{
	uint32_t s;
	uint32_t ns;
	uint32_t reserved:24;
	uint8_t  port;
} __attribute__ ((packed)) cpacket_triler_t;

typedef struct apcon_triler
{
	uint32_t s;
	uint32_t ns;
}__attribute__ ((packed)) apcon_triler_t;

typedef struct arista_key_frame {
	uint64_t    	  asic_time;			/* The full 64bit counter used for frame timestamping. Each tick represents approx. 2.857ns. */
	uint64_t          utc_time;				/* Unix (POSIX) time in nanoseconds */
	uint64_t          last_sync_time;		/* The last ASIC time (in nanoseconds) at which PTP was synchronized.
											   If no sync has occurred, or if the last sync was greater than 8 hours ago, this field is 0. */
	uint64_t          keyframe_gen_time;	/* The generation time of the keyframe itself, in nanoseconds (Unix time). */
	uint64_t          egress_drop;			/* Number of dropped frames on the keyframe's egress interface */
	uint16_t          device_id;			/* User defined device ID */
	uint16_t          egress_intf;			/* The egress switchport of the keyframe */
	uint8_t			  fcs_type;				/* The timestamping mode configured on the keyframe's egress port.
												0 = timestamping disabled
												1 = timestamp is appended to the payload and a new FCS is added to the frame
												2 = timestamp overwrites the existing FCS */
	uint8_t			  reserved;
} __attribute__ ((packed)) arista_key_frame_t;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Utility routines for ERF records: displaying the type as a string, extracting length etc.
 */

/*
 * WARNING: routines in this module are provided for convenience
 *          and to promote code reuse among the dag* tools.
 *          They are subject to change without notice.
 */


/**
Get a string describing the given ERF type.
@param type The type field from the ERF record.
@param legacy_type For legacy ERF records, the type to return (TT_ATM, TT_ETH, TT_POS).  Use TT_ERROR to indicate that the ERF should not be a legacy type.
@return A string constant describing the ERF type.  There is no need to free() this string.
*/
const char * dagerf_type_to_string(uint8_t type, tt_t legacy_type);

/**
Get a string describing the type of the given ERF record.
@param erf A pointer to an ERF record.
@param legacy_type For legacy ERF records, the type to return (TT_ATM, TT_ETH, TT_POS).  Use TT_ERROR to indicate that the ERF should not be a legacy type.
@return A string constant describing the type of the ERF record.  There is no need to free() this string.
*/
const char * dagerf_record_to_string(uint8_t * erf, tt_t legacy_type);

/**
Get the ERF type as an int for given ERF type string starting with prefix ERF_TYPE_.
Legacy types are not supported.
@param type The ERF type as a string.
@param cmp_size string length to compare.
@return on success ERF type as an int, -1 on error or for unknown type.
*/
int dagerf_string_to_type (char * type , size_t cmp_size );

/**
Get the length of an ERF record.
@param erf A pointer to an ERF record.
@return The length of the ERF record (not the wire length of the contained packet).
*/
unsigned int dagerf_get_length(uint8_t * erf);

/**
Determine if an ERF record is of a known (legacy or modern) type.
@param erf A pointer to an ERF record.
@return 1 if the ERF is of a known type, 0 otherwise.
*/
unsigned int dagerf_is_known_type(uint8_t * erf);

/**
Determine if an ERF record is of a known non-legacy type.
@param erf A pointer to an ERF record.
@return 1 if the ERF is of a known type, 0 otherwise.
*/
unsigned int dagerf_is_modern_type(uint8_t * erf);

/**
Determine if an ERF record is a legacy type.
@param erf A pointer to an ERF record.
@return 1 if the ERF is a legacy type, 0 otherwise.
*/
unsigned int dagerf_is_legacy_type(uint8_t * erf);

/**
Determine if an ERF record is an Ethernet type.
@param erf A pointer to an ERF record.
@return 1 if the ERF is an Ethernet type, 0 otherwise.
*/
unsigned int dagerf_is_ethernet_type(uint8_t * erf);

/**
Determine if an ERF record is an IP type.
@param erf A pointer to an ERF record.
@return 1 if the ERF is an IP type, 0 otherwise.
*/
unsigned int dagerf_is_ip_type(uint8_t * erf);

/**
Determine if an ERF record is a PoS type.
@param erf A pointer to an ERF record.
@return 1 if the ERF is a PoS type, 0 otherwise.
*/
unsigned int dagerf_is_pos_type(uint8_t * erf);

/**
Determine if an ERF record is a colored type.
@param erf A pointer to an ERF record.
@return 1 if the ERF is a colored type, 0 otherwise.
*/
unsigned int dagerf_is_color_type(uint8_t * erf);

/**
Determine if an ERF record is a multichannel (MC) type.
@param erf A pointer to an ERF record.
@return 1 if the ERF is a multichannel type, 0 otherwise.
*/
unsigned int dagerf_is_multichannel_type(uint8_t * erf);

/**
Calculates the number of extension (if any) within the ERF record.
@param erf A pointer to an ERF record.
@return the number of extnsion headers present, 0 if there are none.
*/
unsigned int dagerf_ext_header_count(uint8_t * erf, size_t len);

/**
Set IP header's check sum.
@param ip_header Reference to IP header.
@return Success(1) or failure(0). Could fail if IP header length is shorter than 12 as not enough bytes to set the checksum.
*/
uint8_t dagerf_set_ip_header_checksum (uint8_t* ip_header);
/**
Returns IP header's check sum.
@param ip_header Reference to IP header.
@return IP header check sum..
*/
uint16_t dagerf_get_ip_header_checksum (uint8_t* ip_header);

/**
 * Converts cpacket timestamp to ERF timestamp.
 *
 *                                    erf_payload ---+
 *                                                   |
 *                                                   v
 * | ERF header | ext hdrs....| mc headers | eth pad | ERF payload                                                  |
 * |                                                 | L2 header & payload        | cpacket trailer | FCS | ERF PAD |
 * |                                                 |<------------------ wlen -------------------------->|
 *
 * Assumes:
 * 	1. Not a truncated packet
 * 	2. wlen >= cpacket trailer + FCS

@param erf_payload		Reference to start of ERF payload.
@param wlen				wlen size of the record.
@param fcs_in_bytes		Available FCS length in bytes.
@param tick_time_in_ns	cpacket tick time in nano seconds.
@return converted ERF timestamp.
*/
uint64_t dagerf_cpacket_ts_to_erf_ts (uint8_t* erf_payload, uint16_t wlen, uint8_t fcs_in_bytes, long double tick_time_in_ns);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DAGERF_H */ 

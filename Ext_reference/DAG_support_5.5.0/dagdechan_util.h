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
 * Copyright (c) 2010 Endace Technology Ltd, Hamilton, New Zealand.
 * All rights reserved.
 *
 * This source code is proprietary to Endace Technology Limited and no part
 * of it may be redistributed, published or disclosed except as outlined in
 * the written contract supplied with this product.
 *
 */

#ifndef DAG_DECHAN_UTIL_H
#define DAG_DECHAN_UTIL_H

/* DAG headers. */
#include "dag_platform.h"

#define DECHAN_MAX_AUG_INDEX 6
#define DAGDECHAN_MAX_CHANNELS 768
#define DAGDECHAN_MAX_PORTS_BITS 1
#define DAGDECHAN_MAX_PORTS (1<<DAGDECHAN_MAX_PORTS_BITS)
#define DAGDECHAN_MAX_STREAM 128
#define DAGDECHAN_MAX_PATTERN_MATCHER_CHANNELS 64


typedef enum {
	kSdhSizeVCUnknown  = 0,
	kSdhSizeVC3 = 0x1,
	kSdhSizeVC4 = 0x2,
	kSdhSizeVC4_4c = 0x3,
	kSdhSizeVC4_16c = 0x4,
	kSdhSizeVC4_64_c = 0x5,
	kSdhSizeLastUndefined
}enum_vc_size_t;

typedef enum
{
	kSdhRateInvalid = 0x0,
	kSdhRateSTM0 = 0x1,
	kSdhRateSTM1 = 0x2,
	kSdhRateSTM4 =  0x3,
	kSdhRateSTM16 = 0x4,
	kSdhRateSTM64 = 0x05,
	kSdhRateSTM256 = 0x06,
	kSdhRateLastUndefined
}enum_sdh_dechan_line_rate_t;


typedef enum 
{
	kSdhDemappingNone,
	kSdhDemappingATM,
	kSdhDemappingPOS
}enum_sdh_channel_demapping_type_t;
typedef enum 
{
	kPosDemapperFCSNone,
	kPosDemapperFCS32Bit,
	kPosDemapperFCS16Bit
}enum_sdh_demapper_fcs_type_t;
typedef enum
{
	kSdhDechanFramePartTOH				= 0x0,
	kSdhDechanFramePartPOH				= 0x01,
	kSdhDechanFramePartContainer		= 0x02,
	kSdhDechanFramePartPOS				= 0x03,	
	kSdhDechanFramePartATM				= 0x04,
	kSdhDechanFramePartPJOBytes			= 0x05,
	kSdhDechanFramePartRaw				= 0x06,
	kSdhDechanFramePartLastUndefined 
}enum_sdh_dechan_frame_part_t;
typedef struct sdh_g707_format_s
{
	uint8_t m_sdh_line_rate;
	uint8_t m_vc_size ;
    int8_t m_vc_index_array[DECHAN_MAX_AUG_INDEX];
				/*	i = 4 --> ITU-T letter #E - index of AUG-64
				* i = 3 --> ITU-T letter #D - index of AUG-16
				* i = 2 --> ITU-T letter #C - index of AUG-4,
				* i = 1 --> ITU-T letter #B  -index of AUG-1
				* i = 0 --> ITU-T letter #A  - index of AU3*/
}sdh_g707_format_t;
typedef struct sdh_channel_configuration_s
{	
	uint8_t							m_port;
	enum_sdh_dechan_line_rate_t 	m_channel_line_rate;
	uint16_t 						m_channels_count;
	sdh_g707_format_t				m_channel_g707[DAGDECHAN_MAX_CHANNELS];
	uint32_t						m_rule_line_no; /* rule line number , if 0 its not used*/
}sdh_channel_configuration_t;

#pragma pack(1)
typedef struct sdh_dechan_extn_hdr_s
{
	uint32_t	m_frame_part:8;	/* bit 0 - 7*/
	uint32_t	m_line_speed:8;	/* bit 8 - 15*/
	uint32_t	m_vc_size:8;	/* BIT 16- 23*/
	uint32_t	m_vc_id:8; 		/* bit 24 -31*/
	uint32_t	m_RES:8; 		/* bit 32-39*/
	uint32_t 	m_seq_num:15; 	/* bit 40- 54*/
	uint32_t 	m_more_frag:1; 	/* bit 55*/	
	uint32_t 	m_ext_type :7; /* bit 56- 62*/
	uint32_t 	m_more_ext:1; 	/* bit 63*/
}sdh_dechan_extn_hdr_t;
#pragma pack()

int  dechan_fill_sdh_g707_format(sdh_g707_format_t* in_fmt, uint16_t bit_flds, uint8_t vc_size, uint8_t speed);
void dechan_fill_vc_id_string(char* out_string, int max_len, sdh_g707_format_t* in_fmt);

int dagdechan_validate_g707_channel_name(sdh_g707_format_t *in_fmt);

/* get string functions */
const char* dagdechan_get_line_rate_string(enum_sdh_dechan_line_rate_t in_rate);
const char* dagdechan_get_vc_size_string(enum_vc_size_t in_size);
const char* dagdechan_get_frame_part_string(enum_sdh_dechan_frame_part_t in_part);
/* convert functions */
int dagdechan_convert_g707_to_timeslot(const sdh_g707_format_t *in_fmt);
uint16_t dagdechan_convert_g707_to_bitfields(const sdh_g707_format_t *in_fmt);
uint8_t dagdechan_channel_present_inlist(const sdh_g707_format_t* the_channel, const sdh_g707_format_t *in_channel_list, int in_count );
uint8_t dagdechan_compare_bitfilds_g707_name(uint16_t in_bit_flds, const sdh_g707_format_t *in_fmt);
int sdh_check_channel_configuration_completeness(const sdh_channel_configuration_t *in_config, uint8_t *in_missing, uint8_t* in_overlap);
int sdh_dechan_get_extn_header(const uint8_t* in_header, const uint8_t *in_rec, int in_len, sdh_dechan_extn_hdr_t* out_hdr);

#endif 

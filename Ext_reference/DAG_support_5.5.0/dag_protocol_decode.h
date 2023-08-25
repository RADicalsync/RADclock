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
 * Copyright (c) 2006 Endace Technology Ltd, Hamilton, New Zealand.
 * All rights reserved.
 *
 * This source code is proprietary to Endace Technology Ltd and no part
 * of it may be redistributed, published or disclosed except as outlined
 * in the written contract supplied with this product.
 *
 * $Id: a6bd287  from: Tue Aug 13 20:56:07 2013 +0000  author: Stephen Donnelly $
 */

#ifndef DAG_PROTOCOL_DECODE_H
#define DAG_PROTOCOL_DECODE_H

/* Endace headers. */
#include "dag_platform.h"
#include "dagerf.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* Some notes about this hierarchy of routines:
 *
 * The 'decode_foo()' routines are recursive.  They print their own header and any subheaders they find.
 * The 'print_foo()' routines are not.  All they do is to print their own header.
 *
 */

/**
 Find an IP datagram header in an ERF record.
 @param erf_record A pointer to an ERF record.
 @param verbose Display information about progress.
 @param legacy_format If the ERF is possibly in Legacy format, which format (TT_ATM, TT_ETH, TT_POS) to assume.  TT_ERROR indicates the ERF should not be a Legacy format.
 @return A pointer to the beginning of the IP datagram.
 */
uint8_t * dagpd_find_ip_header(uint8_t * erf_record, int verbose, tt_t legacy_format);

/**
 Decode and display all protocols inside an ERF record.
 @param erf_record A pointer to an ERF record.
 @param erf_length The size of the ERF record.
 */
void dagpd_decode_protocols(uint8_t* erf_record, int erf_length);

/* ERF. */
/**
 Decode and display protocols inside an ERF record.
 @param erf_header A pointer to an ERF record.
 @param erf_length The size of the ERF record.
 @param legacy_format If the ERF is possibly in Legacy format, which format (TT_ATM, TT_ETH, TT_POS) to assume.  TT_ERROR indicates the ERF should not be a Legacy format.
 */
void dagpd_decode_erf(uint8_t* erf_header, int erf_length, tt_t legacy_format);

/**
 Display an ERF record header.
 @param erf_header A pointer to an ERF record.
 @param erf_length The size of the ERF record.
 @param legacy_format If the ERF is possibly in Legacy format, which format (TT_ATM, TT_ETH, TT_POS) to assume.  TT_ERROR indicates the ERF should not be a Legacy format.
 @return The number of bytes 'consumed' by the display.
 */
int dagpd_print_erf_header(uint8_t* erf_header, int erf_length, tt_t legacy_format);

/* IP. */
/**
 Decode and display protocols inside an IP datagram.
 @param ip_header A pointer to an IP header.
 @param remaining The size of the data pointed to by ip_header.
 */
void dagpd_decode_ip(uint8_t* ip_header, int remaining);

/**
 Display an IP datagram header.
 @param ip_header A pointer to an IP datagram.
 @param remaining The size of the data pointed to by ip_header.
 @param next_proto Receives the protocol number of the next layer packet (for example UDP,TCP,ICMP etc).
 @return The number of bytes 'consumed' by the routine, i.e. the offset from ip_header to the beginning of unprocessed data.
 */
int dagpd_print_ip_header(uint8_t* ip_header, int remaining, uint8_t *next_proto);

/* TCP. */
/**
 Decode and display protocols inside a TCP datagram.
 @param tcp_header A pointer to a TCP header.
 @param remaining The size of the data pointed to by ip_header.
 */
void dagpd_decode_tcp(uint8_t* tcp_header, int remaining);

/**
 Display a TCP datagram header.
 @param tcp_header A pointer to a TCP datagram.
 @param remaining The size of the data pointed to by tcp_header.
 @return The number of bytes 'consumed' by the routine, i.e. the offset from tcp_header to the beginning of unprocessed data.
 */
int dagpd_print_tcp_header(uint8_t* tcp_header, int remaining);

/* UDP. */
/**
 Decode and display protocols inside a UDP datagram.
 @param udp_header A pointer to a UDP header.
 @param remaining The size of the data pointed to by udp_header.
 */
void dagpd_decode_udp(uint8_t* udp_header, int remaining);

/**
 Display a UDP datagram header.
 @param udp_header A pointer to a UDP datagram.
 @param remaining The size of the data pointed to by udp_header.
 @return The number of bytes 'consumed' by the routine, i.e. the offset from udp_header to the beginning of unprocessed data.
 */
int dagpd_print_udp_header(uint8_t* udp_header, int remaining);

/* ICMP. */
/**
 Decode and display protocols inside an ICMP datagram.
 @param icmp_header A pointer to a ICMP header.
 @param remaining The size of the data pointed to by icmp_header.
 */
void dagpd_decode_icmp(uint8_t* icmp_header, int remaining);

/**
 Display an ICMP datagram.
 @param icmp_header A pointer to an ICMP datagram.
 @param remaining The size of the data pointed to by icmp_header.
 @return The number of bytes 'consumed' by the routine, i.e. the offset from icmp_header to the beginning of unprocessed data.
 */
int dagpd_print_icmp_header(uint8_t* icmp_header, int remaining);

/**
 Display a datagram payload as bytes.
 @param payload A pointer to the data.
 @param remaining The size of the data pointed to by payload.
 */
void dagpd_print_payload_bytes(uint8_t* payload, int remaining);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DAG_PROTOCOL_DECODE_H */

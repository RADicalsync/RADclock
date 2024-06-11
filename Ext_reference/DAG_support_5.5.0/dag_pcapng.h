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

#ifndef DAG_PCAPNG_H
#define DAG_PCAPNG_H

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif /* HAVE_CONFIG_H */
/*
 * $Id: 0f09361  from: Tue Mar 3 03:36:45 2015 +0000  author: Nuwan Gunasekara $
 *
 * Standardised Link Layer Type codes for PCAPNG.
 * Reference http://www.winpcap.org/ntar/draft/PCAP-DumpFileFormat.html#appendixLinkTypes
*/
#define 	DAG_LINKTYPE_NULL	0	/* No link layer information. A packet saved with this link layer contains a raw L3 packet preceded by a 32-bit host-byte-order AF_ value indicating the specific L3 type. */
#define 	DAG_LINKTYPE_ETHERNET	1	/* D/I/X and 802.3 Ethernet */
#define 	DAG_LINKTYPE_EXP_ETHERNET	2	/* Experimental Ethernet (3Mb) */
#define 	DAG_LINKTYPE_AX25	3	/* Amateur Radio AX.25 */
#define 	DAG_LINKTYPE_PRONET	4	/* Proteon ProNET Token Ring */
#define 	DAG_LINKTYPE_CHAOS	5	/* Chaos */
#define 	DAG_LINKTYPE_TOKEN_RING	6	/* IEEE 802 Networks */
#define 	DAG_LINKTYPE_ARCNET	7	/* ARCNET, with BSD-style header */
#define 	DAG_LINKTYPE_SLIP	8	/* Serial Line IP */
#define 	DAG_LINKTYPE_PPP	9	/* Point-to-point Protocol */
#define 	DAG_LINKTYPE_FDDI	10	/* FDDI */
#define 	DAG_LINKTYPE_PPP_HDLC	50	/* PPP in HDLC-like framing */
#define 	DAG_LINKTYPE_PPP_ETHER	51	/* NetBSD PPP-over-Ethernet */
#define 	DAG_LINKTYPE_SYMANTEC_FIREWALL	99	/* Symantec Enterprise Firewall */
#define 	DAG_LINKTYPE_ATM_RFC1483	100	/* LLC/SNAP-encapsulated ATM */
#define 	DAG_LINKTYPE_RAW	101	/* Raw IP */
#define 	DAG_LINKTYPE_SLIP_BSDOS	102	/* BSD/OS SLIP BPF header */
#define 	DAG_LINKTYPE_PPP_BSDOS	103	/* BSD/OS PPP BPF header */
#define 	DAG_LINKTYPE_C_HDLC	104	/* Cisco HDLC */
#define 	DAG_LINKTYPE_IEEE802_11	105	/* IEEE 802.11 (wireless) */
#define 	DAG_LINKTYPE_ATM_CLIP	106	/* Linux Classical IP over ATM */
#define 	DAG_LINKTYPE_FRELAY	107	/* Frame Relay */
#define 	DAG_LINKTYPE_LOOP	108	/* OpenBSD loopback */
#define 	DAG_LINKTYPE_ENC	109	/* OpenBSD IPSEC enc */
#define 	DAG_LINKTYPE_LANE8023	110	/* ATM LANE + 802.3 (Reserved for future use) */
#define 	DAG_LINKTYPE_HIPPI	111	/* NetBSD HIPPI (Reserved for future use) */
#define 	DAG_LINKTYPE_HDLC	112	/* NetBSD HDLC framing (Reserved for future use) */
#define 	DAG_LINKTYPE_LINUX_SLL	113	/* Linux cooked socket capture */
#define 	DAG_LINKTYPE_LTALK	114	/* Apple LocalTalk hardware */
#define 	DAG_LINKTYPE_ECONET	115	/* Acorn Econet */
#define 	DAG_LINKTYPE_IPFILTER	116	/* Reserved for use with OpenBSD ipfilter */
#define 	DAG_LINKTYPE_PFLOG	117	/* OpenBSD DLT_PFLOG */
#define 	DAG_LINKTYPE_CISCO_IOS	118	/* For Cisco-internal use */
#define 	DAG_LINKTYPE_PRISM_HEADER	119	/* 802.11+Prism II monitor mode */
#define 	DAG_LINKTYPE_AIRONET_HEADER	120	/* FreeBSD Aironet driver stuff */
#define 	DAG_LINKTYPE_HHDLC	121	/* Reserved for Siemens HiPath HDLC */
#define 	DAG_LINKTYPE_IP_OVER_FC	122	/* RFC 2625 IP-over-Fibre Channel */
#define 	DAG_LINKTYPE_SUNATM	123	/* Solaris+SunATM */
#define 	DAG_LINKTYPE_RIO	124	/* RapidIO - Reserved as per request from Kent Dahlgren <kent@praesum.com> for private use. */
#define 	DAG_LINKTYPE_PCI_EXP	125	/* PCI Express - Reserved as per request from Kent Dahlgren <kent@praesum.com> for private use. */
#define 	DAG_LINKTYPE_AURORA	126	/* Xilinx Aurora link layer - Reserved as per request from Kent Dahlgren <kent@praesum.com> for private use. */
#define 	DAG_LINKTYPE_IEEE802_11_RADIO	127	/* 802.11 plus BSD radio header */
#define 	DAG_LINKTYPE_TZSP	128	/* Tazmen Sniffer Protocol - Reserved for the TZSP encapsulation, as per request from Chris Waters <chris.waters@networkchemistry.com> TZSP is a generic encapsulation for any other link type, which includes a means to include meta-information with the packet, e.g. signal strength and channel for 802.11 packets. */
#define 	DAG_LINKTYPE_ARCNET_LINUX	129	/* Linux-style headers */
#define 	DAG_LINKTYPE_JUNIPER_MLPPP	130	/* Juniper-private data link type, as per request from Hannes Gredler <hannes@juniper.net>. The corresponding DLT_s are used for passing on chassis-internal metainformation such as QOS profiles, etc.. */
#define 	DAG_LINKTYPE_JUNIPER_MLFR	131	/* Juniper-private data link type, as per request from Hannes Gredler <hannes@juniper.net>. The corresponding DLT_s are used for passing on chassis-internal metainformation such as QOS profiles, etc.. */
#define 	DAG_LINKTYPE_JUNIPER_ES	132	/* Juniper-private data link type, as per request from Hannes Gredler <hannes@juniper.net>. The corresponding DLT_s are used for passing on chassis-internal metainformation such as QOS profiles, etc.. */
#define 	DAG_LINKTYPE_JUNIPER_GGSN	133	/* Juniper-private data link type, as per request from Hannes Gredler <hannes@juniper.net>. The corresponding DLT_s are used for passing on chassis-internal metainformation such as QOS profiles, etc.. */
#define 	DAG_LINKTYPE_JUNIPER_MFR	134	/* Juniper-private data link type, as per request from Hannes Gredler <hannes@juniper.net>. The corresponding DLT_s are used for passing on chassis-internal metainformation such as QOS profiles, etc.. */
#define 	DAG_LINKTYPE_JUNIPER_ATM2	135	/* Juniper-private data link type, as per request from Hannes Gredler <hannes@juniper.net>. The corresponding DLT_s are used for passing on chassis-internal metainformation such as QOS profiles, etc.. */
#define 	DAG_LINKTYPE_JUNIPER_SERVICES	136	/* Juniper-private data link type, as per request from Hannes Gredler <hannes@juniper.net>. The corresponding DLT_s are used for passing on chassis-internal metainformation such as QOS profiles, etc.. */
#define 	DAG_LINKTYPE_JUNIPER_ATM1	137	/* Juniper-private data link type, as per request from Hannes Gredler <hannes@juniper.net>. The corresponding DLT_s are used for passing on chassis-internal metainformation such as QOS profiles, etc.. */
#define 	DAG_LINKTYPE_APPLE_IP_OVER_IEEE1394	138	/* Apple IP-over-IEEE 1394 cooked header */
#define 	DAG_LINKTYPE_MTP2_WITH_PHDR	139	/* ??? */
#define 	DAG_LINKTYPE_MTP2	140	/* ??? */
#define 	DAG_LINKTYPE_MTP3	141	/* ??? */
#define 	DAG_LINKTYPE_SCCP	142	/* ??? */
#define 	DAG_LINKTYPE_DOCSIS	143	/* DOCSIS MAC frames */
#define 	DAG_LINKTYPE_LINUX_IRDA	144	/* Linux-IrDA */
#define 	DAG_LINKTYPE_IBM_SP	145	/* Reserved for IBM SP switch and IBM Next Federation switch. */
#define 	DAG_LINKTYPE_IBM_SN	146	/* Reserved for IBM SP switch and IBM Next Federation switch. */

/* These LINKTYPEs are not defined in the pcapng standard (http://www.winpcap.org/ntar/draft/PCAP-DumpFileFormat.html#appendixLinkTypes), but in libpcap */
#define 	DAG_LINKTYPE_ERF	197
#define 	DAG_LINKTYPE_IPV4	228
#define 	DAG_LINKTYPE_IPV6	229

#endif /* DAG_PCAPNG_H */

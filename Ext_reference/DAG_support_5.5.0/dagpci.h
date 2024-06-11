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


#ifndef DAGPCI_H
#define DAGPCI_H

#if !( defined( __KERNEL__) || defined (_KERNEL) ) 
#ifndef _WIN32
# include <inttypes.h>
#endif /* _WIN32 */
#endif

#if defined(__FreeBSD__) && defined (_KERNEL) 
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/stddef.h> 
#endif 

#define PCI_VENDOR_ID_ENDACE	0xeace

/* DAG card PCI Device IDs */
#define PCI_DEVICE_ID_DAG7_400	0x7400
#define PCI_DEVICE_ID_DAG7_401	0x7401

#define PCI_DEVICE_ID_DAG7_5G2	0x752e
#define PCI_DEVICE_ID_DAG7_5G4	0x754e

#define PCI_DEVICE_ID_DAG8_101	0x8101

#define PCI_DEVICE_ID_DAG9_2X2  0x920e
#define PCI_DEVICE_ID_DAG9_2SX2 0x9200 

#define PCI_DEVICE_ID_DAG10_X4P 0xa140
#define PCI_DEVICE_ID_DAG10_X2P 0xa120

#define PCI_DEVICE_ID_DAG10_X4B 0xa143
#define PCI_DEVICE_ID_DAG10_X4S 0xa14e

#define PCI_DEVICE_ID_DAG10_X2B 0xa121
#define PCI_DEVICE_ID_DAG10_X2S 0xa12e

#define PCI_DEVICE_ID_VDAG      0xeace

#define PCI_DEVICE_ID_TDS24     0x07d5

#if defined ( __KERNEL__) && defined (__linux__) 
#include <linux/pci.h>
extern struct pci_device_id dag_pci_tbl[];
#endif

#if defined(DAGC_EXPORTS) 
typedef __int32 uint32_t;
#endif /* DAGC_EXPORTS */


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


char * dag_device_name(uint32_t device, uint32_t flag);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DAGPCI_H */

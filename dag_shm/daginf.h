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
* $Id: 8373ae3  from: Thu Nov 19 14:06:37 2015 +1300  author: Alexey Korolev $
 */

#ifndef DAGINF_H
#define DAGINF_H

#if defined(__linux__)

#ifndef __KERNEL__
#include <inttypes.h>  /* The Linux kernel has its own stdint types. */
#include <time.h>
#else
#include <linux/kernel.h>
#endif /* __KERNEL__ */

#elif defined(__FreeBSD__)

#ifndef _KERNEL
#include <inttypes.h>
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
#if (__FreeBSD_version >= 700000)
#include <sys/time.h>
#else 
#include <time.h>
#endif 

#elif defined(__NetBSD__)

#ifndef _KERNEL
#include <inttypes.h>
#else
#include <sys/param.h>
#include <sys/inttypes.h>  /* So does the FreeBSD kernel. */
#endif /* _KERNEL */
#include <time.h>

#elif defined(_WIN32)

#include <wintypedefs.h>
#include <time.h>

#elif defined(__SVR4) && defined(__sun)

#include <sys/types.h>

#elif defined(__APPLE__) && defined(__ppc__)

#include <inttypes.h>
#include <time.h>

#else
#error Compiling on an unsupported platform - please contact <support@endace.com> for assistance.
#endif /* Platform-specific code. */
#define DAG_ID_SIZE 20 /* for daginf.bus_id, from BUS_ID_SIZE = KOBJ_NAME_LEN */

/* Backward compatiable for daginf structure, this structure is substituted by dag_card_inf_t */
typedef struct daginf {
        uint32_t                id;             /* DAG device number */

        uint32_t                phy_addr ;
//      unsigned long           phy_addr;       /* PCI address of large buffer (ptr) */

        uint32_t                buf_size;       /* its size */
        uint32_t                iom_size;       /* iom size */
        uint16_t                device_code;    /* PCI device ID */
#if defined(__linux__) || defined(__FreeBSD__) || (defined (__SVR4) && defined (__sun))
        char                    bus_id[DAG_ID_SIZE];
        uint8_t                 brd_rev;        /**Card revision ID which is stored in the PCI configuration space */
#endif
#if defined(_WIN32)
        uint32_t                bus_num;        /* PCI bus number */
        uint16_t                dev_num;        /* PCI device number */
        uint16_t                fun_num;        /* Function number within a PCI device */
        uint32_t                ui_num;         /* User-perceived slot number */
        uint16_t                brd_rev;        /**Card revision ID which is stored in the PCI configuration space */
#endif
} daginf_t;

#ifdef _WIN64
#pragma pack (1)
#endif


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* DAGINF_H */

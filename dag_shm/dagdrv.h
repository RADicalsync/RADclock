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


#ifndef DAGDRV_H
#define DAGDRV_H

/* DAG tree headers. */
#include "dagioctl.h"
struct dag_stream {
	uint64_t dma_base;			/* DMA base address of the steam */
	void *desc;					/* Memory descriptor to keep information about memory allocations */
	int flags;					/* Stream attributes */
	unsigned long size;			/* Size of stream memory buffer*/
	struct file *mode_owner[DAG_LOCK_MODE_MAX];		/* Stream normal and reverse mode locks */
};

typedef struct dag_softc {
	int					unit;	/* which card/device */
	uint32_t				fw_version; /* which card/device */
	struct dag_stream	streams[DAG_STREAM_MAX]; /* stream descs */
	struct mtx			mtx;	/* to lock unit */
	struct file			*card_owner;
	struct file			*conf_owner;
        volatile int                    atomic_phy_status;
#ifdef __i386__
	int					mmaped_stream;
#endif
	int (*get_info)(void *card, dag_card_inf_t *info);
	int (*reset)(void *card, dag_card_reset_t *rst);
        int (*restore_pciconf)(void *card);
	int (*duck_irq_time)(void *card, duck_irq_time_t *duck_irq_time);
	int (*duck_inf)(void *card, duckinf_t *duckinf);
	void * (*get_mmio)(void *card, int offt, int *size);

	void* (*alloc)(void *card, dag_memreq_t *mem_req);
	void (*free)(void *desc);
	void* (*get_vaddr)(void *desc, uint32_t offset, uint32_t *size);

	struct cdev		*dagmemdev;	/* user entry points */
	struct cdev		*dagiomdev;	/* user entry points */
} dag_softc_t;

int dagcore_attach(struct dag_softc *ds);
void dagcore_detach(struct dag_softc *ds);
#endif /* DAGDRV_H */

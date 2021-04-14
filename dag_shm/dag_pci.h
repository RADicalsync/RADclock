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


#ifndef DAG_PCI_H
#define DAG_PCI_H

#if __FreeBSD_version >= 1000000
#define PCIR_EXPRESS_LINK_CTL          PCIER_LINK_CTL
#define PCIR_EXPRESS_DEVICE_CTL        PCIER_DEVICE_CTL
#endif


#include "dagdrv.h"
typedef struct pciconfig {
	unsigned	ba0;	/* base address 0 */
	unsigned	ba1;	/* base address 1 */
	unsigned	ba2;	/* base address 2 */
	unsigned	com;	/* command */
	unsigned char	lin;	/* interrupt line */
	unsigned char	lat;	/* latency timer */
	unsigned	dev_ctrl;	/* PCI-E device control reg */
	unsigned	link_ctrl;	/* PCI-E link control reg */
	unsigned char	brd_rev;	/* Board revision */
} pciconfig_t;

#define DAG_MIN_MSIX_VECTORS  3
#define DAG_MAX_MSIX_VECTORS  32

struct dag_msix_vec {
        struct resource *res;
        int rid;
        void *tag;
};

typedef struct dag_pci_softc {
	dag_softc_t		ds;		/* Glue to dag code */
	int			guest;		/* Guest OS mode flag */
	device_t		device;		/* as opposed to dev_t */
	struct resource		*iomres;	/* IO memory resource  BAR0 */
	unsigned		iomsize;	/* IO memory size */
	struct resource		*memres;	/* Gueest OS DMA resource BAR1*/
	struct resource		*msixres;	/* MSIX memory resource BAR1*/
	struct dag_msix_vec	msix_vecs[DAG_MAX_MSIX_VECTORS]; /* MSIX  vectors */
        unsigned                num_msix;        /* Number of MSIX vectors assigned */
	unsigned		memsize;	/* IO memory size */
	struct resource		*irqres;	/* interrupt resource */
	void			*intrhand;	/* interrupt handler */

	pciconfig_t		pciconfig;	/* PCI configurations */
	void			*duck;		/* duck runtime/stats */
} dag_pci_softc_t;

#define DAG_IO_WRITE(sc, reg, val)	\
	bus_space_write_4(rman_get_bustag((sc)->iomres),	\
					rman_get_bushandle((sc)->iomres), (reg), (val))

#define DAG_IO_READ(sc, reg)	\
	bus_space_read_4(rman_get_bustag((sc)->iomres),	\
					rman_get_bushandle((sc)->iomres), (reg))

#define DAG_MSIX_READ(sc, reg)	\
	bus_space_read_4(rman_get_bustag((sc)->msixres), \
					rman_get_bushandle((sc)->msixres), (reg))
#define DAG_MSIX_WRITE(sc, reg, val)	\
	bus_space_write_4(rman_get_bustag((sc)->msixres),	\
					rman_get_bushandle((sc)->msixres), (reg), (val))


void* dagmem_cont_alloc(void *card, dag_memreq_t *mem_req);
void dagmem_cont_free(void *desc);
void *dagmem_cont_vaddr(void *desc, uint32_t offset, uint32_t *size);

void* dagmem_guest_alloc(void *card, dag_memreq_t *mem_req);
void dagmem_guest_free(void *desc);
void *dagmem_guest_vaddr(void *desc, uint32_t offset, uint32_t *size);

void *duck_new(dag_pci_softc_t * sc);
void duck_destroy(void *duck);
int duck_init(void *duck);
void duck_intr(void *arg);

int duck_ioctl(void *card, duckinf_t *duckinf);
int duck_get_irq_time_ioctl(void *card, duck_irq_time_t *duck_irq_time);

#endif

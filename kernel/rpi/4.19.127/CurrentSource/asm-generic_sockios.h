/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef __ASM_GENERIC_SOCKIOS_H
#define __ASM_GENERIC_SOCKIOS_H

/* Socket-level I/O control calls. */
#define FIOSETOWN	0x8901
#define SIOCSPGRP	0x8902
#define FIOGETOWN	0x8903
#define SIOCGPGRP	0x8904
#define SIOCATMARK	0x8905
#define SIOCGSTAMP	0x8906		/* Get stamp (timeval) */
#define SIOCGSTAMPNS	0x8907		/* Get stamp (timespec) */

#ifdef CONFIG_FFCLOCK
#define SIOCSFFCLOCKTSMODE	0x8908
#define SIOCGFFCLOCKTSMODE	0x8909
#endif

#endif /* __ASM_GENERIC_SOCKIOS_H */

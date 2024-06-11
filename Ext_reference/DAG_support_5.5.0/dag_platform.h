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
 * Copyright (c) 2005-2006 Endace Technology Ltd, Hamilton, New Zealand.
 * All rights reserved.
 *
 * This source code is proprietary to Endace Technology Limited and no part
 * of it may be redistributed, published or disclosed except as outlined in
 * the written contract supplied with this product.
 *
 * $Id: 8373ae3  from: Thu Nov 19 14:06:37 2015 +1300  author: Alexey Korolev $
 */

#ifndef DAG_PLATFORM_H
#define DAG_PLATFORM_H


/* Cross-platform POSIX headers.
 * Headers that do not exist on ALL supported platforms should be in the relevant platform-specific headers.
 */
#include <fcntl.h>

/* Cross-platform C Standard Library headers.
 * Headers that do not exist on ALL supported platforms should be in the relevant platform-specific headers.
 */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#if defined(__FreeBSD__)

#include "dag_platform_freebsd.h"

#elif defined(__linux__)

#include "dag_platform_linux.h"

#elif (defined(__APPLE__) && defined(__ppc__))

#include "dag_platform_macosx.h"

#elif defined(__NetBSD__)

#include "dag_platform_netbsd.h"

#elif (defined(__SVR4) && defined(__sun))

#include "dag_platform_solaris.h"

#elif defined(_WIN32)

#include "dag_platform_win32.h"

#else
#error Compiling on an unsupported platform - please contact <support@endace.com> for assistance.
#endif /* Platform-specific code. */


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* DAG_PLATFORM_H */

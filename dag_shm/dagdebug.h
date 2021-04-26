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
 * Copyright (c) 1999
 *
 * $Id: dagdebug.h 3662 2006-03-20 22:56:57Z koryn $
 */
#ifndef DAGDEBUG_H
#define DAGDEBUG_H
/* #define DEBUG */
#define DAG_ERROR(fmt, ...) \
		printf("error: [%s]" fmt, __func__ , ##__VA_ARGS__)

#define DAG_INFO(fmt, ...) \
		printf(fmt , ##__VA_ARGS__)

#ifdef DEBUG
#define DAG_DEBUG(fmt, ...) \
		printf("%s " fmt, __func__ , ##__VA_ARGS__);
#else
#define DAG_DEBUG(fmt, ...)
#endif

#endif /* DAGDEBUG_H */

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
 * Copyright (c) 2011 Endace Technology Ltd, Hamilton, New Zealand.
 * All rights reserved.
 *
 * This source code is proprietary to Endace Technology Limited and no part
 * of it may be redistributed, published or disclosed except as outlined in
 * the written contract supplied with this product.
 *
 * $Id: a6bd287  from: Tue Aug 13 20:56:07 2013 +0000  author: Stephen Donnelly $
 */

#ifndef DAGDEPRECATED_H
#define DAGDEPRECATED_H

/* Applications can define DAG_ALLOW_DEPRECATED to suppress deprecation warnings,
   or DAG_HIDE_DEPRECATED to prevent deprecated functions being exported,
   otherwise the compiler will issue warnings on deprecated functions */

#ifndef DAG_ALLOW_DEPRECATED
  #ifdef DAG_HIDE_DEPRECATED
    #define DAG_DEPRECATED(func)
  #elif defined __GNUC__
    #define DAG_DEPRECATED(func) func __attribute__ ((deprecated))
  #elif defined(_MSC_VER)
    #define DAG_DEPRECATED(func) __declspec(deprecated) func
  #else
    #pragma message("WARNING: You need to implement DAG_DEPRECATED for this compiler")
    #define DAG_DEPRECATED(func) func
  #endif
#else
  #define DAG_DEPRECATED(func) func
#endif

#endif /* DAGDEPRECATED_H */

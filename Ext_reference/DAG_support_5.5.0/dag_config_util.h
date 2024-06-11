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
 * This source code is proprietary to Endace Technology Limited and no part
 * of it may be redistributed, published or disclosed except as outlined in
 * the written contract supplied with this product.
 *
 */

#ifndef DAG_CONFIG_UTIL_H
#define DAG_CONFIG_UTIL_H


#include "dag_config.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * \defgroup ConfigAndStatusAPI Configuration and Status API
 * The interface that exposes the Components and Attributes that configures the card..
 */
/*@{*/
/**
 * \defgroup ConfigUtil Configuration Utility
 * */
/*@{*/

/**
  Used to configure the memory allocation of the card as a whole.
     \param card_ref card reference
     \param  text e.g. "mem=32@1:32@0"
     \return 0 if success -1 if error 
  */
int dag_config_util_domemconfig(dag_card_ref_t card, const char* text);

/**
  Used to configure the memory allocation of the card in stream based.
     \param card_ref card reference
     \param  text e.g. "streamN=32@1"
     \return 0 if success -1 if error 
  */
int dag_config_util_dostreamconfig(dag_card_ref_t card, const char* text);

/**
  Used to configure the snap length according to GPP or Stream feature.
     \param card_ref card reference
     \param text e.g. "slen0=128 for specified snap length index
or slen=128 to configure all of snap length base on stream feature"
	 \param port_select for GPP component
  */
void dag_config_util_set_snaplength(dag_card_ref_t card, const char* text, int port_select);

/**
 * Utility function to configure an attribute.
 * \param card_ref card reference
 * \param root_component root component
 * \param component_code owner of attribute
 * \param component_index index of component
 * \param attribute_code attribute to configure
 * \param lval new attribute value
 */
void dag_config_util_set_value_to_lval(dag_card_ref_t card_ref, dag_component_t root_component,
                                    dag_component_code_t component_code, int component_index, dag_attribute_code_t attribute_code, int lval);

/**
 * Utility function to configure an attribute which also includes a read back to verify that an attribute was correctly configured. If not, a warning message is printed.
 * \param card_ref card reference
 * \param root_component root component
 * \param component_code owner of attribute
 * \param component_index index of component
 * \param attribute_code attribute to configure
 * \param lval new attribute value
 * \param token_name name of token being set
 */
void dag_config_util_set_value_to_lval_verify(dag_card_ref_t card_ref, dag_component_t root_component,
                                    dag_component_code_t component_code, int component_index, dag_attribute_code_t attribute_code, int lval, char *token_name);

/* used to configure the hlb association table of the card as a whole
 * \param card_ref card reference
 * \param text e.g. "hlb=0-10.1:10.1-30.0:40-80
 * \return 0 if success -1 if error
 */
int dag_config_util_do_hlb_config(dag_card_ref_t card, const char* text);

/**
 * Set value of attribute to that of value_p.
 *
 * value_p can point to all number-like attribute values but not
 * strings or structures.
 *
 * \param card_ref card reference
 * \param uuid attrubite uuid
 * \param value_p pointer to the value to be set
 */
void dag_config_util_set_attribute(dag_card_ref_t card_ref, attr_uuid_t uuid, void* value_p);

int dag_config_util_set_rx_line_rate(dag_card_ref_t card,const char *text,int port_select);
int dag_config_util_set_tx_line_rate(dag_card_ref_t card,const char *text,int port_select);
/**
 * To fill a vector with each bit indicating which RX streams are ready capture.
 *
 * \param card_ref card reference
 * \param out_vects array of vectors to be filled.
 * \param count count of vectors in the array.
 */
int dag_conifg_util_get_configured_rx_stream_vector(dag_card_ref_t in_card, uint64_t *out_vects, int count);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DAG_CONFIG_UTIL_H */
/*@}*/
/*@}*/

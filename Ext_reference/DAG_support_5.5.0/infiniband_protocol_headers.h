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
 * Copyright (c) 2004-2008 Endace Technology Ltd, Hamilton, New Zealand.
 * All rights reserved.
 *
 * This source code is proprietary to Endace Technology Limited and no part
 * of it may be redistributed, published or disclosed except as outlined in
 * the written contract supplied with this product.
 * $Id: a6bd287  from: Tue Aug 13 20:56:07 2013 +0000  author: Stephen Donnelly $
*/


/*
 * Contains the structures used for implementing dagbits and daggen. The structures do
 * not match fields in infiniband protocol, but has been adjusted to work with the code 

 ***************************************WARNING***************************************
 *It is not recommeneded to use these structures for other applications
 ************************************************************************************* 

 */
#ifndef INFINIBAND_PROTO_HEADERS_H
#define INFINIBAND_PROTO_HEADERS_H


#pragma pack(1)
/**
Data structure for the Local Route Header (LRH) of Infiniband
 */
typedef struct ib_lrh {
    unsigned int link_version : 4;
    unsigned int virtual_lane : 4;
    unsigned int lnh : 2;
    unsigned int reserved1 : 2;
    unsigned int service_level : 4;
    unsigned int dest_local_id : 16;
    //unsigned int reserved2 : 5;
    unsigned int packet_length : 16;
    unsigned int src_local_id : 16;

}ib_lrh_t;

/**
Data structure for the Global Route Header (GRH) of Infiniband
 */
typedef struct ib_grh {
    /*unsigned int ip_ver : 4;
    unsigned int traffic_class : 8;
    unsigned int flow_label : 20;*/
    uint32_t word0;
    unsigned int pay_len : 16;
    unsigned int next_header : 8;
    unsigned int hop_limit : 8;
    uint32_t src_global_id[4];
    uint32_t dest_global_id[4];
}ib_grh_t;

/**
Data structure for the Base Transport Header (BTH) of Infiniband
 */
typedef struct ib_bth {
    unsigned int  op_code : 8;
    unsigned int  t_header_version : 4;
    unsigned int  pad_count :2 ;
    unsigned int  migration_state :1 ;
    unsigned int  solicited_event :1 ;
    unsigned int  partition_key : 16;
    unsigned int  reserved1 : 8;
    unsigned int  dest_qp : 24;
    unsigned int  reserved2 : 7;
    unsigned int  ack_req : 1;
    unsigned int  packet_seq_number : 24;
} ib_bth_t ;

/**
Data structure for infiniband RDETH header 
*/
typedef struct ib_ext_rdeth
{
    unsigned int reserved : 8;
    unsigned int ee_context :24;
}ib_ext_rdeth_t;

/**
Data structure for infiniband DETH header 
 */
typedef struct ib_ext_deth
{
    uint32_t queue_key;
    unsigned int reserved : 8;
    unsigned int source_qp : 24;
}ib_ext_deth_t;

/**
   Data structure for infiniband ext headers 
 temporary assumption - for dagbits assumes that external headers means RDETH followed by DETH
 */
typedef struct ib_ext_headers
{
    ib_ext_rdeth_t rdeth;
    ib_ext_deth_t deth;
}ib_ext_headers_t;

/**
Structure representing infiniband with GRH
 */
typedef struct ib_rec_with_grh
{
    ib_lrh_t lrh;
    ib_grh_t grh;
    ib_bth_t bth;
    ib_ext_headers_t ib_ext;
}ib_rec_with_grh_t;

/**
Structure representing infiniband without GRH
 */
typedef struct ib_rec_without_grh
{
    ib_lrh_t lrh;
    ib_bth_t bth;
    ib_ext_headers_t ib_ext;
}ib_rec_without_grh_t;

/**
Structure representing the infiniband record ,used in dagbits.
 */
typedef union _infiniband_rec
{
    ib_rec_with_grh_t ib_with_grh;
    ib_rec_without_grh_t  ib_with_no_grh;
}_infiniband_rec_t;

/**
Data structure for Infiniband frames, TYPE_INFINIBAND.
 */
typedef struct  infiniband_rec
{
	_infiniband_rec_t ib_rec;
	uint8_t pload[1];
				
} infiniband_rec_t;

#pragma  pack () 

#endif //INFINIBAND_PROTO_HEADERS_H


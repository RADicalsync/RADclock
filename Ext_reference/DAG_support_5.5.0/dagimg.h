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
 * Copyright (c) 2005 Endace Technology Ltd, Hamilton, New Zealand.
 * All rights reserved.
 *
 * This source code is proprietary to Endace Technology Limited and no part
 * of it may be redistributed, published or disclosed except as outlined
 * in the written contract supplied with this product.
 *
 * $Id: a6bd287  from: Tue Aug 13 20:56:07 2013 +0000  author: Stephen Donnelly $
 */

#ifndef DAGIMG_H
#define DAGIMG_H

#define	DAGSERIAL_SIZE	128
#define	DAGSERIAL_ID	0x12345678

/*
 * Backwards compatability struct for mapping PCI device IDs + xilinx to
 * image index when this is not available from firmware directly.
 */
typedef struct dag_img {
	int	device_id;
	int	load_idx;
	int	img_idx;
} dag_img_t;

#define DAG_IMG_END 0xffff

/*
 * Struct for mapping image index numbers to A record pefixes and B records
 * for type checking.
 */
typedef struct img_id {
	int	img_idx;
	char	*img_name;
	char	*img_type;
	int	board_rev;
} img_id_t;

#define IMG_ID_END 0xffff


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


int dag_get_img_idx(int device_id, int device_index);
int dag_check_img(int img_idx, char *arec, char *brec, int board_rev);
int dag_check_img_ptr(int img_idx, char *img, int img_size, int board_rev);
int dag_check_img_type(int img_idx, char* img, int img_size, int board_rev);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DAGIMG_H */

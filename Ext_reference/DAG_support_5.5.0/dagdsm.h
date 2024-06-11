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


/**********************************************************************
*
* Copyright (c) 2004-2005 Endace Technology Ltd, Hamilton, New Zealand.
* All rights reserved.
*
* This source code is proprietary to Endace Technology Limited and no part
* of it may be redistributed, published or disclosed except as outlined in
* the written contract supplied with this product.
*
* $Id: a6bd287  from: Tue Aug 13 20:56:07 2013 +0000  author: Stephen Donnelly $
*
**********************************************************************/

/** \defgroup DSMAPI Data Stream Management (DSM) API
Interface functions of the Data Stream Management (DSM) API.
 */

#ifndef DAGDSM_H
#define DAGDSM_H




typedef struct DsmConfig_ DsmConfig_;
typedef DsmConfig_* DsmConfigH;              /* Type-safe opaque type. */

typedef struct DsmFilter_ DsmFilter_;
typedef DsmFilter_* DsmFilterH;              /* Type-safe opaque type. */

typedef struct DsmPartialExp_ DsmPartialExp_;
typedef DsmPartialExp_* DsmPartialExpH;      /* Type-safe opaque type. */

typedef struct DsmOutputExp_ DsmOutputExp_;
typedef DsmOutputExp_* DsmOutputExpH;        /* Type-safe opaque type. */



/** \ingroup DSMAPI
 */
/*@{*/

/**
Flag used in the debugging functions. FIXME: Needs more description?
 */
#define DSM_DBG_INTERNAL_COLUR              0x0001

/**
Flag used in the debugging functions. FIXME: Needs more description?
*/
#define DSM_DBG_INTERNAL_FILTER             0x0001

/**
Constant value used for indicating that early termination is not required.
 */
#define DSM_NO_EARLYTERM                    0xFFFFFFFF
/*@}*/

/** \ingroup DSMAPI
 */
/*@{*/

/**
List of possible layer3 types to filter on.
 */
typedef enum 
{
	kIPv4 = 4
	/* kIPv6 = 6  */
	/* kArp  = 2  */
} layer3_type_t;

/**
Possible hash load balancing (steering) algorithm values.
 */
typedef enum
{
	kCRCLoadBalAlgorithm = 0,
	kParityLoadBalAlgorithm = 1
} load_bal_t;

/**
List of DSM version numbers. FIXME: Needs more description?
 */
typedef enum
{
	DSM_VERSION_0 = 0, /*original dsm version, 8 filters, 2bits interfaces crc&parity hlb, 2 memory holes*/
	DSM_VERSION_1 = 1 /*special dsm version for 4.5Z, 8 filters, 2bits interface 2bits hlb hash, 4 memory holes 8.2Z, 8 filters, 1bits interface 3bits hlb hash, 8 memory holes */
}dsm_version_t;
/*@}*/


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** \ingroup DSMAPI
 */
/*@{*/
	
	/*****************************************************************
	 *
	 *  CARD
	 *
	 *****************************************************************/
	/** \defgroup DSMCardConfig DSM DAG Card Configuration
	 */	
	/*@{*/

	/**
	@brief		Returns whether the DSM functionality is supported by the card.

	@param[in]  dagfd	A file descriptor for a DAG device.

	@return 		1 if DSM is supported by the specified card, 0 if the card can 
				support DSM but wrong firmware is loaded and -1 if the card 
				doesn't support DSM (or another error occurred, call 
				dagdsm_get_last_error() to verify)
	 */
	int
	dagdsm_is_dsm_supported(int dagfd);

	/**
	@brief		Retrieve DSM version.

	@param[in]  dagfd	A file descriptor for a DAG device.

	@return 		The DSM version number, otherwise -1 if an error occurred. Call 
				dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_get_dsm_version(int dagfd);	
	
	/**
	@brief		Prepares the DAG card to support DSM operation. Sets the card configuration 
			so that it can be used by the DSM module. The card settings configured by 
			this functions are indirectly related to the DSM module, however they are 
			not part of the DSM module. This functon should be called after the card 
			is configured.

	@param[in]  dagfd	A file descriptor for a DAG device.

	@return 		0 if card was configured successfully, -1 if an error occurred. 
				Call dagdsm_get_last_error() to retrieve the error code.

	@note			The function sets the following card parameters
				- ERF MUX Drop module enabled
				- IPF Steering module is set to use the packet color
				- On the 6.2 the size of the CRC is copied from the 
				  config API into the DWM control register.
	 */
	int
	dagdsm_prepare_card(int dagfd);
	
	/**
	@brief		Enables or disables the DSM bypass option, in bypass mode all packets skip
			the DSM module and are supplied to the host directly. By default when the 
			card boots the DSM is not bypassed.

	@param[in]  dagfd	A file descriptor for a DAG device.
	@param[in]  bypass	A zero value disables bypass mode, a non-zero value enables 
				the DSM bypass mode.

	@return 		0 if bypass mode was enabled /disabled successfully, otherwise 
				-1 if an error occurred. Call dagdsm_get_last_error() to retrieve 
				the error code.
	 */
	int
	dagdsm_bypass_dsm(int dagfd, uint32_t bypass);
	
	/**
	@brief		Indicates if the card is configured for Ethernet. Return value indicates 
			whether the card is ethernet or not. The supplied card indentifier must 
			be to a card that supports DSM filtering.

	@param[in]  dagfd	A file descriptor for a DAG device.
	@return 		0 if the card is not configured for Ethernet, 1 if configured 
				for Ethernet and -1 if an error occurred. Call dagdsm_get_last_error() 
				to retrieve the error code.
	 */
	int
	dagdsm_is_card_ethernet(int dagfd);
	
	/**
	@brief		Indicates if the card is configured for SONET (PoS). Return value indicates
			whether the card is sonet or not.  The supplied card indentifier must 
                        be to a card that supports DSM filtering

	@param[in]  dagfd	A file descriptor for a DAG device.

	@return 		0 if the card is not configured for SONET, 1 if configured for 
				SONET and -1 if an error occurred. Call dagdsm_get_last_error() to 
				retrieve the error code.
	 */
	int
	dagdsm_is_card_sonet(int dagfd);
	
	/**
	@brief		Returns the number of physical ports on the card.

	@parami[in]  dagfd	A file descriptor for a DAG device.

	@return 		A positive value indicates the number of physical ports (interfaces) 
				on the card, -1 if an error occurred. Call dagdsm_get_last_error() to 
				retrieve the error code.
	 */
	int
	dagdsm_get_port_count(int dagfd);

	
	/**
	@brief		Returns the number of available receive streams on the card.

	@param[in]  dagfd	A file descriptor for a DAG device.

	@return 		A positive value indicates the number of receive streams (filter streams) 
				available, -1 if an error occurred. Call dagdsm_get_last_error() to 
				retrieve the error code.
	 */
	int
	dagdsm_get_filter_stream_count(int dagfd);

	/**
	@brief		Gets the activation status of a DSM filter

	@param[in]  dagfd 	A file descriptor for a DAG device.
	@param[in]  filter 	The number of the physical filter on the card, filter numbers start 
				at 0 and go through to 7.

	@return 		0 if the filter is not active, 1 if active and -1 if an error occurred. 
				Call dagdsm_get_last_error() to retrieve the error code.
 	 */
	int
	dagdsm_is_filter_active(int dagfd, uint32_t filter);
	
	/**
	@brief		Activates or deactivates a particular DSM filter.

	@param[in]  dagfd 	A file descriptor for a DAG device.
	@param[in]  filter	The number of the physical filter on the card, filter numbers start 
				at 0 and go through to 7.
	@param[in]  activate 	A non-zero value activates the filter, a zero value deactivates the filter.
	
	@return 		0 if the filter was activated/deactivated and -1 if an error occurred. 
				Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_activate_filter(int dagfd, uint32_t filter, uint32_t activate);
	
	/**
	@brief		Loads a filter directly to the card (should only load into disabled filters).

	@param[in]  dagfd	A file descriptor for a DAG device.
	@param[in]  filter	The number of the filter.
	@param[in]  term_depth	The element (8-byte chunk) that the filter can early terminate on.
	@param[in]  value 	Array of comparand bytes that are loaded into the filter. This value can 
				be NULL if the size argument is also 0.
	@param[in]  mask	Array of mask bytes that are loaded into the filter. This value can be 
				NULL if the size argument is also 0.
	@param[in]  size 	The size of both the value and mask buffers in bytes.

	@return			0 if the filter was loaded, otherwise -1 if an error occurred. 
				Use dagdsm_get_last_error() to retreive the error code.
	 */
	int
	dagdsm_load_filter(int dagfd, uint32_t filter, uint32_t term_depth, const uint8_t * value, const uint8_t * mask, uint32_t size);

	/**
	@brief		Sets the steering control. When the card is configured in DSM, the steering 
			control must be in Colour/DSM (value 1). Configures the IPF steering control
			module, this should probably be done by the config API but without it the 
			DSM will not work.

	@param[in]  dagfd 	A file descriptor for a DAG device.
	@param[in]  value 	value to written to the steering bits. Possible values
				0 : All memory hole 0 , 
				1 : Use the colour field,
				2 : Use the ethernet pad field,
				3 : Use the interface number.

	@return	 		0 if the card is configured for ethernet, 0 if configured for sonet
				and -1 if an error occured.
	 */
	int dsm_set_ipf_steering_control (int dagfd, uint32_t value);

#if 0 /* Deprecated */
	/* returns the current active lut */
	int
	dagdsm_get_active_lut(int dagfd);
	
	/* sets the active lut */
	int
	dagdsm_set_active_lut(int dagfd, uint32_t lut);
	
	/* loads a lut into the card (should only load into the inactive lut) */
	int
	dagdsm_load_lut(int dagfd, uint32_t lut, const uint8_t * data_p, uint32_t size);
#endif
	/*@}*/
	/*****************************************************************
	 *
	 *  CONFIGURATION
	 *
	 *****************************************************************/

	/** \defgroup DSMVirtualConfig	DSM Virtual Configuration
	 */
	/*@{*/
	/**
	@brief		Creates a new blank virtual configuration

	@parami[in]   dagfd 	A file descriptor for a DAG device.

	@return 		A handle to the new configuration or NULL to indicate an error.
				Use dagdsm_get_last_error() to retrieve the error code.
	 */
	DsmConfigH
	dagdsm_create_configuration(int dagfd);
	
	/**
	@brief		Loads a virtual connection into the DAG card. It starts by reading
                	back the filters from the card and seeing what values have changed to 
			be completed

	@param[in]  config_h 	Handle to a virtual configuration returned by 
				dagdsm_create_configuration().
	
	@return 		0 if the configuration was loaded successfully, -1 is returned 
				to indicate an error. Use dagdsm_get_last_error() to retrieve 
				the error code.
	 */
	int
	dagdsm_load_configuration(DsmConfigH config_h);
	
	/** 
	@brief		Destroys an existing virtual configuration.

	@param[in]  config_h 	Handle to a virtual configuration returned by 
				dagdsm_create_configuration().

	@return 		0 if the configuration was destroyed otherwise -1 is returned 
				to indicate an error. Use dagdsm_get_last_error() to retrieve 
				the error code.
	 */
	int
	dagdsm_destroy_configuration(DsmConfigH config_h);
		
	/** 
	@brief		Returns a handle to a virtual filter. the handle can then be modified by 
			the caller using the dagdsm_filter_??? functions.

	@param[in]  config_h 	Handle to a virtual configuration returned by 
				dagdsm_create_configuration().
	@param[in]  filter 	The number of the filter, this should be a value in 
				the range of 0 to 6.

	@return 		A handle to the virtual filter, NULL is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	DsmFilterH
	dagdsm_get_filter(DsmConfigH config_h, uint32_t filter);
	
	/**
	@brief		Indicates whether the virtual configuration is for Ethernet or not.

	@param[in]  config_h 	Handle to a virtual configuration returned by 
				dagdsm_create_configuration().

	@return 		0 if the virtual configuration is not configured for Ethernet, 
				1 if Ethernet is configured and -1 if an error occurred. Call 
				dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_is_ethernet(DsmConfigH config_h);
	
	/**
	@brief		Indicates whether the virtual configuration is for SONET or not.

	@param[in]  config_h	Handle to a virtual configuration returned by 
				dagdsm_create_configuration().

	@return 		0 if the virtual configuration is not configured for SONET (PoS), 
				1 if SONET (PoS) is configured and -1 if an error occurred. 
				Call dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_is_sonet(DsmConfigH config_h);
	
	/** 
	@brief		Returns a handle to the virtual swap filter. The swap filter can then be 
			editied before swapping with an existing filter.

	@param[in]  config_h	Handle to a virtual configuration returned by 
				dagdsm_create_configuration().

	@return 		A handle to the virtual swap filter, NULL is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	DsmFilterH
	dagdsm_get_swap_filter(DsmConfigH config_h);
	
	/** 
	@brief		Swaps the specified filter on the card with the virtual swap filter. this
			function also updates the COLUR at the same time so that the output of the
			filter is consistant.

	@param[in]  config_h 	Handle to a virtual configuration returned 
				by dagdsm_create_configuration().
	@param[in]  filter 	The number of the virtual filter to swap out, this argument 
				should be in the range of 0 to 6.

	@return 		0 is returned to indicate success, -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.

	@note			The process for doing a filter swap is as follows:

                		1. Update the COLUR (construct, download and activate)
                		2. Install the swap filter on the card and enable. The
                   		   COLUR above will have made all the entries that relate
                   		   to the newly installed swap filter as "don't cares".
                		3. Mark the filter we are suppose to be replacing as
                   		   the new swap filter.
                		4. Update the COLUR (construct, download and activate)
                		5. Disable the replaced filter on the card (this is the
                   		   new swap filter).
	 */
	int
	dagdsm_do_swap_filter(DsmConfigH config_h, uint32_t filter);
		
	/** 
	@brief		Destroys all partial expressions and resets the stream output expressions.
	
	@param[in]  config_h	Handle to a virtual configuration returned by dagdsm_create_configuration().

	@return 		0 is returned to indicate success, 
				-1 is returned to indicate an error. Use dagdsm_get_last_error() 
				to retrieve the error code.
	 */
	int
	dagdsm_clear_expressions(DsmConfigH config_h);
	
	
	/**
	@brief		Creates a new partial expression and returns a handle to it.

	@param[in]  config_h 	Handle to a virtual configuration returned by dagdsm_create_configuration().
	
	@return 		A handle to the new partial expression, NULL is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	DsmPartialExpH
	dagdsm_create_partial_expr(DsmConfigH config_h);

	/**
	@brief		Returns the number of partial expressions currently created.

	@param[in]  config_h 	Handle to a virtual configuration returned 
				by dagdsm_create_configuration().
	@return 		A positive number is returned indicating the number of 
				partial expressions, -1 is returned to indicate an error. 
				Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_get_partial_expr_count(DsmConfigH config_h);
	
	/**
	@brief		Returns a handle to a partial expression with the given index.

	@param[in]  config_h 	Handle to a virtual configuration returned by dagdsm_create_configuration().
	@param[in]  index 	The index of the partial expression to retrieve.

	@return 		A handle to the partial expression at the given index, 
				NULL is returned to indicate an error. Use dagdsm_get_last_error() 
				to retrieve the error code.
	 */
	DsmPartialExpH
	dagdsm_get_partial_expr(DsmConfigH config_h, uint32_t index);
		
	/** 
	@brief		Returns the number of possible stream output expressions available in the 
			virtual configuration. This will return the exact same value as 
			dagdsm_get_filter_stream_count for a given card.

	@param[in]  config_h	Handle to a virtual configuration returned by 
				dagdsm_create_configuration().

	@return 		A positive number is returned indicating the number of possible 
				output expressions, -1 is returned to indicate an error. 
				Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_get_output_expr_count(DsmConfigH config_h);
	
	/** 
	@brief		Returns the handle to an output stream expression associated with 
			a particular receive stream. By convention receive stream are given 
			even numbers 0,2,4, ... etc.

	@param[in]  config_h 	Handle to a virtual configuration returned by 
				dagdsm_create_configuration().
	@param rx_stream 	The stream number to get the output expression for, this refers 
				to the receive streams therefore all stream numbers should even.

	@return 		A handle to the output expression for the given stream, 
				NULL is returned to indicate an error. Use dagdsm_get_last_error() 
				to retrieve the error code.
	 */
	DsmOutputExpH
	dagdsm_get_output_expr(DsmConfigH config_h, uint32_t rx_stream);
	/*@}*/

	/*****************************************************************
	 *
	 *  FILTERS
	 *
	 *****************************************************************/
	
	/** \defgroup DSMVirtualFilterConfig DSM Virtual Filter Configuration
	 */
	/*@{*/

	/**
	@brief		Clears the contents of a virtual filter, sets the value and mask 
			pair to all zero's (an accept all filter).Also clears the layer3 
			and 4 protocols.

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().

	@return 		0 if the filter was cleared, -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the 
				error code.
	 */
	int
	dagdsm_filter_clear (DsmFilterH filter_h);

	/**
	@brief		Enables or disables a virtual filter.This function
                	only sets the internal enabled state within the library,
                	this doesn't update the hardware until dagdsm_load_configuration
                	is called.

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  enable 	A non-zero value enables the filter, zero disables the filter.

	@return 		0 if the virtual filter was enabled/disabled, 
				-1 is returned to indicate an error. Use dagdsm_get_last_error() 
				to retrieve the error code.
	 */
	int
	dagdsm_filter_enable (DsmFilterH filter_h, uint32_t enable);
	
	/**
	@brief		Return the status of a virtual filter. Indicates whether the filter 
			is enabled or not. This function only sets the internal enabled 
			state within the library,this doesn't update the hardware until 
			dagdsm_load_configuration is called.

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  enabled 	The virtual filter to retrieve the status of.

	@return 		0 if the virtual filter was enabled, -1 is returned to 
				indicate an error. Use dagdsm_get_last_error() to 
				retrieve the error code.
	 */
	int
	dagdsm_filter_is_enabled (DsmFilterH filter_h, uint32_t *enabled);
	
	/**
	@brief 		Sets the first element that has the early termination option 
			set. The early termination byte tells the hardware what byte
                	of the filter to terminate on, packets smaller than the
                	early termination size automatically miss the filter. As with
                	dagdsm_enable_filter() this function doesn't update the
                	hardware until dagdsm_load_configuration() is called.

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  element 	The element number to set the early termination option on, 
				if no early termination is required set this parameter to 
				DSM_NO_EARLYTERM.
	@return 		0 if the early termination option was set, -1 is returned 
				to indicate an error. Use dagdsm_get_last_error() to retrieve 
				the error code.
	 */
	int
	dagdsm_filter_set_early_term_depth (DsmFilterH filter_h, uint32_t element);

	/**
	@brief		Gets the first element that has the early termination option set.

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter()
				or dagdsm_get_swap_filter().
	@param[out] element 	The element number to get the early termination option for.

	@return 		0 if the early termination option was set, -1 is returned 
				to indicate an error. Use dagdsm_get_last_error() to 
				retrieve the error code.
	 */
	int
	dagdsm_filter_get_early_term_depth (DsmFilterH filter_h, uint32_t * element);
	
	/**
	@brief		Sets or resets the raw mode of the filter.in this mode the 
			raw filter bytes are used for the filter rather than the settings
                	set by the user.

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  enable 	A non-zero value enables raw mode and zero disables raw mode.

	@return 		0 if raw mode was set, -1 is returned to indicate an error. 
				Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_set_raw_mode (DsmFilterH filter_h, uint32_t enable);
	
	/**
	@brief		Gets the status of raw mode for a filter.

	@param[in]  filter_h	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().

	@return 		1 if raw mode is set, 0 if not set and -1 is returned to 
				indicate an error. Use dagdsm_get_last_error() to retrieve 
				the error code.
	 */
	int
	dagdsm_filter_get_raw_mode (DsmFilterH filter_h);
	
	/**
	@brief		Sets the raw comparand and mask bytes of the virtual filter.
			This clears all previous settings stored for the filter,
                	i.e. what protocol type and port settings.

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  value 	Array that contains the raw bytes to load into the comparand 
				part of the filter. The array must contain at least size number 
				of bytes.
	@param[in]  mask 	Array that contains the raw bytes to load into the mask part 
				of the filter. The array must contain at least size number of bytes.
	@param[in]  size 	The number of bytes in both the value and mask arrays that should 
				be copied into the filter. Currently the maximum filter size is 64 bytes.

	@return	 		0 if the filter values were set otherwise -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_set_raw_filter (DsmFilterH filter_h, const uint8_t * value, const uint8_t * mask, uint32_t size);
	
	/**
 	@brief		 Get the raw byte filters.

	@param[in]  filter_h	Handle to the filter
	@param[out] value	Pointer to a value array, this array should be at least MATCH_DEPTH 
				bytes big.
	@param[out] mask	Pointer to a mask array, this array should be at least MATCH_DEPTH 
				bytes big.
	@param[in] size		The number of bytes in both the value and mask arrays that should 
				be copied from the filter.

	@return			0 if the filter was changed otherwise -1, this function
                		updates the last error code using dagdsm_set_last_error()
	*/
	int
	dagdsm_filter_get_raw_filter (DsmFilterH filter_h, uint8_t * value, uint8_t * mask, uint32_t size);    
	
	
	/** 
	@brief		Enables the VLAN option for filters on Ethernet cards.if enabled
                	the filter expects the ethertype to be VLAN (0x8100) and
                	adjust the rest of the fields (including the ethertype
                	field) down by four bytes.

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  enable 	A non-zero value enables the VLAN option, a zero value disables 
				the VLAN option.

	@return 		0 if the filter was updated otherwise -1 is returned to indicate 
				an error. Use dagdsm_get_last_error to retrieve the error code.	
	 */
	int
	dagdsm_filter_enable_vlan (DsmFilterH filter_h, uint32_t enable);
	
	/**
	@brief		Sets the VLAN ID to filter on, the card must be Ethernet and 
			have VLAN enabled.

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  id 		The 12-bit VLAN ID to filter on.
	@param[in]  mask 	The 12-bit mask of the id.

	@return 		0 if the filter values were set otherwise -1 is returned 
				to indicate an error. Use dagdsm_get_last_error() to retrieve 
				the error code.	
	 */
	int
	dagdsm_filter_set_vlan_id (DsmFilterH filter_h, uint16_t id, uint16_t mask);
	
	/**
 	@brief		gets the VLAN ID to filter on, the card must be ethernet and 
			have VLAN enabled. 

	@param[in]   filter_h	Handle to the filter
        @param[out]  id		Value to filter on, only the lower 12-bits are used.
        @param[out]  mask	Mask value, only the lower 12-bits used.

	@return 		0 if the filter was changed otherwise -1, this function
                		updates the last error code using dagdsm_set_last_error()
	*/
	int
	dagdsm_filter_get_vlan_id (DsmFilterH filter_h, uint16_t * id, uint16_t * mask);    
	
	/**
	@brief		Sets the layer 3 protocol to filter on. currently the only value 
			allowed is kIPv4.

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  type 	The layer 3 protocol type to filter on, currently the only 
				valid value is kIPv4.

	@return 		0 if the filter values were set otherwise -1 is returned to 
				indicate an error. Use dagdsm_get_last_error() to retrieve 
				the error code.
	 */
	int
	dagdsm_filter_set_layer3_type (DsmFilterH filter_h, layer3_type_t type);

	/**
 	@brief		Get the layer3 protocol to filter on, currently the only value 
			allowed is kIPv4.

	@param[in]  filter_h	Handle to the filter
	@param[out] type	Pointer to a layer type
	
	@return			0 if the filter was changed otherwise -1, this function
                		updates the last error code using dagdsm_set_last_error()
	*/
	int
	dagdsm_filter_get_layer3_type (DsmFilterH filter_h, layer3_type_t * type);    
	
	/**
	@brief		Sets the IP protocol to filter on. This doesn't clear any port filters 

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  protocol 	The 8-bit number that defines the IP protocol to filter on.

	@return 		0 if the filter IP protocol were set otherwise -1 is returned 
				to indicate an error. Use dagdsm_get_last_error() to retrieve
				the error code.
	 */
	int
	dagdsm_filter_set_ip_protocol (DsmFilterH filter_h, uint8_t protocol);
	
	/**
 	@brief		 Get IP protocol filter field.
	
	@param[in]  filter_h	Handle to the filter
	@param[out] protocol	Protocol field for filter
	
	@return			0 if the filter was changed otherwise -1, this function
                		updates the last error code using dagdsm_set_last_error()
	*/
	int
	dagdsm_filter_get_ip_protocol (DsmFilterH filter_h, uint8_t * protocol);
		
	
	/** 
	@brief		Sets the ICMP code to filter on. This has the effect of filtering
                	on the ICMP type but also update the layer 4 protocol field.
                	This will invalidate any port filters that are installed.

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  code 	The ICMP code to filter on.
	@param[in]  mask 	The mask to apply to the code.

	@return 		0 if the filter was updated, otherwise -1 is returned to 
				indicate an error. Use dagdsm_get_last_error() to retrieve 
				the error code.
	 */
	int
	dagdsm_filter_set_icmp_code (DsmFilterH filter_h, uint8_t code, uint8_t mask);

	
	/** 
 	@brief		Get the ICMP code filter field 
	
	@param[in]  filter_h	Handle to the filter
	@param[out] code	Code value to filter on
	@param[out] mask	Mask used fot the code
	
	@return			0 if the filter was changed otherwise -1, this function
                		updates the last error code using dagdsm_set_last_error()
	*/
	int
	dagdsm_filter_get_icmp_code (DsmFilterH filter_h, uint8_t * code, uint8_t * icmp_mask);    
	
	/**
	@brief		Sets the ICMP type to filter on.  this has the effect of filtering
                	on the ICMP type but also update the layer 4 protocol field.
                	This will invalidate any port filters that are installed.
		
	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  type 	The ICMP type to filter on.
	@param[in]  mask 	The mask to apply to the type.

	@return 		0 if the filter was updated, otherwise -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_set_icmp_type (DsmFilterH filter_h, uint8_t type, uint8_t mask);
	
	/** 
 	@brief		Get the ICMP type filter field 
	
	@param[in]  filter_h	Handle to the filter
	@param[out] code	Code value to filter on
	@param[out] mask	Mask used fot the code
	
	@return			0 if the filter was changed otherwise -1, this function
                		updates the last error code using dagdsm_set_last_error()
	*/
	int
	dagdsm_filter_get_icmp_type (DsmFilterH filter_h, uint8_t * type, uint8_t * icmp_mask);
    
	/**
	@brief		Sets the destination port to filter on for TCP and UDP filters.
			this function will fail if the protocol is not TCP or UDP. To set the
                	protocol call dagdsm_filter_set_ip_protocol().

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  port 	The port number to filter on, this value should not be converted 
				to network byte order, internally the library maintains the correct 
				byte ordering of values.
	@param[in]  mask 	The mask value to use for the port, as with the port argument the 
				mask should be in host byte order.

	@return 		0 if the filter was updated otherwise -1 is returned to indicate an 
				error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_set_dst_port (DsmFilterH filter_h, uint16_t port, uint16_t mask);

	/**
 	@brief		 get the destination port filter field 

	
	@param[in]  filter_h 	Handle to a virtual filter
	@param[out]  port 	The port number to filter on
	@param[out]  mask 	The mask value to use for the port

	@return			0 if the filter was changed otherwise -1, this function
                		updates the last error code using dagdsm_set_last_error()
	*/
	int
	dagdsm_filter_get_dst_port (DsmFilterH filter_h, uint16_t * port, uint16_t * mask);
	
	/**
	@brief		Sets the source port to filter on for TCP and UDP filters.
			this function will fail if the protocol is not TCP or UDP. To set the
                	protocol call dagdsm_filter_set_ip_protocol().

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  port 	The port number to filter on, this value should NOT be converted to 
				network byte order, internally the library maintains the correct byte 
				ordering of values.
	@param[in]  mask 	The mask value to use for the port, as with the port argument the 
				mask should be in host byte order.

	@return 		0 if the filter was updated otherwise -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_set_src_port (DsmFilterH filter_h, uint16_t port, uint16_t mask);

	/**
	@brief		Get the source port filter field. This function will fail if the 
			protocol is not TCP or UDP. To set the protocol call 
			dagdsm_filter_set_ip_protocol().

	@param[in]   filter_h 	Handle to a virtual filter
	@param[out]  port 	The port number to filter on
	@param[out]  mask 	The mask value to use for the port

	@return 		0 if the filter was updated otherwise -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_get_src_port (DsmFilterH filter_h, uint16_t * port, uint16_t * mask);    
	
	/**
	@brief		Sets the TCP flags to filter on.  the protocol must already
                	be configured for TCP for this function to succeed.

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  flags The 	flags to filter on, only the lower 6 bits of the value are used.
	@param[in]  mask 	The mask to apply to the flags, only the lower 6 bits of the mask are used.
	
	@return 		0 if the filter was updated, otherwise -1 is returned to indicate an error. 
				Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_set_tcp_flags (DsmFilterH filter_h, uint8_t flags, uint8_t mask);
	
	/* get the TCP flags filter field */
	/**
	@brief		Get the TCP flags filter field. the protocol must already
                	be configured for TCP for this function to succeed.

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[out]  flags 	The flags to filter on, only the lower 6 bits of the value are used.
	@param[out]  mask 	The mask to apply to the flags, before checking the value, only 
				the lower six bits are used
	
	@return 		0 if the filter was updated, otherwise -1 is returned to indicate an error. 
				Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_get_tcp_flags (DsmFilterH filter_h, uint8_t * flags, uint8_t * tcp_mask);    
	
	/**
	@brief		Sets the IPv4 source address to filter on.

	@param[in]  filter_h 	Handle to a virtual filter 
	@param[in]  src 	Sources address value to filter on.
	@param[in]  mask 	Pointer to an IP address that is used for Source address mask.

	@return 		0 if the filter was updated otherwise -1 is returned to indicate an error. 
				Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_set_ip_source (DsmFilterH filter_h, const struct in_addr * src, const struct in_addr * mask);

	/**
	@brief		Get IP source address filter field.

	@param[in]  filter_h 	Handle to a virtual filter 
	@param[out]  src 	Sources address value to filter on.
	@param[out]  mask 	Pointer to an IP address that is used for Source address mask.

	@return 		0 if the filter was updated otherwise -1 is returned to indicate an error. 
				Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_get_ip_source (DsmFilterH filter_h, struct in_addr * src, struct in_addr * mask);    
	
	/**
	@brief		Sets the IPv4 destination address to filter on.

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() or 
				dagdsm_get_swap_filter().
	@param[in]  dst 	Destination address value to filter on.
	@param[in]  mask 	Destination address mask.

	@return 		0 if the filter was updated otherwise -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_set_ip_dest (DsmFilterH filter_h, const struct in_addr * dst, const struct in_addr * mask);

	/**
	@brief		Get the IP destination filter.

	@param[in]   filter_h 	Handle to a virtual filter 
	@param[out]  dst 	Destination address value to filter on.
	@param[out   mask 	Destination address mask.

	@return 		0 if the filter was updated otherwise -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_get_ip_dest (DsmFilterH filter_h, struct in_addr * dst, struct in_addr * mask);    
	
	/**
	@brief		Updates the filter to reject IPv4 fragments. Sets the filter to either 
			accept of discard IP fragments

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  enable 	A non-zero value will enable the IPv4 fragment rejection 
				option in the filter. A zero value disables the option.

	@return 		0 if the filter was updated otherwise -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_ip_fragment (DsmFilterH filter_h, uint32_t enable);

	/**
	@brief		Get IP fragment filter field to filter out fragmented packets. 

	@param[in]  filter_h 	Handle to a virtual filter 
	@param[out]  enable 	Indicates if ip fragment is enabled

	@return 		0 if the filter was updated otherwise -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */

	int
	dagdsm_filter_get_ip_fragment (DsmFilterH filter_h, uint32_t * enable);    
	
	/**
	@brief		Sets the IP header length to filter on.

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() or 
				dagdsm_get_swap_filter().
	@param[in]  ihl 	The IP header length in 32-bit words to filter on, only the lower 
				4 bits of the value are used.

	@return 		0 if the filter was updated, otherwise -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_set_ip_hdr_length (DsmFilterH filter_h, uint8_t ihl);

	/**
	@brief		Get IP header length filter field.

	@param[in]  filter_h 	Handle to a virtual filter
	@param[out]  ihl 	IP HDR Lenght

	@return 		0 if the filter was updated, otherwise -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */

	int
	dagdsm_filter_get_ip_hdr_length (DsmFilterH filter_h, uint8_t * ihl);    
		
	/**
	@brief		Sets the ethertype of the Ethernet frame to filter on (only 
			valid for Ethernet cards).

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  ethertype 	The ethertype to filter on.
	@param[in]  mask 	The mask to use for the ethertype.
	@return 		0 if the filter values were set otherwise -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_set_ethertype (DsmFilterH filter_h, uint16_t ethertype, uint16_t mask);

	/**
	@brief		Get the ethertype filter field (only valid for Ethernet cards).

	@param[in]  filter_h 	Handle to a virtual filter 
	@param[in]  ethertype 	Pointer to a value array, this array should be at least FILTER_COUNT
				bytes big
	@param[in]  mask 	Pointer to a mask array, this array should be at least FILTER_COUNT
				bytes big

	@return 		0 if the filter values were set otherwise -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_get_ethertype (DsmFilterH filter_h, uint16_t * ethertype, uint16_t * mask);    
	
	/** 
 	@brief		Sets the source MAC address in the Ethernet header to filter on 
			(only valid for Ethernet cards).

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  src 	The source MAC address to filter on.
	@param[in]  mask 	The mask to use for the MAC address.

	@return 		0 if the filter values were set otherwise -1 is returned to indicate an error.
				Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_set_mac_src_address (DsmFilterH filter_h, const uint8_t src[6], const uint8_t mask[6]);
	
	/** 
 	@brief		Get MAC source address filter field (only valid for Ethernet cards).

	@param[in]  filter_h 	Handle to a virtual filter
				or dagdsm_get_swap_filter().
	@param[out]  src 	Pointer to a value array, this array should be at least
				FILTER_COUNT bytes big.
	@param[out]  mask 	Pointer to a mask array, this array should be at least 
				FILTER_COUNT bytes big.

	@return 		0 if the filter values were set otherwise -1 is returned to indicate an error.
				Use dagdsm_get_last_error() to retrieve the error code.
	 */

	int
	dagdsm_filter_get_mac_src_address (DsmFilterH filter_h, uint8_t * src, uint8_t * mask);
    
	/**
	@brief		Sets the destination MAC address in the Ethernet header to filter on 
			(only valid for Ethernet cards).

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() or dagdsm_get_swap_filter().
	@param[in]  dst 	The destination MAC address to filter on.
	@param[in]  mask 	The mask to use for the MAC address.
	
	@return 		0 if the filter values were set otherwise -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.	
	 */
	int
	dagdsm_filter_set_mac_dst_address (DsmFilterH filter_h, const uint8_t dst[6], const uint8_t mask[6]);
	
	/**
	@brief		Get MAC destination address filter field (only valid for Ethernet cards)

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[in]  dst 	The destination MAC address to filter on.
	@param[in]  mask 	The mask to use for the MAC address.
	
	@return 		0 if the filter values were set otherwise -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.	
	 */
	int
	dagdsm_filter_get_mac_dst_address (DsmFilterH filter_h, uint8_t * dst, uint8_t * mask);    
	
	/**
	@brief		Sets the PoS HDLC/PPP 32-bit header to filter on (only valid on SONET cards)

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() or
				dagdsm_get_swap_filter().
	@param[out]  hdlc_hdr 	The 32-bit HDLC/PPP PoS header to filter on.
	@param[out]  mask 	The mask to use for HDLC/PPP header.

	@return 		0 if the filter values were set otherwise -1 is returned to indicate
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_set_hdlc_header (DsmFilterH filter_h, uint32_t hdlc_hdr, uint32_t mask);

	/**
	@brief		Get the HDLC header filter field (only valid on SONET cards)

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() or
				dagdsm_get_swap_filter().
	@param[out]  hdlc_hdr 	The 32-bit HDLC/PPP PoS header to filter on.
	@param[out]  mask 	The mask to use for HDLC/PPP header.

	@return 		0 if the filter values were set otherwise -1 is returned to indicate
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_get_hdlc_header (DsmFilterH filter_h, uint32_t * hdlc_hdr, uint32_t * mask);    


	/**
	@brief		Copies the contents of one virtual filter to another virtual filter. 
			It is not nesscary for the two filters to be of the same configuration, 
			however they must both be the same network type either sonet or ethernet filters.

	@param[in]  dst_filter_h Handle to a virtual filter returned by dagdsm_get_filter() 
				 or dagdsm_get_swap_filter(), this is the destination filter
				 that is copied over.
	@param[in]  src_filter_h Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter(), this is the source filter.

	@return 		0 if the filter was copied otherwise -1 is returned to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_filter_copy (DsmFilterH dst_filter_h, DsmFilterH src_filter_h);

	/**
	@brief		Copies the raw filter data to user supplied buffer.

	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() 
				or dagdsm_get_swap_filter().
	@param[out]  value 	Pointer to an array that will receive the comparand part of the filter.
	@param[out]  mask 	Pointer to an array that will receive the mask part of the filter.
	@param[in]  max_size 	Pointer to an array that will receive the mask part of the filter.

	@return 		A positive number indicating how many bytes were copied into both arrays, 
				-1 is returned to indicate an error. Use dagdsm_get_last_error() to 
				retrieve the error code.
	 */
	int
	dagdsm_filter_get_values (DsmFilterH filter_h, uint8_t * value, uint8_t * mask, uint32_t max_size);
	/*@}*/	

	/*****************************************************************
	 *
	 *  PARTIAL EXPRESSIONS
	 *
	 *****************************************************************/

	/** \defgroup DSMPartialExprs Partial Expressions
	 */
	/*@{*/

	/**
	@brief		Sets the filter parameter in a partial expression. this
                	has the effect of ORing the filter (or the inverse of the
                	filter) with the existing partial expression.

	@param[in]  expr_h 	Handle to a partial expression returned by either 
				dagdsm_create_partial_expr() or dagdsm_get_partial_expr().
	@param[in]  filter 	The virtual filter number to set in the expression.
	@param[in]  invert 	A non-zero value will invert the filter parameter in the expression, 
				zero will not invert the parameter.

	@return 		0 if the filter was added otherwise 
				-1 is returned to indicate an error. Use dagdsm_get_last_error() 
				to retrieve the error code.
	 */
	int
	dagdsm_expr_set_filter (DsmPartialExpH expr_h, uint32_t filter, uint32_t invert);

	/**
	@brief		Gets the maximum number of filters possible for the Partial expression
                	has the effect of ORing the filter (or the inverse of the
                	filter) with the existing partial expression

	@param[in]  expr_h 	Handle to a partial expression returned by either 
				dagdsm_create_partial_expr() or dagdsm_get_partial_expr().
	@param[in]  count	The number of filters

	@return 		0 if the filter was added otherwise 
				-1 is returned to indicate an error. Use dagdsm_get_last_error() 
				to retrieve the error code.
	 */
	int
	dagdsm_expr_get_filter_count (DsmPartialExpH expr_h,uint32_t * count);

	/**
	@brief		Get a filter from the partial expression, this
                	has the effect of ORing the filter (or the inverse of the
                	filter) with the existing partial expression.

	@param[in]  expr_h 	Handle to a partial expression returned by either 
				dagdsm_create_partial_expr() or dagdsm_get_partial_expr().
	@param[out]  filter 	The number of the filter to get should be in the 
				range of 0-6
	@param[out]  invert 	If 1 the filter is inverted before ORing with the 
				partial expression

	@return 		0 if the filter was added otherwise 
				-1 is returned to indicate an error. Use dagdsm_get_last_error() 
				to retrieve the error code.
	 */
	int
	dagdsm_expr_get_filter (DsmPartialExpH expr_h, uint32_t filter, uint32_t * invert);
	
	/**
	@brief		Sets the physical interface (port) parameter in a partial expression.
			this has the effect of ORing the interface (or the inverse of the
                	interface) with the existing partial expression.

	@param[in]  expr_h 	Handle to a partial expression returned by either 
				dagdsm_create_partial_expr() or dagdsm_get_partial_expr().
	@param[in]  iface 	The physical interface number to set in the partial expression.
	@param[in]  invert 	A non-zero value will invert the interface parameter in the 
				expression. A zero value will not invert the parameter.

	@return 		0 if the interface was added otherwise -1 is returned 
				to indicate an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_expr_set_interface (DsmPartialExpH expr_h, uint32_t iface, uint32_t invert);
	
	/**
	@brief		Gets the number fo interface from the partial expression.

	@param[in]  expr_h 	Handle to a partial expression returned by either 
				dagdsm_create_partial_expr() or dagdsm_get_partial_expr().
	@param[in]  count	The number of interfaces

	@return 		0 if the interface  was added otherwise -1 is returned 
				to indicate an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_expr_get_interface_count (DsmPartialExpH expr_h, uint32_t * count);

	/**
	@brief		Gets an interface from the partial expression expression.	
		
	@param[in]  expr_h 	Handle to a partial expression returned by either 
				dagdsm_create_partial_expr() or dagdsm_get_partial_expr().
	@param[in]  iface 	The physical interface number to set in the partial expression.
				should be in the range of 0-3
	@param[in]  invert 	A non-zero value will invert the interface parameter in the 
				expression. A zero value will not invert the parameter.

	@return 		0 if the interface was added otherwise -1 is returned 
				to indicate an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_expr_get_interface (DsmPartialExpH expr_h, uint32_t iface, uint32_t * invert);

	/**
	@brief		Sets the load balancing (steering) parameter in a partial expression.

	@param[in]  expr_h 	Handle to a partial expression returned by either 
				dagdsm_create_partial_expr() or dagdsm_get_partial_expr().
	@param[in]  hlb 	The number of the load balancing algorithm to use as a parameter 
				in the partial expression. The following constants are defined 
				and can be used for this argument: kCRCLoadBalAlgorithm, 
				kParityLoadBalAlgorithm.
	@param[in]  invert 	A non-zero value will invert the load balancing parameter in the expression, 
				a zero value will not invert the parameter.

	@return 		0 if the partial expression was updated otherwise -1 is returned to 
				indicate an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_expr_set_hlb (DsmPartialExpH expr_h, uint32_t hlb, uint32_t invert);

	/**
	@brief		Gets a HLB count from the partial expression.

	@param[in]  expr_h 	Handle to a partial expression returned by either 
				dagdsm_create_partial_expr() or dagdsm_get_partial_expr().
	@param[in]  count 	The HLB count to get from the partial expression.

	@return 		0 if the HLB output was added otherwise -1 is returned to 
				indicate an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_expr_get_hlb_count (DsmPartialExpH expr_h, uint32_t * count);

	/**
	@brief		 Gets a HLB output from the partial expression expression

	@param[in]  expr_h 	Handle to a partial expression returned by either 
				dagdsm_create_partial_expr() or dagdsm_get_partial_expr()
	@param[in]  hlb		The HLB output to get, should be either 0 or 1
	@param[out]  invert	If 1 the HLB hash value output is inverted before
                                ORing with the partial expression.

	@return 		0 if the HLB output was added otherwise -1 is returned to 
				indicate an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */	
	int
	dagdsm_expr_get_hlb (DsmPartialExpH expr_h, uint32_t hlb, uint32_t * invert);

	/**
	@brief		Adds a HLB Hash association table output to the partial expression expression.

	@param[in]  expr_h 	Handle to a partial expression returned by either 
				dagdsm_create_partial_expr() or dagdsm_get_partial_expr()
	@param[in]  hlb_hash_value The hlb hash value to add, should be value range in 0-7
	@param[in]  invert	If 1 the HLB hash value output is inverted before
                                ORing with the partial expression.

	@return 		0 if the HLB output was added otherwise -1 is returned to 
				indicate an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_expr_set_hlb_hash(DsmPartialExpH expr_h, uint32_t hlb_hash_value, uint32_t invert);

	/**
	@brief		Calculates the output of a partial expression given a complete 
			set of input parameters.

	@param[in]  expr_h 	Handle to a partial expression returned by either 
				dagdsm_create_partial_expr() or dagdsm_get_partial_expr().
	@param[in]  filters 	Bitmasked value that should contain the filters expected 
				to hit, see the comments below for more information.
	@param[in]  iface 	The interface number to check against, only the lower 
				2 bits of this value are used.
	@param[in]  hlb0 	A non-zero value to indicate the output of the CRC load 
				balancing algorithm is true, a zero value indicates the output is false.
	@param[in]  hlb1 	A non-zero value to indicate the output of the parity load 
				balancing algorithm is true, a zero value indicates the output is false.

	@return 0 		if partial expression evaluates to a false output, 1 if the partial 
				expression evaluates to a true output and -1 to indicate an error. 
				Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_compute_partial_expr_value (DsmPartialExpH expr_h, uint32_t filters, uint8_t iface, uint32_t hlb0, uint32_t hlb1);
	
	/**
	@brief		Calculates the output of a partial expression given a complete 
			set of input parameters. This version of the function is specific 
			to Dag4.5Z and Dag8.2Z cards

	@param[in]  expr_h 	Handle to a partial expression returned by either 
				dagdsm_create_partial_expr() or dagdsm_get_partial_expr().
	@param[in]  filters 	Bitmasked value that should contain the filters expected to 
				hit, see the comments below for more information.
	@param[in]  iface 	The interface number to check against, only the lower 2 
				bits of this value are used.
	@param[in]  hlb0 	A non-zero value to indicate the output of the CRC load balancing 
				algorithm is true, a zero value indicates the output is false.
	@param[in]  hlb1 	A non-zero value to indicate the output of the parity load balancing 
				algorithm is true, a zero value indicates the output is false.

	@return 		0 if partial expression evaluates to a false output, 1 if the 
				partial expression evaluates to a true output and -1 to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_compute_partial_expr_value_z (DsmPartialExpH expr_h, uint32_t filters, uint8_t iface, uint32_t hlb_hash_value);
	/*@}*/	
	
	/*****************************************************************
	 *
	 *  OUTPUT EXPRESSIONS
	 *
	 *****************************************************************/
	
	/** \defgroup DSMOutputExprs Output Expressions 
	 */
	/*@{*/

	/**
	@brief		Adds a partial expression to the output expression.This
                	has the effect of ANDing the output of the partial
               		expression with the existing output expression.

	@param[in]  output_h 	Handle to an output expression returned by 
				dagdsm_get_output_expression().
	@param[in]  partial_h 	Handle to a partial expression returned by either 
				dagdsm_create_partial_expr() or dagdsm_get_partial_expr().
	@param[in]  invert 	A non-zero value will invert the output of the partial expression, 
				a zero value will not invert the partial expression output.

	@return 		0 if the output expression was updated otherwise -1 is returned to 
				indicate an error. Use dagdsm_get_last_error to retrieve the error code.
	 */
	int
	dagdsm_expr_add_partial_expr (DsmOutputExpH output_h, DsmPartialExpH partial_h, uint32_t invert);

	/** 
 	@brief	Gets the number of inverted partial expression to an output expression 
	
	@param[in]  output_h	Handle to the output expression the output expression
	@param[out] count	The number of inverted partial expression
	
	@return 		0 if the HLB output was added otherwise -1, this function
                		updates the last error code using dagdsm_set_last_error()
	*/
	int
	dagdsm_expr_get_inverted_partial_expr_count (DsmOutputExpH output_h, uint32_t * count); 

	/** 
 	@brief		Gets the indexed inverted partial expression to an output expression
	
	@param[in]  output_h	Handle to the output expression the output expression
	@param[in] index	The index of partial expression
	
	@return 		0 if the HLB output was added otherwise -1, this function
                		updates the last error code using dagdsm_set_last_error()
	*/
	DsmPartialExpH
	dagdsm_expr_get_inverted_partial_expr(DsmOutputExpH output_h, uint32_t index); 

	/** 
 	@brief		Gets the number of partial expression to an output expression
	
	@param[in]  output_h	Handle to the output expression the output expression
	@param[out] count	The number of partial expressions
	
	@return 		0 if the HLB output was added otherwise -1, this function
                		updates the last error code using dagdsm_set_last_error()
	*/
	int
	dagdsm_expr_get_partial_expr_count (DsmOutputExpH output_h, uint32_t * count); 

	/** 
 	@brief		Gets the indexed partial expression to an output expression
	
	@param[in]  output_h	Handle to the output expression the output expression
	@param[out] index	The index of partial expression 
	
	@return 		0 if the HLB output was added otherwise -1, this function
                		updates the last error code using dagdsm_set_last_error()
	*/
	DsmPartialExpH	
	dagdsm_expr_get_partial_expr(DsmOutputExpH output_h, uint32_t index);     

	/**
	@brief		Computes the output value of an output expression, given a 
			fixed set of input parameters.

	@param[in]  output_h 	Handle to an output expression returned by 
				dagdsm_get_output_expression().
	@param[in]  filters 	Bit-masked value that should contain the filter outputs, 
				one bit per filter, see the comments below for more information.
	@param[in]  iface 	The interface number to check against, only the lower 2 
				bits of this value are used.
	@param[in]  hlb0 	A non-zero value to indicate the output of the CRC load balancing 
				algorithm is true, a zero value indicates the output is false.
	@param[in]  hlb1 	A non-zero value to indicate the output of the parity load balancing 
				algorithm is true, a zero value indicates the output is false.

	@return 		0 if the stream output expression evaluates to a false output, 
				1 if the stream output expression evaluates to a true output and 
				-1 to indicate an error. Use dagdsm_get_last_error() to retrieve 
				the error code.
	 */
	int
	dagdsm_compute_output_expr_value (DsmOutputExpH output_h, uint32_t filters, uint8_t iface, uint32_t hlb0, uint32_t hlb1);

	/**
	@brief		computes whether, given the supplied input, the output expression 
			will accept special version for 4.5Z 8.2Z

	@param[in]  output_h 	Handle to an output expression returned by 
				dagdsm_get_output_expression().
	@param[in]  filters 	Bit-masked value that should contain the filter outputs, 
				one bit per filter, see the comments below for more information.
	@param[in]  iface 	The interface number to check against, only the lower 2 
				bits of this value are used.
	@param[in]  hlb_hash_value The hlb hash value to add, should be value range in 0-7

	@return 		0 if the stream output expression evaluates to a false output, 
				1 if the stream output expression evaluates to a true output and 
				-1 to indicate an error. Use dagdsm_get_last_error() to retrieve 
				the error code.
	 */
	int
	dagdsm_compute_output_expr_value_z (DsmOutputExpH output_h, uint32_t filters, uint8_t iface, uint32_t hlb_hash_value);
	/*@}*/



	/*****************************************************************
	 *
	 *  COUNTERS
	 *
	 *****************************************************************/

	/** \defgroup DSMCounters Counters
	 */
	/*@{*/

	/**
	@brief		Latches and clears all the counters inside the card.

	@param[in]  config_h 	Handle to a virtual configuration returned by 
				dagdsm_create_configuration().

	@return 		0 if the counters were latched and cleared otherwise -1 
				to indicate an error. Use dagdsm_get_last_error() to 
				retrieve the error code.
	 */
 	int
	dagdsm_latch_and_clear_counters (DsmConfigH config_h);

	/**
	@brief		Reads the contents of a latched filter counter and clears the value.

	@param[in]  config_h 	Handle to a virtual configuration returned by dagdsm_create_configuration().
	@param[in]  filter_h 	Handle to a virtual filter returned by dagdsm_get_filter() or 
				dagdsm_get_swap_filter().
	@param[in]  value_p 	Pointer to a 32-bit variable that receives the latched filter count.

	@return 		0 if the latched counter value was read otherwise -1 to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_read_filter_counter (DsmConfigH config_h, DsmFilterH filter_h, uint32_t *value_p);
	
	/**
	@brief		Reads the contents of a latched load balancing algorithm counter 
			and clears the value.

	@param[in]  config_h 	Handle to a virtual configuration returned by 
				dagdsm_create_configuration().
	@param[in]  hlb0_p 	Pointer to a 32-bit variable that receives the latched CRC load 
				balancing algorithm count. This is an optional output, pass NULL 
				if this counter value is not required.
	@param[in]  hlb1_p 	Pointer to a 32-bit variable that receives the latched parity load 
				balancing algorithm count. This is an optional output, pass NULL 
				if this counter value is not required.

	@return 		0 if the latched counter value was read otherwise -1 to indicate 
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_read_hlb_counter (DsmConfigH config_h, uint32_t *hlb0_p, uint32_t *hlb1_p);

	/**
	@brief		Reads the contents of the latched drop counter and clears the value.

	@param[in]  config_h 	Handle to a virtual configuration returned by 
				dagdsm_create_configuration().
	@param[in]  value_p 	Pointer to a 32-bit variable that receives the latched drop count.

	@return 		0 if the latched counter value was read otherwise -1 to indicate
				an error. Use dagdsm_get_last_error() to retrieve the error code.
	 */
	int
	dagdsm_read_drop_counter (DsmConfigH config_h, uint32_t *value_p);

	/** 
 	@brief		Reads the contents of the latched packet counter for a particular receive 
			stream and clears the value.

	@param[in]  config_h 	Handle to a virtual configuration returned by 
				dagdsm_create_configuration().
	@param[in]  stream 	The receive stream to read the packet count from.
	@param[in]  value_p 	Pointer to a 32-bit variable that receives the latched 
				stream packet count.

	@return 		0 if the latched counter value was read otherwise -1 to 
				indicate an error. Use dagdsm_get_last_error() to retrieve 
				the error code.
	 */	
	int
	dagdsm_read_stream_counter (DsmConfigH config_h, uint32_t stream, uint32_t *value_p);
	/*@}*/
	

	/*****************************************************************
	 *
	 *  DEBUGGING
	 *
	 *****************************************************************/

	/**
	@brief		Returns the last generated error code.

	@return 		The last error code generated by one of the DSM functions, 
				refer to the function in question for possible error codes.
	 */
	int
	dagdsm_get_last_error (void);

	/**
 	@brief		User function to display the information in the COLUR

	@param[in]  config_h	Handle to a configuration
        @param[in]  stream      Output stream to dump the data to
	@param[in]  bank	The bank to read and display
	
	@return 		0 is returned to indicate success, -1 is returned to indicate 
                                an error. Use dagdsm_get_last_error() to retrieve the error code.
	*/
	int
	dagdsm_display_colur (DsmConfigH config_h, FILE * stream, uint32_t bank, uint32_t flags);
	
	/**
 	@brief		User function to display the contents of a filter

	@param[in]  config_h	Handle to a configuration
        @param[in]  stream      Output stream to dump the data to
	@param[in]  flags	If an internal filter
	
	@return 		0 is returned to indicate success, -1 is returned to indicate 
                                an error. Use dagdsm_get_last_error() to retrieve the error code.
	*/
	int
	dagdsm_display_filter (DsmConfigH config_h, FILE * stream, uint32_t filter, uint32_t flags);

	/**
 	@brief		User function to display the counter information

	@param[in]  config_h	Handle to a configuration
        @param[in]  stream      Output stream to dump the data to
	
	@return 		0 is returned to indicate success, -1 is returned to indicate 
                                an error. Use dagdsm_get_last_error() to retrieve the error code.
	*/
	int
	dagdsm_display_counters (DsmConfigH config_h, FILE * stream);

	/**
 	@brief		User function to compare the filter on the card with a buffer

	@param[in]  config_h	Handle to a configuration
        @param[in]  filter
	@param[in]  value
	@param[in]  mask
	@param[in]  size
	@param[in]  read_value
	@param[in]  read_mask
	
	@return 		If the filters are a match then 1 is returned, if not
                		a match 0 is returned, if an error occurred -1 is returned.
	*/
	int
	dagdsm_compare_filter (DsmConfigH config_h, uint32_t filter, const uint8_t * value, const uint8_t * mask, uint32_t size, uint8_t * read_value, uint8_t * read_mask);
	
	/**
 	@brief		Compares the COLUR installed on the card with the one
                	supplied in the buffer. If both are the same 0 is returned,
                	otherwise 1 is returned.

	@param[in]  config_h	Handle to a configuration
	@param[in]  bank	The COLUR on the card to compare against
        @param[in]  data_p	The array used to compare the two COLUR's
	@param[in]  size	The size of the buffer to compare
	
	@return 		1 if the buffer and the COLUR on the card match, 0 if
                		they are not a match. -1 is returned if there is an error,
                		call dagdsm_get_last_error to reteive the error code.
	*/
	int
	dagdsm_compare_colur (DsmConfigH config_h, uint32_t bank, const uint8_t * data_p, uint32_t size);
	
	
	/* tests a packet against the current set of filters and expression to see which receive stream it should go to */
	/**	
 
 	@brief		Tests a packet against the current set of filters and
                	expression to see which receive stream it should go to,
                	if any. This function is only used for debugging, it is
                	used to verify the firmware output is correct. The
                	interface is assumed to be in the ERF header.
 
	@param[in]  config_h	Handle to a configuration
        @param[in]  pkt_p 	Pointer to the ERF record to check, the buffer 
				should have a complete ERF record including
                                the ERF header.
        @param[in]  rlen	The length of the ERF record.
        @param[out] lookup      Optional value that accepts the 12-bit lookup value.

	@return 	       -1 if the packet should be dropped or there was an error,
                		otherwise a positive number representing which ERF stream
                		the packet should have come from. To determine if -1 was
                		returned for an error call dagdsm_get_last_error().
                
 	@note			The HLB part of the packet is unknown at the moment, this
                		function doesn't generate a HLB value for the packet therefore
                		it is not a real test, this needs to be fixed.
 	*/
	int
	dagdsm_compute_packet (DsmConfigH config_h, uint8_t * pkt_p, uint16_t rlen, uint32_t * lookup);
/*@}*/



#ifdef __cplusplus
}
#endif /* __cplusplus */



#endif      // DAGDSM_H

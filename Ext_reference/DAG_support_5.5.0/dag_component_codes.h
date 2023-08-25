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
 * of it may be redistributed, published or disclosed except as outlined in
 * the written contract supplied with this product.
 *
 */


#ifndef DAG_COMPONENT_CODES_H
#define DAG_COMPONENT_CODES_H


/* Codes for identifying components. */
/**
\defgroup ConfigAndStatusAPI Configuration and Status API
The interface that exposes the Components and Attributes that configures the card..
*/
/*@{*/
/**
\defgroup CompCodes Component Codes
*/
/*@{*/

/**
 * \ingroup CompCodes
 * Codes for identifying components.
 * */
typedef enum
{
    /**
    Represents and invalid component. In general this value is returned when there is an error.
    For example when a component is requested in a card that does not have that component
    */
    kComponentInvalid,
    
    /**
    The root component is a special component that has no attributes.  All other components are children of the root component.
    */
    kComponentRoot,
//NEW after 257 and 258
    /**
    Represents a Connection component.  Allows connections to be added and removed.
    */
	kComponentConnectionSetup,

//NEW after 257 and 258
    /**
    Represents a Connection component.  Allows connections to be queried.
    */
	kComponentConnection,


    /** 
     * Represents a deframer component. A deframer breaks down a SONET Frame when received and extracts the data.
     */
    kComponentDeframer,

//NEW after 257
    /**
    Present on modules with a drop counter. Hash load balancing firmware supports this component.
    */
    kComponentDrop,

//located on different spot in 257
    /**
    The DUCK (DAG Universal Clock Kit). Used to configure the time keeping abilities of the DAG card.
    */
    kComponentDUCK,

     /**
     * Represents the E1T1 deframer/framer
     */
    kComponentE1T1,
    
    /**
     * Represents the framer component. A Framer encapsulates data within a SONET Frame for transmit
     */
    kComponentFramer,
    
    /**
    Represents a demapper component. Demapper components are used to provide a higher level of functionality over the base framer.
    */
    kComponentDemapper,
    
    /**
    Represents a mapper component. Mapper components are used to provide a higher level of functionality over the base framer.
    */
    kComponentMapper,
    
    /**
    The generic packet processor component captures the packet.  It can be told to capture, using the snaplen attribute, a fixed number of bytes from the wire.
    */
    kComponentGpp,
    
    /**  
     The size reduced packet processor component captures the packet. This is the same as kComponentGpp and is kept just in case for API backwards compatibility. 
     *     Depreciated please use kCOmponetGpp instead 
    */
    kComponentSRGPP = kComponentGpp ,

    /*
      Previous used for SRGPP component for ABI compatibilyty keep  dummy   now srGPP is the same as GPP.
    */

    kComponentSRDUMMY,
    
    /**
    Represents the LED controller for the pod 
    */
    kComponentLEDController,
    
    /**
     * Represents the statistics module for each port
     */
    kComponentMiniMacStatistics,
    
    /**
    Represents the mux component. This component can be used to merge or
    split the receive streams on the card.
    */
    kComponentMux,
    
    /**
    Represents the physical layer on a card.
    */
    kComponentPhy,
    
    /**
    The packet capture statistics module.
    */
    kComponentPacketCapture,
    
    /**
    The port component is generally used to configure and read attributes specific to the line.
    The specific attributes differ widely between cards.   However, there is some commonality depending on the protocol for which the card is designed.
    For example, all Ethernet cards have similar attributes associated with their port component.  However, a SONET card port component will not share many attributes in common with an Ethernet card's port component.
    */
    kComponentPort,
    
    /**
    The stream component models a receive stream or transmit stream.  The number of streams depends on the loaded firmware image.  This component can be used to allocate memory to the stream it represents.
    */
    kComponentStream,

//NEW after 257
    /**
    The steering component. This allows one to choose a algorithm to steer the
    received packets. The steering algorithm allows the packets to be directed
    to different memory holes depending on for example a crc hash function.
    \sa SteeringCodes.
    */
    kComponentSteering,
    
    /**
    Represents the optics component on the card. 
    */
    kComponentOptics,
    
    /**
    Represents the terf register on cards that have the appropriate firmware loaded
    */
    kComponentTerf,
    
//NEW after 257
    /**
    Represents the terf subcomponent with the time release feature
    */
    kComponentTrTerf,
    
    
    /**
    The PCI Burst Manager component handles the transfer of captured packets to the receive memory stream and from the transmit stream back to the card for transmitting.
    This component can be used to check the size of the memory buffer allocated, and to count the number of transmit and receive streams present.
    On some cards one can set the overlap attribute to enable inline forwarding of packets.
    */
    kComponentPbm,
    
    /**
    The hardware monitor (temperature, fan, voltage etc..)
    */
    kComponentHardwareMonitor,
    
    /**
     Single Attribute Counter Module  
     */ 
    kComponentSingleAttributeCounter,

    /**
     * Represent the SC256 component
     */
    kComponentSC256,
    
    /**
     ERF MUX Component. This component can be used to merge or
    split the receive streams on the card.
    */
    kComponentErfMux,
    
    /** 
     * \warning NOT IMPLEMENTED
     * Pseudo-components provided for convenience. 
     */
    kComponentMem,
    
    /**
     * Controls attributes of the SONET/SDH deframer.
     */
    kComponentSonic,

//NEW after 257
    /**
     * The XGMII component.
     */
	kComponentXGMII,

//NEW after 257
    /**
     * The XGMII statistics component.
     */
	kComponentXGMIIStatistics,
    
//NEW after 257
    /**
     * The DPA Dynamic Phase Alignment component.
     */
	kComponentDPA,
    
//NEW after 257 and 258
    /**
     * The Counter Statistic Interface component.
     */
	kComponentInterface,
    
//NEW after 257 and 258
    /**
     * The Counter component.
     */
	kComponentCounter,
    
//NEW after 257 and 258
    /**
     * The General register component.
     */
	kComponentGeneral,
    
    /** 
     * \warning NOT IMPLEMENTED
     * Pseudo-components provided for convenience. 
     */    
    kComponentAllPorts,
    
    /** 
     * \warning NOT IMPLEMENTED
     * Pseudo-components provided for convenience. 
     */    
    kComponentAllGpps,
    
//NEW after 257
    /**
     * The Hlb component
     */
    kComponentHlb,

//NEW after 257 and 258
    /**
     * The Physical Coding Sublayer (PCS) component
     */
    kComponentPCS,

//NEW after 257 and 258
    /** 
     * Card Information 
     **/
    kComponentCardInfo,	
      
//NEW after 257 and 258
/** 
     * Sonet Packe ptoccesor component on the new cards
     **/
    kComponentSonetPP,	

//NEW after 310
//NEW after 257 and 258
    /**
     *Colour Association Table (CAT) module of FCSBM.
     */ 
    kComponentCAT,

//NEW after 310
//NEW after 257 and 258
    /**
    *Hash Load Balancer V2 Moudle of FCSBM.
    */
    kComponentHAT,

//NEW after 310
//NEW after 257 and 258
/** 
     * Component for new per stream features like snap length per stream
     **/
    kComponentStreamFeatures,


//NEW after 310
//NEW after 257 and 258
/** 
     * Component for IPF information 
     **/
    kComponentIPF,

//NEW after 310
//NEW after 257 and 258
    /** 
    *The Cross Point Switch Component for the Infiniband Cards.
    */ 
    kComponentCrossPointSwitch,

//NEW after 310
//NEW after 257 and 258
    /**
     *The Framer Module for the Infiniband Cards.
     */ 
    kComponentInfiniBandFramer,
    /**
     * The Infiniband Classifier Module 
     */ 
    kComponentInfinibandClassifier,

    /**
     * The module that allows transmission of a small amount of data at full line rate.
     */
    kComponentMemTx,

    /**
     * The module represents the DSM module in config and status API
     */
    kComponentDSM,

    /**
     * The module represents the Pattern Match module in config and status API
     */
    kComponentPatternMatch,

    /**
    Represent the IRIG-B component for read IRIG-B time information from card
    */
    kComponentIRIGB, 
    /**
    Represent the Reset Strategy component for Altera FPGA.
    */ 	
    kComponentResetControl,
    /**
    Minimac component as a non-port component.for dual line rate cards.
    */ 	
    kComponentMiniMac,
    /**
    Module for the low level control of the tile like resets.
    */ 
    kComponentTileControl,
    /**
    Module for reporting and altering the operation mode of each interface of the card.
    */
    kComponentPhyMode,
    /**
    Module for controlling the Voltage Controlled Crystal Ocsillator.
    */
    kComponentVcxo,
    /**
     Module for storing the default value of the vcxo.may be used for other purposes as well.
     */
    kComponentScratchPad,
    /** Module for SDH dechannelizer MK2 ( pattern matcher and channel processor 
     */
    kComponentSdhDechan,
    /**Module for switching the clocks for split tx rx.
    */ 	
    kComponentClockSwitch,
    /**Module for reporting and altering the operating mode of rx side of each interface.	
    */
    kComponentPhyModeRx,
    /**Module for reporting and altering the operation mode of tx side of each interface.
    */ 
    kComponentPhyModeTx,
    /**Module for aligning and optionally descrambling various raw data streams
     */
    kComponentRawCapture,
    /**
     Module for unpackaging any ERF encapsulations 
    */
    kComponentFormatSelector,
    /**
     Module for converting extracted NPB timestamp to ERF timestamp.
    */
    kComponentTimestampConverter,
    /*
     * Port identifier component. 
     */   
    kComponentPortIdentifier,
    /**
     * High precision sensors
     * */
    kComponentSensor,
    /**
     *  Internal status register
     * */
    kComponentInternalStatus,
    /**
     *TX behavior component
     */
    kComponentTxBehaviour,
    /**
     * Per TX stream features
     */
    kComponentTxStreamFeatures,
    /**
     * SourceID module
     */
     kComponentSourceId,
    /**
     *  Provenance Module register
     * */
    kComponentProvenanceModule,
    /** 
     * Start of Range of valid component codes. 
     */
    kFirstComponentCode = kComponentRoot,
    /** 
     * End of Range of valid component codes. 
     */    
    kLastComponentCode = kComponentProvenanceModule

} dag_component_code_t;

/*@}*/
/*@}*/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* DAG_COMPONENT_CODES_H */

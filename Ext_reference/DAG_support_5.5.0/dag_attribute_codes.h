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


#ifndef DAG_ATTRIBUTE_CODES_H
#define DAG_ATTRIBUTE_CODES_H

#if defined(_WIN32)
#include "wintypedefs.h"
#else 
#include <inttypes.h>
#endif

#include <time.h>

/* maximum value of hash_width */
#define DAG_HASH_WIDTH_MAX (5)
/* maximum number of bins allowed. (=(1<<DAG_HASH_WIDTH_MAX) */
#define DAG_HASH_BIN_MAX (32)

/*enum
{
	kNullAttributeUuid = 0
};*/

#define kNullAttributeUuid 0 

typedef enum
{
    kDagAttrErr,
    kDagAttrStatus,
    kDagAttrConfig,
    kDagAttrAll
    
} dag_attr_config_status_t;

/**
 * \ingroup  AttributeValueAccessors
 * Attributes can store different values depending on their type 
 */
typedef enum
{
    /**
     * Invalid Type Attribute
     */ 

    kAttributeTypeInvalid,
    /**
     * Boolean Attribute Type
     */ 
    kAttributeBoolean,
    /**
     * Char Attribute Type
     */ 
    kAttributeChar,
    /**
     * String Attribute Type
     */ 
    kAttributeString,
    /**
     * Int32 Attribute Type
     */ 
    kAttributeInt32,
    /**
     * Uint32AttributeType
     */ 
    kAttributeUint32,
    /**
     * Int64 Attribute Type
     */ 
    kAttributeInt64,
    /**
     * Uint64 Attribute Type
     */ 
    kAttributeUint64,
    /**
     * Struct Attribute Type
     */ 
    kAttributeStruct,
    /**
     * Null Attribute Type
     */ 
    kAttributeNull,
    /**
     * Float Attribute Type
     */ 
    kAttributeFloat
} dag_attr_t;
/**
\defgroup ConfigAndStatusAPI Configuration and Status API
The interface that exposes the Components and Attributes that configures the card..
*/
/*@{*/

/**
\defgroup AttrCodes Attribute Codes
Codes for identifying attributes.
*/
/*@{*/
typedef enum
{
    /**
    Generic code for an invalid attribute.
    */
    kAttributeInvalid,

    /**
    Indicates whether a physical interface is active or inactive.
    */
    kBooleanAttributeActive, 
    
    /**
    Indicates whether the All Ones Signal is being detected by the receiver.
    */
    kBooleanAttributeAlarmSignal,
    
    /**
    32-bit or 64-bit alignment of received ERF records.  When this is set, received ERFs will be padded to the next 64-bit boundary.
    */
    kBooleanAttributeAlign64,
    
    /**
    When nic is enabled this indicates if Ethernet auto-negotiation has completed.
    */
    kBooleanAttributeAutoNegotiationComplete,
    
    /**
    Enable or disable ATM scrambling. NOTE: This attribute is a duplicate of kBooleanAttributePayloadScramble. Do not use!
    */
    kBooleanAttributeATMScrambleDUMMY,
    
    /**
    SONET B1 Error indication.
    */
    kBooleanAttributeB1Error,
    
    /**
    SONET B2 Error indication.
    */
    kBooleanAttributeB2Error,
    
    /**
    SONET B3 Error indication.
    */
    kBooleanAttributeB3Error,
    
    /**
    Indicates whether the SONIC core is active.
    */
    kBooleanAttributeCore,
    
    /**
    Clears the statistics counters in the framer when set.
    */
    kBooleanAttributeClear,
   
    /**
    Indicates whether the CRC error count was ever cleared since this attribute was last read.
    */
    kBooleanAttributeCRCErrorEverLo,
    
    /**
    Indicates whether a CRC error was ever seen since this attribute was last read.
    */
    kBooleanAttributeCRCErrorEverHi,
    
    //removed due dupicated with kBooleanAttributeTxCrc
    //do not use 
    kBooleanAttributeCrcInsert_dummy,
    
    /**
    Receive or transmit direction of connection
    */
    kUint32AttributeDirection,
    /**
    Whether or not to discard packets that have an invalid Frame Check Sum.
    */
    kBooleanAttributeDiscardInvalidFcs,
    
    /**
    Indicates whether the optics module is present.
    */
    kBooleanAttributeDetect,

    /**
    Is the DUCK syncrhonized or not
    */
    kBooleanAttributeDUCKSynchronized,
    
    /**
    For a DUCK to be healthy the phase error should be less than the threshold.The default value of threshold for a GPS reciever is 596ns
    */
    kUint32AttributeDUCKThreshold,

    /**
    FIXME
    */
    kUint32AttributeDUCKFailures,

    /**
    FIXME
    */
    kUint32AttributeDUCKResyncs,

    /**
    The difference in frequency (Hz) between the dag clock and the synchronization source clock is called the frequency error.
    */
    kInt32AttributeDUCKFrequencyError,

    /**
    The Phase error denotes the time difference between the DUCK and the source clock.
    It will be measured in nano seconds
    */
    kInt32AttributeDUCKPhaseError,

    /**
    The maximum time difference between the DUCK and the source clock.
    */
    kUint32AttributeDUCKWorstFrequencyError,

    /**
    The maximum difference in frequency between the dag clock and sync source clock.
    */
    kUint32AttributeDUCKWorstPhaseError,

    /**
    The 32 bit binary fraction allows effective time stamping clocks as high as 2^32 Hz(ie 4 GHz.) But generally the current generation DAG cards don't have a 4GHz clock oscillator.they typically have oscillators of the range of 32MHz.This frequency is reffered to as crystal frequency.
    */
    kUint32AttributeDUCKCrystalFrequency,

    /**
    The 32MHz oscillator in the DAGCard is fed into a programmable divider that can produce a constant frequency anywhere from 0-32MHz.for eg:The DAG 3.2 by default produces a timestamp clock that runs at 2^24Hz or 16777216 Hz.This clock is reffered to as Synthetic frequency.
    */
    kUint32AttributeDUCKSynthFrequency,

    /**
    FIXME
    */
    kUint32AttributeDUCKPulses,

    /**
    FIXME
    */
    kUint32AttributeDUCKRejectedPulses,

    /**
    FIXME
    */
    kUint32AttributeDUCKSinglePulsesMissing,

    /**
    FIXME
    */
    kUint32AttributeDUCKLongestPulseMissing,

    /**
    FIXME
    */
    kBooleanAttributeDUCKRS422Input,

    /**
    FIXME
    */
    kBooleanAttributeDUCKHostInput,

    /**
    FIXME
    */
    kBooleanAttributeDUCKOverInput,

    /**
    FIXME
    */
    kBooleanAttributeDUCKAuxInput,

    /**
    FIXME
    */
    kBooleanAttributeDUCKRS422Output,

    /**
    FIXME
    */
    kBooleanAttributeDUCKLoop,

    /**
    FIXME
    */
    kBooleanAttributeDUCKHostOutput,

    /**
    FIXME
    */
    kBooleanAttributeDUCKOverOutput,

    /**
    FIXME
    */
    kBooleanAttributeDUCKSetToHost,
    
    /**
    FIXME
    */
    kNullAttributeDUCKSync,

    /**
    FIXME
    */
    kUint32AttributeDUCKSyncTimeout,

 
    /**
    Use this to get time information from the DAG clock.
    /sa DUCKTimeInfo
    */
    kStructAttributeDUCKTimeInfo,

    /**
    Set the DAG Clock to the PC clock
    */
    kNullAttributeDUCKSetToHost,

    /**
    Clear the DUCK stats.
    */
    kNullAttributeDUCKClearStats,

    /**
    Tells the card where to drop packets and is generally used with hlb
    images. If on, dropping of packets occurs at the individual stream that has
    filled up. If off, dropping occurs at the gpp.
    */
    kBooleanAttributeDrop,
    
    /**
    Indicates that the transmit driver has failed.
    */
    kBooleanAttributeDriverMonitorOutput,
    
    /**
    Enable or disable the equipment loopback (EQL) capability.  Used for testing, should normally be disabled.
    */
    kBooleanAttributeEquipmentLoopback,
    
    /**
    Indicates an Alarm Indication Signal error.
    */
    kBooleanAttributeE1T1AISError,
    
    /**
    Indicates a CRC error.
    */
    kBooleanAttributeE1T1CRCError,
    
    /**
    Indicates if there has been a framer error.  FIXME: is this since the last read?
    */
    kBooleanAttributeE1T1FramerError,
   
    /**
    Indicates whether the framer should generate Alarm Indication Signal errors.
    */
    kBooleanAttributeE1T1GenerateAlarmIndication,
    
    /**
    Apply subsequent configurations to all E1/T1 lines.  Otherwise each E1/T1 line is configured individually.
    */
    kBooleanAttributeE1T1GlobalConfiguration,
    
    /**
    Indicates whether the link is up.
    */
    kBooleanAttributeE1T1Link,
    
    /**
    Indicates whether the current stream (as set by kUint32AttributeE1T1StreamNumber) was in AIS mode since it was last selected.
    */
    kBooleanAttributeE1T1LinkAIS,
    
    /**
    Indicates whether the current stream (as set by kUint32AttributeE1T1StreamNumber) has seen CRC errors since it was last selected.
    This is only valid if E1 with CRC is selected as the framing type for the stream.
    */
    kBooleanAttributeE1T1LinkCRCError,
    
    /**
    Indicates whether the current stream (as set by kUint32AttributeE1T1StreamNumber) has seen framing errors since it was last selected.
    */
    kBooleanAttributeE1T1LinkFramingError,
    
    /**
    Indicates whether the current stream (as set by kUint32AttributeE1T1StreamNumber) is synchronized to the framing information.
    */
    kBooleanAttributeE1T1LinkSynchronized,
    
    /**
    Indicates whether the current stream (as set by kUint32AttributeE1T1StreamNumber) has synchronized to the framing information since it was last selected.
    */
    kBooleanAttributeE1T1LinkSynchronizedUp,
    
    /**
    Indicates whether the current stream (as set by kUint32AttributeE1T1StreamNumber) has lost framing lock since it was last selected.
    */
    kBooleanAttributeE1T1LinkSynchronizedDown,
    
    /**
    FIXME
    */
    kBooleanAttributeE1T1Resynchronize,
    
    /**
    Indicates that nothing is being processed by the SONIC E1/T1 framer.  Usually caused by faulty hardware.
    */
    kBooleanAttributeE1T1Rx0,
    
    /**
    Indicates that nothing is being processed by the SONIC E1/T1 framer.  Usually caused by faulty hardware.
    */
    kBooleanAttributeE1T1Rx1,
    
    /**
    Indicates that nothing is being processed by the SONIC E1/T1 framer.  Usually caused by faulty hardware.
    */
    kBooleanAttributeE1T1Tx0,
    
    /**
    Indicates that nothing is being processed by the SONIC E1/T1 framer.  Usually caused by faulty hardware.
    */
    kBooleanAttributeE1T1Tx1,
    
    /**
    FIXME
    */
    kBooleanAttributeEnableCounterModules,
    
    /**
    Enables the TCP Flow Counter.
    */
    kBooleanAttributeEnableTCPFlowCounter,
    
    /**
    Enables / Disables the IP Address Counter.
    */
    kBooleanAttributeEnableIPAddressCounter,
    
    /**
    Enables the packet capture module on the copro.
    */
    kBooleanAttributeEnablePacketCaptureModules,
    
    /**
    Enables/Disables probability hash table module.
    */
    kBooleanAttributeEnableProbabilityHashTable,
    
    /**
    Enables / Disables the probability sampler module.
    */
    kBooleanAttributeEnableProbabilitySampler,
    
    /**
    Enables /Disables the Hash Flow Table module.
    */
    kBooleanAttributeEnableHostFlowTable,
    
    /**
    Enables/Disables the Host Hash Table Module.
    */
    kBooleanAttributeEnableHostHashTable,

    /**
    Enable or disable the facility loopback (FCL) capability.  Used for testing, should normally be disabled.
    */
    kBooleanAttributeFacilityLoopback,
    
    /**
    Indicates whether the jitter attenuator read/write FIFO pointers are within +/- 3 bits.
    */
    kBooleanAttributeFIFOLimitStatus,
    
    /**
    Indicates whether a FIFO error has occurred.
    */
    kBooleanAttributeFIFOError,
    
    /**
    Indicates whether the link is in full duplex mode or half duplex mode.
    */
    kBooleanAttributeFullDuplex,

    /**
    Used to write IP Counter ERF to the memory hole.(ERF type 13)
    */
    kBooleanAttributeGenerateIPCounterErf,
    
    /**
    Used to write TCP flow erf to the memory hole (ERF type 14)
    */
    kBooleanAttributeGenerateTCPFlowCounterErf,
    
    /**
    Asserted when Hash Table is ready to be loaded with another hash.
    */
    kBooleanAttributeHashTableReady,
    
    /**
    Enable/disable HEC correction.
    */
    kBooleanAttributeHECCorrection,
    
    /**
    Enable/disable idle cell dropping.
    */
    kBooleanAttributeIdleCellMode,

    /**
    FIXME
    */
    kBooleanAttributeInterfaceRewrite,
    
    /**
    In an Ethernet network, jabber is traffic coming from a device that is always
    sending -- effectively bringing the network to a halt.
    
    This attribute indicates whether jabber is being detected.
    */
    kBooleanAttributeJabber,
    
    /**
    Configures the transmit laser to be on or off.
    */
    kBooleanAttributeLaser,
    
    /**
    Indicates whether the core has a lock on the datastream.
    */
    kBooleanAttributeLock,
    
    /**
    The channel receiver is detecting a Line Code Violation or an excessive number of zeros in the B8ZS or HDB3 modes.
    */
    kBooleanAttributeLineCodeViolation,
    
    /**
    This indicates if the demapper is in LCD (loss of cell delineation) state.
    */
    kBooleanAttributeLossOfCellDelineation,

    /**
    Indicates that the card is experiencing Loss Of Pointer (cannot lock to the SONET/SDH framer pointers).
    */
    kBooleanAttributeLossOfPointer,
    
    /**
    Indicates that the card is experiencing Loss Of Signal.
    */
    kBooleanAttributeLossOfSignal,
    
    /**
    Indicates whether there is an SFP Loss Of Signal error.
    */
    kBooleanAttributeSFPLossOfSignalCurrent,
    
    /**
    Indicates whether the SFP Loss Of Signal error was ever set to 0 since this attribute was last read.
    */
    kBooleanAttributeSFPLossOfSignalEverLo,
    
    /**
    Indicates whether an SFP Loss Of Signal error was ever seen since this attribute was last read.
    */
    kBooleanAttributeSFPLossOfSignalEverHi,
    
    /**
    Indicates that the card is experiencing Loss Of Frame.
    */
    kBooleanAttributeLossOfFrame,
    
    /**
    Indicates whether there is a current Loss Of Frame error.
    */
    kBooleanAttributeLossOfFramingCurrent,
    
    /**
    Indicates whether a Loss Of Frame error was ever seen since this attribute was last read.
    */
    kBooleanAttributeLossOfFramingEverHi,
    
    /**
    Indicates whether the Loss Of Frame error was ever set to 0 since this attribute was last read.
    */
    kBooleanAttributeLossOfFramingEverLo,

    /**
    FIXME
    */
    kBooleanAttributeMiniMacLostSync,

    /**
    Merge packets from multiple physical interfaces into a single receive stream.
    */
    kBooleanAttributeMerge,

    /**
    Enable or disable Ethernet auto-negotiation mode.  By default this is disabled;
    the disabled mode is intended for use with optical fibre splitters, and in the disabled mode Ethernet
    auto-negotiation is not performed.
    This attribute is deprecated. Use kBooleanAttributeAutoNegotiation instead.
    */
    kBooleanAttributeNic,
    /**
    Enable or disable Ethernet auto-negotiation mode.  By default this is disabled;
    the disabled mode is intended for use with optical fibre splitters, and in the disabled mode Ethernet
    auto-negotiation is not performed.
	This attribute is the same as kBooleanAttributeNic and has the same enum number it 
    is intended to replace it.  
    */
    kBooleanAttributeAutoNegotiation = kBooleanAttributeNic,
    /**
    Indicates that the card is in an Out Of Frame condition.
    */
    kBooleanAttributeOutOfFrame,

    /**
    Enable or disable payload scrambling for a concatenated POS demapper.
    */
    kBooleanAttributePayloadScramble,

    kBooleanAttributeATMScramble = kBooleanAttributePayloadScramble,
    /**
    Indicates that the receive input signal has been lost.
    */
    kBooleanAttributeReceiveLossOfSignal,
    
    /**
    Indicates that the SONET Remote Data Indication is set.
    */
    kBooleanAttributeRDIError,

    /**
    Indicates that the SONET Enhanced Remote Data Indication is set.
    */
    kUint32AttributeERDIError,

    /**
    Indicates that the SONET Remote Error Indication is set.
    */
    kBooleanAttributeREIError,
    
    /**
    Indicates whether there is a Remote error.
    */
    kBooleanAttributeRemoteErrorCurrent,

    /**
    Indicates whether the Remote error was ever set to 0 since this attribute was last read.
    */
    kBooleanAttributeRemoteErrorEverLo,
    
    /**
    Indicates whether a Remote error was ever seen since this attribute was last read.
    */
    kBooleanAttributeRemoteErrorEverHi,

    /**
    Holds the framer in reset when set; release the framer from reset when cleared.
    */
    kBooleanAttributeReset,
    
    /**
    Resets the Hash Table.
    */
    kBooleanAttributeResetHashTable,
    
    /**
    Resets the Probability Hash Table.
    */
    kBooleanAttributeResetProbabilityHashTable,
    
    /**
    Enable or disable power to a Rocket I/O.
    */
    kBooleanAttributeRocketIOPower,

    /**
    Enable or disable CRC appending onto transmitted frames.
    */
    kBooleanAttributeTxCrc,

    /**
    Enable or disable SONET frame scrambling.
    */
    kBooleanAttributeScramble,
    
    /**
    Enable or disable optics transmit power.
    */
    kBooleanAttributeSfpPwr,
        
    /**
    Indicates whether an SFP module is present.
    */
    kBooleanAttributeSFPDetect,

    /**
    Indicates whether there is an SFP transmit fault error.
    */
    kBooleanAttributeSFPTxFaultCurrent,
    
    /**
    Indicates whether an SFP transmit fault error was ever set to 0 since this attribute was last read.
    */
    kBooleanAttributeSFPTxFaultEverLo,
    
    /**
    Indicates whether an SFP transmit fault error was ever seen since this attribute was last read.
    */
    kBooleanAttributeSFPTxFaultEverHi,

    /**
    Set variable length or fixed length capture.  If disabled (i.e. fixed length capture is set) ERF records produced are padded up to the number of bytes specified by the snaplen attribute.
    */
    kBooleanAttributeVarlen,
    
    /**
    Enable or disable packet transmission.
    */
    kBooleanAttributeTxPkts,
    
    /**
    Enable or disable packet reception.
    */
    kBooleanAttributeRxPkts,

    /**
    Indicates whether the optics module detects an input signal.
    */
    kBooleanAttributeSignal,

    /**
    Indicates whether the link is synchronized.
    */
    kBooleanAttributeSync,

    /**
    Distributes received ERF records across one or more receive streams.
    */
    kBooleanAttributeSplit,

    /**
    When transmitting an ERF record, automatically rewrite the destination interface bit from 0 to 1 (and vice versa).
    This means that ERF records received on port 0 can be sent out port 1 without the need to rewrite the interface bits in software, which can improve the performance of inline forwarding.
    */
    kBooleanAttributeSwap,

    /**
    Indicates whether the link is OK.  In general if there is synchronization then the link will be OK.
    */
    kBooleanAttributeLink,
    //TODO: REMOVE ME  
    kBooleanAttributeRxLinkUp = kBooleanAttributeLink,
    
    /**
    Indicates whether there is a Link error.
    */
    kBooleanAttributeLinkCurrent,
    
    /**
    Indicates whether a Link error was ever seen since this attribute was last read.
    */
    kBooleanAttributeLinkEverHi,
    
    /**
    Indicates whether the Link error was ever set to 0 since this attribute was last read.
    */
    kBooleanAttributeLinkEverLo,

    /**
    Indicates a fault at the remote end of the link.
    */
    kBooleanAttributeRemoteFault,
    
    /**
    Indicates a fault at the local end of the link (the peer signal is not being received correctly).
    */
    kBooleanAttributeLocalFault,

    /**
    Share the memory hole between each the receive and transmit streams.  Used for inline forwarding of packets.
    */
    kBooleanAttributeOverlap,

    /**
    Indicates whether the peer link is up.
    */
    kBooleanAttributePeerLink,
    
    /**
    Indicates whether there is a peer Link error.
    */
    kBooleanAttributePeerLinkCurrent,
    
    /**
    Indicates whether the peer Link error was ever set to 0 since this attribute was last read.
    */
    kBooleanAttributePeerLinkEverLo,
    
    /**
    Indicates whether a peer Link error was ever seen since this attribute was last read.
    */
    kBooleanAttributePeerLinkEverHi,

    /**
    Enable or disable CRC stripping from received packets.
    */
    kBooleanAttributeCrcStrip,

    /**
    Enable or disable dropping errored packets/cells.  When unset, errored packets/cells are passed through to the host.
    */
    kBooleanAttributeLinkDiscard,
    
    /**
    Indicates if the Line Alarm Indication Signal is set.
    */
    kBooleanAttributeLineAlarmIndicationSignal,
    
    /**
    Indicates if the Line Remote Defect Indication Signal is set.
    */
    kBooleanAttributeLineRemoteDefectIndicationSignal,
    
    /**
    Indicates a Data Out Of Lock error condition.
    */
    kBooleanAttributeDataOutOfLock,
    
    /**
    Indicates a Reference Out Of Lock error condition.
    */
    kBooleanAttributeReferenceOutOfLock,
    
    /**
    Enable or disable the Line Side facility loopback (FCL) capability.  Used for testing, should normally be disabled.
    */
    kBooleanAttributeLineSideFacilityLoopback,
    
    /**
    Enable or disable the Line Side equipment loopback (EQL) capability.  Used for testing, should normally be disabled.
    */
    kBooleanAttributeLineSideEquipmentLoopback,
    
    /**
    Enable or disable discarding of packets shorter than kUint32AttributeMinPktLen.
    */
    kBooleanAttributePMinCheck,
    
    /**
    Enable or disable discarding of packets larger than kUint32AttributeMaxPktLen.
    */
    kBooleanAttributePMaxCheck,
    
    /**
    Indicates that the framer is not receiving a valid clock from the optics.
    */
    kBooleanAttributeLossOfClock,
   
    /**
    Transmit Parity Error.
    */
    kBooleanAttributeTxParityError,
    
    /**
    Indicates whether a high bit error rate has been detected.  If this occurs the optical power level should be checked.
    */
    kBooleanAttributeHighBitErrorRateDetected,
    
    /**
    Indicates that the ERF record timestamp is created when the last byte of the packet is seen, rather than when the first byte of the packet is seen.
    */
    kBooleanAttributeTimeStampEnd,
    
    /* Added them for completness. */
    kBooleanAttributeTransmitFIFOError,
    
    /**
    FIXME
    */
    kBooleanAttributeTransmitAlarmIndication,
    
    /**
    FIXME
    */
    kBooleanAttributeLaserBiasAlarm,
    
    /**
    FIXME
    */
    kBooleanAttributeLaserTemperatureAlarm,
    
    /**
    FIXME
    */
    kBooleanAttributeTransmitLockError,
    
    /**
    Caches statistics values before reading them (necessary because they are all cleared once any value is read from the device).
    */
    kBooleanAttributeRefreshCache,
    
    /**
    FIXME
    */
    kBooleanAttributeTCAMInit,
    
    /**
    FIXME
    */
    kBooleanAttributeResetTCPFlowCounter,
    
    /**
    FIXME
    */
    kBooleanAttributeResetIPCounter,
    /**
     * Determines if the ethernet port is set to promiscuous mode or not
     */
    kBooleanAttributePromiscuousMode,
    /**
     * Indicates the current state of the Remote Defect Indication
     */
    kBooleanAttributeRemoteDefectIndication,

    /** 
     * Indicates the current state of the Application Identification Channel (AIC)
     * AIC = 1 is C-bit mode, AIC = 0 is M23 m
     */
    kBooleanAttributeAICM23Cbit,
    
    /**
     * Indicates when the receive frame processor is in Alarm Indication Signal
     */
    kBooleanAttributeAlarmIndicationSignal, 
    
    /**
     * Indicates a fault condition on the Transmit Path
    **/
    kBooleanAttributeTxFault,
    /**
     * Indicates a fault condition on the Receive Path
     */
    kBooleanAttributeRxFault,

    /*
     * Indicates a fault on the receive or transmit paths
     */
    kBooleanAttributeFault,
     
    /*
     * FIXME
    */
    kUint32AttributeTcpFlowCounter,
    
    /**
    FIXME
    */
    kUint32AttributeIpAddressCounter,

    /**
     * Bit error rate counter.
     */
    kUint32AttributeBERCounter,
 
    /**
     * Hi Bit error rate reported.
     */
    kBooleanAttributeHiBER,
    
    /**
    Count of the number of error blocks that occured since the register was last accessed using MDIO.
    */
    kUint32AttributeErrorBlockCounter,
    
    /**
    Clear all connections on the DAG card.
    */
    kNullAttributeClearConnections,

    /**
    Initialize the SC256 Coprocessor.
    */
    kNullAttributeSC256Init,
   
    /**
    A number representing the PCI bus speed. The attribute value has the following meanings:
    */
    kUint32AttributePCIBusSpeed,
    
    /**
    The number of bad symbols received since this attribute was last read.
    */
    kUint32AttributeBadSymbols,
    
    /**
    The size of the buffer allocated to the DAG card.
    */
    kUint64AttributeBufferSize,
    
    /**
    The number of SONET/SDH Section Bit Interleaved Parity 1 errors seen.
    */
    kUint32AttributeB1ErrorCount,
    
    /**
    The number of SONET/SDH Section Bit Interleaved Parity 2 errors seen.
    */
    kUint32AttributeB2ErrorCount,
    
    /**
    The number of SONET/SDH Section Bit Interleaved Parity 3 errors seen.
    */
    kUint32AttributeB3ErrorCount,
    
    /**
    The number of SONET Remote Error Indications seen.
    */
    kUint32AttributeREIErrorCount,
    
    /**
    Indicates the maximum number of bytes allowed in each packet.
    */
    kUint32AttributeByteCount,
    
    //not used duplicated with kUint32AttributeCrcSelect,
    //do not use     
    kUint32AttributeCrc_dummy,
    
    /**
    Used to read the SONET/SDH C2 path label byte (Path Signal Label).  Typical settings are 0x16 for PoS and 0xCF for Cisco HDLC.
    On cards that support virtual containers the path label will be read from the virtual container specified by the kUint32AttributeVCIndex attribute.
    */
    kUint32AttributeC2PathLabel,
    
    /**
    The number of config sequences seen since this attribute was last read.
    */
    kUint32AttributeConfigSequences,
    
    /**
    The cable attenuation indication within +/- 1dB.
    */
    kUint32AttributeCableLoss,
    
    /**
    FIXME
    */
    kUint32AttributeClock,
    
    /**
    The number of CRC errors since this attribute was last read.
    */
    kUint32AttributeCRCErrors,

    /**
    General CRC error
    */
    kBooleanAttributeCRCError,

    /**
    Select the CRC to use.  \sa CRCCodes.
    */
    kUint32AttributeCrcSelect,

    /**
    The maximum value for the range on that stream
    */
    kUint32AttributeHLBRangeMax,

    /**
    The minimum value for the range on that stream
    */
    kUint32AttributeHLBRangeMin,
      
    /**
    The connection number for the current configuration.
    */
    kUint32AttributeConnectionNumber,

    /**
    The retrieves the VC Label for the connection specified by kUint32AttributeConnectionNumber.
    */
    kUint32AttributeConnectionVCLabel,
    
    /**
    The retrieves the VC pointer for the connection specified by kUint32AttributeConnectionNumber.
    */
    kUint32AttributeConnectionVCPointer,
    
    /**
    Retrieves the connection type (PCM-30, 31, 24, custom) for 7.1s
    */
    kUint32AttributeConnectionType,
    
    /**
    FIXME
    */
    kUint32AttributeDeframerRevisionID,
    
    /**
    Retrieves the revision ID of the channelized demapper or mapper.  This can be used to determine which features the demapper supports.
    */
    kUint32Attribute71sChannelizedRevisionID,
    
    /**
    Indicates which data byte to read for kUint32AttributeJ0PathLabel and kUint32AttributeJ1PathLabel.
    */
    kUint32AttributeDataPointer,
    
    /**
    Allows a connection to be removed from a card
    */
  	kUint32AttributeDeleteConnection,

    /**
    Configure the E1/T1 frame type.  \sa LineTypeCodes.
    */
    kUint32AttributeE1T1FrameType,
    
    /**
    Set the stream number for reading status data.
    */
    kUint32AttributeE1T1StreamNumber,
    
    /**
    The number of bad packets received.
    */
    kUint32AttributeErrorCounter,
    kUint32AttributeRxPktErrCount = kUint32AttributeErrorCounter,
    /**
    Retrieves the connection number for the most recently added connection.  This number
    is written to the multi-channel ERF header and can be used along with the physical interface number
    to uniquely identify a connection.
    */
    kUint32AttributeGetLastConnectionNumber,
    
    /**
    The number of cells with HEC errors since this attribute was last read.
    */
    kUint32AttributeHECCount,
    
    /**
    The number of idle cells received since this attribute was last read.
    */
    kUint32AttributeIdleCellCount,
    
    /**
    Contains the section trace byte specified by kUint32AttributeDataPointer from the virtual container specified by kUint32AttributeVCIndex.
    */
    kUint32AttributeJ0PathLabel,
    
    /**
    Contains the path trace byte specified by kUint32AttributeDataPointer from the virtual container specified by kUint32AttributeVCIndex.
    */
    kUint32AttributeJ1PathLabel,
    
    /**
    A 32-bit bitfield representing the on/off status of the LEDs on the 3.7T pod.  \sa LEDStatus.
    */
    kUint32AttributeLEDStatus,

    /**
    The rate at which the line is currently operating.  \sa LineRateCodes.
    */
    kUint32AttributeLineRate,
    /**
     * The mode in which the line is currently operating. \sa LineInterfaceCodes.
     */
    kUint32AttributeLineInterface,

    /**
    The number of times the demapper has experienced Loss of Cell Delineation.  FIXME: is this since the last read of the attribute?
    */
    kUint32AttributeLossOfCellDelineationCount,

    /**
    Force the DAG card to operate at the given line rate.  \sa LineRateCodes.
    */
    kUint32AttributeForceLineRate,
    
    /**
    The line type.  For valid line types \sa LineTypeCodes.
    */
    kUint32AttributeLineType,
    
    /**
    Set the period for blinking LEDs, in hundredths of a second.  This period applies globally to all LEDs.
    */
    kUint32AttributePeriod,
    
    /**
    Set the period of time when LED is OFF when blinking. This period applies globally to all LEDs.
    */
    kUint32AttributeDutyCycle,
    
    /**
    Indicates whether the card is in SONET clock master or slave mode.  \sa MasterSlaveCodes.
    */
    kUint32AttributeMasterSlave,
    
    /**
    Represents the memory (in mebibytes) allocated to a receive or transmit stream.
    Writing to this attribute will allocate the specified amount of memory from the buffer to
    an individual stream.  The size of the buffer can be read using the attribute
    kUint32AttributeBufferSize.
    */
    kUint32AttributeMem,
    
    /**
    Position of record pointer
    */
    kUint64AttributeRecordPointer,

    /**
    Position of limit pointer
    */
    kUint64AttributeLimitPointer,
    
    /**
    Same as kUint64AttributeMemBytes except the unit of measurement is bytes.
    */
    kUint64AttributeMemBytes,
    
    /**
    Indicates which E1/T1 line mode the DAG card is in.  For more information see the Mode Table.
    */
    kUint32AttributeMode,
    
    /**
    FIXME
    */
    kUint32AttributeMux,

    /**
    Retrieves the payload type (ATM, HDLC or RAW)
    */
    kUint32AttributePayloadType,

    /**
    The pointer state for virtual containers.
    */
    kUint32AttributePointerState,

    /**
    The received synchronization status message.
    */
    kUint32AttributeSSM,
    
    /**
    Captures flows that Hash to a given value.
    */
    kUint32AttributeSetCaptureHash,
    
    /**
    Configures the zero code suppression algorithm.  \sa ZeroCodeSuppressCodes.
    */
    kUint32AttributeZeroCodeSuppress,
    
    /**
    Configures PoS or ATM mode.  \sa NetworkModeCodes.
    */
    kUint32AttributeNetworkMode,

    /**
    Configures the payload mapping type.  \sa PayloadMappingCodes.
    */
    kUint32AttributePayloadMapping,
    
    /**
    The number of valid frames received since this attribute was last read.
    */
    kUint32AttributeRxFrames,
//TODO: Remove me 
    kUint32AttributeRxPacketCount = kUint32AttributeRxFrames,

	/**
	The number of short packet errors
	*/
	kUint32AttributeShortPacketErrors,

	/**
	The number of long packet errors
	*/
	kUint32AttributeLongPacketErrors,

    /**
    The number of bytes to capture from each packet.  (The snap length applies to the packet as seen on the wire, and thus excludes the ERF header.)
    */
    kUint32AttributeSnaplength,

    /**
    The number of bytes to capture from each HDLC packet.  (The snap length applies to the packet as seen on the wire, and thus excludes the ERF header.)
    */
    kUint32AttributeHDLCSnaplength,

    /**
    The number of bytes to capture from each RAW packet.  (The snap length applies to the packet as seen on the wire, and thus excludes the ERF header.)
    */
    kUint32AttributeRAWSnaplength,

    /**
    The current temperature.
    */
    kUint32AttributeTemperature,

    /**
    The number of bytes to strip from the end of the ERF record when transmitting.
    Used to prevent a trailing CRC (e.g. on an ERF that has been captured and is now being retransmitted) being sent as part of a packet.
    \sa TerfStripCrcCodes.
    */
    kUint32AttributeTerfStripCrc,
    
    /**
    Mask of Rx-Error in the ERF header. 
    */
    kBooleanAttributeRXErrorA,

    /**
    Mask of Rx-Error in the ERF header for the 2nd output port if available. 
    */
    kBooleanAttributeRXErrorB,
    
    /**
    Mask of Rx-Error in the ERF header for the 3rd output port if available. 
    */
    kBooleanAttributeRXErrorC,
    
    /**
    Mask of Rx-Error in the ERF header for the 4th output port if available. 
    */
    kBooleanAttributeRXErrorD,
    
    /**
    Timed release mode of operation.
    */
    kUint32AttributeTimeMode,

    /**
    Retrieves timeslot pattern in use in connection configuration.
    */
    kUint32AttributeTimeslotPattern,
    
    /**
    Logical shift performed between 2 packet timestamps.
    */
    kUint32AttributeScaleRange,
    
    /**
    Direction of logical shift, 
    left or right, multiplication or division.
    */
    kBooleanAttributeShiftDirection,
 
    /**
     Enable Built-In Self Test (BIST)
     */
    kBooleanAttributePhyBistEnable,
    
    /**
     Diagnostic Loopback Enable
     */
    kBooleanAttributePhyDLEB,
    
    /**
     Dual functionnality bit. During BIST, it clears error. 
     During normal operation, it puts the PHY in Line Loopback mode
     */
    kBooleanAttributePhyLLEB,
    
    /**
     Clock Off Tx. Must be deasserted for normal TX
     */
    kBooleanAttributePhyTxClockOff,
   
    /**
    Deassert to start receiving 
     */
    kBooleanAttributePhyKillRxClock,
    
    /**
    Rate selection 
     */
    kUint32AttributePhyRate,
    
    /**
    Reference selection
     */
    kUint32AttributePhyRefSelect,
    
    /**
    Main clock configuration
     */
    kUint32AttributeConfig,
    
    /**
    Discard data policy
     */
    kBooleanAttributeDiscardData,
    
    /**
    The termination strength.  \sa TerminationCodes.
    */
    kUint32AttributeTermination,
    
    /**
    Indicates whether the SONET type is channelized or concatenated.  \sa SonetTypeCodes.
    */
    kUint32AttributeSonetType,

    /**
    The number of remote errors since this attribute was last read.
    */
    kUint32AttributeRemoteErrors,

    /**
    The number of receive streams supported by the loaded firmware.
    */
    kUint32AttributeRxStreamCount,
    
    /**
    The number of transmit streams supported by the loaded firmware.
    */
    kUint32AttributeTxStreamCount,
    
    /**
    Defines the type of payload to extract.  \sa TributaryCodes.
    */
    kUint32AttributeTributaryUnit,
    
    /**
    The number of packets dropped on a physical interface.  FIXME: is this receive, transmit or both?
    */
    kUint32AttributeDropCount,

	/**
	It supresses most of errors
    */
    kBooleanAttributeSuppressError,

    /**
    FIXME
    */
    kUint32AttributeUnsetCaptureHash,

    /**
    Use this attribute to set the address of the data space on the CAM to read or write.
    */
    kUint32AttributeSC256DataAddress,

    /**
    Use this attribute to set the address of the mask space on the CAM to read or write.
    */
    kUint32AttributeSC256MaskAddress,

    /**
    Use this attribute to set the search length. /sa SC256SearchLength.
    */
    kUint32AttributeSC256SearchLength,

    /**
    The Virtual Container number of a connection.  \sa VirtualContainerNumbers.
    */
    kUint32AttributeVCNumber,

    /**
    The Virtual Container size.  \sa VirtualContainerCodes.
    */
    kUint32AttributeVCSize,

    /**
    Retrieve or specify the index of the virtual container to use.  Any index written should be less than the result of reading kUint32AttributeVCMaxIndex.
    */
    kUint32AttributeVCIndex,

    /**
    The maximum number of active virtual containers in the SONET frame.  This number depends on the hardware, loaded firmware and virtual container size.
    */
    kUint32AttributeVCMaxIndex,


    /**
    The tributary number of a connection.  
    */
    kUint32AttributeTUNumber,

    /**
    The TUG2 number of a connection.  
    */
    kUint32AttributeTUG2Number,

    /**
    The TUG3 number of a connection.  
    */
    kUint32AttributeTUG3Number,

    /**
    The number of the port which a connection has been created.  
    */
    kUint32AttributePortNumber,


    /**
    The current voltage.
    */
    kUint32AttributeVoltage,

    /**
    The number of frames successfully transmitted since this attribute was last read.
    */
    kUint32AttributeTxFrames,

    /**
    The number of Path Bit Interleaved Parity errors seen.  FIXME: is this since last read?
    */
    kUint32AttributePathBIPError,

    /**
    The number of Path Remote Error Indication errors seen.  FIXME: is this since last read?
    */
    kUint32AttributePathREIError,

    /**
    Configures the minimum expected packet length.  Packets shorter than this size are discarded if kBooleanAttributePMinCheck is enabled.
    */
    kUint32AttributeMinPktLen,

    /**
    Configures the maximum expected packet length.  Packets longer than this size are discarded if kBooleanAttributePMaxCheck is enabled.
    */
    kUint32AttributeMaxPktLen,

    /**
    The number of transmit frames dropped since this attribute was last read.
    */
    kUint32AttributeTxFDrop,

    /**
    The number of PoS frames aborted since this attribute was last read.
    */
    kUint32AttributeAborts,
    
    /**
    The number of packets discarded because they were too small since this attribute was last read.
    */
    kUint32AttributeMinPktLenError,
    
    /**
    The number of packets discarded because they were too large since this attribute was last read.
    */
    kUint32AttributeMaxPktLenError,
    
    /**
    The number of receive frames dropped since this attribute was last read.
    */
    kUint32AttributeRxFDrop,

    /**
    FIXME
    */
    kUint32AttributePacketCaptureThreshold,
    
    /**
    FIXME
    */
    kUint32AttributeFlowCaptureThreshold,
    
    /**
    Set the ERF record steering mode.  \sa SteeringCodes.
    */
    kUint32AttributeSteer,

    /**
    Set the Ethernet (LAN / WAN) mode.  \sa EthernetCodes.
    */
    kUint32AttributeEthernetMode,

    /**
     *The type of framing to be used 
     */
    kUint32AttributeFramingMode,

    /**
    FIXME
    */
    kBooleanAttributeReadyEnableTCPFlowModule,
    
    /**
    FIXME
    */
    kBooleanAttributeReadyEnableIPCounterModule,
    
    /**
    The optics report a receive error.  kBooleanAttributeReceiveLockError and/or kBooleanAttributeReceivePowerAlarm will also be set if this occurs.
    */
    kBooleanAttributeReceiveAlarmIndication,
    
    /**
    The optics report a failure in clock recovery from the received signal.
    */
    kBooleanAttributeReceiveLockError,
    
    /**
    The optics report insufficient optical input power (less than -30 dBm).
    */
    kBooleanAttributeReceivePowerAlarm,

    /**
     * Enable or disable receive cell descrambling.
     */
    kBooleanAttributeDescramble,
    /**
     * Enable or disable RX Monitoring
     */
    kBooleanAttributeRXMonitorMode,
    
    /**
    The number of Receive Parity Errors counted between the framer and receive FPGA.
    */
    kUint32AttributeRxParityError,
    
    /**
    Transmit FIFO Errors.
    */
    kUint32AttributeTxFIFOError,

    /**
    The number of bytes successfully received since this attribute was last read.
    */
    kUint64AttributeRxBytes,

    /**
     * The number of bytes successfully received since this attribute was last read.
     * for some of the cards there is no clear of the counter 
     */
    kUint32AttributeRxBytes,
//TODO: Remove me
    kUint32AttributeRxByteCount = kUint32AttributeRxBytes,

    /** 
    The number of bytes received in errored frames since this attribute was last read.
    */
    kUint64AttributeRxBytesBad,
    
    /**
    The number of times a valid length frame was received at a physical interface
    while the PHY indicated a Data Reception Error or Invalid Data Symbol Error.
    */
    kUint64AttributeBadSymbol,
    
    /**
    The number of frames received that did not pass the Frame Check Sum (FCS) check.
    */
    kUint64AttributeCRCFail,
    
    /**
    The number of valid frames received since this attribute was last read.
    */
    kUint64AttributeRxFrames,
    
    /**
    The number of bytes successfully transmitted since this attribute was last read..
    */
    kUint64AttributeTxBytes,

    /**
    The number of bytes successfully transmitted since this attribute was last read.
    */
    kUint32AttributeTxBytes,
    
    /**
    The number of frames successfully transmitted since this attribute was last read.
    */
    kUint64AttributeTxFrames,
    
    /**
    The number of frames that could not be sent correctly because of MAC errors.
    */
    kUint64AttributeInternalMACError,
    
    /**
    The number of frames that could not be sent correctly because of errors other than MAC errors.
    (That is, frames that are already counted by kUint64AttributeInternalMACError are not included in this count.)
    */
    kUint64AttributeTransmitSystemError,

    /**
    The number of PoS/Ethernet FCS (CRC32) errors since this attribute was last read.
    */
    kUint64AttributeFCSErrors,
    
    /**
    The number of errored packets received since this attribute was last read.
    */
    kUint64AttributeBadPackets,
    
    /**
    The number of valid packets received since this attribute was last read.
    */
    kUint64AttributeGoodPackets,
    
    /**
    The number of framer receive FIFO errors since this attribute was last read.
    */
    kUint64AttributeFIFOOverrunCount,

    /**
    Pseudo-attributes provided for convenience.
    */
    kStringAttributeMem,

    /**
    Adds a connection.  \sa ConnectionDescriptionStruct.
    */
    kStructAttributeAddConnection,
    
    /**
    Set Erf mux on 3.7T.  
    */
	kStructAttributeErfMux,

    /**
    Use this attribute to read/write data to the TCAM.
    */
    kStructAttributeSC25672BitData,

    /**
    Use this attribute to read/write mask values to the TCAM.
    */
    kStructAttributeSC25672BitMask,

    /**
    Use this attribute to perform 72-bit searches.
    */
    kStructAttributeSC25672BitSearch,

    /**
    Use this attribute to perform 144-bit searches.
    */
    kStructAttributeSC256144BitSearch,

    /**
    Set this attribute before reading statistics.  It latches the statistics counters so they can be read in a consistent state.
    */
    kBooleanAttributeCounterLatch,
    
    /**
    Configures the line steering mode for the ERF MUX.
    */
    kUint32AttributeLineSteeringMode,
    
    /**
    Sets the host steering mode for the ERF MUX.
    */
    kUint32AttributeHostSteeringMode,
    
    /**
    Sets the IXP steering mode for the ERF MUX.
    */
    kUint32AttributeIXPSteeringMode,
    
    /**
    Indicates if there is a fault during the DPA (Dynamic Phase Alignment).
    */
    kBooleanAttributeDPAFault,
    
    /**
    Starts the DPA (Dynamic Phase Alignment).
    */
    kBooleanAttributeDPAStart,
    
    /**
    Indicates when DPA (Dynamic Phase Alignment) has finished.
    */
    kBooleanAttributeDPADone,
    
    /**
    Indicates if there is a error with the DCM during alignment.
    */
    kBooleanAttributeDPADcmError,
    
    /**
     Indicates if the DCM is lock.
     */
    kBooleanAttributeDPADcmLock,

    /**
     Stores the MAC address of an ethernet port
     */
    kStringAttributeEthernetMACAddress,

    /**
     Counter Statistics Interface type.
     */
    kUint32AttributeCSIType,

    /**
     Number of counters in CSI.
     */
    kUint32AttributeNbCounters,

    /**
     Latch & Clear set up.
     */
    kBooleanAttributeLatchClear,

    /**
     Counter description base address.
     */
    kUint32AttributeCounterDescBaseAdd,

    /**
     Counter value base address.
     */
    kUint32AttributeCounterValueBaseAdd,
    /**
     Counter type ID.
     */
    kUint32AttributeCounterID,

    /**
     Counter subfunction type.
     */
    kUint32AttributeSubFunction,

    /**
     Counter value type: address (1) or value (0).
     */
    kBooleanAttributeValueType,

    /**
     Counter valu size: 32 (0) or 64 bits (1).
     */
    kUint32AttributeCounterSize,

    /**
     Counter type access: direct (0) or indirect (1).
     */
    kBooleanAttributeAccess,

    /**
     Counter value.
     */
    kUint64AttributeValue,

    /**
     * Sets the hlb range for all streams
     */
    kStructAttributeHlbRange,

    /**
     * Transmit FIFO oveflow
     */
    kBooleanAttributeTxFIFOOverflow,

    /**
     * Receive FIFO oveflow
     */
    kBooleanAttributeRxFIFOOverflow,

    /**
     * Transmit FIFO full
     */
    kBooleanAttributeTxFIFOFull,

    /**
     * Receive FIFO empty
     */
    kBooleanAttributeRxFIFOEmpty,

    /**
     * Select between Sonet or SDH mode
     */
    kBooleanAttributeSonetMode,

    /** 
     * Factory Firmware 
     * */
    kStringAttributeFactoryFirmware,
    /**
     * User Firmware
     **/
    kStringAttributeUserFirmware,
    /**
     * Active Firmware
     **/
    kUint32AttributeActiveFirmware,
    /** Card Serial 
     */
    kUint32AttributeSerialID,
    /** PCI Info */
    kStringAttributePCIInfo,
    /** Co Pro info
     */
    kUint32AttributeCoPro,
    /**
    The number of packets dropped on a stream
    */
    kUint32AttributeStreamDropCount,
    /**
     * Read only attribute shows if IDELAY function is presented
     * At the moment supported on VSC8479 component on some cards
     */
    kBooleanAttributeIDELAY_Present,
    
    /**
     * Read only attribute shows the current delay counter value
     * VSC8479 component on some cards
     */
    kUint32AttributeIDELAY,

    /**
     * This value has been added to read the value of the time stamp counter
     */    
    kUint64AttributeDUCKTSC,

    /**
     * Attribute in GPP/SR-GPP which indicates the number of interfaces
     */
    kUint32AttributeInterfaceCount,

    /**
     * Used to set and read transmit C2 path label byte 0.
     */
    kUint32AttributeTXC2PathLabel0,

    /**
     * Used to set and read transmit C2 path label byte 1.
     */
    kUint32AttributeTXC2PathLabel1,

    /**
     * Used to set and read transmit C2 path label byte 2.
     */
    kUint32AttributeTXC2PathLabel2,

    /**
     * Used to set and read transmit C2 path label byte 3.
     */
    kUint32AttributeTXC2PathLabel3,

    /**
     * Used to set and read transmit C2 path label byte 4.
     */
    kUint32AttributeTXC2PathLabel4,

    /**
     * Used to set and read transmit C2 path label byte 5.
     */
    kUint32AttributeTXC2PathLabel5,

    /**
     * Used to set and read transmit C2 path label byte 6.
     */
    kUint32AttributeTXC2PathLabel6,

    /**
     * Used to set and read transmit C2 path label byte 7.
     */
    kUint32AttributeTXC2PathLabel7,

    /**
     * Used to set and read transmit C2 path label byte 8.
     */
    kUint32AttributeTXC2PathLabel8,

    /**
     * Used to set and read transmit C2 path label byte 9.
     */

    kUint32AttributeTXC2PathLabel9,
    /**
     * Used to set and read transmit C2 path label byte 10.
     */
    kUint32AttributeTXC2PathLabel10,

    /**
     * Used to set and read transmit C2 path label byte 11.
     */
    kUint32AttributeTXC2PathLabel11,

    /**
    Attribute to set the V5 signal label path overhead byte
    */

    kUint32AttributeTXV5SignalLabel,

    /**
    The voltage reported by the hardware montoring system on the card.
    */
    kFloatAttributeVoltage,

    /*Adding new attributes for the CAT component.*/
    /**
	Write enable Pin for the CAT Module 
    */
    kBooleanAttributeWriteEnable,
    /**
	Specifies whether the Interface bits should be overwriteen or NOT 
    */
    kBooleanAttributeEnableInterfaceOverwrite,
    
    kUint16AttributeForwardDestinations,
    /**
      Selects one of the two banks in the CAT module.	
     */
    kBooleanAttributeBankSelect,

    kBooleanAttributeClearCounter,

    /**
    Enables the bypass mode for CAT module.This is turned on by default.
    */
    kBooleanAttributeByPass,
    /**
    Value of 1 means output is used a one hot encoded.Value of 0 means output is binary encoded.
    */ 
    kBooleanAttributeOutputType,
    /**
    The number of output bits for CAT module.
     */ 
    kUint32AttributeNumberOfOutputBits,
    /**
    The number of input bits for CAT module.
     */
    kUint32AttributeNumberOfInputBits,	
    /**
    The number of packets deliberately dropped by CAT since the counter was last cleared.
    */
    kUint32AttributeDeliberateDropCount,
    /**
    Address register for addressing the CAT module.
    */
    kUint32AttributeAddressRegister,
    /**
    Data Register for addressing the CAT module.	
    */
    kInt32AttributeDataRegister,
    
    /*Attributes for CAT components -End*/

    /* Attributes for per stream feature component */
    /**
    The number of streams supported.
    */
    kUint32AttributeNumberOfStreams,
   /**
    The number of registers supported per stream.
    */
    kUint32AttributeNumberOfRegisters,
    /**
    Indicates the presence of SLEN attribute per stream. Redefining previous code, as it ws not convention complained
    */
	kBooleanAttributeSnaplengthPresent, 
    kBooleanAttributeSLEN_Present = kBooleanAttributeSnaplengthPresent,
    /**
    Indicates Snap length per stream. Redefining previous code, as it ws not meaningful 
    */
    kUint32AttributePerStreamSnaplength,
    kUint32AttributeSLen = kUint32AttributePerStreamSnaplength,
/**
    Indicates maximum snap lenght allowed per stream.
    */
    //TODO check if it is not better to be used the existing  kUint32AttributeSnapLen
    kUint32AttributeMaxSnapLen,

    /* Attributes for per stream feature component -End */

    /*Adding Attributes for HAT - the new HLB component*/
    /**
    The vlaue of '1' means the output is used as one-hot encoded.
    The value of '0' means the output is binary encoded.
    */
    kBooleanAttributeHatEncodingMode,

    /**
    The number of output bits used by the firmware.When in backwards compatability mode this value will be 9.
    */
    kUint32AttributeHatOutputBits,
    /**
    The number of input bits(hash width).In backwards compatibility mode this value will be 10.	
    */
    kUint32AttributeHatInputBits,
    /**
    Used to specify the range of Hash Load Balancing values.The range is from 0 - 1000.
    To be split across a max of16 outputs.
    */
    kStructAttributeHATRange,
    /**
    Hash Encoding field in the IPF module.Logically forms a part of HLB.
    0 - IPF is running in compatibility mode.
    1 - IPF will embed hash values in the lower four bits of color.
    */
    kBooleanAttributeHashEncoding,
    /**
    N-Tuple select used for hashing.(Part of IPF moudle in the firmware.Logically forms a part of HLB.)
    Valid values are the following.
    2 - 2-tuple(IP Addresses.)
    3 - 3-typle(2-tuple + IP Protocol.)
    4 - 4-tuple(3-tuple + ERF Interface.)
    5 - 5-tuple(3-tuple + TCP/UDP ports)
    6 - 6-tuple(5-tuple + Interface.) 
    */
    kUint32AttributeNtupleSelect,
    /*Internally uses the same structure as kStructAttributeHATRange*/

    /*Attributes for HAT component -End*/
/* Attributes IPF information component */
    /**
     * Indicates whether IPF is enabled or not
     */
    kBooleanAttributeIPFEnable,
    /**
     * Indicates whether the IPF will drop packets that suppose to go to none of the streams .
     */
    kBooleanAttributeIPFDropEnable,
    
    /**
     * shift colour option for hash mode
     */
    kBooleanAttributeIPFShiftColour,

    /**
     * Indicates whether the link type is ethernet or PoS
     */
    kBooleanAttributeIPFLinkType,

    /**
     * Indicates whether the colour is embedded with in the loss counter field 
     */
    kBooleanAttributeIPFSelLctr,

    /**
     * Indicates whether RX error is used to show the pass/drop status
     */
    kBooleanAttributeIPFUseRXError,

    /**
     * Set the rule to interface 0
     */
    kBooleanAttributeIPFRulesetInterface0,

    /**
     * Set the rule to interface 1
     */
    kBooleanAttributeIPFRulesetInterface1,

    /* Attributes for IPF information component -End */

    
    /* Start - attributes for TTR-TERF */
    /**
     Used to clear the trigger
    */ 
    kBooleanAttributeClearTrigger,
    /**
    Indicates a trigger is pending to happen
   */
    kBooleanAttributeTriggerPending,
    /**
    Indicates that a trigger has occured 
    */
    kBooleanAttributeTriggerOccured,
    /**
    Timestamp value at which the trigger has to occur
    */
    kUint64AttributeTriggerTimestamp, 
    /* End - attributes for TTR-TERF */

    /*Attributes for Infiniband Cross Point Switch VSC3312- Start*/
    /**
     * Select the Input or Output connection for following operations and setting 
     */
    kUint32AttributeCrossPointConnectionNumber,
    /**
     * Raw access to the regiter pointed via the connection number and page register
     * used for debuging
     */
    kUint32AttributeRawData,	    
    /**
     * Selects the page which is going to be used 
     */
    kUint32AttributePageRegister,
    /**
     * Enables the access to the cross point switch it is set by the default method 
     * 
     */
    kUint32AttributeSerialInterfaceEnable,
    /**
     * Enables the output specified via kUint32AttributeCrossPointConnectionNumber
     */
    kBooleanAttributeOutputEnable,
    /**
     * Specifies which input to be connected to the output specified via kUint32AttributeCrossPointConnectionNumber
     */
    kUint32AttributeSetConnection,
    
    /**
     *  Configures the input signal equalization for the selected input.
     *  ISE Short Time Constant,ISE Medium Time Constate,ISE Long Time Constant. 
     * */
    kUint32AttributeInputISEShortTimeConstant,
    /**
     See description for kUint32AttributeInputISEShortTimeConstant
     */
    kUint32AttributeInputISEMediumTimeConstant,
    /**
     See description for kUint32AttributeInputISEShortTimeConstant
    */ 
     kUint32AttributeInputISELongTimeConstant,
     /**
     *Defines the Input Enable,Polarity and Termination Settings for the selected input. can be configured
     */	
    kBooleanAttributeInputStateTerminate,
    /**
     *See the descripton for \ref kBooleanAttributeInputStateTerminate
    */
    kBooleanAttributeInputStateOff,
    /**
     *See the descripton for \ref kBooleanAttributeInputStateTerminate
    */
    kBooleanAttributeInputStateInvert,
    /** 
      this register configures the input LOS threshold value for the selected input.
    * LOS threshold settings (peak - to -peak diff voltage.)
    * 111 - unused
    * 110 - unused
    * 101 - 310 mv
    * 100 - 250 mv
    * 011 - 190 mv
    * 010 - 130 mv
    * 001 - unused
    * 000 - unused.
    * */  
 	
    kUint32AttributeInputLOSThreshold,
    
    /**
     *Configures the long time constant preemphasis for selected output.
     *pre-emphasis level 0000 = off  ..... 1111 = maximum
     *pre-emphasis decay. 000 = fastest ..... 111 = slowest.
    */
    kUint32AttributeOutputPreLongDecay,
    /**
     *see description \ref kUint32AttributeOutputPreLongDecay
    */
    kUint32AttributeOutputPreLongLevel,
    /** 
       Configures the short time constant pre-emphasis for selected output.
     *  pre-emphasis level values as above 
     *  pre-emphasis decay.
     *  */
    kUint32AttributeOutputPreShortDecay,
    /**
     *See description \ref kUint32AttributeOutputPreShortDecay
     */
    kUint32AttributeOutputPreShortLevel,
    //FIXME
    kBooleanOutputLevelTerminate,
    //FIXME
    kUint32OutputLevelPower,
    /**  
     * This register provides OOB signalling and output invert for selected output.
     *   Output Operation Mode. - 1010  = Inverted ; 0101 = Normal Operation;0000 = Supressed.;Any other combination result in undefined opreation.
     *   Enable LOS Forwarding - 0 = Ignore LOS;1 = Enable OOB forwarding.
     * */
    kBooleanOutputStateLOSForwarding,
     /**
     see description \ref kBooleanOutputStateLOSForwarding
     */
    kUint32OutputStateOperationMode,
    /**
     * Configures for the selected ?? input ?? channel to transfer LOS to the Status PIN 0 directly(raw) 
     * configuration 
     */
    kUint32AttributeStatusPin0LOS,
    /**
     * Configures for the selected ?? input ?? channel to transfer LOS to the Status PIN 0 latched  
     * configuration 
     */
    kUint32AttributeStatusPin0LOSLatched,
    
    /**
     *Configures for the selected input channel to transfer LOS to status pin 1 (latched/raw) 
     */
    kUint32AttributeStatusPin1LOS,
    kUint32AttributeStatusPin1LOSLatched,
    
    /**
     *The register provides the LOS status for the selected input 
    */
    
    kBooleanAttributeChannelStatusLOS,
    kBooleanAttributeChannelStatusLOSLatched,
    
    /**
     * Status pin 1 current state
     * If configured to the specific chanel LOS the status will show either latched or raw LOS
     */
    kBooleanAttributePin0Status,
    kBooleanAttributePin1Status,
   
    /** 
     *Energises the Right  Core of vsc3312 crosspoint switch
     */ 
    kBooleanAttributeEnergiseRtCore,
    /** 
     *Energises the  Left Core of vsc3312 crosspoint switch
     */
    kBooleanAttributeEnergiseLtCore,
    /**
     *Activates the low glitch programming mode.
     */	
    kBooleanAttributeLowGlitch,
    /**
     *Turns on all core buffers for faster switching speed.
     */
    kBooleanAttributeCoreBufferForceOn,
    /**
     *Inverts the sense of the config pin 0 - Active High 1 - Active Low
     */
    kBooleanAttributeCoreConfig,
    /*Attributes for Inifiband Cross Point Switch - End*/
    /*Attributes for Infiniband Front End Port Registers*/
#ifdef ENDACE_LABS  
    /* these are the fields in the Diagnostic Register.
     * this is used for the test setup.and will be used for internal
     * debugging purposes only
     * */
     	
    kUint32AttributeLoopBackZero,
    kUint32AttributeLoopBackOne,
    kUint32AttributeLoopBackTwo,
    kUint32AttributeLoopBackThree,
    
    kUint32AttributeLaneZeroRxPrbsChecker,
    kUint32AttributeLaneOneRxPrbsChecker,
    kUint32AttributeLaneTwoRxPrbsChecker,
    kUint32AttributeLaneThreeRxPrbsChecker,
    

    kUint32AttributeLaneZeroTxPrbsGenerationControl,
    kUint32AttributeLaneOneTxPrbsGenerationControl,
    kUint32AttributeLaneTwoTxPrbsGenerationControl,
    kUint32AttributeLaneThreeTxPrbsGenerationControl,
    
    kBooleanAttributeLaneZeroPrbsErrorCounterReset,
    kBooleanAttributeLaneOnePrbsErrorCounterReset,
    kBooleanAttributeLaneTwoPrbsErrorCounterReset,
    kBooleanAttributeLaneThreePrbsErrorCounterReset,
#endif
    /*Prbs Error Status for Lane (0-3) 0 -no errors 1 - errors.*/ 
    kBooleanAttributeLaneZeroPrbsError,
    kBooleanAttributeLaneOnePrbsError,
    kBooleanAttributeLaneTwoPrbsError,
    kBooleanAttributeLaneThreePrbsError,
    
   /*Phase locked Loop detect 0 and pll detect 1.
    *These attributes have been combined to a single attribute to show if 
    *a lock is detected in both Tile 0 and Tile 1 */
    
    kBooleanAttributePllDetectZero,
    kBooleanAttributePllDetectOne,
    
   /*GTP Reset for Lanes 0-3. 0 -Not Ready 1 - Ready.
    * These four attriubtes have been combined to form a single attribute
    * which shows if all four GTP resets is ready or not.*/	
    
    kBooleanAttributeGTPResetDoneZero,
    kBooleanAttributeGTPResetDoneOne,
    kBooleanAttributeGTPResetDoneTwo,
    kBooleanAttributeGTPResetDoneThree,
    
   /*Rx Buffer Status for Lanes 0 - 3.
    *000 - Nominal condition
    *001 - number of bytes in buffer are less than CLK_COR_MIN_LAT 
    *010 - number of bytes in buffer greaer than CLK_COR_MAX_LAT
    *101 - Rx Buffer Overflow.
    *110 - Rx Buffer Underflow */
    
    kUint32AttributeRxBufferStatZero,
    kUint32AttributeRxBufferStatOne,
    kUint32AttributeRxBufferStatTwo,
    kUint32AttributeRxBufferStatThree,
    
    /*Rx Date Byte aligned or Not ? Status 1 - aligned 0 - not aligned.*/ 
    kBooleanAttributeRxByteAlignZero,
    kBooleanAttributeRxByteAlignOne,
    kBooleanAttributeRxByteAlignTwo,
    kBooleanAttributeRxByteAlignThree,
    
    /*Rx Date channel aligned or Not ? 1 aligned 0 - not aligned.*/
    kBooleanAttributeRxChannelAlignZero,
    kBooleanAttributeRxChannelAlignOne,
    kBooleanAttributeRxChannelAlignTwo,
    kBooleanAttributeRxChannelAlignThree,
    
    /*All four lanes byte aligned*/
    kBooleanAttributeRxByteAlign,
    
    /*Channel Bonding all four lanes*/
    kBooleanAttributeRxChannelAlign,
    
    /*This is the combined GTP Reset Attribute for all the four lanes*/
    kUint32AttributeGtpReset,
    
    /*CDR Rest Lane 0 -3*/
    kBooleanAttributeCdrZeroReset,
    kBooleanAttributeCdrOneReset,
    kBooleanAttributeCdrTwoReset,
    kBooleanAttributeCdrThreeReset,
    
    /*Rx Buffer Reset - Lane 0 - 3*/
    kBooleanAttributeLaneZeroRxBufferReset,
    kBooleanAttributeLaneOneRxBufferReset,
    kBooleanAttributeLaneTwoRxBufferReset,
    kBooleanAttributeLaneThreeRxBufferReset,

#ifdef ENDACE_LABS
    /* Rx/Tx Polarity and PowerDown Register.
     * and Tx Driver Register.These will be used for internal
     * debugging purposes only
     * */
    /*Polarity and Power Down Register*/
    /*Rx Polarity Port Lane 0 -3*/
    kBooleanAttributeLaneZeroRxPolarityPort,
    kBooleanAttributeLaneOneRxPolarityPort,
    kBooleanAttributeLaneTwoRxPolarityPort,
    kBooleanAttributeLaneThreeRxPolarityPort,

    /*Tx Polarity Port Lane 0 - 3*/
    kBooleanAttributeLaneZeroTxPolarityPort,
    kBooleanAttributeLaneOneTxPolarityPort,
    kBooleanAttributeLaneTwoTxPolarityPort,
    kBooleanAttributeLaneThreeTxPolarityPort,

    /*Rx Power Down Lanes 0 -3*/
    kUint32AttributeLaneZeroRxPowerDown,
    kUint32AttributeLaneOneRxPowerDown,
    kUint32AttributeLaneTwoRxPowerDown,
    kUint32AttributeLaneThreeRxPowerDown,

    /*Tx Power Down Lanes 0 -3*/
    kUint32AttributeLaneZeroTxPowerDown,
    kUint32AttributeLaneOneTxPowerDown,
    kUint32AttributeLaneTwoTxPowerDown,
    kUint32AttributeLaneThreeTxPowerDown,

    /*Tx Driver Register*/	
    /*Tx differential output swing from Lane 0 - 3*/
    kUint32AttributeLaneZeroTxDiffOutputSwing,
    kUint32AttributeLaneOneTxDiffOutputSwing,
    kUint32AttributeLaneTwoTxDiffOutputSwing,
    kUint32AttributeLaneThreeTxDiffOutputSwing,
   
    /*Tx Inhibit Data Transmission Lane 0 - 3*/
    kBooleanAttributeLaneZeroInhibitDataTx,
    kBooleanAttributeLaneOneInhibitDataTx,
    kBooleanAttributeLaneTwoInhibitDataTx,
    kBooleanAttributeLaneThreeInhibitDataTx,

    /*Tx Drive Strength and Preemphasis.*/
    kUint32AttributeLaneZeroTxDriveStrength,
    kUint32AttributeLaneOneTxDriveStrength,
    kUint32AttributeLaneTwoTxDriveStrength,
    kUint32AttributeLaneThreeTxDriveStrength,
#endif
    /*Packet Diagnostic Control Register 1*/
    /*Defines the transmit packet type - 
     * 00 - Raw (No IBA Packet) 
     * 01 - IP  (No IBA Packet)
     * 10 - IBA local
     * 11 - IBA global*/
    kUint32AttributeTxPacketType,
    /*Asserted when Tx Non -Valid ICRC*/
    kBooleanAttributeTxICRCError,
    /*Asserted when Tx Non -Valid VCRC*/
    kBooleanAttributeTxVCRCError,
    /*Reciever Equilization Lane 0 - 3*/
    kBooleanAttributeRxEqLaneZero,
    kBooleanAttributeRxEqLaneOne,
    kBooleanAttributeRxEqLaneTwo,
    kBooleanAttributeRxEqLaneThree,
    /*Wide Band/High Pass Mix Ratio for Rx equilization ckt Lane 0 -3
     * 00 - 50 % wideband 50 % highpass
     * 01 - 62.5% wideband 37.5% highpass
     * 10 - 75% wideband 25% highpass
     * 37.5% wideband 62.5% highpass*/

    kUint32AttributeRxEqMixZero,
    kUint32AttributeRxEqMixOne,
    kUint32AttributeRxEqMixTwo,
    kUint32AttributeRxEqMixThree,
    /*Clears the Rx Packet Error Latch Bit.*/	
    kBooleanAttributePktErrClear,
    /*Packet Diagnostic Status Register.*/
    /* 00 : Raw (no IBA packet)
     * 01 : IP (no IBA packet)
     * 10 : IBA local
     * 11 : IBA global
     */ 
    kUint32AttributeRxPacketType,
    /*Rx Packet Length IBA test packets 
     * IBA LOCAL - 0x18
     * IBA GLOBAL - 0x0E*/
    kUint32AttributeRxPacketLength,
    /*Rx Packet ICRC Valid*/
    kBooleanAttributeRxICRCValid,
    /*Rx Packet VCRC Valid*/
    kBooleanAttributeRxVCRCValid,
#ifdef ENDACE_LABS
    /*8b10b Encoder Status Register*/
    /*Asserted when Rx data on Lane (0 - 3) is an 8B/10B comma.*/ 
    kBooleanAttributeRxCharCommaZero,
    kBooleanAttributeRxCharCommaOne,
    kBooleanAttributeRxCharCommaTwo,
    kBooleanAttributeRxCharCommaThree,

    /*Asserted when Rx data on Lane (0- 3) is and 8B/10B k character*/
    kBooleanAttributeRxCharKZero,
    kBooleanAttributeRxCharKOne,
    kBooleanAttributeRxCharKTwo,
    kBooleanAttributeRxCharKThree,
    /*Asseted when Rx data on Lane (0-3) is recieved with a disparity error*/
    kBooleanAttributeRxDisparityErrorZero,
    kBooleanAttributeRxDisparityErrorOne,
    kBooleanAttributeRxDisparityErrorTwo,
    kBooleanAttributeRxDisparityErrorThree,
    /*Asserted when Rx data on Lane (0-3) is the result of an illegal 8B/10B 
     *code and is in error */
    kBooleanAttributeRxNotInTableErrorZero,
    kBooleanAttributeRxNotInTableErrorOne,
    kBooleanAttributeRxNotInTableErrorTwo,
    kBooleanAttributeRxNotInTableErrorThree,
    /*Showns Lane (0- 3) Rx data running disparity.*/
    kBooleanAttributeRxRunningDisparityZero,
    kBooleanAttributeRxRunningDisparityOne,
    kBooleanAttributeRxRunningDisparityTwo,
    kBooleanAttributeRxRunningDisparityThree,
#endif   
    
    /*Rx Packet Error Latch. 0 - No Error 1 - Error.*/
    kBooleanAttributeRxFrameError,   

    /*Combined attributes added for Infiniband Front End Port Registers.*/

    /*when asserted indicates Phase Locked Loop Detected in Tile 0 and Tile 1*/
    kBooleanAttributePLLDetect,

    /*when asserted indicates GTP Reset Finished In all four lanes (0-3)*/	
    kBooleanAttributeGTPResetDone,

    /*Attributes for Infiniband Port Registers*/	
    /* New Attributes for TR-TERF */
    /**
     * abs_mode_offset value for time release TERF
     */
    kUint64AttributeAbsModeOffset,
	
    /** 
     * conf_limit value for time release TERF
     */
    kUint64AttributeConfLimit,

    /* Attributes for TR-TERF - End */

    /**
     * Restore connection attribute for 7.1S
     */
    kNullAttributeRestoreConnections,

    /*New Registers Added to InfiniBand Framer.*/	

    /*The below four attributes are asserted when the corrosponding Lane Rx data
     * contains the start of the channel bonding sequence.*/
#ifdef ENDACE_LABS

    kBooleanAttributeRxChannelBondingSeq0,
    kBooleanAttributeRxChannelBondingSeq1,
    kBooleanAttributeRxChannelBondingSeq2,
    kBooleanAttributeRxChannelBondingSeq3,

    /*Reports the clock Correction Status of the Elastic Buffer Lanes 0  - 4
     * 000 - No Clock Correction
     * 001 - 1 Sequence Skipped
     * 010 - 2 Sequences Skipped
     * 011 - 3 Sequences Skipped
     * 100 - 4 Sequences Skipped
     * 101 - Reserved
     * 110 - 2 Sequences added.
     * 111 - 1 Sequence added*/

    kUint32AttributeRxClockCorrectionCount0,
    kUint32AttributeRxClockCorrectionCount1,
    kUint32AttributeRxClockCorrectionCount2,
    kUint32AttributeRxClockCorrectionCount3,

    /*The Below four pins are asserted if the byte alignment within Lane 0 - 4
     * serial data stream has changed due to comma detection*/ 

    kBooleanAttributeRxByteRealign0,
    kBooleanAttributeRxByteRealign1,
    kBooleanAttributeRxByteRealign2,
    kBooleanAttributeRxByteRealign3,

    /* Asseted when Lane 0-4 Comma Alignment Block detects a block*/	

    kBooleanAttributeRxCommaDetect0,
    kBooleanAttributeRxCommaDetect1,
    kBooleanAttributeRxCommaDetect2,
    kBooleanAttributeRxCommaDetect3,

    /*Indicates a change in alignment between Transciver Lane 0-4 and Master*/	

    kBooleanAttributeRxRealign0,
    kBooleanAttributeRxRealign1,
    kBooleanAttributeRxRealign2,
    kBooleanAttributeRxRealign3,
#endif

    /** 
     * Co-pro Factory Firmware 
     */
    kStringAttributeFactoryCoproFirmware,
    /**
     * Co-pro User Firmware
     **/
    kStringAttributeUserCoproFirmware,
    /**
    * for SFP/XFP transceiver module attributes
    **/
    /**
    The Transceiver Module Identifier \sa TransceiverModuleCodes.
    */
    kUint32AttributeTransceiverIdentifier,

    kUint32AttributeTransceiverExtendedIdentifier,
    kStringAttributeTransceiverVendorName,
    kStringAttributeTransceiverVendorPN,
    kStringAttributeTransceiverCodes,
    kStringAttributeTransceiverLinkLength,
    kStringAttributeTransceiverMedia,
    kFloatAttributeTransceiverTemperature,
    kUint32AttributeTransceiverVoltage,
    kFloatAttributeTransceiverVoltage = kUint32AttributeTransceiverVoltage,
    kUint32AttributeTransceiverRxPower,
    kFloatAttributeTransceiverRxPower = kUint32AttributeTransceiverRxPower,
    kUint32AttributeTransceiverTxPower,
    kFloatAttributeTransceiverTxPower = kUint32AttributeTransceiverTxPower,
    kBooleanAttributeTransceiverMonitoring,
    /*Combined Input LOS attribute which combines Input LOS from all input lanes.*/
    kBooleanAttributeInputLOS,
    kUint32AttributeSubFunctionType,
    /** 
    * End of transceiver module attributes 
    **/
    /**
     * Feedback to indicate when the LA0 interface has completed initialization
     */ 
    kBooleanAttributeLA0DataPathReset,
    /**
     * Feedback to indicate when the LA1 interface has completed initialization
     */ 
    kBooleanAttributeLA1DataPathReset,
    
    /**
     * Enables the firmware access to LA0
     */ 
    kBooleanAttributeLA0AccessEnable,
    
    /**
     *Enables the firmware access to LA1
     */ 
    kBooleanAttributeLA1AccessEnable,

    /**
     * Database to use for searches.Can change this to affect hotswap.
     */ 
     kUint32AttributeDataBase,

    /**
     * The value that replaces the associated SRAM value if a search misses.
     */ 
    kUint32AttributeSRamMissValue,

    /**
     *Specifies the Global Mask Register to be used for searches.
     */ 
    kUint32Attributegmr,

    /**
     *If set to 1 will pass through all the packets.useful for testing and ruleset debugging.
     */  
    kUint32AttributeDisableSteering,

    /**
     * Enables the Classifier
     */
    kUint32AttributeClassifierEnable,

    /* MEMTX Module register attributes */
    // Sequence memory address
    kUint32AttributeSequenceMemoryEndAddress,

    // Transmit Command 
    kUint32AttributeTxCommand,

    // Sequence memory address
    kUint32AttributeSequenceMemoryAddress,

    // Packet memory address
    kUint32AttributePacketMemoryAddress,

    // Data to be read from or written to the packet memory
    kUint32AttributePacketMemoryData,

    // The number of packets transmitted is one less than this field. If 0, one packet it transmitted for this sequence
    kUint32AttributeSequenceMemoryNumPackets,

    // Packet length - 1. All packets in this sequence are one byte longer than specified in this field
    kUint32AttributeSequenceMemoryPacketLength,

    // Starting address in the packet memory of the packet to be transmitted for this sequence
    kUint32AttributeSequenceMemoryTxPacketAddr,

    // Start the PRCU
    kBooleanAttributePRCUStart,

    // Rate reporting configuration
    kBooleanAttributeRateReportConfig,

    // Rate reporting capability
    kBooleanAttributeRateReportCapability,

    // Configuration interval reporting
    kUint32AttributeReportConfigInterval,

    // Data-path clock frequency in kHz
    kUint32AttributeDatapathClockFrequency,

    // Sets the packet rate for transmission
    kUint32AttributePacketRate,

    // Reports actual packet rate achieved during transmission.
    kUint32AttributePacketRateReport,

    /* MEMTX Module Functionality Attributes */
    // Load an ERF file
    kStringAttributeLoadERFFile,

    // Configure sequence to transmit
    /**
    * for GTP PHY module 
    **/
    /**
    * Reset the GTP module  
    **/
    kBooleanAttributeGTPReset,
    /**
    * Reset the RX data path of the GTP module  
    **/
    kBooleanAttributeGTPRxReset,
    /**
    * Reset the TX data path of the GTP module
    **/
    kBooleanAttributeGTPTxReset,
    /**
    * Reset the PMA level of the GTP module  
    **/
    kBooleanAttributeGTPPMAReset,

    /**
     * Reported voltages less than this are considered a Warning
     */
    kFloatAttributeVoltageWarningLow,

    /**
     * Reported voltages less than this are considered a Error
     */
    kFloatAttributeVoltageErrorLow,

    /**
     * Reported voltages higher than this are considered a Warning
     */
    kFloatAttributeVoltageWarningHigh,

    /**
     * Reported voltages higher than this are considered a Error
     */
    kFloatAttributeVoltageErrorHigh,

    /* Default configuration modes for the 3.7d */
    kNullAttributeDefaultDS3ATM,

    kNullAttributeDefaultDS3HDLC,

    kNullAttributeDefaultE3ATM,

    kNullAttributeDefaultE3HDLC,

    kNullAttributeDefaultE3HDLCFract,

    kNullAttributeDefaultKentrox,

    /* Additional attributes for the 3.7d */
    /**
     * Enable or disable receive cell header descrambling.
     * If enabled, entire data stream (cell header and payload) is descrambled.
     * If disabled, only cell payload is descrambled.
     */
    kBooleanAttributeCellHeaderDescramble,

    /**
     * A number out of 95 specifying the number of timeslots used by whatever is sending the data. This is used with the fractional E3 HDLC firmware.
     */
    kUint32AttributeHDLCFraction,

    /**
     * Enable deletion of extra bits inserted by some routers (e.g. Cisco) when receiving E3 HDLC.
     */
    kBooleanAttributeFF00Delete,

    /**
     * For DSM component. To enable/disable DSM bypass mode
     */
    kBooleanAttributeDSMBypass,

    /* IPF - V2 attributes  */
    /**
     * Shows IPFV4 is supported or not
     */
    kBooleanAttributeIPFV4Support,
    /**
     * Shows IPFV6 is supported or not
     */
    kBooleanAttributeIPFV6Support,
    /**
     * VLAN skipping supported or not
     */
    kBooleanAttributeVLANSkipping,
    /**
     * VLAN filtering supported or not
     */
    kBooleanAttributeVLANFiltering,
    /**
     * VLAN tags
     */
    kBooleanAttributeVLANTags,
    /**
     * MPLS skipping supported or not
     */
    kBooleanAttributeMPLSSkipping,
    /**
     * MPLS filtering supported or not
     */
    kBooleanAttributeMPLSFiltering,
    /**
     * width of the rules
     */
    kBooleanAttributeIPFRuleWidth,

    /**
     * Duck reader timestamp 
     */
    kUint64AttributeDuckTimestamp,

    /**
     *Duck Phase Correction Attribute.
     */	
    kInt64AttributeDUCKPhaseCorrection,

    /**
     * Is per-stream  drop counter present
     */
    kBooleanAttributePerStreamDropCounterPresent,
    /**
     * Is per-stream  almost buffer full drop counter present
     */
    kBooleanAttributePerStreamAlmostFullDropPresent,
    /**
    *   Per-stream  almost buffer full drop counter 
    */
    kUint32AttributeStreamAlmostFullDrop,

    /**
     * Internal DUCK and CSP identification flag.
     */
    kBooleanAttributeDUCKCSPPresent,

    /**
     * CSP input signal.
     */

    kBooleanAttributeDUCKCSPInput,

    /**
     * CSP output signal
     */

    kBooleanAttributeDUCKCSPOutput,
   
    /**
     * CSP output port enable
     */
    kBooleanAttributeCSPOutputPortEnable,
 
    /** 
     * Internal DUCK select signal.
     */
    kBooleanAttributeInternalDuckSelect,

    /**
     * PPS terminate A enable.
     */
    kBooleanAttributePPSTerminateEnable,

    /**
     * The number of Line Remote Error Indication errors seen
     */
    kUint32AttributeLineREIError,

    /**
     * Selects which statistics to accumulate in a general purpose counter.
     */
    kUint32AttributeCounterSelect,

    /**
     * Holds the value of the selected general purpose statistics counters.
     */
    kUint32AttributeCounter1,
    /**
     * Holds the value of the selected general purpose statistics counters.
     */
    kUint32AttributeCounter2,

    /**
     * Board revision information.
     */
    kUint32AttributeBoardRevision,

    /**
     * PCI device code attribute in card_info
     */
	kUint32AttributePCIDeviceID,

    /**
     * The statistic counter for Alarm Indication Signal.
     */
    kUint32AttributeE1T1AISCounter,

    /**
     * The statistic counter for a CRC.
     */
    kUint32AttributeE1T1CRCCounter,

    /**
     * The statistic counter for a framer.
     */
    kUint32AttributeE1T1FramerCounter,
    /**
     * Indicates if PCS block is present in the chip
     */
    kBooleanAttributePCSBlockPresent,
    /**
     * Indicates if WIS block is present in the chip
     */
    kBooleanAttributeWISBlockPresent,
    /**
     * Indicates if the WAN mode is active or inactive
     */
    kBooleanAttributeWanMode,
    /**
     * C2 byte transmitted when TOSI data is inactive.
     */
    kUint32AttributeTxC2Byte,
    /**
     * C2 octect expected 
     */
    kUint32AttributeRxC2Byte,
    /**
     * Enable overriding the input pin value and use force bit setting 
     * 0 - INput pin : 1 - override enable
     */

    kBooleanAttributeWANOverrideEnable,
    /**
     * ONLY IF WAN OVERRIDE BIT IS ENABLED.
     * 0 - WAN MODE DISABLED : 1 - WAN MODE ENABLED.
     */

    kBooleanAttributeWANModeForce,
    /**
     * Enable Referece CLOCK INPUT PIN VALUE AND USE BIT SETTING 
     * 0 - USE REFSEL0 input PIN  : 1 - OVERRIDE ENABLE
     */

    kBooleanAttributeRefClkOverrideEnable,
    /**
     * VALID ONLY IF CLOCK OVERRIDE BIT IS ENABLED.
     * 0 -  REFSEL0 = 0 (622.08mHZ)  : 1 - REFSEL0 = 1  (155.52MhZ)
     */	
    kBooleanAttributeRefClkForce,

    /** Line Time / Normal 
     * if 0 - Tx data clock sync'd to REFCLKP / WREFCLKP
     * if 1 - Tx data clock sync'd to RECOVERED CLOCK  (slave mode.)
     */
    kBooleanAttributeLineTimeForce,

    /**
     * 0 - Normal mode of operation 
     * 1 - LineTime mode - tx data sync'd to recovered clock.
     */
    kBooleanAttributeLineTimeStatus,

    /** 
     *  LAN in MASTER MODE
     */
    kBooleanAttributeLANMasterMode,  
    /** 
     * WAN in MASTER MODE
     */
    kBooleanAttributeWANMasterMode,
#ifdef ENDACE_LABS    
    /**
     * Enable the CLK6A OUTPUT SIGNAL . this signal is used when CLK64A_SRC is asserted.
     */ 
    kBooleanAttributeClkAEnable,
    /**
     * Select the internal clock signal to output on CLK64A
     */ 
    kBooleanAttributeClkASelect,
    /**
     * Enable the CLK6B OUTPUT SIGNAL . this signal is used when CLK64B_SRC is asserted.
     */ 
    kBooleanAttributeClkBEnable,
    /**
     * Select the internal clock signal to output on CLK64B
     */
    kBooleanAttributeClkBSelect,
#endif
   /** 
    * LAN in SLAVE MODE 
    */
    kBooleanAttributeLANSlaveMode,
#ifdef ENDACE_LABS    
    kBooleanAttributeClkASource, 
#endif
    /** 
     * WAN in SLAVE MODE
     */
    kBooleanAttributeWANSlaveMode,    
    /**
     * Width of the rules
    */
    kUint32AttributeSupportedRuleWidths,
    /**
     * To enable 576 bit long rules 
     */
    kUint32AttributeCurrentRuleWidth,

    /**
     * Select the ruleset for  interface 2
     */
    kBooleanAttributeIPFRulesetInterface2,

    /**
     * Select the ruleset for  interface 3
     */
    kBooleanAttributeIPFRulesetInterface3,

    /* Attributes used by the SONET Channel Management component */
    /**
     * Address field for writing to the SONET Channel Detect module
     */
    kUint32AttributeChannelDetectAddr,

    /**
     * Data field for reading from the SONET Channel Detect module
     */
    kUint32AttributeChannelDetectData,

    /**
     * Address field for writing to the SONET Channel Config Memory module
     */
    kUint32AttributeChannelConfigMemAddr,

    /**
     * Data write field for writing to the SONET Channel Config Memory module
     */
    kUint32AttributeChannelConfigMemDataWrite,

    /**
     * Data read field for reading from the SONET Channel Config Memory module
     */
    kUint32AttributeChannelConfigMemDataRead,

    /**
     * Select the VC-ID connection in the SONET Raw SPE Extraction module
     */
    kUint32AttributeVCIDSelect,

    /**
     * Set the snaplength of the selected VCID in the SONET Raw SPE Extraction module
     */
    kUint32AttributeSPESnaplength,

    /**
     * Read the SONET Channel Detect module configuration and load the SONET Channel Config Memory module
     */
    kBooleanAttributeRefreshConfigMem,

    /* Attributes used by the SONET Connection component */
    /**
     * Select the required SONET connection
     */
    kBooleanAttributeConnectionSelect,

    /* Attributes used by the Pattern Match component */
    /**
     * Set the matching value register
     */
    kUint32AttributeMatchValue,

    /**
     * Set the matching mask register
     */
    kUint32AttributeMatchMask,
	
    /**
     *Per Stream Morphing Configuration Register
     */
    kUint32AttributeMorphingConfiguration,
    
    kBooleanAttributeExtensionHeader,
    
    kUint32AttributeHashSize,
    
    kBooleanAttributeExtErfHeaderStripConfigure,
    
    kBooleanAttributeBfsMorphing,
    
    kBooleanAttributeExtErfStrip,

    /**
     * Enable or disable POH capturing in the SONET Raw SPE Extraction module
     */
    kBooleanAttributePOHCaptureMode,

    /**
     * Enable or disable channelized mode in the SONET Channel Config Memory module
     */
    kBooleanAttributeChannelizedMode,

    /*
     * Attributes for the Line Overhead Mask in the SONET Raw SPE Extraction module
     */

    /**
     * Address field for writing the Line Overhead Mask
     */
    kUint32AttributeLOHMaskAddr,

    /**
     * Write enable field for writing the Line Overhead Mask
     */
    kBooleanAttributeLOHMaskWriteEnable,

    /**
     * Data write field for writing the Line Overhead Mask
     */
    kUint32AttributeLOHMaskDataWrite,

    /**
       Data read field for reading the Line Overhead Mask
    */
    kUint32AttributeLOHMaskDataRead,

    /**
     * This bit indicates if variable hashing is supported in CAT module.
     */
    kBooleanAttributeVariableHashSupport,

    kUint32AttributeBFSRuleWidth,

    /**
     * It controls the rate at which the DAG clock runs. DDS_Rate = 2^32 *
     * (synthetic_frequency / crystal_frequency), or alternatively the generated
     * synthetic_frequency = (DDS_Rate / 2^32) * crystal_frequency.
     */
    kUint32AttributeDDSRate,

    /**
     * Force to overwrite VC-ID channel selection in the SONET Raw SPE Extraction module
     */
    kBooleanAttributeVCIDForce,

    /**
     * Reports the Active BFS Bank.
     */
    kBooleanAttributeBfsActiveBank,

    kBooleanAttributeActivateBank,
    
    /**
     * Shows if the ATM Demapper is available.
     */
    kBooleanAttributeATMDemapperAvailable,

    /**
     * Enable/disable ATM payload scrambling.
     */
    kBooleanAttributePayloadScrambleATM,

    /**
     * Enable/disable PoS payload scrambling.
     */
    kBooleanAttributePayloadScramblePoS,

    kBooleanAttributeDisableLinkPackets,

    kBooleanAttributeDisableDataPackets,	
    
    /**
     * Select the signal type on input source - IRIG-B or PPS
     */
    kUint32AttributeIRIGBInputSource,
 
    /**
     * Format received in auto-detect mode
     */
    kUint32AttributeIRIGBAutoMode,

    /**
     * Time of irigb
     */
    kUint64AttributeIRIGBtimestamp,

    /**
    The PCI Bus type  \sa PCIBusType.
    */
    kUint32AttributePCIBusType,
    /**
     * The Ethernet Scrambling enable/disable.
    */	
    kBooleanAttribute10GEthernetScrambling,
    /**
     * The Ethernet Scrambling enable/disable.
     */		
    kBooleanAttributeWISScrambling,	

    /**
     * Disables the link when used copper sfp modules .
     */
    kBooleanAttributeDisableCopperLink,

    /**
     * XGMII ver 2 attribute To enable RX
     */
    kBooleanAttributeRxEnable,
    
    /**
     * XGMII ver 2 attribute To enable TX
     */
    kBooleanAttributeTxEnable,
    /**
     * The Transceiver Module attributes
     */
    kFloatAttributeTransceiverTemperatureHigh,
    kFloatAttributeTransceiverTemperatureLow,
    kFloatAttributeTransceiverRxPowerHigh,
    kFloatAttributeTransceiverRxPowerLow,
    kFloatAttributeTransceiverTxPowerHigh,
    kFloatAttributeTransceiverTxPowerLow,
    kFloatAttributeTransceiverVoltageHigh,
    kFloatAttributeTransceiverVoltageLow,
    /**
     * Writing '1' to this bit initiates a recovery reset sequence.
     */ 	
    kBooleanAttributeInitiateRecoveryReset,
    /**
     *  Reading '1' from this bit indicates recovery reset has finished.
     */
    kBooleanAttributeRecoveryResetFinished,
    /**	
     * Setting '1' enable the reconciliation sublayer fault.
     */
    kBooleanAttributeRSFaultEnable,	
    /**	
     * 64-bits physical addressing support.
     */
    kBooleanAttributeSupportHiAddr,
    /**	
     * Device associated with NUMA node.
     */
    kIntAttributeIOnode,
    /**
     * Represents the memory (in mebibytes) allocated to a receive or transmit stream at
     * giving IO node.
    */
    kStructAttributeMemNode,

    kStringAttributeUserFirmware1,

    kStringAttributeUserFirmware2,

    kStringAttributeUserFirmware3,

    kUint32AttributeTimeslotId,

    kBooleanAttributeMacWanMode,
    
    kBooleanAttributeTileReset,
    /**
     * Reports and modifies the mode (frequency) of the VCXO. 
    */
    kFloatAttributeVcxoFrequency,
    
    kBooleanAttributeEnableFullRate,
    /**
     * Denotes the depth of the scratch pad register	\
     */
    kUint32AttributeAvailableBytes,
    /**
     * The Scratch Pad Address Register 
     */
    kUint32AttributeScratchPadAddress,
    /**
     * The Scratch Pad Address Data Register 
     */
    kUint32AttributeScratchPadData,
    /**
     * Transceiver module attributes to indicate power levels supported
     */
    kUint32AttributeTransceiverPowerLevel,
    kUint32AttributeTransceiverPowerLevelState,
    /**
     * Base address of stream.
    */
    kUint64AttributeMemBase,
    /**
     * Flags used for the stream memory Allocation.
    */
    kUint32AttributeMemAllocFlags,
    /**
     * New link attribute to register the minimac link bit on Dag 9.2sx2.
     * to avoid the problem of duplicate link.
     */
    kBooleanAttributeMacLink,
    /**
     * HSBM may support configurable burst time out to flush stuck packets.    
    */
    kUint32AttributePbmBurstTimeout,
    /**
     * SDH demux attributes 
    */
    kStructAttributeCardSdhChannelConfig,
    kStructAttributeLineSdhChannelConfig,
    /**
     * CAT version 2 attributes
    */
    kUint32AttributeCATVcidSize,
    kBooleanAttributeVariableVcidSupport,
    kUint32AttributeCatIfaceBitsCount,
    /**
     * SDH dechanneliser attribute 
    */
    kBooleanAttributeDechanActiveBank,
    kBooleanAttributeDechanSwitchBank,
    kBooleanAttributeDechanWriteBankBusy,
    kBooleanAttributeDechanReadBankBusy,
    kUint32AttributeDechanPatterMatcherCmd,
    kUint32AttributeDechanChannelProcessorCmd,
    kUint32AttributeDechanPatterMatcherReadCmd,
    kUint32AttributeDechanChannelBufferCmd,
    /** 
     * Clock source used by transceiver for transmit.
     */	
    kBooleanAttributeTxRefClockSelect,

    /**
     * Clock source used by transceiver for receive
     */	
    kBooleanAttributeRxRefClockSelect,

    /** 
     * Clock source used by XGMII for transmit.
     */
    kBooleanAttributeTxXgmiiClockSelect,

    /**
     * Clock source used by XGMII for receive.
     */
    kBooleanAttributeRxXgmiiClockSelect, 

    /**
     * Indicates if the clock is in split mode or unified mode.
     */	
    kBooleanAttributeSplitMode,

    /**
     * Indicates the receive phy rate on v1 on phy mod manger.
     */
    kUint32AttributeRxPhyRate,

    /**
     * Indicates the tx phy rate on v2 of the phy mod manager.
     */
    kUint32AttributeTxPhyRate,
	
    /**
     * Rx Line Rate
     */   
    kUint32AttributeRxLineRate,

    /**
     * Tx Line Rate
     */
    kUint32AttributeTxLineRate,
    /**
     * Attribute representing the last host IRQ time - time at which last duck interrupt has occured
    */
    kUint64AttributeHostIRQTime,
    /**
     * Attribute representing the raw duck timestamp at the time of last IRQ
    */
    kUint64AttributeDagIRQTime,

    /**
     * Reconfigure the tile to apply the chosen configuration.
     */
    kBooleanAttributeRcfgEnact,
    /**
     * Used to enable or disable rx bit-stream processing
     */
    kBooleanAttributeRxBitProcessingEnable,
    /**
     * Modifies and reports the rx processing mode
     */
    kBooleanAttributeRxBitProcessingMode,
    /**
     * Used to enable or disable tx bit-stream processing
     */ 
    kBooleanAttributeTxBitProcessingEnable,
    /**
     * Modifies and reports the tx processing mode
     */ 
    kBooleanAttributeTxBitProcessingMode,
    /**
     * Causes the input to be rotated one bit each time.
     */
    kBooleanAttributeRxBitSlip,
    /**
     * Swap the order of the rx bits. Default is that the first received bit is bit 64.
     */ 
    kBooleanAttributeRxLsbFirst,
   
    #if 0	 
    /**
     * Causes the output to be rotated one bit each time.
     */
    kBooleanAttributeTxBitSlip,
    #endif 
    /**
     * Swap the order of the tx bits. Default is that the first received bit is bit 64.
     */ 
    kBooleanAttributeTxLsbFirst,
    /**
     * The BitRate in kbps of the line at which the raw mode is to operate.
     */
    kUint64AttributeBitRate,
    /**
     * Multiplier factor of CDR PLL
     */ 
    kUint32AttributeMultiplierFactor,
    /**
     * Divider factor of CDR PLL
     */
    kUint32AttributeDividerFactor,
    /**
     * Transmit buffer has experienced an underflow.
     */
    kBooleanAttributeUnderFlow,
    /**
     * Turns on/off the descrambling of SONET/SDH frames on Rx side for split transmit.
     */
    kBooleanAttributeRxDescramble,
    /**
     * Turns on/off the scrambling of SONET/SDH frames on Tx side for split transmit.
     */
    kBooleanAttributeTxScramble,
    /**
     * Select between Sonet(1) or SDH(0) mode on Rx side for split transmit.
     */
    kBooleanAttributeRxSonetMode,
    /**
     * Select between Sonet(1) or SDH(0) mode on Tx side for split transmit.
     */
    kBooleanAttributeTxSonetMode,
    /**
     * select the CRC type for transmit side for split transmit.
     */
    kUint32AttributeTxCRCSelect,
    /**
     * select the CRC type for receive side for split transmit.
     */
    kUint32AttributeRxCRCSelect,
    /**
     * POS or ATM descrambling for receive side for split transmit.
     */
    kBooleanAttributeRxPayloadDescramble,
    /**
     * POS or ATM scrambling for tx side for split transmit.
     */
    kBooleanAttributeTxPayloadScramble,
    /* 2 more attributes for transceiver modules*/
    kStringAttributeTransceiverVendorRev,
    kStringAttributeTransceiverVendorSN,
    
    kStructAttributeAddScratchPadEntry,
    
    kStructAttributeDeleteScratchPadEntry,
    
    kUint64AttributeVcxoDefaultValue,
    
    kUint32AttributeConfigurationHash,
    /**
     * attributes for format selector to unpackage ERF encapsualtion 
     */
    kUint32AttributeInputErfFormat,
    kUint32AttributeDecapEthertype,
    /**
     * attriubtes to card info, related to firmware images.
     */
    kUint32AttributePowerOnFirmware,
    kBooleanAttributeForceFactoryJumper,
    kBooleanAttributeInhibitProgramJumper,
    /**
     * Indicates if the QT2225 framer is up and running or not.
     */
    kBooleanAttributeHeartBeat,
    /**
     * Indicates if a PBM stream is in Paused state
     */
    kBooleanAttributeStreamPause,
    /**
     * A ???1??? puts the RX MAC layer logic in reset while ???0??? clears it
     */
    kBooleanAttributeRxReset,
    /**
     * A ???1??? puts the TX MAC layer logic in reset while ???0??? clears it
     */
    kBooleanAttributeTxReset,
    /**
     * Attribute that indicates default frequency in MHz. 
     */
    kUint32AttributeDefaultFrequency,
    /**
    Indicates whether stream is large (i.e. can be larger than 2GB) kBooleanAttributeLarge
    */
    kBooleanAttributeStreamLarge,
    /**
     *	Attirbute that indicates what frequency the VCXO should be set to.
     */	
    kUint32AttributeRequestFrequency,
    
    /**
     * List of line_rates that are supported by this transceiver.
     */
    kStructAttributeTransceiverLineRates,

    /**
     * Attribute for  TimestampConverter component (converts extracted NPB timestamp to ERF timestamp)
     */
    kUint32AttributeTimestampConvertMode,
    /**
     * SplitMode attriubte stored in Scratch Pad component.
     */	
    kBooleanAttributePersistantSplitMode,
    /**
     * Attributes for  TimestampConverter components protection mechanism
     */
    kBooleanAttributeTimestampConvertCheck,
    kUint32AttributeTimestampConvertCheckWindow,
    kBooleanAttributeTimestampConvertTsFound,
    kUint32AttributeTimestampConvertDrop,
    kBooleanAttributeTimestampConvertCheckReset,
    kBooleanAttributeTimestampConvertDropClear,
    /**
     * Attribute for Port Identifier Type field (e.g 0x01 for MAC address)
     */	
    kUint32AttributePortIdTypeField,
    /**
     * Attribute for bringing down the link on both Copper and Fiber Ethernet links.
     */	
    kBooleanAttributeAdminLinkDown,
    /**
     * Port type attribute to differentiate between a capture port and a timing port.
     */	
    kUint32AttributePortType,
    /**
     * Reports the RX SGMII Auto negotiated phy-rate.
     */
    kUint32AttributeRxSGMIIAutoNegRate,
    /**
     * Reports the Tx SGMII Auto negotiated phy-rate.
     */
    kUint32AttributeTxSGMIIAutoNegRate,
    /**
        BFS maximum rules count
    */
    kUint32AttributeBFSMaxRuleCount,
    /**
     * Indicates the transceiver type 
     * '1' - Fiber
     * '0' - Copper
     */
    kBooleanAttributeTransceiverType,
    /**
     * PSoC High Resolution Sensor Table version
     * */
    kFloatAttributeSensorVersion,
    /**
     * PSoC Card version
     * */
    kFloatAttributeSensorCardVersion,
    /**
     * PSoC code version
     * */
    kFloatAttributeSensorPsocVersion,
    /**
     * PSoC Sensor count
     * */
    kUint32AttributeSensorCount,
    /**
     * PSoC Sensor information structure
     * */
    kStructAttributeSensorInfo,
    /**
     * Internal status attributes
     * */
    kBooleanAttributeInternalStatusFlag,
    kUint32AttributeInternalStatusCount,
    kUint32AttributeInternalID,
    kUint32AttributeInternalStatusReg,
    /**
     * New attributes added for psoc non-volatile fields
     */
    kStringAttributeICTTag,
    kStringAttributeBISTTag,
    /**
     *  New attributes for BFS v6 - Tunneling related
     */
    kBooleanAttributeBFSIpinIp,
    kBooleanAttributeBFSPmIpv6,
    kBooleanAttributeBFSVxLan,
    kBooleanAttributeBFSGre,
    kBooleanAttributeBFSGtp,
    kBooleanAttributeBFSMplsOverVlan,
    /**
     *  New attributes for DUCK - HexRail
     */
    kBooleanAttributeSFPConnector,
    kBooleanAttributePTPSupport,
    kBooleanAttributeCspHrSupport,
    kBooleanAttributeCspHrSlave,
    kBooleanAttributeCspHrMaster,
    kBooleanAttributeLoadLowSupport,

    kBooleanAttributeLatchB1ErrorCount,
    kBooleanAttributeLatchB2ErrorCount,
    kBooleanAttributeLatchB3ErrorCount,
    
    /** Generic port attributes needed for Copper modules */
    kUint32AttributeCopperCableLength,
    kBooleanAttributeCopperTransceiverMode,
    /**
    Index of the PHC device for this card, e.g. /dev/ptpX
    */
    kUint32AttributeDUCKPHCIndex,
    
    /**
    Offset of this DUCK from the PTP Timebase, typically the UTC-TAI offset.
    */
    kInt64AttributeDUCKPHCOffset,
    /**
     * Nominal Transceiver Bit rate.
     */
    kUint32AttributeTransceiverNominalBitRate,
    /**
     * Supported Rx PhyRates
    */
    kStructAttributeSupportedRxPhyRates,
    /**
     * Supported Tx PhyRates
    */
    kStructAttributeSupportedTxPhyRates,
    /**
     * Supported PhyRates
    */
    kStructAttributeSupportedPhyRates,
    /**
     * TX stream features attributes
     */
    kBooleanAttributeTxAspectPresent,
    kBooleanAttributeTxTRPresent,
    kBooleanAttributeTxTTRPresent,
    kUint32AttributeTxSrcCrc,
    kUint32AttributeTxCrcMode,
    kBooleanAttributeTxPassRxError,
    /**
     * TX behaviour mode attributes
     */
    kUint32AttributeTxBehaviourMode,
    /**
     * Source ID module attributes
     */
    kUint32AttributeSourceCardId,
    /**
     * BFS v7 attribute
     */
    kUint32AttributeBFSExtHdrType,
    /**
     * Enables periodic Provenance record insertion
     */
    kBooleanAttributeEnableProvenance,
    /**
     * Manually triggers Provenance record insertion
     */
    kBooleanAttributeProvenanceTrigger,
    /**
     * Maximum rlen supported by Provenance Module.
     */
    kUint32AttributeProvenanceMaxRlen,
    /**
     * Payload length requested for Provenance Module.
     */
    kUint32AttributeProvenanceLengthRequested,
    /**
     * ERF Type for Provenance Module.
     */
    kUint32AttributeProvenanceErfType,
    /**
     *  Address Register for Provenance Module
     */
    kUint32AttributeProvenanceAddressRegister,
    /**
     *  Data Register for Provanance Module.
     */
    kUint32AttributeProvenanceDataRegister,
    /**
     * Contents of Provenance Record.
     */
    kStructAttributeProvenanceRecord,
    /**
    Share the memory hole between the receive and transmit streams.  Used for inline forwarding of packets.
    */
    kBooleanAttributeStreamOverlap,

    /**
     * Reports presence of the dagnetdev on top of a given port
     */
    kBooleanAttributeHasNetdev,
    /**
     * Name of the dagnetdev on top of a given port
     */
    kStringAttributeNetdevName,
    /**
     * RX stream of the dagnetdev
     */
    kUint32AttributeNetdevRxStream,
    /**
     * TX stream of the dagnetdev
     */
    kUint32AttributeNetdevTxStream,
     /**
     * TX stream features attribute
     */
    kBooleanAttributeTxPendingData,
    /**
    The first valid attribute code.
    */
    kFirstAttributeCode = kBooleanAttributeActive,
    /**
    The last valid attribute code.
    */
    kLastAttributeCode = kBooleanAttributeTxPendingData
} dag_attribute_code_t;
/*@}*/

/**
\defgroup AttrEnums Attributes Values enumerators
Codes for identifying attributes.
*/
/*@{*/

/**
SC256 data.
*/
typedef struct
{
    uint8_t data2;
    uint32_t data1;
    uint32_t data0;
    
} sc256_72bit_data_t;

typedef struct
{
    uint8_t mask2;
    uint32_t mask1;
    uint32_t mask0;
    
} sc256_72bit_mask_t;


typedef struct
{
    /**
    Data representing the 72-bit search term.
    */
    uint32_t data2;
    uint32_t data1;
    uint32_t data0;
    
    uint32_t ssel;
    uint32_t cmpr;
    uint32_t rslt;
    uint32_t gmsk;
    
    /**
    Result of the search.
    */
    uint8_t found;
    
    /**
    Where the search term is if found.
    */
    uint32_t index;
    
} sc256_72bit_search_data_t;

/**
\defgroup SC256SearchLength SC256 Search Length
Values representing supported search lengths.
*/
/*@{*/
typedef enum
{
    kSC256SearchLength72,
    kSC256SearchLength144
} sc256_search_length_t;
/*@}*/

/**
\defgroup ScratchPadEntry Entry into a scratch pad register.
*/
/*@{*/
typedef struct
{
    dag_attribute_code_t field_id;
    uint32_t length;
    uint8_t data[32];
}scratch_pad_entry_t;
/*@}*/

/**
\defgroup ScratchPadEntry Entry into a scratch pad register.
*/
/*@{*/
typedef struct
{
    dag_attribute_code_t field_id;
    uint32_t attribute_index;
}scratch_pad_del_entry_t;
/*@}*/
#if 0
/**
\defgroup ProvenanceRecord contents.
*/
/*@{*/
typedef struct
{
    /*Erf header LO*/
    uint16_t rlen;
    uint8_t flags;
    uint8_t ext_hdr_bit;
    uint8_t erf_type;
    /*Erf header HI*/
    uint16_t wlen;
    uint16_t sequence_number;
    /*Ext header 0*/
    uint8_t header_type;
    uint32_t payload_hash;
    /*Ext header 1*/
    uint8_t colour;
    uint32_t flow_hash;
    /*Payload*/
    uint32_t pload[64];
}provenance_record_t;
/*@}*/
#endif

typedef struct
{
    /**
    Data representing the 144-bit search term.
    */
    uint32_t data[5];
    
    uint32_t ssel;
    uint32_t cmpr;
    uint32_t rslt;
    uint32_t gmsk;
    
    /**
    Result of the search.
    */
    uint8_t found;
    
    /**
    Where the search term is if found.
    */
    uint32_t index;

} sc256_144bit_search_data_t;


/**
\defgroup NetworkModeCodes Network Modes
Values representing network modes.
*/
/*@{*/
typedef enum
{
    kNetworkModeInvalid,
    kNetworkModeATM,
    kNetworkModePoS,
    kNetworkModeRAW,
    kNetworkModeEth,
    kNetworkModeHDLC,
    kNetworkModeWAN
} network_mode_t;
/*@}*/

/**
\defgroup CounterSelectCodes Counter codes on 3.8S
Counter select codes for the 3.8s
*/
/*@{*/
typedef enum
{
    kCounterSelectB1Error,
    kCounterSelectB2Error,
    kCounterSelectB3Error,
    kCounterSelectATMBadHEC,
    kCounterSelectATMCorrectableHEC,
    kCounterSelectATMRxIdle,
    kCounterSelectATMRxCell,
    kCounterSelectPoSBadCRC,
    kCounterSelectPoSMinPktLenError,
    kCounterSelectPoSMaxPktLenError,
    kCounterSelectPoSAbort,
    kCounterSelectPoSGoodFrames,
    kCounterSelectPoSRxBytes
} counter_select_t;
/*@}*/

/**
\defgroup EthernetCodes Ethernet Modes
Values for setting Ethernet mode.  Actual Ethernet modes available on a DAG card will depend on the hardware and the loaded firmware.
*/
/*@{*/
typedef enum
{
    kEthernetModeInvalid,
    /**
    10 Gigabit Ethernet (LAN).
    */
    kEthernetMode10GBase_LR,
    /**
    *10 Gigabit Ethernet (WAN).
    * Long wavelength (1310nm) single mode fiber.
    */
    kEthernetMode10GBase_LW,
    /**
     * Short wavelength (850nm) Multimode fiber with 66B encoding.
     */
    kEthernetMode10GBase_SR,
    /**
     * Extra long wavelength (1550nm) single mode fiber with 66B encoding.
     */
    kEthernetMode10GBase_ER
    
    
} ethernet_mode_t;
/*@}*/


typedef enum
{
    kDemapperTypeATM,
    kDemapperTypeHDLC
    
} demapper_type_t;


/**
\defgroup TributaryCodes Tributary Units
An enumeration of valid tributary units.
*/
/*@{*/
typedef enum
{
    /**
    Same as VT1.5.
    */
    kTU11,
    
    /**
    Same as VT2.
    */
    kTU12

} tributary_unit_t;
/*@}*/


/**
 * \defgroup LineInterfaceCodes Line Interfaces
 * Values for setting the line interfaces. Actual line interfaces available depend on the hardware and loaded firmware.
 * .
 */
/*@{*/
typedef enum
{
    /**
     * Serial Gigabit Media Independent Interface
     */
    kLineInterfaceSGMII,
    /**
     * Gigabit Interface Converter
     */
    kLineInterfaceGBIC,
    kLineInterfaceInvalid
} line_interface_t;
/*@}*/

/**
\defgroup LineRateCodes Line Rates
Values for setting line rates.  Actual line rates available on a DAG card will depend on the hardware and the loaded firmware.
*/
/*@{*/
typedef enum
{
    kLineRateInvalid,
    
    /**
    Determine line rate automatically (i.e. by negotiating with peer).
    */
    kLineRateAuto,
    
    /**
    OC-1c line rate (approximately 52 Mbps).
    */
    kLineRateOC1c,
    
    /**
    Concatenated OC-3 line rate (approximately 155 Mbps).
    */
    kLineRateOC3c,
    
    /**
    Concatenated OC-12 line rate (approximately 622 Mbps).
    */
    kLineRateOC12c,
    
    /**
    Concatenated OC-48 line rate (approximately 2488 Mbps).
    */
    kLineRateOC48c,
    
    /**
    Concatenated OC-192 line rate (approximately 9952 Mbps).
    */
    kLineRateOC192c,
    
    /**
    Ethernet line rate (10 Mbps).
    */
    kLineRateEthernet10,
    
    /**
    Fast Ethernet line rate (100 Mbps).
    */
    kLineRateEthernet100,
    
    /**
    Gigabit Ethernet line rate (108800 Mbps).
    */
    kLineRateEthernet1000,

    /**
    STM-0 line rate (approximately 52 Mbps).
    */
    kLineRateSTM0,

    /**
    STM-1 line rate (approximately 155 Mbps).
    */
    kLineRateSTM1,

    /**
    STM-4 line rate (approximately 622 Mbps).
    */
    kLineRateSTM4,

    /**
    STM-16 line rate (approximately 2488 Mbps).
    */
    kLineRateSTM16,

    /**
    STM-64 line rate (approximately 9952 Mbps).
    */
    kLineRateSTM64,
    /**
    10 Gigabit Ethernet line rate 
    */
    kLineRateEthernet10GE,
    /**
     10 Gigabit Ethernet WAN
     */
    kLineRateWAN,
    /**
    40 Gigabit Ethernet line rate 
    */
    kLineRateEthernet40GE,
    /**
    100 Gigabit Ethernet line rate 
    */
    kLineRateEthernet100GE,
    /**
     Raw bit caputre.
     */ 
    kLineRateRaw,
    /**
     * SGMII Auto Neg
     */ 
    kLineRateSGMIIAuto,
    /*
     *SGMII 10 Mbps
     */
    kLineRateSGMII10,
    /*
     *SGMII 100 Mbps
     */
    kLineRateSGMII100,
    /*
     *SGMII 1000 Mbps
     */
    kLineRateSGMII1000,
} line_rate_t;
/*@}*/


/*@{*/
typedef struct
{
    uint8_t oc3_supported:1;
    uint8_t oc12_supported:1;
    uint8_t oc48_supported:1;
    //uint8_t oc192_supported:1;
    uint8_t eth1000_supported:1;
    uint8_t eth10G_supported:1;
    uint8_t eth40G_supported:1;
    uint8_t eth100G_supported:1;
    uint8_t anyrate_supported:1;
    uint8_t sgmii_auto_supported:1;
} transceiver_line_rate_t;

typedef struct
{
    uint8_t oc3_phy_rate_supported:1;         
    uint8_t oc12_phy_rate_supported:1;
    uint8_t oc48_phy_rate_supported:1;
    uint8_t oc192_phy_rate_supported:1;
    uint8_t eth1000_phy_rate_supported:1;
    uint8_t eth100_phy_rate_supported:1;
    uint8_t eth10_phy_rate_supported:1;
    uint8_t eth10G_phy_rate_supported:1;
    uint8_t eth40G_phy_rate_supported:1;
    uint8_t eth100G_phy_rate_supported:1;
    uint8_t wan_phy_rate_supported:1;           
    uint8_t raw_phy_rate_supported:1;          
    uint8_t sgmii_auto_phy_rate_supported:1;
} supported_phyrate_t;


/**
\defgroup BitProcessingModes Bit Processing Modes
Values for setting Bit Processing Mode.  Actual bit processing mode  available on a DAG card will depend on the hardware and the loaded firmware.
*/
/*@{*/
typedef enum
{
  kBitProcessingModeInvalid,
  /**
   Raw Mode Processing.
   */
  kBitProcessingModeRaw,
  /**
   OC-3c line rate (approximately 155 Mbps).
  */
  kBitProcessingModeOC3c,
  /**
   Concatenated OC-12 line rate (approximately 622 Mbps).
  */
  kBitProcessingModeOC12c,
  /**
   Concatenated OC-48 line rate (approximately 2488 Mbps).
  */
  kBitProcessingModeOC48c,
  /**
  Concatenated OC-192 line rate (approximately 9952 Mbps).
  */
  kBitProcessingModeOC192c,
  /**
  10 Gigabit Ethernet line rate 
  */
  kBitProcessingMode10GE
  
}bit_processing_mode_t;
/*@}*/

/* removed due not used any wear instead of that used kMaster
typedef enum
{
    kClockMaster,
    kClockSlave
    
} dag_clock_t;
*/

/**
\defgroup CRCCodes CRC Schemes
Codes for identifying which type of CRC to use.
*/
/*@{*/
typedef enum
{
    kCrcInvalid,
    
    /**
    No CRC.
    */
    kCrcOff,
    
    /**
    16-bit CRC.
    */
    kCrc16,
    
    /**
    32-bit CRC.
    */
    kCrc32

} crc_t;
/*@}*/

// For the MemTx module
typedef enum
{
    kTxCommandInvalid,
    kTxCommandStop,
    kTxCommandBurst,
    kTxCommandStart
} tx_command_t;

/**
\defgroup LineTypeCodes Line Types
Codes for identifying various values of the line type attribute.
*/
/*@{*/
typedef enum
{
    kLineTypeInvalid,
    kLineTypeNoPayload,
    kLineTypeE1,
    kLineTypeE1crc,
    kLineTypeE1unframed,
    kLineTypeT1,
    kLineTypeT1sf,
    kLineTypeT1esf,
    kLineTypeT1unframed
} line_type_t;
/*@}*/


/**
\defgroup TerminationCodes Termination Codes
*/
/*@{*/
typedef enum
{
    kTerminationInvalid,
    /* Both external. */
    kTerminationExternal,
    
    /* One internal, one external. */
    kTerminationRxExternalTx75ohm,
    kTerminationRxExternalTx100ohm,
    kTerminationRxExternalTx120ohm,
    kTerminationRx75ohmTxExternal,
    kTerminationRx100ohmTxExternal,
    kTerminationRx120ohmTxExternal,
    
    /* Both internal. */
    kTermination75ohm,
    kTermination100ohm,
    kTermination120ohm,

	kTerminationRxExternal,
	kTerminationTxExternal
} termination_t;
/*@}*/


/**
\defgroup ZeroCodeSuppressCodes Zero Code Suppression Algorithms
*/
/*@{*/
typedef enum
{
	kZeroCodeInvalid,
    /**
    Use B8ZS for zero code suppression.
    */
    kZeroCodeSuppressB8ZS,
    
    /**
    Use AMI for zero code suppression.
    */
    kZeroCodeSuppressAMI
    
} zero_code_suppress_t;
/*@}*/


typedef enum
{
    kMuxMerge,
    kMuxSplit
    
} mux_t;


/**
\defgroup LEDStatus LED Status
The status of the LED on the DAG 3.7T pod. Use with the attribute kUint32AttributeLEDStatus to change properties of an LED on the pod.
*/
/*@{*/
typedef enum
{
    kLEDOn = 0x0,
    kLEDOff = 0x1,
    kLEDAtBlinkRate0 = 0x2
    
} led_status_t;
/*@}*/


/**
\defgroup VirtualContainerCodes Virtual Container Size
An enumeration of valid virtual container sizes.
*/
/*@{*/
typedef enum
{
   kVCInvalid,
   kVC3,
   kVC4,
   kVC4C
   
} vc_size_t;
/*@}*/


typedef enum
{
    kPointerStateLossOfPointer = 0x0,
    kPointerStateAlarmSignalIndicator,
    kPointerStateValid,
    kPointerStateConcatenationIndicator
    
} vc_pointer_state_t;


/**
\defgroup PCIBusSpeed PCI Bus Speeds
An enumeration of valid PCI bus speeds.
*/
/*@{*/
typedef enum
{
    /**
    33MHz PCI.
    */
    kPCIBusSpeed33Mhz,
    
    /**
    66MHz PCI or PCI-X.
    */
    kPCIBusSpeed66Mhz,
    
    /**
    100MHz PCI-X.
    */
    kPCIBusSpeed100Mhz,
    
    /**
    133MHz PCI-X.
    */
    kPCIBusSpeed133Mhz,
    
    /**
    PCI or PCI-X bus speed is none of the known values.
    */
    kPCIBusSpeedUnknown,
    
    /**
    PCI or PCI-X bus speed could not be reliably determined.
    */    
    
    kPCIBusSpeedUnstable,

    /**
    vDAG virtual PBM bus, report speed as 'Virtual'.
    */    

    kPCIBusSpeedVirtual,
    
   /** PCI-E bus spped which at the moment is equivalent as the tarined number of line 
    * 1 lane is 2 Gbs and we have possible configurations 1,2,4,8, at the moment 16 lanes (32Gbs) is not supported 
   */ 
    kPCIEBusSpeed2Gbs,
    kPCIEBusSpeed4Gbs,
    kPCIEBusSpeed8Gbs,
    kPCIEBusSpeed16Gbs,
    /** PCI-E Gen2 bus speeds 
    */
    kPCIEv2BusSpeed4Gbs,
    kPCIEv2BusSpeed8Gbs,
    kPCIEv2BusSpeed16Gbs,
    kPCIEv2BusSpeed32Gbs,
    /** PCI-E Gen3 bus speeds
    */
    kPCIEv3BusSpeed8Gbs,
    kPCIEv3BusSpeed16Gbs,
    kPCIEv3BusSpeed32Gbs,
    kPCIEv3BusSpeed64Gbs
} pci_bus_speed_t;
/*@}*/


/**
\defgroup PCIBusType PCI Type
An enumeration of PCI bus types
*/
/*@{*/
typedef enum
{
    kBusTypeUnknown = 0,
    kBusTypeVirtual,
    kBusTypePCI,
    kBusTypePCIX,
    kBusTypePCIE,
    kBusTypePCIEv2,
    kBusTypePCIEv3
} pci_bus_type_t;
/*@}*/

typedef enum
{
    kConnectionTypeNotConfigured = 0x0,
    kConnectionTypePCM31 = 0x1,
    kConnectionTypePCM30 = 0x2,
    kConnectionTypePCM24 = 0x4,
    kConnectionTypeUseTimeslotConfig = 0x5
    
} connection_type_t;

typedef enum
{
    kConnectionTypeNULL     = 0x0,
    kConnectionTypeChan     = 0x1,
    kConnectionTypeHyper    = 0x2,
    kConnectionTypeSub      = 0x3,
    kConnectionTypeRaw      = 0x4,
    kConnectionTypeChanRaw  = 0x5,
    kConnectionTypeHyperRaw = 0x6,
    kConnectionTypeSubRaw   = 0x7

} connection_type37t_t;

/**
\defgroup PayloadMappingCodes Payload Mappings
An enumeration of valid payload mappings
*/
/*@{*/
typedef enum
{
    kPayloadTypeNotConfigured = 0x0,
    kPayloadTypeATM = 0x01,
    kPayloadTypeHDLC = 0x02,
    kPayloadTypeRAW = 0x05
    
} payload_type_t;
/*@}*/


/**
\defgroup Direction data direction
An enumeration of valid directions of data
*/
/*@{*/
typedef enum
{
    kDirectionUndefined = 0x0,
    kDirectionReceive = 0x01,
    kDirectionTransmit = 0x02

} direction_t;
/*@}*/

typedef enum
{
    kPayloadMappingDisabled= 0x0,
    kPayloadMappingAsync = 0x01,
    kPayloadMappingBitSync = 0x03,
    kPayloadMappingByteSync1 = 0x04,
    kPayloadMappingByteSync2 = 0x05
    
} payload_mapping_t;

typedef enum
{
    kTXV5ValueDisabled	= 0x0,
    kTXV5ValueAsync		= 0x02,			//0x01 is reserved
    kTXV5ValueBitSync	= 0x03,
    kTXV5ValueByteSync	= 0x04
} txv5_value_t;

/**
\defgroup ConnectionDescriptionStruct Connection Description
A struct for adding connections to a channelized OC-3/OC-12 card.
*/
/*@{*/
typedef struct
{
    uint32_t mTUG3_ID;
    uint32_t mVC_ID;
    uint32_t mTUG2_ID;
    uint32_t mTU_ID;
    uint32_t mPortNumber;
    connection_type_t mConnectionType;
    payload_type_t mPayloadType;
    uint8_t mScramble;
    uint8_t mHECCorrection;
    uint8_t mIdleCellMode;
    uint32_t mTimeslotMask;
    uint32_t mConnectionNumber;
    uint32_t mTableIndex;
    
} connection_description_t;
/*@}*/


/**
\defgroup ConnectionDescription37tStruct Connection Description on 3.7T
A struct for adding connections to a Dag 3.7T card.
*/
/*@{*/
typedef struct
{
    connection_type37t_t mConnectionType;
    payload_type_t mPayloadType;
    direction_t mDirection;
    uint32_t mline;
    uint32_t mTimeslot;
    uint32_t mMask;
    uint32_t mConnectionNumber;
    
} connection_description_37t_t;
/*@}*/

/**
\defgroup ErfMux37tStruct Steering options for 3.7T
A struct for directing data on a Dag 3.7T card.
*/
/*@{*/
typedef struct
{
    
	uint32_t mHost;
	uint32_t mLine;
	uint32_t mXscale;
    
} erf_mux_37t_t;
/*@}*/

/**
\defgroup Direction data direction
An enumeration of valid directions of data
*/
/*@{*/
typedef enum
{
    kErfToHost = 0x0,
    kErfToLine = 0x01,
    kErfToXscale = 0x02

} erf_mux_steering_37t_t;
/*@}*/
/**
\defgroup MasterSlaveCodes SONET Clock Master/Slave Status
An enumeration of valid SONET clock master/slave status.
*/
/*@{*/
typedef enum
{
    kMasterSlaveInvalid,
    
    /**
    SONET clock master.
    */
    kMaster,
    
    /**
    SONET clock slave.
    */
    kSlave
    
} master_slave_t;
/*@}*/


/**
\defgroup SonetTypeCodes SONET Types
An enumeration of valid SONET types.
*/
/*@{*/
typedef enum
{
    kSonetTypeInvalid,
    
    /**
    Channelized SONET.
    */
    kSonetTypeChannelized,
    
    /**
    Concatenated SONET.
    */
    kSonetTypeConcatenated
    
} sonet_type_t;
/*@}*/


typedef enum
{
    kDag71sRevIdInvalid,
    kDag71sRevIdATM,
    kDag71sRevIdATMHDLC,
    kDag71sRevIdATMHDLCRAW,
    kDag71sRevIdHDLC,
    kDag71sRevIdHDLCRAW
    
} dag71s_channelized_rev_id_t;


/**
\defgroup TerfStripCrcCodes TERF CRC Strip Options
Values for TERF CRC stripping.
*/
/*@{*/
typedef enum
{
    kTerfStripInvalid,
    
    /**
    Transmit ERF records without stripping any trailing bits.
    */
    kTerfNoStrip,
    
    /**
    Transmit ERF records after stripping the last 16 bits.
    */
    kTerfStrip16,
    
    /**
    Transmit ERF records after stripping the last 32 bits.
    */
    kTerfStrip32
    
} terf_strip_t;
/*@}*/

/**
\defgroup 
Values for Timed release mode.
*/
/*@{*/
typedef enum
{
    kTrTerfTimeModeInvalid,

    /**
    No mode
    */
    kTrTerfNoDelay,
    
    /**
    Transmit ERF records in relative mode.
    */
    kTrTerfRelative
   
} terf_time_mode_t;
/*@}*/


/**
\defgroup SteeringCodes Load Balancing Steering Options
An enumeration of valid codes for load-balancing across multiple receive streams.
*/
/*@{*/
typedef enum
{
    /**
    Send all ERF records to receive stream 0.
    */
    kSteerStream0,
    
    /**
    Send ERF records to receive stream 0 or receive stream 2 based on a parity calculation.
    */
    kSteerParity,
    
    /**
    Send ERF records to receive stream 0 or receive stream 2 based on a CRC calculation.
    */
    kSteerCrc,
    
    /**
    Send ERF records received on physical interface 1 to receive stream 0, and those received on physical interface 2 to receive stream 2.
    */
    kSteerIface,

	kSteerColour
    
} steer_t;
/*@}*/


typedef enum
{
    kSteerInvalid,
    kSteerLine,
    kSteerHost,
    kSteerIXP,
    kSteerDirectionBit
    
} erfmux_steer_t; 

/**
 * \defgroup FramingMode Framing Mode
 * Enumeration of different types of framing modes available
 */
/*@{*/
typedef enum
{
    kFramingModeInvalid,
    kFramingModeDs3Cbit,
    kFramingModeDs3CbitIF,
    kFramingModeDs3m23,
    kFramingModeDs3m23IF,
    kFramingModeDS3CbitPLCP,
    kFramingModeDS3M23PLCP,
    kFramingModeDs3CbitEF,
    kFramingModeDs3CbitFF,
    kFramingModeDs3m23EF,
    kFramingModeDs3m23FF,
    kFramingModeE3,
    kFramingModeE3G751PLCP,
    kFramingModeE3G751IF,
    kFramingModeE3CC
} framing_mode_t;
/*@}*/

/**
* \defgroup DUCKTimeInfo DUCK Time Info
* mStart is the host time when the duck statistics were last cleared.
* this happen when the firmware is loaded, the duck is reset, or the
* clearstats command is given. It indicates the beginning of the period
* that the stats counters cover.
* 
* mEnd is the host time at the last synchronisation event
* (pulse/interrupt). It indicates when the last sync input was received.
* Will be the current host time when the card is synchronised.
* 
*/
/*mDag*/
/*This field is DUCK time at the last synchronization event.
** As of now this is same as the mEnd field(host time).ie because 
** take the time info from the PC.May change in future implementation.
**  Therefore adding the field.*/
/*@{*/
typedef struct
{
    time_t mStart;
    time_t mEnd;
    /*to read the Last_Ticks field in duckinf structure - time_t*/
    time_t dagTime;
} duck_time_info_t;
/*@}*/
/*Struct - CATEntryInfo*/

#include "dagnew.h"
typedef struct
{
	uint32_t stream_num;
	struct range
	{
		uint32_t min;
		uint32_t max;
	}stream_range[DAG_STREAM_MAX];
} hlb_range_t;

/**Struct HAT Entry Info*/
typedef struct
{
	uint32_t bin_num;
	struct binrange
	{
		uint32_t min;
		uint32_t max;
	}bin_range[DAG_HASH_BIN_MAX];
}hat_range_t;

/* Struct memory node Entry Info */
typedef struct
{
	uint64_t mem_size;
	uint32_t node;
}mem_node_t;

/** The following are possible values for the TXC2 byte.
* It should be noted that these enums are provided only
* for convenience. The user is free to write any value
* to the card.
*/
typedef enum
{
       kTXC2Undefined          = -1,
	   kTXC2Unequiped,
       kTXC2Reserved,
       kTXC2TUG,
       kTXC2LockedTU,
       kTXC2E3T3,
       kTXC2E4                 = 0x12,
       kTXC2ATM,
       kTXC2DQDB,
       kTXC2FDDI,
       kTXC2HDLC,
       kTXC2SDL,
       kTXC2HDLS,
       kTXC210GE               = 0x1A,
       kTXC2IPinPPP            = 0xCF,
       kTXC2PDI                = 0xE1,
       kTXC2TestSignalMapping  = 0xFE,
       kTXC2AIS                = 0xFF

} txc2_value_t;

/**
\defgroup TransceiverModuleCodes Module Identifier
An enumeration of known transceiver module identifiers
*/
/*@{*/
typedef enum 
{
    kInvalidModuleType = -1,
    kUnknown = 0x00,
    kGBIC = 0x01,
    kSolderedConnector = 0x02,
    kSFP = 0x03,
    kXBI300Pin = 0x04,
    kXENPAK = 0x05,
    kXFP = 0x06,
    kXFF = 0x07,
    kXPFE = 0x08,
    kXPAK = 0x09,
    kX2 = 0x0a,
    kDWDM = 0xb,
    kQSFP = 0xc,
    kQSFPPlus = 0xd,
    kCFP = 0xe
}module_identifier_t;
/*@}*/

/*@}*/

/**
\defgroup SensorType Sensor  Identifier
An enumeration of known sennsor types.
*/
/*@{*/

typedef enum dag_sensor_type_s
{
    kSesnorTypeInvalid = 0,
    kSensorTypeVoltage = 0x01,
    kSensorTypeTemperature = 0x02,
    kSensorTypeRPM = 0x03,
    kSensorTypeCurrent = 0x04,
    kSensorTypePower = 0x05,
    kSensorTypeAirFlow = 0x06,
    kSensorTypeHumidity = 0x07,
    kSensorTypeSwitch = 0x08,
    kSensorTypeVersion = 0x09,
    kSensorTypeLastDefined=kSensorTypeVersion,
    kSensorTypeOpticalPower=(kSensorTypeLastDefined+1), /*to distinguish optical power*/
    kSensorTypeCounter=(kSensorTypeLastDefined+2) /*to support generic counter, req by app team*/

}dag_sensor_type_t;
/*@}*/

/**
\defgroup SensorInfo  Sensor structure
A struct for representing the measured seonsor details
\sa SensorType
*/
/*@{*/
typedef struct dag_sensor_measurement_data_s
{
    dag_sensor_type_t mType;
    float mValue;
    float mLowThresholdValue;
    float mHighThresholdValue;
    uint8_t mThresholdStatus;
}dag_sensor_measurement_data_t;;
/*@}*/

/**
\defgroup ErfFormat Output ERF format
An enumeration of different ERF encapsualtion types
*/
/*@{*/
typedef enum {
    kErfFormatInvalid = 0,
    kErfFormatFwdEthernet = 1,
    kErfFormatSingleErfEncap = 2,
    kErfFormatMultiErfEncap = 3,
    kErfFormatDirectErf = 4
}dag_erf_format_t;
/*@}*/

/*@}*/

/**
\defgroup TimestampConvertMode  Time stamp conversion mode
An enumeration of different timestamp conversion modes
*/
/*@{*/
typedef enum {
	kTimestampConvertModeInvalid = 0,
	kTimestampConvertModeOff = 1,
	kTimestampConvertModeApcon = 2,
	kTimestampConvertModeCpacketHigh = 3,
	kTimestampConvertModeCpacketLow = 4
}dag_timestamp_conversion_mode_t;
/*@}*/

/**
\defgroup TxBehaviourMode TX Modes
Values representing tx modes.
*/
/*@{*/
typedef enum
{
	kTxModeInvalid			= -1,
	kTxModeSingleStream 	= 0,
	kTxModeStreamPerIf		= 1,
} dag_tx_mode_t;
/*@}*/

/**
\defgroup TxCRCrMode TX CRC Modes
Values representing tx CRC modes.
*/
/*@{*/
typedef enum
{
    kTxCrcModeInvalid           = -1,
    kTxCrcModeLeaveIntact       = 0,
    kTxCrcModeAppend            = 1,
    kTxCrcModeRemove            = 2,
    kTxCrcModeReplace           = 3
} dag_tx_crc_mode_t;
/*@}*/

/*@}*/
/**
\defgroup ExtHdrType Extension header type
Type of BFS extension header 
*/
/*@{*/
typedef enum
{
    kBFSExtHdrTypeInvalid           = -1,
    kBFSExtHdrTypePacketSignature   = 0,
    kBFSExtHdrTypeFlowId            = 1
} dag_ext_hdr_type_t;
/*@}*/


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* DAG_ATTRIBUTE_CODES_H */

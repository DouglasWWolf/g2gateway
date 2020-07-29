#pragma once
//=================================================================================================
// Date       Who  Version  Changes
// ------------------------------------------------------------------------------------------------
// 26-Jul-20  DWW    20018  Initial creation
//=================================================================================================
#define VERSION 20018

/*
 *   THINGS TO DO YET
 *
 *   We're going to need an EEPROM to save serial stuff to
 *   Need the IP address to startup static
 *   Need the root filesystem to start mounted r/o
 *
 *
 *   Add code to handle CHCP device broadcast
 *
 *   What are we going to do about the alive and busy lines?
 *   Also, need a way to transmit "alive" and "busy" from NIOS core to the ARM
 *
 *   Need a way to fetch the FPGA bitstream version
 */



/*
 *
 *  To be implemented later
 *     CHCP device broadcast
 *     CTL request for comms stats
 *     CTL request to reset the firmware
 *     CTL request for "get busy sites"
 *     CTL request reset to reload and restart the Nios II
 *
 *
 *
 *
 */

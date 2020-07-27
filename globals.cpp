//=================================================================================================
// globals.cpp - Declares global objects available to every source file
//=================================================================================================
#include "globals.h"
#include "socsubsystem.h"
#include "common.h"


// The config file manager
CUpdSpec     Config(EEPROM_DEVICE, 0x1000, "CPHD01");

// Memory map manager
CMemMap      MM(HW_REGS_BASE, HW_REGS_SPAN);

// FIFO to the Nios-II
CFpgaFifo    FIFO;

// Manages the network interface
CNetworkIF   Network;

// Thread that is responsible for sending outgoing heralds
CHeralder    Heralder;

// Thread that listens for and handles incoming CHCP message
CCHCP        CHCP;

// This holds information about this instrument such as IP address, MAC, serial number, etc
instrument_t Instrument;


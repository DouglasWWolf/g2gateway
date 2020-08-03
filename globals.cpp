//=================================================================================================
// globals.cpp - Declares global objects available to every source file
//=================================================================================================
#include <string.h>
#include "globals.h"
#include "socsubsystem.h"
#include "common.h"
#include "history.h"

// Config file manager
CUpdSpec     Config;

// The EEPROM file manager
CUpdSpec     EEPROM;

// Memory map manager
CMemMap      MM(HW_REGS_BASE, HW_REGS_SPAN);

// FIFO to the Nios-II
CFpgaFifo    CommFifo;

// Manages the network interface
CNetworkIF   Network;

// Thread that is responsible for sending outgoing heralds
CHeralder    Heralder;

// Thread that listens for and handles incoming CHCP message
CCHCP        CHCP;

// These are the servers that handle GXIP messages
CServer      Server[MAX_GXIP_SERVERS];
CServer&     MainServer = Server[0];

// Listens for and dispatches handshakes and responses from the firmware to the host
CFWListener  FWListener;

// The gateway download manager
CDLM         DLM;

// This holds information about this instrument such as IP address, MAC, serial number, etc
instrument_t Instrument;



// Utilities search for this string in our executable to find out what version it is
const char* exe_string = "EXEVERSION " VERSION_BUILD;

//=================================================================================================
// get_live_sites() - In a regular gateway, this would be a bitmap of which slots have GX modules
//                    attached to them.   Since we have a dedicated module, just say that only the
//                    first slot has a module in it
//=================================================================================================
int get_live_sites() {return 1;}
//=================================================================================================


//=================================================================================================
// globals.h - Defines global objects available to every source file
//=================================================================================================
#pragma once
#include "updspec.h"
#include "memmap.h"
#include "fpga_fifo.h"
#include "network_if.h"
#include "heralder.h"
#include "chcp.h"
#include "server.h"
#include "fwlistener.h"
#include "dlm_server.h"
#include "memmap.h"

#define MAX_GXIP_SERVERS 4

struct instrument_t
{
    int     letter;
    int     fw_major;
    int     fw_minor;
    int     fw_build;
    bool    is_dlm;
    PString net_iface;
    PString sandbox;
};


// See "globals.cpp" for a description of these objects
extern CUpdSpec     Config;
extern CUpdSpec     EEPROM;
extern CMemMap      MM;
extern CFpgaFifo    CommFifo;
extern CNetworkIF   Network;
extern CHeralder    Heralder;
extern instrument_t Instrument;
extern CCHCP        CHCP;
extern CServer      Server[MAX_GXIP_SERVERS];
extern CServer&     MainServer;
extern CFWListener  FWListener;
extern CDLM         DLM;

int     get_live_sites();

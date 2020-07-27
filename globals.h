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

#define MAX_GXIP_SERVERS 5

struct instrument_t
{
    int     letter;
    int     fw_major;
    int     fw_minor;
    int     fw_build;
};


// See "globals.cpp" for a description of these objects
extern CUpdSpec     Config;
extern CMemMap      MM;
extern CFpgaFifo    FIFO;
extern CNetworkIF   Network;
extern CHeralder    Heralder;
extern instrument_t Instrument;
extern CCHCP        CHCP;
extern CServer      Server[MAX_GXIP_SERVERS];

//=================================================================================================
// chcp_structs.h - Defines the data structures used by the Cepheid Host
//                  Configuration Protocol
//=================================================================================================
#pragma once
#include "ip_mac.h"
#include "typedefs.h"

//=================================================================================================
// These structures are all byte-packed
//=================================================================================================
#pragma pack(push, 1)



//=================================================================================================
// CHCP packet types
//=================================================================================================
#define CHCP_HERALD_ON       0       // From Host
#define CHCP_HERALD          1       // From Gateway
#define CHCP_ASSIGN_IP       2       // From Host
#define CHCP_HERALD_OFF      3       // From Host
#define CHCP_DEVICE_BCAST    4       // From Host
#define CHCP_RESET           5       // From Host
#define CHCP_PING            6       // From Host
#define CHCP_SET_MAC         7       // From Host
#define CHCP_SET_IP          8       // From Host
#define CHCP_START_DLM       9       // From Host
#define CHCP_TRASH_FIRMWARE  10      // From Host
#define CHCP_ASSIGN_LETTER   11      // From Host
#define CHCP_PING_TO         12      // From Host
#define CHCP_LAUNCH_FIRMWARE 100     // From Host
//=================================================================================================



//=================================================================================================
// Herald: Sent every two seconds at startup, or every two seconds upon receipt of a Herald On
//
// The prefix "be_" indicates this field is big-endian
//=================================================================================================
struct sCHCP_HERALD
{
    u8    type;
    sMAC  mac;
    u8    version;
    sIP   be_ip;
    u8    flags;
    u8    block_letter;
    u8    fw_major;
    u8    fw_minor;
    u8    fw_build;
    u32be be_serial_num;
    u8    family;
    u8    filler[10];
};
//=================================================================================================



//=================================================================================================
// Generic CHCP Header
//=================================================================================================
struct sCHCP_HEADER
{
    u8    type;
    sMAC  MAC;
};
//=================================================================================================



//=================================================================================================
// These are the message structures of individual CHCP messages
//=================================================================================================
struct sCHCP_PING
{
    u8   type;
    sMAC mac;
    sIP  ip;
};

struct sCHCP_PING_TO
{
    u8    type;
    sMAC  mac;
    sIP   ip;
    u16be dest_port;
};

struct sCHCP_RESET
{
    u8    type;
    sMAC  mac;
};

struct sCHCP_ASSIGN_IP
{
    u8   type;
    sMAC mac;
    sIP  ip;
};

struct sCHCP_SET_IP
{
    u8   type;
    sMAC mac;
    sIP  ip;
};


struct sCHCP_ASSIGN_LETTER
{
    u8   type;
    sMAC mac;
    u8   letter;
};

struct sCHCP_DEVICE_BCAST
{
    u8   type;
    sMAC mac;
    u8   msg_length;
    u8   msg_data[256];
};
//=================================================================================================




//=================================================================================================
// Product families
//=================================================================================================
#define FAMILY_LEGACY  0
#define FAMILY_HC      1
#define FAMILY_OMNI    2
#define FAMILY_G2      3
//=================================================================================================


//=================================================================================================
// Bit values for the herald's "flag" field
//=================================================================================================
#define FLAG_DLM      0x01
#define FLAG_CANDLM   0x02
#define FLAG_DHCP     0x04
#define FLAG_OMAP     0x08
//=================================================================================================



//=================================================================================================
// Restore structure packing to what it was before compiling this file
//=================================================================================================
#pragma pack(pop)

#pragma once
#include "typedefs.h"

//=================================================================================================
// This is the maximum number of bytes in the data section of a GXPPP frame.
//=================================================================================================
#define GXIP_PAYLOAD_SIZE   2048
//=================================================================================================



//=================================================================================================
// Possible values of the "type" field in gxip_packet_t
//=================================================================================================
enum gxip_type_t : unsigned char
{
    PRO_PKT   =  0,
    CMD_PKT   =  1,
    REQ_PKT   =  2,
    RSP_PKT   =  3,
    HSK_PKT   =  4,
    MRM_PKT   =  5,
    CTL_PKT   =  6,
    CMD_E_PKT =  9,
    REQ_E_PKT = 10,
    RSP_E_PKT = 11
};
//=================================================================================================



//=================================================================================================
// This is the structure of a gxip messsage that comes from a TCP socket
//=================================================================================================
struct gxip_packet_t
{
    unsigned char length_h, length_l;
    gxip_type_t   type;
    unsigned char payload[GXIP_PAYLOAD_SIZE];

    int length()
    {
        return (length_h << 8) | length_l;
    }

    bool is_cmd()
    {
        return (type == CMD_PKT) || (type == CMD_E_PKT);
    }

    bool is_req()
    {
        return (type == REQ_PKT) || (type == REQ_E_PKT);
    }

    bool is_rsp()
    {
        return (type == RSP_PKT) || (type == RSP_E_PKT);
    }

    bool    is_ext()
    {
        return (type == CMD_E_PKT) || (type == RSP_E_PKT);
    }

    unsigned short id()
    {
        if (type == CMD_E_PKT || type == RSP_E_PKT)
            return (payload[0] << 8) | payload[1];
        return payload[0];
    }
};
//=================================================================================================

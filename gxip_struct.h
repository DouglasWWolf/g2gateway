#pragma once

//=================================================================================================
// This is the maximum number of bytes in the data section of a GXPPP frame.
//=================================================================================================
#define GXIP_PAYLOAD_SIZE   2048
//=================================================================================================


//=================================================================================================
// This is the structure of a gxip messsage that comes from a TCP socket
//=================================================================================================
struct gxip_packet_t
{
    unsigned char length_h, length_l;
    unsigned char type;
    unsigned char payload[GXIP_PAYLOAD_SIZE];
};
//=================================================================================================

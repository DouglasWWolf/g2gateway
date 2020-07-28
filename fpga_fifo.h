//=================================================================================================
// fpga_fifo.h - Defines a bidirectional 32-bit wide FIFO to the NIOS-II core
//=================================================================================================
#pragma once
#include "memmap.h"



class CFpgaFifo
{
public:

    // Constructor & Destructor.
    CFpgaFifo();
    ~CFpgaFifo();

    // Pass an already open CMemMap object
    bool    init(CMemMap& mm);

    // Call this to send a character string to the other side
    void    send_string(const char* ptr);

    // Call this to determine whether there is an incoming message waiting
    bool    is_message_waiting();

    // Reads a message from the FIFO and fills in msg_length, msg_type, and payload
    bool    read_message();

    // These are all filled in after a successful call to "read_message"
    int     msg_length;
    int     msg_type;
    char    payload[1024];

protected:

    // Sends a message of an arbitrary type
    void    send_generic(int type, int byte_count, const void* buffer);


    // Our private variables
    void*   m_pv;

};

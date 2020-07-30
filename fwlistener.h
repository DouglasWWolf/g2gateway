//=================================================================================================
// fwlistener.h- Defines a thread that listens for messages from the firmware
//=================================================================================================
#pragma once
#include "cthread.h"
#include "gxip_struct.h"

//=================================================================================================
// CFWListener - Listens for messages from the firmware and sends them back to the host
//=================================================================================================
class CFWListener : public CThread
{
public:

    // Constructor
    CFWListener();

    // When this thread starts up, the entry point is here
    void    main(void* p1, void* p2, void* p3);

    // Called by other threads to send a message to he firmware and notify the listener
    // to expect a response
    bool    transact(gxip_packet_t& message, bool discard_ack = false);

protected:

    // These are from the outgoing message of the most recent transaction
    gxip_type_t   m_outgoing_msg_type;
    u32           m_outgoing_msg_id;

    // Other threads can send us messages by writing to this pipe
    int           m_pipe[2];

    // This will be true when we're waiting for and processing a message from the GX
    bool          m_is_active;
};
//=================================================================================================

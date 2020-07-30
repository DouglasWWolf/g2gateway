//=================================================================================================
// fwlistener.cpp- Implements a thread that listens for messages from the firmware
//=================================================================================================
#include <unistd.h>
#include "fwlistener.h"
#include "globals.h"

#define FWL_HSK         0x01
#define FWL_RSP         0x02
#define FWL_DISCARD_HSK 0x80


// Map a GXIP packet onto the messages that will be received by the CommFifo
gxip_packet_t& response = *(gxip_packet_t*) CommFifo.payload;


//=================================================================================================
// This is how long (in milliseconds) we'll wait around for handshake from the firmware
//=================================================================================================
#define GXPPP_HSK_TIMEOUT 5000

//=================================================================================================
// This is how long (in milliseconds) we'll wait around for response message from the firmware
//=================================================================================================
#define GXPPP_RSP_TIMEOUT 15000


//=================================================================================================
// is_firmware_busy() - Return 'true' if the firmware has asserted its "busy" signal
//=================================================================================================
static bool is_firmware_busy() {return false;}
//=================================================================================================


//=================================================================================================
// send_nak_handshake_to_host() - Sends a NAK GXIP handshake back to the client
//=================================================================================================
void send_nak_handshake_to_host()
{
    // Build a GXIP 'NAK" handshake
    static u8 handshake[4] = {0, 4, HSK_PKT, 'N'};

    // And send it back to the host
    MainServer.send_gxip_to_host(*(gxip_packet_t*)handshake);
}
//=================================================================================================


//=================================================================================================
// send_busy_handshake_to_host() - Sends a BUSY GXIP handshake back to the client
//=================================================================================================
void send_busy_handshake_to_host()
{
    // Build a GXIP "Busy" handshake
    static u8 handshake[4] = {0, 4, HSK_PKT, 'B'};

    // And send it back to the host
    MainServer.send_gxip_to_host(*(gxip_packet_t*)handshake);
}
//=================================================================================================


//=================================================================================================
// send_mrm_to_host() - Sends a "missing response message" packet to the host in lieu of a response
//=================================================================================================
void send_mrm_to_host(int msg_type, int msg_id)
{
    // This will serve as a GXIP "Missing Response Message"
    u8 buffer[5];

    // Fill in the length
    buffer[0] = 0;
    buffer[1] = 5;

    // Fill in the packet type
    buffer[2] = MRM_PKT;

    // Fill in the last request id
    buffer[3] = msg_id;

    // Fill in the last request type
    buffer[4] = msg_type;

    // And send it back to the host
    MainServer.send_gxip_to_host(*(gxip_packet_t*)buffer);
}
//=================================================================================================


//=================================================================================================
// Constructor() - When this is done, the listener is read to receive commands
//=================================================================================================
CFWListener::CFWListener()
{
    // We're not currently active
    m_is_active = false;

    // And create the command pipe
    pipe(m_pipe);
}
//=================================================================================================



//=================================================================================================
// main() - When this thread spawns, execution starts here
//=================================================================================================
void CFWListener::main(void* p1, void* p2, void* p3)
{
    char    cmd;

again:

    // We're not currently waiting for a message from the GX
    m_is_active = false;

    // Wait for a message to arrive on our command pipe
    while (read(m_pipe[0], &cmd, 1) != 1);

    // We're waiting for a response message from the GX
    m_is_active = true;

    // If the top bit of the command is set, it means we're supposed to
    // ignore any handshake we receive
    bool discard_handshake = (cmd & FWL_DISCARD_HSK) != 0;

    // If we should be waiting for a handshake from the firmware...
    while (cmd & FWL_HSK)
    {
        // Wait for messages from the firmware to arrive...
        while (!CommFifo.read_message(GXPPP_HSK_TIMEOUT))
        {
            // If we didn't receive a handshake message from the firmware because it's busy,
            // send a "busy" handshake to the host, and keep waiting for a handshake
            if (is_firmware_busy())
            {
                send_busy_handshake_to_host();
                continue;
            }

            // Otherwise, tell the host that firmware never acknowledged the receipt of the message
            send_nak_handshake_to_host();
            return;
        }

        // Ignore any message that's not a handshake packet
        if (response.type != HSK_PKT) continue;

        // If we're supposed to pass this ACK on to the host, do so
        if (!discard_handshake) MainServer.send_gxip_to_host(response);

        // We're done waiting for a handshake message
        break;
    }


    // If we should be waiting for a response message from the firmware...
    while (cmd & FWL_RSP)
    {
        while (!CommFifo.read_message(GXPPP_RSP_TIMEOUT))
        {
            // If we didn't receive a handshake message from the firmware because it's busy,
            // send a "busy" handshake to the host, and keep waiting for a handshake
            if (is_firmware_busy())
            {
                send_busy_handshake_to_host();
                continue;
            }

            // The firmware failed to send an expected response.  In lieu of a response,
            // send the client an MRM (Missing Response Message)
            send_mrm_to_host(m_outgoing_msg_type, m_outgoing_msg_id);
            break;
        }

        // If this isn't a response message, ignore it
        if (!response.is_rsp()) continue;

        // We got a response from the firmware.  Send it to the host
        MainServer.send_gxip_to_host(response);

        // We're done waiting for a response message
        break;
    }


    // And go wait to be told to start listening for another message from the firmware
    goto again;
}
//=================================================================================================



//=================================================================================================
// transact() - Begins a transaction with the GX firmware.
//
// A transaction consists of:
//
//  (1) Message send to the firmware
//  (2) Firmware sends a handshake
//  (3) Firmware optionally sends a response
//=================================================================================================
bool CFWListener::transact(gxip_packet_t& message, bool discard_ack)
{
    // We can't start a transaction in the previous one is still active
    if (m_is_active) return false;

    // Save the type and ID of this outgoing message
    m_outgoing_msg_type = message.type;
    m_outgoing_msg_id   = message.id();

    // Send the outgoing message to the firmware
    CommFifo.send_gxip(message);

    // The listener is going to wait for a handshake message
    u8 cmd = FWL_HSK;

    // If we're supposed to discard the handshake, set the command flag accordingly
    if (discard_ack) cmd |= FWL_DISCARD_HSK;

    // If we just send a request message, the listener also needs to wait for a response
    if (message.is_req()) cmd |= FWL_RSP;

    // Send this command to the "listener" thread
    write(m_pipe[1], &cmd, 1);

    // And tell the caller that his transaction has been started
    return true;
}
//=================================================================================================


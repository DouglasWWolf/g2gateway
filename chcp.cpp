//=================================================================================================
// chcp.cpp - Implements a CHCP listener
//=================================================================================================
#include <unistd.h>
#include <string.h>
#include "chcp.h"
#include "typedefs.h"
#include "udpsocket.h"
#include "globals.h"
#include "common.h"

//=================================================================================================
// This IP and MAC address are all zeros
//=================================================================================================
static sIP  broadcast_ip;
static sMAC broadcast_mac;
//=================================================================================================


//=================================================================================================
// reset_server_connections() - Causes all servers to drop any existing connection and go back
//                              to listening for new ones
//=================================================================================================
static void reset_server_connections()
{
    // Reset the connection for the download manager
    DLM.reset_connection();

    // Ask all of the servers to drop any connection they happen to have open
    for (int i=0; i<MAX_GXIP_SERVERS; ++i) Server[i].reset_connection();

    // Give the servers a second to come back up
    sleep(1);
}
//=================================================================================================



//=================================================================================================
// main() - Sits in a loop, listening for and handling incoming CHCP messages
//=================================================================================================
void CCHCP::main(void* p1, void* p2, void* p3)
{
    u8        message[300];
    UDPSocket sock;
    u32       source_ip;

    // Map all of our message types over the message buffer
    sCHCP_HEADER        &header            = *(sCHCP_HEADER        *)&message;
    sCHCP_PING          &msg_ping          = *(sCHCP_PING          *)&message;
    sCHCP_PING_TO       &msg_ping_to       = *(sCHCP_PING_TO       *)&message;
    sCHCP_ASSIGN_IP     &msg_assign_ip     = *(sCHCP_ASSIGN_IP     *)&message;
    sCHCP_SET_IP        &msg_set_ip        = *(sCHCP_SET_IP        *)&message;
    sCHCP_ASSIGN_LETTER &msg_assign_letter = *(sCHCP_ASSIGN_LETTER *)&message;
    sCHCP_DEVICE_BCAST  &msg_device_bcast  = *(sCHCP_DEVICE_BCAST  *)&message;

    // CHCP clients will send us CHCP messages on port 1216
    sock.create_listener(1216);

again:

    // Fetch a UDP packet
    sock.get(message, sizeof(message), &source_ip);

    // If this CHCP message isn't intended for us, ignore it
    if (header.MAC != broadcast_mac && header.MAC != Network.mac()) goto again;

    // We've not yet handled this CHCP message
    bool is_handled = false;

    // Process CHCP messages that both the DLM and the gateway can handle
    switch (header.type)
    {
        case CHCP_HERALD_ON:
            Heralder.start();
            is_handled = true;
            break;

        case CHCP_HERALD_OFF:
            Heralder.stop();
            is_handled = true;
            break;

        case CHCP_PING:
            handle_chcp_ping(msg_ping.ip);
            is_handled = true;
            break;

        case CHCP_PING_TO:
            handle_chcp_ping_to(msg_ping_to.ip, msg_ping_to.dest_port);
            is_handled = true;
            break;

        case CHCP_RESET:
            handle_chcp_reset();
            is_handled = true;
            break;

        case CHCP_ASSIGN_IP:
            handle_chcp_assign_ip(msg_assign_ip.ip);
            is_handled = true;
            break;

        case CHCP_DEVICE_BCAST:
            handle_chcp_device_bcast(msg_device_bcast.msg_length, msg_device_bcast.msg_data);
            is_handled = true;
            break;

        case CHCP_START_DLM:
            Instrument.is_dlm = true;
            Heralder.build_herald();
            is_handled = true;
            break;

        case CHCP_LAUNCH_FIRMWARE:
            if (Instrument.is_dlm) ::exit(0);
            is_handled = true;
            break;

    }

    // If we've handled this CHCP message, go wait for the next one
    if (is_handled) goto again;

    // Process CHCP messages that only the gateway (and not the DLM) can handle
    switch (header.type)
    {
        case CHCP_SET_IP:
            handle_chcp_set_ip(msg_set_ip.ip);
            break;

        case CHCP_ASSIGN_LETTER:
            handle_chcp_assign_letter(msg_assign_letter.letter);
            break;

        // This message is obsolete, so we won't handle it
        case CHCP_TRASH_FIRMWARE:
            break;

        // We're not going to handle this message because we want
        // to use the MAC address that is built into our hardware
        case CHCP_SET_MAC:
            break;

        default:
            printf("Unknown CHCP command %i\n", header.type);
            break;
    }

    // And go wait for the next CHCP message to arrive
    goto again;

}
//=================================================================================================


//=================================================================================================
// handle_chcp_ping() - If we have the IP specified in the message, send a herald in response
//=================================================================================================
void CCHCP::handle_chcp_ping(sIP ip)
{
    if (ip == broadcast_ip || ip == Network.ip())
    {
        Heralder.send();
    }
}
//=================================================================================================


//=================================================================================================
// handle_chcp_ping_to() - Like chcp_ping, but sends the herald to a different port
//=================================================================================================
void CCHCP::handle_chcp_ping_to(sIP ip, int dest_port)
{
    if (ip == broadcast_ip || ip == Network.ip())
    {
        Heralder.send(dest_port);
    }
}
//=================================================================================================


//=================================================================================================
// handle_chcp_reset() Tells all of the servers to close their connections
//=================================================================================================
void CCHCP::handle_chcp_reset()
{
    reset_server_connections();
}
//=================================================================================================



//=================================================================================================
// handle_chcp_assign_ip() - Changes our IP address
//=================================================================================================
void CCHCP::handle_chcp_assign_ip(sIP ip)
{
    // If this is already our IP address, do nothing
    if (ip == Network.ip()) return;

    // Make this our new IP address
    Network.set_ip_address(ip);

    // Reset all of the client socket connections
    reset_server_connections();

    // Bind our outgoing herald socket to our new IP address
    Heralder.create_new_socket();

    // Stuff the new IP into our herald
    Heralder.build_herald();

    // And send a single herald as an acknowledgement
    Heralder.send();
}
//=================================================================================================


//=================================================================================================
// handle_chcp_assign_letter() - Changes our letter (A, B, C, etc) in our herald
//=================================================================================================
void CCHCP::handle_chcp_assign_letter(char letter)
{
    // Keep track of the letter that we've been assigned
    Instrument.letter = letter;

    // Stuff the new letter into our herald
    Heralder.build_herald();

    // And send a single herald as an acknowledgement
    Heralder.send();
}
//=================================================================================================




//=================================================================================================
// handle_chcp_set_ip() - Changes our IP address permanently
//=================================================================================================
void CCHCP::handle_chcp_set_ip(sIP ip)
{
    // Tell the network interface to use this IP address
    handle_chcp_assign_ip(ip);

    // Save this to our settings
    EEPROM.set(SPEC_DEFAULT_IP, ip.to_string());

    // And save our settings to disk/EEPROM
    EEPROM.save();
}
//=================================================================================================


//=================================================================================================
// handle_chcp_device_bcast() - Accepts a GXIP command (without the GXIP header), constructs
//                              the appropriate header, and sends the command to the firmware
//=================================================================================================
void CCHCP::handle_chcp_device_bcast(u8 command_length, u8* command)
{
    // Map a GXIP packet in place over the original UDP datagram
    gxip_packet_t& gxip = *(gxip_packet_t*)(command - 3);

    // Build the GXIP header for the message we're about to send
    gxip.set_length(command_length+3);
    gxip.type = CMD_PKT;

    // Perform this transaction, ignoring the resulting ACK from the firmware
    FWListener.transact(gxip, true);
}
//=================================================================================================


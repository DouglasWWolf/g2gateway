//=================================================================================================
// chcp.cpp - Implements a CHCP listener
//=================================================================================================
#include <string.h>
#include "chcp.h"
#include "typedefs.h"
#include "udpsocket.h"
#include "globals.h"

//=================================================================================================
// main() - Sits in a loop, listening for and handling incoming CHCP messages
//=================================================================================================
void CCHCP::main(void* p1, void* p2, void* p3)
{
    u8        message[300];
    UDPSocket sock;
    u32       source_ip;
    sMAC      broadcast_mac;

    // Map all of our message types over the message buffer
    sCHCP_HEADER        & header           = *(sCHCP_HEADER        *)&message;

    // Create a MAC address object that CHCP uses to say "this is for everyone who can hear me"
    memset(&broadcast_mac, 0, sizeof broadcast_mac);

    // CHCP clients will send us CHCP messages on port 1216
    sock.create_listener(1216);

again:

    // Fetch a UDP packet
    sock.get(message, sizeof(message), &source_ip);

    // If this CHCP message isn't intended for us, ignore it
    if (header.MAC != broadcast_mac && header.MAC != Network.mac()) goto again;

    // Process CHCP messages that both the DLM and the gateway can handle
    switch (header.type)
    {
        case CHCP_HERALD_ON:
            Heralder.start();
            goto again;
            break;

        case CHCP_HERALD_OFF:
            Heralder.stop();
            goto again;
            break;
    }

    // And go wait for the next CHCP message to arrive
    goto again;

}
//=================================================================================================


#if 0
U8        message[300];
UDPSocket sock;
U32       source_ip;

// Map all of our message types over the message buffer
sCHCP_HEADER        & header           = *(sCHCP_HEADER        *)&message;
sCHCP_PING          & msg_ping         = *(sCHCP_PING          *)&message;
sCHCP_PING_TO       & msg_ping_to      = *(sCHCP_PING_TO       *)&message;
sCHCP_ASSIGN_IP     & msg_assignIP     = *(sCHCP_ASSIGN_IP     *)&message;
sCHCP_SET_IP        & msg_setIP        = *(sCHCP_SET_IP        *)&message;
sCHCP_SET_MAC       & msg_setMAC       = *(sCHCP_SET_MAC       *)&message;
sCHCP_ASSIGN_LETTER & msg_assignLetter = *(sCHCP_ASSIGN_LETTER *)&message;
sCHCP_TRASH_FIRMWARE& msg_trashFW      = *(sCHCP_TRASH_FIRMWARE*)&message;
sCHCP_DEVICE_BCAST  & msg_device_bcast = *(sCHCP_DEVICE_BCAST  *)&message;


// Start CHCP packets are sent to UDP port 1216
sock.CreateListener(1216);

Again:

// Fetch a UDP packet
sock.Get(message, sizeof(message), &source_ip);

// If we're supposed to filter out messages from this network, then
// ignore this UDP packet
if ((ntohl(source_ip) & FilterMask) != FilterIP) goto Again;

// If this CHCP message isn't intended for us, ignore it
if (header.MAC != BroadcastMAC && header.MAC != CurrentMAC) goto Again;

// Process CHCP messages that both the DLM and the gateway can handle
switch (header.type)
{
    case CHCP_HERALD_ON:
        Heralder.Start();
        goto Again;
        break;

    case CHCP_HERALD_OFF:
        Heralder.Stop();
        goto Again;
        break;

    case CHCP_PING:
        Handle_CHCP_Ping(msg_ping);
        goto Again;
        break;

    case CHCP_PING_TO:
        Handle_CHCP_Ping_To(msg_ping_to);
        goto Again;
        break;

    case CHCP_ASSIGN_IP:
        Handle_CHCP_AssignIP(msg_assignIP);
        goto Again;
        break;

    case CHCP_RESET:
        ResetConnections();
        goto Again;
        break;

    case CHCP_DEVICE_BCAST:
        Handle_CHCP_DeviceBCast(msg_device_bcast);
        goto Again;
        break;
}


// Process CHCP messages that the Gateway can handle
switch (header.type)
{
    case CHCP_SET_IP:
        Handle_CHCP_SetIP(msg_setIP);
        break;

    case CHCP_SET_MAC:
        Handle_CHCP_SetMAC(msg_setMAC);
        break;

    case CHCP_ASSIGN_LETTER:
        Handle_CHCP_AssignLetter(msg_assignLetter);
        break;

    case CHCP_TRASH_FIRMWARE:
        Handle_CHCP_TrashFW(msg_trashFW);
        break;

    default:
        printf("Unknown CHCP command %i\n", header.type);
        break;
}

// And go listen for the next incoming CHCP message
goto Again;
}
//=============================================================================
#endif

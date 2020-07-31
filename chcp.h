//=================================================================================================
// chcp.h - Defines a CHCP listener
//=================================================================================================
#pragma once
#include "cthread.h"
#include "ip_mac.h"

class CCHCP : public CThread
{
public:

    // When this thread spawns, this is the entry point
    void  main(void* p1, void* p2, void* p3);

    // CHCP message handlers
    void handle_chcp_ping         (sIP ip);
    void handle_chcp_ping_to      (sIP ip, int port);
    void handle_chcp_reset        ();
    void handle_chcp_assign_ip    (sIP ip);
    void handle_chcp_set_ip       (sIP ip);
    void handle_chcp_assign_letter(char letter);
    void handle_chcp_device_bcast (u8 command_length, u8* command);
};

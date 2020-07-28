//=================================================================================================
// chcp.h - Defines a CHCP listener
//=================================================================================================
#pragma once
#include "cthread.h"
#include "chcp_structs.h"

class CCHCP : public CThread
{
public:

    // When this thread spawns, this is the entry ppoint
    void  main(void* p1, void* p2, void* p3);

    // CHCP message handlers
    void handle_chcp_ping     (sCHCP_PING      & msg);
    void handle_chcp_ping_to  (sCHCP_PING_TO   & msg);
    void handle_chcp_reset    (sCHCP_RESET     & msg);
    void handle_chcp_assign_ip(sCHCP_ASSIGN_IP & msg);
};

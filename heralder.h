//=================================================================================================
// heralder.h- Defines thread that is responsible for transmitting periodic "Herald" packets.
//=================================================================================================
#pragma once
#include "udpsocket.h"
#include "cthread.h"
#include "network_if.h"
#include "chcp_structs.h"


//=================================================================================================
// CHeralder - This object thread will be responsible for sending periodic CHCP heralds
//=================================================================================================
class CHeralder : public CThread
{
public:
                     CHeralder()
                     {
                         m_is_heralding = true;
                         m_is_silent    = false;
                         m_p_socket     = nullptr;
                     }

    void             main(void* p1, void* p2, void* p3);

    void             create_new_socket();

    void             start() {m_is_heralding = true;}
    void             stop()  {m_is_heralding = false;}
    void             silent(bool state = true) {m_is_silent = state;}
    void             send(int port = 0);

    // These are called by other threads to change the contents of the herald
    void             build_herald     ();
    void             set_herald_mac   ();
    void             set_herald_sn   ();
    void             set_herald_ip    ();
    void             set_herald_letter();

protected:

    bool             m_is_heralding;
    bool             m_is_silent;
    PCriticalSection m_heralding_cs;
    PCriticalSection m_herald_cs;
    UDPSocket*       m_p_socket;
    sCHCP_HERALD     m_herald;
};
//=================================================================================================


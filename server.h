//=================================================================================================
// server.h- Defines threads that manage our TCP connections to the outside world
//=================================================================================================
#pragma once
#include "cthread.h"
#include "netsock.h"


//=================================================================================================
// This is the maximum number of bytes in the data section of a GXPPP frame.
//=================================================================================================
#define PACKET_PAYLOAD_SIZE   2048
//=================================================================================================


//=================================================================================================
// This is the structure of a messsage that comes from a TCP socket
//=================================================================================================
struct tcp_packet_t
{
    unsigned char length_h, length_l;
    unsigned char type;
    unsigned char payload[PACKET_PAYLOAD_SIZE];
};
//=================================================================================================


//=================================================================================================
// CServer - Each CServer object manages one TCP connection
//=================================================================================================
class CServer : public CThread
{
public:

    // Constructor
    CServer();

    // When a
    void    main(void* p1, void* p2, void* p3);

    // Set the slot number (-1 or 0 thru 3) for this server
    void    set_slot(int slot);

    // Call this to find out if the server thread is initialized
    bool    is_initialized() {return m_is_initialized;}

    // Call this to force the server to drop an incoming connection
    void    reset_connection();

protected:

    // This reads a GXIP message from the socket into m_tcp_packet
    bool         read_gxip_msg_from_socket(int fd);

    // -1 (for the gateway master port) or 0 thru 3 (for ordinary module connections)
    int          m_slot;

    // This is the TCP port we're listening to
    int          m_tcp_port;

    // Other threads can send us messages by writing to this pipe
    int          m_special_pipe[2];

    // The main thread will check this after we spawn to see if we're ready to go
    bool         m_is_initialized;

    // This will be true when we think there is a client connected to us
    bool         m_is_connected;

    // This is the server socket that people connect to us on
    CNetSock     m_socket;


    // This is a message from the socket
    tcp_packet_t m_tcp_packet;
};
//=================================================================================================

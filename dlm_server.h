//=================================================================================================
// dlm_server.h - Defines a server that acts as a download manager for gateway software updates
//=================================================================================================
#pragma once
#include "cthread.h"
#include "netsock.h"
#include "gxip_struct.h"
#include "cppstring.h"

//=================================================================================================
// CDLM - Download Manager
//=================================================================================================
class CDLM : public CThread
{
public:

    // Constructor
    CDLM();

    // When this thread spawns, this is the entry point
    void        main(void* p1, void* p2, void* p3);

    // Call this to find out if the server thread is initialized
    bool        is_initialized() {return m_is_initialized;}

    // Call this to force the server to drop an incoming connection
    void        reset_connection();

protected:

    // This reads a DLM message from the socket into m_tcp_packet
    bool          read_dlm_msg_from_socket();

    // Call this to send a response to the most recently received message
    void          send_response(const unsigned char* p, int length);

    // Message handlers
    bool          handle_dlm_flash_init();
    bool          handle_dlm_flash_write();
    bool          handle_dlm_flash_commit();



    // This is the TCP port we're listening to
    int           m_tcp_port;

    // Other threads can send us messages by writing to this pipe
    int           m_special_pipe[2];

    // The main thread will check this after we spawn to see if we're ready to go
    bool          m_is_initialized;

    // This will be true when we think there is a client connected to us
    bool          m_is_connected;

    // This is the server socket that people connect to us on
    CNetSock      m_socket;

    // This will pointer to a buffer on the heap that contains incoming DLM message
    u8*           m_message;

    // This will hold the filename of the file we're downloading
    PString        m_filename;

    // Pointer to an ordinary C/C++ FILE structure
    FILE*         m_ofile;
};
//=================================================================================================

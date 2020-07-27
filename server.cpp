//=================================================================================================
// server.cpp -Implements threads that manage our TCP connections to the outside world
//=================================================================================================
#include <unistd.h>
#include <sys/ioctl.h>
#include "server.h"
#include "typedefs.h"

//=================================================================================================
// These are the commands that can be sent to the server via the special command pipe
//=================================================================================================
#define SPECIAL_CLOSE 1
//=================================================================================================

//=================================================================================================
// bytes_availabe() - Returns the number of bytes available to be read on a file-descriptor
//=================================================================================================
static int bytes_available(int fd)
{
    int count;
    ioctl(fd, FIONREAD, &count);
    return count;
}
//=================================================================================================



//=================================================================================================
// drain_fd() - Drains (and throws away) all bytes available for reading on a file descriptor
//=================================================================================================
static void drain_fd(int fd)
{
    int  count;
    char buffer[100];

    while ((count = bytes_available(fd)))
    {
        if (count > sizeof buffer) count = sizeof buffer;
        read(fd, buffer, count);
    }
}
//=================================================================================================



//=================================================================================================
// Constructor() - Make sure we start in a known state
//=================================================================================================
CServer::CServer()
{
    // When we start, we are neither initialized, nor connected to a client
    m_is_connected   = false;
    m_is_initialized = false;
}
//=================================================================================================


//=================================================================================================
// read_gxip_msg_from_socket() - Fetches a GXIP message from the socket
//=================================================================================================
bool CServer::read_gxip_msg_from_socket(int sd)
{
    char* remaining_packet = ((char*)&m_tcp_packet)+2;
    int   remaining_size   = sizeof(m_tcp_packet) - 2;

    // Read the first two bytes of the message, it's the msg length
    if (read(sd, &m_tcp_packet, 2) < 2) return false;

    // Find out how long entire message is (including the two length bytes)
    int msg_length = (m_tcp_packet.length_h << 8) | m_tcp_packet.length_l;

    // This is how many more bytes we should find in this message
    int bytes_expected = msg_length - 2;

    // If this can't possibly be a valid length, close the socket
    if (bytes_expected < 1 || bytes_expected > remaining_size) return false;

    // Read in the rest of the message
    int bytes_read = read(sd, remaining_packet, bytes_expected);

    // Tell the caller whether we read an entire message
    return (bytes_read == bytes_expected);
}
//=================================================================================================


//=================================================================================================
// set_slot() - Gives the server information to understand which GX module slot (if any) it is
//
// Passed: slot = -1 (for the dedicated gateway server) or 0 thru 3 (for the GX servers)
//
// On Exit: m_slot     = the saved slot number
//          m_tcp_port = the TCP port that this server should listen on
//=================================================================================================
void CServer::set_slot(int slot)
{
    // Find out what slot number we're in
    m_slot = slot;

    // Determine which TCP port this server will be listening on
    m_tcp_port = (slot == -1) ? 1066 :m_slot + 921;
}
//=================================================================================================


//=================================================================================================
// main() - When this thread spawns, execution starts here
//=================================================================================================
void CServer::main(void* p1, void* p2, void* p3)
{
    fd_set  rfds;
    char    special_cmd;

    // Other threads send us messages by writing to this pipe
    pipe(m_special_pipe);

    // Get a convenient name for our side of this pipe
    int special_fd = m_special_pipe[0];

    // Map a message-header structure onto our message buffer
    u16be& msg_length = *(u16be*)&m_tcp_packet;
    u8   & msg_type   = *(u8   *)&m_tcp_packet.type;
    u8   & msg_id     = *(u8   *)&m_tcp_packet.payload[0];

    // Tell the outside world that we are initialized
    m_is_initialized = true;

wait_for_connect:

    // There is not yet a client connected to our socket
    m_is_connected = false;

    // Tell the world what's up
    printf("Waiting for connection on port %i\n", m_tcp_port);

    // Create the server socket
    if (!m_socket.create_server(m_tcp_port))
    {
        printf("FAILED TO CREATE SERVER ON PORT %i\n", m_tcp_port);
    }

    // Wait for a connection from the outside world
    if (!m_socket.accept())
    {
        printf("FAILED TO ACCEPT CONNECTIONS ON PORT %i\n", m_tcp_port);
    }

    // There is now a client connected to our socket
    m_is_connected = true;

    // Display a message to the console
    printf("Client connected to Port %i\n", m_tcp_port);

    // Make sure there are no leftover commands waiting in the command pipe
    drain_fd(special_fd);

    // Turn off nagling on the socket so that data is not buffered after we send it
    m_socket.set_nagling(false);

    // Figure out what the largest file descriptor is
    int sd = m_socket.get_fd();
    int max_fd = 0;
    if (max_fd < sd        ) max_fd = sd;
    if (max_fd < special_fd) max_fd = special_fd;

WaitForData:

    // We want to wake up if any data appears on the socket or on the pipe
    FD_ZERO(&rfds);
    FD_SET(sd,         &rfds);
    FD_SET(special_fd, &rfds);

    // Wait for data to arrive on one of our file descriptors
    select(max_fd+1, &rfds, NULL, NULL, NULL);

    // If a special command has arrived from another thread...
    if (FD_ISSET(special_fd, &rfds))
    {
        // Find out what the command is
        read(special_fd, &special_cmd, 1);

        // If we're being told to forcibly close the socket connection, do so
        if (special_cmd == SPECIAL_CLOSE)
        {
            // Display a message to the console
            printf("Port %i closed by CHCP_RESET\n", m_tcp_port);

            // We're no longer connected
            m_is_connected = false;

            // Close the socket
            m_socket.close();

            // And go wait for another connection
            goto wait_for_connect;
        }
    }

    // If data arrived on the socket...
    if (FD_ISSET(sd, &rfds))
    {
        // Read in an entire GXIP message from the socket into m_tcp_packet;
        if (!read_gxip_msg_from_socket(sd))
        {
            m_socket.close();
            printf("Port %i closed by client\n", m_tcp_port);
            goto wait_for_connect;
        }

#if 0
        // Decide what to do with the packet
        switch (msg_type)
        {
            case PRO_PKT:
                protocolRequest();
                goto WaitForData;
                break;

            case CTL_PKT:
                HandleControlRequest(msg_id);
                goto WaitForData;
                break;

            case CMD_PKT:
            case REQ_PKT:
            case CMD_E_PKT:
            case REQ_E_PKT:
                HandleCmdReqMsg();
                goto WaitForData;
        }
#endif
        // If we get an unknown message type, display it
        printf("Rcvd message type %u: ID = %u, Length = %u\n",
                msg_type, msg_id, (unsigned)msg_length);
    }
    // Go back and wait for more data
    goto WaitForData;
}
//=================================================================================================


#if 0
//=============================================================================
// Main() - This is the routine that begins executing when the thread spawns
//=============================================================================
void CServer::Main(void* P1, void* P2, void *P3)
{
    fd_set  rfds;
    char    special_cmd;

    // We're not yet connected to a GX
    m_gx_connected = false;

    // Initialize the transaction ID to a known value
    m_transID = 0;

    // So far, there have been no unusual handshakes sent or received
    m_CANs_in = m_CANs_out = m_NAKs_in = m_NAKs_out = 0;

    // Initialize the HCM client
    m_HCM.Initialize(m_slot, CC_SRC_GATEWAY);

    // Find the name of the serial-device associated with this lot
    const char* device = tty_name[m_slot];

    // If this socket should talk to one of the GeneXpert devices...
    if (m_slot >= 0)
    {
        // Open a connection to the serial port the GX is connected to
        m_gx_connected = m_SP.Open(device, 57600);

        // And complain if the serial port isn't accessable
        if (!m_gx_connected) printf("Failed to open \"%s\"\n", device);


        // The serial port listener sends us data from the serial port on this pipe
        pipe(m_sp_read_pipe);

        // Other threads send us messages by writing to this pipe
        pipe(m_special_pipe);

        // Start up the serial-port reader
        m_SPReader.Spawn
        (
            (void*) m_slot,
            (void*) m_SP.GetFD(),
            (void*) m_sp_read_pipe[1]
        );

        // Listen for events that should be forwarded to the GX module
        m_EventReader.Spawn((void*)m_slot);
    }

    // Get a convenient name for our side of this pipe
    int specialFD = m_special_pipe[0];

    // Map a message-header structure onto our message buffer
    U16BE& msg_length = *(U16BE*)&m_tcp_packet;
    U8   & msg_type   = *(U8   *)&m_tcp_packet.type;
    U8   & msg_id     = *(U8   *)&m_tcp_packet.Data[0];

    // Tell the outside world that we are initialized;
    m_bInitialized = true;

WaitForConnect:

    // Tell the world what's up
    printf("Waiting for connection on port %i\n", m_tcp_port);

    // There is not yet a client connected to our socket
    m_bConnected = false;

    // Create the server socket
    if (!m_socket.CreateServer(m_tcp_port))
    {
        printf("FAILED TO CREATE SERVER ON PORT %i\n", m_tcp_port);
    }

    // Wait for a connection from the outside world
    if (!m_socket.Accept())
    {
        printf("FAILED TO ACCEPT CONNECTIONS ON PORT %i\n", m_tcp_port);
    }

    // There is now a client connected to our socket
    m_bConnected = true;

    // Display a message to the console
    printf("Client connected to Port %i\n", m_tcp_port);

    // Get rid of any spurious bytes that are sitting in the special pipe
    DrainFD(specialFD);

    // We want all socket data transmitted immediately
    m_socket.SetNagling(false);

    // Find out what the file descriptor of the socket is
    int sd = m_socket.GetFD();

    // Figure out what the largest file descriptor is
    int max_fd = 0;
    if (max_fd < sd       ) max_fd = sd;
    if (max_fd < specialFD) max_fd = specialFD;

WaitForData:

    // We want to wake up if any data appears on the socket or on the pipe
    FD_ZERO(&rfds);
    FD_SET(sd,        &rfds);
    FD_SET(specialFD, &rfds);

    // Wait for data to arrive on one of our file descriptors
    select(max_fd+1, &rfds, NULL, NULL, NULL);

    // If a special command has arrived from another thread...
    if (FD_ISSET(specialFD, &rfds))
    {
        // Find out what the command is
        read(specialFD, &special_cmd, 1);

        if (special_cmd == SPECIAL_CLOSE)
        {
            // Close the data socket
            Handle_SpecialClose();

            // And go wait for another connection
            goto WaitForConnect;
        }
    }

    // If data arrived on the socket...
    if (FD_ISSET(sd, &rfds))
    {
        // Read the first two bytes of the message, it's the msg length
        if (read(sd, &m_tcp_packet, 2) < 2)
        {
            m_socket.Close();
            printf("Port %i closed by client\n", m_tcp_port);
            goto WaitForConnect;
        }

        // Read in the rest of the message
        read(sd, &m_tcp_packet.type, msg_length - 2);

        // Decide what to do with the packet
        switch (msg_type)
        {
            case PRO_PKT:
                protocolRequest();
                goto WaitForData;
                break;

            case CTL_PKT:
                HandleControlRequest(msg_id);
                goto WaitForData;
                break;

            case CMD_PKT:
            case REQ_PKT:
            case CMD_E_PKT:
            case REQ_E_PKT:
                HandleCmdReqMsg();
                goto WaitForData;

            case CAM_PKT:
                HandleCameraRequest(msg_id, msg_length - 4);
                goto WaitForData;
        }

        // If we get an unknown message type, display it
        printf("Rcvd message type %u: ID = %u, Length = %u\n",
                msg_type, msg_id, (unsigned)msg_length);
    }

    // Go back and wait for more data
    goto WaitForData;

}
//=============================================================================

#endif

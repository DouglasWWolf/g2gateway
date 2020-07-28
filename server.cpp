//=================================================================================================
// server.cpp -Implements threads that manage our TCP connections to the outside world
//=================================================================================================
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "server.h"
#include "typedefs.h"
#include "globals.h"
#include "common.h"

//=================================================================================================
// These are the commands that can be sent to the server via the special command pipe
//=================================================================================================
#define SPECIAL_CLOSE 1
//=================================================================================================


//=================================================================================================
// The version number of the GXIP protocol that we use to communicate with the TCP client
//=================================================================================================
#define PROTOCOL_MAJOR  1
#define PROTOCOL_MINOR  1
//=================================================================================================



//=================================================================================================
// When a packet comes in from he TCP client, it will contain one of these packet types
//=================================================================================================
#define PRO_PKT     0
#define CMD_PKT     1
#define REQ_PKT     2
#define RSP_PKT     3
#define HSK_PKT     4
#define MRM_PKT     5
#define CTL_PKT     6
#define CMD_E_PKT   9
#define REQ_E_PKT   10
#define RSP_E_PKT   11
//=================================================================================================



//=================================================================================================
// Message ID for gateway control requests
//=================================================================================================
#define CTL_GET_VERSION       1
#define CTL_GET_COM_STATS     2
#define CTL_GET_LIVE_SITES    3
#define CTL_GET_MAC           4
#define CTL_RESET             5
#define CTL_SET_SERIALNUM     6
#define CTL_GET_SERIALNUM     7
#define CTL_LOOPBACK_TEST     8
#define CTL_GET_BUSY_SITES    9
#define CTL_ECHO             10
#define CTL_GET_DLM_VERSION  11
//=================================================================================================





//=================================================================================================
//                    Structures for the various gateway control messages
//=================================================================================================
#pragma pack(push, 1)

struct ctl_header_t
{
    u16be   msg_length;
    u8      msg_type;
    u8      msg_id;
};

struct ctl_get_version_rsp_t
{
    ctl_header_t  header;
    u8            major;
    u8            minor;
    u8            build;
};

struct ctl_get_dlm_version_rsp_t
{
    ctl_header_t  header;
    u8            major;
    u8            minor;
    u8            build;
};

struct ctl_get_com_stats_rsp_t
{
    ctl_header_t  header;
    u8            naks_in;
    u8            naks_out;
    u8            cans_in;
    u8            cans_out;
};

struct ctl_get_live_sites_rsp_t
{
    ctl_header_t  header;
    u8            live_site_map;
};


struct ctl_get_busy_sites_rsp_t
{
    ctl_header_t  header;
    u8            busy_site_map;
};

struct ctl_get_mac_rsp_t
{
    ctl_header_t  header;
    u8            id_type;
    sMAC          mac;
    u8            filler[4];
};

struct ctl_get_serialnum_rsp_t
{
    ctl_header_t  header;
    u32           serialnum;  // This needs to be little endian!
};

struct ctl_set_serialnum_req_t
{
    ctl_header_t  header;
    u32           serialnum;  // This needs to be little endian!
};

struct ctl_set_serialnum_rsp_t
{
    ctl_header_t  header;
    u8            status;
};

struct ctl_reset_req_t
{
    ctl_header_t  header;
    u8            flags;
    u8            bitmap;
};

struct ctl_reset_rsp_t
{
    ctl_header_t  header;
    u8            status;
};

struct ctl_echo_req_t
{
    ctl_header_t  header;
    u8            data[256];
};

#pragma pack(pop)
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
    m_tcp_port = (slot == -1) ? 1066 : m_slot + 922;
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

wait_for_data:

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

        switch(m_tcp_packet.type)
        {
            case PRO_PKT:
                handle_protocol_request();
                break;

            case CTL_PKT:
                dispatch_control_request();
                break;

            default:
                printf("Rcvd message type %u: ID = %u, \n", m_tcp_packet.type, m_tcp_packet.payload[0]);
                break;
        }
    }

    // And go back and wait for the next incoming message
    goto wait_for_data;

}
//=================================================================================================


//=================================================================================================
// reset_connection() - Sends the server a message that says "Drop your TCP connection"
//=================================================================================================
void CServer::reset_connection()
{
    u8 cmd = SPECIAL_CLOSE;
    if (m_is_connected) write(m_special_pipe[1], &cmd, 1);
}
//=================================================================================================


//=================================================================================================
// handle_protocol_request() - Tell the Host what version of the GXIP protocol we are speaking
//
// A "protocol response" looks like this:
//
// Offset   0   =   High Byte of packet length
//          1   =   Low Byte of packet length
//          2   =   PRO_PKT (which should ALWAYS be zero!)
//          3   =   Major version of protocol
//          4   =   Minor version of protocol
//
// The format of a protocol response must NEVER change.  Ever, ever, ever!
// This way, even if our normal protocol changes, the Host will a pre-determined method of
// finding out what protocol we are speaking.
//=================================================================================================
void CServer::handle_protocol_request()
{
    u8 packet[5];

    // Fill in the two byte packet length
    packet[0] = 0;
    packet[1] = sizeof packet;

    // The returned "Packet Type" will always be "PRO_PKT"
    packet[2] = PRO_PKT;

    // Fill in the two byte protocol version
    packet[3] = PROTOCOL_MAJOR;
    packet[4] = PROTOCOL_MINOR;

    // Send the packet to the host
    m_socket.send(packet, sizeof(packet));
}
//=================================================================================================



//=================================================================================================
// control_response() - Sends a gateway-control response message back to the client
//
// Passed:  ptr    = Pointer to the response message to be sent
//          length = Length of the response message in bytes
//=================================================================================================
void CServer::control_response(void* ptr, int length)
{
    // Map a message header onto the message buffer
    ctl_header_t& header = *(ctl_header_t*)ptr;

    // Store the length of the entire message
    header.msg_length = length;

    // This is a gateway-control response packet
    header.msg_type = RSP_PKT;

    // Fill in ID that tells the client what type of msg we're responding to
    header.msg_id = m_tcp_packet.payload[0];

    // Send it back to the client
    m_socket.send(ptr, length);
}
//=================================================================================================


//=================================================================================================
// dispatch_control_request() - Examines the control request message ID and dispatches the
//                              appropriate handler
//=================================================================================================
void CServer::dispatch_control_request()
{
    // Find out which control request message is being sent
    int msg_id = m_tcp_packet.payload[0];

    switch(msg_id)
    {
        case CTL_GET_VERSION:
            handle_ctl_get_version();
            break;

        case CTL_GET_DLM_VERSION:
            handle_ctl_get_dlm_version();
            break;

        case CTL_GET_COM_STATS:
            handle_ctl_get_com_stats();
            break;

        case CTL_GET_LIVE_SITES:
            handle_ctl_get_live_sites();
            break;

        case CTL_GET_BUSY_SITES:
            handle_ctl_get_busy_sites();
            break;

        case CTL_GET_MAC:
            handle_ctl_get_mac();
            break;

        case CTL_GET_SERIALNUM:
            handle_ctl_get_serialnum();
            break;

        case CTL_SET_SERIALNUM:
            handle_ctl_set_serialnum();
            break;

        case CTL_RESET:
            handle_ctl_reset();
            break;

        case CTL_ECHO:
            handle_ctl_echo();
            break;
    }
}
//=================================================================================================


//=================================================================================================
// handle_get_ctl_version() - Responds with the version of this software
//=================================================================================================
void CServer::handle_ctl_get_version()
{
    ctl_get_version_rsp_t   rsp;

    rsp.major = Instrument.fw_major;
    rsp.minor = Instrument.fw_minor;
    rsp.build = Instrument.fw_build;

    control_response(&rsp, sizeof rsp);
}
//=================================================================================================


//=================================================================================================
// handle_ctl_get_dlm_version() - Responds with the version of the (non-existent) download manager
//
// Note: Since we don't have a download manager, just return all zeros
//=================================================================================================
void CServer::handle_ctl_get_dlm_version()
{
    ctl_get_dlm_version_rsp_t   rsp;

    memset(&rsp, 0, sizeof rsp);

    control_response(&rsp, sizeof rsp);
}
//=================================================================================================


//=================================================================================================
// handle_ctl_get_com_stats() - Responds with the communication statistics
//
// Note: Since we aren't supporting this message at this time, just return all zeros
//=================================================================================================
void CServer::handle_ctl_get_com_stats()
{
    ctl_get_com_stats_rsp_t   rsp;

    memset(&rsp, 0, sizeof rsp);

    control_response(&rsp, sizeof rsp);
}
//=================================================================================================


//=================================================================================================
// handle_ctl_get_live_sites() - Responds with a bitmap of which sites are connected to a GX module
//=================================================================================================
void CServer::handle_ctl_get_live_sites()
{
    ctl_get_live_sites_rsp_t   rsp;

    rsp.live_site_map = get_live_sites();

    control_response(&rsp, sizeof rsp);
}
//=================================================================================================



//=================================================================================================
// handle_ctl_get_busy_sites() - Responds with a bitmap of which sites are busy
//=================================================================================================
void CServer::handle_ctl_get_busy_sites()
{
    ctl_get_busy_sites_rsp_t   rsp;

    rsp.busy_site_map = 0;

    control_response(&rsp, sizeof rsp);
}
//=================================================================================================


//=================================================================================================
// handle_ctl_get_mac() - Responds with the MAC address of our system
//=================================================================================================
void CServer::handle_ctl_get_mac()
{
    ctl_get_mac_rsp_t   rsp;

    memset(&rsp, 0, sizeof rsp);
    rsp.id_type = 1;
    rsp.mac = Network.mac();

    control_response(&rsp, sizeof rsp);
}
//=================================================================================================


//=================================================================================================
// handle_ctl_get_serialnum() - Responds with the serial number of our system
//=================================================================================================
void CServer::handle_ctl_get_serialnum()
{
    u32   serialnum;

    ctl_get_serialnum_rsp_t   rsp;

    Config.get(SPEC_INSTRUMENT_SN, &serialnum);
    rsp.serialnum = serialnum;

    control_response(&rsp, sizeof rsp);
}
//=================================================================================================


//=================================================================================================
// handle_ctl_set_serialnum() - Sets and saves the serial number of this instrument
//=================================================================================================
void CServer::handle_ctl_set_serialnum()
{
    ctl_set_serialnum_req_t& req = *(ctl_set_serialnum_req_t*)&m_tcp_packet;
    ctl_set_serialnum_rsp_t  rsp;

    Config.set(SPEC_INSTRUMENT_SN, to_string("%u", req.serialnum));
    Config.save();

    rsp.status = 1;

    control_response(&rsp, sizeof rsp);
}
//=================================================================================================


//=================================================================================================
// handle_ctl_reset() - Eventually this will have to reboot the Nios
//=================================================================================================
void CServer::handle_ctl_reset()
{
    ctl_reset_rsp_t  rsp;

    rsp.status = 0;

    control_response(&rsp, sizeof rsp);
}
//=================================================================================================


//=================================================================================================
// handle_ctl_echo() - Echos back exactly the same message we received
//=================================================================================================
void CServer::handle_ctl_echo()
{
    // Map our response over the original message we received
    ctl_echo_req_t& rsp = *(ctl_echo_req_t*)&m_tcp_packet;

    // And send the client back the message they sent us
    control_response(&rsp, sizeof rsp);
}
//=================================================================================================




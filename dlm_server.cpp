//=================================================================================================
// dlm_server.cpp - Implements server that acts as a download manager for gateway software updates
//=================================================================================================
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "dlm_server.h"
#include "typedefs.h"
#include "globals.h"
#include "common.h"

//=================================================================================================
// These are the commands that can be sent to the server via the special command pipe
//=================================================================================================
#define SPECIAL_CLOSE     1
//=================================================================================================


//=================================================================================================
// Message ID for DLM requests
//=================================================================================================
#define DLM_GET_VERSION     100
#define DLM_GET_MAC         101
#define DLM_FLASH_INIT      102
#define DLM_FLASH_WRITE     103
#define DLM_FLASH_COMMIT    104
#define DLM_MAGIC_OFFSET    105
//=================================================================================================





//=================================================================================================
//                    Structures for the various gateway control messages
//=================================================================================================
#pragma pack(push, 1)

struct dlm_header_t
{
    u16be   msg_length;
    u8      msg_id;
    u8      data[1];
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
CDLM::CDLM()
{
    // When we start, we are neither initialized, nor connected to a client
    m_is_connected   = false;
    m_is_initialized = false;

    // We don't yet have an incoming DLM message
    m_message = nullptr;

    // This is the port number that our legacy gateway listened for DLM connections on
    m_tcp_port = 24601;

    // We haven't yet opened a file for writing
    m_ofile = nullptr;
}
//=================================================================================================


//=================================================================================================
// handle_dlm_flash_init() - Creates an empty file for data to be downloaded into
//=================================================================================================
bool CDLM::handle_dlm_flash_init()
{
    m_filename = Instrument.sandbox;
    if (m_filename.right(1) != "/") m_filename += '/';
    m_filename += "image";

    // If for some reason we already have a file open, close it!
    if (m_ofile) fclose(m_ofile);

    // Open our output file
    m_ofile = fopen(m_filename, "wb");

    // And tell the caller whether or not this worked
    return (m_ofile != nullptr);
}
//=================================================================================================


//=================================================================================================
// handle_dlm_flash_write() - Writes data to our DLM file
//=================================================================================================
bool CDLM::handle_dlm_flash_write()
{
    // Map a response packet over our message
    dlm_header_t& message = *(dlm_header_t*)m_message;

    // If we don't have a file open, something has gone awry
    if (m_ofile == nullptr) return false;

    // This is how many bytes of data the caller wants us to write to the file
    int data_length = message.msg_length - 3;

    // Write the client's data to our file
    fwrite(message.data, 1, data_length, m_ofile);

    // And tell the caller that all is well
    return true;
}
//=================================================================================================



//=================================================================================================
// handle_dlm_flash_commit() - Closes the file we've been writing and kicks off the upgrade
//                             process.
//=================================================================================================
bool CDLM::handle_dlm_flash_commit()
{
    // If we don't have a file open, something has gone awry
    if (m_ofile == nullptr) return false;

    // Close the file we've been writing, we're done with it
    fclose(m_ofile);
    m_ofile = nullptr;

    // And tell the caller that all is well
    return true;
}
//=================================================================================================





//=================================================================================================
// read_dlm_msg_from_socket() - Fetches a DLM message from the socket
//=================================================================================================
bool CDLM::read_dlm_msg_from_socket()
{
    u16be   msg_length;

    // Make sure we don't leave an old message buffer sitting around on the heap
    delete[] m_message;
    m_message = nullptr;

    // Read the first two bytes of the message, it's the msg length
    if (m_socket.receive(&msg_length, 2) < 2) return false;

    // Create a buffer on the heap large enough to hold this message
    m_message = new u8[msg_length];

    // Fill in the length in our message
    m_message[0] = msg_length.m_octet[0];
    m_message[1] = msg_length.m_octet[1];

    // This is how many more bytes we should find in this message
    int bytes_expected = msg_length - 2;

    // If this can't possibly be a valid length, close the socket
    if (bytes_expected < 1 || bytes_expected > 0x10000) return false;

    // Read in the rest of the message
    int bytes_read = m_socket.receive(m_message+2, bytes_expected);

    // Tell the caller whether we read an entire message
    return (bytes_read == bytes_expected);
}
//=================================================================================================



//=================================================================================================
// send_response() - Sends a response message back to the client
//=================================================================================================
void CDLM::send_response(const u8* data, int response_length)
{
    u8 buffer[128];

    // Map a response packet over our buffer
    dlm_header_t& response = *(dlm_header_t*)buffer;

    // Packet length is the data length plus the length of the 3 byte header
    int packet_length = response_length + 3;

    // Fill in the length of the response packet
    response.msg_length = packet_length;

    // Fill in the response message ID
    response.msg_id = m_message[2];

    // Copy the caller's response data into our outgoing packet
    memcpy(response.data, data, response_length);

    // And send the response back to the client
    m_socket.send(&response, packet_length);
}
//=================================================================================================


//=================================================================================================
// main() - When this thread spawns, execution starts here
//=================================================================================================
void CDLM::main(void* p1, void* p2, void* p3)
{
    fd_set  rfds;
    char    special_cmd;
    u8      rc;

    // Other threads send us messages by writing to this pipe
    pipe(m_special_pipe);

    // Get a convenient name for our side of this pipe
    int special_fd = m_special_pipe[0];

    // Tell the outside world that we are initialized
    m_is_initialized = true;

wait_for_connect:

    // Throw away any existing message
    delete[] m_message;
    m_message = nullptr;

    // There is not yet a client connected to our socket
    m_is_connected = false;

    // Tell the world what's up
    printf("Waiting for DLM connection on port %i\n", m_tcp_port);

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
    printf("Client connected to DLM port %i\n", m_tcp_port);

    // Make sure there are no leftover commands waiting in the command pipe
    drain_fd(special_fd);

    // Turn off Nagling on the socket so that data is not buffered after we send it
    m_socket.set_nagling(false);

    // Figure out what the largest file descriptor is
    int sd = m_socket.get_fd();
    int max_fd = 0;
    if (max_fd < sd        ) max_fd = sd;
    if (max_fd < special_fd) max_fd = special_fd;


wait_for_data:

    // Throw away any existing message
    delete[] m_message;
    m_message = nullptr;

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
        if (!read_dlm_msg_from_socket())
        {
            m_socket.close();
            printf("Port %i closed by client\n", m_tcp_port);
            goto wait_for_connect;
        }

        // Map a DLM message structure over our message buffer
        dlm_header_t& dlm_message = *(dlm_header_t*)m_message;

        // And dispatch this GXIP message to the appropriate handler
        switch(dlm_message.msg_id)
        {
            case DLM_FLASH_INIT:
                rc = handle_dlm_flash_init();
                send_response(&rc, 1);
                break;

            case DLM_FLASH_WRITE:
                rc = handle_dlm_flash_write();
                send_response(&rc, 1);
                break;

            case DLM_FLASH_COMMIT:
                rc = handle_dlm_flash_commit();
                send_response(&rc, 1);
                break;

            default:
                printf("Rcvd DLM msg ID = 0x%02X\n", dlm_message.msg_id);
                rc = 0;
                send_response(&rc, 1);
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
void CDLM::reset_connection()
{
    u8 cmd = SPECIAL_CLOSE;
    if (m_is_connected) write(m_special_pipe[1], &cmd, 1);
}
//=================================================================================================



#if 0
//=================================================================================================
// read_gxip_msg_from_socket() - Fetches a GXIP message from the socket
//=================================================================================================
bool CServer::read_gxip_msg_from_socket(int sd)
{
    char* remaining_packet = ((char*)&m_gxip_packet)+2;
    int   remaining_size   = sizeof(m_gxip_packet) - 2;

    // Read the first two bytes of the message, it's the msg length
    if (read(sd, &m_gxip_packet, 2) < 2) return false;

    // Find out how long entire message is (including the two length bytes)
    int msg_length = m_gxip_packet.length();

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
    m_tcp_port = m_slot + 922;
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

    // Turn off Nagling on the socket so that data is not buffered after we send it
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

        // And dispatch this GXIP message to the appropriate handler
        switch(m_gxip_packet.type)
        {
            case PRO_PKT:
                handle_protocol_request();
                break;

            case CTL_PKT:
                dispatch_control_request();
                break;

            case CMD_PKT:
            case REQ_PKT:
            case CMD_E_PKT:
            case REQ_E_PKT:
                if (m_slot == 0) FWListener.transact(m_gxip_packet);
                break;

            default:
                printf("Rcvd msg type %u: ID = 0x%3X\n", m_gxip_packet.type, m_gxip_packet.id());
                break;
        }
    }

    // And go back and wait for the next incoming message
    goto wait_for_data;

}
//=================================================================================================



//=================================================================================================
// send_gxip_to_host() - Used by other threads to send a GXIP message back to the host
//=================================================================================================
PCriticalSection send_gxip_to_host_cs;
void CServer::send_gxip_to_host(gxip_packet_t& message)
{
    // Make sure that only one thread at a time tries to do this
    PSingleLock lock(&send_gxip_to_host_cs);

    // If there is someone connected to our socket, send the message
    if (m_is_connected) m_socket.send(&message, message.length());
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
    header.msg_id = m_gxip_packet.payload[0];

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
    int msg_id = m_gxip_packet.payload[0];

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

    EEPROM.get(SPEC_INSTRUMENT_SN, &serialnum);
    rsp.serialnum = serialnum;

    control_response(&rsp, sizeof rsp);
}
//=================================================================================================


//=================================================================================================
// handle_ctl_set_serialnum() - Sets and saves the serial number of this instrument
//=================================================================================================
void CServer::handle_ctl_set_serialnum()
{
    ctl_set_serialnum_req_t& req = *(ctl_set_serialnum_req_t*)&m_gxip_packet;
    ctl_set_serialnum_rsp_t  rsp;

    EEPROM.set(SPEC_INSTRUMENT_SN, to_string("%u", req.serialnum));
    EEPROM.save();

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
    ctl_echo_req_t& rsp = *(ctl_echo_req_t*)&m_gxip_packet;

    // And send the client back the message they sent us
    control_response(&rsp, sizeof rsp);
}
//=================================================================================================


#endif

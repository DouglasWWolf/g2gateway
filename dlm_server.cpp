//=================================================================================================
// dlm_server.cpp - Implements server that acts as a download manager for gateway software updates
//=================================================================================================
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <string.h>
#include "dlm_server.h"
#include "typedefs.h"
#include "globals.h"
#include "cprocess.h"
#include "common.h"
#include "filesys.h"

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
// get_other_bank() - Get the directory name of the bank that we did *not* boot from
//=================================================================================================
PString get_other_bank()
{
    char buffer[256];

    // Find out the name of the current working directory
    getcwd(buffer, sizeof buffer);

    // Get a pointer to the last character of that string
    char* last_char = strchr(buffer, 0) - 1;

    // Change a '1' to a '0' and vice-versa
    *last_char = 97 - *last_char;

    // And hand the resulting string to the caller
    return buffer;
}
//=================================================================================================



//=================================================================================================
// software_update() - Copies the file that was just downloaded into the "other" bank (i.e, the
//                     bank we didn't just boot from), untars it, and if "install.sh" exists,
//                     runs it.
//=================================================================================================
bool software_update(const char* original)
{
    CProcess    process;
    bool        result = false;
    int         rc;
    FILE*       ofile;
    int         stage = 1;

    // Make the file-system writable
    remount_rw();

    // Get the path to the software bank we did *not* just boot from
    PString work_dir = get_other_bank();

    // Make sure the folder exists
    mkdir(work_dir.c(), 0777);

    // Make sure it's readable/writable and empty
    process.run(true, "chmod 777 %s && rm -rf %s/*", work_dir.c(), work_dir.c());

    // Build the name of a file on our SD card
    PString tarball = work_dir + "/tarball.tgz";

    // Build the name of the install script on our SD card
    PString install_sh = work_dir + "/install.sh";

    // Build the name of the gateway executable
    PString exe = work_dir + "/g2gateway.arm";

    // Build the name of the pointer file that we need to update
    PString pointer = parent_dir(work_dir) + "/pointer";

    // Copy the tarball into our file system
    stage = 1;
    if (!copy_file(original, tarball, true)) goto end;

    // Erase the original file
    remove(original);

    // Go unpack the tarball
    stage = 2;
    rc = process.run(true, "cd %s && tar xvf %s", work_dir.c(), tarball.c());
    if (rc != 0) goto end;

    // Remove the tarball, we don't need it anymore
    remove(tarball.c());

    // If the install script exists, run it
    if (file_exists(install_sh))
    {
        // Make sure that the installer script is executable
        stage = 3;
        rc = process.run(true, "chmod 777 %s", install_sh.c());
        if (rc != 0) goto end;

        // Run the installer
        stage = 4;
        rc = process.run(true, "cd %s && %s", work_dir.c(), install_sh.c());
        if (rc != 0) goto end;
    }

    // If the gateway executable file exists...
    if (file_exists(exe))
    {
        // Make sure it's executable
        stage = 5;
        rc = process.run(true, "chmod 777 %s", exe.c());
        if (rc != 0) goto end;

        // Update the pointer file
        stage = 6;
        ofile = fopen(pointer, "wb");
        if (ofile == nullptr) goto end;
        fputs(work_dir.c(), ofile);
        fputs("\n", ofile);
        fclose(ofile);

        // And the new software is installed and ready to go
        result = true;
    }


end:

    // Lock down the file system again
    remount_ro();

    if (result)
    {
        // If we're not in DLM mode, go ahead and launch the new software right now
        if (!Instrument.is_dlm) exit_for_restart();
    }
    else
    {
        printf("software updated failed at stage %i\n", stage);
    }

    // Tell the caller whether or not this all worked
    return result;
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
    m_filename += "image.tgz";

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

    // Go perform a software update and tell the caller whether or not it worked
    return software_update(m_filename);
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

//=================================================================================================
// netsock.cpp - Implements a reliable socket class
//=================================================================================================
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <errno.h>
#include <string>
#include "netsock.h"
#include "cthread.h"
using std::string;


//=================================================================================================
// get_host_by_name() - This is a thread-safe version of gethostbyname()
//=================================================================================================
static PCriticalSection m_gethostbyname_cs;

bool CNetSock::get_host_by_name(string server_name, in_addr_t* ip_addr)
{
	// Make this routine accessible to just one thread at a time
	PSingleLock access(&m_gethostbyname_cs);

	// Handle "localhost" efficiently.
	if (server_name == "localhost") server_name = "127.0.0.1";

	// Assume for a moment that "server_name" is a dotted-quad address
	in_addr_t addr = inet_addr(server_name.c_str());

	// If it was indeed a dotted-quad, then we have the IP address
	if (addr)
	{
		*ip_addr = addr;
		return true;
	}

	// Try to resolve this server name into an IP address
	hostent* server = gethostbyname(server_name.c_str());

	// If the server lookup  was successful, save it's IP address
	if (server) *ip_addr = *(in_addr_t*)&server->h_addr;

	// Tell the caller whether or not we were able to resolve the server
	return server != nullptr;
}
//=================================================================================================



//=================================================================================================
// NetSock() - Default constructor
//=================================================================================================
CNetSock::CNetSock()
{
    // We're not bound to a port yet
    m_is_created = false;

    // Make sure our descriptor doesn't point at anything valid!
    m_sd = -1;
}
//=================================================================================================


//=================================================================================================
// Connect() - Connects to a remote host
//=================================================================================================
bool CNetSock::connect(string server_name, int port)
{
	in_addr_t server_addr;

	// Make sure the socket isn't already open
	close();

	// Create the socket
	m_sd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);

	// If the socket() call fails, complain
	if (m_sd < 0)
	{
		m_error_str = "Failure on socket()";
		m_error = SOCKET_FAILED;
		return false;
	}

	// Lookup the server name
	if (!get_host_by_name(server_name.c_str(), &server_addr))
	{
		m_error_str = "Can't resolve server name";
		m_error = NO_SUCH_SERVER;
		return false;
	}

	// Make sure the server address structure is all zeros
	memset(&m_serv_addr, 0, sizeof(m_serv_addr));

	// Fill in the server address
    m_serv_addr.sin_family      = AF_INET;
    m_serv_addr.sin_port        = htons(port);
    m_serv_addr.sin_addr.s_addr = server_addr;

    // Try to connect to the server
    if (::connect(m_sd, (sockaddr *) &m_serv_addr, sizeof(m_serv_addr)) < 0)
    {
		m_error_str = "Can't Connect";
		m_error = CANT_CONNECT;
		return false;
	}

    // Tell the caller that we have a connection
    return true;
}
//=================================================================================================



//=================================================================================================
// CreateServer() - Create a TCP server socket
//=================================================================================================
bool CNetSock::create_server(int port)
{
	// Create the socket
	m_sd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);

	// If the socket() call fails, complain
	if (m_sd < 0)
	{
		m_error_str = "Failure on socket()";
		m_error = SOCKET_FAILED;
		return false;
	}

	// Allow us to re-use this port if it's still in TIME_WAIT
	int optval = 1;
	setsockopt(m_sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

	// Make sure the server addr structure is all zeros
	memset(&m_serv_addr, 0, sizeof(m_serv_addr));

	// Set up the server address structure
	m_serv_addr.sin_family = AF_INET;
	m_serv_addr.sin_addr.s_addr = INADDR_ANY;
	m_serv_addr.sin_port = htons(port);

	// Bind this server structure to the file descriptor of our socket
	if (bind(m_sd, (sockaddr *) &m_serv_addr, sizeof(m_serv_addr)) < 0)
	{
		m_error_str = "Failure on bind()";
		m_error     = BIND_FAILED;
		return false;
	}

	// This socket has been created
	m_is_created = true;

	// Tell the caller this his socket is created!
	return true;
}
//=================================================================================================



//=================================================================================================
// set_nagling() - Turns Nagle's algorithm on or off
//=================================================================================================
void CNetSock::set_nagling(bool flag)
{
	setsockopt(m_sd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof flag);
}
//=================================================================================================



//=================================================================================================
// Accept() - This accepts an incoming connection
//=================================================================================================
bool CNetSock::accept(CNetSock* newsock)
{
    // If we're not created yet, don't even think about it
    if (!m_is_created) return false;

    // Tell the OS to start listening at this socket's port number
    int status = listen(m_sd, 1);

    // If listen() barfed on us, tell the caller
    if (status < 0)
	{
		// Tell the caller that listen() barfed on us
		m_error_str = "Failure on listen()";
		m_error = LISTEN_FAILED;
		return false;
	}

    // Find the size of a sockaddr_in structure
    socklen_t addr_size = sizeof(m_cli_addr);

    // Accept the incoming connection
    int handle = ::accept(m_sd, (sockaddr*)&m_cli_addr, &addr_size);

    // If the call to "accept()" failed, tell the caller
    if (handle < 0)
    {
		m_error_str = "Failure on accept()";
		m_error = ACCEPT_FAILED;
		return false;
    }

    // If the caller passed us a socket to pass this to...
    if (newsock)
    {
        // Copy this object to the new socket object
        *newsock = *this;

        // And give that new socket object the SOCKET handle to talk on
        newsock->m_sd = handle;
    }

    else
    {
		// Close our original socket, we don't need it anymore
		::close(m_sd);

		// This is now the file descriptor we're talking on
		m_sd = handle;
	}

    // And tell the caller that he got connected
    return true;
};
//=================================================================================================


//=================================================================================================
// accept_nonblock() - This accepts an incoming connection without blocking
//=================================================================================================
bool CNetSock::accept_nonblocking()
{
    int flags;

    // If we're not created yet, don't even think about it
    if (!m_is_created) return false;

    // Find the size of a sockaddr_in structure
    socklen_t addr_size = sizeof(m_cli_addr);

    // Make this socket non blocking
    flags = fcntl(m_sd, F_GETFL, 0);
    fcntl(m_sd, F_SETFL, flags | O_NONBLOCK);

    // Accept the incoming connection
    int handle = ::accept(m_sd, (sockaddr*)&m_cli_addr, &addr_size);

    // If the call to "accept()" failed, then no connection is pending
    if (handle < 0) return false;

    // Close our original socket, we don't need it anymore
    ::close(m_sd);

    // This is now the file descriptor we're talking on
    m_sd = handle;

    // Make this socket non blocking
    flags = fcntl(m_sd, F_GETFL, 0);
    fcntl(m_sd, F_SETFL, flags & ~O_NONBLOCK);

    // And tell the caller that he got connected
    return true;
};
//=================================================================================================



//=================================================================================================
// wait_for_data() - Waits for data to become available for reading
//=================================================================================================
bool CNetSock::wait_for_data(int milliseconds)
{
    fd_set readfds;

    // Build the timeout structure
    timeval timeout = {0, milliseconds * 1000};

    // Clear out the list of file descriptors
    FD_ZERO(&readfds);

    // Add our socket to the list of file descriptors
    FD_SET(m_sd, &readfds);

    // Wait for data to become available
    int retval =  select(m_sd+1, &readfds, nullptr, nullptr, &timeout);

    // Tell the caller whether or not there is data waiting
    return (retval > 0);
}
//=================================================================================================


//=================================================================================================
// bytes_available() - Returns the number of bytes ready for a Receive()
//=================================================================================================
int CNetSock::bytes_available()
{
	unsigned int count = 0;
	ioctl(m_sd, FIONREAD, &count);
    return count;
}
//=================================================================================================


//=================================================================================================
// receive() - Waits for and reads incoming data
//
// Passed:          buffer = where the incoming data should be stored
//                  length = how many bytes to wait for
//                  flags  = read flags.  Only supported flag is MSG_PEEK
//
// Returns either:     length
//                  or -1 (indicates an error)
//                  or  0 (indicates the socket was closed)
//=================================================================================================
int CNetSock::receive(void* buffer, int length, int flags)
{
    // Don't attempt to recv zero bytes
    if (length == 0) return 0;

    // Get a pointer to our output
    char* ptr = (char*)buffer;

    // Keep track of how many bytes we have left to read
    int bytes_remaining_to_read = length;

    // Loop until there are no more bytes to read...
    while (bytes_remaining_to_read)
    {
        // Fetch some bytes from the socket
        int bytes_read = recv(m_sd, ptr, bytes_remaining_to_read, flags);

        // If the read failed, tell the caller
        if (bytes_read < 0) return -1;

        // If the socket closed, tell the caller
        if (bytes_read == 0) return 0;

        // Adjust both our pointer, and the # of bytes remaining
        ptr += bytes_read;
        bytes_remaining_to_read -= bytes_read;
    }

    // Tell the caller that we have his data for him
    return length;
}
//=================================================================================================


//=================================================================================================
// get_line() - Fetch a line of data from the socket
//=================================================================================================
bool CNetSock::get_line(void* buffer)
{
    char    chr;

    // Point to our output buffer
    char *ptr = (char*)buffer;

    while (1)
    {
        // Wait for a character
        if (receive(&chr, 1) < 1) return false;
    
        // Throw away carriage returns
        if (chr == '\r') continue;

        // Handle back-space characters
        if (chr == 8)
        {
            if (ptr > (char*)buffer) --ptr;
            continue;
        }

        // If it's a line-feed, it's the end of the line
        if (chr == '\n') break;

        // Otherwise, place this character in the buffer
        *ptr++ = chr;
    }

    // Terminate the output buffer
    *ptr = 0;

    // And tell the caller we have his data
    return true;
}
//=================================================================================================


//=================================================================================================
// send() - Sends data to the other side of a connected socket
//
// Returns either:   -1 = An error occured
//                   or the number of bytes actually sent.  This will
//                   always equal length unless the socket was closed remotely
//=================================================================================================
int CNetSock::send(string s)
{
	const char* p = s.c_str();
	int length = s.length();
	return send(p, length);
}
int CNetSock::sendf(string fmt, ...)
{
	char buffer[8000];

	// This is a pointer to a variable argument list
	va_list first_arg;

	// Point to the first argument after the "fmt" parameter
	va_start(first_arg, fmt);

	// Perform a "printf" of our arguments into the "buffer" area.
	vsprintf(buffer, fmt.c_str(), first_arg);

	// Tell the system we're done with first_arg
	va_end(first_arg);

	// Find out how many characters are in that buffer
	int length = strlen(buffer);

	// And send it!
	return send(buffer, length);
}
int CNetSock::send(const void* buffer, int length)
{
    // Don't attempt to send zero bytes
    if (length == 0) return 0;

    // Get a pointer to our output
    char* ptr = (char*)buffer;

    // Keep track of how many bytes we have left to read
    int bytes_remaining_to_send = length;

    // Loop until there are no more bytes to read...
    while (bytes_remaining_to_send > 0)
    {
        // Send some bytes to the socket
        int bytes_sent = ::send(m_sd, ptr, bytes_remaining_to_send, 0);

        // If the send failed, tell the caller
        if (bytes_sent < 0) return -1;

        // If the socket closed, tell the caller
        if (bytes_sent == 0) break;

        // Adjust both our pointer, and the # of bytes remaining
        ptr += bytes_sent;
        bytes_remaining_to_send -= bytes_sent;
    }

    // Tell the caller how many bytes we sent
    return (length - bytes_remaining_to_send);
}
//=================================================================================================


//=================================================================================================
// close() - Closes a socket.  This does *not* wait for unsent data to be
//           sent!
//=================================================================================================
void CNetSock::close()
{
    if (m_sd != -1) ::close(m_sd);
    m_sd = -1;
}
//=================================================================================================






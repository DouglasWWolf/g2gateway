//=================================================================================================
// udpsocket.cpp - Implements a class that manages UDP sockets
//=================================================================================================

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "udpsocket.h"


//=================================================================================================
// Destructor() - Closes the socket
//=================================================================================================
UDPSocket::~UDPSocket() {if (m_fd > -1) close(m_fd);}
//=================================================================================================


//=================================================================================================
// create_sender() - Create a socket useful for sending UDP packets
//=================================================================================================
bool UDPSocket::create_sender(int port, const char* dest_ip)
{
	int broadcast = 1;

	// If the socket is open, close it
	if (m_fd > 0) close(m_fd);

	// Create the socket
    m_fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);

    // If that failed, tell the caller
    if (m_fd < 0) return false;

	// Make sure the destination IP address is valid
	if (dest_ip == nullptr) dest_ip = "255.255.255.255";

    // If we're broadcasting, set up the socket for broadcast
    if (strcmp(dest_ip, "255.255.255.255") == 0 || strcmp(dest_ip, "broadcast") == 0)
    {
        if (setsockopt(m_fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof broadcast) == -1)
        {
              perror("setsockopt (SO_BROADCAST)");
              return false;
        }
    }

    // Set up destination address
    memset(&m_to_addr,0,sizeof(m_to_addr));
    m_to_addr.sin_family = AF_INET;
    m_to_addr.sin_addr.s_addr = inet_addr(dest_ip);
    m_to_addr.sin_port=htons(port);


    // Tell the caller that all is well
    return true;
}
//=================================================================================================


//=================================================================================================
// create_listener() - Creates a socket for listening on a UDP port
//=================================================================================================
bool UDPSocket::create_listener(int port)
{
	sockaddr_in addr;

	// If the socket is open, close it
	if (m_fd > 0) close(m_fd);

	// Create the socket
    m_fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);

    // If that failed, tell the caller
    if (m_fd < 0) return false;

    // Bind the socket to the specified port
    addr.sin_family = AF_INET;
    addr.sin_port = htons (port);
    addr.sin_addr.s_addr = htonl (INADDR_ANY);
    if (bind(m_fd, (sockaddr *) &addr, sizeof (addr)) < 0)
    {
    	printf("Call to bind() failed for port %i\n", port);
    	return false;
    }

    // Tell the caller that all is well
    return true;
}
//=================================================================================================


//=================================================================================================
// bind_to() - Binds this socket to the the specified network interface
//=================================================================================================
bool UDPSocket::bind_to(const char* interface_ip)
{
    struct sockaddr_in source;

    memset(&source, 0, sizeof source);
    source.sin_family = AF_INET;
    source.sin_addr.s_addr = inet_addr(interface_ip);
    bool status = bind(m_fd, (struct sockaddr*)&source, sizeof source) == 0;
    return status;
}
//=================================================================================================


//=================================================================================================
// send() - Call this to transmit data on a "sender" socket
//=================================================================================================
void UDPSocket::send(const void* msg, int length)
{
	sendto(m_fd, msg, length, 0, (sockaddr *) &m_to_addr, sizeof(m_to_addr));
}
//=================================================================================================


//=================================================================================================
// Get() - Call this to wait for a packet on a "Listener" socket
//=================================================================================================
int UDPSocket::get(void* buffer, int buf_size, unsigned int* p_source)
{
	socklen_t addrlen = sizeof(m_from_addr);

	int result = recvfrom(m_fd, buffer, buf_size, 0,(sockaddr*)&m_from_addr, &addrlen);

	if (p_source) memcpy(p_source, &m_from_addr.sin_addr.s_addr, 4);

	return result;
}
//=================================================================================================


//=================================================================================================
// WaitForData() - Waits for data to become available for reading
//=================================================================================================
bool UDPSocket::wait_for_data(int iMilliseconds)
{
    fd_set readfds;

    // Build the timeout structure
    timeval timeout = {0, iMilliseconds * 1000};

    // Clear out the list of file descriptors
    FD_ZERO(&readfds);

    // Add our socket to the list of file descriptors
    FD_SET(m_fd, &readfds);

    // Wait for data to become available
    int retval =  select(m_fd+1, &readfds, NULL, NULL, &timeout);

    // Tell the caller whether or not there is data waiting
    return (retval > 0);
}
//=================================================================================================

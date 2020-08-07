//=================================================================================================
// udpsocket.h - Defines a class for managing UDP sockets
//=================================================================================================
#pragma once
#include <netinet/in.h>


//=================================================================================================
// UDPSocket() - UDP socket for sending or receiving UDP datagrams
//=================================================================================================
class UDPSocket
{
public:

	// Constructor, initialized member variables
	UDPSocket() {m_fd = -1;}

	// Destructor
	~UDPSocket();

	// Create a socket that we will send UDP packets on.
	// dest_ip is NULL, this will be a broadcast socket
	bool	create_sender(int port, const char* dest_ip = nullptr, const char* if_name = nullptr);

	// Create a socket that we will use to receive UDP packets
	bool	create_listener(int port, const char* if_name = nullptr);

	// Binds this socket to a particular network interface
	bool    bind_to(const char* interface_ip);

	// Call this to send a message
	void	send(const void* msg, int length);

	// Call this to wait for data to arrive on the socket
	bool    wait_for_data(int milliseconds);

	// Call this to wait for a UDP packet to arrive
	int     get(void* buffer, int buffer_length, unsigned int* source_ip = nullptr);

	// Returns the file descriptor of this socket
	int     get_fd() {return m_fd;}

protected:

	// The file descriptor
	int      m_fd;

	// Address that packet was received from
	sockaddr_in m_from_addr;

	// Address that packet will go out to
	sockaddr_in m_to_addr;
};
//=================================================================================================


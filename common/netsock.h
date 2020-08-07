//=================================================================================================
// netsock.h - Defines a class that manages TCP sockets
//=================================================================================================

#pragma once
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include "cthread.h"

//=================================================================================================
// CNetSock() - Supports TCP sockets.
//=================================================================================================
class CNetSock
{
public:

	// These are the codes that can be returned by GetError()"
	enum
	{
		SOCKET_FAILED,
		BIND_FAILED,
		LISTEN_FAILED,
		ACCEPT_FAILED,
		NO_SUCH_SERVER,
		CANT_CONNECT
	};

public:

	// Default Constructor
	CNetSock();

	// Create a server socket
	bool 	create_server(int port);

	// Connect to a server
	bool 	connect(std::string server_name, int port);

	// Call this to turn nagling on and off
	void	set_nagling(bool flag);

	// Accepts an incoming connection
	bool 	accept(CNetSock* newsock = NULL);

	// This is a non-blocking accept
	bool    accept_nonblocking();

	// This waits for data to arrive on the socket
	bool	wait_for_data(int milliseconds);

	// This returns the number of bytes available to Receive
    int     bytes_available();

    // Call this to fetch bytes from the socket
    int 	receive(void* buffer, int length, int flags = 0);

    // Fetch a line from the socket. Line will be terminated with nul
    // and will not contain a carriage-return or a line-feed
    bool 	get_line(void* buffer);

    // Call these to send data to the socket
    int 	sendf(std::string fmt, ...);
    int     send(std::string s);
    int 	send(const void* buffer, int length);

	// If a call fails, call this to get the numeric error code
	int		get_error();

	// If a call fails, call this to get the ASCII error message
	std::string get_error_str() {return m_error_str;}

	// Call this to close the socket
	void	close();

	// This is a thread-safe "gethostbyname()"
	bool	get_host_by_name(std::string server_name, in_addr_t* ip_addr);

	// Returns the file-descriptor of this socket
	int		get_fd() {return m_sd;}


protected:

	// The file descriptor of our socket
	int		    m_sd;

	// 'true' if Create() has been called
	bool	    m_is_created;

	// If a call fails, the error string goes here
	std::string m_error_str;
	int		    m_error;

	// Information about the server
	sockaddr_in m_serv_addr;

	// Information about the client
	sockaddr_in m_cli_addr;
};
//=================================================================================================


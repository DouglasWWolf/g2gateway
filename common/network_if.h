//=================================================================================================
// network_if - Defines a class managing a Linux network interface
//=================================================================================================
#pragma once
#include "cppstring.h"
#include "ip_mac.h"

//=================================================================================================
// CNetworkIF() - Defines a network interface
//=================================================================================================
class CNetworkIF
{
public:

    // Call this one to initialize our access to the interface
    void    set_interface_name(PString interface_name);

    // Set the IP address of this network interface
    bool    set_ip_address(sIP ip, bool force = false);

    // Set the IP address of this network interface
    bool    set_ip_address(PString ip, bool force = false);

    // Call these to retrieve this interface's IP and MAC addresses
    sIP     ip()  {return m_ip;}
    sMAC    mac() {return m_mac;}

protected:

    // Call this to read the current settings of the interface
    bool    read_interface();



    // These are the IP and MAC address of our interface
    sIP     m_ip;
    sMAC    m_mac;

    // This is the name of the interface
    PString m_name;
};
//=================================================================================================



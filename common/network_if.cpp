//=================================================================================================
// network_if.cpp - Implements a class managing a Linux network interface
//=================================================================================================
#include <unistd.h>
#include <string.h>
#include "network_if.h"
#include "cprocess.h"


//=================================================================================================
// is_ip_address() - Checks to see if a string represents an IP address
//=================================================================================================
static bool is_ip_address(const char* in)
{
    char buffer[100], *out;

    // Loop through each of the four octets in "aaa.bbb.ccc.ddd"
    for (int octet = 0; octet < 4; ++octet)
    {
        // Fetch the digits of this octet into "buffer"
        out = buffer;
        while (*in >= '0' && *in <= '9') *out++ = *in++;
        *out = 0;

        // If either of the first three octets don't end in '.', this isn't an IP address
        if (octet < 3 && *in != '.') return false;

        // If the octet was an empty string, this isn't an IP address
        if (out == buffer) return false;

        // If the value of this octet is >255, this isn't an IP address
        if (atoi(buffer) > 255) return false;

        // Skip over the '.' between octets
        ++in;
    }

    // If we get here, the input string was a valid IP address
    return true;
}
//=================================================================================================


//=================================================================================================
// find_ip_in_string() - Searches a buffer for a text-string tag, and if it finds it, attempts
//                       to extract an IP address from the characters that follow the tag
//
// Passed: in   = Ptr to the buffer to be searched
//         tag  = Ptr to the string of characters to search for
//         p_ip = Ptr to the sIP object to be filled in if an IP address is found
//=================================================================================================
static bool find_ip_in_string(const char* in, const char* tag, sIP* p_ip)
{
    // Search the buffer for the caller's tag
    char* p = strstr((char*)in, tag);

    // If we didn't find that tag, we don't have our IP address
    if (p == nullptr) return false;

    // Point to the first character after the tag
    p += strlen(tag);

    // Skip over whitespace
    while (*p == ' ' || *p == '\t') ++p;

    // If the string we're pointing to isn't an IP address, we're done
    if (!is_ip_address(p)) return false;

    // Load this string into an sIP object
    p_ip->from(p);

    // And tell the caller we extracted an IP address from this input buffer
    return true;
}
//=================================================================================================


//=================================================================================================
// set_interface_name() - Saves the name of the network interface
//=================================================================================================
void CNetworkIF::set_interface_name(PString interface_name) {m_name = interface_name;}
//=================================================================================================


//=================================================================================================
// read_interface() - Reads information from the network interface
//
// On Entry: m_name = The name of the interface (usually etho0, enp0s3, etc)
//
// Returns:  true if the interface exists, otherwise false
//
// On Exit:  If this returns true, these fields are filled in
//
//           m_ip  = IP address of the interface
//           m_mac = MAC address of the interface
//=================================================================================================
bool CNetworkIF::read_interface()
{
    char     buffer[200], *p;
    CProcess process;

    // We don't yet have a MAC or an IP
    bool     have_mac = false;
    bool     have_ip  = false;

    // We don't yet know what IP address we are
    m_ip.from("0.0.0.0");

    // Get the output of "ifconfig <interface_name>"
    process.open(false, "ifconfig %s", m_name.c());

    // Start fetching lines of output from the command we just ran
    while (process.get_line(buffer, sizeof buffer))
    {
        // If we don't yet have a MAC address, look for one
        if (!have_mac)
        {
            // Look for the tag that says "This is the MAC address"
            p = strstr(buffer, "HWaddr ");

            // If the "HWaddr" tag existed, extract the MAC address
            if (p)
            {
                m_mac.from(p+7);
                have_mac = true;
                continue;
            }
        }

        // If we don't yet have a MAC address, look for one
        if (!have_mac)
        {
            // Look for the tag that says "This is the MAC address"
            p = strstr(buffer, "ether ");

            // If the "ether " tag existed, extract the MAC address
            if (p)
            {
                m_mac.from(p+6);
                have_mac = true;
                continue;
            }
        }

        // Look for an IP address
        if (!have_ip) have_ip = find_ip_in_string(buffer, "inet addr:", &m_ip);
        if (!have_ip) have_ip = find_ip_in_string(buffer, "inet ",      &m_ip);

        // If we found both a MAC and an IP address, we can stop looking
        if (have_mac && have_ip) break;
    }

    // Tell the caller whether or not we found the network interface
    return have_mac;
}
//=================================================================================================


//=================================================================================================
// set_ip_address() - Sets the IP address of the network interface
//=================================================================================================
bool CNetworkIF::set_ip_address(PString ip_string, bool force)
{
    sIP new_ip;
    new_ip.from(ip_string);
    return set_ip_address(new_ip);
}
bool CNetworkIF::set_ip_address(sIP new_ip, bool force)
{
    CProcess   process;
    char       iface[20];
    sIP        netmask, network;
    char       ch_new_ip[20];
    char       ch_netmask[20];
    char       ch_network[20];

    // We use a fixed netmask
    netmask.from("255.255.255.0");

    // Create the network address by applying the netmask to the new IP address
    network = new_ip;
    for (int i=0; i<4; ++i) network.octet[i] &= netmask.octet[i];

    // Get a displayable string of the name of our interface
    strcpy(iface, m_name.c());

    // Get a displayable string of the new IP address we're trying to set
    strcpy(ch_new_ip, new_ip.to_string().c());

    // Get a displayable string of the netmask
    strcpy(ch_netmask, netmask.to_string().c());

    // Get a displayable string of the network address
    strcpy(ch_network, network.to_string().c());

    // Find out what our current MAC and IP is
    read_interface();

    // If these are already our current settings, we're done
    if (!force && m_ip == new_ip)
    {
        printf("IP is already %s\n", ch_new_ip);
        return true;
    }

    // Bring the interface down
    process.run(true, "ifconfig %s down", iface);

    // Change the IP address of this network interface
    process.run(true, "ifconfig %s %s netmask %s 2>&1", iface, ch_new_ip, ch_netmask);

    // Make sure we have a UDP broadcast route!
    process.run(true, "route add -net 255.255.255.255 netmask 255.255.255.255 dev %s", iface);

    // And make sure our packets have a route to this interface
    process.run(true, "route add -net %s netmask %s dev %s", ch_network, ch_netmask, iface);

    // Bring the interface back up
    process.run(true, "ifconfig %s up", iface);

    // Wait a second for the interface to come back up
    sleep(1);

    // Fetch the current IP and MAC address
    read_interface();

    // Tell the world whether we were able to change the address
    return m_ip == new_ip;
}
//=================================================================================================





//=================================================================================================
// from_str() - Converts a dotted-octet string to an sIP object
//=================================================================================================
void sIP::from(const char* p)
{
    // Loop through each octet
    for (int i=0; i<4; ++i)
    {
        // Skip over the dots
        if (i)
        {
            p = strchr(p, '.');
            if (p == nullptr) return;
            ++p;
        }

        // Convert this ASCII octet to binary
        octet[i] = atoi(p);
    }
}
//=================================================================================================


//=================================================================================================
// from() - Fills in our octets from a string
//=================================================================================================
void sMAC::from(const char* p)
{
    // Loop through each octet
    for (int i=0; i<6; ++i)
    {
        // Skip over the octet separators
        if (i)
        {
            p = strchr(p, ':');
            if (p == nullptr) return;
            ++p;
        }

        // Convert this ASCII octet to binary
        octet[i] = strtoul(p, nullptr, 16);
    }
}
//=================================================================================================


//=================================================================================================
// to_string() - Formats a MAC address in ASCII
//=================================================================================================
PString sMAC::to_string()
{
    PString result;

    result.format
    (
        "%02X:%02X:%02X:%02X:%02X:%02X",
        octet[0], octet[1], octet[2],
        octet[3], octet[4], octet[5]
    );

    return result;
}
//=================================================================================================


//=================================================================================================
// to_string() - Formats an IP address in ASCII
//=================================================================================================
PString sIP::to_string()
{
    PString result;
    result.format("%i.%i.%i.%i", octet[0], octet[1], octet[2], octet[3]);
    return result;
}
//=================================================================================================




//=================================================================================================
// Comparison operators for sMAC and sIP
//=================================================================================================
bool sIP::operator!=(const sIP& other) const
{
    return memcmp(octet, other.octet, 4) != 0;
}

bool sMAC::operator!=(const sMAC& other) const
{
    return memcmp(octet, other.octet, 6) != 0;
}

bool sIP::operator==(const sIP& other) const
{
    return memcmp(octet, other.octet, 4) == 0;
}

bool sMAC::operator==(const sMAC& other) const
{
    return memcmp(octet, other.octet, 6) == 0;
}
//=================================================================================================



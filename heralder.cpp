//=================================================================================================
// heralder.cpp- Implements thread that is responsible for transmitting periodic "Herald" packets.
//=================================================================================================
#include <unistd.h>
#include <string.h>
#include "globals.h"
#include "common.h"

//=================================================================================================
// main() - Sits in a loop, transmitting heralds every two seconds
//=================================================================================================
void CHeralder::main(void* p1, void* p2, void* p3)
{
    // Build the intial herald message
    build_herald();

    // We're not suspended (i.e., it's safe to send heralds)
    m_is_silent = false;

    // Create an outgoing UDP socket that is bound to our network interface
    create_new_socket();

    // We're (potentially) going to transmit herald packets forever
    while (true)
    {
        // If heralding is turned on, send one
        if (m_is_heralding) send();

        // Default time between heralds is 2 seconds
        sleep(2);
    }
}
//=================================================================================================




//=================================================================================================
// create_new_socket() - Creates a new UDP socket bound to our network interface
//=================================================================================================
void CHeralder::create_new_socket()
{
    // Only one thread at a time may use the heralding socket
    m_heralding_cs.lock();

    // Get rid of the old heralder socket
    delete m_p_socket;

    // Create a new heralder socket
    m_p_socket = new UDPSocket;

    // Create a socket that can broadcast UDP packet
    m_p_socket->create_sender(1217);

    // Bind this socket to the network interface we're using
    m_p_socket->bind_to(Network.ip().to_string());

    // Unlock the heralding socket
    m_heralding_cs.unlock();
}
//=================================================================================================


//=================================================================================================
// Send() - Any thread may call this to send a herald
//=================================================================================================
void CHeralder::send(int port)
{
    UDPSocket sock, *p_sock;

    // If we're suspended, don't transmit anything!
    if (m_is_silent) return;

    // No one is allowed to delete the socket while we have this locked
    m_heralding_cs.lock();

    // If the caller wants to send the herald to a non-standard port...
    if (port)
    {

        // Create a socket that can broadcast UDP packets
        sock.create_sender(port);
        sock.bind_to(Network.ip().to_string());

        // And point to this temporary socket we just created
        p_sock = &sock;
    }

    // Otherwise, just point to our already created socket
    else p_sock = m_p_socket;

    // Make sure no one updates the herald while we're sending it
    m_herald_cs.lock();

    // Send this herald
    p_sock->send(&m_herald, sizeof m_herald);

    // Allow other threads to update the herald
    m_herald_cs.unlock();

    // Other threads can now delete our socket if they need to
    m_heralding_cs.unlock();
}
//=================================================================================================




//=================================================================================================
// build_herald() - Builds the herald that we'll be transmitting
//=================================================================================================
void CHeralder::build_herald()
{
    int instrument_sn;

    // Make sure no one access the herald structure while we're modifying it
    m_herald_cs.lock();

    // Fetch the instrument serial number
    Config.get(SPEC_INSTRUMENT_SN, &instrument_sn);

    // To start out, clear all of the fields
    memset(&m_herald, 0, sizeof m_herald);

    // Type '1' marks this message as a herald
    m_herald.type = CHCP_HERALD;

    // Fill in the MAC address with our current MAC address
    m_herald.mac = Network.mac();

    // This is Herald version #2.  (Version #2 adds the gateway serial number)
    m_herald.version = 2;

    // Fill in the IP address with our current IP address
    m_herald.be_ip = Network.ip();

    // We have no special features that we want to advertise
    m_herald.flags = 0;

    // Fill in the current block letter
    m_herald.block_letter = Instrument.letter;

    // Fill in the firmware version
    m_herald.fw_major = Instrument.fw_major;
    m_herald.fw_minor = Instrument.fw_minor;
    m_herald.fw_build = Instrument.fw_build;

    // Fill in the serial number
    m_herald.be_serial_num = instrument_sn;

    // This gateway is in the G2 family
    m_herald.family = FAMILY_G2;

    // Allow other threads to accesss the herald
    m_herald_cs.unlock();
}
//=================================================================================================




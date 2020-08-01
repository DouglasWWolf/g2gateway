
//=================================================================================================
// main.cpp - Mainline code for the G2 gateway
//=================================================================================================
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <vector>
#include <string>
#include <algorithm>
#include "globals.h"
#include "history.h"
#include "common.h"
using std::vector;
using std::string;

//=================================================================================================
// parse_firmware_version() - Parses the firmware version into the Instrument structure
//=================================================================================================
void parse_firmware_version()
{
    int version = VERSION;

    Instrument.fw_major = version / 10000; version %= 10000;
    Instrument.fw_minor = version / 100 ;  version %= 100;
    Instrument.fw_build = version;
}
//=================================================================================================


//=================================================================================================
// setup_network() Determines which network interface we'll use and sets its initial IP address
//=================================================================================================
bool setup_network()
{
    PString default_ip;
    PString iface_name = "eth0";
    bool force;

    // Find out the name of the network interface we're using
#if !defined(__arm__)
    if (!Config.get(SPEC_INTERFACE, &iface_name))
    {
        printf("Missing spec \"%s\"\n", SPEC_INTERFACE);
        exit(1);
    }
#endif

    // Fetch the list of network interfaces on this machine
    vector<string> ifaces = Network.get_interfaces();

    // Does our desired network interface exist?
    auto it = std::find(ifaces.begin(), ifaces.end(), iface_name);

    // If our desired network interface doesn't exist, use the local loop-back interface
    if (it == ifaces.end())
    {
        iface_name = "lo";
        default_ip = "127.0.0.1";
        force      = false;
    }

    // If our desired network interface exists, lookup the IP address we should configure it to
    else
    {
        // We don't yet know what ip address we will default to
        bool have_default_ip = false;

        // Try to fetch the default IP from the config file
        have_default_ip = Config.get(SPEC_DEFAULT_IP, &default_ip);

        // If we still don't have a default IP, use this hard-coded one
        if (!have_default_ip) default_ip = "10.11.14.254";

        // For the network interface configure this IP address, even if we're already using it
        force = true;
    }

    // Tell the engineer which network we'll be using
    printf("Using network interface %s on address %s\n", iface_name.c(), default_ip.c());

    // Tell our network object which network interface we'll be using
    Network.set_interface_name(iface_name);

    // Set our network interface to the address we're supposed to startup with
    return Network.set_ip_address(default_ip, force);
}
//=================================================================================================


//=================================================================================================
// init() - Reads the configuration file and initializes all global objects
//=================================================================================================
void init()
{
    PString str;

    // Make sure that the physical memory region we need is mapped
    if (!MM.is_mapped())
    {
        printf("Memory map failed!\n");
        exit(1);
    }

    // Initialize the FIFO we use to communicate with the firmware on the Nios-II
    CommFifo.init(MM);

    // Read in the configuration file
    if (!Config.load())
    {
        printf("Missing configuration file %s\n", Config.filename());
        exit(1);
    }

    // Set up the network interface we're going ot use to communicate
    setup_network();

    // Populate the "Instrument" object with the version of this software
    parse_firmware_version();

}
//=================================================================================================


//=================================================================================================
// launch_servers() - Launches all the TCP servers and waits for them to initialize
//=================================================================================================
void launch_servers()
{
    int i;

    // Launch all of the servers
    for (i=0; i<MAX_GXIP_SERVERS; ++i)
    {
        Server[i].set_slot(i-1);
        Server[i].spawn();
    }

    // Wait for all of the servers to be ready
    for (i=0; i<MAX_GXIP_SERVERS; ++i)
    {
        while (!Server[i].is_initialized()) usleep(100000);
    }
}
//=================================================================================================


#include "altera_peripherals.h"
#include "socsubsystem.h"
//=================================================================================================
// main() - Execution starts here
//=================================================================================================
int main(int argc, char** argv)
{

    printf("Resetting\n");
    pio_t* reset = (pio_t*) MM[NIOS_RESET_BASE];
    reset->dir =1;

    reset->data = 1;
    usleep(100);
    reset->data = 0;

    usleep(100);
    reset->data = 1;
    usleep(100);
    reset->data = 0;

    // Make sure that writing to a closed socket doesn't cause the program to exit
    signal(SIGPIPE, SIG_IGN);

    // Read in our spec-file and initialize all of our global objects
    init();

    // Launch the thread that listens for messages from the firmware
    FWListener.spawn();

    // Launch and wait for the servers to all come up
    launch_servers();

    // Launch the thread that will emit herald messsages every couple of second
    Heralder.spawn();

    // Spawn the CHCP message handler
    CHCP.spawn();

    // And this thread does nothing forever
    while (true) pause();
}
//=================================================================================================

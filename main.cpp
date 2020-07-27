
//=================================================================================================
// main.cpp - Mainline code for the G2 gateway
//=================================================================================================
#include <unistd.h>
#include <stdio.h>
#include "globals.h"
#include "history.h"
#include "common.h"


//=================================================================================================
// parse_firmware_version() - Parses the firmware version into the Instrument structure
//=================================================================================================
void parse_firmware_version()
{
    int version = VERSION;

    Instrument.fw_major = version / 1000; version %= 1000;
    Instrument.fw_minor = version / 100 ; version %= 100;
    Instrument.fw_build = version;
}
//=================================================================================================


//=================================================================================================
// set_initial_ip() - Change our IP address to the address we're supposed to start with
//=================================================================================================
bool set_initial_ip()
{
    PString default_ip;

    // We don't yet know what ip address we will default to
    bool have_default_ip = false;

    // Try to fetch the default IP from the config file
    have_default_ip = Config.get(SPEC_DEFAULT_IP, &default_ip);

    // If we still don't have a default IP, use this hardcoded one
    if (!have_default_ip) default_ip = "10.11.14.254";

    // Set our network interface to the address we're supposed to startup with
    return Network.set_ip_address(default_ip, true);
}
//=================================================================================================


//=================================================================================================
// init() - Reads the configuration file and initializes all global objects
//=================================================================================================
void init()
{
    PString str;

    // Make sure that the memory map worked
#if defined(__arm__)
    if (!MM.is_mapped())
    {
        printf("Memory map failed!\n");
        exit(1);
    }
#endif

    // Read in the configuration file
    if (!Config.read_file())
    {
        printf("Missing configuration file %s\n", Config.filename());
        exit(1);
    }

    // Find out the name of the network interface we're using
#if defined(__arm__)
    str = "eth0";
#else
    if (!Config.get(SPEC_INTERFACE, &str))
    {
        printf("Missing spec \"%s\"\n", SPEC_INTERFACE);
        exit(1);
    }
#endif

    // Make sure we can initialize that network interface
    Network.set_interface_name(str);

    // Set the network interface to our starting IP address
    if (!set_initial_ip())
    {
        printf("Failed to set the initial IP address\n");
        exit(1);
    }

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



//=================================================================================================
// main() - Execution starts here
//=================================================================================================
int main(int argc, char** argv)
{
    // Read in our spec-file and initialize all of our global objects
    init();

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

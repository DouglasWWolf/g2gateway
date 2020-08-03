
//=================================================================================================
// main.cpp - Mainline code for the G2 gateway
//=================================================================================================
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <vector>
#include <string>
#include <algorithm>
#include <string.h>
#include "globals.h"
#include "history.h"
#include "common.h"
#include "filesys.h"
using std::vector;
using std::string;

//=================================================================================================
// parse_firmware_version() - Parses the firmware version into the Instrument structure
//=================================================================================================
void parse_firmware_version()
{
    char* p = VERSION_BUILD;

    Instrument.fw_major = atoi(p);
    p = strchr(p+1, '.') + 1;
    Instrument.fw_minor = atoi(p);
    p = strchr(p+1, '.') + 1;
    Instrument.fw_build = atoi(p);
}
//=================================================================================================


//=================================================================================================
// setup_network() Determines which network interface we'll use and sets its initial IP address
//
// On Entry: Instrument.net_iface = The name of the network interface we're supposed to use
//
// On Exit:  The network interface has been configured with an IP address
//=================================================================================================
bool setup_network()
{
    PString default_ip;
    bool force;

    // Fetch the list of network interfaces on this machine
    vector<string> ifaces = Network.get_interfaces();

    // Does our desired network interface exist?
    auto it = std::find(ifaces.begin(), ifaces.end(), Instrument.net_iface);

    // If our desired network interface doesn't exist, use the local loop-back interface
    if (it == ifaces.end())
    {
        Instrument.net_iface = "lo";
        default_ip = "127.0.0.1";
        force      = false;
    }

    // If our desired network interface exists, lookup the IP address we should configure it to
    else
    {
        // We don't yet know what ip address we will default to
        bool have_default_ip = false;

        // Try to fetch the default IP from the config file
        have_default_ip = EEPROM.get(SPEC_DEFAULT_IP, &default_ip);

        // If we still don't have a default IP, use this hard-coded one
        if (!have_default_ip) default_ip = "10.11.14.254";

        // For the network interface configure this IP address, even if we're already using it
        force = true;
    }

    // Tell the engineer which network we'll be using
    printf("Using network interface %s on address %s\n", Instrument.net_iface.c(), default_ip.c());

    // Tell our network object which network interface we'll be using
    Network.set_interface_name(Instrument.net_iface);

    // Set our network interface to the address we're supposed to startup with
    return Network.set_ip_address(default_ip, force);
}
//=================================================================================================


//=================================================================================================
// read_config() - Reads in the configuration file
//
// On Exit:  Instrument.net_iface = The name of our network interface
//           Instrument.sandbox   = The directory name of a writable RAM-disk
//           EEPROM object has been configured as a device or as a file
//=================================================================================================
void read_config()
{
    PString str, eeprom_device;

    // This is the name of our configuration file
    const char* config_fn = "../g2gateway.cfg";

    // Our config file is a regular file in the file-system
    Config.configure_as_file(config_fn);

    // If we're missing our configuration file, complain
    if (!Config.load())
    {
        fprintf(stderr, "missing configuration file %s\n", config_fn);
        exit(1);
    }

    // Find out the name of our network interface
    if (Config.get(SPEC_INTERFACE, &str))
        Instrument.net_iface = str;
    else
        Instrument.net_iface = "eth0";

    try
    {
        // Find out what the name of our EEPROM device is
        if (!Config.get(SPEC_EEPROM_DEVICE, &eeprom_device)) throw SPEC_EEPROM_DEVICE;

        // Find out if our EEPROM device is, in fact, a file in the file-system
        if (!Config.get(SPEC_EEPROM_IS_FILE, &str)) throw SPEC_EEPROM_IS_FILE;

        // Parse that string to determine whether it's a file
        bool is_file = (str == "true" || str == "TRUE" || str == "1");

        // Configure our EEPROM object as either a file or as a physical EEPROM device
        if (is_file)
            EEPROM.configure_as_file(eeprom_device);
        else
            EEPROM.configure_as_eeprom(eeprom_device, 0x1000, "CPHD01");

        // Find out where our writable sandbox is
        if (!Config.get(SPEC_SANDBOX, &Instrument.sandbox)) throw SPEC_SANDBOX;

    }
    catch(const char* p)
    {
        fprintf(stderr, "config file missing spec %s\n", p);
        exit(1);
    }

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
    if (!EEPROM.load())
    {
        printf("Missing eeprom file or device %s\n", EEPROM.filename());
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

    // Start the download manager
    DLM.spawn();

    // Launch all of the normal command/request servers
    for (i=0; i<MAX_GXIP_SERVERS; ++i)
    {
        Server[i].set_slot(i);
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
#include "cppstring.h"
void test();

//=================================================================================================
// main() - Execution starts here
//=================================================================================================
int main(int argc, char** argv)
{
    // Tell the engineer what the current working directory is
    printf("Started from %s\n", get_cwd().c());

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

    // Read our configuration file
    read_config();

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

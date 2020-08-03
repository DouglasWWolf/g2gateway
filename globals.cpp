//=================================================================================================
// globals.cpp - Declares global objects available to every source file
//=================================================================================================
#include "globals.h"
#include "socsubsystem.h"
#include "common.h"
#include "history.h"

// Config file manager
CUpdSpec     Config;

// The EEPROM file manager
CUpdSpec     EEPROM;

// Memory map manager
CMemMap      MM(HW_REGS_BASE, HW_REGS_SPAN);

// FIFO to the Nios-II
CFpgaFifo    CommFifo;

// Manages the network interface
CNetworkIF   Network;

// Thread that is responsible for sending outgoing heralds
CHeralder    Heralder;

// Thread that listens for and handles incoming CHCP message
CCHCP        CHCP;

// These are the servers that handle GXIP messages
CServer      Server[MAX_GXIP_SERVERS];
CServer&     MainServer = Server[0];

// Listens for and dispatches handshakes and responses from the firmware to the host
CFWListener  FWListener;

// The gateway download manager
CDLM         DLM;

// This holds information about this instrument such as IP address, MAC, serial number, etc
instrument_t Instrument;



// Utilities search for this string in our executable to find out what version it is
const char* exe_string = "EXEVERSION " VERSION_BUILD;

//=================================================================================================
// get_live_sites() - In a regular gateway, this would be a bitmap of which slots have GX modules
//                    attached to them.   Since we have a dedicated module, just say that only the
//                    first slot has a module in it
//=================================================================================================
int get_live_sites() {return 1;}
//=================================================================================================


void remount_rw() {}
void remount_ro() {}

//=================================================================================================
// get_cwd() - Returns the name of the current working directory
//=================================================================================================
PString get_cwd()
{
    char buffer[256];

    // Find out the name of the current working directory
    getcwd(buffer, sizeof buffer);

    // And hand the resulting string to the caller
    return buffer;
}
//=================================================================================================



//=================================================================================================
// This is the structure of the beginning of one of our standard package headers
//=================================================================================================
#pragma pack(push, 1)
struct pkg_header_t
{
    u64     magic;
    u32be   hdr_version;
    u32be   file_size;
};
#pragma pack(pop)
//=================================================================================================

//=================================================================================================
// file_exists() - Verifies that a file exists
//=================================================================================================
bool file_exists(const char* filename)
{
    FILE* ifile = fopen(filename, "r");
    if (ifile == nullptr) return false;
    fclose(ifile);
    return true;
}
//=================================================================================================



//=================================================================================================
// copy_file() - Copies a file from one place to another.   If the source file has a recognized
//               package header, that header will be stripped off of the destination file
//=================================================================================================
bool copy_file(const char* source_fn, const char* dest_fn, bool strip)
{
    u8 buffer[4096];
    int  chunk_number = 0;

    // Open the input file
    FILE* ifile = fopen(source_fn, "rb");
    if (ifile == nullptr) return false;

    // Create the output file
    FILE* ofile = fopen(dest_fn, "wb");
    if (ofile == nullptr)
    {
        fclose(ifile);
        return false;
    }

    // We're prepared to read in a really big file
    u32 bytes_remaining = 0xFFFFFFFF;

    // While we have bytes still to read...
    while (bytes_remaining)
    {
        // We're going to keep track of how many chunks we read
        ++chunk_number;

        // The first chunk is going to be the size of a package header
        u32 chunk_size = (chunk_number == 1) ? 512 : sizeof buffer;

        // Make sure we never try to read a chunk larger than the number of bytes we have left
        if (chunk_size > bytes_remaining) chunk_size = bytes_remaining;

        // Read in a chunk
        int bytes_read = fread(buffer, 1, chunk_size, ifile);

        // If there's no data left to read, we're done
        if (bytes_read == 0) break;

        // If this is the first block of data and we're supposed to strip package headers
        if (chunk_number == 1 && strip && bytes_read == 512)
        {
            // Map a package header over the data we just read in
            pkg_header_t& header = *(pkg_header_t*)buffer;

            // If this file is a standard package file...
            if (header.magic == 0xDEADACDCDCACADDEULL)
            {
                bytes_remaining = header.file_size;
                continue;
            }
        }

        // Write the data to the destination file
        fwrite(buffer, 1, bytes_read, ofile);

        // Keep track of how many bytes we have left to read
        bytes_remaining -= bytes_read;
    }

    // Close the files, we're done with them
    fclose(ifile);
    fclose(ofile);

    // And tell the caller that his file got copies
    return true;
}
//=================================================================================================




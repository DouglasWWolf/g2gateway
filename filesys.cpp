//=================================================================================================
// filesys.cpp - provides filesystem support functions
//=================================================================================================
#include <unistd.h>
#include <string.h>
#include "filesys.h"
#include "cprocess.h"
#include "globals.h"

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
// parent_dir() - Returns the parent directory of a filename or directory
//=================================================================================================
PString parent_dir(const char* filename)
{
    char buffer[256];
    strcpy(buffer, filename);
    char* p = strrchr(buffer, '/');
    if (p) *p = 0;
    return buffer;
}
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




//=================================================================================================
// remount() - Remounts the file-system as read-only or read-write
//=================================================================================================
void remount(const char* type)
{
    CProcess process;
    char device[512];

    // If we're not allowing the file system to be locked, do nothing
    if (strcmp(type, "ro") == 0 && !Instrument.lock_fs) return;

    // Run a "mount" command and fetch the first line
    process.open(false, "mount | grep \" on / \"");
    process.get_line(device, sizeof device);

    //--------------------------------------------------------------------
    // The first token contains the mount device of the root file-system
    //--------------------------------------------------------------------

    // Make sure this is really the entry for the root file-system
    if (strstr(device, "on / ") == NULL) return;

    // Place a null after the device name
    *strchr(device, ' ') = 0;

    // And remount the root file-system
    process.run(true, "sync;sync;mount -o remount,%s %s /", type, device);
}
void remount_rw()
{
    remount("rw");
}
void remount_ro()
{
#if defined(__arm__)
    remount("ro");
#endif
}
//=================================================================================================



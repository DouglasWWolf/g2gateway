#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "cppstring.h"
#include "cprocess.h"


//=================================================================================================
// get_other_bank() - Get the directory name of the bank that we did *not* boot from
//=================================================================================================
PString get_other_bank()
{
    char buffer[256];

    // Find out the name of the current working directory
    getcwd(buffer, sizeof buffer);

    // Get a pointer to the last character of that string
    char* last_char = strchr(buffer, 0) - 1;

    // Change a '1' to a '0' and vice-versa
    *last_char = 97 - *last_char;

    // And hand the resulting string to the caller
    return buffer;
}
//=================================================================================================


//=================================================================================================
// prepark_other_bank() - Prepare the other bank for a software download
//=================================================================================================
void prepare_other_bank()
{
    CProcess process;

    // Get the path to the software bank we did *not* just boot from
    PString folder = get_other_bank();

    // Get a handy char* to that folder name
    const char* cfolder = folder.c();

    // Make sure the folder exists
    mkdir(folder.c(), 0777);

    // Make sure it's readable/writable and empty
    process.run("chmod 777 %s; rm -rf %s/*", cfolder, cfolder);
}
//=================================================================================================


//=================================================================================================
// update_gateway_software() - Performs a software upgrade on the gateway
//
// Passed: the name of the tarball that we should unpack. We expect to find a file called
//         install.sh in that tarball
//=================================================================================================
bool update_gateway_software(PString tarball)
{
    int      rc;
    char     buffer[300];
    CProcess process;

    // Copy the name of the tarball into our buffer
    strcpy(buffer, tarball);

    // Strip off the filename to leave just the name of the folder
    char* ptr = strrchr(buffer, '/');
    if (ptr) *ptr = 0;

    // This is the name of the folder where the tarball lives
    PString folder = buffer;

    // This is the name of the install file
    PString install_sh = folder + "/install.sh";

    // Go untar the tarball
    rc = process.run(true, "cd %s && tar xf %s", folder.c(), tarball.c());

    // Remove the tarball, we don't need it anymore
    remove(tarball.c());

    // If unpacking the tarball failed, tell the caller
    if (rc != 0) return false;

    // Make sure that the installer script is executable
    rc = process.run(true, "chmod 777 %s", install_sh.c());

    // If the installer doesn't exist, tell the caller
    if (rc != 0) return false;

    // Run the installer
    rc = process.run(true, install_sh.c());

    // And tell the caller whether the installation worked
    return (rc == 0);
}
//=================================================================================================




void test()
{
    //prepare_other_bank();
    if (update_gateway_software("/system/gateway/bank_1/tarball.tgz")) exit(21);

}

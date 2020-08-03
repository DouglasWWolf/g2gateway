//=================================================================================================
// memmap.cpp - Implements an object that maps physical memory into user space
//=================================================================================================
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "memmap.h"

//=================================================================================================
// open() - Opens /dev/mem and maps the requested region into user-space
//=================================================================================================
bool CMemMap::open(off_t where, size_t length)
{
    // Close any open map
    close();

    // This is the Linux device that allows access to physical memory
    m_fd = ::open("/dev/mem", O_RDWR | O_SYNC);

    // If we weren't able to open it, something had gone terribly wrong
    if (m_fd < 0) return false;

    // Map the specified region of memory into user space
    m_base = mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, where);

    // Did that call to mmap work properly?
    m_is_mapped = (m_base != MAP_FAILED);

    // If the memory map failed, close /dev/mem
    if (!m_is_mapped) close();

    // Tell the caller whether this worked
    return m_is_mapped;
}
//=================================================================================================


//=================================================================================================
// operator[] - Returns a userspace pointer to the specified address in physical memory
//=================================================================================================
void* CMemMap::operator[](unsigned int address) {return (void*) ((char*)m_base + address);}
//=================================================================================================


//=================================================================================================
// close() - Closes the memory map
//=================================================================================================
void CMemMap::close()
{
    if (m_fd != -1) ::close(m_fd);
    m_fd = -1;
    m_is_mapped = false;
    m_base = nullptr;
}
//=================================================================================================


//=================================================================================================
// Constructor() - Initializes us to an unmapped condition
//=================================================================================================
CMemMap::CMemMap()
{
    m_fd = -1;
    close();
}
//=================================================================================================


//=================================================================================================
// Constructor() - Initializes us to a mapped condition
//=================================================================================================
CMemMap::CMemMap(off_t where, size_t length)
{
    // We don't yet have a file descriptor for /dev/mem
    m_fd = -1;

    // We don't yet have any memory mapped
    close();

    // Attempt to map the requested memory
    open(where, length);
}
//=================================================================================================


//=================================================================================================
// Destructor() - Closes the memory map
//=================================================================================================
CMemMap::~CMemMap() {close();}
//=================================================================================================


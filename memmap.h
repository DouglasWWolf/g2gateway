//=================================================================================================
// memmap.h - Defines an object that maps physical memory into user space
//=================================================================================================
#pragma once
#include <sys/types.h>

class CMemMap
{
public:

    // Constructors and destructor
    CMemMap();
    CMemMap(off_t where, size_t length);
    ~CMemMap();


    // Call this to map the physical memory
    bool    open(off_t where, size_t length);

    // Closes /dev/mem.  Called automatically by destructor
    void    close();

    // Find out if we have physical memory mapped into user space
    bool    is_mapped() {return m_is_mapped;}

    // Returns a user-space pointer to a physical address
    void*   operator[](unsigned int address);

protected:

    // File descriptor to the /dev/mem device
    int     m_fd;

    // Pointer to where our physical memory is mapped into user-space
    void*   m_base;

    // This will be true when we have physical memory mapped into user space
    bool    m_is_mapped;
};



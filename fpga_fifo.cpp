//=================================================================================================
// fpga_fifo.cpp - Implements a bidirectional 32-bit wide FIFO to the NIOS-II core
//=================================================================================================
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include "fpga_fifo.h"
#include "sopcinfo.h"

//=================================================================================================
// These are the types of messages we can send on the FIFO
//=================================================================================================
enum
{
    FIFO_MSG_STRING = 0,
    FIFO_MSG_GXIP   = 1
};
//=================================================================================================


//=================================================================================================
// This defines a FIFO control/status register
//=================================================================================================
#define ALTERA_FIFO_CSR_FULL        (1 << 0)
#define ALTERA_FIFO_CSR_EMPTY       (1 << 1)
#define ALTERA_FIFO_CSR_ALMOSTFULL  (1 << 2)
#define ALTERA_FIFO_CSR_ALMOSTEMPTY (1 << 3)
#define ALTERA_FIFO_CSR_OVERFLOW    (1 << 4)
#define ALTERA_FIFO_CSR_UNDERFLOW   (1 << 5)

struct altera_fifo_csr
{
    uint32_t fill_level;
    uint32_t i_status;
    uint32_t interuptenable;
    uint32_t almostfull;
    uint32_t almostempty;
};
//=================================================================================================



// The is used to map our private structure to a variable called m
#define access() pv& m = *(pv*)m_pv

//=================================================================================================
// This structure contains our private / protected variables
//=================================================================================================
struct pv
{
    // This is where we write data to the output FIFO
    volatile uint32_t* p_data_out;

    // This is where we read data from the input FIFO
    volatile uint32_t* p_data_in;

    // This points to the control/status register for the input FIFO
    volatile altera_fifo_csr* p_ctrl_in;

};
//=================================================================================================


//=================================================================================================
// Constructor() - Allocates private variables
//=================================================================================================
CFpgaFifo::CFpgaFifo()
{
    // How large is the structure that contains our private variables?
    size_t size = sizeof(pv);

    // Allocate our private variable structure
    m_pv = malloc(size);

    // The entire structure starts out cleared to zeros
    memset(m_pv, 0, size);
}
//=================================================================================================


//=================================================================================================
// Destructor() - Frees the private variable structure
//=================================================================================================
CFpgaFifo::~CFpgaFifo()
{
    if (m_pv) free(m_pv);
    m_pv = nullptr;
}
//=================================================================================================


//=================================================================================================
// init() - Initializes our access to the FIFOs
//=================================================================================================
bool CFpgaFifo::init(CMemMap& mm)
{
    // If the memory-mapper doesn't have the memory mapped, complain!
    if (!mm.is_mapped()) return false;

    // Provide access to our private variables
    access();

    // Fetch a pointer to where we write the data to the output FIFO
    m.p_data_out = (uint32_t*)mm[H2F_FIFO_DATA];

    // Fetch a pointer to where we read the data from the input FIFO
    m.p_data_in  = (uint32_t*)mm[F2H_FIFO_DATA];

    // Fetch a pointer to the input FIFO control registers
    m.p_ctrl_in  = (altera_fifo_csr*)mm[F2H_FIFO_CSR];

    // Drain the incoming message queue just in case is has garbage in it
    while (m.p_ctrl_in->fill_level) *m.p_data_in;

    // Tell the caller that all is well
    return true;
}
//=================================================================================================


//=================================================================================================
// send_generic() - Sends any arbitrary message
//=================================================================================================
void CFpgaFifo::send_generic(int message_type, int byte_count, const void* buffer)
{
    uint32_t word = 0;

    // Provide access to our private variables
    access();

    // Convert the pointer to the input buffer to a byte pointer
    unsigned char* ptr = (unsigned char*)buffer;

    // How many full 32-bit words are in that string?
    int word_count = byte_count  >> 2;

    // Make sure we account for the partially full 32-bit word at the end
    if (byte_count & 3) ++word_count;

    // Write the message type to the FIFO
    *m.p_data_out = message_type;

    // Write the number of 32-bit words to follow
    *m.p_data_out = word_count;

    // Convert that pointer to an address so we can examine it
    uint64_t address = (uint64_t) ptr;

    // If the buffer is on a 32-bit boundary, shovel the data into the FIFO the fast way
    if ((address & 3) == 0)
    {

        uint32_t* word_ptr = (uint32_t*) ptr;
        while (word_count--) *m.p_data_out = *word_ptr++;
    }

    // Otherwise, the buffer isn't on a 32-bit boundary, so we have to do this the slow way
    else while (word_count--)
    {
        // Pack the next 4 bytes of the input string into a 32-bit little-endian word
        word >>= 8; word |= (*ptr++ << 24);
        word >>= 8; word |= (*ptr++ << 24);
        word >>= 8; word |= (*ptr++ << 24);
        word >>= 8; word |= (*ptr++ << 24);

        // And write that word to the FIFO
        *m.p_data_out = word;
    }
}
//=================================================================================================



//=================================================================================================
// send_string() - Used to send a character string message to the Nios-II
//=================================================================================================
void CFpgaFifo::send_string(const char* ptr)
{
    send_generic(FIFO_MSG_STRING, strlen(ptr)+1, ptr);
}
//=================================================================================================


//=================================================================================================
// send_gxip() - Sends a GXIP message to the Nios-II
//=================================================================================================
void CFpgaFifo::send_gxip(gxip_packet_t& message)
{
    send_generic(FIFO_MSG_GXIP, message.length(), &message);
}
//=================================================================================================




//=================================================================================================
// is_message_waiting() - Returns 'true' if there is an incoming message waiting
//=================================================================================================
bool CFpgaFifo::is_message_waiting()
{
    // Provide access to our private variables
    access();

    // If we have more than two words in the FIFO, assume it has a message
    return (m.p_ctrl_in->fill_level > 2);
}
//=================================================================================================


//=================================================================================================
// wait_for_message() - Waits up to a specified number of milliseconds for a message to arrive
//=================================================================================================
bool CFpgaFifo::wait_for_message(int timeout_ms)
{
    // These define the unit of time in between checks for a waiting message
    static const int time_unit_msec = 20;
    static const int time_unit_usec = time_unit_msec * 1000;

    // Is there a message waiting right now?
    bool is_a_message_waiting = is_message_waiting();

    // If we're not supposed to wait around, just tell the caller whether there's a message waiting
    if (timeout_ms == 0 || is_a_message_waiting) return is_a_message_waiting;

    // Otherwise, start counting down
    while (timeout_ms >= 0)
    {
        timeout_ms -= time_unit_msec;
        usleep(time_unit_usec);
        if (is_message_waiting()) return true;
    }

    // If we get here, a message never arrived
    return false;
}
//=================================================================================================


//=================================================================================================
// read_message() - Reads a message from the incoming FIFO
//
// Returns:  true if there was a message read, otherwise false
//
// On Exit:  If there was a message waiting:
//              class variable msg_length = The number of 32 bit words in the payload
//              class variable msg_type   = Which kind of message (string, or GX command?)
//              class variable payload    = The message payload
//=================================================================================================
bool CFpgaFifo::read_message(int timeout_ms)
{

    // If there's no message waiting in the pipe, we're done
    if (!wait_for_message(timeout_ms)) return false;

    // Provide access to our private variables
    access();

    // Fetch the message type
    msg_type = *m.p_data_in;

    // Fetch the number of 32-bit words in the payload
    msg_length = *m.p_data_in;

    // We don't yet know how many words are in the FIFO
    int words_in_pipe = 0;

    // This is the number of 32-bit words we're going to read from the FIFO
    int i = msg_length;

    // Get a point to the payload area as though it were 32-bit words
    int32_t* out = (int32_t*)payload;

    // So long as we have words left to read in our message
    while (i--)
    {
        // Wait for there to be words in the pipe available for reading
        while (words_in_pipe == 0)
        {
            words_in_pipe = m.p_ctrl_in->fill_level;
        }

        // Read this word from the FIFO and put it in the payload buffer
        *out++ = *m.p_data_in;

        // And now we have one fewer words in the FIFO
        words_in_pipe--;
    }

    // Tell the caller that they have a message waiting
    return true;
}
//=================================================================================================






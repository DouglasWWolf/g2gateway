#ifndef _ALTERA_PERIPHERALS_H
#define _ALTERA_PERIPHERALS_H

#include <unistd.h>
#include <stdint.h>





typedef struct _sysid {                                                                                               
        uint32_t id;                                                                                                  
        uint32_t timestamp;                                                                                           
} sysid_t;                                                                                                            

typedef struct _pio {
     uint32_t data;
     uint32_t dir;
     uint32_t intmask;
     uint32_t edge;
     uint32_t outset;
     uint32_t outclear;
} pio_t;

#define SPIM_STAT_ROE (1 << 3)
#define SPIM_STAT_TOE (1 << 4)
#define SPIM_STAT_TMT (1 << 5)
#define SPIM_STAT_TRDY (1 << 6)
#define SPIM_STAT_RRDY (1 << 7)
#define SPIM_STAT_ERR (1 << 8)
#define SPIM_STAT_EOP (1 << 9)

typedef struct _spim {
        uint32_t rxdata;
    uint32_t txdata;
    uint32_t status;
    uint32_t control;
        uint32_t rsvd;
    uint32_t slaveselect;
    uint32_t eop;
} spimregs_t;

static inline int32_t spim_wr_rd(volatile spimregs_t*  spim, uint16_t outword) {
	// clear status bits
	spim->status = 0;
	usleep(10);
	if (!(spim->status & SPIM_STAT_TRDY)) {
		return -1;
	}
	// set SPIM output register
    spim->txdata = outword; 
	// wait for SPIM to signal output complete
	while (!(SPIM_STAT_TMT & spim->status)) { 
        usleep(10); 
    }
	// wait for SPIM status to indicate receive complete
	while (!(SPIM_STAT_RRDY & spim->status)) { 
        usleep(10); 
    }
	return spim->rxdata;
}

#define ALTERA_UART_PE      (1 << 0)  // parity error
#define ALTERA_UART_FE      (1 <<  1)  // framing error
#define ALTERA_UART_BRK     (1 <<  2)  // break detected
#define ALTERA_UART_ROE     (1 <<  3)  // receive overrun error
#define ALTERA_UART_TOE     (1 <<  4)  // transmit overrun error
#define ALTERA_UART_TMT     (1 <<  5)  // transmit empty
#define ALTERA_UART_TRDY    (1 <<  6)  // transmit ready
#define ALTERA_UART_RRDY    (1 <<  7)  // receive ready
#define ALTERA_UART_E       (1 <<  8)  // exception (any above error)
#define ALTERA_UART_DCTS    (1 << 10)  // detected CTS change
#define ALTERA_UART_CTS     (1 << 11)  // current CTS state
#define ALTERA_UART_EOP     (1 << 12)  // eop detected

#define ALTERA_UART_CTL_INT_PE      (1 <<  0) // enable interrupt for parity error
#define ALTERA_UART_CTL_INT_FE      (1 <<  1) // enable interrupt for framing error
#define ALTERA_UART_CTL_INT_BRK     (1 <<  2) // enable interrupt for break detected
#define ALTERA_UART_CTL_INT_ROE     (1 <<  3) // enable interrupt for receive overrun
#define ALTERA_UART_CTL_INT_TOE     (1 <<  4) // enable interrupt for transmit overrun
#define ALTERA_UART_CTL_INT_TMT     (1 <<  5) // enable interrupt for TMT
#define ALTERA_UART_CTL_INT_TRDY    (1 <<  6) // enable interrupt for TRDY
#define ALTERA_UART_CTL_INT_RRDY    (1 <<  7) // enable interrupt for RRDY
#define ALTERA_UART_CTL_INT_E       (1 <<  8) // enable interrupt for E
#define ALTERA_UART_CTL_INT_DCTS    (1 << 10) // enable interrupt for CTS change
#define ALTERA_UART_CTL_INT_EOP     (1 << 12) // enable interrupt for EOP detected

#define ALTERA_UART_CTL_TBDRK       (1 <<  9) // send break
#define ALTERA_UART_CTL_RTS         (1 << 11) // send RTS_N

typedef struct _altera_uart {
    uint32_t rxdata;
    uint32_t txdata;
    uint32_t status;
    uint32_t control;
    uint32_t divisor;   // not present unless enabled in Quartus
    uint32_t eop;       // not present unless enabled in Quartus
} altera_uart_t;

#define ALTERA_FIFO_CSR_FULL        (1 << 0)
#define ALTERA_FIFO_CSR_EMPTY       (1 << 1)
#define ALTERA_FIFO_CSR_ALMOSTFULL  (1 << 2)
#define ALTERA_FIFO_CSR_ALMOSTEMPTY (1 << 3)
#define ALTERA_FIFO_CSR_OVERFLOW    (1 << 4)
#define ALTERA_FIFO_CSR_UNDERFLOW   (1 << 5)

typedef struct _altera_fifo_csr {
    uint32_t fill_level;
    uint32_t i_status;
    uint32_t interuptenable;
    uint32_t almostfull;
    uint32_t almostempty;
} altera_fifo_csr_t;

#endif // _ALTERA_PERIPHERALS_H

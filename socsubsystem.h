#ifndef _SOCSUBSYSTEM
#define _SOCSUBSYSTEM

#include "altera_peripherals.h"

#define HW_REGS_BASE ( 0xFF200000 )
#define HW_REGS_SPAN ( 0x00200000 )

#define SYSID_BASE      (0x00000000)
#define NIOS_SRAM_BASE  (0x00020000)
#define NIOS_SRAM_SIZE  (0x00020000)
#define NIOS_RESET_BASE (0x00040000)
#define NIOS_UART_BASE  (0x00000040)

#define H2F_BASE        (0x00062020)
#define H2F_CSR_BASE    (0x00062000)
#define F2H_BASE        (0x00061020)
#define F2H_CSR_BASE    (0x00061000)

#define SCAN_CTL_BASE   (0x00041000)
#define SCAN_STAT_BASE  (0x00041010)
#define SCAN_UART_BASE  (0x00041020)

#define CART_DOOR_BASE  (0x00045000)

//#define FLED_GREEN 0x01
//#define FLED_RED 0x02

#endif // _SOCSUBSYSTEM

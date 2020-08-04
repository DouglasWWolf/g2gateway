#pragma once

/*
 *  This file was generated from the .sopcinfo via parsesopc
 */


// <connection name="hps_0_arm_a9_0.altera_axi_master/hps_0_bridges.axi_h2f_lw"><parameter name="baseAddress"><value>
#define HW_REGS_BASE          (0xff200000)

// 0x00200000
#define HW_REGS_SPAN          (0x00200000)

// <connection name="hps_0_bridges.h2f_lw/h2f_pipe.in_csr"><parameter name="baseAddress"><value>
#define H2F_FIFO_CSR          (0x00062000)

// <connection name="hps_0_bridges.h2f_lw/h2f_pipe.in"><parameter name="baseAddress"><value>
#define H2F_FIFO_DATA         (0x00062020)

// <connection name="hps_0_bridges.h2f_lw/f2h_pipe.out_csr"><parameter name="baseAddress"><value>
#define F2H_FIFO_CSR          (0x00061000)

// <connection name="hps_0_bridges.h2f_lw/f2h_pipe.out"><parameter name="baseAddress"><value>
#define F2H_FIFO_DATA         (0x00061020)

// <module name="hps_0"><interface name="h2f_lw_axi_master"><memoryBlock name="NIOS_RESET.s1"><baseAddress>
#define NIOS_RESET_PIO        (16)

#ifndef PW_HARDWARE_H
#define PW_HARDWARE_H

/* Memory-mapped H8 registers. Evaluate these volatile lvalues only in the
 * target address space. */
#if defined(PW_RENESAS_H8) || defined(__H8__) || defined(__HITACHI__)
#include "startup/iodefine.h"
#endif

/* SSER byte masks for enabling and disabling the transmitter and receiver. */
#define SSU_TX_ENABLE 0x80
#define SSU_TX_RX_ENABLE 0xc0
#define SSU_TX_RX_CLEAR 0x3f

/* MSB-first SPI mode 3, selected while the shared SSU is disabled. */
#define SSU_MODE3_MAIN_DIV4 0x86
#define SSU_MODE3_SUB_DIV2 0x87

#endif

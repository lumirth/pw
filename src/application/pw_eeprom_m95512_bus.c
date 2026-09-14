#include "types.h"
#include "startup/hardware.h"
#include "project.h"
#include "application/pw_eeprom_m95512.h"
#include "support/lib_common.h"

/* Wait until the transmit holding register accepts the next byte. */
#pragma inline(EepromWaitReady)
static void EepromWaitReady(void)
{
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
}

/* Disable the shared SSU while selecting its clock source. Both settings use
 * MSB-first SPI mode 3: 0x86 selects main clock/4, 0x87 subclock/2. */
void EepromConfigure(void)
{
  CKSTPR2.BYTE |= 0x10;
  SSU.SSER.BYTE = 0;
  if (g_state.events.bits.lowPowerClock) {
    SSU.SSMR.BYTE = SSU_MODE3_MAIN_DIV4;
  } else {
    SSU.SSMR.BYTE = SSU_MODE3_SUB_DIV2;
  }
}

/* Restore the shared bus to transmit-only operation on main clock/4. */
void EepromIdle(void)
{
  SSU.SSER.BYTE = SSU_TX_ENABLE;
  SSU.SSMR.BYTE = SSU_MODE3_MAIN_DIV4;
}

/* Wait for a received byte or overrun, then read SSRDR. An overrun clears the
 * hardware flag and sets eepromError, which stays set until the transfer owner
 * clears it. The register read supplies the result in either case. */
u8 EepromReceive(void)
{
  u8 received;

  for (;;) {
    if (SSU.SSSR.BIT.ORER) {
      SSU.SSSR.BIT.ORER = 0;
      g_state.events.bits.eepromError = 1;
      break;
    }
    if (SSU.SSSR.BIT.RDRF) {
      break;
    }
  }
  received = SSU.SSRDR;
  return received;
}

/* Write a byte, retrying after receive overruns. The final loop test leaves
 * the sampled SystemEvents byte in CH38's return register. Callers consume
 * that byte through the target's implicit return; there is no C return
 * expression. */
u8 EepromWriteByte(u16 address, u8 value)
{
  u8 attempts;
  u8 status;
  u8 addrHi;

  attempts = EEPROM_TRANSFER_ATTEMPTS;
  g_state.events.bits.eepromError = 0;
  addrHi = (address >> 8);
  while (attempts != 0) {
    WatchdogService();
    EepromConfigure();
    SSU.SSER.BYTE &= SSU_TX_RX_CLEAR;
    SSU.SSSR.BYTE = 0;
    SSU.SSER.BYTE |= SSU_TX_RX_ENABLE;
    IO.PDR1.BIT.B2 = 0;
    EepromWaitReady();
    SSU.SSTDR = EEPROM_CMD_READ_STATUS;
    EepromReceive();
    do {
      EepromWaitReady();
      SSU.SSTDR = 0xff;
      status = EepromReceive();
      status &= 1;
    } while (status == 1);
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B2 = 1;
    SSU.SSER.BYTE &= SSU_TX_RX_CLEAR;
    SSU.SSSR.BYTE = 0;
    SSU.SSER.BIT.TE = 1;
    IO.PDR1.BIT.B2 = 0;
    EepromWaitReady();
    SSU.SSTDR = EEPROM_CMD_WRITE_ENABLE;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B2 = 1;
    IO.PDR1.BIT.B2 = 0;
    EepromWaitReady();
    SSU.SSTDR = EEPROM_CMD_PAGE_PROGRAM;
    status = addrHi;
    EepromWaitReady();
    SSU.SSTDR = status;
    status = address;
    EepromWaitReady();
    SSU.SSTDR = status;
    status = value;
    EepromWaitReady();
    SSU.SSTDR = status;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B2 = 1;
    EepromIdle();
    if (g_state.events.bits.eepromError == 0) {
      break;
    }
    attempts--;
  }
}

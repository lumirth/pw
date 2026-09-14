#include "types.h"
#include "startup/hardware.h"
#include "application/pw_accel_bma150.h"

/* The BMA150 uses the H8 synchronous serial unit in four-wire mode. Chip select
 * is active low on PDR9 bit 0. SSSR bits 2, 1 and 3 indicate transmit-empty,
 * receive-full and transfer-end. */

#define ACCEL_REG_CHIP_ID 0x00
#define ACCEL_REG_CONF1 0x0B
#define ACCEL_REG_CONF2 0x15
#define ACCEL_REG_PROTECTED_1E 0x1E

#define ACCEL_READ_FLAG 0x80
/* EE_W opens the protected register window, including address 0x1E. */
#define ACCEL_PROTECTED_ACCESS_ENABLE 0x10

/* Read consecutive BMA150 registers. A dummy byte clocks each result after
 * the address response is discarded. */
u8 AccelRead(u8 registerAddress, u8 *destination, u8 byteCount)
{
  u8 received;

  SSU.SSSR.BYTE &= 4;
  SSU.SSER.BYTE = SSU_TX_RX_ENABLE;
  registerAddress |= ACCEL_READ_FLAG;
  IO.PDR9.BYTE &= 0xfe;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = registerAddress;
  while (SSU.SSSR.BIT.RDRF == 0) {
  }
  (void)SSU.SSRDR;
  do {
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 0xff;
    while (SSU.SSSR.BIT.RDRF == 0) {
    }
    received = SSU.SSRDR;
    *destination = received;
    destination = destination + 1;
  } while (--byteCount != 0);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR9.BYTE |= 1;
  SSU.SSER.BYTE = SSU_TX_ENABLE;
  return 0;
}

/* The caller supplies a write address with bit 7 clear. */
void AccelWrite(u8 registerAddress, u8 value)
{
  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR9.BYTE &= 0xfe;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = registerAddress;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = value;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR9.BYTE |= 1;
  return;
}

/* Select four-wire SPI before reading the identity. A matching chip receives
 * +/-2 g range and 1500 Hz filter bandwidth, preserving calibration bits 7..5.
 * Register 0x1E bit 7 is set only while protected access is enabled. */
u8 AccelInit(void)
{
  u8 scratch[2];

  SSU.SSSR.BYTE &= 4;
  AccelWrite(ACCEL_REG_CONF2, 0x80); /* four-wire SPI */
  AccelRead(ACCEL_REG_CHIP_ID, scratch, 2);

  if ((scratch[0] & 7) != 2) {
    IO.PDR9.BYTE |= 1;
    return 0;
  }
  AccelRead(ACCEL_REG_RANGE_BANDWIDTH, scratch, 1);
  scratch[0] =
      ((scratch[0] & ACCEL_CALIBRATION_BITS) | ACCEL_RANGE_2G_BANDWIDTH_1500HZ);
  AccelWrite(ACCEL_REG_RANGE_BANDWIDTH, scratch[0]);
  AccelWrite(ACCEL_REG_CONF1, 0);
  AccelWrite(ACCEL_REG_CONTROL, ACCEL_PROTECTED_ACCESS_ENABLE);
  AccelRead(ACCEL_REG_PROTECTED_1E, scratch, 1);
  scratch[0] = (scratch[0] | 0x80);
  AccelWrite(ACCEL_REG_PROTECTED_1E, scratch[0]);
  AccelWrite(ACCEL_REG_CONTROL, ACCEL_CONTROL_AWAKE);
  IO.PDR9.BYTE |= 1;
  return 1;
}

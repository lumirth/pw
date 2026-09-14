#ifndef PW_ACCEL_BMA150_H
#define PW_ACCEL_BMA150_H

#include "types.h"

#define ACCEL_REG_X_LSB 0x02
#define ACCEL_REG_CONTROL 0x0A
#define ACCEL_REG_RANGE_BANDWIDTH 0x14
#define ACCEL_CONTROL_AWAKE 0
#define ACCEL_CONTROL_SLEEP 1
#define ACCEL_CALIBRATION_BITS 0xe0
#define ACCEL_RANGE_2G_BANDWIDTH_1500HZ 6
#define ACCEL_RANGE_4G_BANDWIDTH_25HZ 8

/* Read 1..255 consecutive registers through the configured shared SSU.
 * The byte counter encodes 256 transfers as zero. Destination must cover the
 * complete transfer; every completed read returns zero. */
u8 AccelRead(u8 registerAddress, u8 *destination, u8 byteCount);
/* Write one register through the configured SSU; address bit 7 must be clear.
 */
void AccelWrite(u8 registerAddress, u8 value);
/* Recognize and configure the BMA150 for +/-2 g and 1500 Hz filter bandwidth.
 * Return nonzero after configuration, or zero for an unrecognized chip ID. */
u8 AccelInit(void);

#endif

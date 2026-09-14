#ifndef PW_SELFTEST_H
#define PW_SELFTEST_H

#include "types.h"

/* Reset time to midnight and test whether stable seconds have advanced. */
u8 RtcStartupCheck(void);
/* Select +/-4 g and 25 Hz filter bandwidth, then send one XYZ high-byte sample
 * through the LCD chip select. Returns zero if chip recognition fails. */
u8 FactoryAccelDump(void);
/* Send the raw ADC average to the fixture, then protect and store it in both
 * mirrors. Return shared event flags after the write. */
u8 FactoryBatteryCalibrate(void);
/* Preserve the LCD-readback readiness byte carried by the IR view. */
void DiagnosticsInit(void);
/* Advance tests and navigation using holds counted by DiagnosticsRender. */
void DiagnosticsUpdate(void);
/* Draw the current diagnostic screen and increment its capped hold count. */
void DiagnosticsRender(void);
/* Reset acquisition and batch counts, then load motion thresholds from EEPROM
 * and enable motion processing. CaptureSample and MotionProcess perform the
 * subsequent measurements. */
void ThresholdInit(void);
void ThresholdUpdate(void);
void ThresholdRender(void);
void ThresholdFailureRender(void);
void BootSignatureWrite(void);
u8 BootSignatureValid(void);

#endif

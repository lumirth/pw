#ifndef PW_BATTERY_H
#define PW_BATTERY_H

#include "types.h"

/* Pack the reference's low 12 bits with their nibble-sum checksum in bits
 * 12..15. */
uint BatteryProtect(uint value);
/* Return nonzero when a stored reference's four-bit checksum agrees. */
u8 BatteryVerify(u16 value);
/* Enable the measurement circuit, average eight channel-7 conversions in
 * right-aligned ADC units, then switch the circuit off. */
uint BatterySample(void);
/* Loads and repairs the stored threshold, replacing an invalid protected
 * value with zero. Compares a fresh ADC average against scaleFactor/20 of it.
 */
u8 BatteryCheckLow(u16 scaleFactor);
/* Consume the pending battery check and replace the battery-low event flag. */
void BatteryUpdate(void);

#endif

#ifndef PW_RTC_H
#define PW_RTC_H

#include "types.h"

/* Service latched minute, hour and day updates once IR releases them. Hour/day
 * work writes EEPROM and reuses scratch; interrupts can merge into one flag.
 */
void RtcDispatch(void);
/* Set time of day modulo 24 hours, retaining the elapsed counter. */
void RtcSetTime(u32 seconds);
/* Return matching packed-BCD S:M:H snapshots after waiting on busy bits. */
void RtcReadStable(u8 *secondOut, u8 *minuteOut, u8 *hourOut);

#endif

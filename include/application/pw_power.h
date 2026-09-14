#ifndef PW_POWER_H
#define PW_POWER_H

#include "types.h"

#define INTERACTIVE_DISPLAY_SECONDS 60
#define INTERACTIVE_MOTION_SECONDS 90
#define INPUT_DISPLAY_SECONDS 90
#define ACTIVITY_MOTION_SECONDS 30

/* Resume regular sampling in motion mode with a 30-second timeout. */
void MotionSessionWake(void);
/* Return nonzero when the three-axis sum of absolute changes from the
 * preceding ring sample exceeds 30 signed-8-bit sample units. */
u8 MotionActivityCheck(void);
/* Enter interactive mode and wake the display, with 60-second display and
 * 90-second motion timeouts. InputScan refreshes the display timeout to
 * 90 seconds on later button edges. */
void MotionSessionStart(void);
/* On motion timeout, disable Timer B1 and quarter-second refresh. Later
 * foreground wakes still acquire samples for MotionActivityCheck. */
void MotionSessionIdleCheck(void);
/* Clamp and update the RAM copy of the lifetime total. */
void StoreTotalSteps(u32 value);

#endif

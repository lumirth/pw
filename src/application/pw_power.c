#include "types.h"
#include "startup/iodefine.h"
#include "project.h"
#include "application/pw_buzzer.h"
#include "application/pw_nt7508.h"
#include "application/pw_power.h"

/* Timer B1 resumes regular sampling while the display stays asleep.
 * The one-second RTC interrupt maintains the motion timeout. */
void MotionSessionWake(void)
{
  CKSTPR1.BYTE |= 4;
  g_state.flags.byte =
      ((g_state.flags.byte & SYSTEM_MODE_CLEAR) | SYSTEM_MODE_MOTION);
  RTC.RTCCR2.BYTE |= 1;
  g_state.idleSeconds[IDLE_MOTION] = ACTIVITY_MOTION_SECONDS;
  g_state.events.byte &= EVENT_CLEAR(EVENT_MOTION);
}

#define MOTION_ACTIVITY_THRESHOLD 0x1E

/* Each absolute difference reads the volatile sample operands twice: once for
 * the sign test and once for the selected result. */
#define ABS(value) ((value) >= 0 ? (value) : -(value))

u8 MotionActivityCheck(void)
{
  u8 previous;
  int activity;

  previous = ((g_state.sampleIndex + 0x3f) & 0x3f);
  activity =
      ABS(g_work.motion.x[g_state.sampleIndex] - g_work.motion.x[previous]);
  activity +=
      ABS(g_work.motion.y[g_state.sampleIndex] - g_work.motion.y[previous]);
  activity +=
      ABS(g_work.motion.z[g_state.sampleIndex] - g_work.motion.z[previous]);
  if ((uint)activity > MOTION_ACTIVITY_THRESHOLD) {
    return 1;
  }
  return 0;
}

/* Set the display timeout to 60 seconds and the motion timeout to 90 seconds.
 * Entering interactive mode also wakes the screen. */
void MotionSessionStart(void)
{
  g_state.idleSeconds[IDLE_DISPLAY] = INTERACTIVE_DISPLAY_SECONDS;
  g_state.idleSeconds[IDLE_MOTION] = INTERACTIVE_MOTION_SECONDS;
  if ((g_state.flags.byte & SYSTEM_MODE_MASK) != SYSTEM_MODE_INTERACTIVE) {
    if ((g_state.flags.byte & SYSTEM_MODE_MASK) == SYSTEM_MODE_INACTIVE) {
      g_state.sampleIndex = 0;
    }
    g_state.flags.byte =
        ((g_state.flags.byte & SYSTEM_MODE_CLEAR) | SYSTEM_MODE_INTERACTIVE);
    RTC.RTCCR2.BYTE |= 1;
    DisplayExitPowerSave();
  }
}

/* Disable regular sampling wakes and quarter-second refresh. The one-second
 * RTC interrupt continues waking MainTick, whose samples detect new activity.
 */
void MotionSessionEnd(void)
{
  BeepDisableTimer();
  CKSTPR1.BYTE &= 0xfb;
  RTC.RTCCR2.BYTE &= 0xfe;
  g_state.flags.byte &= SYSTEM_MODE_CLEAR;
}

void MotionSessionIdleCheck(void)
{
  if (g_state.idleSeconds[IDLE_MOTION] == 0) {
    MotionSessionEnd();
  }
}

#pragma interrupt(IRQ0Interrupt(vect = 16))
void IRQ0Interrupt(void)
{
  g_state.events.byte |= EVENT_CENTER_PRESS;
  g_state.buttonWake[0] = 1;
  CKSTPR1.BYTE |= 4;
  IRR1.BYTE &= 0xfe;
}

#pragma interrupt(IRQ1Interrupt(vect = 17))
void IRQ1Interrupt(void)
{
  IRR1.BYTE &= 0xfd;
}

#pragma interrupt(IRQAECInterrupt(vect = 18))
void IRQAECInterrupt(void)
{
  IRR1.BYTE &= 0xfb;
}

#pragma interrupt(ADCInterrupt(vect = 38))
void ADCInterrupt(void)
{
  IRR2.BYTE &= 0xbf;
}

/* Store a lifetime total capped at seven display digits. */
void StoreTotalSteps(u32 value)
{
  (void)g_state.save.totalSteps;
  if (value >= TOTAL_STEPS_MAX) {
    value = TOTAL_STEPS_MAX;
  }
  g_state.save.totalSteps = value;
}

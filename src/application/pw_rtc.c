#include "flags.h"
#include "types.h"
#include "startup/iodefine.h"
#include "project.h"
#include "application/pw_diary.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_rtc.h"
#include "support/scratch.h"

void PokemonMinuteTick(void);
void RtcHourUpdate(void);
void RtcDayRollover(void);

#define ELAPSED_DAYS_MAX 9999
#define SECONDS_PER_MINUTE 60
#define HOURS_PER_DAY 24
#define RTC_ONE_HOUR_SECONDS 3600
#define RTC_PENDING_MINUTE 1
#define RTC_PENDING_HOUR 2
#define RTC_PENDING_DAY 4

/* Service latched time updates when the IR-request flag is clear. Repeated
 * interrupts coalesce into one pending flag. Test each flag in order so the
 * hour handler can request a day rollover in the same dispatch. */
void RtcDispatch(void)
{
  SystemEvents flags;

  flags.byte = g_state.events.byte;
  if (!flags.bits.irRequested) {
    if ((g_state.time.pendingUpdates & RTC_PENDING_MINUTE) != 0) {
      PokemonMinuteTick();
    }
    if ((g_state.time.pendingUpdates & RTC_PENDING_HOUR) != 0) {
      RtcHourUpdate();
    }
    if ((g_state.time.pendingUpdates & RTC_PENDING_DAY) != 0) {
      RtcDayRollover();
    }
    g_state.time.pendingUpdates &= 0xf8;
  }
}

void PokemonMinuteTick(void)
{
  if (g_state.save.pokemonMinutes != 0xffffu) {
    g_state.save.pokemonMinutes++;
  }
}

/* Persist elapsedHours before recording this hour in the diary. The counter
 * stops when it or totalSteps reaches 9,999,999. The configured BCD rollover
 * hour marks the start of each activity day. */
void RtcHourUpdate(void)
{
  Course *course;

  g_state.events.byte |= EVENT_BATTERY_CHECK;
  if ((g_state.save.totalSteps < TOTAL_STEPS_MAX) &&
      (g_state.save.elapsedHours < ELAPSED_HOURS_MAX)) {
    g_state.save.elapsedHours++;
  }
  EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                    (u8 *)&g_state.save, sizeof(SaveData));

  if (g_state.flags.bits.hasPokemon) {
    ScratchReset();
    course = ScratchAlloc(sizeof(Course));
    EepromRead(EEPROM_COURSE, course, sizeof(Course));
    DiaryAppend(course, ScratchAlloc(sizeof(DiaryEntry)),
                PW_DIARY_ACTION_PERIODIC_STEPS, g_state.save.bonusCourse, 0, 0);
  }

  g_state.hourSteps = 0;
  if (g_state.rolloverHourBcd == g_state.time.hourBcd24h) {
    g_state.time.pendingUpdates |= RTC_PENDING_DAY;
  }
}

/* Persist the elapsed day, insert today at the front of the seven-day step
 * history, then clear today's count. Clear device IDs in the ten peer-history
 * records, keeping their other fields and the received staging record. */
void RtcDayRollover(void)
{
  WalkData *data;
  u16 historyBytes;
  u32 *history;
  u32 *source;
  u32 *destination;
  u8 shifted;
  PeerRecords *record;
  u8 remaining;

  /* Use the WalkData layout to calculate serial EEPROM addresses. */
  data = (WalkData *)EEPROM_WALK;
  historyBytes = sizeof(data->dailySteps);
  if (g_state.save.days < ELAPSED_DAYS_MAX) {
    g_state.save.days++;
  }
  EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                    (u8 *)&g_state.save, sizeof(SaveData));

  ScratchReset();
  history = ScratchAlloc(historyBytes);
  EepromRead((u16)&data->dailySteps, history, historyBytes);

  /* Shift backward so the overlapping source entries survive each copy. */
  source = history + (STEP_HISTORY_DAYS - 2);
  destination = history + (STEP_HISTORY_DAYS - 1);
  shifted = 0;
  do {
    *destination = *source;
    shifted++;
    source--;
    destination--;
  } while (shifted < (STEP_HISTORY_DAYS - 1));
  *history = g_state.dailySteps;

  EepromWrite((u16)&data->dailySteps, history, historyBytes);
  g_state.dailySteps = 0;

  record = (PeerRecords *)EEPROM_PEER_RECORDS;
  remaining = PEER_HISTORY_SLOTS;
  do {
    record++;
    EepromFill((u16)&record->deviceId, sizeof(record->deviceId), 0xff);
    remaining--;
  } while (remaining != 0);
}

/* Packed BCD stores the tens digit in the high nibble. */
#pragma inline(BinaryToBcd)
static u8 BinaryToBcd(u8 value)
{
  u8 result = ((value / 10) * (u16)16);
  result |= value % 10;
  return result;
}

/* Set time of day modulo 24 hours; elapsed rtcSeconds is a separate counter.
 * Stop and reset the RTC before its ordered S:M:H writes, then enable second,
 * minute, hour and quarter-second interrupts. */
void RtcSetTime(u32 seconds)
{
  const u32 sixty = SECONDS_PER_MINUTE;
  u8 secondBcd;
  u8 minuteBcd;
  u8 hourBcd;
  u8 component;

  component = (seconds % sixty);
  secondBcd = BinaryToBcd(component);
  seconds = seconds / sixty;
  component = (seconds % sixty);
  minuteBcd = BinaryToBcd(component);
  seconds = seconds / sixty;
  component = (seconds % HOURS_PER_DAY);
  hourBcd = BinaryToBcd(component);

  g_state.time.hourBcd24h = hourBcd;
  g_state.time.minuteBcd = minuteBcd;
  g_state.time.secondBcd = secondBcd;

  CKSTPR1.BIT.RTCCKSTP = 1;
  RTC.RTCCR1.BIT.RUN = 0;
  RTC.RTCCR1.BIT.RST = 1;
  RTC.RTCCR1.BIT.RST = 0;
  RTC.RSECDR.BYTE = secondBcd;
  RTC.RMINDR.BYTE = minuteBcd;
  RTC.RHRDR.BYTE = hourBcd;
  RTC.RTCCR1.BIT.HR24 = 1;
  RTC.RTCCR1.BIT.INT = 1;
  RTC.RTCCR2.BYTE = 0x1c;
  RTC.RTCCR2.BIT._025SEIE = 1;
  RTC.RTCCR1.BIT.RUN = 1;
}

/* Wait for each register's BSY bit to clear. Repeat pairs of S:M:H snapshots
 * until they agree, yielding a consistent time across register updates. */
void RtcReadStable(u8 *secondOut, u8 *minuteOut, u8 *hourOut)
{
  u8 snapshot[6];
  u8 sample;

  do {
    sample = 0;
    do {
      while (RTC.RSECDR.BIT.BSY) {
      }
      snapshot[sample * 3] = RTC.RSECDR.BYTE;
      while (RTC.RMINDR.BIT.BSY) {
      }
      snapshot[sample * 3 + 1] = RTC.RMINDR.BYTE;
      while (RTC.RHRDR.BIT.BSY) {
      }
      snapshot[sample * 3 + 2] = RTC.RHRDR.BYTE;
      sample++;
    } while (sample < 2);
  } while ((snapshot[0] != snapshot[3]) || (snapshot[1] != snapshot[4]) ||
           (snapshot[2] != snapshot[5]));

  *secondOut = snapshot[0];
  *minuteOut = snapshot[1];
  *hourOut = snapshot[2];
}

/* A pending refresh bit can merge several quarter-second interrupts. */
#pragma interrupt(RtcQuarterSecondInterrupt(vect = 23))
void RtcQuarterSecondInterrupt(void)
{
  g_state.events.byte |= EVENT_UI_REFRESH;
  RTC.RTCFLG.BYTE &= 0xfe;
}

#pragma interrupt(RtcHalfSecondInterrupt(vect = 24))
void RtcHalfSecondInterrupt(void)
{
  RTC.RTCFLG.BYTE &= 0xfd;
}

/* The one-second interrupt maintains these counters independently of frames. */
#pragma interrupt(RtcSecondInterrupt(vect = 25))
void RtcSecondInterrupt(void)
{
  u8 second;
  u16 socialSeconds;

  second = RTC.RSECDR.BYTE;
  if ((second & 0x80) == 0) {
    g_state.time.secondBcd = second;
  }
  g_state.save.rtcSeconds++;
  socialSeconds = (g_state.socialElapsedSeconds + 1);
  if (socialSeconds > RTC_ONE_HOUR_SECONDS) {
    socialSeconds = RTC_ONE_HOUR_SECONDS;
  }
  g_state.socialElapsedSeconds = socialSeconds;
  if (g_state.idleSeconds[IDLE_DISPLAY] != 0) {
    g_state.idleSeconds[IDLE_DISPLAY]--;
  }
  if (g_state.idleSeconds[IDLE_MOTION] != 0) {
    g_state.idleSeconds[IDLE_MOTION]--;
  }
  RTC.RTCFLG.BYTE &= 0xfb;
}

#pragma interrupt(RtcMinuteInterrupt(vect = 26))
void RtcMinuteInterrupt(void)
{
  u8 minute;

  minute = RTC.RMINDR.BYTE;
  if ((minute & 0x80) == 0) {
    g_state.time.minuteBcd = minute;
  }
  g_state.time.pendingUpdates |= RTC_PENDING_MINUTE;
  RTC.RTCFLG.BYTE &= 0xf7;
}

#pragma interrupt(RtcHourInterrupt(vect = 27))
void RtcHourInterrupt(void)
{
  u8 hour;

  hour = RTC.RHRDR.BYTE;
  if ((hour & 0x80) == 0) {
    g_state.time.hourBcd24h = RTC.RHRDR.BYTE;
  }
  g_state.time.pendingUpdates |= RTC_PENDING_HOUR;
  RTC.RTCFLG.BYTE &= 0xef;
}

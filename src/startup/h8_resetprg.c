#include "types.h"
#include "startup/iodefine.h"
#include "project.h"
#include "application/pw_diary.h"
#include "application/pw_cutscenes.h"
#include "application/pw_accel_bma150.h"
#include "application/pw_battery.h"
#include "application/pw_buzzer.h"
#include "application/pw_nt7508.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_factory_test.h"
#include "support/ir.h"
#include "application/pw_friend.h"
#include "application/pw_home.h"
#include "application/pw_main.h"
#include "application/pw_pedometer.h"
#include "application/pw_player_input.h"
#include "application/pw_power.h"
#include "support/lib_clear.h"
#include "support/lib_common.h"
#include "startup/h8_rominfo.h"
#include "application/pw_rtc.h"
#include "support/scratch.h"
#include "application/pw_selftest.h"
#include "application/pw_ssu_init.h"
#include "startup/h8_resetprg.h"
#include "application/pw_storage.h"

/* The stack starts at the end of section S, above the static on-chip RAM
 * allocations. */
#pragma stacksize 0x8C
#include <machine.h>
#include <stddef.h>

/* Timer B1 wakes the foreground task from sleep. */
void TimerB1Init(void)
{
  CKSTPR1.BYTE |= 4; /* enable Timer B1's clock */
  TB1.TMB1.BYTE = 0xbf;
  TB1.TCB1 = 0xf8;
  IRR2.BYTE &= 0xfb;     /* clear the wake request */
  IENR2.BYTE |= 4;       /* enable the wake interrupt */
  TB1.TMB1.BYTE |= 0x40; /* start Timer B1 */
}

void WalkStartCommit(void);
void WalkEndClear(void);

void ClearWattsInventory(void);

/* Runtime section initialization copies initialized data and clears B. */
void _INITSCT(void);

#define PW_RTC_ONE_HOUR_SECONDS 3600
#define PW_STARTUP_RAM_CLEAR_BYTES 0x3e
#define PW_EEPROM_WATCHDOG_RESET_COUNT (EEPROM_DIAGNOSTIC_LOG + 2)
#define PW_PRNG_SEED_CROSS_RECORD 0x153
#define PW_PRNG_SEED_BYTES 4
#define PW_BATTERY_BOOT_SCALE 0x13

/* Dispatch a queued packet action, including after a communication error.
 * Otherwise select home or the result view from the communication result.
 * After consuming the IR workspace, reset motion and resume the main task. */
void IrComplete(void)
{
  switch (g_work.irc.work.completionAction) {
  case IR_CMD_FACTORY_SETUP:
    DisplayInit();
    g_state.view = VIEW_DIAGNOSTICS;
    DiagnosticsInit();
    goto resumeForeground;
  case IR_CMD_MOTION_TEST_SETUP:
    g_state.view = VIEW_THRESHOLD_TEST;
    ThresholdInit();
    goto resumeForeground;
  case IR_ACTION_FACTORY_RESET:
    DisplayInit();
    g_state.socialElapsedSeconds = PW_RTC_ONE_HOUR_SECONDS;
    PersistentReset(RESET_CLEAR_EVENTS, RESET_CLEAR_LIFETIME);
    goto applyOutputs;
  case IR_CMD_RESET_ALL:
    g_state.socialElapsedSeconds = PW_RTC_ONE_HOUR_SECONDS;
    PersistentReset(RESET_KEEP_EVENTS, RESET_CLEAR_LIFETIME);
    goto applyOutputs;
  case IR_CMD_RESET_KEEP_STEPS:
    g_state.socialElapsedSeconds = PW_RTC_ONE_HOUR_SECONDS;
    PersistentReset(RESET_KEEP_EVENTS, RESET_KEEP_LIFETIME);
  applyOutputs:
    BeepSetOutputMode(g_state.save.volume);
    DisplaySetContrast(g_state.save.contrast);
    goto returnHome;
  case IR_CMD_WALK_START_COMMIT:
    g_state.save.elapsedHours = 0;
    WalkStartCommit();
    goto enterWalkStart;
  case IR_CMD_WALK_END_COMMIT:
    WalkEndClear();
    SetView(VIEW_WALK_END);
    g_ui.view.presentation.stage = WALK_END_BEGIN;
    goto clearPrimary;
  case IR_CMD_WALK_UPDATE_COMMIT:
    WalkStartCommit();
    EepromFill(EEPROM_EVENTS, EEPROM_EVENT_BYTES, 0);
  enterWalkStart:
    SetView(VIEW_WALK_START);
    g_ui.view.presentation.stage = WALK_START_DROP;
    g_ui.view.presentation.frame = 0;
    goto resumeForeground;
  case IR_CMD_GIFT_COLLECTION_COMMIT:
    ClearWattsInventory();
    SetView(VIEW_WALK_END);
    g_ui.view.presentation.stage = WALK_END_COLLECTION_BEGIN;
    goto clearPrimary;
  case IR_CMD_PEER_START:
    SetView(VIEW_PEER);
    PeerFinalize();
    goto resumeForeground;
  case IR_CMD_EVENT_MAP_DONE:
    SetView(VIEW_EVENT_REWARD);
    g_ui.view.presentation.stage = EVENT_REWARD_DROP;
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.rewardKind = PW_EVENT_REWARD_MAP;
    goto resumeForeground;
  case IR_CMD_EVENT_POKEMON_DONE:
    SetView(VIEW_EVENT_REWARD);
    g_ui.view.presentation.stage = EVENT_REWARD_DROP;
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.rewardKind = PW_EVENT_REWARD_POKEMON;
    goto resumeForeground;
  case IR_CMD_EVENT_ITEM_DONE:
    SetView(VIEW_EVENT_REWARD);
    g_ui.view.presentation.stage = EVENT_REWARD_DROP;
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.rewardKind = PW_EVENT_REWARD_ITEM;
    goto resumeForeground;
  case IR_CMD_EVENT_COURSE_DONE:
    SetView(VIEW_EVENT_REWARD);
    g_ui.view.presentation.stage = EVENT_REWARD_DROP;
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.rewardKind = PW_EVENT_REWARD_COURSE;
    goto resumeForeground;
  case IR_CMD_EVENT_STAMP0:
    SetView(VIEW_EVENT_REWARD);
    g_ui.view.presentation.stage = EVENT_REWARD_DROP;
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.rewardKind = PW_EVENT_REWARD_STAMP0;
    goto resumeForeground;
  case IR_CMD_EVENT_STAMP1:
    SetView(VIEW_EVENT_REWARD);
    g_ui.view.presentation.stage = EVENT_REWARD_DROP;
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.rewardKind = PW_EVENT_REWARD_STAMP1;
    goto resumeForeground;
  case IR_CMD_EVENT_STAMP2:
    SetView(VIEW_EVENT_REWARD);
    g_ui.view.presentation.stage = EVENT_REWARD_DROP;
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.rewardKind = PW_EVENT_REWARD_STAMP2;
    goto resumeForeground;
  case IR_CMD_EVENT_STAMP3:
    SetView(VIEW_EVENT_REWARD);
    g_ui.view.presentation.stage = EVENT_REWARD_DROP;
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.rewardKind = PW_EVENT_REWARD_STAMP3;
    goto resumeForeground;
  default:
    if (g_state.irResult == IR_RESULT_NONE)
      goto returnHome;
    SetView(VIEW_IR_RESULT);
    goto clearPrimary;
  }

clearPrimary:
  g_ui.view.presentation.frame = 0;
  goto resumeForeground;

returnHome:
  HomeInit();
  SetView(VIEW_HOME);
resumeForeground:
  g_state.sampleIndex = 0;
  MotionReset();
  InstallTask(MainTick);
  set_ccr(0);
  RtcReadStable(&g_state.time.secondBcd, &g_state.time.minuteBcd,
                &g_state.time.hourBcd24h);
}

/* Initialize RAM and peripherals, restore persistent state, and wait for the
 * battery to exceed its startup threshold. Count watchdog resets in EEPROM.
 * Start Timer B1 and repeatedly call the installed foreground task. */
#pragma entry PowerOnReset(vect = 0)
void PowerOnReset(void)
{
  u32 seed;
  u8 *ram;
  u16 cleared;

  _INITSCT();
  HardwareSetup();
  AccelInit();

  if (WDT.TCSRWD1.BIT.WRST) {
    u8 watchdogResets;

    watchdogResets = EepromReadByte(PW_EEPROM_WATCHDOG_RESET_COUNT);
    watchdogResets++;
    EepromWriteByte(PW_EEPROM_WATCHDOG_RESET_COUNT, watchdogResets);
  }

  ram = (u8 *)&g_state.save;
  cleared = 0;
  do {
    *ram++ = 0;
    cleared++;
  } while (cleared < PW_STARTUP_RAM_CLEAR_BYTES);

  g_state.rolloverHourBcd = 0;
  g_state.events.byte |= EVENT_LOW_POWER_CLOCK;
  g_state.flags.byte =
      ((g_state.flags.byte & SYSTEM_MODE_CLEAR) | SYSTEM_MODE_INTERACTIVE);
  g_state.idleSeconds[IDLE_DISPLAY] = INTERACTIVE_DISPLAY_SECONDS;
  g_state.idleSeconds[IDLE_MOTION] = INTERACTIVE_MOTION_SECONDS;
  g_state.socialElapsedSeconds = PW_RTC_ONE_HOUR_SECONDS;
  MotionReset();
  BootSelfTest();
  WatchdogDisable();
  BootRestore();
  WatchdogStart();

  while (BatteryCheckLow(PW_BATTERY_BOOT_SCALE) != 0)
    WatchdogService();

  BeepInit();
  BeepSetOutputMode(g_state.save.volume);
  DisplayInit();
  RtcRestore();
  EepromRead(PW_PRNG_SEED_CROSS_RECORD, &seed, PW_PRNG_SEED_BYTES);
  RandomSeed(seed);
  IrInit();
  InputInit();
  AccelInit();
  InstallTask(MainTick);
  HomeInit();
  g_state.view = VIEW_HOME;
  TimerB1Init();
  set_ccr(0);
  for (;;)
    g_task();
}

/* Apply the received DeviceStatus time settings. Convert rollover hours below
 * 24 to BCD; a nonzero rtcSeconds value sets the save counter and RTC. */
void StatusApplyTime(void)
{
  int hour;
  u32 seconds;
  u8 *payload = IrPayload();

  g_work.irc.statusA.status = *(DeviceStatus *)payload;
  if ((g_work.irc.statusA.bytes[STATUS_FLAGS_OFFSET] & 0xF8) < 0xC0) {
    hour = g_work.irc.statusA.status.rolloverHour;
    g_state.rolloverHourBcd = ((hour / 10) * (u16)16 | (hour % 10));
  }
  seconds = g_work.irc.statusA.status.rtcSeconds;
  if (seconds != 0) {
    g_state.save.rtcSeconds = seconds;
    RtcSetTime(g_work.irc.statusA.status.rtcSeconds);
  }
}

/* Copy 82 resource pages and five trainer-record pages into active EEPROM,
 * including bytes beyond individual objects. */
void CommitStagedWalk(void)
{
  u16 page;
  u8 *buf;
  u16 source;
  union {
    u32 word32;
    struct {
      u16 pages;
      u16 destination;
    } w;
  } cursor;
  u16 remaining;

  page = EEPROM_PAGE_BYTES;
  ScratchReset();
  {
    u16 allocSize;

    allocSize = page;
    buf = ScratchAlloc(allocSize);
  }
  source = EEPROM_COURSE_TEMP;
  cursor.word32 = ((u32)0x52 << 16) | EEPROM_COURSE;
  remaining = cursor.w.pages;
  do {
    EepromRead(source, buf, page);
    EepromWrite(cursor.w.destination, buf, page);
    source = (source + page);
    cursor.w.destination = (cursor.w.destination + page);
    remaining--;
  } while (remaining != 0);
  source = EEPROM_RECORD_TEMP;
  cursor.word32 = ((u32)5 << 16) | EEPROM_OWN_RECORDS;
  remaining = 0;
  while (remaining < cursor.w.pages) {
    EepromRead(source, buf, page);
    EepromWrite(cursor.w.destination, buf, page);
    source = (source + page);
    cursor.w.destination = (cursor.w.destination + page);
    remaining++;
  }
}

#define PW_TRAINER_NAME_COPY_BYTES STATUS_NAME_BYTES

/* Activate staged resources, register the Pokemon and trainer, then start the
 * diary and clear return inventory. The marker lets boot repeat the page copy;
 * it is cleared before the diary, session and inventory updates. */
void WalkStartCommit(void)
{
  u8 marker;
  u8 courseClear;
  DeviceStatus *status;
  u8 *course;
  u8 encounter;

  marker = WALK_COMMIT_PENDING;
  EepromMirrorWrite(EEPROM_COMMIT_PRIMARY, EEPROM_COMMIT_BACKUP, &marker, 1);
  CommitStagedWalk();
  courseClear = 0;
  EepromMirrorWrite(EEPROM_COMMIT_PRIMARY, EEPROM_COMMIT_BACKUP, &courseClear,
                    1);

  {
    enum { diaryEep = EEPROM_WALK + offsetof(WalkData, diary) };
    DiaryEntry *entry;
    u8 n;

    entry = (DiaryEntry *)diaryEep;
    n = DIARY_ENTRIES;
    do {
      EepromWriteByte((u16)&entry->action, 0);
      entry++;
    } while (--n != 0);
  }

  EepromFill((EEPROM_PEER_RECORDS + sizeof(PeerRecords)),
             (PEER_HISTORY_SLOTS * sizeof(PeerRecords)), 0);

  g_state.hourSteps = 0;
  g_state.save.pokemonMinutes = 0;
  g_state.save.diaryIndex = 0;
  g_state.save.watts = 0;
  EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                    (u8 *)&g_state.save, sizeof(SaveData));

  g_state.flags.bits.hasPokemon = 1;
  g_state.flags.bits.registered = 1;

  status = &g_work.irc.statusB.status;
  EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                   sizeof(DeviceStatus));
  status->registered = 1;
  status->hasPokemon = 1;
  status->generatedPokemon = 0;

  status->consoleCompatibilityLe =
      g_work.irc.statusA.status.consoleCompatibilityLe;
  status->pokemonCompatibilityLe =
      g_work.irc.statusA.status.pokemonCompatibilityLe;
  status->gameVersionLe = g_work.irc.statusA.status.gameVersionLe;
  status->pokemonGameVersionLe = g_work.irc.statusA.status.pokemonGameVersionLe;
  status->trainerIdLe = g_work.irc.statusA.status.trainerIdLe;
  {
    u8 i;

    i = 0;
    do {
      status->trainerNameData[i] = g_work.irc.statusA.status.trainerNameData[i];
      i++;
    } while (i < PW_TRAINER_NAME_COPY_BYTES);
  }
  status->peerProtocol = g_work.irc.statusA.status.peerProtocol;
  status->consoleProtocolLevel = g_work.irc.statusA.status.consoleProtocolLevel;
  status->firmwareCompatibility = FIRMWARE_COMPATIBILITY;
  status->firmwareRevision = FIRMWARE_REVISION;
  EepromMirrorWrite(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                    sizeof(DeviceStatus));

  ScratchReset();
  course = ScratchAlloc(sizeof(Course));
  EepromRead(EEPROM_COURSE, course, sizeof(Course));
  encounter = 0;
  DiaryAppend((Course *)course, ScratchAlloc(sizeof(DiaryEntry)),
              PW_DIARY_ACTION_WALK_STARTED, g_state.save.bonusCourse, encounter,
              0);
  ClearReturnInventory();
}

/* End the walk: clear its Pokemon, Watts, diary actions, return inventory,
 * event resources and peer history, while retaining the console pairing. */
void WalkEndClear(void)
{
  DeviceStatus *status;

  status = &g_work.irc.statusA.status;
  g_state.flags.bits.hasPokemon = 0;
  g_state.save.bonusCourse = 0;
  g_state.save.diaryIndex = 0;
  g_state.save.watts = 0;
  EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                    (u8 *)&g_state.save, sizeof(SaveData));
  EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                   sizeof(DeviceStatus));
  status->pokemonCompatibilityLe = 0;
  status->pokemonGameVersionLe = 0;
  status->hasPokemon = 0;
  status->generatedPokemon = 0;
  EepromMirrorWrite(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                    sizeof(DeviceStatus));
  ClearReturnInventory();
  ClearDiaryActions();
  EepromFill(EEPROM_EVENTS, EEPROM_EVENT_BYTES, 0);
  EepromFill((EEPROM_PEER_RECORDS + sizeof(PeerRecords)),
             (PEER_HISTORY_SLOTS * sizeof(PeerRecords)), 0);
  {
    CourseResources *course;

    course = (CourseResources *)EEPROM_COURSE;
    EepromFill((u16)&course->values.pokemon, sizeof(course->values.pokemon), 0);
  }
}

/* Clear collected Watts, Pokemon and items after collection by the console. */
void ClearWattsInventory(void)
{
  g_state.save.watts = 0;
  EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                    (u8 *)&g_state.save, sizeof(SaveData));
  ClearReturnInventory();
}

/* Acknowledge the Timer B1 wake request. */
#pragma interrupt(TimerB1Interrupt(vect = 33))
void TimerB1Interrupt(void)
{
  IRR2.BIT.IRRTB1 = 0;
}

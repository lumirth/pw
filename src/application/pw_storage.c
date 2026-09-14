#include "types.h"
#include "project.h"
#include "application/pw_buzzer.h"
#include "application/pw_nt7508.h"
#include "application/pw_eeprom_m95512.h"
#include "support/ir.h"
#include "support/lib_clear.h"
#include "support/lib_common.h"
#include "startup/h8_rominfo.h"
#include "application/pw_rtc.h"
#include "support/scratch.h"
#include "application/pw_selftest.h"
#include "startup/h8_resetprg.h"
#include "application/pw_storage.h"

#define PW_SAVE_RTC_DEFAULT_SECONDS 0x0d2b0b80UL
#define PW_SAVE_CONTRAST_MAX 9
#define PW_TRAINER_NAME_COPY_BYTES STATUS_NAME_BYTES

/* Reset session progress and output preferences, then save both mirrors.
 * resetLifetimeFields also resets total steps, days, saved RTC seconds and
 * elapsed hours. */
void SaveReset(u8 resetLifetimeFields)
{
  if (resetLifetimeFields != 0) {
    g_state.save.totalSteps = 0;
    g_state.save.days = 0;
    g_state.save.rtcSeconds = PW_SAVE_RTC_DEFAULT_SECONDS;
    g_state.save.elapsedHours = 0;
  }
  g_state.save.pokemonMinutes = 0;
  g_state.save.watts = 0;
  g_state.save.stepsTowardNextWatt = 0;
  g_state.save.bonusCourse = 0;
  g_state.save.volume = SAVE_DEFAULT_VOLUME;
  g_state.save.contrast = SAVE_DEFAULT_CONTRAST;
  g_state.save.diaryIndex = 0;
  EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                    (u8 *)&g_state.save, sizeof(SaveData));
}

/* Reuse scratch to reload the persistent pairing and Pokemon flags through
 * the repairing mirrored read. */
void StatusRestoreFlags(void)
{
  DeviceStatus *status;

  ScratchReset();
  status = ScratchAlloc(sizeof(DeviceStatus));
  EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                   sizeof(DeviceStatus));
  g_state.flags.bits.registered = status->registered;
  g_state.flags.bits.hasPokemon = status->hasPokemon;
}

/* Reset pairing, session progress, return inventory, weekly steps and peer
 * history. clearEvents also removes receipts and event resources.
 * clearLifetime resets lifetime counters and clears all of WalkData; keeping
 * lifetime progress also keeps diary payloads with their action IDs cleared. */
void PersistentReset(u8 clearEvents, u8 clearLifetime)
{
  DeviceStatus *status;
  u16 i;

  g_state.dailySteps = 0;
  g_state.hourSteps = 0;
  g_state.flags.bits.registered = 0;
  g_state.flags.bits.hasPokemon = 0;
  status = (DeviceStatus *)IrPayload();
  EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                   sizeof(DeviceStatus));
  status->consoleCompatibilityLe = 0;
  status->pokemonCompatibilityLe = 0;
  status->gameVersionLe = 0;
  status->pokemonGameVersionLe = 0;
  status->trainerIdLe = 0;
  i = 0;
  do {
    status->trainerNameData[i] = 0;
    i++;
  } while (i < PW_TRAINER_NAME_COPY_BYTES);
  if (clearEvents != 0) {
    i = 0;
    do {
      status->receivedEvents[i] = 0;
      i++;
    } while (i < 0x10);
  }
  status->registered = 0;
  status->hasPokemon = 0;
  status->generatedPokemon = 0;
  status->rolloverHour = 0;
  status->receiptIndex = 0;
  status->firmwareCompatibility = FIRMWARE_COMPATIBILITY;
  status->firmwareRevision = FIRMWARE_REVISION;
  status->rtcSeconds = 0;
  EepromMirrorRead(EEPROM_ID_PRIMARY, EEPROM_ID_BACKUP, status->deviceId,
                   DEVICE_ID_BYTES);
  if (clearLifetime != 0) {
    status->totalSteps = 0;
  }
  EepromMirrorWrite(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                    sizeof(DeviceStatus));
  SaveReset(clearLifetime);
  if (clearLifetime != 0) {
    EepromFill(EEPROM_WALK, sizeof(WalkData), 0);
  } else {
    ClearReturnInventory();
    ClearDiaryActions();
    ClearWeeklySteps();
  }
  if (clearEvents != 0) {
    EepromFill(EEPROM_EVENTS, EEPROM_EVENT_BYTES, 0);
  }
  EepromFill((EEPROM_PEER_RECORDS + sizeof(PeerRecords)),
             (PEER_HISTORY_SLOTS * sizeof(PeerRecords)), 0);
}

/* Restore the save and flags, replacing excessive contrast with its default.
 * An invalid Nintendo signature triggers a full reset and signature write.
 * A pending 0xA5 marker completes the staged walk copy before being cleared. */
void BootRestore(void)
{
  u8 marker;

  if (BootSignatureValid() != 0) {
    EepromMirrorRead(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                     (u8 *)&g_state.save, sizeof(SaveData));
    if (g_state.save.contrast > PW_SAVE_CONTRAST_MAX) {
      g_state.save.contrast = SAVE_DEFAULT_CONTRAST;
    }
    StatusRestoreFlags();
  } else {
    PersistentReset(RESET_CLEAR_EVENTS, RESET_CLEAR_LIFETIME);
    BeepSetOutputMode(g_state.save.volume);
    DisplaySetContrast(g_state.save.contrast);
    BootSignatureWrite();
  }
  EepromMirrorRead(EEPROM_COMMIT_PRIMARY, EEPROM_COMMIT_BACKUP, &marker, 1);
  if (marker == WALK_COMMIT_PENDING) {
    CommitStagedWalk();
    marker = 0;
    EepromMirrorWrite(EEPROM_COMMIT_PRIMARY, EEPROM_COMMIT_BACKUP, &marker, 1);
  }
}

/* On reset, wait through 15000 watchdog/delay pairs before restoring the saved
 * RTC seconds. */
void RtcRestore(void)
{
  u16 n;

  n = 15000;
  do {
    WatchdogService();
    LowClockDelay();
  } while (--n != 0);
  RtcSetTime(g_state.save.rtcSeconds);
}

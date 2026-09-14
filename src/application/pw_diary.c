#include "types.h"
#include "eeprom_address.h"
#include <stddef.h>
#include "project.h"
#include "application/pw_diary.h"
#include "application/pw_eeprom_m95512.h"

#define PW_DIARY_RING_SLOTS 23

/* Rotate through the first 23 of the 24 stored records. Periodic steps require
 * an empty cursor slot; other actions skip a walk-start marker at the cursor
 * before writing. */
void DiaryAppend(Course *course, DiaryEntry *diary, u8 actionId,
                 u8 useBonusCourse, u8 encounterSelector, u16 itemIdLe)
{
  enum { diaryAddress = EEPROM_WALK + offsetof(WalkData, diary) };
  DiaryEntry *entryAddress;

  entryAddress = (DiaryEntry *)diaryAddress + g_state.save.diaryIndex;
  {
    u8 existingAction;
    existingAction = EepromReadByte((u16)&entryAddress->action);
    if ((existingAction != 0) && (actionId == PW_DIARY_ACTION_PERIODIC_STEPS)) {
      return;
    }
    if (existingAction == PW_DIARY_ACTION_WALK_STARTED) {
      g_state.save.diaryIndex =
          ((g_state.save.diaryIndex + 1) % PW_DIARY_RING_SLOTS);
      entryAddress = (DiaryEntry *)diaryAddress + g_state.save.diaryIndex;
    }
  }
  /* Callers prefill peer details for actions 1..10. Higher action IDs clear the
   * record before local walk details are added. */
  if (actionId > PW_DIARY_ACTION_PREFILLED_MAX) {
    u8 *p;
    u16 n;
    p = (u8 *)diary;
    n = 0;
    do {
      *p = 0;
      p++;
      n++;
    } while (n < sizeof(DiaryEntry));
  }
  diary->action = actionId;
  diary->itemIdLe = itemIdLe;
  diary->rtcSeconds = g_state.save.rtcSeconds;
  diary->ownHourSteps = g_state.hourSteps;
  diary->ownDaySteps = g_state.dailySteps;
  diary->pokemonIdLe = course->pokemon.idLe;
  {
    u16 n;
    n = 0;
    do {
      diary->nickname[n] = course->nickname[n];
      n++;
    } while ((int)n < (int)sizeof(course->nickname));
  }
  diary->friendship = course->friendship;
  diary->ownForm = course->pokemon.form;
  diary->ownSex = course->pokemon.sex;
  diary->ownShiny = course->pokemon.shiny;
  if (useBonusCourse == 0) {
    u16 n;
    diary->journalTheme = course->journalTheme;
    n = 0;
    for (;;) {
      diary->courseNameText[n] = course->courseNameText[n];
      n++;
      if ((int)n >= (int)sizeof(course->courseNameText)) {
        break;
      }
    }
  } else {
    diary->journalTheme = EepromReadByte(PW_EEPROM_MEMBER_ADDRESS(
        EEPROM_BONUS_COURSE, BonusResources, values.journalTheme));
    EepromRead(
        (u16)((BonusResources *)EEPROM_BONUS_COURSE)->values.courseNameText,
        diary->courseNameText, sizeof(diary->courseNameText));
  }
  switch (encounterSelector) {
  case 1:
  case 2:
  case 3:
    diary->encounterIdLe = course->encounters[encounterSelector - 1].idLe;
    diary->peerForm = course->encounters[encounterSelector - 1].form;
    diary->peerSex = course->encounters[encounterSelector - 1].sex;
    break;
  case DIARY_BONUS_ENCOUNTER: {
    u8 packedAppearance;

    /* Flee/defeat reads the bonus-course species; other actions read the stored
     * event species. Both paths decode bonus-region byte 0x0d as form and sex.
     */
    if ((actionId == PW_DIARY_ACTION_BATTLE_FLED) ||
        (actionId == PW_DIARY_ACTION_BATTLE_DEFEATED)) {
      EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_BONUS_COURSE, BonusResources,
                                          values.pokemon.idLe),
                 &diary->encounterIdLe, 2);
    } else {
      EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_POKEMON, EventPokemon,
                                          pokemon.idLe),
                 &diary->encounterIdLe, 2);
    }
    packedAppearance = EepromReadByte((u16)((u8 *)EEPROM_BONUS_COURSE + 0x0d));
    diary->peerForm = (packedAppearance & 0x1f);
    diary->peerSex = ((packedAppearance / 32) & 3);
    break;
  }
  default:
    break;
  }
  EepromWrite((u16)entryAddress, diary, sizeof(DiaryEntry));
  g_state.save.diaryIndex =
      ((g_state.save.diaryIndex + 1) % PW_DIARY_RING_SLOTS);
  EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                    (u8 *)&g_state.save, sizeof(SaveData));
}

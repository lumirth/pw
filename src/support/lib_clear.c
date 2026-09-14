#include "types.h"
#include <stddef.h>
#include "project.h"
#include "application/pw_eeprom_m95512.h"
#include "support/lib_clear.h"

void ClearReturnInventory(void)
{
  WalkData *data;

  data = (WalkData *)EEPROM_WALK;
  EepromFill((u16)&data->pokemon,
             sizeof(data->pokemon) + sizeof(data->items) +
                 sizeof(data->friendItems),
             0);
}

/* Mark diary entries empty by clearing their action byte; keep other fields. */
void ClearDiaryActions(void)
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

void ClearWeeklySteps(void)
{
  WalkData *data;

  data = (WalkData *)EEPROM_WALK;
  EepromFill((u16)&data->dailySteps, sizeof(data->dailySteps), 0);
}

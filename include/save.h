#ifndef PW_SAVE_H
#define PW_SAVE_H

#include <stddef.h>
#include "types.h"

#define TOTAL_STEPS_MAX 9999999ul
#define HOURLY_STEPS_MAX 9999
#define DAILY_STEPS_MAX 99999
#define ELAPSED_HOURS_MAX 9999999ul
#define STEPS_PER_WATT 20
#define WATTS_MAX 9999
#define SAVE_DEFAULT_VOLUME 2
#define SAVE_DEFAULT_CONTRAST 4
#define SAVE_SETTINGS_OFFSET 23
#define SAVE_CONTRAST_MASK 0x78
#define SAVE_CONTRAST_MAX_BITS 0x48

#pragma bit_order right

/* Native H8 byte order. Saving and restoring includes the preserved bytes. */
typedef struct {
  volatile u32 totalSteps;
  /* Counts elapsed hours, including time with an empty Pokemon slot. Cleared
   * on a new walk or lifetime reset; both hour and step caps stop increments.
   */
  volatile u32 elapsedHours;
  volatile u32 rtcSeconds;
  u16 days;
  u16 watts;
  /* Minute ticks since receiving or locally generating a Pokemon. Continues
   * through periods with an empty slot; the consumer checks for a Pokemon.
   */
  u16 pokemonMinutes;
  volatile u8 stepsTowardNextWatt;
  u8 diaryIndex;
  u8 preserved[3];
  u8 bonusCourse : 1;
  u8 volume : 2;
  u8 contrast : 4;
  u8 preservedSettings : 1;
} SaveData;

#pragma bit_order left

typedef char SaveSize[sizeof(SaveData) == 24 ? 1 : -1];
typedef char SaveTime[offsetof(SaveData, rtcSeconds) == 8 ? 1 : -1];
typedef char SaveWatts[offsetof(SaveData, watts) == 14 ? 1 : -1];
typedef char SaveMinutes[offsetof(SaveData, pokemonMinutes) == 16 ? 1 : -1];
typedef char
    SaveProgress[offsetof(SaveData, stepsTowardNextWatt) == 18 ? 1 : -1];
typedef char SaveDiary[offsetof(SaveData, diaryIndex) == 19 ? 1 : -1];
typedef char SavePreserved[offsetof(SaveData, preserved) == 20 ? 1 : -1];

#endif

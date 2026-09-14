#include "types.h"
#include "raster_column.h"
#include "eeprom_address.h"
#include <stddef.h>
#include <machine.h>
#include "startup/iodefine.h"
#include "project.h"
#include "application/pw_nt7508.h"
#include "application/pw_eeprom_m95512.h"
#include "support/lib_common.h"
#include "application/pw_power.h"
#include "support/scratch.h"

#define ENEMY_NAME_SCRATCH_BYTES 384

/* Typed EEPROM expressions calculate resource addresses for serial reads. */

/* Draw selected edges of a 16-row raster. Each word holds both bitplanes of
 * one column; unusedHeight is ignored. */
void RasterBorder16(u8 width, u8 unusedHeight, u16 *raster, u8 edgeFlags)
{
  u16 *cursor;
  u8 index;

  cursor = raster;
  if ((edgeFlags & BORDER_TOP) != 0) {
    index = 0;
    while (index < width) {
      cursor[index] = (cursor[index] | 0x0101);
      index++;
    }
  }
  {
    if ((edgeFlags & BORDER_LEFT) != 0) {
      cursor[0] = 0xffff;
      cursor[width] = 0xffff;
    }
    if ((edgeFlags & BORDER_RIGHT) != 0) {
      (cursor + width)[-1] = 0xffff;
      (cursor + width * 2)[-1] = 0xffff;
    }
    if ((edgeFlags & BORDER_BOTTOM) != 0) {
      index = 0;
      while (index < width) {
        (cursor + width)[index] = ((cursor + width)[index] | 0x8080);
        index++;
      }
    }
  }
}

void RenderHeldPokemon(u8 x, u8 y)
{
  u8 *raster;
  u8 *eepromSource;
  enum { base = EEPROM_COURSE + offsetof(CourseResources, pokemonImage) };

  ScratchReset();
  raster = ScratchAlloc(POKEMON_FRAME_BYTES);
  eepromSource = (u8 *)(base + (g_state.uiFrame & 1) * POKEMON_FRAME_BYTES);
  EepromRead((u16)eepromSource, raster, POKEMON_FRAME_BYTES);
  DisplayBlit(x, y, 32, 24, raster);
}

void RenderHeldPokemonMirrored(u8 x, u8 y)
{
  u8 *raster;
  u8 *eepromSource;
  enum { base = EEPROM_COURSE + offsetof(CourseResources, pokemonImage) };

  ScratchReset();
  raster = ScratchAlloc(POKEMON_FRAME_BYTES);
  eepromSource = (u8 *)(base + (g_state.uiFrame & 1) * POKEMON_FRAME_BYTES);
  EepromRead((u16)eepromSource, raster, POKEMON_FRAME_BYTES);
  RasterMirror(32, 24, raster);
  DisplayBlit(x, y, 32, 24, raster);
}

void RenderEnemyPokemon(u8 x, u8 y, u8 recordIndex)
{
  u8 *raster;
  u8 *eepromSource;
  enum {
    stride = sizeof(((CourseResources *)0)->encounterImages) / COURSE_ENCOUNTERS
  };

  ScratchReset();
  raster = ScratchAlloc(POKEMON_FRAME_BYTES);
  eepromSource =
      ((CourseResources *)EEPROM_COURSE)->encounterImages +
      (recordIndex * stride + (g_state.uiFrame & 1) * POKEMON_FRAME_BYTES);
  EepromRead((u16)eepromSource, raster, POKEMON_FRAME_BYTES);
  DisplayBlit(x, y, 32, 24, raster);
}

void RenderPeerPokemon(u8 x, u8 y, u8 mirrorRequested)
{
  u8 *raster;

  ScratchReset();
  raster = ScratchAlloc(POKEMON_FRAME_BYTES);
  EepromRead((EEPROM_PEER_IMAGE + (g_state.uiFrame & 1) * POKEMON_FRAME_BYTES),
             raster, POKEMON_FRAME_BYTES);
  if (mirrorRequested != 0) {
    RasterMirror(32, 24, raster);
  }
  DisplayWriteSpan(x, y, 0x20, 0x18, (RasterColumn *)raster);
}

void RenderHeldName(u8 x, u8 y, u8 edgeFlags)
{
  u8 *raster;
  u8 *eepromSource;

  ScratchReset();
  raster = ScratchAlloc(TEXT_RASTER_BYTES);
  if (y == PW_DISPLAY_BAND_6_ORIGIN_Y) {
    DisplayBottomNameFrame();
  } else {
    DisplayNameFrame();
  }
  eepromSource = ((CourseResources *)EEPROM_COURSE)->pokemonName;
  EepromRead((u16)eepromSource, raster, TEXT_RASTER_BYTES);
  RasterBorder16(80, 16, (u16 *)raster, edgeFlags);
  DisplayBlit(x, y, 80, 16, raster);
}

void RenderPeerName(u8 x, u8 y, u8 edgeFlags)
{
  u8 *raster;
  enum { addr = EEPROM_PEER_IMAGE + POKEMON_ANIMATION_BYTES };

  ScratchReset();
  raster = ScratchAlloc(TEXT_RASTER_BYTES);
  DisplayNameFrame();
  EepromRead(addr, raster, TEXT_RASTER_BYTES);
  RasterBorder16(80, 16, (u16 *)raster, edgeFlags);
  DisplayBlit(x, y, 80, 16, raster);
}

void RenderDistributionName(u8 x, u8 y, u8 edgeFlags)
{
  u8 *raster;

  ScratchReset();
  raster = ScratchAlloc(TEXT_RASTER_BYTES);
  if (y == PW_DISPLAY_BAND_6_ORIGIN_Y) {
    DisplayBottomNameFrame();
  } else {
    DisplayNameFrame();
  }
  EepromRead(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_POKEMON, EventPokemon, pokemonName),
      raster, TEXT_RASTER_BYTES);
  RasterBorder16(80, 16, (u16 *)raster, edgeFlags);
  DisplayBlit(x, y, 80, 16, raster);
}

void RenderBonusName(u8 x, u8 y, u8 edgeFlags)
{
  u8 *raster;

  ScratchReset();
  raster = ScratchAlloc(TEXT_RASTER_BYTES);
  if (y == PW_DISPLAY_BAND_6_ORIGIN_Y) {
    DisplayBottomNameFrame();
  } else {
    DisplayNameFrame();
  }
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_BONUS_COURSE, BonusResources,
                                      pokemonName),
             raster, TEXT_RASTER_BYTES);
  RasterBorder16(80, 16, (u16 *)raster, edgeFlags);
  DisplayBlit(x, y, 80, 16, raster);
}

void RenderTreasure(u8 x, u8 y)
{
  u8 *raster;
  u8 *eepromSource;

  ScratchReset();
  raster = ScratchAlloc(sizeof(((UiResources *)0)->treasure));
  eepromSource = ((UiResources *)EEPROM_UI)->treasure;
  EepromRead((u16)eepromSource, raster, sizeof(((UiResources *)0)->treasure));
  DisplayBlit(x, y, 8, 8, raster);
}

void RenderEnemyName(u8 x, u8 y, u8 recordIndex, u8 edgeFlags)
{
  CourseResources *course;
  u8 *raster;

  course = (CourseResources *)EEPROM_COURSE;
  ScratchReset();
  /* Load the 320-byte name into a 384-byte scratch reservation. */
  raster = ScratchAlloc(ENEMY_NAME_SCRATCH_BYTES);
  if (y == PW_DISPLAY_BAND_6_ORIGIN_Y) {
    DisplayBottomNameFrame();
  } else {
    DisplayNameFrame();
  }
  EepromRead((u16)&course->encounterNames[recordIndex * TEXT_RASTER_BYTES],
             raster, TEXT_RASTER_BYTES);
  RasterBorder16(80, 16, (u16 *)raster, edgeFlags);
  DisplayBlit(x, y, 80, 16, raster);
}

void RenderCourseItem(u8 x, u8 y, u8 recordIndex, u8 edgeFlags)
{
  CourseResources *course;
  u8 *raster;
  u16 length;
  u8 *eepromSource;

  course = (CourseResources *)EEPROM_COURSE;
  ScratchReset();
  length = MESSAGE_RASTER_BYTES;
  raster = ScratchAlloc(length);
  eepromSource = course->itemNames + recordIndex * length;
  EepromRead((u16)eepromSource, raster, length);
  RasterBorder16(96, 16, (u16 *)raster, edgeFlags);
  DisplayBlit(x, y, 96, 16, raster);
}

void RenderDistributionItem(u8 x, u8 y, u8 edgeFlags)
{
  u8 *raster;

  ScratchReset();
  raster = ScratchAlloc(MESSAGE_RASTER_BYTES);
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_ITEM, EventItem, itemName),
             raster, MESSAGE_RASTER_BYTES);
  RasterBorder16(96, 16, (u16 *)raster, edgeFlags);
  DisplayBlit(x, y, 96, 16, raster);
}

void RenderTreasureIcon(u8 x, u8 y)
{
  u8 *raster;
  u8 *eepromSource;

  eepromSource = ((UiResources *)EEPROM_UI)->itemTreasure;

  ScratchReset();
  raster = ScratchAlloc(sizeof(((UiResources *)0)->itemTreasure));
  EepromRead((u16)eepromSource, raster,
             sizeof(((UiResources *)0)->itemTreasure));
  DisplayBlit(x, y, 32, 24, raster);
}

void RenderPresentIcon(u8 x, u8 y)
{
  u8 *raster;
  u8 *eepromSource;

  eepromSource = ((UiResources *)EEPROM_UI)->itemPresent;

  ScratchReset();
  raster = ScratchAlloc(sizeof(((UiResources *)0)->itemPresent));
  EepromRead((u16)eepromSource, raster,
             sizeof(((UiResources *)0)->itemPresent));
  DisplayBlit(x, y, 32, 24, raster);
}

/* Reload DeviceStatus, set the receipt bit, and write both mirrors. Event ID
 * zero leaves the buffer and EEPROM unchanged. */
void StatusSetReceived(DeviceStatus *status, u8 eventId)
{
  u8 byteIndex;

  if (eventId != 0) {
    byteIndex = (eventId >> 3);
    EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                     sizeof(DeviceStatus));
    status->receivedEvents[byteIndex] |= (1u << (eventId & 7));
    EepromMirrorWrite(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                      sizeof(DeviceStatus));
  }
}

/* Load the mirrored DeviceStatus and test its event-receipt bit. Event ID zero
 * returns false. */
u8 StatusLoadReceived(DeviceStatus *status, u8 eventId)
{
  u8 byteIndex;

  if (eventId != 0) {
    byteIndex = (eventId >> 3);
    EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)status,
                     sizeof(DeviceStatus));
    if ((status->receivedEvents[byteIndex] & (1u << (eventId & 7))) != 0) {
      return 1;
    }
  }
  return 0;
}

/* Copy from the top for upward shifts and the bottom for downward shifts,
 * preserving adjacent source bits until they are merged. Vacated rows become
 * white. */
void RasterShift(u8 width, u8 height, s8 shift, u8 *raster)
{
  u8 band, column;

  if (shift < 0) {
    for (band = 0; band < height / 8; ++band) {
      for (column = 0; column < width * 2; ++column) {
        raster[column] >>= -shift;
        if (band != height / 8 - 1)
          raster[column] =
              raster[column] | (raster[width * 2 + column] << (8 + shift));
      }
      raster += width * 2;
    }
  } else {
    raster += (height / 8 - 1) * width * 2;
    for (band = height / 8; band != 0; --band) {
      for (column = 0; column < width * 2; ++column) {
        raster[column] <<= shift;
        if (band != 1)
          raster[column] =
              raster[column] | (raster[column - width * 2] >> (8 - shift));
      }
      raster -= width * 2;
    }
  }
}

u8 PokemonSlotFindEmpty(Pokemon *pokemon)
{
  u8 i;

  for (i = 0; i < INVENTORY_SLOTS; i++) {
    if (pokemon[i].idLe == 0) {
      return i;
    }
  }
  return INVENTORY_SLOTS;
}

/* The course reload replaces only the leading Course-sized region.
 * PeerAwardGift keeps its peer and item records beyond that prefix. */
u16 CourseLoadItemIdLe(u8 courseItemIndex)
{
  Course *course;

  ScratchReset();
  course = ScratchAlloc(sizeof(Course));
  EepromRead(EEPROM_COURSE, course, sizeof(Course));
  return course->itemIdLe[courseItemIndex];
}

u8 ItemSlotFindEmpty(Item *item)
{
  u8 i;

  for (i = 0; i < INVENTORY_SLOTS; i++) {
    if (item[i].idLe == 0) {
      return i;
    }
  }
  return INVENTORY_SLOTS;
}

/* Add Watts, cap the balance at 9,999, and write both save mirrors. */
void WattsAdd(u8 increment)
{
  g_state.save.watts = (g_state.save.watts + increment);
  if (g_state.save.watts > WATTS_MAX) {
    g_state.save.watts = WATTS_MAX;
  }
  EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                    (u8 *)&g_state.save, sizeof(SaveData));
}

/* Prepare the fixed middle frame, then draw the amount and Watt icon. */
void RenderWatts(u8 x, u8 y, u16 value, u8 unusedEdgeFlags)
{
  u8 *raster;
  u8 i;
  u16 *words;
  const u8 *eepromSource;

  ScratchReset();
  raster = ScratchAlloc(0x140);
  DisplayNameFrame();
  DisplayFillRect(1, 0x28, 0x5e, 8, 0);
  RenderDecimal((x + 8), y, value, NUMBER_TOP_RULE);
  /* RenderDecimal reuses this same prefix for digits. Their transfer is
   * complete, so the Watt icon can now replace them through the saved pointer.
   */
  eepromSource = ((const UiResources *)EEPROM_UI)->watts;
  EepromRead((u16)eepromSource, raster, sizeof(((UiResources *)0)->watts));
  words = (u16 *)raster;
  for (i = 0; i < 0x10; i++) {
    words[i] |= 0x0101;
  }
  DisplayBlit((x + 0x10), y, 16, 16, raster);
}

void RenderDecimal(u8 x, u8 y, u32 value, u8 topRule)
{
  u8 *raster;

  ScratchReset();
  raster = ScratchAlloc(DECIMAL_GLYPHS_BYTES);
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, digits), raster,
             DECIMAL_GLYPHS_BYTES);
  if (topRule != 0) {
    u16 *cursor;
    u8 n;

    cursor = (u16 *)raster;
    n = 10;
    do {
      *cursor = (*cursor | 0x0101);
      cursor++;
      *cursor = (*cursor | 0x0101);
      cursor++;
      *cursor = (*cursor | 0x0101);
      cursor++;
      *cursor = (*cursor | 0x0101);
      cursor++;
      *cursor = (*cursor | 0x0101);
      cursor++;
      *cursor = (*cursor | 0x0101);
      cursor++;
      *cursor = (*cursor | 0x0101);
      cursor++;
      *cursor = (*cursor | 0x0101);
      cursor += 9;
      n--;
    } while (n != 0);
  }
  if (value == 0) {
    DisplayBlit(x, y, 8, 16, raster);
  } else {
    u8 digit;

    while (value != 0) {
      digit = (value % 10);
      DisplayBlit(x, y, 8, 16, raster + digit * NUMBER_GLYPH_BYTES);
      value = value / 10;
      x = (x - 8);
    }
  }
}

/* Draw a 96x16 message with an optional blinking continuation cursor.
 * Each of the cursor's eight mask bytes applies to both bitplanes of a
 * column; its sixteen image bytes are then ORed into those same columns. */
void RenderMessage(u8 y, u8 messageId, u8 edgeFlags, u8 blink)
{
  u8 *raster;
  u8 *overlay;
  u16 length;
  u8 *eepromSource;
  u8 i;

  ScratchReset();
  length = 0x180;
  raster = ScratchAlloc(length);
  overlay = ScratchAlloc(0x18);
  eepromSource = ((UiResources *)EEPROM_UI)->messages + messageId * length;
  EepromRead((u16)eepromSource, raster, length);
  RasterBorder16(0x60, 0x10, (u16 *)raster, edgeFlags);
  if (blink != 0) {
    if (((g_state.uiFrame >> 1) & 1) != 0) {
      EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, next),
                 overlay, 0x18);
      i = 0;
      do {
        (raster + i * 2)[0x170] &= overlay[i + 0x10];
        (raster + (i * 2 + 1))[0x170] &= overlay[i + 0x10];
        i++;
      } while (i < 8);
      i = 0;
      do {
        raster[i + 0x170] = (raster[i + 0x170] | overlay[i]);
        i++;
      } while (i < 0x10);
    }
  }
  DisplayBlit(0, y, 0x60, 0x10, raster);
}

/* Swap column words from the outside in, one 8-row page at a time. */
void RasterMirror(u8 width, u8 height, u8 *raster)
{
  u8 band;
  u8 left;
  u8 last;
  u8 right;
  u16 *words;
  s16 half;
  s16 bands;

  words = (u16 *)raster;
  band = 0;
  half = (width) >> 1;
  bands = (height) >> 3;
  last = (width + 0xff);
  while (band < bands) {
    right = last;
    left = 0;
    while (left < half) {
      words[left] ^= words[right];
      words[right] ^= words[left];
      words[left] ^= words[right];
      left++;
      right--;
    }
    words += width;
    band++;
  }
}

/* Blink the low-battery icon for four refresh phases, then hide it for four. */
void RenderBattery(u8 x, u8 y)
{
  u8 *raster;

  if (g_state.events.bits.batteryLow == 0) {
    return;
  }
  if (((g_state.uiFrame >> 2) & 1) != 0) {
    return;
  }
  ScratchReset();
  raster = ScratchAlloc(sizeof(((UiResources *)0)->battery));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, battery), raster,
             sizeof(((UiResources *)0)->battery));
  DisplayBlit(x, y, 8, 8, raster);
}

/* Preserve masked destination pixels before adding the source. The first
 * page places the mask differently in the high and low bitplanes; the carry
 * page uses the same placement for both. */
void RasterMasked(u8 *destination, u8 destinationWidth, u8 destinationHeight,
                  const u8 *source, const u8 *mask, u8 x, u8 y, u8 width,
                  u8 height)
{
  u8 column, span;

  if (x + width > destinationWidth)
    span = destinationWidth - x;
  else
    span = width;
  destination += ((y / 8) * destinationWidth + x) * 2;
  for (column = 0; column < span; ++column) {
    *(destination + column * 2) =
        *(destination + column * 2) &
        ((*(mask + column) << (8 - (y & 7))) | (0xff >> (y & 7)));
    *(destination + column * 2 + 1) =
        *(destination + column * 2 + 1) &
        ((*(mask + column) << (y & 7)) | (0xff >> (8 - (y & 7))));
    if (y + 8 < destinationHeight) {
      *(destination + (column + destinationWidth) * 2) =
          *(destination + (column + destinationWidth) * 2) &
          ((*(mask + column) >> (8 - (y & 7))) | (0xff << (y & 7)));
      *(destination + (column + destinationWidth) * 2 + 1) =
          *(destination + (column + destinationWidth) * 2 + 1) &
          ((*(mask + column) >> (8 - (y & 7))) | (0xff << (y & 7)));
    }
  }
  for (column = 0; column < span; ++column) {
    *(destination + column * 2) =
        *(destination + column * 2) | (*(source + column * 2) << (y & 7));
    *(destination + column * 2 + 1) = *(destination + column * 2 + 1) |
                                      (*(source + column * 2 + 1) << (y & 7));
    if (y + 8 < destinationHeight) {
      *(destination + (column + destinationWidth) * 2) =
          *(destination + (column + destinationWidth) * 2) |
          (*(source + column * 2) >> (8 - (y & 7)));
      *(destination + (column + destinationWidth) * 2 + 1) =
          *(destination + (column + destinationWidth) * 2 + 1) |
          (*(source + column * 2 + 1) >> (8 - (y & 7)));
    }
  }
}

/* WDT unlock sequence, then stop the counter. */
void WatchdogDisable(void)
{
  WDT.TCSRWD1.BYTE = 0x9e;
  WDT.TCSRWD1.BYTE = 0xa2;
  WDT.TCSRWD1.BYTE = 0x8e;
}

/* WDT unlock, enable, and load the reload value. */
void WatchdogStart(void)
{
  WDT.TCSRWD1.BYTE = 0x9e;
  WDT.TCSRWD1.BYTE = 0xa6;
  WDT.TCSRWD1.BYTE = 0x8e;
  WDT.TMWD.BYTE = 0xf5;
}

void ScratchReset(void)
{
  g_state.scratchCursor = (u16)g_work.motion.scratch.layout.scratch;
}

void *ScratchAlloc(u16 byteCount)
{
  u16 *cursor;
  u16 next;
  u8 *old;

  cursor = &g_state.scratchCursor;
  next = *cursor;
  old = (u8 *)next;
  next += byteCount;
  *cursor = next;
  if ((*cursor - (u16)g_work.motion.scratch.layout.scratch) > 0x400) {
    sleep();
  }
  return old;
}

/* Spread a batch across foreground ticks, crediting at most one step per tick.
 * The phase crosses a strict >64 boundary; the emitted count stops the batch.
 * Fractional progress toward the next Watt survives across batches. */
void StepPacingTick(void)
{
  if (g_state.stepPacing.stepsEmitted != g_state.stepPacing.batchSteps) {
    g_state.stepPacing.stepPhase =
        (g_state.stepPacing.stepPhase + g_state.stepPacing.batchSteps);
    if (g_state.stepPacing.stepPhase > 64) {
      g_state.hourSteps++;
      if (g_state.hourSteps > HOURLY_STEPS_MAX) {
        g_state.hourSteps = HOURLY_STEPS_MAX;
      }
      g_state.dailySteps++;
      if (g_state.dailySteps > (u32)DAILY_STEPS_MAX) {
        g_state.dailySteps = (u32)DAILY_STEPS_MAX;
      }
      StoreTotalSteps(g_state.save.totalSteps + 1);
      g_state.save.stepsTowardNextWatt++;
      if (g_state.save.stepsTowardNextWatt >= STEPS_PER_WATT) {
        g_state.save.stepsTowardNextWatt =
            (g_state.save.stepsTowardNextWatt - STEPS_PER_WATT);
        g_state.save.watts++;
        if (g_state.save.watts > WATTS_MAX) {
          g_state.save.watts = WATTS_MAX;
        }
      }
      g_state.stepPacing.stepsEmitted++;
      if (g_state.stepPacing.stepsEmitted > g_state.stepPacing.batchSteps) {
        g_state.stepPacing.stepsEmitted = g_state.stepPacing.batchSteps;
      }
      g_state.stepPacing.stepPhase = (g_state.stepPacing.stepPhase - 64);
    }
  }
}

/* Select the clock for the sleep transition and publish it to peripheral
 * timing helpers. Values other than the two defined modes do nothing. */
void ClockSleep(uint mode)
{
  if (mode == CLOCK_SLEEP_LOW_POWER) {
    SYSCR1.BYTE = 0xa7;
    SYSCR2.BYTE = 0xe0;
    g_state.events.bits.lowPowerClock = 1;
    sleep();
  } else if (--mode == 0) {
    SYSCR1.BYTE = 0xaf;
    SYSCR2.BYTE = 0xe3;
    g_state.events.bits.lowPowerClock = 0;
    sleep();
  }
}

/* Kick the watchdog: clear the counter under the write-enable key. */
void WatchdogService(void)
{
  WDT.TCSRWD1.BYTE = 0x5e;
  WDT.TCWD = 0;
  WDT.TCSRWD1.BYTE = 0x9e;
}

/* Add settling time only while the CPU uses the slower clock. */
void LowClockDelay(void)
{
  u16 remaining;

  if (g_state.events.bits.lowPowerClock != 0) {
    remaining = 37;
    do {
      nop();
      nop();
      nop();
      nop();
      nop();
      remaining--;
    } while (remaining != 0);
  }
}

void RandomSeed(u32 seed)
{
  g_state.randomState = seed;
}

/* Every draw advances the shared 32-bit linear congruential generator. Call
 * order therefore couples encounters, rewards and communication retry jitter.
 */
u32 RandomNext(void)
{
  u32 *state;
  u32 value;

  state = &g_state.randomState;
  value = *state;
  value = value * 1664525ul + 1013904223ul;
  *state = value;
  return value;
}

/* Expand one bulk block to its advertised byte count. Flag bits run MSB first;
 * a set bit repeats 3..18 bytes from 1..256 bytes behind the output cursor.
 * Forward copies let a run repeat bytes it has just produced. */
void BulkDecode(u8 *packed, u8 *out)
{
  u8 remaining;
  u8 flags;
  s8 bits;
  u8 length;
  u16 back;

  remaining = packed[1];
  packed += 4;
  while (remaining != 0) {
    flags = *packed++;
    bits = 8;
    while (--bits >= 0) {
      if ((flags & 0x80) == 0) {
        *out++ = *packed++;
        remaining--;
      } else {
        length = (*packed / 16);
        length += 3;
        packed++;
        back = (*packed++ + 1);
        remaining = (remaining - length);
        back = -back;
        do {
          *out = *(out + back);
          out++;
        } while (--length != 0);
      }
      if (remaining == 0) {
        break;
      }
      flags = (flags << 1);
    }
  }
}

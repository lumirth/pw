#include "types.h"
#include "eeprom_address.h"
#include "project.h"
#include "application/pw_nt7508.h"
#include "application/pw_diary.h"
#include "application/pw_buzzer.h"
#include "application/pw_carry_overflow.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_home.h"
#include "application/pw_player_input.h"
#include "support/lib_common.h"
#include "support/scratch.h"

/* Start on the middle occupied slot. The caller supplies inventoryKind and
 * sourceSelection through the shared UI bank. */
void DiscardInit(void)
{
  g_ui.view.discard.selectedSlot = 1;
}

#define DISCARD_POKEMON_INVENTORY_BYTES 0x30
#define DISCARD_POKEMON_RECORD_BYTES 0x10
#define PW_DISCARD_PICKER_LAST_SLOT 2

/* Replace the selected catch using the one-based course encounter selection. */
void DiscardReplacePokemon(void)
{
  Pokemon *pokemonRecords;
  Course *course;

  if (g_ui.view.discard.sourceSelection == 0) {
    return;
  }
  ScratchReset();
  pokemonRecords = ScratchAlloc(sizeof(((WalkData *)0)->pokemon));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon),
             pokemonRecords, sizeof(((WalkData *)0)->pokemon));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(
                 EEPROM_COURSE, Course,
                 encounters[g_ui.view.discard.sourceSelection - 1]),
             &pokemonRecords[(s8)g_ui.view.discard.selectedSlot],
             sizeof(Pokemon));
  EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon),
              pokemonRecords, sizeof(((WalkData *)0)->pokemon));
  course = ScratchAlloc(sizeof(Course));
  EepromRead(EEPROM_COURSE, course, sizeof(Course));
  DiaryAppend(course, ScratchAlloc(sizeof(DiaryEntry)),
              PW_DIARY_ACTION_CAPTURED_ROUTE_POKEMON, 0,
              g_ui.view.discard.sourceSelection, 0);
}

/* Replace the selected item's ID, preserving the other stored bytes.
 * sourceSelection indexes the course catalogue from zero. */
void DiscardReplaceItem(void)
{
  Item *itemRecords;
  Course *course;

  ScratchReset();
  itemRecords = ScratchAlloc(sizeof(((WalkData *)0)->items));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items),
             itemRecords, sizeof(((WalkData *)0)->items));
  EepromRead(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course,
                               itemIdLe[g_ui.view.discard.sourceSelection]),
      &itemRecords[(s8)g_ui.view.discard.selectedSlot].idLe,
      sizeof(itemRecords[0].idLe));
  EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items),
              itemRecords, sizeof(((WalkData *)0)->items));
  course = ScratchAlloc(sizeof(Course));
  EepromRead(EEPROM_COURSE, course, sizeof(Course));
  DiaryAppend(course, ScratchAlloc(sizeof(DiaryEntry)),
              PW_DIARY_ACTION_DOWSING_ITEM, 0, 0,
              course->itemIdLe[g_ui.view.discard.sourceSelection]);
}

/* Left from the first slot cancels the new reward. Confirm replaces one slot.
 */
void DiscardUpdate(void)
{
  if (InputPressed(BUTTON_LEFT) != 0) {
    if (g_ui.view.discard.selectedSlot == 0) {
      HomeInit();
      SetView(VIEW_HOME);
      BeepLoadScore(SCORE_BACK);
      return;
    }
    g_ui.view.discard.selectedSlot--;
    BeepLoadScore(SCORE_MOVE);
  }
  if (InputPressed(BUTTON_RIGHT) != 0) {
    if (g_ui.view.discard.selectedSlot == PW_DISCARD_PICKER_LAST_SLOT) {
      BeepLoadScore(SCORE_BACK);
      return;
    }
    g_ui.view.discard.selectedSlot++;
    BeepLoadScore(SCORE_MOVE);
  }
  if (InputPressed(BUTTON_CENTER) != 0) {
    if (g_ui.view.discard.inventoryKind == PW_DISCARD_INVENTORY_POKEMON) {
      DiscardReplacePokemon();
    } else {
      DiscardReplaceItem();
    }
    HomeInit();
    SetView(VIEW_HOME);
    BeepLoadScore(SCORE_CONFIRM);
  }
}

/* Scratch holds three course encounters followed by the selected Pokemon.
 * Compare their stored species IDs and display the first matching name. */
void DiscardRenderPokemonName(void)
{
  u8 *pokemonRecords;
  u8 encounterIndex;

  ScratchReset();
  pokemonRecords = ScratchAlloc(0x40);
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course, encounters[0]),
             pokemonRecords, DISCARD_POKEMON_INVENTORY_BYTES);
  EepromRead(
      (((s8)g_ui.view.discard.selectedSlot * DISCARD_POKEMON_RECORD_BYTES) +
       PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon)),
      pokemonRecords + DISCARD_POKEMON_INVENTORY_BYTES,
      DISCARD_POKEMON_RECORD_BYTES);
  encounterIndex = 0;
  do {
    if (*(u16 *)(pokemonRecords + DISCARD_POKEMON_INVENTORY_BYTES) ==
        *(u16 *)(pokemonRecords +
                 encounterIndex * DISCARD_POKEMON_RECORD_BYTES)) {
      RenderEnemyName(0, 0x30, encounterIndex,
                      BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT);
      break;
    }
    encounterIndex++;
  } while (encounterIndex < 3);
}

/* Find the selected item's name among the course's ten item IDs. */
void DiscardRenderItemName(void)
{
  u16 *itemIdsLe;
  Item selected;
  u8 itemIndex;

  ScratchReset();
  itemIdsLe = ScratchAlloc(sizeof(((Course *)0)->itemIdLe));
  EepromRead(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData,
                               items[(s8)g_ui.view.discard.selectedSlot]),
      &selected, sizeof(selected));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course, itemIdLe[0]),
             itemIdsLe, sizeof(((Course *)0)->itemIdLe));
  itemIndex = 0;
  do {
    if (selected.idLe == itemIdsLe[itemIndex]) {
      RenderCourseItem(0, 0x30, itemIndex,
                       BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT);
      break;
    }
    itemIndex++;
  } while (itemIndex < COURSE_ITEMS);
}

/* Draw arrows and slot icons before the name helpers reuse scratch. */
void DiscardRender(void)
{
  u8 *raster;
  UiResources *image;

  ScratchReset();
  raster = ScratchAlloc(0x180);
  image = (UiResources *)EEPROM_UI;
  EepromRead((u16)(image->returnArrow), raster, 0x20);
  DisplayBlit(0, 0, 8, 0x10, raster);
  EepromRead((u16)image->confirmationPrompt, raster,
             sizeof(image->confirmationPrompt));
  DisplayBlit(8, 0, 0x50, 0x10, raster);
  EepromRead(
      (u16)(image->cursorArrows + CURSOR_OFFSET(CURSOR_DOWN, CURSOR_FILLED)),
      raster, 0x20);
  DisplayBlit((g_ui.view.discard.selectedSlot * 0x14 + 0x18), 0x18, 8, 8,
              raster + (g_state.uiFrame & 1) * 0x10);
  if (g_ui.view.discard.inventoryKind == PW_DISCARD_INVENTORY_POKEMON) {
    EepromRead((u16)image->ball, raster, 0x10);
  } else {
    EepromRead((u16)image->treasure, raster, 0x10);
  }
  DisplayBlit(0x18, 0x20, 8, 8, raster);
  DisplayBlit(0x2c, 0x20, 8, 8, raster);
  DisplayBlit(0x40, 0x20, 8, 8, raster);
  if (((s8)g_ui.view.discard.selectedSlot >= 0) &&
      ((s8)g_ui.view.discard.selectedSlot <= PW_DISCARD_PICKER_LAST_SLOT)) {
    if (g_ui.view.discard.inventoryKind == PW_DISCARD_INVENTORY_POKEMON) {
      DiscardRenderPokemonName();
    } else {
      DiscardRenderItemName();
    }
    RenderBattery(0x58, 0);
  }
}

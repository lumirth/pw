#include "types.h"
#include "eeprom_address.h"
#include "project.h"
#include "application/pw_nt7508.h"
#include "application/pw_buzzer.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_home.h"
#include "application/pw_pack.h"
#include "application/pw_pictogram_menu.h"
#include "application/pw_player_input.h"
#include "support/lib_common.h"
#include "support/scratch.h"

u8 InventoryNext(uint entryMask);
void InventoryNextWrap(uint entryMask);
u8 InventoryPrevious(uint entryMask);
void InventoryPreviousWrap(uint entryMask);

/* Build occupied-entry masks: held Pokemon, captures, items and event gifts
 * in the first word; friend items in the second. */
void InventoryMasks(u8 *entryMasksOut)
{
  Pokemon *pokemon;
  Item *item;
  u8 i;
  u8 eventPresence;

  ScratchReset();
  pokemon = ScratchAlloc((sizeof(Pokemon) * INVENTORY_SLOTS));
  item = ScratchAlloc((sizeof(Item) * (INVENTORY_SLOTS + FRIEND_ITEM_SLOTS)));

  i = 0;
  do {
    *(u16 *)(entryMasksOut + i * 2) = 0;
    i++;
  } while (i < 2);

  if (g_state.flags.bits.hasPokemon) {
    *(u16 *)entryMasksOut |= WALK_MASK_HELD_POKEMON;
  }

  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon), pokemon,
             (sizeof(Pokemon) * INVENTORY_SLOTS));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items), item,
             (sizeof(Item) * (INVENTORY_SLOTS + FRIEND_ITEM_SLOTS)));

  i = 0;
  do {
    if (pokemon[i].idLe != 0) {
      *(u16 *)entryMasksOut |= (WALK_MASK_CAUGHT_FIRST << i);
    }
    i++;
  } while (i < INVENTORY_SLOTS);

  i = 0;
  do {
    if (item[i].idLe != 0) {
      *(u16 *)entryMasksOut |= (WALK_MASK_ITEM_FIRST << i);
    }
    i++;
  } while (i < INVENTORY_SLOTS);

  item += INVENTORY_SLOTS;
  i = 0;
  do {
    if (item[i].idLe != 0) {
      *(u16 *)(entryMasksOut + 2) |= (1 << i);
    }
    i++;
  } while (i < FRIEND_ITEM_SLOTS);

  eventPresence = EepromReadByte(EEPROM_EVENTS);
  if ((eventPresence & EVENT_PRESENT_POKEMON) != 0) {
    *(u16 *)entryMasksOut |= WALK_MASK_EVENT_POKEMON;
  }
  if ((eventPresence & EVENT_PRESENT_MAP) != 0) {
    *(u16 *)entryMasksOut |= WALK_MASK_EVENT_MAP;
  }
  if ((eventPresence & EVENT_PRESENT_ITEM) != 0) {
    /* Set entry 9 through the high byte of the first native 16-bit mask. */
    *entryMasksOut |= WALK_MASK_EVENT_ITEM_HIGH_BYTE;
  }
}

/* Move to the next occupied entry and return 0. Exhausting the right side
 * returns 1 with the cursor unchanged. */
u8 InventoryNext(uint entryMask)
{
  u8 index;
  u8 steps;

  index = g_ui.view.inventory.entryIndex;
  if (index == INVENTORY_LAST_ENTRY) {
    return 1;
  }
  index++;
  steps = 0;
  while (steps < INVENTORY_ENTRY_COUNT) {
    if ((entryMask & (1 << index)) != 0) {
      g_ui.view.inventory.entryIndex = index;
      return 0;
    }
    if (index == INVENTORY_LAST_ENTRY) {
      return 1;
    }
    index++;
    steps++;
  }
  return 0;
}

/* Callers supply a nonempty mask when selecting with wraparound. */
void InventoryNextWrap(uint entryMask)
{
  u8 steps;

  g_ui.view.inventory.entryIndex =
      ((g_ui.view.inventory.entryIndex + 1) % INVENTORY_ENTRY_COUNT);
  steps = 0;
  do {
    if ((entryMask & (1 << g_ui.view.inventory.entryIndex)) != 0) {
      return;
    }
    g_ui.view.inventory.entryIndex =
        ((g_ui.view.inventory.entryIndex + 1) % INVENTORY_ENTRY_COUNT);
    steps++;
  } while (steps < INVENTORY_ENTRY_COUNT);
}

/* Search left by moving the shared cursor. Return 0 on a match; an exhausted
 * search returns 1 with the cursor at 0. */
u8 InventoryPrevious(uint entryMask)
{
  u8 steps;

  if (g_ui.view.inventory.entryIndex == 0) {
    return 1;
  }
  g_ui.view.inventory.entryIndex--;
  steps = 0;
  while (steps < INVENTORY_ENTRY_COUNT) {
    if ((entryMask & (1 << g_ui.view.inventory.entryIndex)) != 0) {
      return 0;
    }
    if (g_ui.view.inventory.entryIndex == 0) {
      return 1;
    }
    g_ui.view.inventory.entryIndex--;
    steps++;
  }
  return 0;
}

void InventoryPreviousWrap(uint entryMask)
{
  u8 steps;

  g_ui.view.inventory.entryIndex =
      ((g_ui.view.inventory.entryIndex + 9) % INVENTORY_ENTRY_COUNT);
  steps = 0;
  do {
    if ((entryMask & (1 << g_ui.view.inventory.entryIndex)) != 0) {
      return;
    }
    g_ui.view.inventory.entryIndex =
        ((g_ui.view.inventory.entryIndex + 9) % INVENTORY_ENTRY_COUNT);
    steps++;
  } while (steps < INVENTORY_ENTRY_COUNT);
}

void WalkInventorySelectFirst(void)
{
  g_ui.view.inventory.entryIndex = INVENTORY_LAST_ENTRY;
  InventoryNextWrap(g_ui.view.inventory.walkEntryMask);
}

void WalkInventoryUpdate(void)
{
  u8 blocked;

  if (InputPressed(BUTTON_LEFT) != 0) {
    blocked = InventoryPrevious(g_ui.view.inventory.walkEntryMask);
    if (blocked != 0) {
      MenuReset();
      SetView(VIEW_MAIN_MENU);
      BeepLoadScore(SCORE_BACK);
      return;
    }
    BeepLoadScore(SCORE_MOVE);
  }
  if (InputPressed(BUTTON_RIGHT) != 0) {
    blocked = InventoryNext(g_ui.view.inventory.walkEntryMask);
    if (blocked != 0) {
      if (g_ui.view.inventory.friendItemMask != 0) {
        FriendItemSelectFirst();
        SetView(VIEW_FRIEND_ITEMS);
        BeepLoadScore(SCORE_MOVE);
        return;
      }
      BeepLoadScore(SCORE_BACK);
      return;
    }
    BeepLoadScore(SCORE_MOVE);
  }
  if (InputPressed(BUTTON_CENTER) == 0) {
    return;
  }
  if (g_ui.view.inventory.friendItemMask != 0) {
    FriendItemSelectFirst();
    SetView(VIEW_FRIEND_ITEMS);
  } else {
    HomeInit();
    SetView(VIEW_HOME);
  }
  BeepLoadScore(SCORE_CONFIRM);
}

/* Use each capture's species number to select the first matching course image
 * and name. */
void WalkInventoryRender(void)
{
  UiResources *resources;
  u8 *raster;
  u8 *eepromSource;
  u8 entryIndex;
  u8 pokemonSlot;
  u8 x;
  u8 y;
  int i;
  Item selectedItem;
  u16 length;

  resources = (UiResources *)EEPROM_UI;
  length = 0x10;
  ScratchReset();
  raster = ScratchAlloc(0x180);
  eepromSource = resources->menuLabels +
                 MENU_INVENTORY * (sizeof(resources->menuLabels) / MENU_COUNT);
  EepromRead((u16)eepromSource, raster,
             sizeof(resources->menuLabels) / MENU_COUNT);
  DisplayBlit(8, 0, 0x50, 0x10, raster);

  entryIndex = g_ui.view.inventory.entryIndex;
  switch (entryIndex) {
  case WALK_ENTRY_HELD_POKEMON:
    RenderHeldPokemon(0x3c, 0x18);
    RenderHeldName(0, 0x30, BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT);
    break;
  case WALK_ENTRY_CAUGHT_FIRST:
  case WALK_ENTRY_CAUGHT_SECOND:
  case WALK_ENTRY_CAUGHT_THIRD:
    pokemonSlot = (entryIndex - WALK_ENTRY_CAUGHT_FIRST);
    if (pokemonSlot > 2) {
      pokemonSlot = 0;
    }
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course, encounters[0]),
               raster, (sizeof(Pokemon) * COURSE_ENCOUNTERS));
    EepromRead(
        PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon[pokemonSlot]),
        raster + sizeof(Pokemon) * COURSE_ENCOUNTERS, sizeof(Pokemon));
    i = 0;
    do {
      if (((Pokemon *)raster)[COURSE_ENCOUNTERS].idLe ==
          ((Pokemon *)raster)[i].idLe) {
        RenderEnemyPokemon(0x3c, 0x18, i);
        RenderEnemyName(0, 0x30, i, BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT);
        break;
      }
      i++;
    } while (i < COURSE_ENCOUNTERS);
    break;
  case WALK_ENTRY_EVENT_POKEMON:
    /* Read 384 bytes from the selected frame; draw the leading 192 bytes. */
    EepromRead((u16)(((EventPokemon *)EEPROM_EVENT_POKEMON)->pokemonImage +
                     (g_state.uiFrame & 1) * POKEMON_FRAME_BYTES),
               raster, POKEMON_ANIMATION_BYTES);
    DisplayBlit(0x3c, 0x18, 0x20, 0x18, raster);
    RenderDistributionName(0, 0x30, BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT);
    break;
  case WALK_ENTRY_EVENT_MAP:
    eepromSource = resources->itemMap;
    EepromRead((u16)eepromSource, raster, sizeof(resources->itemMap));
    DisplayBlit(0x3c, 0x18, 0x20, 0x18, raster);
    RenderMessage(0x30, MESSAGE_SPECIAL_MAP,
                  BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_NO_PROMPT);
    break;
  case WALK_ENTRY_ITEM_FIRST:
  case WALK_ENTRY_ITEM_SECOND:
  case WALK_ENTRY_ITEM_THIRD:
    RenderTreasureIcon(0x3c, 0x18);
    EepromRead(
        PW_EEPROM_MEMBER_ADDRESS(
            EEPROM_WALK, WalkData,
            items[g_ui.view.inventory.entryIndex - WALK_ENTRY_ITEM_FIRST]),
        &selectedItem, sizeof(Item));
    {
      u16 *itemIdsLe;
      itemIdsLe = ScratchAlloc(sizeof(((Course *)0)->itemIdLe));
      EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course, itemIdLe[0]),
                 itemIdsLe, sizeof(((Course *)0)->itemIdLe));
      i = 0;
      do {
        if (selectedItem.idLe == itemIdsLe[i]) {
          RenderCourseItem(0, 0x30, i,
                           BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT |
                               BORDER_RIGHT);
          break;
        }
        i++;
      } while (i < COURSE_ITEMS);
    }
    break;
  case WALK_ENTRY_EVENT_ITEM:
    eepromSource = resources->itemTreasure;
    EepromRead((u16)eepromSource, raster, sizeof(resources->itemTreasure));
    DisplayBlit(0x3c, 0x18, 0x20, 0x18, raster);
    RenderDistributionItem(
        0, 0x30, BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT);
    break;
  }

  /* The detail helpers reuse scratch. Reload each cursor, arrow and slot icon
   * before drawing it. */
  eepromSource =
      resources->cursorArrows + CURSOR_OFFSET(CURSOR_DOWN, g_state.uiFrame & 1);
  EepromRead((u16)eepromSource, raster, length);
  x = ((g_ui.view.inventory.entryIndex % 5) * 8 + 0x10);
  if (g_ui.view.inventory.entryIndex == WALK_ENTRY_HELD_POKEMON) {
    x = (x - 8);
  }
  y = ((g_ui.view.inventory.entryIndex / 5) * 0x10 + 0x10);
  DisplayBlit(x, y, 8, 8, raster);

  eepromSource = resources->rightArrow;
  EepromRead((u16)eepromSource, raster, 2 * NAVIGATION_ICON_BYTES);
  DisplayBlit(0, 0, 8, 0x10, raster + NAVIGATION_ICON_BYTES);
  if (g_ui.view.inventory.friendItemMask != 0) {
    DisplayBlit(0x58, 0, 8, 0x10, raster);
  }

  eepromSource = resources->ball;
  EepromRead((u16)eepromSource, raster, length);
  if ((g_ui.view.inventory.walkEntryMask & WALK_MASK_HELD_POKEMON) != 0) {
    DisplayBlit(8, 0x18, 8, 8, raster);
  }
  i = 0;
  do {
    if ((g_ui.view.inventory.walkEntryMask & (WALK_MASK_CAUGHT_FIRST << i)) !=
        0) {
      DisplayBlit((i * 8 + 0x18), 0x18, 8, 8, raster);
    }
    i++;
  } while (i < 3);

  eepromSource = resources->treasure;
  EepromRead((u16)eepromSource, raster, length);
  i = 0;
  do {
    if ((g_ui.view.inventory.walkEntryMask & (WALK_MASK_ITEM_FIRST << i)) !=
        0) {
      DisplayBlit((i * 8 + 0x18), 0x28, 8, 8, raster);
    }
    i++;
  } while (i < 3);

  if ((g_ui.view.inventory.walkEntryMask & WALK_MASK_EVENT_POKEMON) != 0) {
    eepromSource = resources->eventBall;
    EepromRead((u16)eepromSource, raster, length);
    DisplayBlit(0x30, 0x18, 8, 8, raster);
  }
  if ((g_ui.view.inventory.walkEntryMask & WALK_MASK_EVENT_ITEM) != 0) {
    eepromSource = resources->eventTreasure;
    EepromRead((u16)eepromSource, raster, length);
    DisplayBlit(0x30, 0x28, 8, 8, raster);
  }
  if ((g_ui.view.inventory.walkEntryMask & WALK_MASK_EVENT_MAP) != 0) {
    eepromSource = resources->eventMap;
    EepromRead((u16)eepromSource, raster, length);
    DisplayBlit(0x10, 0x28, 8, 8, raster);
  }

  RenderBattery(0x58, 0);
}

void FriendItemSelectFirst(void)
{
  g_ui.view.inventory.entryIndex = INVENTORY_LAST_ENTRY;
  InventoryNextWrap(g_ui.view.inventory.friendItemMask);
}

void FriendItemInventoryUpdate(void)
{
  u8 blocked;

  if (InputPressed(BUTTON_LEFT) != 0) {
    blocked = InventoryPrevious(g_ui.view.inventory.friendItemMask);
    if (blocked != 0) {
      if (g_ui.view.inventory.walkEntryMask != 0) {
        g_ui.view.inventory.entryIndex = 0;
        InventoryPreviousWrap(g_ui.view.inventory.walkEntryMask);
        SetView(VIEW_WALK_INVENTORY);
        BeepLoadScore(SCORE_MOVE);
        return;
      }
      BeepLoadScore(SCORE_BACK);
      return;
    }
    BeepLoadScore(SCORE_MOVE);
  }
  if (InputPressed(BUTTON_RIGHT) != 0) {
    blocked = InventoryNext(g_ui.view.inventory.friendItemMask);
    if (blocked != 0) {
      BeepLoadScore(SCORE_BACK);
      return;
    }
    BeepLoadScore(SCORE_MOVE);
  }
  if (InputPressed(BUTTON_CENTER) == 0) {
    return;
  }
  HomeInit();
  SetView(VIEW_HOME);
  BeepLoadScore(SCORE_CONFIRM);
}

/* Friend-item slots are zero-based; stored IDs select course-item names. */
void FriendItemInventoryRender(void)
{
  UiResources *resources;
  u8 *raster;
  u8 *eepromSource;
  u16 *itemIdsLe;
  u8 x;
  int i;
  Item selectedItem;

  resources = (UiResources *)EEPROM_UI;
  ScratchReset();
  raster = ScratchAlloc(sizeof(resources->menuLabels) / MENU_COUNT);
  eepromSource = resources->leftArrow;
  EepromRead((u16)eepromSource, raster, NAVIGATION_ICON_BYTES);
  DisplayBlit(0, 0, 8, 0x10, raster);
  eepromSource = resources->menuLabels +
                 MENU_INVENTORY * (sizeof(resources->menuLabels) / MENU_COUNT);
  EepromRead((u16)eepromSource, raster,
             sizeof(resources->menuLabels) / MENU_COUNT);
  DisplayBlit(8, 0, 0x50, 0x10, raster);
  eepromSource = resources->itemPresent;
  EepromRead((u16)eepromSource, raster, sizeof(resources->itemPresent));
  DisplayBlit(0x3c, 0x18, 0x20, 0x18, raster);

  eepromSource =
      resources->cursorArrows + CURSOR_OFFSET(CURSOR_DOWN, g_state.uiFrame & 1);
  EepromRead((u16)eepromSource, raster, 0x10);
  x = ((g_ui.view.inventory.entryIndex % 5) * 8 + 0x10);
  DisplayBlit(x, ((g_ui.view.inventory.entryIndex / 5) * 0x10 + 0x10), 8, 8,
              raster);

  eepromSource = resources->treasure;
  EepromRead((u16)eepromSource, raster, 0x10);
  i = 0;
  do {
    if ((g_ui.view.inventory.friendItemMask & (1 << i)) != 0) {
      DisplayBlit((i * 8 + 0x10), 0x18, 8, 8, raster);
    }
    i++;
  } while (i < 5);
  i = 0;
  do {
    if ((g_ui.view.inventory.friendItemMask & (0x20 << i)) != 0) {
      DisplayBlit((i * 8 + 0x10), 0x28, 8, 8, raster);
    }
    i++;
  } while (i < 5);

  EepromRead(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData,
                               friendItems[g_ui.view.inventory.entryIndex]),
      &selectedItem, sizeof(Item));
  itemIdsLe = ScratchAlloc(sizeof(((Course *)0)->itemIdLe));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course, itemIdLe[0]),
             itemIdsLe, sizeof(((Course *)0)->itemIdLe));
  i = 0;
  do {
    if (selectedItem.idLe == itemIdsLe[i]) {
      RenderCourseItem(0, 0x30, i,
                       BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT);
      break;
    }
    i++;
  } while (i < COURSE_ITEMS);

  RenderBattery(0x58, 0);
}

#include "types.h"
#include "project.h"
#include "application/pw_nt7508.h"
#include "application/pw_buzzer.h"
#include "application/pw_dowsing.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_home.h"
#include "application/pw_local_settings.h"
#include "application/pw_pack.h"
#include "application/pw_pictogram_menu.h"
#include "application/pw_player_input.h"
#include "support/lib_common.h"
#include "application/pw_pokeradar.h"
#include "support/scratch.h"
#include "application/pw_trainer.h"

extern const u8 g_mainMenuWattCosts[MENU_COUNT];
extern const u8 g_menuIconY[MENU_COUNT];

#define RADAR_WATT_COST 10
#define DOWSING_WATT_COST 3

void MenuReset(void)
{
  g_ui.view.menu.error = MENU_ERROR_NONE;
}

/* Subtract the cost and save the Watt balance before opening radar or dowsing.
 * Any button dismisses an error; moving past either menu end returns home. */
void MainMenuUpdate(void)
{
  u16 availableWatts;
  u16 entryMasks[2];
  u8 *wattCosts;
  int index;

  if (g_ui.view.menu.error != MENU_ERROR_NONE) {
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    g_ui.view.menu.error = MENU_ERROR_NONE;
    BeepLoadScore(SCORE_CONFIRM);
    return;
  }

  if (InputPressed(BUTTON_CENTER) != 0) {
    availableWatts = g_state.save.watts;
    wattCosts = (u8 *)g_mainMenuWattCosts;
    if (availableWatts < wattCosts[g_state.menuSelection]) {
      g_ui.view.menu.error = MENU_ERROR_WATTS;
      BeepLoadScore(SCORE_FAILURE);
      return;
    }
    switch (g_state.menuSelection) {
    case MENU_RADAR:
      if (g_state.flags.bits.hasPokemon == 0) {
        g_ui.view.menu.error = MENU_ERROR_NO_POKEMON;
        BeepLoadScore(SCORE_FAILURE);
        return;
      }
      if (availableWatts < wattCosts[g_state.menuSelection]) {
        g_state.save.watts = 0;
      } else {
        g_state.save.watts =
            (g_state.save.watts - wattCosts[g_state.menuSelection]);
      }
      EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                        (u8 *)&g_state.save, sizeof(SaveData));
      SetView(VIEW_RADAR);
      RadarInit();
      BeepLoadScore(SCORE_CONFIRM);
      return;
    case MENU_DOWSING:
      if (g_state.save.watts < wattCosts[g_state.menuSelection]) {
        g_state.save.watts = 0;
      } else {
        g_state.save.watts =
            (g_state.save.watts - wattCosts[g_state.menuSelection]);
      }
      EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                        (u8 *)&g_state.save, sizeof(SaveData));
      DowsingInit();
      SetView(VIEW_DOWSING);
      BeepLoadScore(SCORE_CONFIRM);
      return;
    case MENU_CONNECT:
      TryBeginIr();
      return;
    case MENU_TRAINER:
      SetView(VIEW_TRAINER);
      TrainerInit();
      BeepLoadScore(SCORE_CONFIRM);
      return;
    case MENU_INVENTORY:
      InventoryMasks((u8 *)entryMasks);
      if (entryMasks[INVENTORY_WALK_MASK] != 0) {
        g_ui.view.inventory.walkEntryMask = entryMasks[INVENTORY_WALK_MASK];
        g_ui.view.inventory.friendItemMask = entryMasks[INVENTORY_FRIEND_MASK];
        WalkInventorySelectFirst();
        SetView(VIEW_WALK_INVENTORY);
        BeepLoadScore(SCORE_CONFIRM);
        return;
      }
      if (entryMasks[INVENTORY_FRIEND_MASK] != 0) {
        g_ui.view.inventory.walkEntryMask = entryMasks[INVENTORY_WALK_MASK];
        g_ui.view.inventory.friendItemMask = entryMasks[INVENTORY_FRIEND_MASK];
        FriendItemSelectFirst();
        SetView(VIEW_FRIEND_ITEMS);
        BeepLoadScore(SCORE_CONFIRM);
        return;
      }
      g_ui.view.menu.error = MENU_ERROR_EMPTY_INVENTORY;
      BeepLoadScore(SCORE_FAILURE);
      return;
    case MENU_SETTINGS:
      SettingsInit();
      SetView(VIEW_SETTINGS);
      BeepLoadScore(SCORE_CONFIRM);
      return;
    }
  }

  if (InputPressed(BUTTON_LEFT) != 0) {
    if (g_state.menuSelection == MENU_RADAR) {
      HomeInit();
      SetView(VIEW_HOME);
      BeepLoadScore(SCORE_BACK);
      return;
    }
    index = g_state.menuSelection;
    index = (index + 5) % MENU_COUNT;
    g_state.menuSelection = index;
    BeepLoadScore(SCORE_MOVE);
  }
  if (InputPressed(BUTTON_RIGHT) != 0) {
    if (g_state.menuSelection == MENU_SETTINGS) {
      HomeInit();
      SetView(VIEW_HOME);
      BeepLoadScore(SCORE_BACK);
      return;
    }
    index = g_state.menuSelection;
    index = (index + 1) % MENU_COUNT;
    g_state.menuSelection = index;
    BeepLoadScore(SCORE_MOVE);
  }
}

void MainMenuRender(void)
{
  UiResources *resources;
  u8 *raster;
  u8 *iconRaster;
  u8 *eepromSource;
  u16 labelBytes;
  u16 iconBytes;
  int i;

  resources = (UiResources *)EEPROM_UI;
  iconBytes = (sizeof(resources->menuIcons) / MENU_COUNT);
  ScratchReset();
  labelBytes = (sizeof(resources->menuLabels) / MENU_COUNT);
  raster = ScratchAlloc(labelBytes);
  iconRaster = ScratchAlloc(0x80);
  eepromSource = resources->menuLabels + g_state.menuSelection * labelBytes;
  EepromRead((u16)eepromSource, raster, labelBytes);
  DisplayBlit(8, 0, 0x50, 0x10, raster);

  i = 0;
  do {
    u16 j;
    j = 0;
    do {
      iconRaster[j] = 0;
      j++;
    } while (j < 0x80);
    if (i == g_state.menuSelection) {
      eepromSource = resources->cursorArrows +
                     CURSOR_OFFSET(CURSOR_DOWN, g_state.uiFrame & 1);
      EepromRead((u16)eepromSource, raster, 0x10);
      RasterOr(8, 8, raster, 4, (g_menuIconY[i] - 8), iconRaster, 0x10, 0x20);
    }
    eepromSource = resources->menuIcons + i * iconBytes;
    EepromRead((u16)eepromSource, raster, iconBytes);
    RasterOr(0x10, 0x10, raster, 0, g_menuIconY[i], iconRaster, 0x10, 0x20);
    DisplayBlit((i * 0x10), 0x10, 0x10, 0x20, iconRaster);
    i++;
  } while (i < MENU_COUNT);

  switch (g_ui.view.menu.error) {
  case MENU_ERROR_NONE:
    RenderDecimal(0x48, 0x30, g_state.save.watts, NUMBER_NO_RULE);
    switch (g_state.menuSelection) {
    case MENU_RADAR:
      RenderDecimal(8, 0x30, (u32)RADAR_WATT_COST, NUMBER_NO_RULE);
      break;
    case MENU_DOWSING:
      RenderDecimal(8, 0x30, (u32)DOWSING_WATT_COST, NUMBER_NO_RULE);
      break;
    }
    eepromSource = resources->watts;
    EepromRead((u16)eepromSource, raster, iconBytes);
    DisplayBlit(0x50, 0x30, 0x10, 0x10, raster);
    if (g_state.menuSelection < MENU_CONNECT) {
      DisplayBlit(0x18, 0x30, 0x10, 0x10, raster);
      eepromSource = resources->slash;
      EepromRead((u16)eepromSource, raster, 0x20);
      DisplayBlit(0x28, 0x30, 8, 0x10, raster);
    }
    break;
  case MENU_ERROR_WATTS:
    RenderMessage(0x30, MESSAGE_NOT_ENOUGH_WATTS,
                  BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_BLINK_PROMPT);
    break;
  case MENU_ERROR_NO_POKEMON:
    RenderMessage(0x30, MESSAGE_NO_POKEMON,
                  BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_BLINK_PROMPT);
    break;
  case MENU_ERROR_EMPTY_INVENTORY:
    RenderMessage(0x30, MESSAGE_NO_ITEMS,
                  BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_BLINK_PROMPT);
    break;
  }

  eepromSource = resources->leftArrow;
  EepromRead((u16)eepromSource, raster, 2 * NAVIGATION_ICON_BYTES);
  DisplayBlit(0, 0, 8, 0x10, raster);
  DisplayBlit(0x58, 0, 8, 0x10, raster + NAVIGATION_ICON_BYTES);
  RenderBattery(0x58, 0);
}

/* Both tables use MENU_* order. */
const u8 g_mainMenuWattCosts[MENU_COUNT] = {
    RADAR_WATT_COST, DOWSING_WATT_COST, 0x00, 0x00, 0x00, 0x00};

/* Pixel Y within each 16-by-32 menu-icon work raster. */
const u8 g_menuIconY[MENU_COUNT] = {0x08, 0x0b, 0x0d, 0x0d, 0x0b, 0x08};

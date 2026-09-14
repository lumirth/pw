#include "types.h"
#include "project.h"
#include "application/pw_nt7508.h"
#include "application/pw_buzzer.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_home.h"
#include "application/pw_local_settings.h"
#include "application/pw_pictogram_menu.h"
#include "application/pw_player_input.h"
#include "support/lib_common.h"
#include "support/scratch.h"

#define VOLUME_LEVELS 3

void SettingsInit(void)
{
  g_ui.view.settings.page = SETTINGS_PAGE_SELECT;
  g_ui.view.settings.selection = SETTINGS_SELECT_VOLUME;
}

/* Left from volume returns to the menu; contrast is the right selection. */
void SettingsNavigate(void)
{
  if (InputPressed(BUTTON_LEFT) != 0) {
    if (g_ui.view.settings.selection == SETTINGS_SELECT_VOLUME) {
      BeepLoadScore(SCORE_BACK);
      MenuReset();
      SetView(VIEW_MAIN_MENU);
      return;
    }
    g_ui.view.settings.selection = SETTINGS_SELECT_VOLUME;
    BeepLoadScore(SCORE_MOVE);
  }
  if (InputPressed(BUTTON_RIGHT) == 0) {
    return;
  }
  if (g_ui.view.settings.selection == SETTINGS_SELECT_CONTRAST) {
    return;
  }
  g_ui.view.settings.selection = SETTINGS_SELECT_CONTRAST;
  BeepLoadScore(SCORE_MOVE);
}

/* Apply volume changes immediately so the navigation beep previews them. */
void SettingsVolume(void)
{
  int volume;

  if (InputPressed(BUTTON_LEFT) != 0) {
    volume = g_state.save.volume;
    volume = (volume + 2) % VOLUME_LEVELS;
    g_state.save.volume = volume;
    BeepSetOutputMode(g_state.save.volume);
    BeepLoadScore(SCORE_MOVE);
  }
  if (InputPressed(BUTTON_RIGHT) != 0) {
    volume = g_state.save.volume;
    volume = (volume + 1) % VOLUME_LEVELS;
    g_state.save.volume = volume;
    BeepSetOutputMode(g_state.save.volume);
    BeepLoadScore(SCORE_MOVE);
  }
}

/* Left decrements any nonzero contrast; right increments values below 9.
 * Every left/right press sends the current value to the LCD. */
void SettingsContrast(void)
{
  u8 settingsBits;

  if (InputPressed(BUTTON_LEFT) != 0) {
    settingsBits = ((const u8 *)&g_state.save)[SAVE_SETTINGS_OFFSET];
    if ((settingsBits & SAVE_CONTRAST_MASK) != 0) {
      g_state.save.contrast--;
      BeepLoadScore(SCORE_MOVE);
    }
    DisplaySetContrast(g_state.save.contrast);
  }
  if (InputPressed(BUTTON_RIGHT) != 0) {
    settingsBits = ((const u8 *)&g_state.save)[SAVE_SETTINGS_OFFSET];
    if ((settingsBits & SAVE_CONTRAST_MASK) < SAVE_CONTRAST_MAX_BITS) {
      g_state.save.contrast++;
      BeepLoadScore(SCORE_MOVE);
    }
    DisplaySetContrast(g_state.save.contrast);
  }
}

/* Center opens the selected edit page. On an edit page, center saves the
 * settings already applied to the hardware and returns home. */
void SettingsUpdate(void)
{
  u8 page;

  page = g_ui.view.settings.page;
  switch (page) {
  case SETTINGS_PAGE_SELECT:
    SettingsNavigate();
    break;
  case SETTINGS_PAGE_VOLUME:
    SettingsVolume();
    break;
  case SETTINGS_PAGE_CONTRAST:
    SettingsContrast();
    break;
  }

  if (InputPressed(BUTTON_CENTER) == 0) {
    return;
  }
  if (g_ui.view.settings.page == SETTINGS_PAGE_SELECT) {
    BeepLoadScore(SCORE_CONFIRM);
    g_ui.view.settings.page = (g_ui.view.settings.selection + 1);
  } else {
    BeepLoadScore(SCORE_CONFIRM);
    HomeInit();
    SetView(VIEW_HOME);
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
  }
}

void SettingsRender(void)
{
  UiResources *resources;
  u8 *raster;
  u8 *eepromSource;
  u8 cursorX;
  int i;

  resources = (UiResources *)EEPROM_UI;
  ScratchReset();
  raster = ScratchAlloc(sizeof(resources->menuLabels) / MENU_COUNT);
  eepromSource =
      &resources->menuLabels[MENU_SETTINGS *
                             (sizeof(resources->menuLabels) / MENU_COUNT)];
  EepromRead((u16)eepromSource, raster,
             sizeof(resources->menuLabels) / MENU_COUNT);
  DisplayBlit(8, 0, 80, 16, raster);
  eepromSource = resources->volume;
  EepromRead((u16)eepromSource, raster, sizeof(resources->volume));
  DisplayBlit(8, 16, 40, 16, raster);
  eepromSource = resources->contrast;
  EepromRead((u16)eepromSource, raster, sizeof(resources->contrast));
  DisplayBlit(56, 16, 40, 16, raster);
  eepromSource = resources->cursorArrows;
  EepromRead((u16)eepromSource, raster, sizeof(resources->cursorArrows));
  cursorX = (g_ui.view.settings.selection * 48);

  switch (g_ui.view.settings.page) {
  case SETTINGS_PAGE_SELECT:
    DisplayBlit(cursorX, 20, 8, 8,
                raster + CURSOR_OFFSET(CURSOR_RIGHT, g_state.uiFrame & 1));
    break;
  case SETTINGS_PAGE_VOLUME:
    DisplayBlit(cursorX, 20, 8, 8,
                raster + CURSOR_OFFSET(CURSOR_RIGHT, CURSOR_OUTLINE));
    DisplayBlit((g_state.save.volume * 32), 44, 8, 8,
                raster + CURSOR_OFFSET(CURSOR_RIGHT, g_state.uiFrame & 1));
    eepromSource = resources->soundLevels[0];
    EepromRead((u16)eepromSource, raster, sizeof(resources->soundLevels));
    DisplayBlit(8, 40, 24, 16, raster);
    DisplayBlit(40, 40, 24, 16, raster + SOUND_LEVEL_RASTER_BYTES);
    DisplayBlit(72, 40, 24, 16, raster + 2 * SOUND_LEVEL_RASTER_BYTES);
    break;
  case SETTINGS_PAGE_CONTRAST:
    DisplayBlit((g_state.save.contrast * 8 + 8), 32, 8, 8,
                raster + CURSOR_OFFSET(CURSOR_DOWN, g_state.uiFrame & 1));
    DisplayBlit((g_ui.view.settings.selection * 48), 20, 8, 8,
                raster + CURSOR_OFFSET(CURSOR_RIGHT, CURSOR_OUTLINE));
    eepromSource = resources->contrastSample;
    EepromRead((u16)eepromSource, raster, sizeof(resources->contrastSample));
    for (i = 0; i < 10; i++) {
      DisplayBlit((i * 8 + 8), 40, 8, 16, raster);
    }
    break;
  }

  eepromSource = resources->returnArrow;
  EepromRead((u16)eepromSource, raster, NAVIGATION_ICON_BYTES);
  DisplayBlit(0, 0, 8, 16, raster);
  RenderBattery(88, 0);
}

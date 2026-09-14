#include "types.h"
#include "eeprom_address.h"
#include "project.h"
#include "application/pw_nt7508.h"
#include "application/pw_buzzer.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_home.h"
#include "application/pw_pictogram_menu.h"
#include "application/pw_player_input.h"
#include "support/lib_common.h"
#include "support/scratch.h"
#include "application/pw_trainer.h"

void TrainerTime(void);
void TrainerHistory(void);
void TrainerStepCapMetrics(void);
void TrainerStepCapReward(void);

/* The span covers navigation artwork, the gap and following status icons. */
#define TRAINER_NAVIGATION_READ_BYTES 192

void TrainerInit(void)
{
  u8 zero;

  zero = 0;
  g_ui.view.trainer.page = zero;
  g_ui.view.trainer.historyDaysAgo = zero;
}

/* Acknowledging the step-cap panel unlocks the reward once. Later visits
 * return home on acknowledgment. */
void TrainerUpdate(void)
{
  u8 rewardFlags;
  u16 prefixWordAddress;
  enum {
    REWARD_FLAG_OFFSET =
        WALK_REWARD_FLAGS_OFFSET - offsetof(WalkData, opaqueWords[1])
  };

  if (g_ui.view.trainer.page == TRAINER_PAGE_CARD) {
    if (InputPressed(BUTTON_LEFT) != 0) {
      if (g_ui.view.trainer.historyDaysAgo == TRAINER_CURRENT_DAY) {
        BeepLoadScore(SCORE_BACK);
        MenuReset();
        SetView(VIEW_MAIN_MENU);
        return;
      }
      g_ui.view.trainer.historyDaysAgo--;
      BeepLoadScore(SCORE_MOVE);
    }
    if ((InputPressed(BUTTON_RIGHT) != 0) &&
        (g_ui.view.trainer.historyDaysAgo < TRAINER_HISTORY_DAYS)) {
      g_ui.view.trainer.historyDaysAgo++;
      BeepLoadScore(SCORE_MOVE);
    }
  }

  if (InputPressed(BUTTON_CENTER) == 0) {
    return;
  }

  switch (g_ui.view.trainer.page) {
  case TRAINER_PAGE_CARD:
    if (g_state.save.totalSteps < TOTAL_STEPS_MAX) {
      BeepLoadScore(SCORE_CONFIRM);
      HomeInit();
      SetView(VIEW_HOME);
      return;
    }
    g_ui.view.trainer.page = TRAINER_PAGE_STEP_CAP;
    BeepLoadScore(SCORE_CONFIRM);
    return;
  case TRAINER_PAGE_STEP_CAP:
    prefixWordAddress =
        PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, opaqueWords[1]);
    rewardFlags = EepromReadByte(prefixWordAddress + REWARD_FLAG_OFFSET);
    if ((rewardFlags & STEP_CAP_REWARD_UNLOCKED) == 0) {
      EepromWriteByte(prefixWordAddress + REWARD_FLAG_OFFSET,
                      (rewardFlags | STEP_CAP_REWARD_UNLOCKED));
      g_ui.view.trainer.page = TRAINER_PAGE_REWARD;
      BeepLoadScore(SCORE_SUCCESS);
      return;
    }
    BeepLoadScore(SCORE_CONFIRM);
    HomeInit();
    SetView(VIEW_HOME);
    return;
  case TRAINER_PAGE_REWARD:
    BeepLoadScore(SCORE_CONFIRM);
    HomeInit();
    SetView(VIEW_HOME);
    return;
  }
}

/* Draw the current packed-BCD clock. */
void TrainerTime(void)
{
  UiResources *resources;
  u8 *raster;
  u8 *eepromSource;
  u16 bufferBytes;
  u16 iconLength;

  resources = (UiResources *)EEPROM_UI;
  iconLength = sizeof(resources->trainerIcon);
  ScratchReset();
  /* One text row or the ten decimal glyphs fill the same work buffer. */
  bufferBytes = TEXT_RASTER_BYTES;
  raster = ScratchAlloc(bufferBytes);
  eepromSource = resources->menuLabels +
                 MENU_TRAINER * (sizeof(resources->menuLabels) / MENU_COUNT);
  EepromRead((u16)eepromSource, raster, bufferBytes);
  DisplayBlit(8, 0, 0x50, 0x10, raster);
  eepromSource = resources->trainerIcon;
  EepromRead((u16)eepromSource, raster, iconLength);
  DisplayBlit(0x00, 0x10, 0x10, 0x10, raster);
  eepromSource = resources->trainerName;
  EepromRead((u16)eepromSource, raster, bufferBytes);
  DisplayBlit(0x10, 0x10, 0x50, 0x10, raster);
  eepromSource = resources->courseIcon;
  EepromRead((u16)eepromSource, raster, iconLength);
  DisplayBlit(0x00, 0x20, 0x10, 0x10, raster);
  if (g_state.save.bonusCourse) {
    eepromSource = ((BonusResources *)EEPROM_BONUS_COURSE)->courseName;
  } else {
    eepromSource = ((CourseResources *)EEPROM_COURSE)->courseName;
  }
  EepromRead((u16)eepromSource, raster, bufferBytes);
  DisplayBlit(0x10, 0x20, 0x50, 0x10, raster);
  eepromSource = resources->leftArrow;
  EepromRead((u16)eepromSource, raster, TRAINER_NAVIGATION_READ_BYTES);
  DisplayBlit(0, 0, 0x08, 0x10, raster + 2 * NAVIGATION_ICON_BYTES);
  DisplayBlit(0x58, 0x00, 0x08, 0x10, raster + NAVIGATION_ICON_BYTES);
  eepromSource = resources->timeLabel;
  EepromRead((u16)eepromSource, raster, sizeof(resources->timeLabel));
  DisplayBlit(0x00, 0x30, 0x20, 0x10, raster);
  eepromSource = resources->digits;
  EepromRead((u16)eepromSource, raster, bufferBytes);
  DisplayBlit(0x20, 0x30, 0x08, 0x10,
              raster +
                  ((g_state.time.hourBcd24h >> 4) & 7) * NUMBER_GLYPH_BYTES);
  DisplayBlit(0x28, 0x30, 0x08, 0x10,
              raster + (g_state.time.hourBcd24h & 0x0f) * NUMBER_GLYPH_BYTES);
  DisplayBlit(0x38, 0x30, 0x08, 0x10,
              raster +
                  ((g_state.time.minuteBcd >> 4) & 7) * NUMBER_GLYPH_BYTES);
  DisplayBlit(0x40, 0x30, 0x08, 0x10,
              raster + (g_state.time.minuteBcd & 0x0f) * NUMBER_GLYPH_BYTES);
  DisplayBlit(0x50, 0x30, 0x08, 0x10,
              raster +
                  ((g_state.time.secondBcd >> 4) & 7) * NUMBER_GLYPH_BYTES);
  DisplayBlit(0x58, 0x30, 0x08, 0x10,
              raster + (g_state.time.secondBcd & 0x0f) * NUMBER_GLYPH_BYTES);
  EepromRead((u16)(resources->digits + sizeof(resources->digits)), raster,
             sizeof(resources->colon));
  DisplayBlit(0x30, 0x30, 0x08, 0x10, raster);
  DisplayBlit(0x48, 0x30, 0x08, 0x10, raster);
}

void TrainerHistory(void)
{
  UiResources *resources;
  WalkData *data;
  u8 *raster;
  u8 *eepromSource;
  u16 length;
  u32 historySteps;

  length = TEXT_RASTER_BYTES;
  resources = (UiResources *)EEPROM_UI;
  ScratchReset();
  raster = ScratchAlloc(length);
  eepromSource = resources->leftArrow;
  EepromRead((u16)eepromSource, raster, TRAINER_NAVIGATION_READ_BYTES);
  DisplayBlit(0, 0, 0x08, 0x10, raster);
  if (g_ui.view.trainer.historyDaysAgo < TRAINER_HISTORY_DAYS) {
    DisplayBlit(0x58, 0x00, 0x08, 0x10, raster + NAVIGATION_ICON_BYTES);
  }
  eepromSource = resources->daysLabel;
  EepromRead((u16)eepromSource, raster, sizeof(((UiResources *)0)->daysLabel));
  DisplayBlit(0x28, 0x00, 0x28, 0x10, raster);
  eepromSource = resources->totalDays;
  EepromRead((u16)eepromSource, raster, sizeof(((UiResources *)0)->totalDays));
  DisplayBlit(0x00, 0x20, 0x40, 0x10, raster);
  eepromSource = resources->steps;
  EepromRead((u16)eepromSource, raster, sizeof(((UiResources *)0)->steps));
  DisplayBlit(0x38, 0x10, 0x28, 0x10, raster);
  DisplayBlit(0x38, 0x30, 0x28, 0x10, raster);
  eepromSource = resources->hyphen;
  EepromRead((u16)eepromSource, raster, NUMBER_GLYPH_BYTES);
  DisplayBlit(0x18, 0x00, 0x08, 0x10, raster);
  eepromSource = resources->digits;
  EepromRead((u16)eepromSource, raster, DECIMAL_GLYPHS_BYTES);
  DisplayBlit(0x20, 0x00, 0x08, 0x10,
              raster + g_ui.view.trainer.historyDaysAgo * NUMBER_GLYPH_BYTES);
  data = (WalkData *)EEPROM_WALK;
  EepromRead((u16)&data->dailySteps[g_ui.view.trainer.historyDaysAgo - 1],
             &historySteps, sizeof(historySteps));
  RenderDecimal(0x30, 0x10, historySteps, NUMBER_NO_RULE);
  RenderDecimal(0x58, 0x20, g_state.save.days, NUMBER_NO_RULE);
  RenderDecimal(0x30, 0x30, g_state.save.totalSteps, NUMBER_NO_RULE);
}

/* The step-cap panel reports the saved elapsed-hours counter. */
void TrainerStepCapMetrics(void)
{
  UiResources *resources;
  u8 *raster;
  u16 length;
  u8 column;

  length = TEXT_RASTER_BYTES;
  resources = (UiResources *)EEPROM_UI;
  ScratchReset();
  raster = ScratchAlloc(length);
  EepromRead((u16)resources->digits, raster, DECIMAL_GLYPHS_BYTES);
  column = 0;
  do {
    DisplayBlit((column * 8), 0x08, 0x08, 0x10,
                raster + 9 * NUMBER_GLYPH_BYTES);
    column++;
  } while (column < 7);
  EepromRead((u16)resources->steps, raster, sizeof(((UiResources *)0)->steps));
  DisplayBlit(0x38, 0x08, 0x28, 0x10, raster);
  EepromRead((u16)resources->hoursLabel, raster,
             sizeof(((UiResources *)0)->hoursLabel));
  DisplayBlit(0x38, 0x28, 0x28, 0x10, raster);
  RenderDecimal(0x30, 0x28, g_state.save.elapsedHours, NUMBER_NO_RULE);
  RenderMessage(0x18, MESSAGE_ENDING_SECOND,
                BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                MESSAGE_BLINK_PROMPT);
}

void TrainerStepCapReward(void)
{
  u8 *raster;

  ScratchReset();
  raster = ScratchAlloc(sizeof(((UiResources *)0)->itemTreasure));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, itemTreasure),
             raster, sizeof(((UiResources *)0)->itemTreasure));
  DisplayBlit(0x20, 0x04, 0x20, 0x18, raster);
  RenderMessage(0x20, MESSAGE_ENDING_FIRST,
                BORDER_TOP | BORDER_LEFT | BORDER_RIGHT, MESSAGE_NO_PROMPT);
  RenderMessage(0x30, MESSAGE_RECEIVED,
                BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                MESSAGE_BLINK_PROMPT);
}

void TrainerRender(void)
{
  switch (g_ui.view.trainer.page) {
  case TRAINER_PAGE_CARD:
    if (g_ui.view.trainer.historyDaysAgo == TRAINER_CURRENT_DAY) {
      TrainerTime();
    } else {
      TrainerHistory();
    }
    break;
  case TRAINER_PAGE_STEP_CAP:
    TrainerStepCapMetrics();
    break;
  case TRAINER_PAGE_REWARD:
    TrainerStepCapReward();
    break;
  }
  RenderBattery(0x58, 0x00);
}

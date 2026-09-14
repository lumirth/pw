#include "types.h"
#include "eeprom_address.h"
#include <stddef.h>
#include "project.h"
#include "application/pw_nt7508.h"
#include <machine.h>
#include "application/pw_battle.h"
#include "application/pw_buzzer.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_home.h"
#include "application/pw_player_input.h"
#include "support/lib_common.h"
#include "application/pw_pokeradar.h"
#include "support/scratch.h"

extern const u8 g_radarResponseUpdates[];
extern const u8 g_radarPatchX[4];
extern const u8 g_radarDelayRanges[];
extern const u8 g_radarBubbleByRound[];

void RadarChooseEncounter(void);
void RadarPollGrass(void);

#define RADAR_SELECT 0
#define RADAR_BATTLE_TRANSITION 1
/* Watt-result display; no search phase selects it. */
#define RADAR_WATT_RESULT 2
#define RADAR_REVEAL 3

/* Try the bonus encounter first. Ordinary encounters share one chance draw
 * across their step gates; the third encounter is the fallback. */
void RadarChooseEncounter(void)
{
  u8 receiptIndex;
  DeviceStatus *buffer;
  EncounterRule *rule;
  u8 eventPresence;
  u32 randomValue;
  u8 chanceRoll;
  Course *course;
  u8 index;

  g_ui.view.radar.encounter = 0;
  if (g_state.save.bonusCourse != 0) {
    receiptIndex = EepromReadByte(PW_EEPROM_MEMBER_ADDRESS(
        EEPROM_BONUS_COURSE, BonusResources, values.pokemonReceipt));
    ScratchReset();
    buffer = ScratchAlloc(sizeof(DeviceStatus));
    if (StatusLoadReceived(buffer, receiptIndex) == 0) {
      eventPresence = EepromReadByte(EEPROM_EVENTS);
      if ((eventPresence & EVENT_PRESENT_POKEMON) == 0) {
        ScratchReset();
        rule = ScratchAlloc(sizeof(EncounterRule));
        EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_BONUS_COURSE, BonusResources,
                                            values.encounterRule),
                   rule, sizeof(*rule));
        if (g_state.dailySteps >=
            ((rule->stepsLe >> 8) | (rule->stepsLe << 8))) {
          randomValue = RandomNext() % 100ul;
          if (rule->chance > randomValue) {
            g_ui.view.radar.encounter = 4;
            randomValue = RandomNext();
            g_ui.view.radar.roundCount = (((randomValue >> 3) & 1) + 3);
            return;
          }
        }
      }
    }
  }

  ScratchReset();
  /* This inventory read has no consumer before Course replaces it in scratch.
   */
  buffer = ScratchAlloc(sizeof(((WalkData *)0)->pokemon));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon), buffer,
             sizeof(((WalkData *)0)->pokemon));
  chanceRoll = (RandomNext() % 100ul);
  ScratchReset();
  course = ScratchAlloc(sizeof(Course));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources, values),
             course, sizeof(Course));
  index = 0;
  while (index < 3) {
    if (g_state.dailySteps >= ((course->encounterStepsLe[index] >> 8) |
                               (course->encounterStepsLe[index] << 8))) {
      if (chanceRoll < course->encounterChance[index]) {
        g_ui.view.radar.encounter = (index + 1);
        randomValue = RandomNext();

        {
          /* Course indices 0, 1, 2 require 3-4, 2-3, 1-2 correct patches. */
          u8 draw = ((randomValue >> 3) & 1);
          index = -index;
          index += draw;
          index += 3;
        }
        g_ui.view.radar.roundCount = index;
        return;
      }
    }
    index++;
  }
  g_ui.view.radar.encounter = 3;
  randomValue = RandomNext();
  g_ui.view.radar.roundCount = (((randomValue >> 3) & 1) + 1);
}

/* The chosen encounter stays in the shared UI byte used by battle. */
void RadarInit(void)
{
  RadarChooseEncounter();
  g_ui.view.radar.phase = RADAR_SELECT;
  g_ui.view.radar.selectedPatch = 0;
  g_ui.view.radar.round = 0;
  g_ui.view.radar.delayUpdates = 5;
  g_ui.view.radar.phaseCounter = g_radarResponseUpdates[g_ui.view.radar.round];
  g_ui.view.radar.activePatch = ((((u16)RandomNext() << 3) >> 8) & 3);
  g_ui.view.radar.revealCounter = 0;
}

/* Wrap among four patches. A wrong first-round choice can be retried; a
 * later mistake ends the search. The delay and response window count polls. */
void RadarPollGrass(void)
{
  if (InputPressed(BUTTON_LEFT) != 0) {
    g_ui.view.radar.selectedPatch = ((g_ui.view.radar.selectedPatch + 3) & 3);
    BeepLoadScore(SCORE_MOVE);
  }
  if (InputPressed(BUTTON_RIGHT) != 0) {
    g_ui.view.radar.selectedPatch = ((g_ui.view.radar.selectedPatch + 1) & 3);
    BeepLoadScore(SCORE_MOVE);
  }
  if ((InputPressed(BUTTON_CENTER) != 0) &&
      (g_ui.view.radar.phaseCounter != 0)) {
    /* Confirmation is accepted during the hidden delay as well as the
     * visible response window. Only polls without confirmation spend time. */
    if (g_ui.view.radar.selectedPatch == g_ui.view.radar.activePatch) {
      BeepLoadScore(SCORE_RADAR_RESPONSE);
      g_ui.view.radar.phase = RADAR_REVEAL;
      g_ui.view.radar.revealCounter = 0x10;
      return;
    }
    if (g_ui.view.radar.round == 0) {
      BeepLoadScore(SCORE_FAILURE);
      return;
    }
  } else {
    {
      u8 delayUpdates;

      delayUpdates = g_ui.view.radar.delayUpdates;
      if (delayUpdates != 0) {
        delayUpdates--;
        g_ui.view.radar.delayUpdates = delayUpdates;
        return;
      }
    }
    {
      u8 responseUpdates;

      responseUpdates = g_ui.view.radar.phaseCounter;
      if (responseUpdates != 0) {
        responseUpdates--;
        g_ui.view.radar.phaseCounter = responseUpdates;
      }
      if (g_ui.view.radar.phaseCounter != 0) {
        return;
      }
    }
  }
  BeepLoadScore(SCORE_ESCAPE);
  SetView(VIEW_RADAR_FAILURE);
}

/* After each reveal, start another round or close the screen for battle.
 * RadarRender advances the closing animation's phaseCounter. */
void RadarUpdate(void)
{
  u32 randomValue;

  /* Sounds suspend both input polling and reveal countdown. */
  if (BeepHasScore() != 0) {
    return;
  }
  switch (g_ui.view.radar.phase) {
  case RADAR_SELECT:
    RadarPollGrass();
    return;
  case RADAR_BATTLE_TRANSITION:
    if (g_ui.view.radar.phaseCounter <= 4) {
      return;
    }
    BattleInit();
    SetView(VIEW_BATTLE);
    return;
  case RADAR_WATT_RESULT:
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    BeepLoadScore(SCORE_CONFIRM);
    HomeInit();
    SetView(VIEW_HOME);
    return;
  case RADAR_REVEAL:
    if (g_ui.view.radar.revealCounter == 0) {
      return;
    }
    if (--g_ui.view.radar.revealCounter != 0) {
      return;
    }
    if (g_ui.view.radar.round >= (g_ui.view.radar.roundCount - 1)) {
      g_ui.view.radar.phase = RADAR_BATTLE_TRANSITION;
      g_ui.view.radar.revealCounter = 1;
      g_ui.view.radar.phaseCounter = 0;
      return;
    }
    g_ui.view.radar.phase = RADAR_SELECT;
    randomValue = RandomNext();
    g_ui.view.radar.delayUpdates =
        ((u8)(randomValue >> 2) % g_radarDelayRanges[g_ui.view.radar.round] +
         0x10);
    g_ui.view.radar.phaseCounter =
        g_radarResponseUpdates[++g_ui.view.radar.round];
    randomValue = RandomNext();
    g_ui.view.radar.activePatch = ((randomValue >> 5) & 3);
    return;
  }
}

void RadarRender(void)
{
  const UiResources *image;
  u8 *raster;
  const u8 *resourceAddress;
  int patch;

  image = (const UiResources *)EEPROM_UI;
  ScratchReset();
  raster = ScratchAlloc(sizeof(image->radarIndicators));
  resourceAddress =
      image->cursorArrows + CURSOR_OFFSET(CURSOR_RIGHT, g_state.uiFrame & 1);
  EepromRead((u16)resourceAddress, raster, 0x10);
  DisplayBlit((g_radarPatchX[g_ui.view.radar.selectedPatch] - 8),
              ((g_ui.view.radar.selectedPatch & 1) * 0x18 + 8), 0x08, 0x08,
              raster);
  resourceAddress = image->radarGrass;
  EepromRead((u16)resourceAddress, raster, sizeof(image->radarGrass));
  patch = 0;
  do {
    DisplayBlit(g_radarPatchX[patch], ((patch & 1) * 0x18), 0x20, 0x18, raster);
    patch++;
  } while (patch < 4);

  if (g_ui.view.radar.revealCounter != 0) {
    resourceAddress = image->radarIndicators.bubbles;
    EepromRead((u16)resourceAddress, raster, sizeof(image->radarIndicators));
    DisplayBlit((g_radarPatchX[g_ui.view.radar.activePatch] + 0x10),
                ((g_ui.view.radar.activePatch & 1) * 0x18), 0x10, 0x10,
                raster + sizeof(image->radarIndicators.bubbles));
    switch (g_ui.view.radar.phase) {
    case RADAR_REVEAL:
      RenderMessage(0x30, MESSAGE_RADAR_RESPONSE,
                    BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                    MESSAGE_NO_PROMPT);
      break;
    case RADAR_BATTLE_TRANSITION:
      DisplayFillRect(0, 0, 0x60, (g_ui.view.radar.phaseCounter * 8), 3);
      DisplayFillRect(0, (0x40 - g_ui.view.radar.phaseCounter * 8), 0x60,
                      (g_ui.view.radar.phaseCounter * 8), 3);
      g_ui.view.radar.phaseCounter++;
      break;
    case RADAR_WATT_RESULT:
      RenderWatts(0x02, 0x20, g_ui.view.radar.encounter, 0x0d);
      RenderMessage(0x30, MESSAGE_RECEIVED,
                    BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                    MESSAGE_BLINK_PROMPT);
      break;
    }
  } else {
    RenderMessage(0x30, MESSAGE_RADAR_SEARCH,
                  BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_NO_PROMPT);
    if (g_ui.view.radar.delayUpdates == 0) {
      resourceAddress = image->radarIndicators.bubbles;
      EepromRead((u16)resourceAddress, raster, sizeof(image->radarIndicators));
      DisplayBlit((g_radarPatchX[g_ui.view.radar.activePatch] + 0x10),
                  ((g_ui.view.radar.activePatch & 1) * 0x18), 0x10, 0x10,
                  raster + g_radarBubbleByRound[g_ui.view.radar.round] * 64);
    }
  }
  RenderBattery(0, 0);
}

/* Return home when any button acknowledges the escape screen. */
void RadarFailureUpdate(void)
{
  if (InputPressed(BUTTON_ANY) != 0) {
    g_ui.view.radar.selectedPatch = 0;
    BeepLoadScore(SCORE_FAILURE);
    HomeInit();
    SetView(VIEW_HOME);
  }
}

void RadarFailureRender(void)
{
  UiResources *image;
  u8 *raster;
  u8 *resourceAddress;
  int patch;

  image = (UiResources *)EEPROM_UI;
  ScratchReset();
  raster = ScratchAlloc(sizeof(image->radarGrass));
  resourceAddress = image->radarGrass;
  EepromRead((u16)resourceAddress, raster, sizeof(image->radarGrass));
  patch = 0;
  do {
    DisplayBlit(g_radarPatchX[patch], ((patch & 1) * 0x18), 0x20, 0x18, raster);
    patch++;
  } while (patch < 4);
  RenderMessage(0x30, MESSAGE_RADAR_ESCAPE,
                BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                MESSAGE_BLINK_PROMPT);
  RenderBattery(0, 0);
}

/* Response window in selection updates after the initial delay. */
const u8 g_radarResponseUpdates[4] = {0x40, 0x30, 0x20, 0x18};

/* Random delay ranges, added to the minimum delay of 16 updates. */
const u8 g_radarDelayRanges[3] = {0x10, 0x20, 0x30};

/* Bubble selected by search round, before the encounter is revealed. */
const u8 g_radarBubbleByRound[4] = {0x00, 0x00, 0x01, 0x02};

/* Patch X positions; odd indices occupy the lower row. */
const u8 g_radarPatchX[4] = {0x08, 0x10, 0x38, 0x40};

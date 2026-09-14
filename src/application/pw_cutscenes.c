#include "types.h"
#include "eeprom_address.h"
#include "project.h"
#include "application/pw_nt7508.h"
#include "application/pw_diary.h"
#include <machine.h>
#include "application/pw_buzzer.h"
#include "application/pw_cutscenes.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_home.h"
#include "support/lib_common.h"
#include "support/scratch.h"

extern const s8 g_arrivalDropY[];

typedef struct {
  u8 x;
  u8 y;
} SparkleXY;

extern const SparkleXY g_arrivalSparkleCoordinates3[];
extern const s8 g_departureRiseY[];

void ArrivalDrop(void);
void BallSparkle(void);
void ArrivalCloud(void);
void ArrivalPokemon(void);
void ArrivalComplete(void);
void RewardInfo(void);
void DeparturePokemon(void);
void DepartureCloud(void);
void DepartureRise(void);
void DepartureComplete(void);
void CommunicationComplete(void);

#define PW_PANEL_RASTER_SIZE 0x180

/* Arrival, departure and reward views each give stage its own encoding.
 * Render helpers advance frame; BallSparkle and DepartureCloud also advance
 * stage. Completion helpers return home after the display interval and sound
 * finish. */

/* Drop the transfer sprite or event ball between the screen borders. */
void ArrivalDrop(void)
{
  u8 *raster;
  u8 *resourceAddress;

  ScratchReset();
  raster = ScratchAlloc(PW_PANEL_RASTER_SIZE);
  if (g_state.view == VIEW_EVENT_REWARD) {
    resourceAddress = ((UiResources *)EEPROM_UI)->ball;
  } else {
    resourceAddress = ((UiResources *)EEPROM_UI)->transferSprite;
  }
  EepromRead((u16)resourceAddress, raster, RASTER_BYTES(8, 8));
  DisplayBlit(0x2c, g_arrivalDropY[g_ui.view.presentation.frame], 0x08, 0x08,
              raster);
  DisplayFillRect(0, 0, 0x60, 8, 3);
  DisplayFillRect(0x00, 0x38, 0x60, 8, 3);
  g_ui.view.presentation.frame++;
}

/* Advance to the cloud stage after three sparkle frames. */
void BallSparkle(void)
{
  u8 *raster;
  u8 *resourceAddress;

  ScratchReset();
  raster = ScratchAlloc(PW_PANEL_RASTER_SIZE);
  resourceAddress = ((UiResources *)EEPROM_UI)->ball;
  EepromRead((u16)resourceAddress, raster, sizeof(((UiResources *)0)->ball));
  DisplayBlit(0x2c, 0x10, 0x08, 0x08, raster);
  resourceAddress = ((UiResources *)EEPROM_UI)->star;
  EepromRead((u16)resourceAddress, raster, sizeof(((UiResources *)0)->star));
  DisplayBlit(g_arrivalSparkleCoordinates3[g_ui.view.presentation.frame].x,
              g_arrivalSparkleCoordinates3[g_ui.view.presentation.frame].y,
              0x08, 0x08, raster);
  DisplayFillRect(0, 0, 0x60, 8, 3);
  DisplayFillRect(0x00, 0x38, 0x60, 8, 3);
  g_ui.view.presentation.frame++;
  if (g_ui.view.presentation.frame > 2) {
    g_ui.view.presentation.stage = EVENT_REWARD_CLOUD;
    g_ui.view.presentation.frame = 0;
  }
}

void ArrivalCloud(void)
{
  u8 *raster;
  u8 *resourceAddress;

  ScratchReset();
  raster = ScratchAlloc(PW_PANEL_RASTER_SIZE);
  resourceAddress = ((UiResources *)EEPROM_UI)->cloud;
  EepromRead((u16)resourceAddress, raster, sizeof(((UiResources *)0)->cloud));
  DisplayBlit(0x20, 0x10, 0x20, 0x18, raster);
  DisplayFillRect(0, 0, 0x60, 8, 3);
  DisplayFillRect(0x00, 0x38, 0x60, 8, 3);
  if (g_ui.view.presentation.frame == 0) {
    g_ui.view.presentation.frame++;
  }
}

void ArrivalPokemon(void)
{
  RenderLargePokemon(0x10, 0x08);
  DisplayFillRect(0, 0, 0x60, 8, 3);
  DisplayFillRect(0x00, 0x38, 0x60, 8, 3);
  g_ui.view.presentation.frame++;
}

void ArrivalComplete(void)
{
  RenderHeldPokemon(0x20, 0x04);
  RenderHeldName(0x00, 0x20, BORDER_TOP | BORDER_LEFT);
  RenderMessage(0x30, MESSAGE_POKEMON_RECEIVED,
                BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT, MESSAGE_NO_PROMPT);
  if (g_ui.view.presentation.frame < 0x10) {
    g_ui.view.presentation.frame++;
  }
  if ((BeepHasScore() == 0) && (g_ui.view.presentation.frame > 8)) {
    HomeInit();
    SetView(VIEW_HOME);
  }
}

void RewardInfo(void)
{
  u8 *raster;
  UiResources *image;
  u8 *resourceAddress;
  u16 length;
  u16 iconLength;

  image = (UiResources *)EEPROM_UI;
  iconLength = (sizeof(image->stamps) / 4);
  ScratchReset();
  length = PW_PANEL_RASTER_SIZE;
  raster = ScratchAlloc(length);
  switch (g_ui.view.presentation.rewardKind) {
  case PW_EVENT_REWARD_MAP:
    resourceAddress = image->itemMap;
    EepromRead((u16)resourceAddress, raster, sizeof(image->itemMap));
    DisplayBlit(0x20, 0x04, 0x20, 0x18, raster);
    RenderMessage(0x20, MESSAGE_SPECIAL_MAP,
                  BORDER_TOP | BORDER_LEFT | BORDER_RIGHT, MESSAGE_NO_PROMPT);
    break;
  case PW_EVENT_REWARD_POKEMON:
    EepromRead((g_state.uiFrame & 1) * 0xc0 +
                   PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_POKEMON, EventPokemon,
                                            pokemonImage),
               raster, length);
    DisplayBlit(0x20, 0x08, 0x20, 0x18, raster);
    RenderDistributionName(0x00, 0x20, BORDER_TOP | BORDER_LEFT);
    break;
  case PW_EVENT_REWARD_ITEM:
    RenderTreasureIcon(0x20, 0x04);
    RenderDistributionItem(0x00, 0x20, BORDER_TOP | BORDER_LEFT | BORDER_RIGHT);
    break;
  case PW_EVENT_REWARD_COURSE:
    resourceAddress = image->itemMap;
    EepromRead((u16)resourceAddress, raster, sizeof(image->itemMap));
    DisplayBlit(0x20, 0x04, 0x20, 0x18, raster);
    RenderMessage(0x20, MESSAGE_SPECIAL_COURSE,
                  BORDER_TOP | BORDER_LEFT | BORDER_RIGHT, MESSAGE_NO_PROMPT);
    break;
  case PW_EVENT_REWARD_STAMP0:
    resourceAddress = image->stamps;
    EepromRead((u16)resourceAddress, raster, iconLength);
    DisplayBlit(0x2c, 0x10, 0x08, 0x08, raster);
    RenderMessage(0x20, MESSAGE_STAMP, BORDER_TOP | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_NO_PROMPT);
    break;
  case PW_EVENT_REWARD_STAMP1:
    resourceAddress = image->stamps + (sizeof(image->stamps) / 4);
    EepromRead((u16)resourceAddress, raster, iconLength);
    DisplayBlit(0x2c, 0x10, 0x08, 0x08, raster);
    RenderMessage(0x20, MESSAGE_STAMP, BORDER_TOP | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_NO_PROMPT);
    break;
  case PW_EVENT_REWARD_STAMP2:
    resourceAddress = image->stamps + (sizeof(image->stamps) / 4) * 2;
    EepromRead((u16)resourceAddress, raster, iconLength);
    DisplayBlit(0x2c, 0x10, 0x08, 0x08, raster);
    RenderMessage(0x20, MESSAGE_STAMP, BORDER_TOP | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_NO_PROMPT);
    break;
  case PW_EVENT_REWARD_STAMP3:
    resourceAddress = image->stamps + (sizeof(image->stamps) / 4) * 3;
    EepromRead((u16)resourceAddress, raster, iconLength);
    DisplayBlit(0x2c, 0x10, 0x08, 0x08, raster);
    RenderMessage(0x20, MESSAGE_STAMP, BORDER_TOP | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_NO_PROMPT);
    break;
  }

  RenderMessage(0x30, MESSAGE_RECEIVED,
                BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT, MESSAGE_NO_PROMPT);
  if (g_ui.view.presentation.frame < 0x10) {
    g_ui.view.presentation.frame++;
  }
}

void WalkStartUpdate(void)
{
  u8 frame;

  frame = g_ui.view.presentation.frame;
  switch (g_ui.view.presentation.stage) {
  case WALK_START_DROP:
    if (frame > 4) {
      g_ui.view.presentation.stage = WALK_START_CLOUD;
      g_ui.view.presentation.frame = 0;
    }
    break;
  case WALK_START_CLOUD:
    if (frame == 0) {
      break;
    }
    g_ui.view.presentation.stage = WALK_START_POKEMON;
    g_ui.view.presentation.frame = 0;
    BeepLoadScore(SCORE_CONFIRM);
    break;
  case WALK_START_POKEMON:
    if (frame <= 8) {
      break;
    }
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.stage = WALK_START_COMPLETE;
    BeepLoadScore(SCORE_COMPLETION);
    break;
  }
}

void WalkStartRender(void)
{
  switch (g_ui.view.presentation.stage) {
  case WALK_START_DROP:
    ArrivalDrop();
    break;
  case WALK_START_CLOUD:
    ArrivalCloud();
    break;
  case WALK_START_POKEMON:
    ArrivalPokemon();
    break;
  case WALK_START_COMPLETE:
    ArrivalComplete();
    break;
  default:
    break;
  }
  RenderBattery(0, 0);
}

/* IR stores the reward before this sequence starts. Append item/Pokemon
 * receipts to the diary after the result display and sound finish. */
void EventRewardUpdate(void)
{
  u8 frame;
  u16 length;
  Course *course;
  u8 *record;
  u8 encounter;
  EventItem *item;

  frame = g_ui.view.presentation.frame;
  switch (g_ui.view.presentation.stage) {
  case EVENT_REWARD_DROP:
    if (frame <= 4) {
      break;
    }
    g_ui.view.presentation.stage = EVENT_REWARD_SPARKLE;
    g_ui.view.presentation.frame = 0;
    BeepLoadScore(SCORE_CONFIRM);
    break;
  case EVENT_REWARD_CLOUD:
    if (frame == 0) {
      break;
    }
    g_ui.view.presentation.stage = EVENT_REWARD_INFO;
    g_ui.view.presentation.frame = 0;
    BeepLoadScore(SCORE_COMPLETION);
    break;
  case EVENT_REWARD_INFO:
    if (BeepHasScore() != 0) {
      break;
    }
    if (g_ui.view.presentation.frame <= 8) {
      break;
    }
    length = sizeof(Course);
    switch (g_ui.view.presentation.rewardKind) {
    case PW_EVENT_REWARD_POKEMON:
      ScratchReset();
      course = ScratchAlloc(length);
      EepromRead(EEPROM_COURSE, course, length);
      record = ScratchAlloc(sizeof(Pokemon));
      EepromRead(
          PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_POKEMON, EventPokemon, pokemon),
          record, sizeof(Pokemon));
      encounter = DIARY_BONUS_ENCOUNTER;
      DiaryAppend(course, ScratchAlloc(sizeof(DiaryEntry)),
                  DIARY_ACTION_RECEIVED_POKEMON, g_state.save.bonusCourse,
                  encounter, 0);
      break;
    case PW_EVENT_REWARD_ITEM:
      ScratchReset();
      course = ScratchAlloc(length);
      EepromRead(EEPROM_COURSE, course, length);
      item = ScratchAlloc(sizeof(EventItem));
      EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_ITEM, EventItem, header),
                 item, sizeof(EventItem));
      encounter = 0;
      DiaryAppend(course, ScratchAlloc(sizeof(DiaryEntry)),
                  DIARY_ACTION_RECEIVED_ITEM, g_state.save.bonusCourse,
                  encounter, item->header.itemIdLe);
      break;
    default:
      break;
    }
    HomeInit();
    SetView(VIEW_HOME);
    break;
  default:
    break;
  }
}

void EventRewardRender(void)
{
  switch (g_ui.view.presentation.stage) {
  case EVENT_REWARD_DROP:
    ArrivalDrop();
    break;
  case EVENT_REWARD_SPARKLE:
    BallSparkle();
    break;
  case EVENT_REWARD_CLOUD:
    ArrivalCloud();
    break;
  case EVENT_REWARD_INFO:
    RewardInfo();
    break;
  default:
    break;
  }
  RenderBattery(0, 0);
}

void DeparturePokemon(void)
{
  RenderLargePokemon(0x10, 0x08);
  DisplayFillRect(0, 0, 0x60, 8, 3);
  DisplayFillRect(0x00, 0x38, 0x60, 8, 3);
  g_ui.view.presentation.frame++;
}

void DepartureCloud(void)
{
  u8 *raster;
  u8 *resourceAddress;

  ScratchReset();
  raster = ScratchAlloc(PW_PANEL_RASTER_SIZE);
  resourceAddress = ((UiResources *)EEPROM_UI)->cloud;
  EepromRead((u16)resourceAddress, raster, sizeof(((UiResources *)0)->cloud));
  DisplayBlit(0x20, 0x10, 0x20, 0x18, raster);
  DisplayFillRect(0, 0, 0x60, 8, 3);
  DisplayFillRect(0x00, 0x38, 0x60, 8, 3);
  g_ui.view.presentation.stage = WALK_END_RISE;
  g_ui.view.presentation.frame = 0;
}

void DepartureRise(void)
{
  u8 *raster;
  u8 *resourceAddress;

  ScratchReset();
  raster = ScratchAlloc(PW_PANEL_RASTER_SIZE);
  resourceAddress = ((UiResources *)EEPROM_UI)->transferSprite;
  if (g_ui.view.presentation.frame <= 4) {
    EepromRead((u16)resourceAddress, raster,
               sizeof(((UiResources *)0)->transferSprite));
    DisplayBlit(0x2c, g_departureRiseY[g_ui.view.presentation.frame], 0x08,
                0x08, raster);
  }
  DisplayFillRect(0, 0, 0x60, 8, 3);
  DisplayFillRect(0x00, 0x38, 0x60, 8, 3);
  g_ui.view.presentation.frame++;
}

void DepartureComplete(void)
{
  ScratchReset();
  /* RenderHeldName resets scratch before allocating its raster. */
  ScratchAlloc(PW_PANEL_RASTER_SIZE);
  RenderHeldName(0x00, 0x20, BORDER_TOP | BORDER_LEFT);
  RenderMessage(0x30, MESSAGE_POKEMON_RETURNED,
                BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT, MESSAGE_NO_PROMPT);
  if (g_ui.view.presentation.frame < 0x10) {
    g_ui.view.presentation.frame++;
  }
  if ((BeepHasScore() == 0) && (g_ui.view.presentation.frame > 8)) {
    HomeInit();
    SetView(VIEW_HOME);
  }
}

void CommunicationComplete(void)
{
  u8 *raster;
  u8 *resourceAddress;

  ScratchReset();
  raster = ScratchAlloc(sizeof(((UiResources *)0)->walker));
  resourceAddress = ((UiResources *)EEPROM_UI)->walker;
  EepromRead((u16)resourceAddress, raster, sizeof(((UiResources *)0)->walker));
  DisplayBlit(0x20, 0x10, 0x20, 0x20, raster);
  RenderMessage(0x30, MESSAGE_COMMUNICATION_COMPLETE,
                BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                MESSAGE_NO_PROMPT);
  if (g_ui.view.presentation.frame < 0x10) {
    g_ui.view.presentation.frame++;
  }
  if ((BeepHasScore() == 0) && (g_ui.view.presentation.frame > 8)) {
    HomeInit();
    SetView(VIEW_HOME);
  }
}

void WalkEndUpdate(void)
{
  u8 frame;

  frame = g_ui.view.presentation.frame;
  switch (g_ui.view.presentation.stage) {
  case WALK_END_BEGIN:
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.stage = WALK_END_POKEMON;
    BeepLoadScore(SCORE_CONFIRM);
    break;
  case WALK_END_POKEMON:
    if (frame <= 8) {
      break;
    }
    g_ui.view.presentation.frame = 0;
    g_ui.view.presentation.stage = WALK_END_CLOUD;
    break;
  case WALK_END_RISE:
    if (frame < 9) {
      break;
    }
    g_ui.view.presentation.stage = WALK_END_COMPLETE;
    g_ui.view.presentation.frame = 0;
    BeepLoadScore(SCORE_COMPLETION);
    break;
  case WALK_END_COLLECTION_BEGIN:
    g_ui.view.presentation.stage = WALK_END_COLLECTION_COMPLETE;
    g_ui.view.presentation.frame = 0;
    BeepLoadScore(SCORE_COMPLETION);
    break;
  default:
    break;
  }
}

void WalkEndRender(void)
{
  switch (g_ui.view.presentation.stage) {
  case WALK_END_POKEMON:
    DeparturePokemon();
    break;
  case WALK_END_CLOUD:
    DepartureCloud();
    break;
  case WALK_END_RISE:
    DepartureRise();
    break;
  case WALK_END_COMPLETE:
    DepartureComplete();
    break;
  case WALK_END_COLLECTION_BEGIN:
  case WALK_END_COLLECTION_COMPLETE:
    CommunicationComplete();
    break;
  default:
    break;
  }
  RenderBattery(0, 0);
}

/* Arrival Y coordinates, in pixels, indexed by rendered frame. */
const s8 g_arrivalDropY[6] = {0, 2, 6, 12, 20, 16};

/* Sparkle X/Y pixel coordinates for the three rendered frames. */
const SparkleXY g_arrivalSparkleCoordinates3[3] = {{36, 22}, {52, 16}, {42, 8}};

/* Departure Y coordinates, in pixels, indexed by rendered frame. */
const s8 g_departureRiseY[6] = {20, 18, 14, 8, 0, 0};

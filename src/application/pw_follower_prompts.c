#include "types.h"
#include "eeprom_address.h"
#include "project.h"
#include "application/pw_nt7508.h"
#include "application/pw_buzzer.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_follower_prompts.h"
#include "application/pw_home.h"
#include "application/pw_player_input.h"
#include "support/lib_common.h"
#include "support/scratch.h"

#define SOCIAL_MIN_INTERVAL_SECONDS 3600
#define SOCIAL_OFFER_HOME_UPDATES 0x30
#define SOCIAL_INITIAL_SCRATCH_BYTES 192

/* Center advances one record and starts its score unless marked silent.
 * On the terminal record, center returns home. */
void SocialUpdate(void)
{
  const SocialFrame *record;

  if (InputPressed(BUTTON_CENTER) != 0) {
    record = g_ui.view.social.sequenceFrame;
    if (record->flags.bits.terminal) {
      HomeInit();
      SetView(VIEW_HOME);
    } else {
      record++;
      g_ui.view.social.sequenceFrame = record;
      if (record->scoreId != SOCIAL_SILENT) {
        BeepLoadScore(g_ui.view.social.sequenceFrame->scoreId);
      }
    }
  }
}

/* The upper panel can show the record's text, name or Watt amount. The lower
 * message always uses a blinking continuation cursor. */
void SocialRender(void)
{
  u8 rewardValue;

  ScratchReset();
  /* The first drawing helper resets scratch before allocating its raster. */
  ScratchAlloc(SOCIAL_INITIAL_SCRATCH_BYTES);

  if (g_ui.view.social.sequenceFrame->flags.bits.showPokemon) {
    RenderHeldPokemon(0x20, 0x04);
  }

  if (g_ui.view.social.sequenceFrame->flags.bits.bubbleIndex !=
      SOCIAL_NO_BUBBLE) {
    RenderFeeling(g_ui.view.social.sequenceFrame->flags.bits.bubbleIndex);
  }

  if (g_ui.view.social.sequenceFrame->flags.bits.showTreasure) {
    RenderTreasure(0x14, 0x14);
  }

  rewardValue = g_ui.view.social.rewardValue;
  switch (g_ui.view.social.sequenceFrame->upperContent) {
  case SOCIAL_POKEMON_NAME:
    RenderHeldName(0x00, 0x20, BORDER_TOP | BORDER_LEFT);
    break;
  case SOCIAL_ITEM_NAME:
    RenderCourseItem(0x00, 0x20, rewardValue,
                     BORDER_TOP | BORDER_LEFT | BORDER_RIGHT);
    break;
  case SOCIAL_WATTS:
    RenderWatts(0x02, 0x20, rewardValue, 0x0d);
    break;
  case SOCIAL_NO_MESSAGE:
    break;
  default:
    RenderMessage(0x20, g_ui.view.social.sequenceFrame->upperContent,
                  BORDER_TOP | BORDER_LEFT | BORDER_RIGHT, MESSAGE_NO_PROMPT);
    break;
  }

  if (g_ui.view.social.sequenceFrame->flags.bits.lowerMessageMode >
      SOCIAL_PANEL_FIXED) {
    RenderMessage(0x30,
                  (g_ui.view.social.sequenceFrame->lowerMessage +
                   g_ui.view.social.messageVariant),
                  BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_BLINK_PROMPT);
  } else if (g_ui.view.social.sequenceFrame->upperContent ==
             SOCIAL_NO_MESSAGE) {
    RenderMessage(0x30, g_ui.view.social.sequenceFrame->lowerMessage,
                  BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_BLINK_PROMPT);
  } else {
    RenderMessage(0x30, g_ui.view.social.sequenceFrame->lowerMessage,
                  BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_BLINK_PROMPT);
  }
  RenderBattery(0, 0);
}

/* Consume one pending offer check while home is interactive. The random gate
 * comes before eligibility checks. Without a companion, 300 hourly steps can
 * offer one; otherwise one hour must pass since the last accepted event.
 * Eligible rewards are tested in order: item, 50/20/10 watts, then boredom. */
void SocialOfferCheck(void)
{
  volatile u16 hourStepSnapshot;
  Course *course;
  Item *items;
  u8 friendship;

  if ((g_state.flags.byte & SYSTEM_MODE_MASK) != SYSTEM_MODE_INTERACTIVE) {
    return;
  }
  if (g_state.view != VIEW_HOME) {
    return;
  }
  if (g_state.flags.bits.socialOfferPending == 0) {
    return;
  }

  g_state.flags.bits.socialOfferPending = 0;
  g_ui.view.home.eventOfferCountdown = g_ui.view.home.pendingEventId = 0;
  hourStepSnapshot = g_state.hourSteps;
  if ((u8)(RandomNext() % 100ul) >= 40) {
    return;
  }

  if (g_state.flags.bits.hasPokemon == 0) {
    if (g_state.hourSteps < 300) {
      return;
    }
    g_ui.view.home.pendingEventId = SOCIAL_EVENT_NEW_POKEMON;
  } else {
    if (g_state.socialElapsedSeconds < SOCIAL_MIN_INTERVAL_SECONDS) {
      return;
    }

    ScratchReset();
    course = ScratchAlloc(sizeof(Course));
    EepromRead(EEPROM_COURSE, course, sizeof(Course));
    friendship = course->friendship;
    ScratchReset();
    items = ScratchAlloc(sizeof(Item) * 3);
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items), items,
               sizeof(Item) * 3);
    if ((ItemSlotFindEmpty(items) < 3) && (friendship >= 90) &&
        (hourStepSnapshot >= 500)) {
      g_ui.view.home.pendingEventId = SOCIAL_EVENT_ITEM;
    } else if ((friendship >= 80) && (hourStepSnapshot >= 250)) {
      g_ui.view.home.pendingEventId = SOCIAL_EVENT_WATTS_50;
    } else if (hourStepSnapshot >= 200) {
      g_ui.view.home.pendingEventId = SOCIAL_EVENT_WATTS_20;
    } else if (hourStepSnapshot >= 100) {
      g_ui.view.home.pendingEventId = SOCIAL_EVENT_WATTS_10;
    } else if ((g_state.save.pokemonMinutes >= 60) &&
               (hourStepSnapshot <= 50)) {
      g_ui.view.home.pendingEventId = SOCIAL_EVENT_BORED;
    } else {
      return;
    }
  }

  g_ui.view.home.eventOfferCountdown = SOCIAL_OFFER_HOME_UPDATES;
}

/* Center presses start record scores from the second record onward. Message
 * values are EEPROM text IDs; panel variants add 0..2 to their stored base. */
const SocialFrame g_socialItemSequence[3] = {
    {{SOCIAL_FLAGS(0, SOCIAL_PANEL_FIXED, SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_POKEMON_NAME,
     MESSAGE_SOCIAL_ITEM_OFFER},
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, SOCIAL_PANEL_FIXED, SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_NO_MESSAGE,
     MESSAGE_SOCIAL_REWARD_REVEAL},
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, SOCIAL_PANEL_FIXED,
                   SOCIAL_TERMINAL | SOCIAL_SHOW_POKEMON |
                       SOCIAL_SHOW_TREASURE)},
     SCORE_FOUND,
     SOCIAL_ITEM_NAME,
     MESSAGE_FOUND}};

const SocialFrame g_socialWatts50Sequence[3] = {
    {{SOCIAL_FLAGS(1, SOCIAL_PANEL_VARIANT, SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_POKEMON_NAME,
     MESSAGE_SOCIAL_WATTS_50_BASE},
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, SOCIAL_PANEL_FIXED, SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_NO_MESSAGE,
     MESSAGE_SOCIAL_REWARD_REVEAL},
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, SOCIAL_PANEL_FIXED,
                   SOCIAL_TERMINAL | SOCIAL_SHOW_POKEMON |
                       SOCIAL_SHOW_TREASURE)},
     SCORE_FOUND,
     SOCIAL_WATTS,
     MESSAGE_FOUND}};

const SocialFrame g_socialWatts20Sequence[3] = {
    {{SOCIAL_FLAGS(2, SOCIAL_PANEL_VARIANT, SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_POKEMON_NAME,
     MESSAGE_SOCIAL_WATTS_20_BASE},
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, SOCIAL_PANEL_FIXED, SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_NO_MESSAGE,
     MESSAGE_SOCIAL_REWARD_REVEAL},
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, SOCIAL_PANEL_FIXED,
                   SOCIAL_TERMINAL | SOCIAL_SHOW_POKEMON |
                       SOCIAL_SHOW_TREASURE)},
     SCORE_FOUND,
     SOCIAL_WATTS,
     MESSAGE_FOUND}};

const SocialFrame g_socialWatts10Sequence[3] = {
    {{SOCIAL_FLAGS(3, SOCIAL_PANEL_VARIANT, SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_POKEMON_NAME,
     MESSAGE_SOCIAL_WATTS_10_BASE},
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, SOCIAL_PANEL_FIXED, SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_NO_MESSAGE,
     MESSAGE_SOCIAL_REWARD_REVEAL},
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, SOCIAL_PANEL_FIXED,
                   SOCIAL_TERMINAL | SOCIAL_SHOW_POKEMON |
                       SOCIAL_SHOW_TREASURE)},
     SCORE_FOUND,
     SOCIAL_WATTS,
     MESSAGE_FOUND}};

const SocialFrame g_socialBoredSequence[1] = {
    {{SOCIAL_FLAGS(4, SOCIAL_PANEL_VARIANT,
                   SOCIAL_TERMINAL | SOCIAL_SHOW_POKEMON)},
     SOCIAL_SILENT,
     SOCIAL_POKEMON_NAME,
     MESSAGE_SOCIAL_BORED_BASE}};

const SocialFrame g_socialNewPokemonSequence[2] = {
    {{SOCIAL_FLAGS(SOCIAL_NO_BUBBLE, SOCIAL_PANEL_FIXED, 0)},
     SOCIAL_SILENT,
     SOCIAL_NO_MESSAGE,
     MESSAGE_SOCIAL_NEW_POKEMON_FIRST},
    {{SOCIAL_FLAGS(6, SOCIAL_PANEL_FIXED,
                   SOCIAL_TERMINAL | SOCIAL_SHOW_POKEMON)},
     SCORE_SUCCESS,
     SOCIAL_POKEMON_NAME,
     MESSAGE_SOCIAL_NEW_POKEMON_SECOND}};

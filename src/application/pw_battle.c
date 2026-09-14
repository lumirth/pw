#include "application/pw_fourier.h"
#include "types.h"
#include "raster_column.h"
#include "eeprom_address.h"
#include "project.h"
#include "application/pw_diary.h"
#include "application/pw_nt7508.h"
#include <machine.h>
#include "application/pw_battle.h"
#include "application/pw_buzzer.h"
#include "application/pw_carry_overflow.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_home.h"
#include "application/pw_player_input.h"
#include "support/lib_common.h"
#include "support/scratch.h"

extern const u8 g_battleResponseWeights[];
extern const u8 g_battleCaptureThresholds[];
extern const s8 g_battleEntranceX[];

typedef struct {
  s8 playerX;
  s8 opponentX;
} BattleParticipantPositions;

extern const BattleParticipantPositions g_playerTurnPositions[];
extern const BattleParticipantPositions g_opponentTurnPositions[];

void BattlePollAction(void);
u8 BattleCheckCapture(void);
void BattleStoreCapture(void);

#define BATTLE_FRAME_READ_BYTES POKEMON_ANIMATION_BYTES
#define PW_BATTLE_WATT_LOSS_CAP 10
#define PW_BATTLE_EVENT_SPRITE_BYTES 0x170

/* One response code has two interpretations, selected by the player's action:
 *   code   Attack              Evade
 *     0    ordinary hit        dodge and counterattack
 *     1    opponent dodges     staredown
 *     2    critical hit        opponent leaves
 * The previous exchange selects the probability row.
 */
#define BATTLE_OPENING 0
#define BATTLE_ENCOUNTER 1
#define BATTLE_CHOOSE_ACTION 2
#define BATTLE_PLAYER_ACTION 3
#define BATTLE_OPPONENT_ACTION 4
#define BATTLE_DEFEATED 5
#define BATTLE_WATTS_LOST 6
#define BATTLE_OPPONENT_LEAVING 7
#define BATTLE_STAREDOWN 8
#define BATTLE_CAPTURE_FAILED 9
#define BATTLE_THROW_BALL 10
#define BATTLE_BALL_CLOSE 0x0b
#define BATTLE_BALL_SETTLE 0x0c
#define BATTLE_CAPTURE_CHECK 0x0d
#define BATTLE_CAPTURE_PAUSE 0x0e
#define BATTLE_CAPTURE_STARS 0x0f
#define BATTLE_CAPTURE_CONFIRM 0x10
#define BATTLE_BALL_BREAK 0x11

#define BATTLE_RESPONSE_HIT_OR_COUNTER 0
#define BATTLE_RESPONSE_DODGE_OR_STARE 1
#define BATTLE_RESPONSE_CRITICAL_OR_LEAVE 2

#define BATTLE_ACTION_ATTACK 0
#define BATTLE_ACTION_EVADE 1

#define BATTLE_ROW_AFTER_HIT 1
#define BATTLE_ROW_AFTER_CRITICAL 2
#define BATTLE_ROW_AFTER_DODGED 3
#define BATTLE_ROW_AFTER_STAREDOWN 4
#define BATTLE_RESPONSE_COLUMNS 3u
#define BATTLE_ROLL_RANGE 100ul
#define BATTLE_FULL_HP 4
#define BATTLE_CAPTURE_PASSES 3

/* Keep the radar's encounter selection and start both participants at full HP.
 */
void BattleInit(void)
{
  u8 zero;
  u8 fullHp;

  zero = 0;
  g_ui.view.battle.phase = zero;
  fullHp = BATTLE_FULL_HP;
  g_ui.view.battle.playerHp = fullHp;
  g_ui.view.battle.opponentHp = fullHp;
  g_ui.view.battle.animationFrame = zero;
  g_ui.view.battle.frameLimit = 6;
  g_ui.view.battle.playerX = 0x38;
  g_ui.view.battle.opponentX = 0xe0;
  /* Clear the probability row and hide opponent HP. Action polling fills the
   * response code; Attack/Evade also set playerAction. */
  g_ui.view.battle.flags.byte &= 0x1e;
  g_ui.view.battle.capturePasses = zero;
  BeepLoadScore(SCORE_BATTLE_ENCOUNTER);
}

/* Each poll with no active score draws a response before checking buttons. */
void BattlePollAction(void)
{
  u8 roll;

  if (BeepHasScore() != 0) {
    return;
  }

  roll = ((RandomNext() >> 3) % BATTLE_ROLL_RANGE);

  if (roll < g_battleResponseWeights[g_ui.view.battle.flags.bits.responseRow *
                                         BATTLE_RESPONSE_COLUMNS +
                                     BATTLE_RESPONSE_CRITICAL_OR_LEAVE]) {
    g_ui.view.battle.flags.bits.responseCode =
        BATTLE_RESPONSE_CRITICAL_OR_LEAVE;
  } else if (roll <
             (int)(g_battleResponseWeights[g_ui.view.battle.flags.bits
                                                   .responseRow *
                                               BATTLE_RESPONSE_COLUMNS +
                                           BATTLE_RESPONSE_CRITICAL_OR_LEAVE] +
                   g_battleResponseWeights[g_ui.view.battle.flags.bits
                                                   .responseRow *
                                               BATTLE_RESPONSE_COLUMNS +
                                           BATTLE_RESPONSE_DODGE_OR_STARE])) {
    g_ui.view.battle.flags.bits.responseCode = BATTLE_RESPONSE_DODGE_OR_STARE;
  } else {
    g_ui.view.battle.flags.bits.responseCode = BATTLE_RESPONSE_HIT_OR_COUNTER;
  }

  if (InputPressed(BUTTON_LEFT) != 0) {
    g_ui.view.battle.flags.bits.playerAction = BATTLE_ACTION_ATTACK;
    switch (g_ui.view.battle.flags.bits.responseCode) {
    case BATTLE_RESPONSE_HIT_OR_COUNTER:
    case BATTLE_RESPONSE_CRITICAL_OR_LEAVE:
    case BATTLE_RESPONSE_DODGE_OR_STARE:
      g_ui.view.battle.phase = BATTLE_PLAYER_ACTION;
      g_ui.view.battle.animationFrame = 0;
      g_ui.view.battle.frameLimit = 8;
      return;
    default:
      break;
    }
  }

  if (InputPressed(BUTTON_RIGHT) != 0) {
    g_ui.view.battle.flags.bits.playerAction = BATTLE_ACTION_EVADE;
    switch (g_ui.view.battle.flags.bits.responseCode) {
    case BATTLE_RESPONSE_CRITICAL_OR_LEAVE:
      BeepLoadScore(SCORE_ESCAPE);
      g_ui.view.battle.phase = BATTLE_OPPONENT_LEAVING;
      g_ui.view.battle.animationFrame = 0;
      g_ui.view.battle.frameLimit = 10;
      return;
    case BATTLE_RESPONSE_DODGE_OR_STARE:
      BeepLoadScore(SCORE_CONFIRM);
      g_ui.view.battle.phase = BATTLE_STAREDOWN;
      g_ui.view.battle.animationFrame = 0;
      g_ui.view.battle.frameLimit = 6;
      return;
    case BATTLE_RESPONSE_HIT_OR_COUNTER:
      g_ui.view.battle.phase = BATTLE_OPPONENT_ACTION;
      g_ui.view.battle.animationFrame = 0;
      g_ui.view.battle.frameLimit = 8;
      return;
    }
  }

  if (InputPressed(BUTTON_CENTER) != 0) {
    BeepLoadScore(SCORE_THROW);
    g_ui.view.battle.phase = BATTLE_THROW_BALL;
    g_ui.view.battle.animationFrame = 0;
    g_ui.view.battle.frameLimit = 6;
  }
}

/* Store the capture. A full course inventory passes its encounter selection
 * to the discard view through the shared UI byte. */
void BattleStoreCapture(void)
{
  u8 encounterIndex;
  Pokemon *pokemon;
  u8 freeSlot;
  u8 *course;
  DeviceStatus *status;
  u8 receiptIndex;
  u8 eventPresence;
  u8 *transferBuffer;
  u16 imageBytes;

  encounterIndex = (g_ui.view.battle.encounter - 1);
  if (encounterIndex > 3) {
    return;
  }

  if (encounterIndex < 3) {
    ScratchReset();
    pokemon = ScratchAlloc((sizeof(Pokemon) * 3));
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon),
               pokemon, (sizeof(Pokemon) * 3));
    freeSlot = PokemonSlotFindEmpty(pokemon);
    if (freeSlot >= 3) {
      g_ui.view.discard.inventoryKind = PW_DISCARD_INVENTORY_POKEMON;
      DiscardInit();
      SetView(VIEW_DISCARD);
      return;
    }
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course,
                                        encounters[encounterIndex]),
               &pokemon[freeSlot], sizeof(Pokemon));
    EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon),
                pokemon, (sizeof(Pokemon) * 3));
    course = ScratchAlloc(sizeof(Course));
    EepromRead(EEPROM_COURSE, course, sizeof(Course));
    DiaryAppend((Course *)course, ScratchAlloc(sizeof(DiaryEntry)),
                PW_DIARY_ACTION_CAPTURED_ROUTE_POKEMON, 0,
                g_ui.view.battle.encounter, 0);
    return;
  }

  ScratchReset();
  status = ScratchAlloc(0x68);
  receiptIndex = EepromReadByte(PW_EEPROM_MEMBER_ADDRESS(
      EEPROM_BONUS_COURSE, BonusResources, values.pokemonReceipt));
  if (StatusLoadReceived(status, receiptIndex) != 0) {
    return;
  }
  eventPresence = EepromReadByte(EEPROM_EVENTS);
  if ((eventPresence & EVENT_PRESENT_POKEMON) != 0) {
    return;
  }
  /* Copy the first 368 bytes of the 384-byte bonus animation and copy its
   * name separately. */
  imageBytes = PW_BATTLE_EVENT_SPRITE_BYTES;
  transferBuffer = ScratchAlloc(imageBytes);
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_BONUS_COURSE, BonusResources,
                                      values.pokemon),
             transferBuffer, sizeof(Pokemon));
  EepromWrite(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_POKEMON, EventPokemon, pokemon),
      transferBuffer, sizeof(Pokemon));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_BONUS_COURSE, BonusResources,
                                      values.metadata),
             transferBuffer, sizeof(PokemonMetadata));
  EepromWrite(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_POKEMON, EventPokemon, metadata),
      transferBuffer, sizeof(PokemonMetadata));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_BONUS_COURSE, BonusResources,
                                      pokemonImage),
             transferBuffer, imageBytes);
  EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_POKEMON, EventPokemon,
                                       pokemonImage),
              transferBuffer, imageBytes);
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_BONUS_COURSE, BonusResources,
                                      pokemonName),
             transferBuffer, 0x140);
  EepromWrite(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_EVENT_POKEMON, EventPokemon, pokemonName),
      transferBuffer, 0x140);
  EepromWriteByte(EEPROM_EVENTS, (eventPresence | EVENT_PRESENT_POKEMON));
  StatusSetReceived(status, receiptIndex);
  course = ScratchAlloc(sizeof(Course));
  EepromRead(EEPROM_COURSE, course, sizeof(Course));
  DiaryAppend((Course *)course, ScratchAlloc(sizeof(DiaryEntry)),
              PW_DIARY_ACTION_CAPTURED_EVENT_POKEMON, 1,
              g_ui.view.battle.encounter, 0);
}

/* Test this shake against the threshold for the opponent's remaining HP.
 * Capture requires three passes; zero HP fails every check. */
u8 BattleCheckCapture(void)
{
  u8 roll;
  u8 opponentHp;

  roll = ((RandomNext() >> 3) % BATTLE_ROLL_RANGE);
  opponentHp = g_ui.view.battle.opponentHp;
  if (opponentHp != 0) {
    if (roll < g_battleCaptureThresholds[(u16)opponentHp - 1]) {
      return 1;
    }
  }
  return 0;
}

/* Advance phases after their rendered animation reaches its capped frame. */
void BattleUpdate(void)
{
  u8 *course;

  if (g_ui.view.battle.phase == BATTLE_CHOOSE_ACTION) {
    BattlePollAction();
    return;
  }

  switch (g_ui.view.battle.phase) {
  case BATTLE_OPENING:
    if (g_ui.view.battle.animationFrame < g_ui.view.battle.frameLimit) {
      return;
    }
    g_ui.view.battle.phase = BATTLE_ENCOUNTER;
    g_ui.view.battle.animationFrame = 0;
    g_ui.view.battle.frameLimit = 3;
    break;
  case BATTLE_ENCOUNTER:
    g_ui.view.battle.opponentX =
        g_battleEntranceX[g_ui.view.battle.animationFrame];
    if (g_ui.view.battle.animationFrame < g_ui.view.battle.frameLimit) {
      return;
    }
    g_ui.view.battle.flags.bits.opponentHpVisible = 1;
    if (BeepHasScore() != 0) {
      return;
    }
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    g_ui.view.battle.phase = BATTLE_CHOOSE_ACTION;
    break;
  case BATTLE_PLAYER_ACTION:
    g_ui.view.battle.playerX =
        g_playerTurnPositions[g_ui.view.battle.animationFrame].playerX;
    g_ui.view.battle.opponentX =
        g_playerTurnPositions[g_ui.view.battle.animationFrame].opponentX;
    if (BeepHasScore() != 0) {
      return;
    }
    if (g_ui.view.battle.animationFrame < g_ui.view.battle.frameLimit) {
      return;
    }
    g_ui.view.battle.playerX = 0x38;
    g_ui.view.battle.opponentX = 8;
    switch (g_ui.view.battle.flags.bits.responseCode) {
    case BATTLE_RESPONSE_HIT_OR_COUNTER:
      if (g_ui.view.battle.opponentHp <= 1) {
        goto enemyDefeated;
      }
      g_ui.view.battle.opponentHp--;
      switch (g_ui.view.battle.flags.bits.playerAction) {
      case BATTLE_ACTION_ATTACK:
        break;
      case BATTLE_ACTION_EVADE:
        g_ui.view.battle.phase = BATTLE_CHOOSE_ACTION;
        g_ui.view.battle.flags.bits.responseRow = BATTLE_ROW_AFTER_HIT;
        return;
      default:
        return;
      }
      break;
    case BATTLE_RESPONSE_DODGE_OR_STARE:
      break;
    case BATTLE_RESPONSE_CRITICAL_OR_LEAVE:
      if (g_ui.view.battle.opponentHp <= 2) {
        goto enemyDefeated;
      }
      g_ui.view.battle.opponentHp -= 2;
      break;
    default:
      return;
    }
    g_ui.view.battle.phase = BATTLE_OPPONENT_ACTION;
    g_ui.view.battle.animationFrame = 0;
    g_ui.view.battle.frameLimit = 8;
    break;
  enemyDefeated:
    g_ui.view.battle.opponentHp = 0;
    BeepLoadScore(SCORE_ESCAPE);
    g_ui.view.battle.phase = BATTLE_OPPONENT_LEAVING;
    g_ui.view.battle.animationFrame = 0;
    g_ui.view.battle.frameLimit = 10;
    return;
  case BATTLE_OPPONENT_ACTION:
    g_ui.view.battle.playerX =
        g_opponentTurnPositions[g_ui.view.battle.animationFrame].playerX;
    g_ui.view.battle.opponentX =
        g_opponentTurnPositions[g_ui.view.battle.animationFrame].opponentX;
    if (BeepHasScore() != 0) {
      return;
    }
    if (g_ui.view.battle.animationFrame < g_ui.view.battle.frameLimit) {
      return;
    }
    g_ui.view.battle.playerX = 0x38;
    g_ui.view.battle.opponentX = 8;
    switch (g_ui.view.battle.flags.bits.playerAction) {
    case BATTLE_ACTION_ATTACK:
      /* Attacking permits the opponent's reply even after a critical hit. */
      g_ui.view.battle.playerHp--;
      if (g_ui.view.battle.playerHp != 0) {
        switch (g_ui.view.battle.flags.bits.responseCode) {
        case BATTLE_RESPONSE_HIT_OR_COUNTER:
          g_ui.view.battle.flags.bits.responseRow = BATTLE_ROW_AFTER_HIT;
          break;
        case BATTLE_RESPONSE_DODGE_OR_STARE:
          g_ui.view.battle.flags.bits.responseRow = BATTLE_ROW_AFTER_DODGED;
          break;
        case BATTLE_RESPONSE_CRITICAL_OR_LEAVE:
          g_ui.view.battle.flags.bits.responseRow = BATTLE_ROW_AFTER_CRITICAL;
          break;
        default:
          break;
        }
        g_ui.view.battle.phase = BATTLE_CHOOSE_ACTION;
        break;
      }
      g_ui.view.battle.phase = BATTLE_DEFEATED;
      g_ui.view.battle.animationFrame = 0;
      g_ui.view.battle.frameLimit = 6;
      return;
    case BATTLE_ACTION_EVADE:
      /* The evade avoids damage and leads into the player's counterattack. */
      g_ui.view.battle.phase = BATTLE_PLAYER_ACTION;
      g_ui.view.battle.animationFrame = 0;
      g_ui.view.battle.frameLimit = 8;
      return;
    default:
      return;
    }
    break;
  case BATTLE_DEFEATED:
    if (g_ui.view.battle.animationFrame < g_ui.view.battle.frameLimit) {
      return;
    }
    ScratchReset();
    course = ScratchAlloc(sizeof(Course));
    EepromRead(EEPROM_COURSE, course, sizeof(Course));
    if (g_ui.view.battle.encounter < 4) {
      DiaryAppend((Course *)course, ScratchAlloc(sizeof(DiaryEntry)),
                  PW_DIARY_ACTION_BATTLE_DEFEATED, 0,
                  g_ui.view.battle.encounter, 0);
    } else {
      DiaryAppend((Course *)course, ScratchAlloc(sizeof(DiaryEntry)),
                  PW_DIARY_ACTION_BATTLE_DEFEATED, 1,
                  g_ui.view.battle.encounter, 0);
    }
    if (g_state.save.watts < PW_BATTLE_WATT_LOSS_CAP) {
      g_ui.view.battle.wattsLost = g_state.save.watts;
    } else {
      g_ui.view.battle.wattsLost = PW_BATTLE_WATT_LOSS_CAP;
    }
    g_state.save.watts = (g_state.save.watts - g_ui.view.battle.wattsLost);
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
    g_ui.view.battle.phase = BATTLE_WATTS_LOST;
    g_ui.view.battle.animationFrame = 0;
    g_ui.view.battle.frameLimit = 8;
    break;
  case BATTLE_WATTS_LOST:
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    HomeInit();
    SetView(VIEW_HOME);
    break;
  case BATTLE_STAREDOWN:
    if (g_ui.view.battle.animationFrame < g_ui.view.battle.frameLimit) {
      return;
    }
    g_ui.view.battle.phase = BATTLE_CHOOSE_ACTION;
    g_ui.view.battle.flags.bits.responseRow = BATTLE_ROW_AFTER_STAREDOWN;
    break;
  case BATTLE_OPPONENT_LEAVING:
    /* Acknowledgment may end this phase before the exit animation finishes. */
    if (g_ui.view.battle.animationFrame <= 3) {
      g_ui.view.battle.opponentX =
          g_battleEntranceX[3 - g_ui.view.battle.animationFrame];
    } else {
      g_ui.view.battle.opponentX = 0xe0;
    }
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    ScratchReset();
    course = ScratchAlloc(sizeof(Course));
    EepromRead(EEPROM_COURSE, course, sizeof(Course));
    if (g_ui.view.battle.encounter < 4) {
      DiaryAppend((Course *)course, ScratchAlloc(sizeof(DiaryEntry)),
                  PW_DIARY_ACTION_BATTLE_FLED, 0, g_ui.view.battle.encounter,
                  0);
    } else {
      DiaryAppend((Course *)course, ScratchAlloc(sizeof(DiaryEntry)),
                  PW_DIARY_ACTION_BATTLE_FLED, 1, g_ui.view.battle.encounter,
                  0);
    }
    BeepLoadScore(SCORE_FAILURE);
    HomeInit();
    SetView(VIEW_HOME);
    break;
  case BATTLE_THROW_BALL:
    if (g_ui.view.battle.animationFrame < g_ui.view.battle.frameLimit) {
      return;
    }
    g_ui.view.battle.flags.bits.opponentHpVisible = 0;
    g_ui.view.battle.phase = BATTLE_BALL_CLOSE;
    g_ui.view.battle.animationFrame = 0;
    g_ui.view.battle.frameLimit = 2;
    break;
  case BATTLE_BALL_CLOSE:
    if (g_ui.view.battle.animationFrame < g_ui.view.battle.frameLimit) {
      return;
    }
    g_ui.view.battle.phase = BATTLE_BALL_SETTLE;
    g_ui.view.battle.animationFrame = 0;
    g_ui.view.battle.frameLimit = 3;
    break;
  case BATTLE_BALL_SETTLE:
    if (g_ui.view.battle.animationFrame < g_ui.view.battle.frameLimit) {
      return;
    }
    g_ui.view.battle.phase = BATTLE_CAPTURE_CHECK;
    g_ui.view.battle.animationFrame = 0;
    g_ui.view.battle.frameLimit = 4;
    break;
  case BATTLE_CAPTURE_CHECK:
    if (g_ui.view.battle.animationFrame < g_ui.view.battle.frameLimit) {
      return;
    }
    if (BattleCheckCapture() != 0) {
      g_ui.view.battle.capturePasses++;
      g_ui.view.battle.phase = BATTLE_CAPTURE_PAUSE;
      g_ui.view.battle.animationFrame = 0;
      g_ui.view.battle.frameLimit = 4;
    } else {
      g_ui.view.battle.phase = BATTLE_BALL_BREAK;
      g_ui.view.battle.animationFrame = 0;
      g_ui.view.battle.frameLimit = 1;
      return;
    }
    break;
  case BATTLE_CAPTURE_PAUSE:
    if (g_ui.view.battle.animationFrame < g_ui.view.battle.frameLimit) {
      return;
    }
    if (g_ui.view.battle.capturePasses < BATTLE_CAPTURE_PASSES) {
      g_ui.view.battle.phase = BATTLE_CAPTURE_CHECK;
      g_ui.view.battle.animationFrame = 0;
      g_ui.view.battle.frameLimit = 4;
    } else {
      g_ui.view.battle.phase = BATTLE_CAPTURE_STARS;
      g_ui.view.battle.animationFrame = 0;
      g_ui.view.battle.frameLimit = 6;
      BeepLoadScore(SCORE_CONFIRM);
    }
    break;
  case BATTLE_CAPTURE_STARS:
    if (g_ui.view.battle.animationFrame < g_ui.view.battle.frameLimit) {
      return;
    }
    g_ui.view.battle.phase = BATTLE_CAPTURE_CONFIRM;
    g_ui.view.battle.animationFrame = 0;
    g_ui.view.battle.frameLimit = 6;
    BeepLoadScore(SCORE_SUCCESS);
    break;
  case BATTLE_CAPTURE_CONFIRM:
    if (g_ui.view.battle.animationFrame < g_ui.view.battle.frameLimit) {
      return;
    }
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    BattleStoreCapture();
    if (g_state.view != VIEW_BATTLE) {
      return;
    }
    HomeInit();
    SetView(VIEW_HOME);
    break;
  case BATTLE_BALL_BREAK:
    if (g_ui.view.battle.animationFrame < g_ui.view.battle.frameLimit) {
      return;
    }
    g_ui.view.battle.phase = BATTLE_CAPTURE_FAILED;
    g_ui.view.battle.animationFrame = 0;
    g_ui.view.battle.frameLimit = 6;
    break;
  case BATTLE_CAPTURE_FAILED:
    if (g_ui.view.battle.animationFrame >= g_ui.view.battle.frameLimit) {
      BeepLoadScore(SCORE_ESCAPE);
      g_ui.view.battle.phase = BATTLE_OPPONENT_LEAVING;
      g_ui.view.battle.animationFrame = 0;
      g_ui.view.battle.frameLimit = 10;
    }
    break;
  default:
    break;
  }
}

/* Advance the animation counter once per render and hold it at frameLimit
 * until the phase can finish. */
void BattleRender(void)
{
  const UiResources *image;
  const u8 *resourceAddress;
  u8 *raster;
  const u8 *ball;
  u8 *ballStorage;
  u8 *mask;
  u8 hpSegment;
  u8 responseCode;
  u8 playerAction;
  u8 sineIndex;

  image = (const UiResources *)EEPROM_UI;
  ScratchReset();
  raster = ScratchAlloc(0x300);
  ballStorage = ScratchAlloc(0x18);
  ball = ballStorage;
  mask = ScratchAlloc(8);
  EepromRead(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                               pokemonImage[(g_state.uiFrame & 1) * 0xc0]),
      raster, 0xc0);
  DisplayBlit(g_ui.view.battle.playerX, 0x08, 0x20, 0x18, raster);
  resourceAddress = image->hpSegment;
  EepromRead((u16)resourceAddress, raster,
             sizeof(((UiResources *)0)->hpSegment));
  hpSegment = 0;
  while (hpSegment < g_ui.view.battle.playerHp) {
    DisplayBlit(hpSegment * 8 + 0x38, 0, 0x08, 0x08, raster);
    hpSegment++;
  }
  if (g_ui.view.battle.flags.bits.opponentHpVisible) {
    hpSegment = 0;
    while (hpSegment < g_ui.view.battle.opponentHp) {
      DisplayBlit(hpSegment * 8 + 8, 0x18, 0x08, 0x08, raster);
      hpSegment++;
    }
  }

  if (g_ui.view.battle.phase <= BATTLE_THROW_BALL) {

    u8 fixedFacing;
    u16 imageAddress;

    if (g_ui.view.battle.encounter < 4) {
      EepromRead(
          PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, Course,
                                   encounters[g_ui.view.battle.encounter - 1]),
          raster, sizeof(Pokemon));
      fixedFacing = ((Pokemon *)raster)->fixedFacing;
      imageAddress = PW_EEPROM_MEMBER_ADDRESS(
          EEPROM_COURSE, CourseResources,
          encounterImages[(g_ui.view.battle.encounter - 1) *
                              POKEMON_ANIMATION_BYTES +
                          (g_state.uiFrame & 1) * POKEMON_FRAME_BYTES]);
    } else {
      EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_BONUS_COURSE, BonusResources,
                                          values.pokemon),
                 raster, sizeof(Pokemon));
      fixedFacing = ((Pokemon *)raster)->fixedFacing;
      imageAddress = PW_EEPROM_MEMBER_ADDRESS(
          EEPROM_BONUS_COURSE, BonusResources,
          pokemonImage[(g_state.uiFrame & 1) * POKEMON_FRAME_BYTES]);
    }
    /* Read 384 bytes from the selected frame; draw the leading 192 bytes. */
    EepromRead(imageAddress, raster, BATTLE_FRAME_READ_BYTES);
    if (fixedFacing == 0) {
      RasterMirror(0x20, 0x18, raster);
    }
    if (g_ui.view.battle.phase != BATTLE_THROW_BALL) {
      DisplayWriteSpan(g_ui.view.battle.opponentX, 0, 0x20, 0x18,
                       (RasterColumn *)raster);
    } else {
      int verticalOffset;
      u8 ballX;
      u8 ballY;

      sineIndex = (g_ui.view.battle.animationFrame << 2);
      ballX = (0x2c - sineIndex);
      /* Scale the signed Q11 coordinate as a word before narrowing it to a
       * pixel coordinate. */
      verticalOffset = g_sineQ11[sineIndex] >> 7;
      ballY = (0x14 - verticalOffset);
      EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, ball),
                 ballStorage, sizeof(((UiResources *)0)->ball));
      EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, ballMask),
                 mask, sizeof(((UiResources *)0)->ballMask));
      DisplayBlit(ballX, ballY, 0x08, 0x08, ballStorage);
      RasterMasked(raster, 0x20, 0x18, ball, mask,
                   (ballX - g_ui.view.battle.opponentX), ballY, 8, 8);
      DisplayBlit(g_ui.view.battle.opponentX, 0, 0x20, 0x18, raster);
    }
  }

  switch (g_ui.view.battle.phase) {
  case BATTLE_OPENING: {
    u8 frame = g_ui.view.battle.animationFrame;
    u8 height;

    if (frame < 3) {
      DisplayFillRect(0, 0, 0x60, height = (3 - frame) * 8, 3);
      DisplayFillRect(0, (0x40 - height), 0x60, height, 3);
    }
  } break;
  case BATTLE_ENCOUNTER:
    if (g_ui.view.battle.animationFrame >= g_ui.view.battle.frameLimit) {
      if (BeepHasScore() == 0) {
        if (g_ui.view.battle.encounter < 4) {
          RenderEnemyName(0x00, 0x20, (g_ui.view.battle.encounter - 1),
                          BORDER_TOP | BORDER_LEFT);
        } else {
          RenderBonusName(0x00, 0x20, BORDER_TOP | BORDER_LEFT);
        }
        RenderMessage(0x30, MESSAGE_BATTLE_ENCOUNTER,
                      BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                      MESSAGE_BLINK_PROMPT);
      }
    }
    break;
  case BATTLE_CHOOSE_ACTION:
    resourceAddress = image->battleMenu;
    EepromRead((u16)resourceAddress, raster,
               sizeof(((UiResources *)0)->battleMenu));
    DisplayBlit(0x00, 0x20, 0x60, 0x20, raster);
    break;
  case BATTLE_PLAYER_ACTION:
    responseCode = g_ui.view.battle.flags.bits.responseCode;
    switch (responseCode) {
    case BATTLE_RESPONSE_HIT_OR_COUNTER:
      if (g_ui.view.battle.animationFrame == 4) {
        BeepLoadScore(SCORE_HIT);
        EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, hitEffect),
                   raster, sizeof(((UiResources *)0)->hitEffect));
        DisplayBlit(0x28, 0x00, 0x10, 0x20, raster);
      }
      if (g_ui.view.battle.animationFrame >= 4) {
        RenderHeldName(0x00, 0x20, BORDER_TOP | BORDER_LEFT);
        RenderMessage(0x30, MESSAGE_BATTLE_ATTACK,
                      BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                      MESSAGE_NO_PROMPT);
      }
      break;
    case BATTLE_RESPONSE_DODGE_OR_STARE:
      if (g_ui.view.battle.animationFrame == 4) {
        BeepLoadScore(SCORE_DODGE);
      }
      if (g_ui.view.battle.animationFrame >= 4) {
        if (g_ui.view.battle.encounter < 4) {
          RenderEnemyName(0x00, 0x20, (g_ui.view.battle.encounter - 1),
                          BORDER_TOP | BORDER_LEFT);
        } else {
          RenderBonusName(0x00, 0x20, BORDER_TOP | BORDER_LEFT);
        }
        RenderMessage(0x30, MESSAGE_BATTLE_DODGE,
                      BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                      MESSAGE_NO_PROMPT);
      }
      break;
    case BATTLE_RESPONSE_CRITICAL_OR_LEAVE:
      if (g_ui.view.battle.animationFrame == 4) {
        BeepLoadScore(SCORE_CRITICAL);
        EepromRead(
            PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, criticalHitEffect),
            raster, sizeof(((UiResources *)0)->criticalHitEffect));
        DisplayBlit(0x28, 0x00, 0x10, 0x20, raster);
      }
      if (g_ui.view.battle.animationFrame >= 4) {
        RenderMessage(0x20, MESSAGE_BATTLE_CRITICAL,
                      BORDER_TOP | BORDER_LEFT | BORDER_RIGHT,
                      MESSAGE_NO_PROMPT);
        RenderMessage(0x30, MESSAGE_BATTLE_HIT,
                      BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                      MESSAGE_NO_PROMPT);
      }
      break;
    }
    break;
  case BATTLE_OPPONENT_ACTION:
    playerAction = g_ui.view.battle.flags.bits.playerAction;
    switch (playerAction) {
    case BATTLE_ACTION_ATTACK:
      if (g_ui.view.battle.animationFrame == 4) {
        BeepLoadScore(SCORE_HIT);
        EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_UI, UiResources, hitEffect),
                   raster, sizeof(((UiResources *)0)->hitEffect));
        DisplayBlit(0x28, 0x00, 0x10, 0x20, raster);
      }
      if (g_ui.view.battle.animationFrame < 4) {
        break;
      }
      if (g_ui.view.battle.encounter < 4) {
        RenderEnemyName(0x00, 0x20, (g_ui.view.battle.encounter - 1),
                        BORDER_TOP | BORDER_LEFT);
      } else {
        RenderBonusName(0x00, 0x20, BORDER_TOP | BORDER_LEFT);
      }
      RenderMessage(0x30, MESSAGE_BATTLE_ATTACK,
                    BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                    MESSAGE_NO_PROMPT);
      break;
    case BATTLE_ACTION_EVADE:
      if (g_ui.view.battle.animationFrame == 4) {
        BeepLoadScore(SCORE_DODGE);
      }
      if (g_ui.view.battle.animationFrame < 4) {
        break;
      }
      RenderHeldName(0x00, 0x20, BORDER_TOP | BORDER_LEFT);
      RenderMessage(0x30, MESSAGE_BATTLE_DODGE,
                    BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                    MESSAGE_NO_PROMPT);
      break;
    }
    break;
  case BATTLE_DEFEATED:
    if (g_ui.view.battle.encounter < 4) {
      RenderEnemyName(0x00, 0x20, (g_ui.view.battle.encounter - 1),
                      BORDER_TOP | BORDER_LEFT);
    } else {
      RenderBonusName(0x00, 0x20, BORDER_TOP | BORDER_LEFT);
    }
    RenderMessage(0x30, MESSAGE_BATTLE_DEFEAT,
                  BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_NO_PROMPT);
    break;
  case BATTLE_WATTS_LOST:
    RenderWatts(0x02, 0x20, g_ui.view.battle.wattsLost, 0x0d);
    RenderMessage(0x30, MESSAGE_BATTLE_DROPPED,
                  BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_BLINK_PROMPT);
    break;
  case BATTLE_STAREDOWN:
    RenderMessage(0x30, MESSAGE_BATTLE_STARE,
                  BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_NO_PROMPT);
    break;
  case BATTLE_OPPONENT_LEAVING:
    if (g_ui.view.battle.animationFrame > 3) {
      if (g_ui.view.battle.encounter < 4) {
        RenderEnemyName(0x00, 0x20, (g_ui.view.battle.encounter - 1),
                        BORDER_TOP | BORDER_LEFT);
      } else {
        RenderBonusName(0x00, 0x20, BORDER_TOP | BORDER_LEFT);
      }
      RenderMessage(0x30, MESSAGE_BATTLE_ESCAPE,
                    BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                    MESSAGE_BLINK_PROMPT);
    }
    break;
  case BATTLE_THROW_BALL:
    RenderMessage(0x30, MESSAGE_BATTLE_THROW,
                  BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_NO_PROMPT);
    break;
  case BATTLE_BALL_CLOSE:
    resourceAddress = image->cloud;
    EepromRead((u16)resourceAddress, raster, sizeof(((UiResources *)0)->cloud));
    DisplayBlit(8, 0, 0x20, 0x18, raster);
    break;
  case BATTLE_BALL_SETTLE:
    resourceAddress = image->ball;
    EepromRead((u16)resourceAddress, raster, sizeof(((UiResources *)0)->ball));
    DisplayBlit(0x14, 0x0c, 0x08, 0x08, raster);
    break;
  case BATTLE_CAPTURE_CHECK: {
    u8 x;

    switch (g_state.uiFrame & 3) {
    case 0:
      x = 0x13;
      break;
    case 1:
      x = 0x14;
      break;
    case 2:
      x = 0x15;
      break;
    case 3:
      x = 0x14;
      break;
    }
    resourceAddress = image->ball;
    EepromRead((u16)resourceAddress, raster, sizeof(((UiResources *)0)->ball));
    DisplayBlit(x, 0x0c, 0x08, 0x08, raster);
    break;
  }
  case BATTLE_CAPTURE_PAUSE:
    resourceAddress = image->ball;
    EepromRead((u16)resourceAddress, raster, sizeof(((UiResources *)0)->ball));
    DisplayBlit(0x14, 0x0c, 0x08, 0x08, raster);
    break;
  case BATTLE_CAPTURE_STARS:
    resourceAddress = image->ball;
    EepromRead((u16)resourceAddress, raster, sizeof(((UiResources *)0)->ball));
    DisplayBlit(0x14, 0x0c, 0x08, 0x08, raster);
    resourceAddress = image->star;
    EepromRead((u16)resourceAddress, raster, sizeof(((UiResources *)0)->star));
    {
      s8 x, y;

      y = 0x0a - g_ui.view.battle.animationFrame * 2;
      x = 0x0c - g_ui.view.battle.animationFrame;
      DisplayBlit(x, y, 8, 8, raster);
      y = 0x0c - g_ui.view.battle.animationFrame * 2;
      x = g_ui.view.battle.animationFrame + 0x1c;
      DisplayBlit(x, y, 8, 8, raster);
    }
    break;
  case BATTLE_CAPTURE_CONFIRM:
    resourceAddress = image->ball;
    EepromRead((u16)resourceAddress, raster, sizeof(((UiResources *)0)->ball));
    DisplayBlit(0x14, 0x0c, 0x08, 0x08, raster);
    if (g_ui.view.battle.encounter < 4) {
      RenderEnemyName(0x00, 0x20, (g_ui.view.battle.encounter - 1),
                      BORDER_TOP | BORDER_LEFT);
    } else {
      RenderBonusName(0x00, 0x20, BORDER_TOP | BORDER_LEFT);
    }
    RenderMessage(0x30, MESSAGE_BATTLE_CAPTURED,
                  BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_BLINK_PROMPT);
    break;
  case BATTLE_BALL_BREAK:
    resourceAddress = image->cloud;
    EepromRead((u16)resourceAddress, raster, sizeof(((UiResources *)0)->cloud));
    DisplayBlit(8, 0, 0x20, 0x18, raster);
    break;
  case BATTLE_CAPTURE_FAILED:
    RenderMessage(0x30, MESSAGE_BATTLE_ALMOST,
                  BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_NO_PROMPT);
    break;
  default:
    break;
  }

  RenderBattery(0, 0);
  g_ui.view.battle.animationFrame++;

  if (g_ui.view.battle.animationFrame >= g_ui.view.battle.frameLimit) {
    g_ui.view.battle.animationFrame = g_ui.view.battle.frameLimit;
  }
}

/* Opponent entrance positions; departure reads the same path backwards. */
const s8 g_battleEntranceX[4] = {-16, -4, 8, 8};

const BattleParticipantPositions g_playerTurnPositions[9] = {
    {56, 8}, {56, 8}, {54, 8}, {52, 8}, {53, 0},
    {54, 0}, {55, 4}, {56, 8}, {56, 8}};

const BattleParticipantPositions g_opponentTurnPositions[9] = {
    {56, 8},  {56, 8},  {56, 10}, {56, 12}, {64, 12},
    {64, 11}, {64, 10}, {60, 9},  {56, 8}};

/* Columns use response codes 0, 1, 2 from the table above. The cumulative
 * probability test visits codes 2, 1, 0. */
const u8 g_battleResponseWeights[15] = {
    45, 35, 20, /* Opening. */
    40, 30, 30, /* After hit/counter. */
    50, 40, 10, /* After critical. */
    60, 30, 10, /* After dodged. */
    20, 30, 50  /* After staredown. */
};

/* Per-check threshold indexed by remaining opponent HP minus one. */
const u8 g_battleCaptureThresholds[5] = {0x61, 0x4f, 0x42, 0x38, 0x00};

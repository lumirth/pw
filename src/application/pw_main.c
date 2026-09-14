#include "application/pw_builtin.h"
#include "flags.h"
#include "types.h"
#include "eeprom_address.h"
#include "startup/hardware.h"
#include "project.h"
#include "application/pw_nt7508.h"
#include <machine.h>
#include "application/pw_accel_bma150.h"
#include "application/pw_battery.h"
#include "application/pw_battle.h"
#include "application/pw_buzzer.h"
#include "application/pw_carry_overflow.h"
#include "application/pw_cutscenes.h"
#include "application/pw_dowsing.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_follower_prompts.h"
#include "support/ir.h"
#include "application/pw_friend.h"
#include "application/pw_home.h"
#include "application/pw_local_settings.h"
#include "application/pw_main.h"
#include "application/pw_pack.h"
#include "application/pw_pedometer.h"
#include "application/pw_pictogram_menu.h"
#include "application/pw_player_input.h"
#include "support/lib_common.h"
#include "application/pw_pokeradar.h"
#include "application/pw_power.h"
#include "application/pw_rtc.h"
#include "support/scratch.h"
#include "application/pw_selftest.h"
#include "application/pw_trainer.h"

void RenderUnregisteredHome(void);
void RenderUnregisteredResult(void);
void RenderIrResult(void);
void RenderStepCount(void);

void CaptureSample(void);

void ViewUpdate(void);
void ViewRender(void);

void BeepTick(void);

#define PW_ACCEL_RING_MASK 0x3f
#define PW_ACCEL_RING_LAST 0x3f

/* Blink the unregistered walker's up-arrow every four UI frames. */
void RenderUnregisteredHome(void)
{
  u8 *raster;
  const u8 *shell;
  const u8 *overlay;
  u8 *cell;
  uint i;

  ScratchReset();
  raster = ScratchAlloc(0x100);
  shell = g_residentResources.assets.walkerImage;
  i = 0;
  do {
    raster[i] = shell[i];
    i++;
  } while (i < 0x100);
  overlay = g_residentResources.assets.neutralFace;
  cell = raster + 0x50;
  i = 0;
  do {
    cell[i] = (cell[i] | (overlay[i] * 8));
    cell[i + 0x40] = (cell[i + 0x40] | (overlay[i] / 0x20));
    i++;
  } while (i < 0x20);
  if (((g_state.uiFrame >> 2) & 1) != 0) {
    overlay = g_residentResources.assets.buttonArrow;
    cell = raster + 0xd8;
    i = 0;
    do {
      cell[i] = (cell[i] | (overlay[i] * 0x10));
      i++;
    } while (i < 0x10);
    DisplayBlit(0x20, 0x10, 0x20, 0x20, raster);
    i = 0;
    do {
      raster[i] = (overlay[i] / 0x10);
      i++;
    } while (i < 0x10);
    DisplayBlit(0x2c, 0x30, 8, 8, raster);
  } else {
    DisplayBlit(0x20, 0x10, 0x20, 0x20, raster);
  }
}

/* Display a frown and return home on the ninth render. */
void RenderUnregisteredResult(void)
{
  u8 *raster;
  const u8 *shell;
  const u8 *overlay;
  u8 *cell;
  uint i;

  ScratchReset();
  raster = ScratchAlloc(0x100);
  shell = g_residentResources.assets.walkerImage;
  i = 0;
  do {
    raster[i] = shell[i];
    i++;
  } while (i < 0x100);
  overlay = g_residentResources.assets.frownFace;
  cell = raster + 0x50;
  i = 0;
  do {
    cell[i] = (cell[i] | (overlay[i] * 8));
    cell[i + 0x40] = (cell[i + 0x40] | (overlay[i] / 0x20));
    i++;
  } while (i < 0x20);
  DisplayBlit(0x20, 0x10, 0x20, 0x20, raster);
  g_ui.view.presentation.frame++;
  if (g_ui.view.presentation.frame > 8) {
    HomeInit();
    SetView(VIEW_HOME);
  }
}

/* Draw the resident smiling walker with an optional signal icon. */
void RenderIrRomFrame(u8 signalIconRequested)
{
  u8 *raster;
  const u8 *shell;
  const u8 *overlay;
  u8 *cell;
  uint i;

  ScratchReset();
  raster = ScratchAlloc(0x100);
  if (signalIconRequested != 0) {
    DisplayBlit(0x2c, 0x00, 0x08, 0x08, g_residentResources.assets.irSignal);
  }
  shell = g_residentResources.assets.walkerImage;
  i = 0;
  do {
    raster[i] = shell[i];
    i++;
  } while (i < 0x100);
  overlay = g_residentResources.assets.smileFace;
  cell = raster + 0x50;
  i = 0;
  do {
    cell[i] = (cell[i] | (overlay[i] * 8));
    cell[i + 0x40] = (cell[i + 0x40] | (overlay[i] / 0x20));
    i++;
  } while (i < 0x20);
  DisplayBlit(0x20, 0x10, 0x20, 0x20, raster);
}

/* Draw the uploaded walker, connecting message and optional communication icon.
 */
void RenderIrUploadedFrame(u8 signalIconRequested)
{
  u8 *raster;
  u8 *eepromSource;

  ScratchReset();
  raster = ScratchAlloc(0x180);
  if (signalIconRequested != 0) {
    eepromSource = ((UiResources *)EEPROM_UI)->communicationIcon;
    EepromRead((u16)eepromSource, raster,
               sizeof(((UiResources *)0)->communicationIcon));
    DisplayBlit(0x2c, 0x00, 0x08, 0x10, raster);
  }
  eepromSource = ((UiResources *)EEPROM_UI)->walker;
  EepromRead((u16)eepromSource, raster, sizeof(((UiResources *)0)->walker));
  DisplayBlit(0x20, 0x10, 0x20, 0x20, raster);
  RenderMessage(0x30, MESSAGE_CONNECTING,
                BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                MESSAGE_NO_PROMPT);
}

void RenderIrResult(void)
{
  u8 *raster;
  u8 *eepromSource;

  ScratchReset();
  raster = ScratchAlloc(0x180);
  eepromSource = ((UiResources *)EEPROM_UI)->walker;
  EepromRead((u16)eepromSource, raster, sizeof(((UiResources *)0)->walker));
  DisplayBlit(0x20, 0x10, 0x20, 0x20, raster);
  switch (g_state.irResult) {
  case IR_RESULT_NO_RESPONSE:
    RenderMessage(0x30, MESSAGE_NO_TRAINER,
                  BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_BLINK_PROMPT);
    break;
  case IR_RESULT_CONNECTION_ERROR:
  case IR_RESULT_RECEIVE_OVERFLOW:
    RenderMessage(0x30, MESSAGE_CANNOT_CONNECT,
                  BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_BLINK_PROMPT);
    break;
  case IR_RESULT_REJECTED:
    RenderMessage(0x20, MESSAGE_CONNECTION_REJECTED_FIRST,
                  BORDER_TOP | BORDER_LEFT | BORDER_RIGHT, MESSAGE_NO_PROMPT);
    RenderMessage(0x30, MESSAGE_CONNECTION_REJECTED_SECOND,
                  BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_BLINK_PROMPT);
    break;
  case IR_RESULT_PEER_ALREADY_SEEN:
    RenderMessage(0x20, MESSAGE_PEER_ALREADY_SEEN_FIRST,
                  BORDER_TOP | BORDER_LEFT | BORDER_RIGHT, MESSAGE_NO_PROMPT);
    RenderMessage(0x30, MESSAGE_PEER_ALREADY_SEEN_SECOND,
                  BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_BLINK_PROMPT);
    break;
  case IR_RESULT_EVENT_ALREADY_RECEIVED:
    RenderMessage(0x20, MESSAGE_EVENT_ALREADY_RECEIVED_FIRST,
                  BORDER_TOP | BORDER_LEFT | BORDER_RIGHT, MESSAGE_NO_PROMPT);
    RenderMessage(0x30, MESSAGE_EVENT_ALREADY_RECEIVED_SECOND,
                  BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_BLINK_PROMPT);
    break;
  case IR_RESULT_EVENT_NOT_RECEIVED:
    RenderMessage(0x20, MESSAGE_EVENT_NOT_RECEIVED_FIRST,
                  BORDER_TOP | BORDER_LEFT | BORDER_RIGHT, MESSAGE_NO_PROMPT);
    RenderMessage(0x30, MESSAGE_EVENT_NOT_RECEIVED_SECOND,
                  BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_BLINK_PROMPT);
    break;
  case IR_RESULT_NO_POKEMON:
    RenderMessage(0x30, MESSAGE_NO_POKEMON,
                  BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_BLINK_PROMPT);
    break;
  }

  RenderBattery(0x58, 0x00);
}

/* Each update consumes one PRNG value before routing input to the view. */
void ViewUpdate(void)
{
  SystemFlags registered;

  RandomNext();
  switch (g_state.view) {
  case VIEW_HOME:
    HomeUpdate();
    break;
  case VIEW_SOCIAL:
    SocialUpdate();
    break;
  case VIEW_PEER:
    PeerUpdate();
    break;
  case VIEW_MAIN_MENU:
    MainMenuUpdate();
    break;
  case VIEW_DOWSING:
    DowsingUpdate();
    break;
  case VIEW_RADAR:
    RadarUpdate();
    break;
  case VIEW_BATTLE:
    BattleUpdate();
    break;
  case VIEW_RADAR_FAILURE:
    RadarFailureUpdate();
    break;
  case VIEW_DISCARD:
    DiscardUpdate();
    break;
  case VIEW_TRAINER:
    TrainerUpdate();
    break;
  case VIEW_WALK_INVENTORY:
    WalkInventoryUpdate();
    break;
  case VIEW_SETTINGS:
    SettingsUpdate();
    break;
  case VIEW_FRIEND_ITEMS:
    FriendItemInventoryUpdate();
    break;
  case VIEW_IR_RESULT:
    registered.byte = g_state.flags.byte;
    if (registered.bits.registered != 0) {
      if (InputPressed(BUTTON_ANY) != 0) {
        HomeInit();
        SetView(VIEW_HOME);
        BeepLoadScore(SCORE_CONFIRM);
      }
    }
    break;
  case VIEW_WALK_START:
    WalkStartUpdate();
    break;
  case VIEW_WALK_END:
    WalkEndUpdate();
    break;
  case VIEW_EVENT_REWARD:
    EventRewardUpdate();
    break;
  case VIEW_DIAGNOSTICS:
    DiagnosticsUpdate();
    break;
  case VIEW_THRESHOLD_TEST:
    ThresholdUpdate();
    break;
  default:
    break;
  }
  g_state.viewUpdates++;
}

/* Renderers may advance their view state. Call once per consumed refresh. */
void ViewRender(void)
{
  SystemFlags registered;

  switch (g_state.view) {
  case VIEW_HOME:
    registered.byte = g_state.flags.byte;
    if (registered.bits.registered == 0) {
      RenderUnregisteredHome();
    } else {
      HomeRender();
      RenderStepCount();
    }
    break;
  case VIEW_SOCIAL:
    SocialRender();
    break;
  case VIEW_PEER:
    PeerRender();
    break;
  case VIEW_MAIN_MENU:
    MainMenuRender();
    break;
  case VIEW_DOWSING:
    DowsingRender();
    break;
  case VIEW_RADAR:
    RadarRender();
    break;
  case VIEW_BATTLE:
    BattleRender();
    break;
  case VIEW_RADAR_FAILURE:
    RadarFailureRender();
    break;
  case VIEW_DISCARD:
    DiscardRender();
    break;
  case VIEW_TRAINER:
    TrainerRender();
    break;
  case VIEW_WALK_INVENTORY:
    WalkInventoryRender();
    break;
  case VIEW_SETTINGS:
    SettingsRender();
    break;
  case VIEW_FRIEND_ITEMS:
    FriendItemInventoryRender();
    break;
  case VIEW_IR_RESULT:
    registered.byte = g_state.flags.byte;
    if (registered.bits.registered == 0) {
      RenderUnregisteredResult();
    } else {
      RenderIrResult();
    }
    break;
  case VIEW_WALK_START:
    WalkStartRender();
    break;
  case VIEW_WALK_END:
    WalkEndRender();
    break;
  case VIEW_EVENT_REWARD:
    EventRewardRender();
    break;
  case VIEW_DIAGNOSTICS:
    DiagnosticsRender();
    break;
  case VIEW_THRESHOLD_TEST:
    ThresholdRender();
    break;
  case VIEW_THRESHOLD_FAILURE:
    ThresholdFailureRender();
    break;
  default:
    break;
  }
}

/* Draw daily steps and the home stamp, item and Pokemon icons. */
void RenderStepCount(void)
{
  u8 eventPresence;
  u8 *raster;
  u8 *eepromSource;
  UiResources *resources;
  Pokemon *pokemon;
  Item *item;
  int i;

  resources = (UiResources *)EEPROM_UI;
  DisplayMessageRule();
  eventPresence = EepromReadByte(EEPROM_EVENTS);
  ScratchReset();
  raster = ScratchAlloc(0x180);
  if ((eventPresence & EVENT_PRESENT_POKEMON) != 0) {
    eepromSource = resources->eventBall;
    EepromRead((u16)eepromSource, raster,
               sizeof(((UiResources *)0)->eventBall));
    i = 0;
    do {
      raster[i] |= 1;
      i++;
    } while (i < (int)sizeof(((UiResources *)0)->eventBall));
    DisplayBlit(0x00, 0x30, 0x08, 0x08, raster);
  }
  if ((eventPresence & EVENT_PRESENT_ITEM) != 0) {
    eepromSource = (u8 *)&((EventItem *)EEPROM_EVENT_ITEM)->header;
    EepromRead((u16)eepromSource, raster, sizeof(EventItemHeader));
    if (((EventItemHeader *)raster)->itemIdLe != 0) {
      eepromSource = resources->eventTreasure;
      EepromRead((u16)eepromSource, raster,
                 sizeof(((UiResources *)0)->eventTreasure));
      i = 0;
      do {
        raster[i] |= 1;
        i++;
      } while (i < (int)sizeof(((UiResources *)0)->eventTreasure));
      DisplayBlit(0x08, 0x30, 0x08, 0x08, raster);
    }
  }
  eepromSource = resources->stamps;
  EepromRead((u16)eepromSource, raster, sizeof(((UiResources *)0)->stamps));
  i = 0;
  do {
    raster[i] |= 1;
    i++;
  } while (i < (int)sizeof(((UiResources *)0)->stamps));
  if ((eventPresence & EVENT_PRESENT_STAMP0) != 0) {
    DisplayBlit(0x10, 0x30, 0x08, 0x08, raster);
  }
  if ((eventPresence & EVENT_PRESENT_STAMP1) != 0) {
    DisplayBlit(0x18, 0x30, 0x08, 0x08, raster + 0x10);
  }
  if ((eventPresence & EVENT_PRESENT_STAMP2) != 0) {
    DisplayBlit(0x20, 0x30, 0x08, 0x08, raster + 0x20);
  }
  if ((eventPresence & EVENT_PRESENT_STAMP3) != 0) {
    DisplayBlit(0x28, 0x30, 0x08, 0x08, raster + 0x30);
  }
  eepromSource = resources->ball;
  EepromRead((u16)eepromSource, raster, sizeof(((UiResources *)0)->ball));
  pokemon = (Pokemon *)(raster + sizeof(((UiResources *)0)->ball));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, pokemon), pokemon,
             (sizeof(Pokemon) * 3));
  i = 0;
  do {
    if (pokemon[i].idLe != 0) {
      DisplayBlit((i * 8), 0x38, 0x08, 0x08, raster);
    }
    i++;
  } while (i < 3);
  eepromSource = resources->treasure;
  EepromRead((u16)eepromSource, raster, sizeof(((UiResources *)0)->treasure));
  item = (Item *)(raster + sizeof(((UiResources *)0)->treasure));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items), item,
             (sizeof(Item) * 3));
  i = 0;
  do {
    if (item[i].idLe != 0) {
      DisplayBlit((i * 8 + 0x18), 0x38, 0x08, 0x08, raster);
    }
    i++;
  } while (i < 3);
  if ((eventPresence & EVENT_PRESENT_MAP) != 0) {
    eepromSource = resources->eventMap;
    EepromRead((u16)eepromSource, raster, sizeof(((UiResources *)0)->eventMap));
    DisplayBlit(0x30, 0x38, 0x08, 0x08, raster);
  }
  RenderDecimal(0x58, 0x30, g_state.dailySteps, NUMBER_TOP_RULE);
  RenderBattery(0, 0);
}

/* Reevaluate the volatile sample difference in the selected arm. */
#define PW_ABS(value) ((value) >= 0 ? (value) : -(value))

/* Wake the accelerometer for one conversion, retain the signed high byte
 * of each axis, and return it to sleep. Threshold diagnostics also sum
 * adjacent-sample changes, including the preceding ring sample. */
void CaptureSample(void)
{
  u8 sample[6];
  u8 previous;

  SSU.SSMR.BYTE = SSU_MODE3_SUB_DIV2;
  IO.PDR9.BIT.B0 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = ACCEL_REG_CONTROL;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  SSU.SSTDR = ACCEL_CONTROL_AWAKE;
  WDT.TCSRWD1.BYTE = 0x5e;
  WDT.TCWD = 0;
  WDT.TCSRWD1.BYTE = 0x9e;
  SYSCR1.BYTE = 0xa7;
  SYSCR2.BYTE = 0xeb;
  g_state.events.bits.lowPowerClock = 1;
  IO.PDR9.BIT.B0 = 1;
  SSU.SSMR.BYTE = SSU_MODE3_MAIN_DIV4;
  sleep();
  AccelRead(ACCEL_REG_X_LSB, sample, 6);
  IO.PDR9.BIT.B0 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = ACCEL_REG_CONTROL;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = ACCEL_CONTROL_SLEEP;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR9.BIT.B0 = 1;
  g_work.motion.x[g_state.sampleIndex] = sample[1];
  g_work.motion.y[g_state.sampleIndex] = sample[3];
  g_work.motion.z[g_state.sampleIndex] = sample[5];
  if (g_state.view == VIEW_THRESHOLD_TEST) {
    previous = ((g_state.sampleIndex + 0x3f) & 0x3f);
    if (g_state.sampleIndex == 0) {
      g_ui.view.accel.xActivity = 0;
      g_ui.view.accel.yActivity = 0;
      g_ui.view.accel.zActivity = 0;
    }
    g_ui.view.accel.xActivity += PW_ABS(g_work.motion.x[g_state.sampleIndex] -
                                        g_work.motion.x[previous]);
    g_ui.view.accel.yActivity += PW_ABS(g_work.motion.y[g_state.sampleIndex] -
                                        g_work.motion.y[previous]);
    g_ui.view.accel.zActivity += PW_ABS(g_work.motion.z[g_state.sampleIndex] -
                                        g_work.motion.z[previous]);
  }
}

/* Every wake collects a sample and scans buttons. Inactive operation checks
 * for activity; interactive operation first gives input to the current view.
 * A complete sample batch takes precedence over rendering, which in turn
 * defers RTC work. IR takes ownership immediately when a view starts it.
 * Otherwise step credit runs before any handoff to score playback. */
void MainTick(void)
{
  IENR2.BIT.IENTB1 = 1;
  ClockSleep(CLOCK_SLEEP_NORMAL);
  IENR2.BIT.IENTB1 = 0;
  IENR1.BIT.IENRTC = 0;
  CaptureSample();
  IENR1.BIT.IENRTC = 1;
  InputScan();
  if ((g_state.flags.byte & SYSTEM_MODE_MASK) == SYSTEM_MODE_INACTIVE) {
    RtcDispatch();
    if ((MotionActivityCheck() != 0) ||
        (g_state.events.bits.centerPressed != 0)) {
      MotionSessionWake();
    }
  } else {
    if ((g_state.flags.byte & SYSTEM_MODE_MASK) == SYSTEM_MODE_INTERACTIVE) {
      SocialOfferCheck();
      ViewUpdate();
      /* After the view starts IR, advance sampleIndex and return. IR owns the
       * motion and scratch storage from this point. */
      if (g_task == IrProtocolTick) {
        goto tickTail;
      }
    }
    if ((g_state.flags.bits.registered != 0) &&
        (g_state.sampleIndex == PW_ACCEL_RING_LAST)) {
      MotionProcess();
    } else {
      if (g_state.events.bits.uiRefreshPending != 0) {
        if ((g_state.flags.byte & SYSTEM_MODE_MASK) ==
            SYSTEM_MODE_INTERACTIVE) {
          DisplayClear(0x40);
          ViewRender();
          DisplayToggleBank();
          g_state.uiFrame++;
        }
        g_state.events.bits.uiRefreshPending = 0;
      } else {
        RtcDispatch();
        if ((g_state.flags.byte & SYSTEM_MODE_MASK) ==
            SYSTEM_MODE_INTERACTIVE) {
          if (g_state.idleSeconds[IDLE_DISPLAY] == 0) {
            DisplayEnterPowerSave();
            g_state.flags.byte =
                ((g_state.flags.byte & SYSTEM_MODE_CLEAR) | SYSTEM_MODE_MOTION);
            g_state.buttonWake[0] = 0;
            g_state.centerHoldScans = 0;
          }
        } else {
          set_ccr(0x80);
          BatteryUpdate();
          set_ccr(0);
          MotionSessionIdleCheck();
        }
      }
    }
    StepPacingTick();
    if (BeepHasScore() != 0) {
      InstallTask(BeepTick);
      IENR2.BIT.IENTB1 = 0;
      BeepEnableTimer();
    }
  }
tickTail:
  g_state.sampleIndex = ((g_state.sampleIndex + 1) & PW_ACCEL_RING_MASK);
}

/* Service input and view updates during playback. When the score ends, return
 * the workspace to motion and start a fresh sample ring. */
void BeepTick(void)
{
  SYSCR1.BYTE = 0x27;
  SYSCR2.BYTE = 0xe0;
  g_state.events.bits.lowPowerClock = 1;
  if (TW.GRA != 0) {
    sleep();
  }
  WatchdogService();
  InputScan();
  ViewUpdate();
  if (g_state.events.bits.uiRefreshPending != 0) {
    if ((g_state.flags.byte & SYSTEM_MODE_MASK) == SYSTEM_MODE_INTERACTIVE) {
      DisplayClear(0x40);
      ViewRender();
      DisplayToggleBank();
      g_state.uiFrame++;
    }
    g_state.events.bits.uiRefreshPending = 0;
  }
  if (BeepHasScore() == 0) {
    BeepDisableTimer();
    InstallTask(MainTick);
    g_state.sampleIndex = 0;
  }
}

/* Feeling bubble for each social offer. */
const u8 g_socialBubbles[8] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x05};

#include "flags.h"
#include "types.h"
#include "eeprom_address.h"
#include <stddef.h>
#include "project.h"
#include "application/pw_nt7508.h"
#include <machine.h>
#include "application/pw_buzzer.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_follower_events.h"
#include "support/ir.h"
#include "application/pw_home.h"
#include "application/pw_main.h"
#include "application/pw_pictogram_menu.h"
#include "application/pw_player_input.h"
#include "support/lib_common.h"
#include "support/scratch.h"

#define HOME_DISPLAY_HEIGHT_PIXELS 0x40
#define HOME_INITIAL_X 0x20
#define HOME_POKEMON_LEFT_X 0x20
#define HOME_POKEMON_ENTRY_RIGHT_X 0x60
#define HOME_POKEMON_IDLE_RIGHT_X 0x40
#define HOME_POKEMON_OFFSCREEN_X 0x68
#define HOME_POKEMON_STEP_PIXELS 4
#define HOME_POKEMON_ICON_Y 0x18

void InstallTask(void (*nextTarget)(void))
{
  g_previousTask = g_task;
  g_task = nextTarget;
}

/* With MainTick in the foreground, prepare the two connection frames and start
 * IR with interrupts masked. Registered walkers use uploaded artwork and a
 * battery icon; unregistered walkers use the resident smile/signal images. */
void TryBeginIr(void)
{
  SystemFlags flags;

  if (g_task == MainTick) {
    flags.byte = g_state.flags.byte;
    if (flags.bits.registered == 0) {
      DisplayClear(HOME_DISPLAY_HEIGHT_PIXELS);
      RenderIrRomFrame(0);
      DisplayToggleBank();
      DisplayClear(HOME_DISPLAY_HEIGHT_PIXELS);
      RenderIrRomFrame(1);
    } else {
      DisplayClear(HOME_DISPLAY_HEIGHT_PIXELS);
      RenderIrUploadedFrame(0);
      RenderBattery(0, 0);
      DisplayToggleBank();
      DisplayClear(HOME_DISPLAY_HEIGHT_PIXELS);
      RenderIrUploadedFrame(1);
      RenderBattery(0, 0);
    }
    DisplayToggleBank();
    set_ccr(0x80);
    IrBegin();
    InstallTask(IrProtocolTick);
  }
}

void SetView(u8 nextView)
{
  g_state.view = nextView;
}

/* UI byte 1 selects image frame 0 whenever nonzero. It is the home offer
 * countdown and the arrival/departure frame counter. When it is zero, alternate
 * image frames every two UI refreshes. Each call resets scratch. */
void RenderLargePokemon(u8 x, u8 y)
{
  u8 *raster;
  u16 length;

  length = POKEMON_LARGE_FRAME_BYTES;
  ScratchReset();
  raster = ScratchAlloc(length);
  EepromRead(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources, values.pokemon),
      raster, sizeof(Pokemon));
  if (g_ui.view.home.eventOfferCountdown != 0) {
    EepromRead((u16)((CourseResources *)EEPROM_COURSE)->pokemonImageLarge,
               raster, length);
  } else {
    EepromRead((u16)(((CourseResources *)EEPROM_COURSE)->pokemonImageLarge +
                     ((g_state.uiFrame >> 1) & 1) * POKEMON_LARGE_FRAME_BYTES),
               raster, length);
  }
  DisplayBlit(x, y, 0x40, 0x30, raster);
}

void HomeInit(void)
{
  g_ui.view.home.pendingEventId = 0;
  g_ui.view.home.eventOfferCountdown = 0;
  g_ui.view.home.pokemonX = HOME_INITIAL_X;
  g_ui.view.home.animationControl.bits.entered = 0;
  g_ui.view.home.animationControl.bits.smallFrame = 0;
  g_ui.view.home.animationControl.bits.movingRight = 0;
}

/* Accept a pending social offer on the next button press. Count its lifetime
 * in HomeUpdate calls. */
void HomeUpdate(void)
{
  {
    SystemFlags registered;

    registered.byte = g_state.flags.byte;
    if (registered.bits.registered == 0) {
      if (InputPressed(BUTTON_CENTER) == 0) {
        SystemEvents event;

        event.byte = g_state.events.byte;
        if (event.bits.irRequested == 0) {
          return;
        }
      }
      TryBeginIr();
      return;
    }
  }

  if (g_ui.view.home.eventOfferCountdown != 0) {
    if (InputPressed(BUTTON_ANY) != 0) {
      g_ui.view.home.eventOfferCountdown = 0;
      SocialApply(g_ui.view.home.pendingEventId);
      return;
    }
    g_ui.view.home.eventOfferCountdown--;
  }

  if (InputPressed(BUTTON_CENTER) != 0) {
    BeepLoadScore(SCORE_CONFIRM);
    MenuReset();
    g_state.menuSelection = MENU_CONNECT;
  } else {
    if (InputPressed(BUTTON_LEFT) != 0) {
      g_state.menuSelection = MENU_SETTINGS;
      BeepLoadScore(SCORE_CONFIRM);
      MenuReset();
    } else {
      if (InputPressed(BUTTON_RIGHT) == 0) {
        return;
      }
      g_state.menuSelection = MENU_RADAR;
      BeepLoadScore(SCORE_CONFIRM);
      MenuReset();
    }
  }
  SetView(VIEW_MAIN_MENU);
}

/* Draw one of the seven uploaded feeling bubbles. */
void RenderFeeling(u8 recordIndex)
{
  u8 *raster;
  u8 *eepromSource;

  ScratchReset();
  raster = ScratchAlloc(sizeof(((UiResources *)0)->feelings) / 7);
  eepromSource = ((UiResources *)EEPROM_UI)->feelings +
                 recordIndex * (sizeof(((UiResources *)0)->feelings) / 7);
  EepromRead((u16)eepromSource, raster,
             sizeof(((UiResources *)0)->feelings) / 7);
  DisplayBlit(0x04, 0x04, 0x18, 0x10, raster);
}

void RenderCourseBackground(void)
{
  u8 *raster;
  u8 *eepromSource;

  ScratchReset();
  raster = ScratchAlloc(sizeof(((CourseResources *)0)->background));
  if (g_state.save.bonusCourse) {
    eepromSource = ((BonusResources *)EEPROM_BONUS_COURSE)->background;
  } else {
    eepromSource = ((CourseResources *)EEPROM_COURSE)->background;
  }
  EepromRead((u16)eepromSource, raster,
             sizeof(((CourseResources *)0)->background));
  DisplayBlit(0x00, 0x18, 0x20, 0x18, raster);
}

/* Motion at the left boundary starts the entry animation. Switch to the small
 * sprite after reaching the far boundary. */
void HomeEntryAdvance(void)
{
  {
    HomeMotionFlags motion;

    motion.byte = g_ui.view.home.animationControl.byte;
    if (motion.bits.movingRight == 0) {
      g_ui.view.home.pokemonX =
          (g_ui.view.home.pokemonX - HOME_POKEMON_STEP_PIXELS);
      if (g_ui.view.home.pokemonX <= HOME_POKEMON_LEFT_X) {
        g_ui.view.home.pokemonX = HOME_POKEMON_LEFT_X;
      }
    } else {
      g_ui.view.home.pokemonX =
          (g_ui.view.home.pokemonX + HOME_POKEMON_STEP_PIXELS);
      if (g_ui.view.home.pokemonX >= HOME_POKEMON_ENTRY_RIGHT_X) {
        g_ui.view.home.animationControl.bits.smallFrame = 1;
        g_ui.view.home.animationControl.bits.movingRight = 0;
      }
    }
  }

  {
    SystemEvents event;

    event.byte = g_state.events.byte;
    if (event.bits.motionDetected != 0) {
      if (g_ui.view.home.pokemonX <= HOME_POKEMON_LEFT_X) {
        g_ui.view.home.animationControl.bits.movingRight = 1;
        g_ui.view.home.animationControl.bits.entered = 1;
      }
    }
  }
}

/* Advance every fourth UI frame. Park the Pokemon offscreen when motion stops.
 */
void HomeOscillationAdvance(void)
{
  HomeMotionFlags flags;

  if ((g_state.uiFrame & 3) != 0) {
    return;
  }

  flags.byte = g_ui.view.home.animationControl.byte;
  if (flags.bits.movingRight == 0) {
    g_ui.view.home.pokemonX =
        (g_ui.view.home.pokemonX - HOME_POKEMON_STEP_PIXELS);
    if (g_ui.view.home.pokemonX <= HOME_POKEMON_LEFT_X) {
      g_ui.view.home.pokemonX = HOME_POKEMON_LEFT_X;
      g_ui.view.home.animationControl.bits.movingRight ^= 1;
    }
  } else {
    g_ui.view.home.pokemonX =
        (g_ui.view.home.pokemonX + HOME_POKEMON_STEP_PIXELS);
    if (g_ui.view.home.pokemonX >= HOME_POKEMON_IDLE_RIGHT_X) {
      g_ui.view.home.pokemonX = HOME_POKEMON_IDLE_RIGHT_X;
      g_ui.view.home.animationControl.bits.movingRight ^= 1;
    }
  }

  if ((g_state.events.byte & EVENT_MOTION) != 0) {
    return;
  }
  g_ui.view.home.pokemonX = HOME_POKEMON_OFFSCREEN_X;
  g_ui.view.home.animationControl.bits.smallFrame = 0;
  g_ui.view.home.animationControl.bits.movingRight = 0;
}

/* Draw the current position and sprite size, then advance the animation for
 * the next render. */
void HomeRender(void)
{
  u8 pokemonX;
  u8 *raster;
  Pokemon *pokemon;

  if (g_ui.view.home.eventOfferCountdown != 0) {
    RenderFeeling(g_socialBubbles[g_ui.view.home.pendingEventId - 1]);
  }
  RenderCourseBackground();

  if (g_state.flags.bits.hasPokemon == 0) {
    return;
  }

  pokemonX = g_ui.view.home.pokemonX;
  if (g_ui.view.home.animationControl.bits.smallFrame == 0) {
    RenderLargePokemon(pokemonX, 0);
  } else if (g_ui.view.home.animationControl.bits.movingRight == 0) {
    RenderHeldPokemon(pokemonX, HOME_POKEMON_ICON_Y);
  } else {
    ScratchReset();
    raster = ScratchAlloc(sizeof(Pokemon));
    EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                                        values.pokemon),
               raster, sizeof(Pokemon));
    pokemon = (Pokemon *)raster;
    if (pokemon->fixedFacing == 0) {
      RenderHeldPokemonMirrored(g_ui.view.home.pokemonX, HOME_POKEMON_ICON_Y);
    } else {
      RenderHeldPokemon(g_ui.view.home.pokemonX, HOME_POKEMON_ICON_Y);
    }
  }

  if (g_ui.view.home.animationControl.bits.smallFrame != 0) {
    HomeOscillationAdvance();
  } else {
    HomeEntryAdvance();
  }
}

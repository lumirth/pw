#include "types.h"
#include "eeprom_address.h"
#include <stddef.h>
#include "project.h"
#include "application/pw_diary.h"
#include "application/pw_nt7508.h"
#include <machine.h>
#include "application/pw_buzzer.h"
#include "application/pw_carry_overflow.h"
#include "application/pw_dowsing.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_home.h"
#include "application/pw_player_input.h"
#include "support/lib_common.h"
#include "support/scratch.h"

extern const s8 g_dowsingGrassShift[];

void EventItemRead(u8 *destination);
void EventItemWrite(u8 *source);
void DowsingChooseReward(void);

#define PW_EEPROM_ITEM_SLOTS                                                   \
  (PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items))
#define DOWSING_PATCH_COUNT 6
#define DOWSING_NO_EMPTY_PATCH 0xff
#define DOWSING_BONUS_ITEM 10

#define DOWSING_SELECT 0
#define DOWSING_REVEAL 1
#define DOWSING_FOUND 2
#define DOWSING_EMPTY 3
#define DOWSING_HINT 4

/* Both attempts search the same hidden patch. The first empty patch stays
 * disabled. */
void DowsingInit(void)
{
  g_ui.view.dowsing.phase = DOWSING_SELECT;
  g_ui.view.dowsing.selectedPatch = 0;
  g_ui.view.dowsing.attemptsLeft = 2;
  g_ui.view.dowsing.hiddenPatch =
      ((u8)(((u16)RandomNext() << 3) >> 8) % DOWSING_PATCH_COUNT);
  g_ui.view.dowsing.emptyPatch = DOWSING_NO_EMPTY_PATCH;
  g_ui.view.dowsing.wattsAwarded = 0;
}

/* Confirming a patch starts a reveal timed by rendered frames. Result and hint
 * panels advance on a button press. */
void DowsingUpdate(void)
{
  switch (g_ui.view.dowsing.phase) {
  case DOWSING_SELECT:
    if (InputPressed(BUTTON_CENTER) != 0) {
      if (g_ui.view.dowsing.selectedPatch == g_ui.view.dowsing.emptyPatch) {
        BeepLoadScore(SCORE_BACK);
        return;
      }
      BeepLoadScore(SCORE_CONFIRM);
      g_ui.view.dowsing.phase = DOWSING_REVEAL;
      g_ui.view.dowsing.revealFramesLeft = 4;
      return;
    }
    if (InputPressed(BUTTON_LEFT) != 0) {
      g_ui.view.dowsing.selectedPatch =
          ((g_ui.view.dowsing.selectedPatch + 5) % DOWSING_PATCH_COUNT);
      BeepLoadScore(SCORE_MOVE);
    }
    if (InputPressed(BUTTON_RIGHT) != 0) {
      g_ui.view.dowsing.selectedPatch =
          ((g_ui.view.dowsing.selectedPatch + 1) % DOWSING_PATCH_COUNT);
      BeepLoadScore(SCORE_MOVE);
    }
    return;
  case DOWSING_REVEAL:
    if (g_ui.view.dowsing.revealFramesLeft != 0) {
      return;
    }
    if (BeepHasScore() != 0) {
      return;
    }
    if (g_ui.view.dowsing.selectedPatch == g_ui.view.dowsing.hiddenPatch) {
      g_ui.view.dowsing.phase = DOWSING_FOUND;
      DowsingChooseReward();
      BeepLoadScore(SCORE_FOUND);
      return;
    }
    g_ui.view.dowsing.phase = DOWSING_EMPTY;
    BeepLoadScore(SCORE_FAILURE);
    if (g_ui.view.dowsing.attemptsLeft == 2) {
      g_ui.view.dowsing.emptyPatch = g_ui.view.dowsing.selectedPatch;
    }
    g_ui.view.dowsing.attemptsLeft--;
    return;
  case DOWSING_FOUND:
    /* Acknowledgment stores course items. Bonus items and watts are stored
     * when the successful patch is revealed. */
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    if (g_state.save.bonusCourse == 0) {
      u8 slot;

      slot = g_ui.view.dowsing.freeItemSlot;
      if (slot == 3) {
        g_ui.view.discard.inventoryKind = PW_DISCARD_INVENTORY_ITEM;
        g_ui.view.discard.sourceSelection = g_ui.view.dowsing.itemIndex;
        DiscardInit();
        SetView(VIEW_DISCARD);
        return;
      }
      /* Replace only the item ID; preserve the other bytes of its record. */
      EepromWrite((slot * 4 + PW_EEPROM_ITEM_SLOTS),
                  &g_ui.view.dowsing.itemIdLe, 2);
      if (g_state.flags.bits.hasPokemon != 0) {
        u8 *course;

        course = ScratchAlloc(sizeof(Course));
        EepromRead(EEPROM_COURSE, course, sizeof(Course));
        DiaryAppend((Course *)course, ScratchAlloc(sizeof(DiaryEntry)),
                    PW_DIARY_ACTION_DOWSING_ITEM, 0, 0,
                    g_ui.view.dowsing.itemIdLe);
      }
    }
    BeepLoadScore(SCORE_CONFIRM);
    HomeInit();
    SetView(VIEW_HOME);
    return;
  case DOWSING_EMPTY:
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    if (g_ui.view.dowsing.attemptsLeft == 0) {
      BeepLoadScore(SCORE_CONFIRM);
      HomeInit();
      SetView(VIEW_HOME);
      return;
    }
    BeepLoadScore(SCORE_CONFIRM);
    g_ui.view.dowsing.phase = DOWSING_HINT;
    return;
  case DOWSING_HINT:
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    BeepLoadScore(SCORE_CONFIRM);
    g_ui.view.dowsing.phase = DOWSING_SELECT;
    return;
  }
}

/* Event-item transfers begin at the header's EEPROM address and cover the
 * complete record. */
void EventItemRead(u8 *destination)
{
  EventItemHeader *header;

  header = &((EventItem *)EEPROM_EVENT_ITEM)->header;
  EepromRead((u16)header, destination, sizeof(EventItem));
}

void EventItemWrite(u8 *source)
{
  EventItemHeader *header;

  header = &((EventItem *)EEPROM_EVENT_ITEM)->header;
  EepromWrite((u16)header, source, sizeof(EventItem));
}

/* Store an eligible bonus item immediately. Otherwise award ten watts on
 * the first successful guess, or six after an earlier miss. */
void DowsingAwardBonus(void)
{
  DeviceStatus *status;
  EventItem *eventItem;
  BonusCourse *bonus;
  u8 *course;
  uint requiredSteps;
  u8 noEncounter;
  u8 flags;

  ScratchReset();
  status = ScratchAlloc(0x68);
  eventItem = ScratchAlloc(sizeof(EventItem));
  bonus = ScratchAlloc(sizeof(BonusCourse));
  EepromRead(EEPROM_BONUS_COURSE, bonus, sizeof(BonusCourse));
  requiredSteps = bonus->itemStepsLe;
  requiredSteps = (requiredSteps >> 8) | (requiredSteps << 8);
  if (g_state.dailySteps < requiredSteps) {
    g_ui.view.dowsing.wattsAwarded = (g_ui.view.dowsing.attemptsLeft * 4 + 2);
    WattsAdd(g_ui.view.dowsing.wattsAwarded);
    return;
  }
  /* Reduce an eight-bit draw modulo 100 for the bonus-item chance. */
  if ((u8)(RandomNext() >> 3) % 100 >= bonus->itemChance) {
    g_ui.view.dowsing.wattsAwarded = (g_ui.view.dowsing.attemptsLeft * 4 + 2);
    WattsAdd(g_ui.view.dowsing.wattsAwarded);
    return;
  }
  EventItemRead((u8 *)eventItem);
  if (eventItem->header.itemIdLe != 0) {
    g_ui.view.dowsing.wattsAwarded = (g_ui.view.dowsing.attemptsLeft * 4 + 2);
    WattsAdd(g_ui.view.dowsing.wattsAwarded);
    return;
  }
  if (StatusLoadReceived(status, bonus->itemReceipt) != 0) {
    g_ui.view.dowsing.wattsAwarded = (g_ui.view.dowsing.attemptsLeft * 4 + 2);
    WattsAdd(g_ui.view.dowsing.wattsAwarded);
    return;
  }
  StatusSetReceived(status, bonus->itemReceipt);
  course = ScratchAlloc(sizeof(Course));
  EepromRead(EEPROM_COURSE, course, sizeof(Course));
  g_ui.view.dowsing.itemIndex = DOWSING_BONUS_ITEM;
  eventItem->header.prefix.transfer.word = bonus->itemPrefix.transfer.word;
  eventItem->header.prefix.transfer.halfword =
      bonus->itemPrefix.transfer.halfword;
  eventItem->header.itemIdLe = bonus->itemIdLe;
  EepromRead((u16)((BonusResources *)EEPROM_BONUS_COURSE)->itemName,
             eventItem->itemName, sizeof(eventItem->itemName));
  EventItemWrite((u8 *)eventItem);
  flags = (EepromReadByte(EEPROM_EVENTS) | EVENT_PRESENT_ITEM);
  EepromWriteByte(EEPROM_EVENTS, flags);
  if (g_state.flags.bits.hasPokemon == 0) {
    return;
  }
  noEncounter = 0;
  DiaryAppend((Course *)course, ScratchAlloc(sizeof(DiaryEntry)),
              PW_DIARY_ACTION_BONUS_COURSE_ITEM, 1, noEncounter,
              eventItem->header.itemIdLe);
}

/* Test eligible course items in order with fresh draws; use the last item if
 * none succeeds. */
void DowsingChooseReward(void)
{
  Item *items;
  Course *course;
  u8 itemIndex;

  ScratchReset();
  items = ScratchAlloc(sizeof(((WalkData *)0)->items));
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, items), items,
             sizeof(((WalkData *)0)->items));
  g_ui.view.dowsing.freeItemSlot = ItemSlotFindEmpty(items);
  if (g_state.save.bonusCourse != 0) {
    DowsingAwardBonus();
    return;
  }
  course = ScratchAlloc(sizeof(Course));
  EepromRead(EEPROM_COURSE, course, sizeof(Course));
  itemIndex = 0;
  do {
    uint itemSlot = itemIndex;

    if (g_state.dailySteps >= ((course->itemStepsLe[itemSlot] >> 8) |
                               (course->itemStepsLe[itemSlot] << 8))) {
      if ((u8)(RandomNext() >> 3) % 100 < course->itemChance[itemSlot]) {
        break;
      }
    }
    itemIndex++;
  } while (itemIndex < 10);
  if (itemIndex > 9) {
    itemIndex = 9;
  }
  g_ui.view.dowsing.itemIndex = itemIndex;
  g_ui.view.dowsing.itemIdLe = course->itemIdLe[itemIndex];
}

/* Bob the selected patch and count down the reveal in rendered frames. */
void DowsingRenderReveal(void)
{
  UiResources *image;
  u8 *raster;
  u16 courseImageAddress;
  int patch;
  u8 x;

  image = (UiResources *)EEPROM_UI;
  ScratchReset();
  raster = ScratchAlloc(0x180);
  EepromRead((u16)(image->digits + g_ui.view.dowsing.attemptsLeft * 0x20),
             raster, 0x20);
  DisplayBlit(0x40, 0x00, 0x08, 0x10, raster);
  EepromRead((u16)image->attemptsLabel, raster, sizeof(image->attemptsLabel));
  DisplayBlit(0x20, 0x00, 0x20, 0x10, raster);
  EepromRead((u16)image->attemptsSuffix, raster, sizeof(image->attemptsSuffix));
  DisplayBlit(0x48, 0x00, 0x18, 0x10, raster);
  if (g_state.save.bonusCourse != 0) {
    courseImageAddress =
        (u16)((BonusResources *)EEPROM_BONUS_COURSE)->background;
  } else {
    courseImageAddress = (u16)((CourseResources *)EEPROM_COURSE)->background;
  }
  EepromRead(courseImageAddress, raster, 0xc0);
  DisplayBlit(0, 0, 0x20, 0x18, raster);
  if (g_ui.view.dowsing.revealFramesLeft != 0) {
    g_ui.view.dowsing.revealFramesLeft--;
  }
  EepromRead((u16)image->grass, raster,
             sizeof(image->grass) + sizeof(image->litGrass));
  patch = 0;
  do {
    x = (patch * 0x10);
    if (patch == g_ui.view.dowsing.emptyPatch) {
      DisplayBlit(x, 0x18, 0x10, 0x10, raster + sizeof(image->grass));
    } else if (patch != g_ui.view.dowsing.selectedPatch) {
      DisplayBlit(x, 0x18, 0x10, 0x10, raster);
    }
    patch++;
  } while (patch < DOWSING_PATCH_COUNT);
  RasterShift(0x10, 0x10, g_dowsingGrassShift[g_state.uiFrame & 3], raster);
  DisplayBlit((g_ui.view.dowsing.selectedPatch * 0x10), 0x18, 0x10, 0x10,
              raster);
  RenderMessage(0x30, MESSAGE_DOWSING_SEARCH,
                BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                MESSAGE_NO_PROMPT);
}

/* Each render advances the reveal countdown. Field transfers finish before
 * message helpers reuse scratch. */
void DowsingRender(void)
{
  UiResources *image;
  u8 *digits;
  u8 *raster;
  u8 *resourceAddress;
  u8 *courseImageAddress;
  int patch;
  u8 itemIndex;
  u8 x;

  image = (UiResources *)EEPROM_UI;
  if (g_ui.view.dowsing.phase == DOWSING_REVEAL) {
    DowsingRenderReveal();
    RenderBattery(0, 0);
    return;
  }

  ScratchReset();
  digits = ScratchAlloc(0x140);
  raster = ScratchAlloc(0x180);
  resourceAddress = image->digits;
  EepromRead((u16)resourceAddress, digits, 0x140);
  DisplayBlit(0x40, 0x00, 0x08, 0x10,
              digits + g_ui.view.dowsing.attemptsLeft * 0x20);
  resourceAddress = image->attemptsLabel;
  EepromRead((u16)resourceAddress, raster, sizeof(image->attemptsLabel));
  DisplayBlit(0x20, 0x00, 0x20, 0x10, raster);
  resourceAddress = image->attemptsSuffix;
  EepromRead((u16)resourceAddress, raster, sizeof(image->attemptsSuffix));
  DisplayBlit(0x48, 0x00, 0x18, 0x10, raster);
  if (g_state.save.bonusCourse != 0) {
    courseImageAddress = ((BonusResources *)EEPROM_BONUS_COURSE)->background;
  } else {
    courseImageAddress = ((CourseResources *)EEPROM_COURSE)->background;
  }
  EepromRead((u16)courseImageAddress, raster, 0xc0);
  DisplayBlit(0, 0, 0x20, 0x18, raster);
  if (g_ui.view.dowsing.revealFramesLeft != 0) {
    g_ui.view.dowsing.revealFramesLeft--;
  }
  resourceAddress = image->grass;
  EepromRead((u16)resourceAddress, raster,
             sizeof(image->grass) + sizeof(image->litGrass));
  patch = 0;
  do {
    if ((g_ui.view.dowsing.phase != DOWSING_FOUND) ||
        (patch != g_ui.view.dowsing.selectedPatch)) {
      x = (patch * 0x10);
      if (patch == g_ui.view.dowsing.emptyPatch) {
        DisplayBlit(x, 0x18, 0x10, 0x10, raster + sizeof(image->grass));
      } else {
        DisplayBlit(x, 0x18, 0x10, 0x10, raster);
      }
    }
    patch++;
  } while (patch < DOWSING_PATCH_COUNT);

  switch (g_ui.view.dowsing.phase) {
  case DOWSING_SELECT:
    resourceAddress =
        image->cursorArrows + CURSOR_OFFSET(CURSOR_UP, g_state.uiFrame & 1);
    EepromRead((u16)resourceAddress, raster, 0x10);
    DisplayBlit((g_ui.view.dowsing.selectedPatch * 0x10 + 4), 0x28, 0x08, 0x08,
                raster);
    RenderMessage(0x30, MESSAGE_DOWSING_SEARCH,
                  BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_NO_PROMPT);
    break;
  case DOWSING_FOUND:
    resourceAddress = image->treasure;
    EepromRead((u16)resourceAddress, raster, 0x10);
    DisplayBlit((g_ui.view.dowsing.selectedPatch * 0x10 + 4), 0x18, 0x08, 0x08,
                raster);
    if (g_ui.view.dowsing.wattsAwarded != 0) {
      RenderWatts(0x02, 0x20, g_ui.view.dowsing.wattsAwarded, 0x0d);
      RenderMessage(0x30, MESSAGE_RECEIVED,
                    BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                    MESSAGE_BLINK_PROMPT);
    } else {
      itemIndex = g_ui.view.dowsing.itemIndex;
      if (itemIndex >= DOWSING_BONUS_ITEM) {
        RenderDistributionItem(0x00, 0x20,
                               BORDER_TOP | BORDER_LEFT | BORDER_RIGHT);
      } else {
        RenderCourseItem(0x00, 0x20, itemIndex,
                         BORDER_TOP | BORDER_LEFT | BORDER_RIGHT);
      }
      RenderMessage(0x30, MESSAGE_FOUND,
                    BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                    MESSAGE_BLINK_PROMPT);
    }
    break;
  case DOWSING_EMPTY:
    RenderMessage(0x30, MESSAGE_DOWSING_EMPTY,
                  BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_BLINK_PROMPT);
    if (g_ui.view.dowsing.attemptsLeft == 0) {
      int copiesLeft;

      /* Reload the treasure raster after RenderMessage reuses scratch. */
      resourceAddress = image->treasure;
      EepromRead((u16)resourceAddress, raster, 0x10);
      copiesLeft = 3;
      do {
        DisplayBlit((g_ui.view.dowsing.hiddenPatch * 0x10 + 4), 0x16, 0x08,
                    0x08, raster);
        copiesLeft--;
      } while (copiesLeft != 0);
    }
    break;
  case DOWSING_HINT:
    /* Hints use the distance between patch positions; the two end patches
     * are far apart. */
    if (((g_ui.view.dowsing.selectedPatch - g_ui.view.dowsing.hiddenPatch) >= 0
             ? (g_ui.view.dowsing.selectedPatch - g_ui.view.dowsing.hiddenPatch)
             : -(g_ui.view.dowsing.selectedPatch -
                 g_ui.view.dowsing.hiddenPatch)) < 2) {
      RenderMessage(0x30, MESSAGE_DOWSING_NEAR,
                    BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                    MESSAGE_BLINK_PROMPT);
    } else {
      RenderMessage(0x30, MESSAGE_DOWSING_FAR,
                    BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                    MESSAGE_BLINK_PROMPT);
    }
    break;
  }
  RenderBattery(0, 0);
}

/* Vertical bob of the selected patch over four rendered UI frames. */
const s8 g_dowsingGrassShift[4] = {2, -2, 2, -2};

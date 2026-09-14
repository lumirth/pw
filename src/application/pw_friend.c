#include "flags.h"
#include "types.h"
#include "eeprom_address.h"
#include "project.h"
#include "application/pw_diary.h"
#include "application/pw_nt7508.h"
#include "application/pw_buzzer.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_friend.h"
#include "application/pw_home.h"
#include "support/lib_common.h"
#include "support/scratch.h"

extern const u8 g_peerNoteShifts[6];

#define PEER_ENTER 0
#define PEER_GREETING 1
#define PEER_PLAY 2
#define PEER_PRESENT 3
#define PEER_REWARD 4
#define PEER_COMPLETE 5

void PeerAwardGift(void);
void PeerStoreHistory(void);
void PeerAppendDiary(void);

#define PW_PEER_SHIFT_SRC                                                      \
  (EEPROM_PEER_RECORDS + (PEER_HISTORY_SLOTS - 1) * sizeof(PeerRecords))
#define PW_PEER_SHIFT_DST                                                      \
  (EEPROM_PEER_RECORDS + PEER_HISTORY_SLOTS * sizeof(PeerRecords))
#define PW_PEER_SHIFT_BYTES sizeof(PeerRecords)
#define PEER_DIARY_NAME_COPY_BYTES 18

typedef union {
  PeerInfo record;
  struct {
    u8 prefix[offsetof(PeerInfo, trainerName)];
    u8 bytes[PEER_DIARY_NAME_COPY_BYTES];
  } nameSpan;
} PeerNameView;

typedef union {
  DiaryEntry record;
  struct {
    u8 prefix[offsetof(DiaryEntry, trainerName)];
    u8 bytes[PEER_DIARY_NAME_COPY_BYTES];
  } nameSpan;
} DiaryNameView;

typedef char PeerNameSpanFits[offsetof(PeerInfo, trainerName) +
                                          PEER_DIARY_NAME_COPY_BYTES <=
                                      sizeof(PeerInfo)
                                  ? 1
                                  : -1];
typedef char DiaryNameSpanFits[offsetof(DiaryEntry, trainerName) +
                                           PEER_DIARY_NAME_COPY_BYTES <=
                                       sizeof(DiaryEntry)
                                   ? 1
                                   : -1];

void RenderPeerNote(u8 x, u8 y, u8 columnBitShift);

/* Apply the peer's sprite orientation, resolve the gift, and update the peer
 * history and diary. */
void PeerFinalize(void)
{
  PeerInfo *peer;

  ScratchReset();
  peer = ScratchAlloc(sizeof(PeerInfo));
  EepromRead(EEPROM_PEER_INFO, peer, sizeof(PeerInfo));
  g_ui.view.peer.session.bits.fixedFacing = peer->fixedFacing;
  g_ui.view.peer.phase = PEER_ENTER;
  g_ui.view.peer.phaseFrame = 0;
  PeerAwardGift();
  PeerStoreHistory();
  PeerAppendDiary();
}

/* PeerRender advances this sequence automatically. */
void PeerUpdate(void)
{
}

/* The gift score combines both daily totals with ten times the hourly sum,
 * then caps at 20,000. The hourly sum and product wrap in native 16-bit
 * arithmetic before joining the daily totals. A full gift inventory converts
 * the score to 1..99 watts; otherwise each tier selects one of two course
 * items. The larger daily total gets the even index; ties take the odd index.
 */
void PeerAwardGift(void)
{
  PeerInfo *peer;
  Item *items;
  u32 giftScore;
  u8 freeSlot;

  ScratchReset();
  /* Reserve the prefix for CourseLoadItemIdLe's scratch reset and course
   * reload. Use the peer and item bytes beyond it before another scratch
   * allocation.
   */
  ScratchAlloc(sizeof(Course));
  peer = ScratchAlloc(sizeof(PeerInfo));
  EepromRead(EEPROM_PEER_INFO, peer, sizeof(PeerInfo));
  items = ScratchAlloc(sizeof(Item) * 10);
  EepromRead(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, friendItems),
             items, sizeof(Item) * 10);
  giftScore = g_state.dailySteps + peer->dailySteps +
              ((g_state.hourSteps + peer->hourSteps) * 10);
  if (giftScore > 20000ul) {
    giftScore = 20000ul;
  }
  freeSlot = 0;
  do {
    if (items[freeSlot].idLe == 0) {
      break;
    }
    freeSlot++;
  } while (freeSlot < 10);
  g_ui.view.peer.wattsAwarded = 0;
  if (freeSlot >= 10) {
    if ((g_ui.view.peer.wattsAwarded = (giftScore / 200ul)) == 0) {
      g_ui.view.peer.wattsAwarded = 1;
    }
    if (g_ui.view.peer.wattsAwarded > 99) {
      g_ui.view.peer.wattsAwarded = 99;
    }
    WattsAdd(g_ui.view.peer.wattsAwarded);
  }
  if (giftScore >= 20000ul) {
    g_ui.view.peer.giftMessageId = MESSAGE_PEER_PLAY_FIVE_NOTES;
    if (g_ui.view.peer.wattsAwarded != 0) {
      return;
    }
    if (g_state.dailySteps > peer->dailySteps) {
      g_ui.view.peer.itemIndex = 0;
    } else {
      g_ui.view.peer.itemIndex = 1;
    }
    items[freeSlot].idLe = CourseLoadItemIdLe(g_ui.view.peer.itemIndex);
    EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, friendItems),
                items, sizeof(Item) * 10);
  } else if (giftScore >= 10000ul) {
    g_ui.view.peer.giftMessageId = MESSAGE_PEER_PLAY_FOUR_NOTES;
    if (g_ui.view.peer.wattsAwarded != 0) {
      return;
    }
    if (g_state.dailySteps > peer->dailySteps) {
      g_ui.view.peer.itemIndex = 2;
    } else {
      g_ui.view.peer.itemIndex = 3;
    }
    items[freeSlot].idLe = CourseLoadItemIdLe(g_ui.view.peer.itemIndex);
    EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, friendItems),
                items, sizeof(Item) * 10);
  } else if (giftScore >= 5000ul) {
    g_ui.view.peer.giftMessageId = MESSAGE_PEER_PLAY_THREE_NOTES;
    if (g_ui.view.peer.wattsAwarded != 0) {
      return;
    }
    if (g_state.dailySteps > peer->dailySteps) {
      g_ui.view.peer.itemIndex = 4;
    } else {
      g_ui.view.peer.itemIndex = 5;
    }
    items[freeSlot].idLe = CourseLoadItemIdLe(g_ui.view.peer.itemIndex);
    EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, friendItems),
                items, sizeof(Item) * 10);
  } else if (giftScore >= 2500ul) {
    g_ui.view.peer.giftMessageId = MESSAGE_PEER_PLAY_TWO_NOTES;
    if (g_ui.view.peer.wattsAwarded != 0) {
      return;
    }
    if (g_state.dailySteps > peer->dailySteps) {
      g_ui.view.peer.itemIndex = 6;
    } else {
      g_ui.view.peer.itemIndex = 7;
    }
    items[freeSlot].idLe = CourseLoadItemIdLe(g_ui.view.peer.itemIndex);
    EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, friendItems),
                items, sizeof(Item) * 10);
  } else {
    g_ui.view.peer.giftMessageId = MESSAGE_PEER_PLAY_ONE_NOTE;
    if (g_ui.view.peer.wattsAwarded != 0) {
      return;
    }
    if (g_state.dailySteps > peer->dailySteps) {
      g_ui.view.peer.itemIndex = 8;
    } else {
      g_ui.view.peer.itemIndex = 9;
    }
    items[freeSlot].idLe = CourseLoadItemIdLe(g_ui.view.peer.itemIndex);
    EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, friendItems),
                items, sizeof(Item) * 10);
  }
}

/* Shift each plane's column bits within the single eight-row note image. */
void RenderPeerNote(u8 x, u8 y, u8 columnBitShift)
{
  u8 *raster;
  u8 *resourceAddress;
  u8 byteIndex;

  ScratchReset();
  raster = ScratchAlloc(sizeof(((UiResources *)0)->note));
  resourceAddress = ((UiResources *)EEPROM_UI)->note;
  EepromRead((u16)resourceAddress, raster, sizeof(((UiResources *)0)->note));
  byteIndex = 0;
  do {
    raster[byteIndex] >>= columnBitShift;
    byteIndex++;
  } while (byteIndex < sizeof(((UiResources *)0)->note));
  DisplayBlit(x, y, 8, 8, raster);
}

/* Advance phases every eight renders, including during sound playback.
 * Phase changes start sounds and return home after the reward. */
void PeerRender(void)
{
  u8 fixedFacing;
  u8 x;
  u8 elapsedFrames;
  u8 noteCount;
  u8 noteIndex;

  if (g_ui.view.peer.phase < PEER_PRESENT) {
    RenderHeldPokemon(0x38, 0x08);
    fixedFacing = g_ui.view.peer.session.bits.fixedFacing;
    if (g_ui.view.peer.phase == PEER_ENTER) {
      x = (8 - (7 - g_ui.view.peer.phaseFrame) * 3);
      if (fixedFacing != 0) {
        RenderPeerPokemon(x, 8, 0);
      } else {
        RenderPeerPokemon(x, 8, 1);
      }
    } else if (fixedFacing != 0) {
      RenderPeerPokemon(8, 8, 0);
    } else {
      RenderPeerPokemon(8, 8, 1);
    }
  }

  switch (g_ui.view.peer.phase) {
  case PEER_GREETING:
    RenderPeerName(0x02, 0x20, BORDER_TOP);
    RenderMessage(0x30, MESSAGE_PEER_GREETING,
                  BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_NO_PROMPT);
    break;
  case PEER_PLAY:
    elapsedFrames = (g_ui.view.peer.phaseFrame + 1);
    switch (g_ui.view.peer.giftMessageId) {
    case MESSAGE_PEER_PLAY_FIVE_NOTES:
      noteCount = elapsedFrames;
      if (noteCount > 5) {
        noteCount = 5;
      }
      for (noteIndex = 0; noteIndex < noteCount; noteIndex++) {
        RenderPeerNote((noteIndex * 8 + 0x1c), 0, g_peerNoteShifts[noteIndex]);
      }
      break;
    case MESSAGE_PEER_PLAY_FOUR_NOTES:
      noteCount = elapsedFrames;
      if (noteCount > 4) {
        noteCount = 4;
      }
      for (noteIndex = 0; noteIndex < noteCount; noteIndex++) {
        RenderPeerNote((noteIndex * 8 + 0x1c), 0, g_peerNoteShifts[noteIndex]);
      }
      break;
    case MESSAGE_PEER_PLAY_THREE_NOTES:
      noteCount = ((g_ui.view.peer.phaseFrame >> 1) + 1);
      if (noteCount > 3) {
        noteCount = 3;
      }
      for (noteIndex = 0; noteIndex < noteCount; noteIndex++) {
        RenderPeerNote((noteIndex * 8 + 0x24), 0,
                       g_peerNoteShifts[noteIndex + 1]);
      }
      break;
    case MESSAGE_PEER_PLAY_TWO_NOTES:
      noteCount = ((g_ui.view.peer.phaseFrame >> 1) + 1);
      if (noteCount > 2) {
        noteCount = 2;
      }
      for (noteIndex = 0; noteIndex < noteCount; noteIndex++) {
        RenderPeerNote((noteIndex * 8 + 0x24), 0,
                       g_peerNoteShifts[noteIndex + 1]);
      }
      break;
    case MESSAGE_PEER_PLAY_ONE_NOTE:
      RenderPeerNote(0x2c, 0, g_peerNoteShifts[2]);
      break;
    }
    RenderMessage(0x30, g_ui.view.peer.giftMessageId,
                  BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_NO_PROMPT);
    break;
  case PEER_PRESENT:
    RenderPresentIcon(0x20, 0x04);
    RenderMessage(0x30, MESSAGE_PEER_PRESENT,
                  BORDER_TOP | BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_NO_PROMPT);
    break;
  case PEER_REWARD:
    RenderPresentIcon(0x20, 0x04);
    if (g_ui.view.peer.wattsAwarded != 0) {
      RenderWatts(0x02, 0x20, g_ui.view.peer.wattsAwarded, 0x0d);
    } else {
      RenderCourseItem(0x00, 0x20, g_ui.view.peer.itemIndex,
                       BORDER_TOP | BORDER_LEFT | BORDER_RIGHT);
    }
    RenderMessage(0x30, MESSAGE_RECEIVED,
                  BORDER_BOTTOM | BORDER_LEFT | BORDER_RIGHT,
                  MESSAGE_NO_PROMPT);
    break;
  }

  if (++g_ui.view.peer.phaseFrame >= 8) {
    g_ui.view.peer.phase++;
    g_ui.view.peer.phaseFrame = 0;
    switch (g_ui.view.peer.phase) {
    case PEER_PLAY:
      BeepLoadScore(SCORE_PEER_PLAY);
      break;
    case PEER_REWARD:
      BeepLoadScore(SCORE_COMPLETION);
      break;
    }
  }
  if (g_ui.view.peer.phase >= PEER_COMPLETE) {
    HomeInit();
    SetView(VIEW_HOME);
  }
  RenderBattery(0, 0);
}

/* Slot 0 stages the record received by IR. Search only history slots 1..10
 * for the complete device ID, using the IR workspace's EEPROM read buffer. */
u8 SeenPeer(u8 *deviceId)
{
  PeerRecords *recordAddress;
  u8 *storedId;
  u8 historySlot;
  u8 byteIndex;
  u8 matches;

  recordAddress = (PeerRecords *)EEPROM_PEER_RECORDS + 1;
  storedId = g_work.irc.eepromScratch;
  historySlot = 0;
  while (historySlot < PEER_HISTORY_SLOTS) {
    matches = 1;
    EepromRead((u16)&recordAddress->deviceId, storedId,
               sizeof(recordAddress->deviceId));
    byteIndex = 0;
    do {
      if (storedId[byteIndex] != deviceId[byteIndex]) {
        matches = 0;
      }
      byteIndex++;
    } while (byteIndex < sizeof(recordAddress->deviceId));
    if (matches == 1) {
      return 1;
    }
    historySlot++;
    recordAddress++;
  }
  return 0;
}

/* Move history slots 9..0 to 10..1, descending so each source survives until
 * copied. The newly received slot 0 becomes the newest history entry at 1. */
void PeerStoreHistory(void)
{
  u8 *buffer;
  u16 sourceAddress;
  u16 destinationAddress;
  u8 recordsLeft;

  ScratchReset();
  buffer = ScratchAlloc(PW_PEER_SHIFT_BYTES);
  sourceAddress = PW_PEER_SHIFT_SRC;
  destinationAddress = PW_PEER_SHIFT_DST;
  recordsLeft = PEER_HISTORY_SLOTS;
  do {
    EepromRead(sourceAddress, buffer, PW_PEER_SHIFT_BYTES);
    EepromWrite(destinationAddress, buffer, PW_PEER_SHIFT_BYTES);
    sourceAddress = (sourceAddress - PW_PEER_SHIFT_BYTES);
    destinationAddress = (destinationAddress - PW_PEER_SHIFT_BYTES);
  } while (--recordsLeft != 0);
}

/* Append a diary entry for item gifts using actions 1..10. DiaryAppend
 * preserves the peer fields filled here. */
void PeerAppendDiary(void)
{
  Course *course;
  PeerInfo *peer;
  DiaryEntry *diary;
  u8 i;

  ScratchReset();
  course = ScratchAlloc(sizeof(Course));
  EepromRead(EEPROM_COURSE, course, sizeof(Course));
  peer = ScratchAlloc(sizeof(PeerInfo));
  EepromRead(EEPROM_PEER_INFO, peer, sizeof(PeerInfo));
  diary = ScratchAlloc(sizeof(DiaryEntry));
  diary->compatibilityLe = peer->compatibilityLe;
  diary->gameVersionLe = peer->gameVersionLe;
  diary->encounterIdLe = peer->idLe;
  diary->peerForm = peer->form;
  diary->peerSex = peer->sex;
  diary->peerShiny = peer->shiny;
  diary->peerHourSteps = peer->hourSteps;
  diary->peerDaySteps = peer->dailySteps;
  i = 0;
  do {
    diary->peerNickname[i] = peer->nickname[i];
    i++;
  } while (i < sizeof(peer->nickname));
  i = 0;
  /* Copy the peer name plus its following two appearance bytes. DiaryAppend
   * overwrites those two destination bytes with the start of the local
   * Pokemon's nickname. */
  do {
    ((DiaryNameView *)diary)->nameSpan.bytes[i] =
        ((const PeerNameView *)peer)->nameSpan.bytes[i];
    i++;
  } while (i < PEER_DIARY_NAME_COPY_BYTES);
  if (g_ui.view.peer.wattsAwarded == 0) {
    if (g_ui.view.peer.itemIndex < 10) {
      DiaryAppend(course, diary, (g_ui.view.peer.itemIndex + 1),
                  g_state.save.bonusCourse, 0,
                  course->itemIdLe[g_ui.view.peer.itemIndex]);
    }
  }
}

/* Per-note column shifts; lower tiers use the center entries. */
const u8 g_peerNoteShifts[6] = {0x00, 0x01, 0x02, 0x01, 0x00, 0x00};

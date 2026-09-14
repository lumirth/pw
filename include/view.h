#ifndef PW_VIEW_H
#define PW_VIEW_H

#include "data.h"
#include "flags.h"
#include "application/pw_follower_events.h"

#define VIEW_HOME 0
#define VIEW_MAIN_MENU 1
#define VIEW_DOWSING 2
#define VIEW_RADAR 3
#define VIEW_BATTLE 4
#define VIEW_RADAR_FAILURE 6
#define VIEW_DISCARD 7
#define VIEW_TRAINER 8
#define VIEW_SETTINGS 9
#define VIEW_WALK_INVENTORY 10
#define VIEW_FRIEND_ITEMS 11
#define VIEW_SOCIAL 12
#define VIEW_PEER 13
#define VIEW_IR_RESULT 14
#define VIEW_WALK_START 15
#define VIEW_WALK_END 16
#define VIEW_EVENT_REWARD 17
#define VIEW_DIAGNOSTICS 22
#define VIEW_THRESHOLD_TEST 23
#define VIEW_THRESHOLD_FAILURE 24

/* The active view selects the interpretation of this shared 18-byte UI bank.
 * Word fields require two-byte alignment. */

/* The first byte becomes DiscardView.sourceSelection when the item inventory is
 * full. */
typedef struct {
  u8 itemIndex; /* Course index 0..9; 10 selects the bonus item. */
  u8 phase;
  u8 revealFramesLeft; /* Decremented by the renderer. */
  u8 selectedPatch;
  u8 attemptsLeft;
  u8 hiddenPatch;
  u8 emptyPatch; /* Failed first patch, or 0xff before any miss. */
  u8 wattsAwarded;
  u8 freeItemSlot; /* Inventory slot 0..2, or 3 when full. */
  u8 unused9;
  u16 itemIdLe;
  u8 unusedTail[6];
} DowsingView;

typedef struct {
  u8 sourceSelection; /* One-based for Pokémon; zero-based for items. */
  u8 selectedSlot;
  u8 inventoryKind;
  u8 unusedTail[15];
} DiscardView;

/* Device diagnostics, including stable RTC samples and the probe result. */
typedef struct {
  u8 stage;
  u8 diagnosticsReady; /* Preserved from the IR LCD setup result. */
  u8 renderCount;
  u8 rtcFirstSample;
  u8 rtcSecondSample;
  u8 probeResult;
  u16 batteryReference;
  u8 unusedTail[10];
} DiagnosticsView;

typedef struct {
  u16 resetWord;
  u8 walkingBatches;
  u8 stillBatches;
  u16 xActivity;
  u16 yActivity;
  u16 zActivity;
  MotionThresholds thresholds;
} ThresholdView;

/* List entry indices and native occupancy words; see list.h. */
typedef struct {
  u8 entryIndex;
  u8 unused1;
  u16 walkEntryMask;
  u16 friendItemMask;
  u8 unusedTail[12];
} InventoryView;

typedef struct {
  IrRoleFlags role;
  /* Carried into DiagnosticsView byte 1; its initializer preserves it. */
  u8 diagnosticsReady;
  u8 unusedTail[16];
} IrView;

typedef struct {
  u8 pendingEventId;
  u8 eventOfferCountdown; /* Remaining HomeUpdate calls. */
  u8 pokemonX; /* Pixel coordinate, including offscreen entry motion. */
  HomeMotionFlags animationControl;
  u8 unusedTail[14];
} HomeView;

typedef struct {
  /* Shared with BattleView.encounter; the watt result panel reuses this byte.
   */
  u8 encounter;
  u8 phase;
  u8 selectedPatch;
  u8 round; /* Zero-based successful-patch round. */
  u8 roundCount;
  u8 activePatch;
  u8 delayUpdates; /* Input polls until the target bubble appears. */
  /* Response polls left, then render index during the transition to battle. */
  u8 phaseCounter;
  /* Feedback polls left; a nonzero count also keeps the marker visible. */
  u8 revealCounter;
  u8 unusedTail[9];
} RadarView;

typedef union {
  u8 byte;
  struct {
    u8 responseRow : 3;  /* Probability row chosen by the previous exchange. */
    u8 responseCode : 2; /* Interpreted together with playerAction. */
    u8 playerAction : 2;
    u8 opponentHpVisible : 1;
  } bits;
} BattleFlags;

typedef struct {
  /* Radar supplies 1..3 for course encounters, 4 for the bonus. */
  u8 encounter;
  u8 phase;
  u8 playerHp;
  u8 opponentHp;
  u8 animationFrame;
  /* Last animation frame, held until the phase advances. */
  u8 frameLimit;
  u8 playerX;
  u8 opponentX; /* Offscreen negative positions wrap in this byte. */
  u16 wattsLost;
  BattleFlags flags;
  u8 capturePasses; /* Three successful shake checks complete a capture. */
} BattleView;

typedef struct {
  PeerFlags session;
  u8 phase;
  u8 phaseFrame;    /* Eight render calls per peer presentation phase. */
  u8 giftMessageId; /* 0x2c..0x30 select both gift tier and message. */
  u8 itemIndex;
  u8 wattsAwarded;
  u8 unusedTail[12];
} PeerView;

typedef struct {
  u8 historyDaysAgo; /* 0 is the current card; 1..7 index past days. */
  u8 page;           /* TRAINER_PAGE_* */
  u8 unusedTail[16];
} TrainerView;

typedef struct {
  u8 page;      /* SETTINGS_PAGE_* */
  u8 selection; /* SETTINGS_SELECT_* */
  u8 unusedTail[16];
} SettingsView;

typedef struct {
  u8 eventId;
  u8 rewardValue; /* Course item index or watt amount, selected by eventId. */
  u8 messageVariant;
  u8 unused3;
  const SocialFrame
      *sequenceFrame; /* Resident sequence, advanced by center presses. */
} SocialView;

/* Arrival, departure and event-reward views use distinct stage encodings. */
typedef struct {
  u8 stage;
  u8 frame; /* Render calls within the current stage. */
  u8 rewardKind;
  u8 unusedTail[15];
} PresentationView;

typedef struct {
  u8 error; /* MENU_ERROR_* */
  u8 unusedTail[17];
} MenuView;

typedef union {
  u8 raw[18];
  PresentationView presentation;
  MenuView menu;
  DiscardView discard;
  DowsingView dowsing;
  InventoryView inventory;
  u16 words[9];
  HomeView home;
  DiagnosticsView diag;
  ThresholdView accel;
  IrView ir;
  RadarView radar;
  BattleView battle;
  PeerView peer;
  TrainerView trainer;
  volatile SettingsView settings;
  SocialView social;
} ViewState;

#define PW_DISPLAY_BAND_6_ORIGIN_Y 0x30

#endif

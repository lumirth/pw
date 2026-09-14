#ifndef PW_GLOBALS_H
#define PW_GLOBALS_H

#include "flags.h"

#include "data.h"
#include "view.h"

#include <stddef.h>
#include "support/ir_state.h"

#define IDLE_DISPLAY 0
#define IDLE_MOTION 1

/* Interrupt counters and latches feed the foreground task. The scratch cursor
 * holds the next allocation's RAM address. */

typedef struct {
  u8 secondBcd;
  u8 minuteBcd;
  u8 hourBcd24h;
  u8 pendingUpdates;
} RtcTimeShadow;

/* A bin position is the accepted steps per 64-sample batch, scaled by 512.
 * The fractional step survives rejected batches until MotionReset. */
typedef struct {
  u32 stepFractionQ9;
  /* Applied as immediate credit; normal operation keeps this value zero. */
  u32 pendingStepsQ9;
  u8 resetOnlyByte;
  u8 lastRejected;
} MotionBatch;

/* Spread the whole-step budget across later sample ticks. The phase starts
 * at 32, adds batchSteps per tick, and emits on >64 before subtracting 64. */
typedef struct {
  u8 batchSteps;
  u8 stepsEmitted;
  u8 stepPhase;
} MotionStepPacing;

typedef struct {
  SaveData save;
  volatile u8 buttons;
  volatile u8 previousButtons;
  volatile u8 pressedButtons;
  volatile u8 centerHoldScans; /* InputScan calls after a center wake. */
  volatile u32 dailySteps;
  volatile u16 hourSteps;
  volatile u16 socialElapsedSeconds;
  volatile RtcTimeShadow time;
  u8 rolloverHourBcd;
  u8 baseContrast;
  volatile u8 menuSelection;
  /* Incremented after input dispatch; retained without a consumer. */
  u8 viewUpdates;
  volatile u8 uiFrame;
  volatile u8 irResult; /* IR_RESULT_* */
  volatile u8 sampleIndex;
  volatile u8 idleSeconds[2]; /* Display and regular-motion countdowns. */
  volatile u8 view;
  MotionStepPacing stepPacing;
  SystemEvents events;
  volatile SystemFlags flags;
  volatile u16 irTimerReference; /* Timer W counts. */
  u8 irReceivedBytes;
  volatile u8 buttonWake[3];
  u16 scratchCursor;
  u32 randomState;
} RuntimeState;

/* Sound output and interrupt timing accompany the active view. */
typedef struct {
  u8 outputMode;
  /* Remaining Timer W compare-A interrupts. */
  volatile u16 periodsRemaining;
  volatile u16 separatorPeriodsRemaining;
  /* The worker samples tempo separately in each duration arm. */
  volatile u8 durationDivisor;
  ViewState view;
} PresentationState;

/* One workspace is reused by motion, sound, and infrared communication. Motion
 * owns the first 256 bytes, its following 10-byte batch state, and scratch
 * storage. Infrared communication uses two status records, session state, a
 * complete 136-byte receive frame, and a 128-byte decompression buffer. The
 * receive frame overlaps both motion batch state and scratch storage. A task
 * handoff must finish using the old view before reinitializing the new owner.
 */
typedef union {
  DeviceStatus status;
  u8 bytes[sizeof(DeviceStatus)];
} StatusBuffer;

typedef union {
  u8 bytes[0x600];
  u16 words[0x300];
  struct {
    u8 scratch[0x400];
    u8 largeTransferTail[0x200];
  } layout;
} ScratchStorage;

typedef union {
  struct {
    u16 fftAccumulator[32];
    /* High bytes of the BMA150's signed 10-bit axis samples; low two bits are
     * omitted. Power and diagnostic delta expressions re-read each operand. */
    volatile s8 x[64];
    volatile s8 y[64];
    volatile s8 z[64];
    MotionBatch batch;
    ScratchStorage scratch;
  } motion;
  struct {
    u8 reservedBeforeScore[64];
    /* Shares all 192 bytes with x/y/z. Audio owns the foreground while this
     * score plays; the main task restarts sampling at index zero afterward. */
    Note score[96];
  } beeper;
  struct {
    StatusBuffer statusA;
    StatusBuffer statusB;
    IrcWork work;
    u8 packet[IR_HEADER_BYTES + IR_PAYLOAD_BYTES];
    u8 eepromScratch[EEPROM_PAGE_BYTES];
  } irc;
} Workspace;

typedef char StatusBufferMustBe104Bytes[sizeof(StatusBuffer) == 104 ? 1 : -1];
typedef char BeeperScoreMustStartAfter64Bytes
    [offsetof(Workspace, beeper.score) == 64 ? 1 : -1];
typedef char WorkspaceMustBe0x70ABytes[sizeof(Workspace) == 0x70A ? 1 : -1];

typedef union {
  u16 word;
  struct {
    u8 index;
    u8 reserved;
  } bytes;
} DisplayBank;

/* The allocator checks a 0x400-byte window. Large Pokemon artwork transfers
 * directly reuse all 0x600 bytes of ScratchStorage from its base. */
typedef char RuntimeStateMustBe0x44Bytes[sizeof(RuntimeState) == 0x44 ? 1 : -1];
typedef char
    ScratchStorageMustBe0x600Bytes[sizeof(ScratchStorage) == 0x600 ? 1 : -1];
typedef char WorkspacePacketMustFollowSession
    [offsetof(Workspace, irc.packet) == 232 ? 1 : -1];
typedef char WorkspaceDecodeMustFollowPacket
    [offsetof(Workspace, irc.eepromScratch) == 368 ? 1 : -1];
typedef char WorkspaceScratchMustFollowMotion
    [offsetof(Workspace, motion.scratch) == 266 ? 1 : -1];

extern RuntimeState g_state;
extern const Note *g_note;
extern PresentationState g_ui;
extern void (*g_task)(void);
/* InstallTask records the replaced task here; firmware never dispatches it. */
extern void (*g_previousTask)(void);
extern DisplayBank g_displayBank;
extern Workspace g_work;

#endif

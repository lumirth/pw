#ifndef PW_FLAGS_H
#define PW_FLAGS_H

#include "types.h"

#define SYSTEM_SOCIAL_OFFER 0x01
#define SYSTEM_REGISTERED 0x02
#define SYSTEM_HAS_POKEMON 0x04
#define SYSTEM_MODE_MASK 0x18
#define SYSTEM_MODE_CLEAR 0xE7
#define SYSTEM_MODE_INACTIVE 0
#define SYSTEM_MODE_MOTION 0x08
#define SYSTEM_MODE_INTERACTIVE 0x10

/* The quarter-second interrupt latches one request; ticks can coalesce. */
#define EVENT_UI_REFRESH 0x01
#define EVENT_BATTERY_LOW 0x02
#define EVENT_BATTERY_CHECK 0x04
#define EVENT_CENTER_PRESS 0x08
#define EVENT_LOW_POWER_CLOCK 0x10
#define EVENT_IR_REQUEST 0x20
#define EVENT_EEPROM_ERROR 0x40
#define EVENT_MOTION 0x80
#define EVENT_CLEAR(event) (0xFF ^ (event))

/* CH38 allocates these byte-sized bitfields most-significant bit first. */
typedef union {
  u8 byte;
  struct {
    u8 reserved7 : 1;
    u8 reserved6 : 1;
    u8 reserved5 : 1;
    u8 interactiveMode : 1;
    u8 motionMode : 1;
    u8 hasPokemon : 1;
    u8 registered : 1;
    u8 socialOfferPending : 1;
  } bits;
} SystemFlags;

typedef char SystemFlagsMustBeOneByte[sizeof(SystemFlags) == 1 ? 1 : -1];

typedef union {
  u8 byte;
  struct {
    u8 motionDetected : 1;
    u8 eepromError : 1;
    u8 irRequested : 1;
    u8 lowPowerClock : 1;
    u8 centerPressed : 1;
    u8 batteryCheckPending : 1;
    u8 batteryLow : 1;
    u8 uiRefreshPending : 1;
  } bits;
} SystemEvents;

typedef char SystemEventsMustBeOneByte[sizeof(SystemEvents) == 1 ? 1 : -1];

typedef union {
  u8 byte;
  struct {
    u8 reserved7 : 1;
    u8 reserved6 : 1;
    u8 reserved5 : 1;
    u8 reserved4 : 1;
    u8 reserved3 : 1;
    u8 movingRight : 1;
    u8 smallFrame : 1;
    u8 entered
        : 1; /* Home entry writes this bit; its animation never reads it. */
  } bits;
} HomeMotionFlags;

typedef char
    HomeMotionFlagsMustBeOneByte[sizeof(HomeMotionFlags) == 1 ? 1 : -1];

typedef union {
  u8 byte;
  struct {
    u8 reserved7 : 1;
    u8 reserved6 : 1;
    u8 reserved5 : 1;
    u8 reserved4 : 1;
    u8 reserved3 : 1;
    u8 reserved2 : 1;
    u8 reserved1 : 1;
    u8 receivedBurst : 1; /* Any nonempty burst, before checksum validation. */
  } bits;
} IrSessionFlags;

typedef char IrSessionFlagsMustBeOneByte[sizeof(IrSessionFlags) == 1 ? 1 : -1];

typedef union {
  u8 byte;
  struct {
    u8 reserved7 : 1;
    u8 reserved6 : 1;
    u8 reserved5 : 1;
    u8 reserved4 : 1;
    u8 reserved3 : 1;
    u8 reserved2 : 1;
    u8 reserved1 : 1;
    /* Both peers transfer data; this peer answers the status request. */
    u8 peerStatusResponder : 1;
  } bits;
} IrRoleFlags;

typedef char IrRoleFlagsMustBeOneByte[sizeof(IrRoleFlags) == 1 ? 1 : -1];

typedef union {
  u8 byte;
  struct {
    u8 reserved7 : 1;
    u8 reserved6 : 1;
    u8 reserved5 : 1;
    u8 reserved4 : 1;
    u8 reserved3 : 1;
    u8 reserved2 : 1;
    u8 fixedFacing : 1;
    u8 reserved0 : 1;
  } bits;
} PeerFlags;

typedef char PeerFlagsMustBeOneByte[sizeof(PeerFlags) == 1 ? 1 : -1];

#endif

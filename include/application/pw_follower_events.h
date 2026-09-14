#ifndef PW_FOLLOWER_EVENTS_H
#define PW_FOLLOWER_EVENTS_H

#include "types.h"

/* IDs also select the home bubble and diary action (event ID + 16).
 * Slot 6 has no sequence and is never offered by SocialOfferCheck. */
#define SOCIAL_EVENT_ITEM 1
#define SOCIAL_EVENT_WATTS_50 2
#define SOCIAL_EVENT_WATTS_20 3
#define SOCIAL_EVENT_WATTS_10 4
#define SOCIAL_EVENT_BORED 5
#define SOCIAL_EVENT_NEW_POKEMON 7

#define SOCIAL_PANEL_FIXED 1
#define SOCIAL_PANEL_VARIANT 3

#define SOCIAL_SILENT 16
#define SOCIAL_NO_BUBBLE 7
#define SOCIAL_TERMINAL 0x01
#define SOCIAL_SHOW_POKEMON 0x02
#define SOCIAL_SHOW_TREASURE 0x04
#define SOCIAL_POKEMON_NAME 0xFC
#define SOCIAL_ITEM_NAME 0xFD
#define SOCIAL_WATTS 0xFE
#define SOCIAL_NO_MESSAGE 0xFF
#define SOCIAL_FLAGS(bubbleIndex, lowerMessageMode, icons)                     \
  (((bubbleIndex) << 5) | ((lowerMessageMode) << 3) | (icons))

typedef union {
  u8 byte;
  struct {
    u8 bubbleIndex : 3;
    u8 lowerMessageMode : 2;
    u8 showTreasure : 1;
    u8 showPokemon : 1;
    u8 terminal : 1;
  } bits;
} SocialFlags;

/* Resident sequence records: upperContent is a message ID or a special
 * content selector. Variant lower messages add messageVariant (0..2). */
typedef struct {
  SocialFlags flags;
  u8 scoreId;
  u8 upperContent;
  u8 lowerMessage;
} SocialFrame;

/* Accept one offered event ID (1..5 or 7), apply its persistent reward, append
 * a diary entry and start its display sequence. Resets the scratch arena. */
void SocialApply(u8 eventId);

#endif

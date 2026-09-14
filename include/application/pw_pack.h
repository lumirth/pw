#ifndef PW_PACK_H
#define PW_PACK_H

#include "types.h"

/* The walk mask indexes these display entries. Captured Pokemon and course
 * items each occupy three consecutive entries. */
#define WALK_ENTRY_HELD_POKEMON 0
#define WALK_ENTRY_CAUGHT_FIRST 1
#define WALK_ENTRY_CAUGHT_SECOND 2
#define WALK_ENTRY_CAUGHT_THIRD 3
#define WALK_ENTRY_EVENT_POKEMON 4
#define WALK_ENTRY_EVENT_MAP 5
#define WALK_ENTRY_ITEM_FIRST 6
#define WALK_ENTRY_ITEM_SECOND 7
#define WALK_ENTRY_ITEM_THIRD 8
#define WALK_ENTRY_EVENT_ITEM 9
#define INVENTORY_LAST_ENTRY 9
#define INVENTORY_ENTRY_COUNT 10

#define WALK_MASK_HELD_POKEMON 1
#define WALK_MASK_CAUGHT_FIRST 2
#define WALK_MASK_EVENT_POKEMON 0x10
#define WALK_MASK_EVENT_MAP 0x20
#define WALK_MASK_ITEM_FIRST 0x40
#define WALK_MASK_EVENT_ITEM 0x200
#define WALK_MASK_EVENT_ITEM_HIGH_BYTE 2

#define INVENTORY_WALK_MASK 0
#define INVENTORY_FRIEND_MASK 1

/* Write two native u16 masks to a word-aligned four-byte buffer: walk entries,
 * then friendItems[0..9]. Resets scratch and reads the inventory from EEPROM.
 */
void InventoryMasks(u8 *entryMasksOut);
void WalkInventorySelectFirst(void);
void WalkInventoryUpdate(void);
void WalkInventoryRender(void);
void FriendItemSelectFirst(void);
void FriendItemInventoryUpdate(void);
void FriendItemInventoryRender(void);

#endif

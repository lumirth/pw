#ifndef PW_EEPROM_MAP_H
#define PW_EEPROM_MAP_H

#include "resources.h"

#define EEPROM_PAGE_BYTES 128
#define EEPROM_PAGE_OFFSET_MASK (EEPROM_PAGE_BYTES - 1)
#define EEPROM_CAPACITY 0x10000
#define EEPROM_SIGNATURE_BYTES 8
#define EEPROM_COUNTERS 0x0008
#define EEPROM_DIAGNOSTIC_LOG 0x0070

/* Fixed serial-storage addresses used by the firmware. Each mirrored payload
 * is followed by one checksum byte. */
#define EEPROM_BATTERY_PRIMARY 0x0080
#define EEPROM_ID_PRIMARY 0x0083
#define EEPROM_LCD_PRIMARY 0x00AC
#define EEPROM_STATUS_PRIMARY 0x00ED
#define EEPROM_SAVE_PRIMARY 0x0156
#define EEPROM_COMMIT_PRIMARY 0x016F
#define EEPROM_BATTERY_BACKUP 0x0180
#define EEPROM_ID_BACKUP 0x0183
#define EEPROM_LCD_BACKUP 0x01AC
#define EEPROM_STATUS_BACKUP 0x01ED
#define EEPROM_SAVE_BACKUP 0x0256
#define EEPROM_COMMIT_BACKUP 0x026F

/* Startup repeats the staged course/record copy while this marker is set. */
#define WALK_COMMIT_PENDING 0xa5

#define EEPROM_UI 0x0280
#define EEPROM_SOUND_DIRECTORY 0x8CB0
#define EEPROM_SOUND_DATA 0x8CF0
#define EEPROM_COURSE 0x8F00
#define EEPROM_EVENTS 0xB800
#define EEPROM_EVENT_BYTES 0x06c8
#define EEPROM_EVENT_MAP 0xB804
#define EEPROM_EVENT_POKEMON 0xBA44
#define EEPROM_EVENT_ITEM 0xBD40
#define EEPROM_BONUS_COURSE 0xBF00
#define EEPROM_OWN_RECORDS 0xCC00
#define EEPROM_WALK 0xCE80
#define EEPROM_PEER_RECORDS 0xDC00
#define EEPROM_PEER_IMAGE 0xF400
/* Received small animation, rendered name, then peer step/identity data. */
#define EEPROM_PEER_NAME (EEPROM_PEER_IMAGE + POKEMON_ANIMATION_BYTES)
#define EEPROM_PEER_INFO (EEPROM_PEER_NAME + TEXT_RASTER_BYTES)

/* Staged transfers overlap active storage and are committed page by page. */
#define EEPROM_COURSE_TEMP 0xD700
#define EEPROM_RECORD_TEMP 0xD480

#endif

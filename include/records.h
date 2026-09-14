#ifndef PW_RECORDS_H
#define PW_RECORDS_H

#include <stddef.h>
#include "types.h"

#define DEVICE_ID_BYTES 40
#define TRAINER_NAME_BYTES 16
#define STATUS_NAME_BYTES 18
#define STATUS_RECEIPT_BYTES 16
#define STATUS_FLAGS_OFFSET 91
#define POKEMON_METADATA_BYTES 44
#define POKEMON_NAME_BYTES 22
#define COURSE_NAME_BYTES 42
#define INVENTORY_SLOTS 3
#define FRIEND_ITEM_SLOTS 10
#define PEER_HISTORY_SLOTS 10
#define DIARY_ENTRIES 24
#define STEP_HISTORY_DAYS 7
#define COURSE_ENCOUNTERS 3
#define COURSE_ITEMS 10
#define TRAINER_HOUSE_TRANSFER_BYTES 500

/* Records exchanged with the console allocate the low bits first. Multi-byte
 * fields retain the representation used by their individual consumers. */
#pragma bit_order right

typedef struct {
  /* Forwarded in their serialized little-endian representation. */
  u32 consoleCompatibilityLe;
  u32 pokemonCompatibilityLe;
  u16 gameVersionLe;
  u16 pokemonGameVersionLe;
  u32 trainerIdLe;
  u8 deviceId[DEVICE_ID_BYTES];
  u8 receivedEvents[STATUS_RECEIPT_BYTES];
  /* The console uses the first 16 bytes for encoded text. Commit and reset
   * operate on all 18 bytes, including the two preserved trailing bytes. */
  u8 trainerNameData[STATUS_NAME_BYTES];
  u8 receiptIndex;
  u8 registered : 1;
  u8 hasPokemon : 1;
  u8 generatedPokemon : 1;
  u8 rolloverHour : 5;
  /* Peers require equal protocol bytes. The console accepts protocol levels
   * no greater than its own. */
  u8 peerProtocol;
  u8 consoleProtocolLevel;
  /* The first firmware-identification byte gates peer compatibility. */
  u8 firmwareCompatibility;
  u8 firmwareRevision;
  u32 rtcSeconds;
  u32 totalSteps;
} DeviceStatus;

typedef char DeviceStatusSize[sizeof(DeviceStatus) == 104 ? 1 : -1];
typedef char DeviceStatusId[offsetof(DeviceStatus, deviceId) == 16 ? 1 : -1];
typedef char
    DeviceStatusReceipts[offsetof(DeviceStatus, receivedEvents) == 56 ? 1 : -1];
typedef char
    DeviceStatusName[offsetof(DeviceStatus, trainerNameData) == 72 ? 1 : -1];
typedef char
    DeviceStatusIndex[offsetof(DeviceStatus, receiptIndex) == 90 ? 1 : -1];
typedef char
    DeviceStatusProtocol[offsetof(DeviceStatus, peerProtocol) == 92 ? 1 : -1];
typedef char DeviceStatusFirmware
    [offsetof(DeviceStatus, firmwareCompatibility) == 94 ? 1 : -1];
typedef char
    DeviceStatusTime[offsetof(DeviceStatus, rtcSeconds) == 96 ? 1 : -1];

/* Species, held item and moves retain their serialized little-endian bytes.
 * The meanings of reserved members remain unresolved. */
typedef struct {
  u16 idLe;
  u16 heldItemLe;
  u16 movesLe[4];
  u8 level;
  u8 form : 5;
  u8 sex : 2;
  u8 reservedAppearance : 1;
  u8 fixedFacing : 1;
  u8 shiny : 1;
  u8 preservedFlags : 6;
  u8 preserved0F;
} Pokemon;

typedef char PokemonSize[sizeof(Pokemon) == 16 ? 1 : -1];
typedef char PokemonMoves[offsetof(Pokemon, movesLe) == 4 ? 1 : -1];
typedef char PokemonLevel[offsetof(Pokemon, level) == 12 ? 1 : -1];
typedef char PokemonTrailingByte[offsetof(Pokemon, preserved0F) == 15 ? 1 : -1];

/* The little-endian ID occupies the first two bytes. Reward paths replace it
 * while carrying the trailing bytes through inventory writes. Their meaning
 * is unresolved; the item import and display paths consume the ID. */
typedef struct {
  u16 idLe;
  u8 preserved[2];
} Item;

typedef struct {
  Pokemon pokemon;
  u8 nickname[POKEMON_NAME_BYTES];
  u8 friendship;
  u8 journalTheme;
  u8 courseNameText[COURSE_NAME_BYTES];
  Pokemon encounters[COURSE_ENCOUNTERS];
  u16 encounterStepsLe[COURSE_ENCOUNTERS]; /* Little-endian step gates. */
  u8 encounterChance[COURSE_ENCOUNTERS];
  u8 padding[1]; /* Retained by whole-course updates. */
  u16 itemIdLe[COURSE_ITEMS];
  u16 itemStepsLe[COURSE_ITEMS]; /* Little-endian step gates. */
  u8 itemChance[COURSE_ITEMS];
} Course;

/* The console interprets this record when importing an event Pokemon. The
 * Walker transports it whole, retaining integers as little-endian bytes. */
typedef struct {
  u8 preserved00[4];
  u8 trainerIdLe[4];
  u8 preserved08[2];
  u8 metLocationLe[2];
  u8 preserved0C[2];
  u8 trainerName[TRAINER_NAME_BYTES];
  /* Bit 0 is the trainer gender; the other bits are preserved. */
  u8 trainerFlags;
  u8 ability;
  u8 ballItemLe[2];
  u8 preserved22[10];
} PokemonMetadata;

typedef char
    PokemonMetadataSize[sizeof(PokemonMetadata) == POKEMON_METADATA_BYTES ? 1
                                                                          : -1];
typedef char
    PokemonMetadataId[offsetof(PokemonMetadata, trainerIdLe) == 4 ? 1 : -1];
typedef char
    PokemonMetadataMet[offsetof(PokemonMetadata, metLocationLe) == 10 ? 1 : -1];
typedef char
    PokemonMetadataName[offsetof(PokemonMetadata, trainerName) == 14 ? 1 : -1];
typedef char PokemonMetadataFlags[offsetof(PokemonMetadata, trainerFlags) == 30
                                      ? 1
                                      : -1];
typedef char
    PokemonMetadataAbility[offsetof(PokemonMetadata, ability) == 31 ? 1 : -1];
typedef char
    PokemonMetadataBall[offsetof(PokemonMetadata, ballItemLe) == 32 ? 1 : -1];

typedef struct {
  u16 stepsLe;
  u8 chance;
  u8 preserved;
} EncounterRule;

/* The bonus-item transfer preserves these six bytes through native 32-bit
 * and 16-bit copies. Their meaning remains unresolved. */
typedef union {
  u8 bytes[6];
  struct {
    u32 word;
    u16 halfword;
  } transfer;
} ItemPrefix;

typedef char ItemPrefixSize[sizeof(ItemPrefix) == 6 ? 1 : -1];

typedef struct {
  ItemPrefix itemPrefix;
  u8 journalTheme;
  u8 preserved07;
  Pokemon pokemon;
  PokemonMetadata metadata;
  EncounterRule encounterRule;
  u16 itemIdLe;
  u16 itemStepsLe;
  u8 itemChance;
  u8 preserved4D[3];
  u8 courseNameText[COURSE_NAME_BYTES];
  u8 pokemonReceipt;
  u8 itemReceipt;
} BonusCourse;

typedef char BonusCourseSize[sizeof(BonusCourse) == 124 ? 1 : -1];
typedef char BonusCoursePokemon[offsetof(BonusCourse, pokemon) == 8 ? 1 : -1];
typedef char
    BonusCourseEncounter[offsetof(BonusCourse, encounterRule) == 68 ? 1 : -1];
typedef char BonusCourseItem[offsetof(BonusCourse, itemIdLe) == 72 ? 1 : -1];
typedef char
    BonusCourseName[offsetof(BonusCourse, courseNameText) == 80 ? 1 : -1];

typedef struct {
  u32 rtcSeconds;
  u32 compatibilityLe;
  u16 gameVersionLe;
  u16 pokemonIdLe;
  u16 encounterIdLe;
  u16 itemIdLe;
  u8 trainerName[TRAINER_NAME_BYTES];
  u8 nickname[POKEMON_NAME_BYTES];
  u8 peerNickname[POKEMON_NAME_BYTES];
  u8 courseNameText[COURSE_NAME_BYTES];
  u8 journalTheme;
  u8 friendship;
  u16 ownHourSteps;
  u16 peerHourSteps;
  u32 ownDaySteps;
  u32 peerDaySteps;
  u8 action;
  u8 ownForm : 5;
  u8 ownSex : 2;
  u8 ownShiny : 1;
  u8 peerForm : 5;
  u8 peerSex : 2;
  u8 peerShiny : 1;
  /* Local entries clear this byte; peer entries retain its scratch contents. */
  u8 preservedTail[1];
} DiaryEntry;

/* Presence byte at EEPROM_EVENTS. Stamp bits also accompany event rewards. */
#define EVENT_PRESENT_STAMP0 1
#define EVENT_PRESENT_STAMP1 2
#define EVENT_PRESENT_STAMP2 4
#define EVENT_PRESENT_STAMP3 8
#define EVENT_PRESENT_STAMPS 0x0f
#define EVENT_PRESENT_MAP 0x10
#define EVENT_PRESENT_POKEMON 0x20
#define EVENT_PRESENT_ITEM 0x40
#define EVENT_PRESENT_COURSE 0x80

#define STEP_CAP_REWARD_UNLOCKED 0x01
#define WALK_REWARD_FLAGS_OFFSET 8

/* The two leading words survive normal stroll transactions. Acknowledging
 * the step cap sets the following reward flag. */
typedef struct {
  u32 opaqueWords[2];
  u8 stepCapRewardUnlocked : 1;
  u8 preservedFlags : 7;
  u8 preserved09;
  u16 watts;
  Pokemon pokemon[INVENTORY_SLOTS];
  Item items[INVENTORY_SLOTS];
  Item friendItems[FRIEND_ITEM_SLOTS];
  u32 dailySteps[STEP_HISTORY_DAYS];
  DiaryEntry diary[DIARY_ENTRIES]; /* Device append rotates only slots 0..22. */
} WalkData;

typedef char WalkRewardFlagsFollowPrefix
    [offsetof(WalkData, preserved09) == WALK_REWARD_FLAGS_OFFSET + 1 ? 1 : -1];
typedef char WalkWattsOffset[offsetof(WalkData, watts) == 10 ? 1 : -1];

/* Peer step counters use native H8 order; compatibility/version fields are
 * forwarded in their serialized little-endian representation. */
typedef struct {
  u32 dailySteps;
  u16 hourSteps;
  u16 preserved06; /* Peer construction retains these packet-buffer bytes. */
  u32 compatibilityLe;
  u16 gameVersionLe;
  u16 idLe;
  u8 nickname[POKEMON_NAME_BYTES];
  u8 trainerName[TRAINER_NAME_BYTES];
  u8 form : 5;
  u8 sex : 2;
  u8 shiny : 1;
  u8 fixedFacing : 1;
} PeerInfo;

/* EEPROM_PEER_RECORDS slot 0 stages the received peer. Slots 1..10 retain
 * history, newest first; the local outgoing record lives at EEPROM_OWN_RECORDS.
 */
typedef struct {
  u8 preserved00[4];
  u16 gameVersionLe;
  u8 preserved06[2];
  u8 deviceId[DEVICE_ID_BYTES];
  u8 trainerHouseData[TRAINER_HOUSE_TRANSFER_BYTES];
} PeerRecords;

typedef char PeerRecordsSize[sizeof(PeerRecords) == 548 ? 1 : -1];
typedef char PeerRecordsId[offsetof(PeerRecords, deviceId) == 8 ? 1 : -1];
typedef char
    PeerRecordsBody[offsetof(PeerRecords, trainerHouseData) == 48 ? 1 : -1];

/* Each activity value sums 64 absolute adjacent changes in signed eight-bit
 * axis samples, including the preceding ring sample. A batch accepted as
 * walking must keep all axes in the inclusive range; stillness requires all
 * axes below the still limit. */
typedef struct {
  u8 walkingBatchTarget;
  u8 stillBatchTarget;
  u16 walkingActivityMin;
  u16 walkingActivityMax;
  u16 stillActivityLimit;
} MotionThresholds;

typedef struct {
  u8 deviceId[DEVICE_ID_BYTES];
  u8 lcdParameters[64];
  MotionThresholds thresholds;
  u8 mode;
  /* Transfer bytes following the last field consumed by setup. */
  u8 ignoredTail[3];
} FactoryData;

#pragma bit_order left

#endif

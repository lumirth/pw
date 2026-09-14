#ifndef PW_RESOURCES_H
#define PW_RESOURCES_H

#include "records.h"
#include "raster_column.h"

/* Each 8-row page stores two bit planes per column. These layouts calculate
 * serial EEPROM addresses for the storage driver. */
#define TEXT_RASTER_BYTES RASTER_BYTES(80, 16)
#define MESSAGE_RASTER_BYTES RASTER_BYTES(96, 16)
#define MESSAGE_COUNT 68
#define NUMBER_GLYPH_BYTES RASTER_BYTES(8, 16)
#define DECIMAL_GLYPHS_BYTES (10 * NUMBER_GLYPH_BYTES)
#define CURSOR_FRAME_BYTES RASTER_BYTES(8, 8)
#define CURSOR_VARIANTS 3
#define CURSOR_OFFSET(direction, variant)                                      \
  (((variant) + (direction) * CURSOR_VARIANTS) * CURSOR_FRAME_BYTES)
#define NAVIGATION_ICON_BYTES RASTER_BYTES(8, 16)
#define SOUND_LEVEL_RASTER_BYTES RASTER_BYTES(24, 16)

enum { CURSOR_UP, CURSOR_DOWN, CURSOR_LEFT, CURSOR_RIGHT };
enum { CURSOR_FILLED, CURSOR_INSET, CURSOR_OUTLINE };
#define POKEMON_FRAME_BYTES RASTER_BYTES(32, 24)
#define POKEMON_ANIMATION_BYTES (2 * POKEMON_FRAME_BYTES)
#define POKEMON_LARGE_FRAME_BYTES RASTER_BYTES(64, 48)
#define POKEMON_LARGE_ANIMATION_BYTES (2 * POKEMON_LARGE_FRAME_BYTES)

/* Three encounter-strength bubbles and the target-reveal marker share one
 * 256-byte EEPROM read. */
typedef struct {
  u8 bubbles[3 * RASTER_BYTES(16, 16)];
  u8 revealMarker[RASTER_BYTES(16, 16)];
} RadarIndicators;

typedef struct {
  u8 digits[DECIMAL_GLYPHS_BYTES];
  u8 colon[NUMBER_GLYPH_BYTES];
  u8 hyphen[NUMBER_GLYPH_BYTES];
  u8 slash[NUMBER_GLYPH_BYTES];
  u8 watts[RASTER_BYTES(16, 16)];
  u8 ball[RASTER_BYTES(8, 8)];
  u8 eventBall[RASTER_BYTES(8, 8)];
  u8 ballMask[8];
  u8 treasure[RASTER_BYTES(8, 8)];
  u8 eventTreasure[RASTER_BYTES(8, 8)];
  u8 eventMap[RASTER_BYTES(8, 8)];
  u8 stamps[4 * RASTER_BYTES(8, 8)];
  u8 cursorArrows[4 * CURSOR_VARIANTS * CURSOR_FRAME_BYTES];
  u8 leftArrow[NAVIGATION_ICON_BYTES];
  u8 rightArrow[NAVIGATION_ICON_BYTES];
  u8 returnArrow[NAVIGATION_ICON_BYTES];
  u8 preservedAfterNavigation[32];
  u8 next[RASTER_BYTES(8, 8)];
  u8 nextMask[8];
  u8 present[RASTER_BYTES(8, 8)];
  u8 battery[RASTER_BYTES(8, 8)];
  u8 feelings[7 * RASTER_BYTES(24, 16)];
  u8 menuLabels[6 * TEXT_RASTER_BYTES];
  u8 menuIcons[6 * RASTER_BYTES(16, 16)];
  u8 trainerIcon[RASTER_BYTES(16, 16)];
  u8 trainerName[TEXT_RASTER_BYTES];
  u8 courseIcon[RASTER_BYTES(16, 16)];
  u8 steps[RASTER_BYTES(40, 16)];
  u8 timeLabel[RASTER_BYTES(32, 16)];
  u8 daysLabel[RASTER_BYTES(40, 16)];
  u8 totalDays[RASTER_BYTES(64, 16)];
  u8 volume[RASTER_BYTES(40, 16)];
  u8 contrast[RASTER_BYTES(40, 16)];
  u8 soundLevels[3][SOUND_LEVEL_RASTER_BYTES];
  u8 contrastSample[RASTER_BYTES(8, 16)];
  u8 itemTreasure[RASTER_BYTES(32, 24)];
  u8 itemMap[RASTER_BYTES(32, 24)];
  u8 itemPresent[RASTER_BYTES(32, 24)];
  u8 grass[RASTER_BYTES(16, 16)];
  u8 litGrass[RASTER_BYTES(16, 16)];
  u8 attemptsLabel[RASTER_BYTES(32, 16)];
  u8 attemptsSuffix[RASTER_BYTES(24, 16)];
  u8 radarGrass[RASTER_BYTES(32, 24)];
  RadarIndicators radarIndicators;
  u8 hitEffect[RASTER_BYTES(16, 32)];
  u8 criticalHitEffect[RASTER_BYTES(16, 32)];
  u8 cloud[RASTER_BYTES(32, 24)];
  u8 hpSegment[RASTER_BYTES(8, 8)];
  u8 star[RASTER_BYTES(8, 8)];
  u8 battleMenu[RASTER_BYTES(96, 32)];
  u8 walker[RASTER_BYTES(32, 32)];
  u8 communicationIcon[RASTER_BYTES(8, 16)];
  u8 note[RASTER_BYTES(8, 8)];
  u8 transferSprite[RASTER_BYTES(8, 8)];
  u8 hoursLabel[RASTER_BYTES(40, 16)];
  u8 messages[MESSAGE_COUNT * MESSAGE_RASTER_BYTES];
  u8 confirmationPrompt[RASTER_BYTES(80, 16)];
  u8 preservedAfterConfirmation[64];
} UiResources;

typedef char
    UiNavigationStart[offsetof(UiResources, leftArrow) == 0x338 ? 1 : -1];
typedef char
    UiReturnArrow[offsetof(UiResources, returnArrow) == 0x378 ? 1 : -1];
typedef char UiTimeLabel[offsetof(UiResources, timeLabel) == 0x11F0 ? 1 : -1];
typedef char
    UiTransferSprite[offsetof(UiResources, transferSprite) == 0x2200 ? 1 : -1];
typedef char UiMessageStart[offsetof(UiResources, messages) == 0x22B0 ? 1 : -1];
typedef char UiConfirmationPrompt
    [offsetof(UiResources, confirmationPrompt) == 0x88B0 ? 1 : -1];
typedef char UiResourcesSize[sizeof(UiResources) == 0x8A30 ? 1 : -1];

typedef struct {
  Course values;
  u8 background[RASTER_BYTES(32, 24)];
  u8 courseName[TEXT_RASTER_BYTES];
  u8 pokemonImage[POKEMON_ANIMATION_BYTES];
  u8 pokemonImageLarge[POKEMON_LARGE_ANIMATION_BYTES];
  u8 pokemonName[TEXT_RASTER_BYTES];
  u8 encounterImages[COURSE_ENCOUNTERS * POKEMON_ANIMATION_BYTES];
  /* The third encounter supplies the large artwork for a joining companion. */
  u8 joiningPokemonImageLarge[POKEMON_LARGE_ANIMATION_BYTES];
  u8 encounterNames[COURSE_ENCOUNTERS * TEXT_RASTER_BYTES];
  u8 itemNames[COURSE_ITEMS * MESSAGE_RASTER_BYTES];
} CourseResources;

typedef char CourseEncounterImages
    [offsetof(CourseResources, encounterImages) == 0x0B7E ? 1 : -1];
typedef char CourseJoiningImages
    [offsetof(CourseResources, joiningPokemonImageLarge) == 0x0FFE ? 1 : -1];

#pragma bit_order right

typedef struct {
  Pokemon pokemon;
  PokemonMetadata metadata;
  u8 pokemonImage[POKEMON_ANIMATION_BYTES];
  u8 pokemonName[TEXT_RASTER_BYTES];
} EventPokemon;

typedef struct {
  ItemPrefix prefix;
  u16 itemIdLe;
} EventItemHeader;

typedef struct {
  EventItemHeader header;
  u8 itemName[MESSAGE_RASTER_BYTES];
} EventItem;

typedef char EventItemHeaderSize[sizeof(EventItemHeader) == 8 ? 1 : -1];
typedef char EventItemNumber[offsetof(EventItemHeader, itemIdLe) == 6 ? 1 : -1];
typedef char EventItemName[offsetof(EventItem, itemName) == 8 ? 1 : -1];
typedef char EventItemSize[sizeof(EventItem) == 392 ? 1 : -1];

typedef struct {
  BonusCourse values;
  u8 pokemonImage[POKEMON_ANIMATION_BYTES];
  u8 preservedAfterPokemonImage[0x600];
  u8 pokemonName[TEXT_RASTER_BYTES];
  u8 background[RASTER_BYTES(32, 24)];
  u8 courseName[TEXT_RASTER_BYTES];
  u8 itemName[MESSAGE_RASTER_BYTES];
} BonusResources;

typedef char
    BonusPokemonImage[offsetof(BonusResources, pokemonImage) == 0x7C ? 1 : -1];
typedef char
    BonusPokemonName[offsetof(BonusResources, pokemonName) == 0x7FC ? 1 : -1];
typedef char BonusResourcesSize[sizeof(BonusResources) == 0xCBC ? 1 : -1];

#pragma bit_order left

#endif

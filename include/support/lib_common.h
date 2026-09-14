#ifndef PW_LIB_COMMON_H
#define PW_LIB_COMMON_H

#include "types.h"
#include "records.h"

#define BORDER_NONE 0
#define BORDER_TOP 1
#define BORDER_BOTTOM 2
#define BORDER_LEFT 4
#define BORDER_RIGHT 8
#define MESSAGE_NO_PROMPT 0
#define MESSAGE_BLINK_PROMPT 1
#define NUMBER_NO_RULE 0
#define NUMBER_TOP_RULE 1
#define CLOCK_SLEEP_LOW_POWER 0
#define CLOCK_SLEEP_NORMAL 1

/* Artwork helpers reset scratch and load into its leading bytes. A nested
 * artwork call can replace the caller's raster; see scratch.h for reuse. */
void RenderHeldPokemon(u8 x, u8 y);
void RenderHeldPokemonMirrored(u8 x, u8 y);
void RenderEnemyPokemon(u8 x, u8 y, u8 recordIndex);
void RenderPeerPokemon(u8 x, u8 y, u8 mirrorRequested);

/* Place an 80x16 name raster at (x,y). These helpers prepare a full-width
 * frame at row 48 when y==48, or row 32 otherwise. */
void RenderHeldName(u8 x, u8 y, u8 edgeFlags);
void RenderDistributionName(u8 x, u8 y, u8 edgeFlags);
void RenderBonusName(u8 x, u8 y, u8 edgeFlags);
void RenderEnemyName(u8 x, u8 y, u8 recordIndex, u8 edgeFlags);
/* Place the peer name at (x,y) and prepare the frame at fixed row 32. */
void RenderPeerName(u8 x, u8 y, u8 edgeFlags);

/* Draw the 8x8 treasure marker. */
void RenderTreasure(u8 x, u8 y);
void RenderCourseItem(u8 x, u8 y, u8 recordIndex, u8 edgeFlags);
void RenderDistributionItem(u8 x, u8 y, u8 edgeFlags);
/* Draw a 32x24 item reward icon. */
void RenderTreasureIcon(u8 x, u8 y);
void RenderPresentIcon(u8 x, u8 y);

/* Reset scratch and draw decimal digits right-to-left, starting at (x,y).
 * topRule adds a black top row to each displayed digit. */
void RenderDecimal(u8 x, u8 y, u32 value, u8 topRule);
/* Clear a fixed frame at x=0..95, y=32..47. Place the amount's rightmost digit
 * at x+8 and its Watt icon at x+16, both at y, with a black top row. Resets
 * scratch; the final argument is ignored. */
void RenderWatts(u8 x, u8 y, u16 value, u8 unusedEdgeFlags);
/* Reset scratch and draw a 96x16 message at x=0, with selected borders and
 * an optional blinking continuation cursor. */
void RenderMessage(u8 y, u8 messageId, u8 edgeFlags, u8 blink);
/* Draw the low-battery icon during its visible blink phase, using scratch. */
void RenderBattery(u8 x, u8 y);

/* Shift by -8..8 rows, merging adjacent pages and clearing exposed rows.
 * The raster has a positive whole-page height and width at most 127. */
void RasterShift(u8 width, u8 height, s8 shift, u8 *raster);
/* Reverse columns within each complete page. The raster must be aligned for
 * u16 column access. */
void RasterMirror(u8 width, u8 height, u8 *raster);
/* Merge one source page using one preservation-mask byte per column. The
 * first-page mask placement differs between bitplanes. x/y must start inside
 * the destination; the right-hand span and spill into a following page are
 * limited to its bounds. height is ignored. */
void RasterMasked(u8 *destination, u8 destinationWidth, u8 destinationHeight,
                  const u8 *source, const u8 *mask, u8 x, u8 y, u8 width,
                  u8 height);

/* Return the first empty index in three slots, or INVENTORY_SLOTS (3) if full.
 */
u8 PokemonSlotFindEmpty(Pokemon *pokemon);
u8 ItemSlotFindEmpty(Item *item);
/* Reset scratch, load the active Course into its prefix, and select an item
 * ID in stored little-endian form. courseItemIndex is 0..COURSE_ITEMS-1. */
u16 CourseLoadItemIdLe(u8 courseItemIndex);
/* Update the capped Watt balance and write both persistent save copies. */
void WattsAdd(u8 increment);
void StepPacingTick(void);

void WatchdogDisable(void);
void WatchdogStart(void);
void ClockSleep(uint mode);
void WatchdogService(void);
void LowClockDelay(void);
void RandomSeed(u32 seed);
u32 RandomNext(void);
/* Decode a block with a four-byte header and output length in header byte 1.
 * The caller supplies that much output space and a complete encoded stream;
 * each backreference must address earlier output and fit the remaining count.
 * Other header bytes and the low nibble of a run's first byte are ignored. */
void BulkDecode(u8 *packed, u8 *out);

#endif

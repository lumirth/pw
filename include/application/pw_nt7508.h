#ifndef PW_NT7508_H
#define PW_NT7508_H

#include "types.h"
#include "raster_column.h"

/* Coordinates are pixels except the explicit page in DisplayAddress.
 * Pixel values below assume the normal (non-inverted) panel mode. */
#define PIXEL_WHITE 0
#define PIXEL_LIGHT_GRAY 1
#define PIXEL_DARK_GRAY 2
#define PIXEL_BLACK 3

/* OR complete source pages in RAM. The caller provides valid destination
 * bounds, including the extra page touched even for page-aligned y;
 * destinationHeight is ignored. */
void RasterOr(u8 width, u8 height, const u8 *source, u8 x, u8 y,
              u8 *destination, u8 destinationWidth, u8 destinationHeight);
/* Reads repairing mirrored storage and resets the scratch arena. */
void DisplayInit(void);
void DisplaySetContrast(u8 contrastDelta);
/* Caller holds chip select; page is 0..7 within the drawing bank. */
void DisplayAddress(u8 x, u8 page);
/* Display the drawing bank, then make the other bank the drawing target. */
void DisplayToggleBank(void);
/* For bank 0 or 1, display that bank and draw to its opposite. Other values
 * leave the bank selection intact. */
void DisplaySelectBank(u8 bank);
void DisplayFill(u8 pixelValue);
/* Whole-page fill: callers supply page-aligned y/height and valid extents. */
void DisplayFillRect(u8 x, u8 y, u8 width, u8 height, u8 fillPattern);
void DisplayExitPowerSave(void);
void DisplayEnterPowerSave(void);
/* Clear floor(height/8) pages, starting at page zero. */
void DisplayClear(u8 height);
/* Transfer whole 8-row source pages. All width columns are sent, including
 * columns beyond the visible right edge. y is decoded as signed 8-bit before
 * page arithmetic; callers keep y in 0..63 and y+height at most 64. */
void DisplayBlit(u8 x, u8 y, u8 width, u8 height, const u8 *raster);
/* Skip columns left of the display and complete pages above it, and limit
 * writes at the bottom. The caller must keep the right edge within bounds.
 * Negative fractional pages truncate toward zero. */
void DisplayWriteSpan(s8 x, s8 y, uint columnCount, uint rowCount,
                      RasterColumn *raster);
/* Page-aligned text: space, 0..9 and A..Z; four columns per character. */
void DisplayText(u8 x, u8 y, const char *glyphStream);
/* Draw top and sides at x=0..95, y=32..47. Clear the upper page's interior;
 * preserve the lower page's interior. */
void DisplayNameFrame(void);
/* Draw a closed frame at x=0..95, y=48..63, clearing its interior. */
void DisplayBottomNameFrame(void);
/* Draw a black row at y=48 and clear rows 49..55 across all 96 columns. */
void DisplayMessageRule(void);

#endif

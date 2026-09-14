#include "application/pw_builtin.h"
#include "types.h"
#include "raster_column.h"
#include "startup/hardware.h"
#include "project.h"
#include "application/pw_nt7508.h"
#include <machine.h>
#include "application/pw_eeprom_m95512.h"
#include "support/lib_common.h"
#include "support/scratch.h"

extern const u8 g_lcdInitScript[];

/* OR complete 8-row source pages into RAM, splitting shifted bits across two
 * destination pages. Each column reads and writes both pages, including zero
 * carry at page-aligned y. Callers supply the extra writable page;
 * destinationHeight is unused. */
void RasterOr(u8 width, u8 height, const u8 *source, u8 x, u8 y,
              u8 *destination, u8 destinationWidth, u8 destinationHeight)
{
  u8 band;
  u8 column;
  u8 first;
  u8 second;
  u8 pixel;
  u8 *row;

  height /= RASTER_PAGE_HEIGHT;
  row = destination;
  row += ((y / RASTER_PAGE_HEIGHT) * destinationWidth + x) * 2;
  for (band = 0; band < height; ++band) {
    for (column = 0; column < width; ++column) {
      first = *(source + column * 2);
      second = *(source + column * 2 + 1);
      pixel = *(row + column * 2);
      pixel = pixel | (first << (y & 7));
      *(row + column * 2) = pixel;
      pixel = *(row + column * 2 + 1);
      pixel = pixel | (second << (y & 7));
      *(row + column * 2 + 1) = pixel;
      pixel = *(row + (destinationWidth + column) * 2);
      pixel = pixel | (first >> (RASTER_PAGE_HEIGHT - (y & 7)));
      *(row + (destinationWidth + column) * 2) = pixel;
      pixel = *(row + (destinationWidth + column) * 2 + 1);
      pixel = pixel | (second >> (RASTER_PAGE_HEIGHT - (y & 7)));
      *(row + (destinationWidth + column) * 2 + 1) = pixel;
    }
    row += destinationWidth * 2;
    source += width * 2;
  }
}

/* PDR1.0 selects the panel; PDR1.1 is the caller-selected command/data line.
 * Keep chip select asserted until the complete byte has left the SSU. */
void DisplaySend(u8 txByte)
{
  IO.PDR1.BIT.B0 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = txByte;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Script delay unit: 100 calls to the conditional low-clock delay. */
void DisplayWait(void)
{
  u16 remaining;

  remaining = 100;
  do {
    LowClockDelay();
  } while (--remaining != 0);
}

#define DISPLAY_CMD_EXIT_POWER_SAVE 0xe1
#define DISPLAY_CMD_ENTER_POWER_SAVE 0xa9
#define LCD_SCRIPT_END 0xfe
#define LCD_SCRIPT_DELAY 0xfd
#define DISPLAY_CMD_NORMAL_POLARITY 0xa6
#define DISPLAY_CMD_ON 0xaf
#define DISPLAY_CMD_CONTRAST 0x81
#define DISPLAY_BANK_HEIGHT 0x40
#define DISPLAY_RASTER_BANK_1 1
#define DISPLAY_RASTER_BANK_0 0
#define LCD_SCRIPT_LENGTH 0x40

/* Reset scratch and load the 64-byte mirrored initialization script. Its first
 * byte is base contrast; 0x00/0xFF selects resident defaults. DELAY consumes a
 * count and END terminates the command stream. Clear both display RAM banks
 * before turning the panel on. */
void DisplayInit(void)
{
  u8 *script;
  u16 i;

  ScratchReset();
  script = ScratchAlloc(LCD_SCRIPT_LENGTH);
  EepromMirrorRead(EEPROM_LCD_PRIMARY, EEPROM_LCD_BACKUP, script,
                   LCD_SCRIPT_LENGTH);
  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B1 = 0;
  DisplaySend(DISPLAY_CMD_EXIT_POWER_SAVE);
  if ((*script == 0) || (*script == 0xff)) {
    script = (u8 *)g_lcdInitScript;
  }
  g_state.baseContrast = *script++;
  for (;;) {
    if (*script == LCD_SCRIPT_END) {
      break;
    }
    if (*script == LCD_SCRIPT_DELAY) {
      script++;
      for (i = 0; i < *script; i++) {
        DisplayWait();
      }
      script++;
    }
    /* The command immediately after a delay is sent in this iteration. */
    DisplaySend(*script++);
  }
  DisplaySend(DISPLAY_CMD_NORMAL_POLARITY);
  DisplaySetContrast(g_state.save.contrast);
  g_displayBank.bytes.index = DISPLAY_RASTER_BANK_1;
  DisplayClear(DISPLAY_BANK_HEIGHT);
  g_displayBank.bytes.index = DISPLAY_RASTER_BANK_0;
  DisplayClear(DISPLAY_BANK_HEIGHT);
  IO.PDR1.BIT.B1 = 0;
  DisplaySend(DISPLAY_CMD_ON);
}

/* Send the sum of the script's base contrast and the user adjustment to the
 * panel, narrowed to one byte. */
void DisplaySetContrast(u8 contrastDelta)
{
  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 0;
  DisplaySend(DISPLAY_CMD_CONTRAST);
  DisplaySend((g_state.baseContrast + contrastDelta));
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

#define DISPLAY_COL_MSB_COMMAND 0x10
#define DISPLAY_PAGE_COMMAND_BASE 0xb0
#define DISPLAY_START_LINE_COMMAND 0x40
#define DISPLAY_BANK_PAGES 8
#define DISPLAY_COL_MSB_DIVISOR 0x10
#define DISPLAY_COL_LSB_MASK 0x0f
#define DISPLAY_COL_MSB_MASK 0x07
#define DISPLAY_EDGE_INTERIOR_COUNT 0xbc
#define DISPLAY_EDGE_FILL_BIT0 1
#define DISPLAY_EDGE_FILL_BIT7 0x80
#define DISPLAY_PAGE_CMD_4 0xb4
#define DISPLAY_PAGE_CMD_5 0xb5
#define DISPLAY_PAGE_CMD_6 0xb6
#define DISPLAY_PAGE_CMD_7 0xb7
#define DISPLAY_LAST_COL_MSB 0x15
#define DISPLAY_LAST_COL_LSB 0x0f
#define DISPLAY_GLYPH_WIDTH 4
#define DISPLAY_GLYPH_CELL_BYTES 3
#define DISPLAY_RULE_PAGE_OFFSET 6
#define DISPLAY_RULE_BYTE_COUNT 0xc0

/* Select an NT7508 pixel column and an 8-row page in the drawing bank.
 * Callers hold chip select. A page above seven sleeps, then sends the page
 * command on wake. */
void DisplayAddress(u8 x, u8 page)
{
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = (((x / DISPLAY_COL_MSB_DIVISOR) & DISPLAY_COL_MSB_MASK) +
               DISPLAY_COL_MSB_COMMAND);
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = (x & DISPLAY_COL_LSB_MASK);
  if (page > (DISPLAY_BANK_PAGES - 1)) {
    sleep();
  }
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = (g_displayBank.bytes.index * DISPLAY_BANK_PAGES + page +
               DISPLAY_PAGE_COMMAND_BASE);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
}

/* Show the current raster bank and switch drawing to the other bank. */
void DisplayToggleBank(void)
{
  u8 *bank;

  bank = &g_displayBank.bytes.index;
  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = DISPLAY_START_LINE_COMMAND;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = (*bank * DISPLAY_BANK_HEIGHT);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
  *bank ^= DISPLAY_RASTER_BANK_1;
}

/* Show bank 0 or 1 and switch drawing to the opposite bank. Other values leave
 * the selection unchanged. */
void DisplaySelectBank(u8 bank)
{
  if (bank > 1) {
    return;
  }
  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = DISPLAY_START_LINE_COMMAND;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = (bank * DISPLAY_BANK_HEIGHT);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
  g_displayBank.bytes.index = bank ^ DISPLAY_RASTER_BANK_1;
}

#define DISPLAY_RASTER_COLUMN_COUNT 0x60

/* Fill all 96 columns and eight pages of the current drawing bank. Each
 * column sends the high then low plane for eight vertically adjacent pixels. */
void DisplayFill(u8 pixelValue)
{
  u8 band;
  u8 y;
  u8 remaining;

  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B0 = 0;
  band = 0;
  do {
    y = band;
    IO.PDR1.BIT.B1 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = DISPLAY_COL_MSB_COMMAND;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 0;
    if (y > (DISPLAY_BANK_PAGES - 1)) {
      sleep();
    }
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = (g_displayBank.bytes.index * DISPLAY_BANK_PAGES + y +
                 DISPLAY_PAGE_COMMAND_BASE);
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B1 = 1;
    remaining = DISPLAY_RASTER_COLUMN_COUNT;
    while (remaining != 0) {
      switch (pixelValue) {
      case PIXEL_WHITE:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0;
        break;
      case PIXEL_LIGHT_GRAY:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0xff;
        break;
      case PIXEL_DARK_GRAY:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0xff;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0;
        break;
      case PIXEL_BLACK:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0xff;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0xff;
        break;
      }
      remaining--;
    }
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    band++;
  } while (band < DISPLAY_BANK_PAGES);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Wait for SSU readiness before evaluating each column expression. */
#define LCD_WRITE(value)                                                       \
  do {                                                                         \
    while (SSU.SSSR.BIT.TDRE == 0) {                                           \
    }                                                                          \
    SSU.SSTDR = (value);                                                       \
  } while (0)
#define LCD_END()                                                              \
  do {                                                                         \
    while (SSU.SSSR.BIT.TEND == 0) {                                           \
    }                                                                          \
  } while (0)

#pragma inline(DisplaySetPosition)
static void DisplaySetPosition(u8 x, u8 page)
{
  IO.PDR1.BIT.B1 = 0;
  LCD_WRITE(((x / 16) & 7) + 0x10);
  LCD_WRITE(x & 15);
  if (page > 7) {
    sleep();
  }
  LCD_WRITE(g_displayBank.bytes.index * 8 + page + 0xb0);
  LCD_END();
}

/* Fill complete pages between the pixel endpoints y and y+height, rounding
 * both down. Callers supply page-aligned rectangles within the panel bounds. */
void DisplayFillRect(u8 x, u8 y, u8 width, u8 height, u8 fillPattern)
{
  u8 page;
  u8 col;
  int endCol;
  int endPage;

  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B0 = 0;
  page = (y >> 3);
  endCol = x + width;
  endPage = (y + height) / RASTER_PAGE_HEIGHT;
  while (page < endPage) {
    DisplaySetPosition(x, page);
    IO.PDR1.BIT.B1 = 1;
    col = x;
    while (col < endCol) {
      switch (fillPattern) {
      case PIXEL_WHITE:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0;
        break;
      case PIXEL_LIGHT_GRAY:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0xff;
        break;
      case PIXEL_DARK_GRAY:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0xff;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0;
        break;
      case PIXEL_BLACK:
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0xff;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = 0xff;
        break;
      }
      col++;
    }
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    page++;
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Release NT7508 power save while retaining the display RAM. */
void DisplayExitPowerSave(void)
{
  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 0;
  DisplaySend(DISPLAY_CMD_EXIT_POWER_SAVE);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Stop the panel oscillator and power circuits; display RAM is retained. */
void DisplayEnterPowerSave(void)
{
  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 0;
  DisplaySend(DISPLAY_CMD_ENTER_POWER_SAVE);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Clear floor(height/8) pages of the drawing bank. Height is in pixels; the
 * usual 64-row clear writes 96 zero-valued columns on each of eight pages. */
void DisplayClear(u8 height)
{
  u8 band;
  u8 y;
  u8 remaining;
  int endBand;

  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B0 = 0;
  band = 0;
  endBand = height >> 3;
  while (band < endBand) {
    y = band;
    IO.PDR1.BIT.B1 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = DISPLAY_COL_MSB_COMMAND;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 0;
    if (y > (DISPLAY_BANK_PAGES - 1)) {
      sleep();
    }
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = (g_displayBank.bytes.index * DISPLAY_BANK_PAGES + y +
                 DISPLAY_PAGE_COMMAND_BASE);
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B1 = 1;
    remaining = DISPLAY_RASTER_COLUMN_COUNT;
    while (remaining != 0) {
      while (SSU.SSSR.BIT.TDRE == 0) {
      }
      SSU.SSTDR = 0;
      while (SSU.SSSR.BIT.TDRE == 0) {
      }
      SSU.SSTDR = 0;
      remaining--;
    }
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    band++;
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* A vertical pixel offset splits source pages across panel pages; the last
 * page contains the carried bits. Horizontal animations send their full
 * width, including columns past the visible right edge. */
void DisplayBlit(u8 x, u8 y, u8 width, u8 height, const u8 *raster)
{
  u8 yBit;
  u8 startPage;
  u8 endPage;
  int lastPage;
  int page;
  int col;
  const RasterColumn *columns;
  int bits;

  yBit = (y & 7);
  /* Decode y as signed before division, then store the page indices as bytes.
   * Callers keep the raster within rows 0..63. */
  startPage = (s8)y / RASTER_PAGE_HEIGHT;
  endPage = (height + (s8)y + 7) / RASTER_PAGE_HEIGHT;
  columns = (const RasterColumn *)raster;
  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B0 = 0;
  page = startPage;
  lastPage = endPage - 1;
  for (; page < endPage; page++) {
    u8 row;
    u8 left;

    row = page;
    left = x;
    IO.PDR1.BIT.B1 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = (((left / DISPLAY_COL_MSB_DIVISOR) & DISPLAY_COL_MSB_MASK) +
                 DISPLAY_COL_MSB_COMMAND);
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = (left & DISPLAY_COL_LSB_MASK);
    if (row > (DISPLAY_BANK_PAGES - 1)) {
      sleep();
    }
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = (g_displayBank.bytes.index * DISPLAY_BANK_PAGES + row +
                 DISPLAY_PAGE_COMMAND_BASE);
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B1 = 1;
    for (col = 0; col < width; col++, columns++) {
      const u8 *secondPlane;

      secondPlane = &columns[0][1];
      if (yBit == 0) {
        bits = columns[0][0] << yBit;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = bits;
        bits = *secondPlane << yBit;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = bits;
      } else if (page == lastPage) {
        bits = (columns - width)[0][0] >> (RASTER_PAGE_HEIGHT - yBit);
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = bits;
        bits = (columns - width)[0][1] >> (RASTER_PAGE_HEIGHT - yBit);
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = bits;
      } else if (page != startPage) {
        bits = ((columns - width)[0][0] >> (RASTER_PAGE_HEIGHT - yBit)) |
               (columns[0][0] << yBit);
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = bits;
        bits = ((columns - width)[0][1] >> (RASTER_PAGE_HEIGHT - yBit)) |
               (*secondPlane << yBit);
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = bits;
      } else {
        bits = columns[0][0] << yBit;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = bits;
        bits = *secondPlane << yBit;
        while (SSU.SSSR.BIT.TDRE == 0) {
        }
        SSU.SSTDR = bits;
      }
    }
    while (SSU.SSSR.BIT.TEND == 0) {
    }
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

typedef struct {
  int left;
  int end;
} DisplaySpanInterval;

/* Skip negative full pages and left columns, stopping at the bottom page.
 * The caller supplies a right edge within the panel. Signed y/8 truncates
 * toward zero, while y&7 supplies the pixel offset within each page. */
void DisplayWriteSpan(s8 x, s8 y, uint columnCount, uint rowCount,
                      RasterColumn *raster)
{
  u8 yBit;
  u8 bits;
  int startPage;
  uint endRow;
  int endPage;
  int lastPage;
  int page;
  int col;
  DisplaySpanInterval bounds[1];
  RasterColumn *columns;

  yBit = (y & 7);
  startPage = y / RASTER_PAGE_HEIGHT;
  /* Round the bottom pixel endpoint in unsigned 16-bit arithmetic, then derive
   * its page. */
  endRow = y + rowCount + 7;
  endPage = (endRow >> 3);
  columns = raster;
  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B0 = 0;
  page = startPage;
  bounds[0].left = x;
  bounds[0].end = x + columnCount;
  lastPage = endPage - 1;
  for (; page < endPage; page++) {
    if (page >= DISPLAY_BANK_PAGES) {
      break;
    }
    if (page < 0) {
      columns += columnCount;
    } else {
      if (x < 0) {
        DisplaySetPosition(0, page);
      } else {
        DisplaySetPosition(x, page);
      }
      IO.PDR1.BIT.B1 = 1;
      for (col = bounds[0].left; col < bounds[0].end; col++, columns++) {
        if (col >= 0) {
          u8 *secondPlane;

          secondPlane = &columns[0][1];

          if (yBit == 0) {
            bits = (columns[0][0] << yBit);
            while (SSU.SSSR.BIT.TDRE == 0) {
            }
            SSU.SSTDR = bits;
            bits = (*secondPlane << yBit);
            while (SSU.SSSR.BIT.TDRE == 0) {
            }
            SSU.SSTDR = bits;
          } else if (page == lastPage) {
            bits =
                ((columns - columnCount)[0][0] >> (RASTER_PAGE_HEIGHT - yBit));
            while (SSU.SSSR.BIT.TDRE == 0) {
            }
            SSU.SSTDR = bits;
            bits =
                ((columns - columnCount)[0][1] >> (RASTER_PAGE_HEIGHT - yBit));
            while (SSU.SSSR.BIT.TDRE == 0) {
            }
            SSU.SSTDR = bits;
          } else if (page != startPage) {
            /* Middle pages carry the previous low plane into high and the
             * previous high plane into low. */
            bits = (((columns - columnCount)[0][1] >>
                     (RASTER_PAGE_HEIGHT - yBit)) |
                    (columns[0][0] << yBit));
            while (SSU.SSSR.BIT.TDRE == 0) {
            }
            SSU.SSTDR = bits;
            bits = ((*((u8 *)columns - columnCount * 2) >>
                     (RASTER_PAGE_HEIGHT - yBit)) |
                    (*secondPlane << yBit));
            while (SSU.SSSR.BIT.TDRE == 0) {
            }
            SSU.SSTDR = bits;
          } else {
            bits = (columns[0][0] << yBit);
            while (SSU.SSSR.BIT.TDRE == 0) {
            }
            SSU.SSTDR = bits;
            bits = (*secondPlane << yBit);
            while (SSU.SSSR.BIT.TDRE == 0) {
            }
            SSU.SSTDR = bits;
          }
        }
      }
      while (SSU.SSSR.BIT.TEND == 0) {
      }
    }
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Draw page-aligned black text from spaces, digits and A..Z. Each 3x8 glyph
 * occupies four columns, including a blank separator. The caller supplies
 * valid characters and enough room to the right edge. */
void DisplayText(u8 x, u8 y, const char *glyphStream)
{
  u8 ch;
  u8 i;
  u8 cell;
  u8 page;

  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B0 = 0;
  page = y / RASTER_PAGE_HEIGHT;
  DisplaySetPosition(x, page);
  IO.PDR1.BIT.B1 = 1;
  for (; *glyphStream != 0;) {
    ch = *glyphStream++;
    if (ch == ' ') {
      LCD_WRITE(0);
      LCD_WRITE(0);
      LCD_WRITE(0);
      LCD_WRITE(0);
      LCD_WRITE(0);
      LCD_WRITE(0);
      LCD_WRITE(0);
      LCD_WRITE(0);
      x += DISPLAY_GLYPH_WIDTH;
    } else {
      if (ch > '9') {
        cell = ch - '7';
        IO.PDR1.BIT.B0 = 0;
        i = 0;
        do {
          LCD_WRITE(g_residentResources.assets
                        .alphanumericFont[cell * DISPLAY_GLYPH_CELL_BYTES + i]);
          LCD_WRITE(g_residentResources.assets
                        .alphanumericFont[cell * DISPLAY_GLYPH_CELL_BYTES + i]);
        } while (++i < DISPLAY_GLYPH_CELL_BYTES);
        LCD_WRITE(0);
        LCD_WRITE(0);
        LCD_END();
      } else {
        cell = ch - '0';
        IO.PDR1.BIT.B0 = 0;
        i = 0;
        do {
          LCD_WRITE(g_residentResources.assets
                        .alphanumericFont[cell * DISPLAY_GLYPH_CELL_BYTES + i]);
          LCD_WRITE(g_residentResources.assets
                        .alphanumericFont[cell * DISPLAY_GLYPH_CELL_BYTES + i]);
        } while (++i < DISPLAY_GLYPH_CELL_BYTES);
        LCD_WRITE(0);
        LCD_WRITE(0);
        LCD_END();
      }
      x += DISPLAY_GLYPH_WIDTH;
    }
  }
  LCD_END();
  IO.PDR1.BIT.B0 = 1;
}

#undef LCD_WRITE
#undef LCD_END

/* Draw the top and sides of a 96x16 message frame at pixel row 32. Clear the
 * upper page's interior and preserve the lower page's interior. */
void DisplayNameFrame(void)
{
  u8 *bank;
  u8 remaining;

  bank = &g_displayBank.bytes.index;
  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = DISPLAY_COL_MSB_COMMAND;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = (*bank * DISPLAY_BANK_PAGES + DISPLAY_PAGE_CMD_4);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 1;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0xff;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0xff;
  remaining = DISPLAY_EDGE_INTERIOR_COUNT;
  while (remaining != 0) {
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = DISPLAY_EDGE_FILL_BIT0;
    remaining--;
  }
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0xff;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0xff;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = DISPLAY_COL_MSB_COMMAND;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = (*bank * DISPLAY_BANK_PAGES + DISPLAY_PAGE_CMD_5);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 1;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0xff;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0xff;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = DISPLAY_LAST_COL_MSB;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = DISPLAY_LAST_COL_LSB;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = (*bank * DISPLAY_BANK_PAGES + DISPLAY_PAGE_CMD_5);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 1;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0xff;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0xff;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Draw a closed 96x16 frame at row 48 and clear both interior pages. */
void DisplayBottomNameFrame(void)
{
  u8 remaining;

  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = DISPLAY_COL_MSB_COMMAND;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR =
      (g_displayBank.bytes.index * DISPLAY_BANK_PAGES + DISPLAY_PAGE_CMD_6);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 1;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0xff;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0xff;
  remaining = DISPLAY_EDGE_INTERIOR_COUNT;
  while (remaining != 0) {
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = DISPLAY_EDGE_FILL_BIT0;
    remaining--;
  }
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0xff;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0xff;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = DISPLAY_COL_MSB_COMMAND;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR =
      (g_displayBank.bytes.index * DISPLAY_BANK_PAGES + DISPLAY_PAGE_CMD_7);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 1;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0xff;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0xff;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  remaining = DISPLAY_EDGE_INTERIOR_COUNT;
  while (remaining != 0) {
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = DISPLAY_EDGE_FILL_BIT7;
    remaining--;
  }
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0xff;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0xff;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Replace page six with a black top row and seven white rows. */
void DisplayMessageRule(void)
{
  u8 remaining;

  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = DISPLAY_COL_MSB_COMMAND;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = (g_displayBank.bytes.index * DISPLAY_BANK_PAGES +
               DISPLAY_PAGE_COMMAND_BASE + DISPLAY_RULE_PAGE_OFFSET);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B1 = 1;
  remaining = DISPLAY_RULE_BYTE_COUNT;
  while (remaining != 0) {
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 1;
    remaining--;
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Base contrast followed by NT7508 commands and firmware delay/end directives.
 * The EEPROM format stores the same stream in a 64-byte mirrored payload. */
const u8 g_lcdInitScript[44] = {
    0x14,
    /* 64-row duty, normal segment/common order, first common line 32. */
    0x48, 0x40, 0xa0, 0xc0, 0x44, 0x20,
    /* Oscillator, supply ratios, contrast, bias and frame modulation. */
    0xab, 0x67, 0x25, 0x81, 0x18, 0x52, 0x95,
    /* White, light-gray, dark-gray and black pulse widths across four frames.
     */
    0x88, 0x00, 0x89, 0x00, 0x8a, 0x55, 0x8b, 0x55, 0x8c, 0x77, 0x8d, 0x77,
    0x8e, 0x99, 0x8f, 0x99,
    /* Inversion, temperature compensation, oscillator and frame frequency. */
    0x4c, 0x04, 0xf1, 0x00, 0xf7, 0x02, 0xf6, 0x0a,
    /* Enable supply circuits, delay, then show display RAM from row zero. */
    0x2f, 0xfd, 0x01, 0x40, 0x00, 0xfe};

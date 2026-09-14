#include "application/pw_selftest_data.h"
#include "project.h"
#include "types.h"
#include "startup/hardware.h"
#include "application/pw_buzzer.h"
#include "application/pw_accel_bma150.h"
#include "application/pw_battery.h"
#include "application/pw_nt7508.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_player_input.h"
#include "application/pw_power.h"
#include "support/lib_common.h"
#include "application/pw_rtc.h"
#include "support/scratch.h"
#include "application/pw_selftest.h"
#include "application/pw_storage.h"

/* Ordered sequence: navigation and test completion advance stage by one. */
#define DIAG_CHECK_LCD_SETUP 0
#define DIAG_LCD_BOTH_PLANES 1
#define DIAG_LCD_HIGH_PLANE 2
#define DIAG_LCD_LOW_PLANE 3
#define DIAG_LCD_CLEAR 4
#define DIAG_LCD_BORDER 5
#define DIAG_LCD_GRAY_BARS 6
#define DIAG_BUTTON_LEFT 7
#define DIAG_BUTTON_CENTER 8
#define DIAG_BUTTON_RIGHT 9
#define DIAG_EEPROM_LABEL 10
#define DIAG_EEPROM_TEST 11
#define DIAG_EEPROM_RESULT 12
#define DIAG_BATTERY_TEST 13
#define DIAG_BATTERY_RESULT 14
#define DIAG_RTC_RESULT 15
#define DIAG_ACCEL_TEST 16
#define DIAG_ACCEL_RESULT 17
#define DIAG_COMPLETE 18

#define PW_RTC_STARTUP_DELAY_UNITS 10000
#define PW_ACCEL_SAMPLE_DELAY_UNITS 500
#define PW_EEPROM_PATTERN_BLOCK 0x100
#define PW_EEPROM_FILL_BYTE 0xff
#define DIAGNOSTICS_MOTION_SECONDS 0x1e
#define DIAGNOSTICS_NAV_RENDER_COUNT 4
#define DIAGNOSTICS_INTER_TEST_RENDERS 2
#define PW_EEPROM_SELFTEST_DIAGNOSTICS_ORIGIN 0x300
#define LCD_NORMAL_DISPLAY 0xa6
#define LCD_REVERSE_DISPLAY 0xa7
#define PW_DIAGNOSTICS_GLYPH_X 0x20
#define PW_DIAGNOSTICS_GLYPH_Y 8
#define PW_DIAGNOSTICS_MARKER_Y 0x38
#define PW_DIAGNOSTICS_MARKER_X_LEFT 6
#define PW_DIAGNOSTICS_MARKER_X_MID 0x2d
#define PW_DIAGNOSTICS_MARKER_X_RIGHT 0x55
#define PW_DIAGNOSTICS_OK_Y 0
#define PW_DIAGNOSTICS_HEX_Y 0x18
#define PW_DIAGNOSTICS_ACCEL_OK_X 0x08
#define PW_DIAGNOSTICS_ACCEL_OK_Y 0x20
#define PW_DIAGNOSTICS_FAILURE_X 0x08
#define PW_DIAGNOSTICS_FAILURE_Y 0x08

/* Write and verify incrementing patterns from a 256-byte-aligned start through
 * 0xffff. Return zero at the first mismatch; after success, fill the tested
 * pages with 0xFF. */
u8 EepromSelfTest(uint startAddress)
{
  u16 length;
  u8 *source;
  volatile struct testCursor {
    u8 pattern;
    u16 address;
  } cursor;
  int i;

  length = PW_EEPROM_PATTERN_BLOCK;
  ScratchReset();
  source = ScratchAlloc(length);
  cursor.address = startAddress;
  cursor.pattern = 0;
  do {
    WatchdogService();
    i = 0;
    do {

      ((volatile u8 *)source)[i] = cursor.pattern++;
      i++;
    } while (i < PW_EEPROM_PATTERN_BLOCK);
    EepromWrite(cursor.address, source, length);
    cursor.address += length;
    cursor.pattern++;
  } while (cursor.address != 0);

  cursor.address = startAddress;
  cursor.pattern = 0;
  do {
    WatchdogService();
    EepromRead(cursor.address, source, length);
    i = 0;
    while (i < PW_EEPROM_PATTERN_BLOCK) {
      if (source[i] != cursor.pattern++) {
        return 0;
      }
      i++;
    }
    cursor.address += PW_EEPROM_PATTERN_BLOCK;
    cursor.pattern++;
  } while (cursor.address != 0);

  cursor.address = startAddress;
  do {
    WatchdogService();
    EepromFillPage(cursor.address, PW_EEPROM_FILL_BYTE);
    cursor.address += EEPROM_PAGE_BYTES;
  } while (cursor.address != 0);
  return 1;
}

/* Zero the RTC and wait through the startup delay. Read seconds until two
 * successive samples agree; a nonzero sample passes the check. */
u8 RtcStartupCheck(void)
{
  u16 remaining;
  u8 first;
  u8 second;

  RtcSetTime(0);
  remaining = PW_RTC_STARTUP_DELAY_UNITS;
  do {
    LowClockDelay();
  } while (--remaining != 0);
  do {
    while (RTC.RSECDR.BIT.BSY) {
    }
    first = RTC.RSECDR.BYTE;
    second = RTC.RSECDR.BYTE;
  } while (first != second);
  if (first == 0) {
    return 0;
  }
  return 1;
}

/* Select +/-4 g and 25 Hz filter bandwidth, then send the XYZ high bytes
 * through the LCD chip select. This configuration remains active afterward.
 */
u8 FactoryAccelDump(void)
{
  u8 sample[6];
  u8 *buf;
  u16 remaining;
  u8 txByte;

  if (AccelInit() == 0) {
    return 0;
  }
  buf = sample;
  AccelRead(ACCEL_REG_RANGE_BANDWIDTH, buf, 1);
  sample[0] = (sample[0] & ACCEL_CALIBRATION_BITS);
  sample[0] |= ACCEL_RANGE_4G_BANDWIDTH_25HZ;
  AccelWrite(ACCEL_REG_RANGE_BANDWIDTH, sample[0]);
  AccelWrite(ACCEL_REG_CONTROL, ACCEL_CONTROL_AWAKE);
  remaining = PW_ACCEL_SAMPLE_DELAY_UNITS;
  while (remaining != 0) {
    LowClockDelay();
    remaining--;
  }
  AccelRead(ACCEL_REG_X_LSB, buf, 6);
  txByte = sample[1];
  IO.PDR1.BIT.B0 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = txByte;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
  LowClockDelay();
  txByte = sample[3];
  IO.PDR1.BIT.B0 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = txByte;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
  LowClockDelay();
  txByte = sample[5];
  IO.PDR1.BIT.B0 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = txByte;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
  LowClockDelay();
  return 1;
}

/* Send the ADC sample to the fixture, high byte first, and save it as the
 * checksum-protected battery threshold in both EEPROM mirrors. */
u8 FactoryBatteryCalibrate(void)
{
  uint sample;
  u8 txByte;

  sample = BatterySample();
  txByte = (sample >> 8);
  IO.PDR1.BIT.B0 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = txByte;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
  LowClockDelay();
  txByte = sample;
  IO.PDR1.BIT.B0 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = txByte;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
  LowClockDelay();
  sample = BatteryProtect(sample);
  return EepromMirrorWrite(EEPROM_BATTERY_PRIMARY, EEPROM_BATTERY_BACKUP,
                           (u8 *)&sample, 2);
}

/* Stream four 24-column grayscale bars (00/00, 00/FF, FF/00, FF/FF) across all
 * eight pages of the drawing bank. */
void DiagnosticsGray(void)
{
  u8 row;
  u8 col;
  int band;

  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B0 = 0;
  IO.PDR1.BIT.B1 = 1;
  row = 0;
  do {
    DisplayAddress(0, row);
    IO.PDR1.BIT.B1 = 1;
    col = 0;
    do {
      band = col / 24;
      switch (band) {
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
    } while (col < 0x60);
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    row++;
  } while (row < 8);
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Start diagnostics with the LCD readback result left in diagnosticsReady by
 * IR setup. That result stays in the same byte across the view-bank handoff. */
void DiagnosticsInit(void)
{
  g_ui.view.diag.stage = DIAG_CHECK_LCD_SETUP;
  g_ui.view.diag.renderCount = 0;
  g_ui.view.diag.batteryReference = 1;
  g_state.save.volume = SAVE_DEFAULT_VOLUME;
  BeepSetOutputMode(SAVE_DEFAULT_VOLUME);
  DisplaySetContrast(SAVE_DEFAULT_CONTRAST);
}

/* Snapshot BCD seconds around the destructive EEPROM test. */
#pragma inline(DiagnosticsSecond)
static u8 DiagnosticsSecond(void)
{
  u8 first;

  for (;;) {
    while (RTC.RSECDR.BIT.BSY) {
    }
    first = RTC.RSECDR.BYTE;
    if (first == RTC.RSECDR.BYTE) {
      break;
    }
  }
  return first;
}

/* Tests and result screens occupy separate stages. Failed probes stop on their
 * result screen; successful probes advance in order. Wait gates count calls
 * to DiagnosticsRender. */
void DiagnosticsUpdate(void)
{
  u8 first;

  g_state.idleSeconds[IDLE_DISPLAY] = INTERACTIVE_DISPLAY_SECONDS;
  g_state.idleSeconds[IDLE_MOTION] = DIAGNOSTICS_MOTION_SECONDS;
  switch (g_ui.view.diag.stage) {
  case DIAG_CHECK_LCD_SETUP:
    if (g_ui.view.diag.diagnosticsReady == 0) {
      return;
    }
    g_ui.view.diag.stage++;
    return;
  case DIAG_LCD_BOTH_PLANES:
  case DIAG_LCD_HIGH_PLANE:
  case DIAG_LCD_LOW_PLANE:
  case DIAG_LCD_CLEAR:
  case DIAG_LCD_BORDER:
  case DIAG_LCD_GRAY_BARS:
    if (g_ui.view.diag.renderCount < DIAGNOSTICS_NAV_RENDER_COUNT) {
      return;
    }
    if (InputPressed(BUTTON_CENTER | BUTTON_RIGHT) != 0) {
      BeepSelectScore(g_testScore);
      g_ui.view.diag.stage++;
      g_ui.view.diag.renderCount = 0;
      return;
    }
    if (g_ui.view.diag.stage == DIAG_LCD_BOTH_PLANES) {
      return;
    }
    if (InputPressed(BUTTON_LEFT) == 0) {
      return;
    }
    BeepSelectScore(g_testScore);
    g_ui.view.diag.stage--;
    g_ui.view.diag.renderCount = 0;
    return;
  case DIAG_BUTTON_LEFT:
    if (InputPressed(BUTTON_LEFT) == 0) {
      return;
    }
    BeepSelectScore(g_testScore);
    g_ui.view.diag.stage++;
    g_ui.view.diag.renderCount = 0;
    return;
  case DIAG_BUTTON_CENTER:
    if (InputPressed(BUTTON_CENTER) == 0) {
      return;
    }
    BeepSelectScore(g_testScore);
    g_ui.view.diag.stage++;
    g_ui.view.diag.renderCount = 0;
    return;
  case DIAG_BUTTON_RIGHT:
    if (InputPressed(BUTTON_RIGHT) == 0) {
      return;
    }
    BeepSelectScore(g_testScore);
    g_ui.view.diag.stage++;
    g_ui.view.diag.renderCount = 0;
    return;
  case DIAG_EEPROM_LABEL:
    if (g_ui.view.diag.renderCount < DIAGNOSTICS_INTER_TEST_RENDERS) {
      return;
    }
    g_ui.view.diag.stage++;
    g_ui.view.diag.renderCount = 0;
    return;
  case DIAG_EEPROM_TEST:
    first = DiagnosticsSecond();
    g_ui.view.diag.rtcFirstSample = first;
    g_ui.view.diag.probeResult =
        EepromSelfTest(PW_EEPROM_SELFTEST_DIAGNOSTICS_ORIGIN);
    PersistentReset(RESET_CLEAR_EVENTS, RESET_CLEAR_LIFETIME);
    first = DiagnosticsSecond();
    g_ui.view.diag.rtcSecondSample = first;
    g_ui.view.diag.stage++;
    g_ui.view.diag.renderCount = 0;
    return;
  case DIAG_EEPROM_RESULT:
    if (g_ui.view.diag.probeResult == 0) {
      return;
    }
    if (g_ui.view.diag.renderCount < DIAGNOSTICS_NAV_RENDER_COUNT) {
      return;
    }
    BeepSelectScore(g_testScore);
    g_ui.view.diag.stage++;
    g_ui.view.diag.renderCount = 0;
    return;
  case DIAG_BATTERY_TEST:
    EepromMirrorRead(EEPROM_BATTERY_PRIMARY, EEPROM_BATTERY_BACKUP,
                     (u8 *)&g_ui.view.diag.batteryReference, 2);
    g_ui.view.diag.probeResult = BatteryVerify(g_ui.view.diag.batteryReference);
    if (g_ui.view.diag.batteryReference == 0) {
      g_ui.view.diag.probeResult = 0;
    }
    g_ui.view.diag.stage++;
    g_ui.view.diag.renderCount = 0;
    return;
  case DIAG_BATTERY_RESULT:
    if (g_ui.view.diag.probeResult == 0) {
      return;
    }
    if (g_ui.view.diag.renderCount < DIAGNOSTICS_NAV_RENDER_COUNT) {
      return;
    }
    BeepSelectScore(g_testScore);
    g_ui.view.diag.stage++;
    g_ui.view.diag.renderCount = 0;
    return;
  case DIAG_RTC_RESULT:
    if (g_ui.view.diag.rtcFirstSample == g_ui.view.diag.rtcSecondSample) {
      return;
    }
    BeepSelectScore(g_testScore);
    g_ui.view.diag.stage++;
    g_ui.view.diag.renderCount = 0;
    return;
  case DIAG_ACCEL_TEST:
    g_ui.view.diag.probeResult = AccelInit();
    g_ui.view.diag.stage++;
    g_ui.view.diag.renderCount = 0;
    return;
  case DIAG_ACCEL_RESULT:
    if (g_ui.view.diag.probeResult == 0) {
      return;
    }
    BeepSelectScore(g_testScore);
    g_ui.view.diag.stage++;
    g_ui.view.diag.renderCount = 0;
    return;
  case DIAG_COMPLETE:
    if (g_ui.view.diag.renderCount < DIAGNOSTICS_NAV_RENDER_COUNT) {
      return;
    }
    if (InputPressed(BUTTON_ANY) == 0) {
      return;
    }
    IO.PDR1.BIT.B0 = 0;
    IO.PDR1.BIT.B1 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = LCD_NORMAL_DISPLAY;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    g_state.view = VIEW_THRESHOLD_TEST;
    ThresholdInit();
    BeepSelectScore(g_testScore);
    return;
  }
}

/* Draw a one-pixel border around the full display. */
void DiagnosticsBorder(void)
{
  u16 remaining;
  int page;

  SSU.SSER.BYTE = SSU_TX_ENABLE;
  IO.PDR1.BIT.B0 = 0;
  DisplayAddress(1, 0);
  IO.PDR1.BIT.B1 = 1;
  remaining = 0xbc;
  while (remaining != 0) {
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 1;
    remaining--;
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  page = 0;
  do {
    DisplayAddress(0, page);
    IO.PDR1.BIT.B1 = 1;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 0xff;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 0xff;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    page++;
  } while (page < 8);
  page = 0;
  do {
    DisplayAddress(0x5f, page);
    IO.PDR1.BIT.B1 = 1;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 0xff;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 0xff;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    page++;
  } while (page < 8);
  DisplayAddress(1, 7);
  IO.PDR1.BIT.B1 = 1;
  remaining = 0xbc;
  while (remaining != 0) {
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = 0x80;
    remaining--;
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
}

/* Rendering advances the per-stage hold count, saturating at four calls.
 * Update uses that count to admit navigation and begin subsequent tests. */
void DiagnosticsRender(void)
{
  char hex[5];

  switch (g_ui.view.diag.stage) {
  case DIAG_CHECK_LCD_SETUP:
    if (g_ui.view.diag.diagnosticsReady != 0) {
      break;
    }
    DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_GLYPH_Y, "NG1");
    break;
  case DIAG_EEPROM_LABEL:
  case DIAG_EEPROM_TEST:
    DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_GLYPH_Y, "EEP");
    break;
  case DIAG_EEPROM_RESULT:
    if (g_ui.view.diag.probeResult == 0) {
      DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_GLYPH_Y, "NG2");
    }
    break;
  case DIAG_BATTERY_RESULT:
    if (g_ui.view.diag.probeResult == 0) {
      DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_GLYPH_Y, "NG3");
    }
    break;
  case DIAG_RTC_RESULT:
    if (g_ui.view.diag.rtcFirstSample == g_ui.view.diag.rtcSecondSample) {
      DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_GLYPH_Y, "NG4");
    }
    break;
  case DIAG_LCD_BOTH_PLANES:
    DisplayFill(3);
    break;
  case DIAG_LCD_HIGH_PLANE:
    DisplayFill(2);
    break;
  case DIAG_LCD_LOW_PLANE:
    DisplayFill(1);
    break;
  case DIAG_LCD_CLEAR:
    DisplayFill(0);
    break;
  case DIAG_LCD_BORDER:
    DiagnosticsBorder();
    break;
  case DIAG_LCD_GRAY_BARS:
    DiagnosticsGray();
    break;
  case DIAG_BUTTON_LEFT:
    if (((g_state.uiFrame >> 1) & 1) == 0) {
      DisplayText(PW_DIAGNOSTICS_MARKER_X_LEFT, PW_DIAGNOSTICS_MARKER_Y, "V");
    }
    break;
  case DIAG_BUTTON_CENTER:
    if (((g_state.uiFrame >> 1) & 1) == 0) {
      DisplayText(PW_DIAGNOSTICS_MARKER_X_MID, PW_DIAGNOSTICS_MARKER_Y, "V");
    }
    break;
  case DIAG_BUTTON_RIGHT:
    if (((g_state.uiFrame >> 1) & 1) == 0) {
      DisplayText(PW_DIAGNOSTICS_MARKER_X_RIGHT, PW_DIAGNOSTICS_MARKER_Y, "V");
    }
    break;
  case DIAG_ACCEL_RESULT:
    if (g_ui.view.diag.probeResult == 0) {
      DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_GLYPH_Y, "NG5");
    }
    break;
  case DIAG_COMPLETE:
    IO.PDR1.BIT.B0 = 0;
    IO.PDR1.BIT.B1 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = LCD_REVERSE_DISPLAY;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_OK_Y, "OK");
    hex[4] = 0;
    hex[0] = g_hexDigits[(g_ui.view.diag.batteryReference / 0x1000) & 0xf];
    hex[1] = g_hexDigits[(g_ui.view.diag.batteryReference >> 8) & 0xf];
    hex[2] = g_hexDigits[(g_ui.view.diag.batteryReference >> 4) & 0xf];
    hex[3] = g_hexDigits[g_ui.view.diag.batteryReference & 0xf];
    DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_HEX_Y, hex);
    if (((g_state.uiFrame >> 1) & 1) != 0) {
      break;
    }
    DisplayText(PW_DIAGNOSTICS_MARKER_X_LEFT, PW_DIAGNOSTICS_MARKER_Y, "V");
    DisplayText(PW_DIAGNOSTICS_MARKER_X_MID, PW_DIAGNOSTICS_MARKER_Y, "V");
    DisplayText(PW_DIAGNOSTICS_MARKER_X_RIGHT, PW_DIAGNOSTICS_MARKER_Y, "V");
    break;
  }
  if (g_ui.view.diag.renderCount < DIAGNOSTICS_NAV_RENDER_COUNT) {
    g_ui.view.diag.renderCount++;
  }
}

/* Reset acquisition position and diagnostic accumulators, then load the
 * motion acceptance thresholds. Old samples are replaced as acquisition runs.
 */
void ThresholdInit(void)
{
  g_state.sampleIndex = 0;
  g_ui.view.accel.resetWord = 0x10;
  g_ui.view.accel.walkingBatches = 0;
  g_ui.view.accel.stillBatches = 0;
  g_ui.view.accel.xActivity = 0;
  g_ui.view.accel.yActivity = 0;
  g_ui.view.accel.zActivity = 0;
  EepromRead(EEPROM_COUNTERS, &g_ui.view.accel.thresholds,
             sizeof(MotionThresholds));
  g_state.flags.byte |= SYSTEM_REGISTERED;
}

void ThresholdUpdate(void)
{
}

/* Show walking and still batch counts, their targets, and hexadecimal activity
 * thresholds. Display "OK" when stillBatches equals its target. */
void ThresholdRender(void)
{
  char hex[5];

  g_state.idleSeconds[IDLE_DISPLAY] = INTERACTIVE_DISPLAY_SECONDS;
  g_state.idleSeconds[IDLE_MOTION] = DIAGNOSTICS_MOTION_SECONDS;

  hex[0] = (g_ui.view.accel.walkingBatches + '0');
  hex[1] = 0;
  DisplayText(0x4c, 0x00, hex);

  hex[0] = (g_ui.view.accel.thresholds.walkingBatchTarget + '0');
  hex[1] = 0;
  DisplayText(0x54, 0x00, hex);

  hex[0] = g_hexDigits[g_ui.view.accel.thresholds.walkingActivityMin / 0x1000];
  hex[1] =
      g_hexDigits[(g_ui.view.accel.thresholds.walkingActivityMin >> 8) & 0xf];
  hex[2] =
      g_hexDigits[(g_ui.view.accel.thresholds.walkingActivityMin >> 4) & 0xf];
  hex[3] = g_hexDigits[g_ui.view.accel.thresholds.walkingActivityMin & 0xf];
  hex[4] = 0;
  DisplayText(PW_DIAGNOSTICS_GLYPH_X, PW_DIAGNOSTICS_GLYPH_Y, hex);

  hex[0] = g_hexDigits[g_ui.view.accel.thresholds.walkingActivityMax / 0x1000];
  hex[1] =
      g_hexDigits[(g_ui.view.accel.thresholds.walkingActivityMax >> 8) & 0xf];
  hex[2] =
      g_hexDigits[(g_ui.view.accel.thresholds.walkingActivityMax >> 4) & 0xf];
  hex[3] = g_hexDigits[g_ui.view.accel.thresholds.walkingActivityMax & 0xf];
  DisplayText(0x40, 0x08, hex);

  hex[0] = g_hexDigits[g_ui.view.accel.thresholds.stillActivityLimit / 0x1000];
  hex[1] =
      g_hexDigits[(g_ui.view.accel.thresholds.stillActivityLimit >> 8) & 0xf];
  hex[2] =
      g_hexDigits[(g_ui.view.accel.thresholds.stillActivityLimit >> 4) & 0xf];
  hex[3] = g_hexDigits[g_ui.view.accel.thresholds.stillActivityLimit & 0xf];
  DisplayText(0x20, 0x10, hex);

  hex[0] = (g_ui.view.accel.stillBatches + '0');
  hex[1] = 0;
  DisplayText(0x4c, 0x10, hex);

  hex[0] = (g_ui.view.accel.thresholds.stillBatchTarget + '0');
  DisplayText(0x54, 0x10, hex);

  if (g_ui.view.accel.stillBatches ==
      g_ui.view.accel.thresholds.stillBatchTarget) {
    IO.PDR1.BIT.B0 = 0;
    IO.PDR1.BIT.B1 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = LCD_REVERSE_DISPLAY;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    DisplayText(PW_DIAGNOSTICS_ACCEL_OK_X, PW_DIAGNOSTICS_ACCEL_OK_Y, "OK");
  }
}

void ThresholdFailureRender(void)
{
  DisplayText(PW_DIAGNOSTICS_FAILURE_X, PW_DIAGNOSTICS_FAILURE_Y, "NG6");
}

/* Write the eight-byte "nintendo" signature at the start of EEPROM. */

void BootSignatureWrite(void)
{
  u8 i;

  i = 0;
  do {
    EepromWriteByte(i, g_eepromSignature[i]);
    i++;
  } while (i < EEPROM_SIGNATURE_BYTES);
}

u8 BootSignatureValid(void)
{
  u8 i;

  i = 0;
  while (i < EEPROM_SIGNATURE_BYTES) {
    if (EepromReadByte(i) != g_eepromSignature[i]) {
      return 0;
    }
    i++;
  }
  return 1;
}

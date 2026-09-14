#include "flags.h"
#include "types.h"
#include "project.h"
#include "application/pw_battery.h"
#include "application/pw_eeprom_m95512.h"
#include "support/lib_common.h"

/* The mirrored EEPROM word stores the battery threshold in bits 0..11.
 * Bits 12..15 hold the low nibble of the sum of its three payload nibbles. */
#define PW_BATTERY_THRESHOLD_MASK 0x0fffu
#define PW_BATTERY_NIBBLE_MASK 0x0fu
#define PW_BATTERY_CHECKSUM_SHIFT 12

/* Replace the high nibble with the checksum of the payload's three nibbles. */
uint BatteryProtect(uint value)
{
  u8 checksum;

  value &= PW_BATTERY_THRESHOLD_MASK;
  checksum = ((value >> 4) & PW_BATTERY_NIBBLE_MASK);
  checksum += (u8)((value >> 8) & PW_BATTERY_NIBBLE_MASK);
  checksum += (u8)(value & PW_BATTERY_NIBBLE_MASK);
  value |= checksum << PW_BATTERY_CHECKSUM_SHIFT;

  return value;
}

#include <machine.h>
#include "startup/iodefine.h"

/* Recompute the checksum from the three payload nibbles and compare it with the
 * stored high nibble. */
u8 BatteryVerify(u16 value)
{
  u8 checksum;

  checksum = (((value >> 4) & PW_BATTERY_NIBBLE_MASK) +
              ((value >> 8) & PW_BATTERY_NIBBLE_MASK) +
              (value & PW_BATTERY_NIBBLE_MASK)) &
             PW_BATTERY_NIBBLE_MASK;
  if (checksum != (value / 0x1000)) {
    return 0;
  }
  return 1;
}

/* Average eight channel-7 conversions after removing the result register
 * alignment. Settle the switched measurement circuit before sampling; clock
 * selection determines the ADC conversion timing. */
uint BatterySample(void)
{
  int total;
  u16 samples;
  u16 adcAlignment;

  total = 0;
  IO.PCR8 |= 0x10;
  IO.PDR8.BYTE = 0x10;
  LowClockDelay();
  samples = 8;
  adcAlignment = 64;
  do {
    CKSTPR1.BYTE |= 0x10;
    nop();
    nop();
    nop();
    nop();
    nop();
    IENR2.BYTE &= 0xbf;
    AD.AMR.BYTE = ((AD.AMR.BYTE & 0xf0) | 7);
    if (g_state.events.bits.lowPowerClock) {
      AD.AMR.BYTE = ((AD.AMR.BYTE & 0xcf) | 0x20);
    } else {
      AD.AMR.BYTE = ((AD.AMR.BYTE & 0xcf) | 0x30);
    }
    AD.ADSR.BIT.ADSF = 1;
    while (AD.ADSR.BIT.ADSF) {
    }
    AD.AMR.BYTE &= 0xf0;
    CKSTPR1.BYTE &= 0xef;
    total += (AD.ADRR / adcAlignment);
    samples--;
  } while (samples != 0);
  IO.PDR8.BYTE = 0;
  IO.PCR8 &= 0xef;
  return (total / 8);
}

u8 BatteryCheckLow(u16 scaleFactor)
{
  volatile u16 record;

  EepromMirrorRead(EEPROM_BATTERY_PRIMARY, EEPROM_BATTERY_BACKUP, (u8 *)&record,
                   2);
  if (BatteryVerify(record) == 0) {
    record = 0;
    EepromMirrorWrite(EEPROM_BATTERY_PRIMARY, EEPROM_BATTERY_BACKUP,
                      (u8 *)&record, 2);
  }
  record = (record & PW_BATTERY_THRESHOLD_MASK);
  record = (record * scaleFactor / 20);
  if (BatterySample() <= record) {
    return 1;
  }
  return 0;
}

void BatteryUpdate(void)
{
  SystemEvents flags;

  flags.byte = g_state.events.byte;
  if (flags.bits.batteryCheckPending) {
    if (BatteryCheckLow(0x14) != 0) {
      g_state.events.byte |= EVENT_BATTERY_LOW;
    } else {
      g_state.events.byte &= EVENT_CLEAR(EVENT_BATTERY_LOW);
    }
    g_state.events.byte &= EVENT_CLEAR(EVENT_BATTERY_CHECK);
  }
}

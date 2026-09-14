#include "startup/h8_rominfo.h"
#include "types.h"
#include "startup/hardware.h"
#include <machine.h>
#include "application/pw_accel_bma150.h"
#include "application/pw_eeprom_m95512.h"
#include "application/pw_factory_test.h"
#include "support/lib_common.h"
#include "application/pw_selftest.h"
#include "application/pw_storage.h"

/* The fixture receives the two identification bytes before the 12-byte build
 * timestamp, including its terminator. */

#define PW_DISPLAY_HANDSHAKE_COMMAND_E2 0xe2u
#define PW_DISPLAY_HANDSHAKE_RESPONSE_AA 0xaau
#define PW_DISPLAY_HANDSHAKE_REJECT_B0 0xb0u
#define PW_BOOT_SELF_TEST_STAGE_EEPROM 4
#define PW_BOOT_SELF_TEST_STAGE_RTC 3
#define PW_BOOT_SELF_TEST_STAGE_ACCEL 2
#define PW_BOOT_SELF_TEST_STAGE_ADC 1
#define PW_BOOT_SELF_TEST_STAGE_DONE 0
#define PW_BOOT_SELF_TEST_FAILURE_F4 0xf4u
#define PW_BOOT_SELF_TEST_FAILURE_F3 0xf3u
#define PW_BOOT_SELF_TEST_FAILURE_F1 0xf1u
#define PW_BUILD_DATE_BYTES 12

/* An accepted fixture handshake runs destructive EEPROM and RTC checks,
 * streams accelerometer samples, and calibrates the battery reference before
 * sleeping. EEPROM/RTC failures hang; the accelerometer return is ignored and
 * calibration tests the event byte. A rejected handshake resumes startup. */
void BootSelfTest(void)
{
  u8 handshake;
  u8 txByte;
  int i;

  set_ccr(0x80);

  SSU.SSER.BYTE = SSU_TX_RX_ENABLE;
  SSU.SSMR.BYTE = ((SSU.SSMR.BYTE & 0xf8) | 6);
  if (SSU.SSSR.BIT.ORER) {
    SSU.SSSR.BIT.ORER = 0;
  }

  IO.PDR1.BIT.B0 = 0;
  while (SSU.SSSR.BIT.TDRE == 0) {
  }
  SSU.SSTDR = PW_DISPLAY_HANDSHAKE_COMMAND_E2;
  while (SSU.SSSR.BIT.TEND == 0) {
  }
  IO.PDR1.BIT.B0 = 1;
  while (SSU.SSSR.BIT.RDRF == 0) {
  }
  handshake = SSU.SSRDR;
  SSU.SSER.BYTE = SSU_TX_ENABLE;
  LowClockDelay();

  if (handshake != PW_DISPLAY_HANDSHAKE_RESPONSE_AA) {
    IO.PDR1.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_DISPLAY_HANDSHAKE_REJECT_B0;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    LowClockDelay();
  } else {
    txByte = g_firmwareId[0];
    IO.PDR1.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = txByte;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    LowClockDelay();

    txByte = g_firmwareId[1];
    IO.PDR1.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = txByte;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    LowClockDelay();

    i = 0;
    do {
      txByte = g_buildDate[i];
      IO.PDR1.BIT.B0 = 0;
      while (SSU.SSSR.BIT.TDRE == 0) {
      }
      SSU.SSTDR = txByte;
      while (SSU.SSSR.BIT.TEND == 0) {
      }
      IO.PDR1.BIT.B0 = 1;
      LowClockDelay();
      i++;
    } while (i < PW_BUILD_DATE_BYTES);

    WDT.TCSRWD1.BYTE = 0x9e;
    WDT.TCSRWD1.BYTE = 0xa2;
    WDT.TCSRWD1.BYTE = 0x8e;

    IO.PDR1.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_BOOT_SELF_TEST_STAGE_EEPROM;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    if (EepromSelfTest(0) == 0) {
      IO.PDR1.BIT.B0 = 0;
      while (SSU.SSSR.BIT.TDRE == 0) {
      }
      SSU.SSTDR = PW_BOOT_SELF_TEST_FAILURE_F4;
      while (SSU.SSSR.BIT.TEND == 0) {
      }
      IO.PDR1.BIT.B0 = 1;
      for (;;) {
      }
    }

    PersistentReset(RESET_CLEAR_EVENTS, RESET_CLEAR_LIFETIME);

    IO.PDR1.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_BOOT_SELF_TEST_STAGE_RTC;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    if (RtcStartupCheck() == 0) {
      IO.PDR1.BIT.B0 = 0;
      while (SSU.SSSR.BIT.TDRE == 0) {
      }
      SSU.SSTDR = PW_BOOT_SELF_TEST_FAILURE_F3;
      while (SSU.SSSR.BIT.TEND == 0) {
      }
      IO.PDR1.BIT.B0 = 1;
      for (;;) {
      }
    }

    IO.PDR1.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_BOOT_SELF_TEST_STAGE_ACCEL;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    FactoryAccelDump();

    IO.PDR1.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_BOOT_SELF_TEST_STAGE_ADC;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;
    /* Fail when the returned SystemEvents byte is zero. The low-power-clock
     * flag set during reset normally keeps that byte nonzero. */
    if (FactoryBatteryCalibrate() == 0) {
      IO.PDR1.BIT.B0 = 0;
      while (SSU.SSSR.BIT.TDRE == 0) {
      }
      SSU.SSTDR = PW_BOOT_SELF_TEST_FAILURE_F1;
      while (SSU.SSSR.BIT.TEND == 0) {
      }
      IO.PDR1.BIT.B0 = 1;
      for (;;) {
      }
    }

    IO.PDR1.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = PW_BOOT_SELF_TEST_STAGE_DONE;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR1.BIT.B0 = 1;

    IO.PDR9.BIT.B0 = 0;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = ACCEL_REG_CONTROL;
    while (SSU.SSSR.BIT.TDRE == 0) {
    }
    SSU.SSTDR = ACCEL_CONTROL_SLEEP;
    while (SSU.SSSR.BIT.TEND == 0) {
    }
    IO.PDR9.BIT.B0 = 1;

    set_ccr(0x80);
    CKSTPR1.BIT.S3CKSTP = 0;
    CKSTPR1.BIT.ADCKSTP = 0;
    CKSTPR1.BIT.TB1CKSTP = 0;
    CKSTPR1.BIT.RTCCKSTP = 0;
    CKSTPR2.BYTE = 0;
    IO.PDR3.BYTE = 1;
    ClockSleep(CLOCK_SLEEP_NORMAL);
  }

  IO.PDR1.BIT.B0 = 1;
  set_ccr(0);
  return;
}

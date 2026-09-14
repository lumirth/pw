#include "project.h"
#include "types.h"
#include "eeprom_address.h"
#include "startup/iodefine.h"
#include "application/pw_buzzer.h"
#include "application/pw_builtin.h"
#include "application/pw_eeprom_m95512.h"

/* Four-byte EEPROM directory entry. The offset is stored little-endian and
 * names a score relative to EEPROM_SOUND_DATA. */
typedef struct {
  u16 offsetLe;
  u8 byteLength;
  u8 checksum;
} SoundEntry;

typedef char SoundEntrySize[sizeof(SoundEntry) == 4 ? 1 : -1];

enum { SOUND_DIRECTORY_ENTRIES = 16, SOUND_STORAGE_BYTES = 480 };

typedef struct {
  SoundEntry entries[SOUND_DIRECTORY_ENTRIES];
  u8 scores[SOUND_STORAGE_BYTES];
} SoundArchive;

typedef char SoundArchiveData[offsetof(SoundArchive, scores) ==
                                      EEPROM_SOUND_DATA - EEPROM_SOUND_DIRECTORY
                                  ? 1
                                  : -1];
typedef char SoundArchiveSize[sizeof(SoundArchive) == 544 ? 1 : -1];

#define TIMER_W_WATCH_CLEAR_A 0xc0
#define TIMER_W_WATCH_CLEAR_A_B_HIGH 0xc2
#define TIMER_W_COMPARE_ONLY 0x80
#define TIMER_W_PWM_BC 0x83
#define TIMER_W_COMPARE_B_LOW 0x10
#define TIMER_W_COMPARE_C_LOW 1

u8 BeepHasScore(void)
{
  if (g_note == 0) {
    return 0;
  }
  return 1;
}

/* Initialize the output pins and compare controls, clear the score selection,
 * and gate Timer W's clock. */
void BeepInit(void)
{
  g_ui.durationDivisor = 0x78;
  g_ui.outputMode = 0;
  IO.PCR8 |= 0x0c;
  IO.PDR8.BIT.B2 = 0;
  IO.PDR8.BIT.B3 = 0;
  CKSTPR2.BYTE |= 0x40;
  TW.TCRW.BYTE = TIMER_W_WATCH_CLEAR_A;
  TW.TIOR0.BYTE = TIMER_W_COMPARE_B_LOW;
  TW.TIOR1.BYTE = TIMER_W_COMPARE_C_LOW;
  TW.GRA = 0;
  TW.GRB = 0;
  TW.GRC = 0;
  CKSTPR2.BYTE &= 0xbf;
  g_note = 0;
}

#define BEEPER_SEQUENCE_RAM g_work.beeper.score
#define BEEPER_SEQUENCE_MAX_BYTES sizeof(g_work.beeper.score)
#define BEEPER_CONTROL_VALUE_MASK 0x7f
#define BEEPER_CONTROL_RESTART_MIN 0x7e

/* Load an EEPROM score into the motion sample RAM. Check the byte sum of
 * complete two-byte notes against the descriptor checksum and require the
 * last complete note's pitch to repeat or end. An oversized descriptor keeps
 * the previous score selected. */
void BeepLoadScore(u8 sequenceId)
{
  SoundEntry descriptor;
  u16 location;
  u8 id;
  u8 noteIndex;
  u8 checksum;
  SoundEntry *directoryEntry;

  if (g_ui.outputMode == 0) {
    return;
  }
  TW.TIERW.BIT.IMIEA = 0;
  id = sequenceId;
  /* Calculate the selected descriptor's serial EEPROM address. */
  directoryEntry = (SoundEntry *)EEPROM_SOUND_DIRECTORY;
  directoryEntry += id;
  EepromRead((u16)directoryEntry, &descriptor, sizeof(SoundEntry));
  /* Decode the stored little-endian offset, then rebase it into EEPROM. */
  location = descriptor.offsetLe;
  location = ((location >> 8) | (location << 8));
  location =
      (location + (u16) & ((SoundArchive *)EEPROM_SOUND_DIRECTORY)->scores[0]);
  if (descriptor.byteLength <= BEEPER_SEQUENCE_MAX_BYTES) {
    g_note = BEEPER_SEQUENCE_RAM;
    EepromRead(location, g_note, descriptor.byteLength);
    noteIndex = 0;
    checksum = 0;
    while (noteIndex < ((u16)descriptor.byteLength >> 1)) {
      checksum = (checksum + g_note[noteIndex].duration);
      checksum = (checksum + g_note[noteIndex].pitch);
      noteIndex++;
    }
    if ((checksum != descriptor.checksum) ||
        ((((u8 *)g_note)[(((u16)descriptor.byteLength >> 1) << 1) - 1] &
          BEEPER_CONTROL_VALUE_MASK) < BEEPER_CONTROL_RESTART_MIN)) {
      g_note = 0;
    } else {
      g_ui.periodsRemaining = 0;
      g_ui.separatorPeriodsRemaining = 0;
    }
  }
  TW.TIERW.BIT.IMIEA = 1;
}

void BeepSelectScore(const Note *score)
{
  g_note = score;
  g_ui.periodsRemaining = 0;
  g_ui.separatorPeriodsRemaining = 0;
}

/* Count watch-clock cycles and interrupt at compare A. Score selection and
 * the remaining duration counters are independent of the timer enable. */
void BeepEnableTimer(void)
{
  CKSTPR2.BYTE |= 0x40;
  TW.TIERW.BIT.IMIEA = 0;
  TW.TCRW.BYTE = TIMER_W_WATCH_CLEAR_A;
  TW.TIOR0.BYTE = TIMER_W_COMPARE_B_LOW;
  TW.TIOR1.BYTE = TIMER_W_COMPARE_C_LOW;
  TW.TSRW.BIT.IMFA = 0;
  TW.TIERW.BIT.IMIEA = 1;
  TW.TCNT = 0;
  TW.TMRW.BYTE = TIMER_W_COMPARE_ONLY;
}

/* Stop interrupts and the module clock, preserving the selected score. */
void BeepDisableTimer(void)
{
  TW.TIERW.BIT.IMIEA = 0;
  TW.TMRW.BYTE = 0;
  TW.TCRW.BYTE = TIMER_W_WATCH_CLEAR_A;
  TW.TSRW.BIT.IMFA = 0;
  CKSTPR2.BYTE &= 0xbf;
}

void BeepSetOutputMode(u8 mode)
{
  g_ui.outputMode = mode;
}

#define BEEPER_OUTPUT_ALL_EQUAL 0
#define BEEPER_OUTPUT_GRB_HALF 1
#define BEEPER_OUTPUT_GRB_GRC_HALF 2
#define BEEPER_SEPARATOR_COMPARE 0x140
/* At 32768 watch clocks/s, 60/24 converts score duration units and tempo
 * into a clock budget. */
#define BEEPER_DURATION_CLOCK_SCALE 0x14000ul

/* Set GRA and select equal or half-value B/C compares for the output mode.
 * The period lasts GRA+1 watch clocks; half-value compares round down. Other
 * modes preserve the compares. Every call resets the timer count. */
void BeepSetPeriod(u8 compareValue)
{
  switch (g_ui.outputMode) {
  case BEEPER_OUTPUT_ALL_EQUAL:
    TW.GRA = compareValue;
    TW.GRB = compareValue;
    TW.GRC = compareValue;
    break;
  case BEEPER_OUTPUT_GRB_HALF:
    TW.GRA = compareValue;
    TW.GRB = compareValue >> 1;
    TW.GRC = compareValue;
    break;
  case BEEPER_OUTPUT_GRB_GRC_HALF:
    TW.GRA = compareValue;
    TW.GRB = compareValue >> 1;
    TW.GRC = compareValue >> 1;
    break;
  }
  TW.TCNT = 0;
}

/* Called per compare-A interrupt. g_note points to the next record while the
 * preceding note's period count runs down. A non-legato note schedules one
 * separator period before the next record is interpreted. */
void BeepAdvance(void)
{
  const u8 *periodTable;

  if (g_note == 0) {
    return;
  }
  if (g_ui.periodsRemaining != 0) {
    g_ui.periodsRemaining--;
    if ((g_ui.periodsRemaining == 1) &&
        ((g_note->pitch & NOTE_PITCH_MASK) == NOTE_END)) {
      TW.TMRW.BYTE = TIMER_W_COMPARE_ONLY;
      TW.TIOR0.BYTE = TIMER_W_COMPARE_B_LOW;
      TW.TIOR1.BYTE = TIMER_W_COMPARE_C_LOW;
    }
    if (g_ui.periodsRemaining != 0) {
      return;
    }
  }
  if (g_ui.periodsRemaining != 0) {
    goto playNote;
  } else if (g_ui.separatorPeriodsRemaining == 0) {
    goto playNote;
  } else {
    TW.GRA = BEEPER_SEPARATOR_COMPARE;
    TW.GRB = BEEPER_SEPARATOR_COMPARE;
    TW.GRC = BEEPER_SEPARATOR_COMPARE;
    TW.TCNT = 0;

    if (g_ui.separatorPeriodsRemaining != 0) {
      g_ui.separatorPeriodsRemaining--;
    }
    return;
  }
playNote:
  if ((g_note->pitch & NOTE_PITCH_MASK) == NOTE_END) {
    g_note = 0;
    return;
  }
  /* Consume at most one tempo command. A repeat selects the shared RAM score
   * regardless of the current score's storage. */
  if ((g_note->pitch & NOTE_PITCH_MASK) == NOTE_SET_TEMPO) {
    g_ui.durationDivisor = g_note->duration;
    g_note++;
  }
  if ((g_note->pitch & NOTE_PITCH_MASK) == NOTE_REPEAT) {
    g_note = BEEPER_SEQUENCE_RAM;
    return;
  }
  /* Narrow the duration's clock budget to 16 bits, then divide by the raw
   * compare byte. Rest and other pitch indices read adjacent resident artwork
   * when they extend beyond the pitch table. */
  periodTable = g_residentResources.bytes;
  if ((g_note->pitch & NOTE_PITCH_MASK) == NOTE_REST) {
    g_ui.periodsRemaining =
        ((uint)((BEEPER_DURATION_CLOCK_SCALE * g_note->duration) /
                g_ui.durationDivisor) /
         periodTable[g_note->pitch & NOTE_PITCH_MASK]);
    BeepSetPeriod(0);
  } else {
    if ((g_note->pitch & NOTE_LEGATO) != 0) {
      g_ui.periodsRemaining =
          ((uint)((BEEPER_DURATION_CLOCK_SCALE * g_note->duration) /
                  g_ui.durationDivisor) /
           periodTable[g_note->pitch & NOTE_PITCH_MASK]);
      g_ui.separatorPeriodsRemaining = 0;
    } else {
      g_ui.periodsRemaining =
          ((uint)(((BEEPER_DURATION_CLOCK_SCALE * g_note->duration) /
                   g_ui.durationDivisor) -
                  BEEPER_SEPARATOR_COMPARE) /
           periodTable[g_note->pitch & NOTE_PITCH_MASK]);
      g_ui.separatorPeriodsRemaining = 1;
    }
    /* The preceding record's legato flag controls whether pitch is changed. */
    if ((g_note != BEEPER_SEQUENCE_RAM) &&
        ((g_note[-1].pitch & NOTE_LEGATO) != NOTE_LEGATO)) {
      TW.TMRW.BYTE = TIMER_W_PWM_BC;
      TW.TCRW.BYTE = TIMER_W_WATCH_CLEAR_A_B_HIGH;

      BeepSetPeriod(g_residentResources.bytes[g_note->pitch & NOTE_PITCH_MASK]);
    }
  }
  g_note++;
}

#pragma interrupt(TimerWInterrupt(vect = 35))
void TimerWInterrupt(void)
{
  BeepAdvance();
  TW.TSRW.BYTE &= 0xfe;
}

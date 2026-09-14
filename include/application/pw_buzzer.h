#ifndef PW_BUZZER_H
#define PW_BUZZER_H

#include "types.h"
#include "score.h"

/* The foreground enables playback while a score is selected and disables the
 * timer when it ends. */
u8 BeepHasScore(void);
void BeepInit(void);
/* IDs 0..15 select EEPROM entries. Loading overwrites the motion sample RAM.
 * EEPROM score records must have even length >=2. Output mode zero skips
 * loading entirely. */
void BeepLoadScore(u8 sequenceId);
/* Borrow a score until playback ends; its storage must stay valid. An external
 * score's first pitched note needs a readable predecessor, usually a leading
 * tempo record.
 * NOTE_REPEAT redirects playback to the shared RAM score. See score.h for
 * command sequencing. */
void BeepSelectScore(const Note *score);
void BeepEnableTimer(void);
/* Stop the timer, retaining the selected score and duration counters. */
void BeepDisableTimer(void);
/* Select the compare layout for subsequent period updates: 0 sets equal
 * compares, 1 sets B to A/2, and 2 sets B and C to A/2. Halving rounds down.
 * Playback continues with the current compare values until the next update. */
void BeepSetOutputMode(u8 mode);

#endif

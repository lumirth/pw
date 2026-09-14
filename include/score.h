#ifndef PW_SCORE_H
#define PW_SCORE_H

#include "types.h"

/* EEPROM directory slots selected by menus, activities and presentations.
 * Back feedback also marks a blocked choice; found feedback covers items and
 * Watts. */
#define SCORE_CONFIRM 0
#define SCORE_BACK 1
#define SCORE_MOVE 2
#define SCORE_RADAR_RESPONSE 3
#define SCORE_FAILURE 4
#define SCORE_FOUND 5
#define SCORE_COMPLETION 6
#define SCORE_SUCCESS 7
#define SCORE_PEER_PLAY 9
#define SCORE_BATTLE_ENCOUNTER 10
#define SCORE_HIT 11
#define SCORE_DODGE 12
#define SCORE_CRITICAL 13
#define SCORE_ESCAPE 14
#define SCORE_THROW 15

/* Note duration uses 24 units per quarter note; commands reuse that byte as
 * an argument. Pitch values index timer compare bytes. Bit 7 removes the
 * separator and retains the programmed pitch into the next ordinary note.
 * Tempo supplies the divisor used for duration arithmetic. */
typedef struct {
  u8 duration;
  u8 pitch;
} Note;

/* At each boundary the worker handles an end marker first, then consumes at
 * most one tempo command before a note or repeat. Tempo must be nonzero.
 * Repeat restarts the shared RAM score. For a pitched note outside that RAM's
 * first record, the preceding record's legato flag controls the pitch update.
 */
#define NOTE_THIRTY_SECOND 3
#define NOTE_SIXTEENTH 6
#define NOTE_PITCH_MASK 0x7F
#define NOTE_LEGATO 0x80
#define NOTE_SET_TEMPO 0x7B
#define NOTE_REST 0x7D
#define NOTE_REPEAT 0x7E
#define NOTE_END 0x7F

#endif

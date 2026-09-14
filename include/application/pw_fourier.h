#ifndef PW_FOURIER_H
#define PW_FOURIER_H

#include "types.h"

/* Add bins 0..31 from 64 signed samples to the shared magnitude accumulator.
 * The caller initializes that accumulator. Resets scratch reservations and
 * overwrites the first 256 bytes with two arrays of 64 signed words.
 */
void FftAccumulate(const volatile s8 *samples);

extern const s16 g_sineQ11[];

#endif

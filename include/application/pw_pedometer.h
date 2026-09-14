#ifndef PW_PEDOMETER_H
#define PW_PEDOMETER_H

#include "types.h"

/* Reset batch history when returning from another shared-workspace owner. */
void MotionReset(void);
/* Analyze 64 samples per axis and update the motion event. Accepted batches
 * restart step pacing. The FFT resets scratch and overwrites its first
 * 256 bytes with real and imaginary working samples. */
void MotionProcess(void);

#endif

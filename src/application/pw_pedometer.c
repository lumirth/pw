#include "types.h"
#include "project.h"
#include "application/pw_fourier.h"
#include "application/pw_pedometer.h"
#include "application/pw_power.h"

/* Clear batch history when motion takes ownership of the shared workspace. */
void MotionReset(void)
{
  g_work.motion.batch.resetOnlyByte = 0;
  g_work.motion.batch.pendingStepsQ9 = 0;
  g_work.motion.batch.stepFractionQ9 = 0;
  g_work.motion.batch.lastRejected = 0;
}

extern const u8 g_motionCandidateOrder[10];

s32 MotionEstimateQ9(u16 *spectrum);

#define MOTION_Q9_SHIFT 9
#define MOTION_FRACTION_MASK 0x1fful
#define MOTION_BIN13_Q9 0x1a00
#define MOTION_BIN14_Q9 0x1c00
#define MOTION_MIN_MAGNITUDE 0x200
#define MOTION_NO_CLASS 0xff

/* Magnitude-weighted bin position in Q9 (one bin = 512). Classes index bins
 * 5..14. Average the selected bin with its neighbors, using two bins at an
 * edge and three elsewhere. Magnitude totals use 16 bits; weighted products
 * widen before summing, and division truncates the remainder. */
s16 MotionCentroidQ9(u8 classIndex, u16 *candidateBins)
{
  u16 totalMagnitude;
  u32 weightedBinQ9;

  if (classIndex == 0) {
    totalMagnitude = candidateBins[classIndex] + candidateBins[classIndex + 1];
    weightedBinQ9 =
        (u32)candidateBins[classIndex] * ((classIndex + 5) << MOTION_Q9_SHIFT) +
        (u32)candidateBins[classIndex + 1] *
            ((classIndex + 6) << MOTION_Q9_SHIFT);
  } else if (classIndex == 9) {
    totalMagnitude = candidateBins[8] + candidateBins[9];
    weightedBinQ9 = (u32)candidateBins[8] * MOTION_BIN13_Q9 +
                    (u32)candidateBins[9] * MOTION_BIN14_Q9;
  } else {
    totalMagnitude = candidateBins[classIndex - 1] +
                     candidateBins[classIndex + 1] + candidateBins[classIndex];
    weightedBinQ9 =
        (u32)candidateBins[classIndex - 1] *
            ((classIndex + 4) << MOTION_Q9_SHIFT) +
        (u32)candidateBins[classIndex + 1] *
            ((classIndex + 6) << MOTION_Q9_SHIFT) +
        (u32)candidateBins[classIndex] * ((classIndex + 5) << MOTION_Q9_SHIFT);
  }
  return (weightedBinQ9 / totalMagnitude);
}

/* Analyze one 64-sample batch across all three axes. The accepted Q9 bin
 * position gives steps per batch; StepPacingTick spreads its whole steps
 * across subsequent samples. */
void MotionProcess(void)
{
  u16 bin;
  s32 stepsQ9;
  u8 view;

  g_state.events.byte &= EVENT_CLEAR(EVENT_MOTION);
  for (bin = 0; bin < 32; bin++) {
    g_work.motion.fftAccumulator[bin] = 0;
  }

  FftAccumulate(g_work.motion.x);
  FftAccumulate(g_work.motion.y);
  FftAccumulate(g_work.motion.z);
  stepsQ9 = MotionEstimateQ9(g_work.motion.fftAccumulator);

  view = g_state.view;
  /* Diagnostics count accepted walking batches, then sufficiently still
   * batches. Axis activity is the sum of sample-to-sample absolute changes. */
  if (view == VIEW_THRESHOLD_TEST) {
    if (g_ui.view.accel.walkingBatches <
        g_ui.view.accel.thresholds.walkingBatchTarget) {
      if (stepsQ9 != 0) {
        g_ui.view.accel.walkingBatches++;
        if (g_ui.view.accel.xActivity <
            g_ui.view.accel.thresholds.walkingActivityMin) {
          g_state.view = VIEW_THRESHOLD_FAILURE;
        }
        if (g_ui.view.accel.yActivity <
            g_ui.view.accel.thresholds.walkingActivityMin) {
          g_state.view = VIEW_THRESHOLD_FAILURE;
        }
        if (g_ui.view.accel.zActivity <
            g_ui.view.accel.thresholds.walkingActivityMin) {
          g_state.view = VIEW_THRESHOLD_FAILURE;
        }
        if (g_ui.view.accel.xActivity >
            g_ui.view.accel.thresholds.walkingActivityMax) {
          g_state.view = VIEW_THRESHOLD_FAILURE;
        }
        if (g_ui.view.accel.yActivity >
            g_ui.view.accel.thresholds.walkingActivityMax) {
          g_state.view = VIEW_THRESHOLD_FAILURE;
        }
        if (g_ui.view.accel.zActivity >
            g_ui.view.accel.thresholds.walkingActivityMax) {
          g_state.view = VIEW_THRESHOLD_FAILURE;
        }
      }
    } else if (g_ui.view.accel.stillBatches <
               g_ui.view.accel.thresholds.stillBatchTarget) {
      if ((g_ui.view.accel.xActivity <
           g_ui.view.accel.thresholds.stillActivityLimit) &&
          (g_ui.view.accel.yActivity <
           g_ui.view.accel.thresholds.stillActivityLimit) &&
          (g_ui.view.accel.zActivity <
           g_ui.view.accel.thresholds.stillActivityLimit)) {
        g_ui.view.accel.stillBatches++;
      }
    }
  }

  /* On rejection, clear pending credit and carry the fractional step forward.
   */
  if (stepsQ9 == 0) {
    g_work.motion.batch.pendingStepsQ9 = 0;
    return;
  }

  g_state.events.byte |= EVENT_MOTION;

  /* Credit pending Q9 steps immediately. The firmware clears this field;
   * its nonzero producer remains unresolved. */
  if (g_work.motion.batch.pendingStepsQ9 != 0) {
    g_work.motion.batch.stepFractionQ9 += g_work.motion.batch.pendingStepsQ9;
    g_work.motion.batch.pendingStepsQ9 = 0;
    g_state.stepPacing.batchSteps =
        ((s32)g_work.motion.batch.stepFractionQ9 >> MOTION_Q9_SHIFT);
    g_work.motion.batch.stepFractionQ9 &= MOTION_FRACTION_MASK;
    g_state.hourSteps = (g_state.hourSteps + g_state.stepPacing.batchSteps);
    if (g_state.hourSteps > HOURLY_STEPS_MAX) {
      g_state.hourSteps = HOURLY_STEPS_MAX;
    }
    g_state.dailySteps += g_state.stepPacing.batchSteps;
    if (g_state.dailySteps > DAILY_STEPS_MAX) {
      g_state.dailySteps = DAILY_STEPS_MAX;
    }
    StoreTotalSteps(g_state.save.totalSteps + g_state.stepPacing.batchSteps);
    g_state.save.stepsTowardNextWatt += g_state.stepPacing.batchSteps;
    if (g_state.save.stepsTowardNextWatt >= STEPS_PER_WATT) {
      g_state.save.stepsTowardNextWatt =
          (g_state.save.stepsTowardNextWatt - STEPS_PER_WATT);
      g_state.save.watts++;
      if (g_state.save.watts > WATTS_MAX) {
        g_state.save.watts = WATTS_MAX;
      }
    }
  }

  /* Carry fractions between accepted batches, then start pacing the new
   * whole-step budget halfway through the 64-sample phase interval. */
  g_work.motion.batch.stepFractionQ9 += stepsQ9;
  g_state.stepPacing.batchSteps =
      ((s32)g_work.motion.batch.stepFractionQ9 >> MOTION_Q9_SHIFT);
  g_work.motion.batch.stepFractionQ9 &= MOTION_FRACTION_MASK;
  if (g_state.stepPacing.batchSteps != 0) {
    g_state.idleSeconds[IDLE_MOTION] = ACTIVITY_MOTION_SECONDS;
  }
  g_state.stepPacing.stepsEmitted = 0;
  g_state.stepPacing.stepPhase = 0x20;
}

/* Select from bins 5..14 in the candidate order below. A replacement needs
 * magnitude >=512 and 2*candidate > 3*the current choice.
 * Compare that choice with the maximum in bins 1..29: after a rejection the
 * limit is (choice*4)/3; otherwise it is choice*2. Products and shifts use
 * unsigned 16-bit arithmetic, including wrap. */
s32 MotionEstimateQ9(u16 *spectrum)
{
  u16 spectrumMaximum;
  u16 candidateMagnitude;
  u16 *cursor;
  u16 *bins;
  u16 restartDivisor;
  u8 *candidate;
  struct {
    u16 magnitude;
    u8 classIndex;
    u8 candidateIndex;
  } walk;

  spectrumMaximum = 0;
  bins = spectrum;
  bins++;
  walk.candidateIndex = 0;
  cursor = bins;
  do {
    if (spectrumMaximum < *cursor) {
      spectrumMaximum = *cursor;
    }
    walk.candidateIndex++;
    cursor++;
  } while (walk.candidateIndex < 0x1d);

  candidateMagnitude = 0;
  walk.classIndex = MOTION_NO_CLASS;
  bins = spectrum;
  bins += 5;
  walk.candidateIndex = 0;
  do {
    walk.magnitude =
        bins[*(candidate = g_motionCandidateOrder + walk.candidateIndex)];
    if (walk.magnitude >= MOTION_MIN_MAGNITUDE) {
      if ((candidateMagnitude * 3) < (walk.magnitude * 2)) {
        walk.classIndex = *candidate;
        candidateMagnitude = walk.magnitude;
      }
    }
    walk.candidateIndex++;
  } while (walk.candidateIndex < 10);

  if (g_work.motion.batch.lastRejected != 0) {
    restartDivisor = 3;
    candidateMagnitude = ((candidateMagnitude << 2) / restartDivisor);
    if (spectrumMaximum > candidateMagnitude) {
      walk.classIndex = MOTION_NO_CLASS;
    }
  } else {
    if (spectrumMaximum > (candidateMagnitude << 1)) {
      walk.classIndex = MOTION_NO_CLASS;
    }
  }

  if (walk.classIndex == MOTION_NO_CLASS) {
    g_work.motion.batch.resetOnlyByte = 0;
    g_work.motion.batch.pendingStepsQ9 = 0;
    g_work.motion.batch.lastRejected = 1;
    return 0;
  }
  g_work.motion.batch.lastRejected = 0;
  return MotionCentroidQ9(walk.classIndex, spectrum + 5);
}

/* Class indices relative to FFT bin 5; this order determines replacements. */
const u8 g_motionCandidateOrder[10] = {0x03, 0x02, 0x04, 0x01, 0x05,
                                       0x00, 0x06, 0x07, 0x08, 0x09};

#include "types.h"
#include "project.h"
#include "application/pw_fourier.h"
#include "support/scratch.h"

#define FFT_N 64
#define FFT_LAST 63
#define FFT_HALF 32
#define FFT_AXIS_WORK_BYTES (FFT_N * sizeof(s16))
#define Q11_SHIFT 11

#define FFT_PRODUCT(a, b) ((s32)(a) * (b))

/* 64-point radix-2 DIT FFT of one signed 8-bit accelerometer axis. Real inputs
 * are sign-extended samples and imaginary inputs start at zero. After six
 * stages, add |real| + |imaginary| for bins 0..31 to the shared accumulator.
 * Real and imaginary arrays reuse the first 256 bytes of scratch. */
void FftAccumulate(const volatile s8 *samples)
{
  s16 *real;
  s16 *imaginary;
  u8 i;
  u8 j;
  u8 halfSpan;
  u8 span;
  u8 phaseStep;
  u8 twiddle;
  u8 partner;
  s16 cosineQ11;
  s16 sineQ11;
  s16 rotatedReal;
  s16 rotatedImaginary;
  u16 bin;

  ScratchReset();
  real = ScratchAlloc(FFT_AXIS_WORK_BYTES);
  imaginary = ScratchAlloc(FFT_AXIS_WORK_BYTES);

  {
    u8 n;
    for (n = 0; n < FFT_N; n++) {
      real[n] = samples[n];
    }
  }

  {
    u8 n;
    s16 zero;
    zero = 0;
    n = 0;
    do {
      imaginary[n] = zero;
      n++;
      imaginary[n] = zero;
      n++;
    } while (n < FFT_N);
  }

  /* Bit-reversed input order allows each later butterfly to stay in place. */
  j = 0;
  for (i = 1; i < FFT_LAST; i++) {
    for (partner = FFT_HALF; partner > (j ^= partner); partner >>= 1) {
    }
    if (i < j) {
      real[j] ^= real[i];
      real[i] ^= real[j];
      real[j] ^= real[i];
    }
  }

  /* Positive sine gives the positive-angle transform. Scale Q11 twiddle
   * products back to sample units; each stage leaves the FFT unnormalized. */
  phaseStep = FFT_N;
  for (halfSpan = 1; (span = (halfSpan * 2)) <= FFT_N; halfSpan = span) {
    phaseStep >>= 1;
    for (j = twiddle = 0; j < halfSpan; j++, twiddle += phaseStep) {
      cosineQ11 = g_sineQ11[(u16)twiddle + 16];
      sineQ11 = g_sineQ11[twiddle];
      for (i = j; i < FFT_N; i += span) {
        partner = (i + halfSpan);
        if (twiddle == 0) {
          rotatedReal = real[partner];
          rotatedImaginary = imaginary[partner];
        } else if (twiddle == 16) {
          rotatedReal = -imaginary[partner];
          rotatedImaginary = real[partner];
        } else if (imaginary[partner] == 0) {
          rotatedReal = ((FFT_PRODUCT(real[partner], cosineQ11)) >> Q11_SHIFT);
          rotatedImaginary =
              ((FFT_PRODUCT(real[partner], sineQ11)) >> Q11_SHIFT);
        } else {
          /* Scale each complete complex component once, after the wide sum. */
          rotatedReal = ((FFT_PRODUCT(real[partner], cosineQ11) -
                          FFT_PRODUCT(imaginary[partner], sineQ11)) >>
                         Q11_SHIFT);
          rotatedImaginary = ((FFT_PRODUCT(imaginary[partner], cosineQ11) +
                               FFT_PRODUCT(real[partner], sineQ11)) >>
                              Q11_SHIFT);
        }
        real[partner] = (real[i] - rotatedReal);
        imaginary[partner] = (imaginary[i] - rotatedImaginary);
        real[i] = (real[i] + rotatedReal);
        imaginary[i] = (imaginary[i] + rotatedImaginary);
      }
    }
  }

  for (bin = 0; bin < FFT_HALF; bin++) {
    g_work.motion.fftAccumulator[bin] +=
        ((real[bin] >= 0 ? real[bin] : -real[bin]) +
         (imaginary[bin] >= 0 ? imaginary[bin] : -imaginary[bin]));
  }
}

/* sin(2*pi*i/64) * 2048. The extra quarter-cycle lets i+16 supply cosine. */
const s16 g_sineQ11[80] = {
    0,     201,   400,   595,   784,   965,   1138,  1299,  1448,  1583,
    1703,  1806,  1892,  1960,  2009,  2038,  2048,  2038,  2009,  1960,
    1892,  1806,  1703,  1583,  1448,  1299,  1138,  965,   784,   595,
    400,   201,   0,     -201,  -400,  -595,  -784,  -965,  -1138, -1299,
    -1448, -1583, -1703, -1806, -1892, -1960, -2009, -2038, -2048, -2038,
    -2009, -1960, -1892, -1806, -1703, -1583, -1448, -1299, -1138, -965,
    -784,  -595,  -400,  -201,  0,     201,   400,   595,   784,   965,
    1138,  1299,  1448,  1583,  1703,  1806,  1892,  1960,  2009,  2038};

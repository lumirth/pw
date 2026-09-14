#ifndef PW_BUILTIN_H
#define PW_BUILTIN_H

#include "types.h"
#include "raster_column.h"
#include <stddef.h>

/* Audio indexes this storage as bytes, including the artwork after its
 * 42 compare values. Rest code 0x7D reads walkerImage[0x53], whose value 2
 * determines rest timing. Keep the assets within the same bounded object. */
typedef struct {
  u8 noteCompareValues[42];
  u8 walkerImage[RASTER_BYTES(32, 32)];
  u8 neutralFace[RASTER_BYTES(16, 8)];
  u8 smileFace[RASTER_BYTES(16, 8)];
  u8 frownFace[RASTER_BYTES(16, 8)];
  u8 buttonArrow[RASTER_BYTES(8, 8)];
  u8 irSignal[RASTER_BYTES(8, 8)];
  /* 36 monochrome 3x8 cells: digits, then uppercase letters. */
  u8 alphanumericFont[36 * 3];
} ResidentAssets;

typedef union {
  ResidentAssets assets;
  u8 bytes[sizeof(ResidentAssets)];
} ResidentResources;

typedef char
    ResidentWalkerStart[offsetof(ResidentAssets, walkerImage) == 42 ? 1 : -1];
typedef char ResidentFontStart[offsetof(ResidentAssets, alphanumericFont) == 426
                                   ? 1
                                   : -1];
typedef char ResidentResourcesSize[sizeof(ResidentResources) == 534 ? 1 : -1];

extern const ResidentResources g_residentResources;

#endif

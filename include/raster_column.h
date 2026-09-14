#ifndef PW_RASTER_COLUMN_H
#define PW_RASTER_COLUMN_H

#include "types.h"

/* One page covers eight vertical pixels. Each column stores the high then
 * low bit plane, with the upper pixel at bit zero. */
#define RASTER_PAGE_HEIGHT 8
#define RASTER_BYTES(width, height) ((width) * (height) / 4)

typedef u8 RasterColumn[2];

#endif

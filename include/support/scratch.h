#ifndef PW_SCRATCH_H
#define PW_SCRATCH_H

#include "types.h"

/* Rewind the allocation cursor. Existing bytes remain until another scratch
 * user or workspace owner overwrites them. Callers retaining a pointer across
 * a reset must account for the regions written by each intervening helper. */
void ScratchReset(void);
/* Reserve consecutive bytes and advance the cursor by byteCount. Callers
 * provide initialization and alignment. Crossing the 1,024-byte allocation
 * window sleeps after advancing the cursor; waking returns the requested
 * pointer. Large artwork transfers directly use the 1,536-byte backing store.
 */
void *ScratchAlloc(u16 byteCount);

#endif

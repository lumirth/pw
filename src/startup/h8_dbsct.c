#include "types.h"
/* Runtime section tables derive ROM and RAM bounds from __sectop and __secend.
 * Startup copies initialized D data into R and clears B. */
#pragma section $DSEC
static const struct {
  u8 *romS; /* ROM copy source. */
  u8 *romE; /* End of ROM data. */
  u8 *ramS; /* RAM copy destination. */
} DTBL[] = {
    {__sectop("D"), __secend("D"), __sectop("R")},
};
#pragma section $BSEC
static const struct {
  u8 *bS; /* Start of RAM to clear. */
  u8 *bE; /* End of RAM to clear. */
} BTBL[] = {
    {__sectop("B"), __secend("B")},
};
#pragma section

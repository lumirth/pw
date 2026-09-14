#ifndef PW_DOWSING_H
#define PW_DOWSING_H

#include "types.h"

/* Start two attempts at one hidden patch. Rendering spends the reveal's frame
 * budget; update then resolves the reward. Course items are stored after
 * acknowledgment, while bonus items and Watts are stored on reveal. */
void DowsingInit(void);
void DowsingUpdate(void);
void DowsingRender(void);

#endif

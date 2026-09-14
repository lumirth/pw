#ifndef PW_MAIN_H
#define PW_MAIN_H

#include "types.h"

void RenderIrRomFrame(u8 signalIconRequested);
void RenderIrUploadedFrame(u8 signalIconRequested);
/* Collect one sample, dispatch view/motion/RTC work in priority order, then
 * pace steps. Views can hand the shared workspace to IR or sound playback. */
void MainTick(void);

extern const u8 g_socialBubbles[];

#endif

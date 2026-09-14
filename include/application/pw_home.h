#ifndef PW_HOME_H
#define PW_HOME_H

#include "types.h"

/* Select the foreground function called by the main loop. */
void InstallTask(void (*nextTarget)(void));
/* From MainTick, prepare both display banks and hand the workspace to IR. */
void TryBeginIr(void);
void SetView(u8 nextView);
/* Draw a 64-by-48 raster using the home/presentation frame byte. Nonzero
 * selects frame zero; zero alternates frames every two UI refreshes. Resets
 * scratch and reads the course artwork from EEPROM. */
void RenderLargePokemon(u8 x, u8 y);
void HomeInit(void);
void HomeUpdate(void);
void RenderFeeling(u8 recordIndex);
/* Draw the home scene and advance its entry or walking animation. */
void HomeRender(void);

#endif

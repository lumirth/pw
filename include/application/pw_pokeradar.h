#ifndef PW_POKERADAR_H
#define PW_POKERADAR_H

#include "types.h"

/* Choose an encounter and retain it for battle in the shared view bank.
 * Updates count the search windows and pauses; rendering advances the closing
 * screen transition. Active scores suspend input and the update countdowns.
 */
void RadarInit(void);
void RadarUpdate(void);
void RadarRender(void);
void RadarFailureUpdate(void);
void RadarFailureRender(void);

#endif

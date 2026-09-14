#ifndef PW_PLAYER_INPUT_H
#define PW_PLAYER_INPUT_H

#include "types.h"

#define BUTTON_CENTER 0x02
#define BUTTON_LEFT 0x04
#define BUTTON_RIGHT 0x08
#define BUTTON_ANY 0x0E

void InputInit(void);
/* Poll buttons and replace edge state. Button edges refresh the display
 * timeout; eight held-center scans after a wake enter interactive mode.
 * Edge presses are suppressed while the display is asleep. */
void InputScan(void);
/* Return requested press bits from the edge state retained by InputScan. */
u8 InputPressed(u8 requestedMask);

#endif

#ifndef PW_FRIEND_H
#define PW_FRIEND_H

#include "types.h"

/* Apply the received peer session: award its gift, rotate EEPROM history,
 * append an item-gift diary entry and start the presentation sequence. */
void PeerFinalize(void);
/* The peer presentation advances in render calls; update has no work. */
void PeerUpdate(void);
void PeerRender(void);
/* Read EEPROM history slots 1..10; consumes the IR EEPROM scratch buffer. */
u8 SeenPeer(u8 *deviceId);

#endif

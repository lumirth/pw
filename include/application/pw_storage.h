#ifndef PW_STORAGE_H
#define PW_STORAGE_H

#include "types.h"

#define RESET_KEEP_EVENTS 0
#define RESET_CLEAR_EVENTS 1
#define RESET_KEEP_LIFETIME 0
#define RESET_CLEAR_LIFETIME 1

/* Always clears pairing/session state, return inventory, weekly steps and peer
 * history. The independent policies also clear event receipts/resources and
 * lifetime progress. Uses the IR payload buffer, invalidating its old content.
 */
void PersistentReset(u8 clearEvents, u8 clearLifetime);
/* Restore save mirrors and pairing flags, resetting invalid signature state
 * and completing a pending staged-walk copy. Uses the shared work buffers. */
void BootRestore(void);
void RtcRestore(void);

#endif

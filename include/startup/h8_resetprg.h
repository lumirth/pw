#ifndef PW_H8_RESETPRG_H
#define PW_H8_RESETPRG_H

#include "types.h"

/* Apply the queued communication action, choose the next view, then give
 * shared storage back to motion and resume MainTick. Queued actions run even
 * when the communication result reports an error. */
void IrComplete(void);
/* Apply rollover hours below 24 and nonzero RTC seconds from the received
 * status payload. Uses the IR status workspace. */
void StatusApplyTime(void);
/* Activate staged resource and trainer pages in EEPROM, reusing one scratch
 * page. The walk-commit marker makes this copy repeatable during boot recovery.
 */
void CommitStagedWalk(void);

#endif

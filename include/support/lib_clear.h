#ifndef PW_LIB_CLEAR_H
#define PW_LIB_CLEAR_H

#include "types.h"

/* Clear captured Pokemon, ordinary items and friend gifts in EEPROM_WALK. */
void ClearReturnInventory(void);
/* Empty every diary slot by clearing its action byte. */
void ClearDiaryActions(void);
/* Clear the seven daily-step totals in EEPROM_WALK. */
void ClearWeeklySteps(void);

#endif

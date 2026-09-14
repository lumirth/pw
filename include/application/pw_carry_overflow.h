#ifndef PW_CARRY_OVERFLOW_H
#define PW_CARRY_OVERFLOW_H

#include "types.h"

#define PW_DISCARD_INVENTORY_POKEMON 0
#define PW_DISCARD_INVENTORY_ITEM 1

/* The caller selects inventoryKind and retains the reward's sourceSelection:
 * Pokemon encounters are one-based, course items zero-based. Initialization
 * selects the middle slot; confirming later replaces that inventory entry.
 */
void DiscardInit(void);

void DiscardUpdate(void);
void DiscardRender(void);

#endif

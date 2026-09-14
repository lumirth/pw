#ifndef PW_BATTLE_H
#define PW_BATTLE_H

#include "types.h"

/* Keep radar's encounter selection in the shared view bank. Update resolves
 * phases, input and rewards; render advances each phase's animation counter.
 */
void BattleInit(void);
void BattleUpdate(void);
void BattleRender(void);

#endif

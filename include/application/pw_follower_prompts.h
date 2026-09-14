#ifndef PW_FOLLOWER_PROMPTS_H
#define PW_FOLLOWER_PROMPTS_H

#include "application/pw_follower_events.h"

void SocialUpdate(void);
void SocialRender(void);
/* May consume the pending check and install a timed home offer. Uses EEPROM
 * and replaces scratch storage while checking inventory and friendship. */
void SocialOfferCheck(void);

extern const SocialFrame g_socialItemSequence[];
extern const SocialFrame g_socialWatts50Sequence[];
extern const SocialFrame g_socialWatts20Sequence[];
extern const SocialFrame g_socialWatts10Sequence[];
extern const SocialFrame g_socialBoredSequence[];
extern const SocialFrame g_socialNewPokemonSequence[];

#endif

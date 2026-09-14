#ifndef PW_CUTSCENES_H
#define PW_CUTSCENES_H

#include "types.h"

/* Stage encodings belong to each view; frame counts render calls. */
#define WALK_START_DROP 0
#define WALK_START_CLOUD 1
#define WALK_START_POKEMON 2
#define WALK_START_COMPLETE 3

#define WALK_END_RISE 0
#define WALK_END_CLOUD 1
#define WALK_END_POKEMON 2
#define WALK_END_COMPLETE 3
#define WALK_END_BEGIN 5
#define WALK_END_COLLECTION_BEGIN 6
#define WALK_END_COLLECTION_COMPLETE 7

#define EVENT_REWARD_DROP 0
#define EVENT_REWARD_CLOUD 1
#define EVENT_REWARD_INFO 3
#define EVENT_REWARD_SPARKLE 4

/* IR completion and the reward display use these same received-reward IDs. */
#define PW_EVENT_REWARD_POKEMON 0
#define PW_EVENT_REWARD_COURSE 1
#define PW_EVENT_REWARD_ITEM 2
#define PW_EVENT_REWARD_MAP 3
#define PW_EVENT_REWARD_STAMP0 4
#define PW_EVENT_REWARD_STAMP1 5
#define PW_EVENT_REWARD_STAMP2 6
#define PW_EVENT_REWARD_STAMP3 7

void WalkStartUpdate(void);
void WalkStartRender(void);
void EventRewardUpdate(void);
void EventRewardRender(void);
void WalkEndUpdate(void);
void WalkEndRender(void);

#endif

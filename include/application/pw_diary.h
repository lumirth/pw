#ifndef PW_DIARY_H
#define PW_DIARY_H

#include "types.h"
#include "records.h"
#include "resources.h"

/* Actions 1..10 encode peer item index + 1 and preserve caller-filled peer
 * fields. Device events above 10 clear the record before adding walk context.
 */
#define PW_DIARY_ACTION_PREFILLED_MAX 0x0a
#define PW_DIARY_ACTION_DOWSING_ITEM 0x0b
#define PW_DIARY_ACTION_BONUS_COURSE_ITEM 0x0c
#define PW_DIARY_ACTION_CAPTURED_ROUTE_POKEMON 0x0d
#define PW_DIARY_ACTION_CAPTURED_EVENT_POKEMON 0x0e
#define PW_DIARY_ACTION_BATTLE_FLED 0x0f
#define PW_DIARY_ACTION_BATTLE_DEFEATED 0x10
#define PW_DIARY_ACTION_WALK_STARTED 0x19
#define PW_DIARY_ACTION_PERIODIC_STEPS 0x1b
#define DIARY_ACTION_RECEIVED_ITEM 0x1c
#define DIARY_ACTION_RECEIVED_POKEMON 0x1d

#define DIARY_BONUS_ENCOUNTER 4

/* encounterSelector: 0 performs no encounter lookup, 1..3 selects a course
 * encounter, and 4 reads the bonus/event records. itemIdLe retains its
 * serialized bytes. May replace diary contents, write EEPROM, and advance and
 * persist the save cursor. Uses caller-provided buffers and preserves scratch
 * allocations.
 */
void DiaryAppend(Course *course, DiaryEntry *diary, u8 actionId,
                 u8 useBonusCourse, u8 encounterSelector, u16 itemIdLe);

#endif

#ifndef PW_TRAINER_H
#define PW_TRAINER_H

#include "types.h"

#define TRAINER_PAGE_CARD 0
#define TRAINER_PAGE_STEP_CAP 1
#define TRAINER_PAGE_REWARD 2

/* Zero displays the current card; 1..7 select dailySteps[index - 1]. */
#define TRAINER_CURRENT_DAY 0
#define TRAINER_HISTORY_DAYS 7

void TrainerInit(void);
void TrainerUpdate(void);
void TrainerRender(void);

#endif

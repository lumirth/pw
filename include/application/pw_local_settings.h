#ifndef PW_LOCAL_SETTINGS_H
#define PW_LOCAL_SETTINGS_H

#include "types.h"

#define SETTINGS_SELECT_VOLUME 0
#define SETTINGS_SELECT_CONTRAST 1

/* An edit page is the selected setting plus one. */
#define SETTINGS_PAGE_SELECT 0
#define SETTINGS_PAGE_VOLUME 1
#define SETTINGS_PAGE_CONTRAST 2

void SettingsInit(void);
/* Apply volume/contrast to hardware while editing; confirmation saves both
 * EEPROM mirrors and returns home. */
void SettingsUpdate(void);
void SettingsRender(void);

#endif

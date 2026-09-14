#ifndef PW_PICTOGRAM_MENU_H
#define PW_PICTOGRAM_MENU_H

#include "types.h"

/* These ordinals select the menu labels, icons and watt-cost table. */
#define MENU_RADAR 0
#define MENU_DOWSING 1
#define MENU_CONNECT 2
#define MENU_TRAINER 3
#define MENU_INVENTORY 4
#define MENU_SETTINGS 5
#define MENU_COUNT 6

#define MENU_ERROR_NONE 0
#define MENU_ERROR_WATTS 1
#define MENU_ERROR_NO_POKEMON 2
#define MENU_ERROR_EMPTY_INVENTORY 3

void MenuReset(void);
/* Charge and persist the selected activity's Watt cost before entering it. */
void MainMenuUpdate(void);
void MainMenuRender(void);

#endif

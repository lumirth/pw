#include "types.h"
#include "project.h"

u8 g_resetData[4] = {0x00, 0x00, 0x00, 0x00};

RuntimeState g_state;
const Note *g_note;
PresentationState g_ui;
void (*g_task)(void);
void (*g_previousTask)(void);
DisplayBank g_displayBank;

#ifndef UI_DOCK
#define UI_DOCK

#include <utilitieslib/network/net_typedefs.h>

// This is the new dock window, it does everything on its own
// the animations make this hideously complicated
//
int dock_draw();
void dock_open();
void dock_openAtMouse(void);

#endif
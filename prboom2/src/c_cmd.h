#ifndef __C_CMD__
#define __C_CMD__

#include "doomtype.h"
#include "d_event.h"

void C_ConsoleCommand(char* cmd);
const char* C_NavigateCommandHistory(int direction);
void C_ResetCommandHistoryPosition();
void C_ConsolePrintf(const char *s, ...);
const char* C_GetMessage();
void C_ClearMessage();
dboolean C_HasMessage();
dboolean C_Responder();
dboolean C_RegisterBind(int keycode, char* cmd, evtype_t type);
dboolean C_UnregisterBind(int keycode, evtype_t type);
void C_SaveSettings();
void C_LoadSettings();

typedef struct _command {
    const char* name;
    void (*func)(char*);
} command;

extern dboolean c_drawpsprites;
#endif

#ifndef __C_CMD__
#define __C_CMD__

#include "doomtype.h"

void C_ConsoleCommand(char* cmd);
const char* C_NavigateCommandHistory(int direction);
void C_ResetCommandHistoryPosition();

typedef struct _command {
    const char* name;
    void (*func)(char*);
} command;

extern dboolean c_drawpsprites;
#endif

#ifndef __C_CMD__
#define __C_CMD__

void C_ConsoleCommand(char* cmd);

typedef struct _command {
    char* name;
    void (*func)(char*);
} command;

#endif

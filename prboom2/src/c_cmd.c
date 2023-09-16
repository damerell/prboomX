#include "c_cmd.h"
#include "doomstat.h"
#include "g_game.h"
#include "r_data.h"
#include "p_inter.h"
#include "p_tick.h"
#include "m_cheat.h"
#include "m_argv.h"
#include "s_sound.h"
#include "sounds.h"
#include "dstrings.h"
#include "r_main.h"
#include "p_map.h"
#include "d_deh.h" 
#include "p_tick.h"
#include "e6y.h" // G_GotoNextLevel()
#include "umapinfo.h"
#include "hu_stuff.h"
#include "lprintf.h"
#include "m_io.h"

#include <time.h>

extern char *basesavegame;
extern void M_QuitResponse(int ch);
extern const char * const ActorNames[];

#define plyr (players+consoleplayer)
#define CONSOLE_COMMAND_HISTORY_LEN (16)
static char* command_history[CONSOLE_COMMAND_HISTORY_LEN] = { 0 };
static char console_message[HU_MSGWIDTH];

static int KeyNameToKeyCode(const char* name);

dboolean c_drawpsprites = true;

static int C_SendCheat(const char* cheat)
{
    int ch = 0;
    M_FindCheats(' ');
    for (int i=0; i < strlen(cheat); i++) {
        ch |= M_FindCheats(tolower(cheat[i]));
    }
    return ch;
}

static void C_noclip(char* cmd)
{
  plyr->message = (plyr->cheats ^= CF_NOCLIP) & CF_NOCLIP ?
    s_STSTR_NCON : s_STSTR_NCOFF;
}

static void C_noclip2(char* cmd)
{
    if (plyr->mo != NULL) {
        /* if both are on, turn off. otherwise on */
        if ((plyr->cheats & (CF_NOCLIP | CF_FLY)) == (CF_NOCLIP | CF_FLY)) {
            plyr->message = "NOCLIP2 DISABLED";
            plyr->mo->flags &= ~MF_NOGRAVITY;
            plyr->mo->flags &= ~MF_FLY;
            plyr->cheats &= ~CF_NOCLIP;
        } else {
            plyr->message = "NOCLIP2 ENABLED";
            plyr->cheats |= CF_NOCLIP;
            plyr->cheats |= CF_FLY;
            plyr->mo->flags |= MF_NOGRAVITY;
            plyr->mo->flags |= MF_FLY;
        }
    }
}

static void C_resurrect(char* cmd)
{
  if (plyr->playerstate == PST_DEAD) {
      plyr->message = "You live...again!";
      plyr->playerstate = PST_LIVE;
      plyr->health = initial_health;
      plyr->mo->health = initial_health;
      plyr->readyweapon = wp_fist;
      plyr->pendingweapon = wp_fist;
      plyr->mo->flags = MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_PICKUP|MF_NOTDMATCH;
      plyr->mo->radius = 16*FRACUNIT;
      plyr->mo->height = 56*FRACUNIT;
  } else {
      plyr->message = "Your life force throbs on...";
  }
}

static void C_god(char* cmd)
{
  if (plyr->playerstate == PST_DEAD)
    {
      signed int an;
      mapthing_t mt = {0};

      P_MapStart();
      mt.x = plyr->mo->x >> FRACBITS;
      mt.y = plyr->mo->y >> FRACBITS;
      mt.angle = (plyr->mo->angle + ANG45/2)*(uint_64_t)45/ANG45;
      mt.type = consoleplayer + 1;
      mt.options = 1; // arbitrary non-zero value
      P_SpawnPlayer(consoleplayer, &mt);

      // spawn a teleport fog
      an = plyr->mo->angle >> ANGLETOFINESHIFT;
      P_SpawnMobj(plyr->mo->x+20*finecosine[an], plyr->mo->y+20*finesine[an], plyr->mo->z, MT_TFOG);
      S_StartSound(plyr, sfx_slop);
      P_MapEnd();
    }
  plyr->cheats ^= CF_GODMODE;
  if (plyr->cheats & CF_GODMODE)
    {
      if (plyr->mo)
        plyr->mo->health = god_health;  // Ty 03/09/98 - deh

      plyr->health = god_health;
      plyr->message = s_STSTR_DQDON; // Ty 03/27/98 - externalized
    }
  else
    plyr->message = s_STSTR_DQDOFF; // Ty 03/27/98 - externalized
}

static void C_nextlevel(char* cmd)
{
    G_ExitLevel();
}

static void C_nextlevelsecret(char* cmd)
{
    G_SecretExitLevel();
}

static void C_printcmd(char* cmd)
{
    if(cmd)
        doom_printf("%s", cmd);
}

static void C_kill(char* cmd)
{
    int i;
    if (!cmd) return;
    /* strip spaces */
    while(isspace(*cmd)) cmd++;
    char* end = &cmd[strlen(cmd)];
    while(isspace(*(end-1))) end--;

    /* find actor matching cmd */
    for (i=0; ActorNames[i]; i++)
        if (stricmp(cmd,ActorNames[i]) == 0)
            break;

    if (!ActorNames[i]) {
        doom_printf("Actor type %s not found.", cmd);
        return;
    }

    int killcount=0;
    thinker_t *currentthinker = NULL;
    extern void A_PainDie(mobj_t *);

    P_MapStart();
    while ((currentthinker = P_NextThinker(currentthinker,th_all)) != NULL) {
        if (currentthinker->function == P_MobjThinker &&
                (((mobj_t *) currentthinker)->flags & MF_COUNTKILL ||
                 ((mobj_t *) currentthinker)->type == MT_SKULL)) {
            if ((((mobj_t *) currentthinker)->health > 0) && (((mobj_t *) currentthinker)->type == i)) {
                killcount++;
                P_DamageMobj((mobj_t *)currentthinker, NULL, NULL, 10000);
            }
        }
    }
    P_MapEnd();
    // killough 3/22/98: make more intelligent about plural
    // Ty 03/27/98 - string(s) *not* externalized
    doom_printf("%d thing%s of type %s Killed", killcount, killcount==1 ? "" : "s", ActorNames[i]);

}

static void C_quit(char* cmd)
{
    M_QuitResponse('y');
}

static void C_togglepsprites(char* cmd)
{
    c_drawpsprites = !c_drawpsprites;
    doom_printf("Draw player sprites %s", (c_drawpsprites ? "on" : "off"));
}

static void C_sndvol(char* cmd)
{
    if (cmd && *cmd) {
        int vol = atoi(cmd);
        vol = MIN(vol,15);
        vol = MAX(vol, 0);
        S_SetSfxVolume(vol);
    }
    doom_printf("Sound volume: %d/15", snd_SfxVolume);
}

static void C_musvol(char* cmd)
{
    if (cmd && *cmd) {
        int vol = atoi(cmd);
        vol = MIN(vol,15);
        vol = MAX(vol, 0);
        S_SetMusicVolume(vol);
    }
    doom_printf("Music volume: %d/15", snd_MusicVolume);
}

extern dboolean plat_skip;
dboolean allmap_always = false;
static void C_jds(char* cmd)
{
    extern dboolean buddha;
    static unsigned char jds = false;
    static int had_allmap = false;
    jds = !jds;
    if (jds) {
        buddha = true;
        plat_skip = true;
        had_allmap = plyr->powers[pw_allmap];
        plyr->powers[pw_allmap] = true;
        allmap_always = true;
    } else {
        buddha = false;
        plat_skip = false;
        allmap_always = false;
        plyr->powers[pw_allmap] = had_allmap;
    }
    doom_printf("JDS mode %s", jds ? "on" : "off");
}

static void C_platskip(char* cmd)
{
    plat_skip = !plat_skip;
}

static void C_map(char* cmd)
{
    char buf[9] = { 0 };
    int bufwr = 7;
    int len = strlen(cmd);
    for (int i = len - 1; i >= 0; i--) {
        if (cmd[i] >= '0' && cmd[i] <= '9') {
            buf[bufwr--] = cmd[i];
        }
    }
    if (strlen(&buf[bufwr+1]) == 1) {
        buf[bufwr--] = '0';
    }
    if (buf[bufwr+1]) {
        char clev[18];
        snprintf(clev, 18, "IDCLEV%s", &buf[bufwr+1]);
        C_SendCheat(clev);
    }
}

typedef struct
{
    char* give;
    char* cheat;
} cheat_map_t;

/* map of give to cheat */
static cheat_map_t cheatmap[] = {
    {"allmap", "idbeholda"},
    {"visor", "idbeholdl"},
    {"invuln", "idbeholdv"},
    {"invulnerability", "idbeholdv"},
    {"invisibility", "idbeholdi"},
    {"radsuit", "idbeholdr"},
    {"keys", "tntka"},
    {"health", "idbeholdh"},
    {"ammo", "idfa"},
    {"all", "idkfa"},
    {"armor", "idbeholdm"},
    {"armour", "idbeholdm"},
    {"redkey", "tntkeyrc"},
    {"redskull", "tntkeyrs"},
    {"bluekey", "tntkeybc"},
    {"blueskull", "tntkeybs"},
    {"yellowkey", "tntkeyyc"},
    {"yellowskull", "tntkeyys"},
    {"bullets", "tntammo1"},
    {"shells", "tntammo2"},
    {"rockets", "tntammo3"},
    {"cells", "tntammo4"},
    {"backpack", "tntammob"},
    {"berserk", "idbeholds"},
    {"chainsaw", "tntweap8"},
    {"shotgun", "tntweap3"},
    {"chaingun", "tntweap4"},
    {"rocketlauncher", "tntweap5"},
    {"plasmagun", "tntweap6"},
    {"plasmarifle", "tntweap6"},
    {"bfg", "tntweap7"},
    {"bfg9000", "tntweap7"},
    {"supershotgun", "tntweap9"},
    {"ssg", "tntweap9"},
    {"doubleshotgun", "tntweap9"},
    {"shotgun2", "tntweap9"},
    {NULL, NULL}
};


static void C_give(char* cmd)
{
    char* giveme = strtok(cmd, " ");
    int i = 0;
    while (giveme) {
        for (i = 0; cheatmap[i].cheat; i++) {
            if (stricmp(giveme, cheatmap[i].give) == 0) {
                C_SendCheat(cheatmap[i].cheat);
                break;
            }
        }
        if (!cheatmap[i].cheat) {
            C_ConsolePrintf("Did not find give cheat: %s", giveme);
        }
        giveme = strtok(NULL, " ");
    }
}

extern hu_textline_t  w_title;
static void C_note(char* cmd)
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    FILE* f;
    char* notefile = (char*) malloc(sizeof(char)*(20));
    static dboolean first_note = true;
    if (!cmd || !*cmd) {
        doom_printf("Invalid note command: Supply a message to write.");
    }

    if (notefile) {
        snprintf(notefile, 20, "notes_%04d%02d%02d.txt", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
        f = M_fopen(notefile,"a");
        if (f) {
            if (first_note) {
                fprintf(f, "=== %02d:%02d:%02d\n", tm.tm_hour, tm.tm_min, tm.tm_sec);
                for (int j = 0; j < numwadfiles; j++) {
                    /* ignore GWA files */
                    if (stricmp(strrchr(wadfiles[j].name,'.'), ".gwa")) {
                        fprintf(f, "%s\n", wadfiles[j].name);
                    }
                }
                first_note = false;
            } else {
                fprintf(f, "%02d:%02d:%02d\n", tm.tm_hour, tm.tm_min, tm.tm_sec);
            }
            if (gamestate == GS_LEVEL && gamemap > 0) {
                /* Fixme: Ideally we should use the same code as the
                 * automap widget generator, but that needs to be
                 * refactored to its own function. Will do that later
                 */
                char* mapname = w_title.l;
                if (mapname) {
                    while (*mapname) {
                        fprintf(f, "%c", toupper(*mapname));
                        mapname++;
                    }
                    fprintf(f, "\n");
                }
            }
            fprintf(f, "Position (%d,%d,%d)\tAngle %-.0f\n\n",
                    players[consoleplayer].mo->x >> FRACBITS,
                    players[consoleplayer].mo->y >> FRACBITS,
                    players[consoleplayer].mo->z >> FRACBITS,
                    players[consoleplayer].mo->angle * (90.0/ANG90));
            fprintf(f, "%s", cmd);
            fprintf(f, "\n\n");
            fclose(f);
            doom_printf("Note written to %s", notefile);
        } else {
            doom_printf("Couldn't open note file %s", notefile);
        }
    }
    free(notefile);
}


static void C_mdk(char* cmd)
{
    extern void P_LineAttack(mobj_t* t1, angle_t angle, fixed_t distance, fixed_t slope, int damage);
    fixed_t bulletslope = finetangent[(ANG90 - plyr->mo->pitch) >> ANGLETOFINESHIFT];
    P_LineAttack(plyr->mo, plyr->mo->angle, MISSILERANGE, bulletslope, 1000000);
}


static void C_bind(char* cmd)
{
    char* key_to_bind;
    char* bind_command;
    int keycode_to_bind;

    if (!cmd || !*cmd) {
        doom_printf("bind [key] [command]");
        return;
    }

    key_to_bind = cmd;
    bind_command = strchr(cmd, ' ');

    if (!key_to_bind || !bind_command) {
        doom_printf("bind [key] [command]");
        return;
    }

    *bind_command = '\0';
    bind_command++;

    while (isspace(*bind_command))
        bind_command++;

    if (!bind_command) {
        doom_printf("bind [key] [command]");
        return;
    }

    keycode_to_bind = KeyNameToKeyCode(key_to_bind);
    if (keycode_to_bind < 0) {
        doom_printf("Bind failed; invalid key: %s", key_to_bind);
    } else {
        if (C_RegisterBind(keycode_to_bind,bind_command)) {
            doom_printf("Bound %s", key_to_bind);
        } else {
            doom_printf("Could not bind %s", key_to_bind);
        }
    }
}

static void C_unbind(char* cmd)
{
    int keycode_to_unbind = KeyNameToKeyCode(cmd);
    if (keycode_to_unbind < 0) {
        doom_printf("Bind failed; invalid key: %s", cmd);
    } else {
        if (C_UnregisterBind(keycode_to_unbind)) {
            doom_printf("Unbound %s", cmd);
        } else {
            doom_printf("No bind found for %s", cmd);
        }
    }
}

command command_list[] = {
    {"noclip", C_noclip},
    {"noclip2", C_noclip2},
    {"resurrect", C_resurrect},
    {"god", C_god},
    {"next", C_nextlevel},
    {"nextsecret", C_nextlevelsecret},
    {"kill", C_kill},
    {"print", C_printcmd},
    {"quit", C_quit},
    {"toggle_psprites", C_togglepsprites},
    {"snd_sfxvolume", C_sndvol},
    {"snd_musicvolume", C_musvol},
    {"jds", C_jds},
    {"plat_skip", C_platskip},
    {"map", C_map},
    {"warp", C_map},
    {"give", C_give},
    {"note", C_note},
    {"mdk", C_mdk},
    {"bind", C_bind},
    {"unbind", C_unbind},

    /* aliases */
    {"snd", C_sndvol},
    {"mus", C_musvol},
    {"exit", C_quit},
    {0,0}
};

static int command_wrptr = 0;
static int command_rdptr = 0;
void C_AddCommandToHistory(const char* cmd)
{
    if (!cmd || !*cmd) return;
    if (command_history[command_wrptr])
        free(command_history[command_wrptr]);
    command_history[command_wrptr] = malloc(sizeof(char)*(1+strlen(cmd)));
    strcpy(command_history[command_wrptr], cmd);
    command_wrptr = (command_wrptr + 1) % CONSOLE_COMMAND_HISTORY_LEN;
}

void C_ConsoleCommand(char* cmd)
{
    /* try cheats first */
    int ch = 0;
    if(!cmd || !cmd[0]) return;
    int cmdlen = strlen(cmd);
    /* send a bogus space to clear out any cached
     * cheat keystrokes */
    ch = C_SendCheat(cmd);

    if (ch) {
        C_AddCommandToHistory(cmd);
    } else {
        /* tokenize based on first space */
        int i = 0;
        char* cptr = cmd;
        char* fullcmd = strdup(cmd);
        const char* sendcmd = NULL;
        while(cptr && *cptr && !isspace(*cptr)) cptr++;
        if(*cptr) {
            *cptr = '\0';
            sendcmd = cptr+1;
        } else {
            sendcmd = cptr;
        }

        for (i=0; command_list[i].name; i++) {
            if (stricmp(command_list[i].name, cmd) == 0) {
                command_list[i].func(cptr+1);
                C_AddCommandToHistory(fullcmd);
                break;
            }
        }
        free(fullcmd);
        if(!command_list[i].name)
            doom_printf("Command not found: %s", cmd);
    }
}

void C_ResetCommandHistoryPosition()
{
    command_rdptr = command_wrptr;
}

const char* C_NavigateCommandHistory(int direction)
{
    int new_command_rdptr = (command_rdptr+direction) % CONSOLE_COMMAND_HISTORY_LEN;
    if (new_command_rdptr < 0)
        new_command_rdptr = CONSOLE_COMMAND_HISTORY_LEN - 1;
    if (command_history[new_command_rdptr])
        command_rdptr = new_command_rdptr;
    return command_history[command_rdptr];
}

void C_ConsolePrintf(const char *s, ...)
{
    va_list v;
    va_start(v,s);
    doom_vsnprintf(console_message,sizeof(console_message),s,v);
    va_end(v);
}

const char* C_GetMessage()
{
    return console_message;
}

dboolean C_HasMessage()
{
    return (console_message[0] != '\0');
}

void C_ClearMessage()
{
    console_message[0] = '\0';
}


/* keybind system */
static int KeyNameToKeyCode(const char* name)
{
    int i;
    static const char* keynames[] = {
        "RIGHTARROW",
        "LEFTARROW",
        "UPARROW",
        "DOWNARROW",
        "ESCAPE",
        "ENTER",
        "TAB",
        "F1",
        "F2",
        "F3",
        "F4",
        "F5",
        "F6",
        "F7",
        "F8",
        "F9",
        "F10",
        "F11",
        "F12",
        "BACKSPACE",
        "PAUSE",
        "EQUALS",
        "MINUS",
        "RSHIFT",
        "RCTRL",
        "RALT",
        "LALT",
        "CAPSLOCK",
        "PRINTSC",
        "INSERT",
        "HOME",
        "PAGEUP",
        "PAGEDOWN",
        "DEL",
        "END",
        "SCROLLLOCK",
        "SPACEBAR",
        "NUMLOCK",
        "KEYPAD0",
        "KEYPAD1",
        "KEYPAD2",
        "KEYPAD3",
        "KEYPAD4",
        "KEYPAD5",
        "KEYPAD6",
        "KEYPAD7",
        "KEYPAD8",
        "KEYPAD9",
        "KEYPADENTER",
        "KEYPADDIVIDE",
        "KEYPADMULTIPLY",
        "KEYPADMINUS",
        "KEYPADPLUS",
        "KEYPADPERIOD",
        "MOUSE1",
        "MOUSE2",
        "MOUSE3",
        "MWHEELUP",
        "MWHEELDOWN",
        NULL
    };

    static const int keycodes[] = {
        0xae,
        0xac,
        0xad,
        0xaf,
        27,
        13,
        9,
        (0x80+0x3b),
        (0x80+0x3c),
        (0x80+0x3d),
        (0x80+0x3e),
        (0x80+0x3f),
        (0x80+0x40),
        (0x80+0x41),
        (0x80+0x42),
        (0x80+0x43),
        (0x80+0x44),
        (0x80+0x57),
        (0x80+0x58),
        127,
        0xff,
        0x3d,
        0x2d,
        (0x80+0x36),
        (0x80+0x1d),
        (0x80+0x38),
        KEYD_RALT,
        0xba,
        0xfe,
        0xd2,
        0xc7,
        0xc9,
        0xd1,
        0xc8,
        0xcf,
        0xc6,
        0x20,
        0xC5,
        (0x100 + '0'),
        (0x100 + '1'),
        (0x100 + '2'),
        (0x100 + '3'),
        (0x100 + '4'),
        (0x100 + '5'),
        (0x100 + '6'),
        (0x100 + '7'),
        (0x100 + '8'),
        (0x100 + '9'),
        (0x100 + KEYD_ENTER),
        (0x100 + '/'),
        (0x100 + '*'),
        (0x100 + '-'),
        (0x100 + '+'),
        (0x100 + '.'),
        (0x80 + 0x60),
        (0x80 + 0x61),
        (0x80 + 0x62),
        (0x80 + 0x6b),
        (0x80 + 0x6c)
    };

    /* FIXME: Handle "uppercase" characters */
    /* printable ascii chars go out as-is */
    if (strlen(name) == 1 &&
            name[0] > '0' &&
            name[0] < 'z')
        return tolower(name[0]);

    for (i=0; keynames[i]; i++) {
        if (stricmp(name, keynames[i]) == 0)
            return keycodes[i];
    }

    return -1;
}

typedef struct keybind_t
{
    int keycode;
    char* cmd;
    struct keybind_t* next;
} keybind_t;

static keybind_t* keybind_head = NULL;

dboolean C_RegisterBind(int keycode, char* cmd)
{
    keybind_t* kb = keybind_head;
    keybind_t* new_bind = malloc(sizeof(keybind_t));

    if (!new_bind)
        I_Error("Out of space for keybind");

    /* note: multi-bind allowed! */
    /* no need to search for existing binds */
    while (kb && kb->next)
        kb = kb->next;

    new_bind->keycode = keycode;
    new_bind->cmd = strdup(cmd);
    new_bind->next = NULL;

    if (kb)
        kb->next = new_bind;
    else
        keybind_head = new_bind;

    return true;
}

dboolean C_UnregisterBind(int keycode)
{
    keybind_t* kb = keybind_head;
    keybind_t* prev = NULL;
    dboolean found = false;

    while (kb) {
        if (kb->keycode == keycode) {
            found = true;
            if (prev) {
                prev->next = kb->next;
                free(kb->cmd);
                free(kb);
                kb = prev->next;
            } else if (kb->next) {
                keybind_head = kb->next;
                free(kb->cmd);
                free(kb);
                kb = keybind_head;
            } else {
                free(kb->cmd);
                free(kb);
                kb = NULL;
                keybind_head = NULL;
            }
        } else {
            prev = kb;
            kb = kb->next;
        }
    }

    /* returns true if one or more unbind(s) occur */
    return found;
}

dboolean C_ExecuteBind(int keycode)
{
    keybind_t* kb = keybind_head;
    dboolean executed = false;

    if (netgame) {
        doom_printf("Binds not allowed during net play.");
        return false;
    } else if (demorecording || demoplayback) {
        doom_printf("Binds not allowed during demos.");
        return false;
    }

    while (kb) {
        if (kb->keycode == keycode) {
            C_ConsoleCommand(kb->cmd);
            executed = true;
        }
        kb = kb->next;
    }

    return executed;
}

dboolean C_Responder(event_t* ev)
{
    if (ev && ev->type == ev_keydown) {
        C_ExecuteBind(ev->data1);
    }
    /* key binds never consume the key */
    return false;
}

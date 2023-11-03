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
#include "i_system.h"
#include "m_menu.h"
#include "m_misc.h"
#include "am_map.h"

#include <time.h>

extern char *basesavegame;
extern void M_QuitResponse(int ch);
extern const char * const ActorNames[];

#define plyr (players+consoleplayer)
#define CONSOLE_COMMAND_HISTORY_LEN (16)
static char* command_history[CONSOLE_COMMAND_HISTORY_LEN] = { 0 };
static char console_message[HU_MSGWIDTH];

static int KeyNameToKeyCode(const char* name);
static int MouseNameToMouseCode(const char* name);

dboolean c_drawpsprites = true;

/* Parses arguments out from a string using spaces as separators.
 * Returns the number of arguments successfully parsed.
 *
 * WARNING: This will MODIFY the input string (similar to strtok)!
 *
 * Parameters:
 *  arg = string to parse out arguments from
 *  arglist = array of lenth max_args to deposit args into
 *  max_args = array length of arglist and the max number of args to parse.
 *
 *  if more arguments are found beyond max_args, the last element in arglist
 *  contains the remainder of the string, with any trailing separators removed.
 */
static int C_ParseArgs(char* arg, char** arglist, const int max_args)
{
    int cc = 0; /* current char */
    int ca = 0; /* current arg  */
    int total_len = 0;

    /* bail out on bad inputs */
    if (!arg || !arglist || max_args <= 0) return 0;

    total_len = strlen(arg);

    for (cc = 0; cc < total_len && ca < max_args;) {
        /* fast forward over leading separators */
        while (cc < total_len && isspace(arg[cc]))
            cc++;

        /* we are at a token */
        if (arg[cc])
            arglist[ca++] = &arg[cc];

        /* move forward until we hit a separator */
        while (cc < total_len && !isspace(arg[cc]))
            cc++;

        /* we hit a separator OR already at null terminator in
         * which case this does nothing */
        if (ca < max_args)
            arg[cc++] = '\0';
        else if (isspace(arg[total_len-1])) {
            /* else, if the end of the string contains separators,
             * strip them out. otherwise, we're done */
            int i = total_len-1;

            while (isspace(arg[i]) && i >= cc)
                i--;

            arg[i+1] = '\0';
        }

    }

    return ca;
}

static char* C_StripSpaces(char* s)
{
    char* end;
    char* start;
    int len;

    if (!s) return NULL;

    start = s;
    len = strlen(s);

    if (len > 0) {
        end = start + strlen(s) - 1;
        if (end < start) end = start;

        while (start < end && isspace(*start)) start++;
        while (end >= start && isspace(*end)) end--;

        *++end = '\0';
    }

    return start;
}

static int C_SendCheatKey(int key)
{
  static uint_64_t sr;
  static char argbuf[CHEAT_ARGS_MAX+1], *arg;
  static int init, argsleft, cht;
  int i, ret, matchedbefore;

  // If we are expecting arguments to a cheat
  // (e.g. idclev), put them in the arg buffer

  if (argsleft)
    {
      *arg++ = tolower(key);             // store key in arg buffer
      if (!--argsleft)                   // if last key in arg list,
        cheat[cht].func(argbuf);         // process the arg buffer
      return 1;                          // affirmative response
    }

  key = tolower(key) - 'a';
  if (key < 0 || key >= 32)              // ignore most non-alpha cheat letters
    {
      sr = 0;        // clear shift register
      return 0;
    }

  if (!init)                             // initialize aux entries of table
    {
      init = 1;
      for (i=0;cheat[i].orig_cheat;i++)
        {
          uint_64_t c=0, m=0;
          const char *p;

          for (p=cheat[i].orig_cheat; *p; p++)
            {
              unsigned key = tolower(*p)-'a';  // convert to 0-31
              if (key >= 32)            // ignore most non-alpha cheat letters
                continue;
              c = (c<<5) + key;         // shift key into code
              m = (m<<5) + 31;          // shift 1's into mask
            }
          cheat[i].code = c;            // code for this cheat key
          cheat[i].mask = m;            // mask for this cheat key
        }
    }

  sr = (sr<<5) + key;                   // shift this key into shift register

  for (matchedbefore = ret = i = 0; cheat[i].orig_cheat; i++)
    if ((sr & cheat[i].mask) == cheat[i].code &&      // if match found
        !(cheat[i].when & not_dm   && deathmatch) &&  // and if cheat allowed
        !(cheat[i].when & not_coop && netgame && !deathmatch) &&
        !(cheat[i].when & not_demo && (demorecording || demoplayback)) &&
        !(cheat[i].when & not_menu && menuactive) &&
        !(cheat[i].when & not_deh  && M_CheckParm("-deh"))) {
      if (cheat[i].arg < 0)               // if additional args are required
        {
          cht = i;                        // remember this cheat code
          arg = argbuf;                   // point to start of arg buffer
          argsleft = -cheat[i].arg;       // number of args expected
          ret = 1;                        // responder has eaten key
        }
      else
        if (!matchedbefore)               // allow only one cheat at a time
          {
            matchedbefore = ret = 1;      // responder has eaten key
            cheat[i].func(cheat[i].arg);  // call cheat handler
          }
    }
  return ret;
}

static int C_SendCheatConsole(const char* cheat)
{
    int ch = 0;
    C_SendCheatKey(' ');
    for (int i=0; i < strlen(cheat); i++) {
        ch |= C_SendCheatKey(tolower(cheat[i]));
    }
    return ch;
}

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
    C_SendCheatConsole("iddqd");
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
    int killcount=0;
    thinker_t *currentthinker = NULL;
    extern void A_PainDie(mobj_t *);
    char* end;

    if (!cmd) return;
    /* strip spaces */
    while(isspace(*cmd)) cmd++;
    end = &cmd[strlen(cmd)];
    while(isspace(*(end-1))) end--;

    /* find actor matching cmd */
    for (i=0; ActorNames[i]; i++)
        if (strcasecmp(cmd,ActorNames[i]) == 0)
            break;

    if (!ActorNames[i]) {
        doom_printf("Actor type %s not found.", cmd);
        return;
    }

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
    doom_printf("Platform wait skipping %s", plat_skip ? "enabled" : "disabled");
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
        C_SendCheatConsole(clev);
    }
}

typedef struct
{
    const char* give;
    const char* cheat;
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
            if (strcasecmp(giveme, cheatmap[i].give) == 0) {
                C_SendCheatConsole(cheatmap[i].cheat);
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
                    if (strcasecmp(strrchr(wadfiles[j].name,'.'), ".gwa")) {
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
    P_MapStart();
    P_LineAttack(plyr->mo, plyr->mo->angle, MISSILERANGE, bulletslope, 1000000);
    P_MapEnd();
}


static void C_bind(char* cmd)
{
    extern setup_menu_t* keys_settings[];
    char* key_to_bind;
    char* bind_command;
    int keycode_to_bind;
    evtype_t bind_type;
    setup_menu_t* keys_to_check = NULL;
    const char* already_bound_to = NULL;
    dboolean already_bound = false;
    int i;


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
    if (keycode_to_bind > 0) {
        bind_type = ev_keydown;
    } else {
        keycode_to_bind = MouseNameToMouseCode(key_to_bind);
        if (keycode_to_bind > 0) {
            bind_type = ev_mouse;
        }
    }

    /* if we reach here and keycode is not set, the nour attempts 
     * to match this bind to an event type have failed */
    if (keycode_to_bind <= 0) {
        doom_printf("Bind failed; invalid key: %s", key_to_bind);
        return;
    }

    /* see if already bound and warn the user */
    for (i = 0; !already_bound && keys_settings[i]; i++) {
        for (keys_to_check = keys_settings[i] ; !(keys_to_check->m_flags & S_END) ; keys_to_check++) {
            if ((keys_to_check->m_flags & S_KEY) &&
                    (keys_to_check->var.m_key) &&
                    (*(keys_to_check->var.m_key) == keycode_to_bind) &&
                    ((bind_type == ev_mouse && keys_to_check->m_mouse) ||
                     (bind_type != ev_mouse && !keys_to_check->m_mouse)
                     )) {
                already_bound = true;
                already_bound_to = strdup(keys_to_check->m_text);
                break;
            }
        }
    }

    if (C_RegisterBind(keycode_to_bind,bind_command,bind_type)) {
        if (already_bound && already_bound_to) {
            doom_printf("Bound %s; WARNING: Already bound to %s", key_to_bind, already_bound_to);
        } else {
            doom_printf("Bound %s", key_to_bind);
        }
    } else {
        doom_printf("Could not bind %s", key_to_bind);
    }
}

static void C_unbind(char* cmd)
{
    evtype_t bind_type;
    int keycode_to_unbind = KeyNameToKeyCode(cmd);
    if (keycode_to_unbind > 0) {
        bind_type = ev_keydown;
    } else {
        keycode_to_unbind = MouseNameToMouseCode(cmd);
        if (keycode_to_unbind > 0) {
            bind_type = ev_mouse;
        }
    }

    if (keycode_to_unbind <= 0) {
        doom_printf("Unbind failed; invalid key: %s", cmd);
        return;
    }

    if (C_UnregisterBind(keycode_to_unbind, bind_type)) {
        doom_printf("Unbound %s", cmd);
    } else {
        doom_printf("No bind found for %s", cmd);
    }
}

extern void AM_Start();
extern void AM_Stop();
static void C_mapfollow(char* cmd)
{
  if (!(automapmode & am_active)) {
      automapmode |= am_follow;
      AM_Start();
  } else {
      AM_Stop();
  }
}

static void C_complevel(char* cmd)
{
    int newcl = -1;
    if (cmd && *cmd) {
        char* endptr = cmd;
        int clarg = -1;
        clarg = strtoul(cmd, &endptr, 0);
        if (cmd != endptr)
            newcl = clarg;
    }

    if (newcl > 0 && newcl < MAX_COMPATIBILITY_LEVEL) {
        char cl_change[8];
        snprintf(cl_change, 8, "TNTCL%02d", newcl);
        C_SendCheatConsole(cl_change);
    } else { 
        doom_printf("Compatibility level: %d (%s)", compatibility_level, comp_lev_str[compatibility_level]);
    }
}

typedef struct weapon_names_s {
    const char* name;
    unsigned int slot;
} weapon_names_t;

static weapon_names_t weapon_names[] = {
    {"fist", 0},

    {"pistol", 1},

    {"shotgun", 2},

    {"supershotgun", 8},
    {"super shotgun", 8},
    {"ssg", 8},
    {"shotgun2", 8},

    {"chaingun", 3},
    {"minigun", 3},

    {"rocket launcher", 4},
    {"rocketlauncher", 4},
    {"rl", 4},

    {"plasma rifle", 5},
    {"plasmarifle", 5},
    {"plasmagun", 5},
    {"plasma gun", 5},

    {"bfg", 6},
    {"bfg9000", 6},
    {"bfg-9000", 6},
    {"bfg 9000", 6},

    {"chainsaw", 7},
    {NULL, NUMWEAPONS}
};

static void C_switchweapon(char* cmd)
{
    unsigned int newweapon = NUMWEAPONS;
    if (cmd && *cmd) {
        unsigned int weaponarg = NUMWEAPONS;
        char* endptr = cmd;
        weaponarg = strtoul(cmd, &endptr, 0);
        if (cmd != endptr)
            newweapon = weaponarg;
    }

    /* try string matching */
    if (newweapon == NUMWEAPONS) {
        int i;
        char* cmd_arg = C_StripSpaces(cmd);
        for (i = 0; weapon_names[i].name; i++) {
            if (strcasecmp(weapon_names[i].name,cmd_arg) == 0) {
                newweapon = weapon_names[i].slot;
                break;
            }
        }
    }

    /* newweapon unsigned so it must be >= 0 */
    if (newweapon == plyr->readyweapon) {
        doom_printf("Weapon %u already selected", newweapon);
    } else if (newweapon < NUMWEAPONS) {
        if (plyr->weaponowned[newweapon]) {
            plyr->pendingweapon = newweapon;
        } else {
            doom_printf("Weapon %u not owned", newweapon);
        }
    } else {
        doom_printf("Invalid weapon: %s", cmd);
    }
}

static void C_automapwarp(char* cmd)
{
    if (automapmode & am_active) {
        fixed_t tmapx;
        fixed_t tmapy;
        AM_GetCrosshairPosition(&tmapx, &tmapy);
        P_MapStart();
        P_TeleportMove(plyr->mo,tmapx,tmapy,false);
        P_MapEnd();
    } else {
        doom_printf("Must be in automap mode to use this command.");
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
    {"mapfollow", C_mapfollow},
    {"complevel", C_complevel},
    {"switchweapon", C_switchweapon},
    {"am_warpto", C_automapwarp},

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

static dboolean C_ConsoleCommandHandler(char* cmd)
{
    /* tokenize based on first space */
    dboolean found = false;
    int i = 0;
    char* command = strdup(cmd);
    char* cptr = command;
    char* arguments = NULL;
    while(cptr && *cptr && !isspace(*cptr)) cptr++;
    if(*cptr) {
        *cptr = '\0';
        arguments = cptr+1;
    } else {
        arguments = cptr;
    }

    for (i=0; command_list[i].name; i++) {
        if (strcasecmp(command_list[i].name, command) == 0) {
            command_list[i].func(arguments);
            found = true;
            break;
        }
    }
    free(command);
    return found;
}

static dboolean C_ConsoleCheatHandler(char* cmd)
{
    return C_SendCheat(cmd);
}

static dboolean C_ConsoleSettingHandler(char* cmd)
{
    dboolean found = false;
    dboolean write = false;
    int i = 0;
    char* command = strdup(cmd);
    char* argument = NULL;
    char* hold = command;
    default_t* setting = NULL;

    if(!command || !*command)
        return false;

    /* strip whitespace */
    while(isspace(*command)) command++;
    argument = command;
    while(*argument && !isspace(*argument)) argument++;
    while(isspace(*argument)) argument++;
    if (strlen(argument) > 0) {
        argument = M_StrRTrim(argument);
        write = true;
    }

    for (i = 0 ; i < numdefaults ; i++) {
        if ((defaults[i].type != def_none) && !strcasecmp(command, defaults[i].name)) {
            setting = &defaults[i];
            found = true;
            break;
        }
    }

    if (found) {
        switch (setting->type) {
            case def_str:
                if (setting->location.ppsz) {
                    doom_printf("%s=%s",setting->name, *setting->location.ppsz);
                } else {
                    doom_printf("%s=(bad value)",setting->name);
                }
                break;
            case def_int:
                if (setting->location.pi) {
                    doom_printf("%s=%d",setting->name, *setting->location.pi);
                } else {
                    doom_printf("%s=(bad value)",setting->name);
                }
                break;
            case def_hex:
                if (setting->location.pi) {
                    doom_printf("%s=0x%x",setting->name, *setting->location.pi);
                } else {
                    doom_printf("%s=(bad value)",setting->name);
                }
                break;
            case def_arr:
                if (write) { 
                    doom_printf("%s=(can't write arrays)",setting->name);
                } else {
                    doom_printf("%s=(array)",setting->name);
                }
                break;
            case def_none:
            default:
                doom_printf("%s=???",setting->name);
                break;
        }
    }

    free(hold);
    return found;
}

void C_ConsoleCommand(char* cmd)
{
    if(!cmd || !cmd[0]) return;

    if (C_ConsoleCommandHandler(cmd) ||
            C_ConsoleSettingHandler(cmd) ||
            C_ConsoleCheatHandler(cmd)
            ) {
        C_AddCommandToHistory(cmd);
    } else {
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
    (0x80 + 0x6b),
    (0x80 + 0x6c),
    0 /* "null" terminator */
};

static const char* mousenames[] = {
    "MOUSE1",
    "MOUSE2",
    "MOUSE3",
    "MOUSE4",
    "MOUSE5",
    "MOUSE6",
    "MOUSE7",
    "MOUSE8",
    NULL
};

static int mousecodes[] = {
    1,
    2,
    4,
    32,
    64,
    8,
    16,
    128,
    0,
};

static int KeyNameToKeyCode(const char* name)
{
    int i;
    /* printable ascii chars go out as-is */
    if (strlen(name) == 1 && isprint(name[0]))
        return tolower(name[0]);
    for (i=0; keynames[i]; i++) {
        if (strcasecmp(name, keynames[i]) == 0)
            return keycodes[i];
    }
    return -1;
}

static const char* KeyCodeToKeyName(const int keycode)
{
    int i;
    for (i=0; keycodes[i]; i++) {
        if (keycodes[i] == keycode)
            return keynames[i];
    }
    return NULL;
}

static int MouseNameToMouseCode(const char* name)
{
    int i;
    for (i=0; mousenames[i]; i++) {
        if (strcasecmp(name, mousenames[i]) == 0)
            return mousecodes[i];
    }
    return -1;
}

static const char* MouseCodeToMouseName(const int mousecode)
{
    int i;
    for (i=0; mousecodes[i]; i++) {
        if (mousecodes[i] == mousecode)
            return mousenames[i];
    }
    return NULL;
}

typedef struct keybind_t
{
    int keycode;
    char* cmd;
    evtype_t type;
    struct keybind_t* next;
} keybind_t;

static keybind_t* keybind_head = NULL;

dboolean C_RegisterBind(int keycode, char* cmd, evtype_t type)
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
    new_bind->type = type;
    new_bind->next = NULL;

    if (kb)
        kb->next = new_bind;
    else
        keybind_head = new_bind;

    return true;
}

dboolean C_UnregisterBind(int keycode, evtype_t type)
{
    keybind_t* kb = keybind_head;
    keybind_t* prev = NULL;
    dboolean found = false;

    while (kb) {
        if (kb->keycode == keycode && kb->type == type) {
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

dboolean C_ExecuteBind(int keycode, evtype_t evtype)
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
        if (kb->keycode == keycode && kb->type == evtype) {
            C_ConsoleCommand(kb->cmd);
            executed = true;
        }
        kb = kb->next;
    }

    return executed;
}

dboolean C_Responder(event_t* ev)
{
    if (ev && (ev->type == ev_keydown || ev->type == ev_mouse)) { 
        C_ExecuteBind(ev->data1, ev->type);
    }

    /* key binds never consume the key */
    return false;
}

static char* C_GetConsoleSettingsFile()
{
    static char* console_settings_file = NULL;
#define PRBOOMX_CONSOLE_CFG "prboomx_console.cfg"
    if (!console_settings_file) {
        int i;
        i = M_CheckParm ("-consoleconfig");
        if (i && i < myargc-1)
        {
            console_settings_file = strdup(myargv[i+1]);
        }
        else
        {
            const char* exedir = strdup(I_DoomExeDir());
            /* get config file from same directory as executable */
            int len = doom_snprintf(NULL, 0, "%s/" PRBOOMX_CONSOLE_CFG, exedir);
            console_settings_file = malloc(len+1);
            doom_snprintf(console_settings_file, len+1, "%s/" PRBOOMX_CONSOLE_CFG, exedir);
        }

        lprintf (LO_CONFIRM, " console settings file: %s\n",console_settings_file);
    }
    return console_settings_file;
}

#define CONSOLE_CONFIG_LINE_MAX (256)
void C_SaveSettings()
{
    keybind_t* kb = keybind_head;
    FILE* bindfile = M_fopen (C_GetConsoleSettingsFile(), "w");

    if (bindfile) {
        while (kb) {
            if (kb->type == ev_keydown) {
                const char* keyname = KeyCodeToKeyName(kb->keycode);
                if (keyname) {
                    fprintf(bindfile, "bind %s %s\n", keyname, kb->cmd);
                } else if (isprint(kb->keycode)) {
                    fprintf(bindfile, "bind %c %s\n", kb->keycode, kb->cmd);
                } else {
                    lprintf(LO_WARN, " bad keybind found: %x (bound to: %s)\n",kb->keycode, kb->cmd);
                }
            } else if (kb->type == ev_mouse) {
                const char* mousename = MouseCodeToMouseName(kb->keycode);
                if (mousename) {
                    fprintf(bindfile, "bind %s %s\n", mousename, kb->cmd);
                } else {
                    lprintf(LO_WARN, " bad mouse bind found: %x (bound to: %s)\n",kb->keycode, kb->cmd);
                }

            } else {
                lprintf(LO_WARN, " bad bind found: %x (bound to: %s, event type: %d)\n",kb->keycode, kb->cmd, kb->type);
            }
            kb = kb->next;
        }
        fclose(bindfile);
    }
}

void C_LoadSettings()
{
    FILE* bindfile = NULL;
    char* linebuffer = malloc(sizeof(char)*CONSOLE_CONFIG_LINE_MAX);

    bindfile = M_fopen(C_GetConsoleSettingsFile(), "r");
    if (bindfile) {
        while (!feof(bindfile)) {
            char* line = linebuffer;
            line = fgets(line, CONSOLE_CONFIG_LINE_MAX, bindfile);
            /* strip leading whitespace */
            while (line && isspace(*line))
                line++;
            /* if there's any line left, execute it */
            /* comment lines start with '#' */
            if (line && *line != '\0' && *line != '#') {
                /* strip ending newline, if there */
                char* end = strrchr(line, '\n');
                if (end) *end = '\0';
                C_ConsoleCommand(line);
            }
        }
        fclose (bindfile);
        /* flush console display */
        linebuffer[0] = '\0';
        C_printcmd(linebuffer);
    }

    free(linebuffer);
}


/* Complete a partial command with the nearest match
 * If successful, returns the string match
 * Returns NULL if no match was found.
 */
const char* C_CommandComplete(const char* partial)
{
    /* check order:
     * 1. Console Commands
     * 2. Settings
     * 3. Cheats */
    int i;
    int partial_len;
    cheatseq_t* cht;

    if (partial && *partial) {
        partial_len = strlen(partial);
    } else {
        return NULL;
    }

    /*         C_ConsoleCheatHandler(cmd) */

    /* console command check */
    for (i = 0; command_list[i].name; i++) {
        if (!strncasecmp(command_list[i].name, partial, partial_len)) {
            return command_list[i].name;
        }
    }

    /* settings check */
    for (i = 0; i < numdefaults; i++) {
        if ((defaults[i].type != def_none) && !strncasecmp(defaults[i].name, partial, partial_len)) {
            return defaults[i].name;
        }
    }

    /* cheat check */
    /* ensure cheats are initialized */
    M_FindCheats(' ');
    for (cht = cheat; cht->cheat; cht++) {
        if (cht && cht->cheat && !strncasecmp(cht->cheat, partial, partial_len)) {
            return cht->cheat;
        }
    }

    return NULL;
}

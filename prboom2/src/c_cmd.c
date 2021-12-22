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


extern void M_QuitDOOM(int choice);

#define plyr (players+consoleplayer)
#define CONSOLE_COMMAND_HISTORY_LEN (16)
static char* command_history[CONSOLE_COMMAND_HISTORY_LEN] = { 0 };

dboolean c_drawpsprites = true;

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
        doom_printf(cmd);
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
    S_StartSound(NULL,sfx_swtchn);
    M_QuitDOOM(0);
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
    M_FindCheats(' ');
    for (int i=0; i < cmdlen; i++) {
        ch |= M_FindCheats(tolower(cmd[i]));
    }

    if (ch) {
        C_AddCommandToHistory(cmd);
    } else {
        /* tokenize based on first space */
        int i = 0;
        char* cptr = cmd;
        char fullcmd[cmdlen+1];
        strcpy(fullcmd, cmd);
        while(cptr && *cptr && !isspace(*cptr)) cptr++;
        if(*cptr) *cptr = '\0';

        for (i=0; command_list[i].name; i++) {
            if (stricmp(command_list[i].name, cmd) == 0) {
                command_list[i].func(cptr+1);
                C_AddCommandToHistory(fullcmd);
                return;
            }
        }
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

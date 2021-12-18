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

#define plyr (players+consoleplayer)

static void C_noclip(char* cmd)
{
  plyr->message = (plyr->cheats ^= CF_NOCLIP) & CF_NOCLIP ?
    s_STSTR_NCON : s_STSTR_NCOFF;
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

command command_list[] = {
    {"noclip", C_noclip},
    {"resurrect", C_resurrect},
    {"god", C_god},
    {"next", C_nextlevel},
    {"nextsecret", C_nextlevelsecret},
    {"print", C_printcmd},
    {0,0}
};

void C_ConsoleCommand(char* cmd)
{
    /* tokenize based on first space */
    int i = 0;
    char* cptr = cmd;
    if(!cmd || !cmd[0]) return;
    while(cptr && *cptr && !isspace(*cptr)) cptr++;
    if(*cptr) *cptr = '\0';

    for (i=0; command_list[i].name; i++) {
        if (stricmp(command_list[i].name, cmd) == 0) {
            command_list[i].func(cptr+1);
            return;
        }
    }
    doom_printf("Command not found: %s", cmd);
}

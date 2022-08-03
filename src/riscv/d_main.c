// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// $Log:$
//
// DESCRIPTION:
//      DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
//      plus functions to determine game mode (shareware, registered),
//      parse command line parameters, configure game parameters (turbo),
//      and call the startup functions.
//
//-----------------------------------------------------------------------------

static const char __attribute__((unused))
rcsid[] = "$Id: d_main.c,v 1.8 1997/02/03 22:45:09 b1 Exp $";


#define BGCOLOR         7
#define FGCOLOR         8


#ifdef NORMALUNIX
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif


#include "doomdef.h"
#include "doomstat.h"

#include "dstrings.h"
#include "sounds.h"


#include "z_zone.h"
#include "w_wad.h"
#include "s_sound.h"
#include "v_video.h"

#include "f_finale.h"
#include "f_wipe.h"

#include "m_argv.h"
#include "m_misc.h"
#include "m_menu.h"

#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"

#include "g_game.h"

#include "hu_stuff.h"
#include "wi_stuff.h"
#include "st_stuff.h"
#include "am_map.h"

#include "p_setup.h"
#include "r_local.h"


#include "d_main.h"

//
// D-DoomLoop()
// Not a globally visible function,
//  just included for source reference,
//  called by D_DoomMain, never exits.
// Manages timing and IO,
//  calls all ?_Responder, ?_Ticker, and ?_Drawer,
//  calls I_GetTime, I_StartFrame, and I_StartTic
//
void D_DoomLoop (void);


char*           wadfiles[MAXWADFILES];


boolean         devparm;        // started game with -devparm
boolean         nomonsters;     // checkparm of -nomonsters
boolean         respawnparm;    // checkparm of -respawn
boolean         fastparm;       // checkparm of -fast

boolean         drone;

boolean         singletics = false; // debug flag to cancel adaptiveness



//extern int soundVolume;
//extern  int   sfxVolume;
//extern  int   musicVolume;

extern  boolean inhelpscreens;

skill_t         startskill;
int             startepisode;
int             startmap;
boolean         autostart;

boolean         advancedemo;




char            wadfile[1024];          // primary wad file
char            mapdir[1024];           // directory of development maps
char            basedefault[] = "doomrc"; // default file


void D_CheckNetGame (void);
void D_ProcessEvents (void);
void G_BuildTiccmd (ticcmd_t* cmd);
void D_DoAdvanceDemo (void);


//
// EVENT HANDLING
//
// Events are asynchronous inputs generally generated by the game user.
// Events can be discarded if no responder claims them
//
event_t         events[MAXEVENTS];
int             eventhead;
int             eventtail;


//
// D_PostEvent
// Called by the I/O functions when input is detected
//
void D_PostEvent (event_t* ev)
{
    events[eventhead] = *ev;
    eventhead = (eventhead+1)&(MAXEVENTS-1);
}


//
// D_ProcessEvents
// Send all the events of the given timestamp down the responder chain
//
void D_ProcessEvents (void)
{
    event_t*    ev;

    for ( ; eventtail != eventhead ; eventtail = (eventtail+1)&(MAXEVENTS-1) )
    {
        ev = &events[eventtail];
        if (M_Responder (ev))
            continue;               // menu ate the event
        G_Responder (ev);
    }
}




//
// D_Display
//  draw current display, possibly wiping it from the previous
//

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t     wipegamestate = GS_DEMOSCREEN;
extern  boolean setsizeneeded;
extern  int             showMessages;
void R_ExecuteSetViewSize (void);

void D_Display (void)
{
    static  boolean             viewactivestate = false;
    static  boolean             menuactivestate = false;
    static  boolean             inhelpscreensstate = false;
    static  boolean             fullscreen = false;
    static  gamestate_t         oldgamestate = -1;
    static  int                 borderdrawcount;
    int                         nowtime;
    int                         tics;
    int                         wipestart;
    int                         y;
    boolean                     done;
    boolean                     wipe;
    boolean                     redrawsbar;

    if (nodrawers)
        return;                    // for comparative timing / profiling

    redrawsbar = false;

    // change the view size if needed
    if (setsizeneeded)
    {
        R_ExecuteSetViewSize ();
        oldgamestate = -1;                      // force background redraw
        borderdrawcount = 3;
    }

    // save the current screen if about to wipe
    if (gamestate != wipegamestate)
    {
        wipe = true;
        wipe_StartScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);
    }
    else
        wipe = false;

    if (gamestate == GS_LEVEL && gametic)
        HU_Erase();

    // do buffered drawing
    switch (gamestate)
    {
      case GS_LEVEL:
        if (!gametic)
            break;
        if (automapactive)
            AM_Drawer ();
        if (wipe || (viewheight != 200 && fullscreen) )
            redrawsbar = true;
        if (inhelpscreensstate && !inhelpscreens)
            redrawsbar = true;              // just put away the help screen
        ST_Drawer (viewheight == 200, redrawsbar );
        fullscreen = viewheight == 200;
        break;

      case GS_INTERMISSION:
        WI_Drawer ();
        break;

      case GS_FINALE:
        F_Drawer ();
        break;

      case GS_DEMOSCREEN:
        D_PageDrawer ();
        break;
    }

    // draw buffered stuff to screen
    I_UpdateNoBlit ();

    // draw the view directly
    if (gamestate == GS_LEVEL && !automapactive && gametic)
        R_RenderPlayerView (&players[displayplayer]);

    if (gamestate == GS_LEVEL && gametic)
        HU_Drawer ();

    // clean up border stuff
    if (gamestate != oldgamestate && gamestate != GS_LEVEL)
        I_SetPalette (W_CacheLumpName ("PLAYPAL",PU_CACHE));

    // see if the border needs to be initially drawn
    if (gamestate == GS_LEVEL && oldgamestate != GS_LEVEL)
    {
        viewactivestate = false;        // view was not active
        R_FillBackScreen ();    // draw the pattern into the back screen
    }

    // see if the border needs to be updated to the screen
    if (gamestate == GS_LEVEL && !automapactive && scaledviewwidth != 320)
    {
        if (menuactive || menuactivestate || !viewactivestate)
            borderdrawcount = 3;
        if (borderdrawcount)
        {
            R_DrawViewBorder ();    // erase old menu stuff
            borderdrawcount--;
        }

    }

    menuactivestate = menuactive;
    viewactivestate = viewactive;
    inhelpscreensstate = inhelpscreens;
    oldgamestate = wipegamestate = gamestate;

    // draw pause pic
    if (paused)
    {
        if (automapactive)
            y = 4;
        else
            y = viewwindowy+4;
        V_DrawPatchDirect(viewwindowx+(scaledviewwidth-68)/2,
                          y,0,W_CacheLumpName ("M_PAUSE", PU_CACHE));
    }


    // menus go directly to the screen
    M_Drawer ();          // menu is drawn even on top of everything
    NetUpdate ();         // send out any new accumulation


    // normal update
    if (!wipe)
    {
        I_FinishUpdate ();              // page flip or blit buffer
        return;
    }

    // wipe update
    wipe_EndScreen(0, 0, SCREENWIDTH, SCREENHEIGHT);

    wipestart = I_GetTime () - 1;

    do
    {
        do
        {
            nowtime = I_GetTime ();
            tics = nowtime - wipestart;
        } while (!tics);
        wipestart = nowtime;
        done = wipe_ScreenWipe(wipe_Melt
                               , 0, 0, SCREENWIDTH, SCREENHEIGHT, tics);
        I_UpdateNoBlit ();
        M_Drawer ();                            // menu is drawn even on top of wipes
        I_FinishUpdate ();                      // page flip or blit buffer
    } while (!done);
}



//
//  D_DoomLoop
//
void D_DoomLoop (void)
{
    I_InitGraphics ();

    while (1)
    {
        // frame syncronous IO operations
        I_StartFrame ();

        // process one or more tics
        if (singletics)
        {
            I_StartTic ();
            D_ProcessEvents ();
            G_BuildTiccmd (&netcmds[consoleplayer][maketic%BACKUPTICS]);
            if (advancedemo)
                D_DoAdvanceDemo ();
            M_Ticker ();
            G_Ticker ();
            gametic++;
            maketic++;
        }
        else
        {
            TryRunTics (); // will run at least one tic
        }

        // Update display, next frame, with current state.
        D_Display ();
    }
}



//
//  DEMO LOOP
//
int             demosequence;
int             pagetic;
char                    *pagename;


//
// D_PageTicker
// Handles timing for warped projection
//
void D_PageTicker (void)
{
    if (--pagetic < 0)
        D_AdvanceDemo ();
}



//
// D_PageDrawer
//
void D_PageDrawer (void)
{
    V_DrawPatch (0,0, 0, W_CacheLumpName(pagename, PU_CACHE));
}


//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//
void D_AdvanceDemo (void)
{
    advancedemo = true;
}


//
// This cycles through the demo sequences.
// FIXME - version dependend demo numbers?
//
 void D_DoAdvanceDemo (void)
{
    players[consoleplayer].playerstate = PST_LIVE;  // not reborn
    advancedemo = false;
    usergame = false;               // no save / end game here
    paused = false;
    gameaction = ga_nothing;

    if ( gamemode == retail )
      demosequence = (demosequence+1)%7;
    else
      demosequence = (demosequence+1)%6;

    switch (demosequence)
    {
      case 0:
        if ( gamemode == commercial )
            pagetic = 35 * 11;
        else
            pagetic = 170;
        gamestate = GS_DEMOSCREEN;
        pagename = "TITLEPIC";
        if ( gamemode == commercial )
          S_StartMusic(mus_dm2ttl);
        else
          S_StartMusic (mus_intro);
        break;
      case 1:
        G_DeferedPlayDemo ("demo1");
        break;
      case 2:
        pagetic = 200;
        gamestate = GS_DEMOSCREEN;
        pagename = "CREDIT";
        break;
      case 3:
        G_DeferedPlayDemo ("demo2");
        break;
      case 4:
        gamestate = GS_DEMOSCREEN;
        if ( gamemode == commercial)
        {
            pagetic = 35 * 11;
            pagename = "TITLEPIC";
            S_StartMusic(mus_dm2ttl);
        }
        else
        {
            pagetic = 200;

            if ( gamemode == retail )
              pagename = "CREDIT";
            else
              pagename = "HELP2";
        }
        break;
      case 5:
        G_DeferedPlayDemo ("demo3");
        break;
        // THE DEFINITIVE DOOM Special Edition demo
      case 6:
        G_DeferedPlayDemo ("demo4");
        break;
    }
}



//
// D_StartTitle
//
void D_StartTitle (void)
{
    gameaction = ga_nothing;
    demosequence = -1;
    D_AdvanceDemo ();
}






//
// D_AddFile
//
void D_AddFile (char *file)
{
    int     numwadfiles;
    char    *newfile;

    for (numwadfiles = 0 ; wadfiles[numwadfiles] ; numwadfiles++)
        ;

    newfile = malloc (strlen(file)+1);
    strcpy (newfile, file);

    wadfiles[numwadfiles] = newfile;
}

//
// IdentifyVersion
//
void IdentifyVersion (void)
{
    devparm = false;

    // Manually select how to start
#if 0
    gamemode = commercial;
    language = french;
    D_AddFile ("doom2f.wad");
#elif 0
    gamemode = commercial;
    D_AddFile ("doom2.wad");
#elif 0
    gamemode = commercial;
    D_AddFile ("plutonia.wad");
#elif 0
    gamemode = commercial;
    D_AddFile ("tnt.wad");
#elif 1
    gamemode = retail;
    D_AddFile ("doomu.wad");
#elif 0
    gamemode = registered;
    D_AddFile ("doom.wad");
#elif 0
    gamemode = shareware;
    D_AddFile ("doom1.wad");
#else
    printf("Game mode indeterminate.\n");
    gamemode = indetermined;
#endif
}


//
// D_DoomMain
//
void D_DoomMain (void)
{
    IdentifyVersion ();

    //setbuf (stdout, NULL);
    modifiedgame = false;

    /* Static options */
    nomonsters  = false;
    respawnparm = false;
    fastparm    = false;
    devparm     = false;
    deathmatch  = 0;

    startskill   = sk_medium;
    startepisode = 1;
    startmap     = 1;
    autostart    = false;

    /* Custom title */
    printf ( "----------------------------\n"
             "RISC-V DOOM Startup v%i.%i\n"
             "----------------------------\n",
             VERSION/100,VERSION%100);

    // init subsystems
    printf ("V_Init: allocate screens.\n");
    V_Init ();

    printf ("M_LoadDefaults: Load system defaults.\n");
    M_LoadDefaults ();              // load before initing other systems

    printf ("Z_Init: Init zone memory allocation daemon. \n");
    Z_Init ();

    printf ("W_Init: Init WADfiles.\n");
    W_InitMultipleFiles (wadfiles);

    printf ("M_Init: Init miscellaneous info.\n");
    M_Init ();

    printf ("R_Init: Init DOOM refresh daemon - ");
    R_Init ();

    printf ("\nP_Init: Init Playloop state.\n");
    P_Init ();

    printf ("I_Init: Setting up machine state.\n");
    I_Init ();

    printf ("D_CheckNetGame: Checking network game status.\n");
    D_CheckNetGame ();

    printf ("S_Init: Setting up sound.\n");
    S_Init (snd_SfxVolume /* *8 */, snd_MusicVolume /* *8*/ );

    printf ("HU_Init: Setting up heads up display.\n");
    HU_Init ();

    printf ("ST_Init: Init status bar.\n");
    ST_Init ();

    if ( gameaction != ga_loadgame )
    {
        if (autostart || netgame)
            G_InitNew (startskill, startepisode, startmap);
        else
            D_StartTitle ();                // start up intro loop
    }

    D_DoomLoop ();  // never returns
}

#include "p_tick.h"

#include "Doom/Base/i_main.h"
#include "Doom/Base/m_random.h"
#include "Doom/Base/s_sound.h"
#include "Doom/Base/w_wad.h"
#include "Doom/Base/z_zone.h"
#include "Doom/d_main.h"
#include "Doom/Renderer/r_main.h"
#include "Doom/UI/am_main.h"
#include "Doom/UI/o_main.h"
#include "Doom/UI/st_main.h"
#include "g_game.h"
#include "info.h"
#include "p_base.h"
#include "p_mobj.h"
#include "p_sight.h"
#include "p_spec.h"
#include "p_user.h"
#include "PcPsx/Utils.h"
#include "PsxVm/PsxVm.h"
#include "PsyQ/LIBETC.h"
#include "PsyQ/LIBGPU.h"
#include "Wess/psxcd.h"
#include "Wess/psxspu.h"
#include "Wess/wessapi.h"

// The maximum level for the warp cheat.
// PC-PSX: For this version of the game I'm allowing the user to warp to the secret levels!
// If you're cheating you can more or less do anything anyway, so not much point in hiding these.
#if PC_PSX_DOOM_MODS
    static constexpr int32_t MAX_CHEAT_WARP_LEVEL = 59;
#else
    static constexpr int32_t MAX_CHEAT_WARP_LEVEL = 54;
#endif

const VmPtr<int32_t>                gVBlanksUntilMenuMove(0x80077EF8);      // How many 60 Hz ticks until we can move the cursor on the menu
const VmPtr<bool32_t>               gbGamePaused(0x80077EC0);               // Whether the game is currently paused by either player
const VmPtr<int32_t>                gMapNumToCheatWarpTo(0x80078270);       // What map the player currently has selected for cheat warp
const VmPtr<int32_t>                gVramViewerTexPage(0x80077ED4);         // What page of texture memory to display in the VRAM viewer
const VmPtr<uint32_t[MAXPLAYERS]>   gTicButtons(0x80077F44);                // Currently pressed buttons by all players
const VmPtr<uint32_t[MAXPLAYERS]>   gOldTicButtons(0x80078214);             // Previously pressed buttons by all players


static const VmPtr<int32_t>     gCurCheatBtnSequenceIdx(0x80077FE4);    // What button press in the cheat sequence we are currently on
static const VmPtr<int32_t>     gTicConOnPause(0x800782D8);             // What 60Hz tick we paused on, used to discount paused time on unpause

void P_AddThinker() noexcept {
loc_80028C38:
    v0 = 0x80090000;                                    // Result = 80090000
    v0 = lw(v0 + 0x6550);                               // Load from: gThinkerCap[0] (80096550)
    sw(a0, v0 + 0x4);
    v0 = 0x80090000;                                    // Result = 80090000
    v0 += 0x6550;                                       // Result = gThinkerCap[0] (80096550)
    sw(v0, a0 + 0x4);
    v1 = lw(v0);                                        // Load from: gThinkerCap[0] (80096550)
    sw(v1, a0);
    sw(a0, v0);                                         // Store to: gThinkerCap[0] (80096550)
    return;
}

void P_RemoveThinker() noexcept {
loc_80028C68:
    v0 = -1;                                            // Result = FFFFFFFF
    sw(v0, a0 + 0x8);
    return;
}

void P_RunThinkers() noexcept {
    sp -= 0x20;
    v0 = 0x80090000;                                    // Result = 80090000
    v0 += 0x6554;                                       // Result = gThinkerCap[1] (80096554)
    sw(ra, sp + 0x1C);
    sw(s2, sp + 0x18);
    sw(s1, sp + 0x14);
    sw(s0, sp + 0x10);
    s0 = lw(v0);                                        // Load from: gThinkerCap[1] (80096554)
    v0 -= 4;                                            // Result = gThinkerCap[0] (80096550)
    sw(0, gp + 0xD14);                                  // Store to: gNumActiveThinkers (800782F4)
    s2 = -1;                                            // Result = FFFFFFFF
    if (s0 == v0) goto loc_80028D14;
    s1 = v0;                                            // Result = gThinkerCap[0] (80096550)
loc_80028CA8:
    v0 = lw(s0 + 0x8);
    a1 = s0;
    if (v0 != s2) goto loc_80028CE4;
    v1 = lw(s0 + 0x4);
    v0 = lw(s0);
    a0 = *gpMainMemZone;
    sw(v0, v1);
    v1 = lw(s0);
    v0 = lw(s0 + 0x4);
    sw(v0, v1 + 0x4);
    _thunk_Z_Free2();
    goto loc_80028D04;
loc_80028CE4:
    if (v0 == 0) goto loc_80028CF4;
    a0 = s0;
    ptr_call(v0);
loc_80028CF4:
    v0 = lw(gp + 0xD14);                                // Load from: gNumActiveThinkers (800782F4)
    v0++;
    sw(v0, gp + 0xD14);                                 // Store to: gNumActiveThinkers (800782F4)
loc_80028D04:
    s0 = lw(s0 + 0x4);
    if (s0 != s1) goto loc_80028CA8;
loc_80028D14:
    ra = lw(sp + 0x1C);
    s2 = lw(sp + 0x18);
    s1 = lw(sp + 0x14);
    s0 = lw(sp + 0x10);
    sp += 0x20;
    return;
}

void P_RunMobjLate() noexcept {
    sp -= 0x20;
    v0 = 0x800B0000;                                    // Result = 800B0000
    v0 -= 0x715C;                                       // Result = gMObjHead[5] (800A8EA4)
    sw(ra, sp + 0x18);
    sw(s1, sp + 0x14);
    sw(s0, sp + 0x10);
    a0 = lw(v0);                                        // Load from: gMObjHead[5] (800A8EA4)
    v0 -= 0x14;                                         // Result = gMObjHead[0] (800A8E90)
    s1 = v0;                                            // Result = gMObjHead[0] (800A8E90)
    if (a0 == v0) goto loc_80028D7C;
loc_80028D58:
    v0 = lw(a0 + 0x18);
    s0 = lw(a0 + 0x14);
    if (v0 == 0) goto loc_80028D70;
    ptr_call(v0);
loc_80028D70:
    a0 = s0;
    if (a0 != s1) goto loc_80028D58;
loc_80028D7C:
    ra = lw(sp + 0x18);
    s1 = lw(sp + 0x14);
    s0 = lw(sp + 0x10);
    sp += 0x20;
    return;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Handles the following:
//  (1) Pausing/unpausing the game.
//  (2) Opening the options menu when the game is paused.
//  (3) Checking for cheat sequences in singleplayer.
//  (4) Controls for cheats that require them (VRAM viewer, level warp).
//------------------------------------------------------------------------------------------------------------------------------------------
void P_CheckCheats() noexcept {
    // Check for pause or options menu actions by any player.
    // Note that one player doing the action causes the action to happen for other players too.
    for (int32_t playerIdx = 1; playerIdx >= 0; --playerIdx) {
        // Skip this player if not in the game, otherwise grab inputs for the player
        if (!gbPlayerInGame[playerIdx])
            continue;

        const uint32_t padBtns = gTicButtons[playerIdx];
        const uint32_t oldPadBtns = gOldTicButtons[playerIdx];
        
        // Toggling pause?
        if (padBtnJustPressed(PADstart, padBtns, oldPadBtns)) {
            *gbGamePaused = (!*gbGamePaused);

            // Handle the game being paused, if just pausing
            if (*gbGamePaused) {
                // Pause all audio
                psxcd_pause();
                
                a0 = 0xD;
                wess_seq_stop();
                
                a0 = 0xE;
                wess_seq_stop();

                S_Pause();

                // Remember the tick we paused on and reset cheat button sequences
                *gCurCheatBtnSequenceIdx = 0;
                *gTicConOnPause = *gTicCon;
                return;
            }

            // Otherwise restart audio
            a0 = 0;
            psxcd_restart();

            do {
                psxcd_seeking_for_play();
            } while (v0 != 0);

            a0 = 500;
            a1 = lw(0x800775F8);        // Load from: gCdMusicVol (800775F8)            
            psxspu_start_cd_fade();

            S_Resume();

            // Not sure why this is hardcoded to clear this flag specifically for player 2? Seems rather strange...
            gPlayers[1].cheats &= ~(CF_VRAMVIEWER|CF_WARPMENU);

            // Restore previous tick counters on unpause
            *gTicCon = *gTicConOnPause;
            *gLastTgtGameTicCount = *gTicConOnPause / 2;
        }

        // Showing the options menu if the game is paused and the options button has just been pressed.
        // Otherwise do not do any of the logic below...
        if ((!padBtnJustPressed(PADselect, padBtns, oldPadBtns)) || (!*gbGamePaused))
            continue;
        
        // About to open up the options menu, disable these player cheats and present what we have to the screen
        player_t& player = gPlayers[playerIdx];
        player.cheats &= ~(CF_VRAMVIEWER|CF_WARPMENU);
        I_DrawPresent();

        // Run the options menu and before restarting or exiting a demo, do one final draw.
        const gameaction_t optionsAction = MiniLoop(O_Init, O_Shutdown, O_Control, O_Drawer);
        
        if (optionsAction != ga_exit) {
            *gGameAction = optionsAction;

            if (optionsAction == ga_restart || optionsAction == ga_exitdemo) {
                O_Drawer();
            }
        }
        
        return;
    }

    // Cheats are disallowed in a multiplayer game
    if (*gNetGame != gt_single)
        return;

    // Grab inputs for the 1st player.
    // The rest of the cheat logic is for singleplayer mode only!
    const uint32_t padBtns = gTicButtons[0];
    const uint32_t oldPadBtns = gOldTicButtons[0];

    if (padBtns == 0) {
        *gVBlanksUntilMenuMove = 0;
    }
    
    // Are we showing the cheat warp menu?
    // If so then do the controls for that and exit.
    player_t& player = gPlayers[0];
    
    if (player.cheats & CF_WARPMENU) {
        *gVBlanksUntilMenuMove -= gPlayersElapsedVBlanks[0];

        if (*gVBlanksUntilMenuMove <= 0) {
            if ((padBtns & PADLleft) != 0) {
                *gMapNumToCheatWarpTo -= 1;

                if (*gMapNumToCheatWarpTo <= 0) {
                    *gMapNumToCheatWarpTo = 1;
                }
                
                *gVBlanksUntilMenuMove = MENU_MOVE_VBLANK_DELAY;
            }
            else if ((padBtns & PADLright) != 0) {
                *gMapNumToCheatWarpTo += 1;

                if (*gMapNumToCheatWarpTo > MAX_CHEAT_WARP_LEVEL) {
                    *gMapNumToCheatWarpTo = MAX_CHEAT_WARP_LEVEL;
                }

                *gVBlanksUntilMenuMove = MENU_MOVE_VBLANK_DELAY;
            }
        }

        // Are we initiating the the actual warp?
        if (padBtns != oldPadBtns) {
            if (padBtns & (PADRup|PADRdown|PADRleft|PADRright)) {
                // Button pressed to initiate the level warp - kick it off!
                *gGameAction = ga_warped;
                player.cheats &= (~CF_WARPMENU);
                *gStartMapOrEpisode = *gMapNumToCheatWarpTo;
                *gGameMap = *gMapNumToCheatWarpTo;
            }
        }

        return;
    }

    // Are we showing the VRAM viewer?
    // If so then do the controls for that and exit.
    if (player.cheats & CF_VRAMVIEWER) {
        if (padBtns != oldPadBtns) {
            if (padBtns & PADLleft) {
                --*gVramViewerTexPage;

                if (*gVramViewerTexPage < 0) {
                    *gVramViewerTexPage = 0;
                }
            }
            else if (padBtns & PADLright) {
                ++*gVramViewerTexPage;

                if (*gVramViewerTexPage > 10) {
                    *gVramViewerTexPage = 10;
                }
            }
        }

        return;
    }

    // Check for cheat sequences if the game is paused and buttons are pressed
    if ((!*gbGamePaused) || (!padBtns) || (padBtns == oldPadBtns))
        return;

    t6 = 0x800A91A4;                                    // Result = gCheatSequenceBtns[0] (800A91A4)
    t3 = 0x80098740;                                    // Result = gpStatusBarMsgStr (80098740)
    t4 = t3 + 4;                                        // Result = gStatusBarMsgTicsLeft (80098744)
    t2 = 1;
    t1 = 0x80067778;                                    // Result = CheatSequenceButtons[0] (80067778)
    at = 0x800A91A4;                                    // Result = gCheatSequenceBtns[0] (800A91A4)                                     
    at += *gCurCheatBtnSequenceIdx * 2;
    sh(padBtns, at);
    ++*gCurCheatBtnSequenceIdx;

    uint32_t cheatIdx = 0;
    a3 = ptrToVmAddr(&player);

    do {
        t0 = *gCurCheatBtnSequenceIdx;
        a0 = 0;

        if (i32(t0) > 0) {
            a2 = t1;
            a1 = t6;                // Result = gCheatSequenceBtns[0] (800A91A4)

            do {
                if (lh(a1) != lh(a2))
                    break;

                a2 += 2;
                a0++;
                a1 += 2;
            } while (i32(a0) < i32(t0));
        }

        if (i32(a0) >= 8) {
            switch (cheatIdx) {
                // Toggle show all map lines cheat
                case 0: {
                    player.cheats ^= CF_ALLLINES;

                    if (player.cheats & CF_ALLLINES) {
                        v0 = 0x80011060;                                    // Result = STR_Cheat_AllMapLines_On[0] (80011060)
                        sw(v0, t3);
                        sw(t2, t4);
                    } else {
                        v0 = 0x80011074;                                    // Result = STR_Cheat_AllMapLines_Off[0] (80011074)                                    
                        sw(v0, t3);
                        sw(t2, t4);
                    }
                }   break;

                // Toggle show all map things cheat
                case 1: {
                    player.cheats ^= CF_ALLMOBJ;

                    if (player.cheats & CF_ALLMOBJ) {
                        v0 = 0x80011088;                                    // Result = STR_Cheat_AllMapThings_On[0] (80011088)
                        sw(v0, t3);
                        sw(t2, t4);
                    } else {
                        v0 = 0x8001109C;                                    // Result = STR_Cheat_AllMapThings_Off[0] (8001109C)                                  
                        sw(v0, t3);
                        sw(t2, t4);
                    }
                }   break;

                // Toggle god mode cheat
                case 2: {
                    player.cheats ^= CF_GODMODE;

                    if (player.cheats & CF_GODMODE) {
                        player.health = 100;
                        player.mo->health = 100;
                        v0 = 0x800110B0;                                    // Result = STR_Cheat_GodMode_On[0] (800110B0)
                        sw(v0, t3);
                        sw(t2, t4);
                    } else {
                        v0 = 0x800110C8;                                    // Result = STR_Cheat_GodMode_Off[0] (800110C8)
                        sw(v0, t3);
                        sw(t2, t4);
                    }
                }   break;
                
                // Weapons ammo and keys cheat
                case 3: {
                    v0 = 0x800B0000;                                    // Result = 800B0000
                    v0 -= 0x715C;                                       // Result = gMObjHead[5] (800A8EA4)
                    a0 = lw(v0);                                        // Load from: gMObjHead[5] (800A8EA4)
                    v0 -= 0x14;                                         // Result = gMObjHead[0] (800A8E90)

                    if (a0 != v0) {
                        a1 = v0;                                        // Result = gMObjHead[0] (800A8E90)
        
                        // TODO: looks like this code is granting keys based on mobjs in the level
                        do {
                            v0 = lw(a0 + 0x54);            
                            const int32_t sval = v0;

                            switch (sval) {
                                case MT_MISC4: sw(t2, a3 + 0x4C); break;
                                case MT_MISC5: sw(t2, a3 + 0x48); break;
                                case MT_MISC6: sw(t2, a3 + 0x50); break;
                                case MT_MISC7: sw(t2, a3 + 0x5C); break;
                                case MT_MISC8: sw(t2, a3 + 0x54); break;
                                case MT_MISC9: sw(t2, a3 + 0x58); break;
                            }

                            a0 = lw(a0 + 0x14);
                        } while (a0 != a1);
                    }

                    a0 = 8;
                    v1 = a3 + 0x20;
                    v0 = 0xC8;
                    sw(v0, a3 + 0x28);
                    v0 = 2;
                    sw(v0, a3 + 0x2C);

                    do {
                        sw(t2, v1 + 0x74);
                        a0--;
                        v1 -= 4;
                    } while (i32(a0) >= 0);

                    a0 = 0;
                    v1 = a3;

                    do {
                        v0 = lw(v1 + 0xA8);
                        a0++;
                        sw(v0, v1 + 0x98);
                        v0 = (i32(a0) < 4);
                        v1 += 4;
                    } while (v0 != 0);

                    v0 = 0x800110E0;                                    // Result = STR_Cheat_LotsOfGoodies[0] (800110E0)
                    sw(v0, t3);
                    sw(t2, t3 + 0x4);
                }   break;

                // Level warp cheat, bring up the warp menu
                case 5: {
                    player.cheats |= CF_WARPMENU;
                    
                    if (*gGameMap > MAX_CHEAT_WARP_LEVEL) {
                        *gMapNumToCheatWarpTo = MAX_CHEAT_WARP_LEVEL;
                    } else {
                        *gMapNumToCheatWarpTo = *gGameMap;
                    }
                }   break;

                // Enable/disable 'xray vision' cheat
                case 9:
                    player.cheats ^= CF_XRAYVISION;
                    break;

                // Unused cheat slots
                case 4:
                case 6:
                case 7:
                case 8:
                    break;
            }
            
            // A full cheat sequence (8 buttons) was entered - abort the loop
            break;
        } else {
            ++cheatIdx;
            t1 += 0x10;
        }
    } while (cheatIdx < 12);    // TODO: USE A CONSTANT

    // Wraparound this if we need to!
    *gCurCheatBtnSequenceIdx %= 8;      // TODO: USE A CONSTANT
}

void P_Ticker() noexcept {
    sp -= 0x20;
    sw(ra, sp + 0x1C);
    sw(s2, sp + 0x18);
    sw(s1, sp + 0x14);
    sw(s0, sp + 0x10);
    *gGameAction = ga_nothing;
    P_CheckCheats();
    v0 = *gbGamePaused;
    if (v0 != 0) goto loc_8002955C;
    v1 = *gGameTic;
    v0 = *gPrevGameTic;
    v0 = (i32(v0) < i32(v1));
    if (v0 == 0) goto loc_8002955C;
    s0 = 0x80090000;                                    // Result = 80090000
    s0 = lw(s0 + 0x6554);                               // Load from: gThinkerCap[1] (80096554)
    v0 = 0x80090000;                                    // Result = 80090000
    v0 += 0x6550;                                       // Result = gThinkerCap[0] (80096550)
    sw(0, gp + 0xD14);                                  // Store to: gNumActiveThinkers (800782F4)
    if (s0 == v0) goto loc_800294F8;
    s2 = -1;                                            // Result = FFFFFFFF
    s1 = v0;                                            // Result = gThinkerCap[0] (80096550)
loc_8002948C:
    v0 = lw(s0 + 0x8);
    a1 = s0;
    if (v0 != s2) goto loc_800294C8;
    v1 = lw(s0 + 0x4);
    v0 = lw(s0);
    a0 = *gpMainMemZone;
    sw(v0, v1);
    v1 = lw(s0);
    v0 = lw(s0 + 0x4);
    sw(v0, v1 + 0x4);
    _thunk_Z_Free2();
    goto loc_800294E8;
loc_800294C8:
    if (v0 == 0) goto loc_800294D8;
    a0 = s0;
    ptr_call(v0);
loc_800294D8:
    v0 = lw(gp + 0xD14);                                // Load from: gNumActiveThinkers (800782F4)
    v0++;
    sw(v0, gp + 0xD14);                                 // Store to: gNumActiveThinkers (800782F4)
loc_800294E8:
    s0 = lw(s0 + 0x4);
    if (s0 != s1) goto loc_8002948C;
loc_800294F8:
    P_CheckSights();
    P_RunMobjBase();
    a0 = 0x800B0000;                                    // Result = 800B0000
    a0 = lw(a0 - 0x715C);                               // Load from: gMObjHead[5] (800A8EA4)
    v0 = 0x800B0000;                                    // Result = 800B0000
    v0 -= 0x7170;                                       // Result = gMObjHead[0] (800A8E90)
    s1 = v0;                                            // Result = gMObjHead[0] (800A8E90)
    if (a0 == v0) goto loc_80029544;
loc_80029520:
    v0 = lw(a0 + 0x18);
    s0 = lw(a0 + 0x14);
    if (v0 == 0) goto loc_80029538;
    ptr_call(v0);
loc_80029538:
    a0 = s0;
    if (a0 != s1) goto loc_80029520;
loc_80029544:
    P_UpdateSpecials();
    P_RespawnSpecials();
    ST_Ticker();
loc_8002955C:
    sw(0, gp + 0xD0C);                                  // Store to: gPlayerNum (800782EC)
    s0 = 0x800B0000;                                    // Result = 800B0000
    s0 -= 0x7814;                                       // Result = gPlayer1[0] (800A87EC)
    s2 = 0x80080000;                                    // Result = 80080000
    s2 -= 0x7F54;                                       // Result = gbPlayerInGame[0] (800780AC)
    s1 = 2;                                             // Result = 00000002
loc_80029574:
    a0 = lw(gp + 0xD0C);                                // Load from: gPlayerNum (800782EC)
    v0 = a0 << 2;
    v0 += s2;
    v0 = lw(v0);
    if (v0 == 0) goto loc_800295BC;
    v0 = lw(s0 + 0x4);
    if (v0 != s1) goto loc_800295AC;
    G_DoReborn();
loc_800295AC:
    a0 = s0;
    AM_Control();
    a0 = s0;
    P_PlayerThink();
loc_800295BC:
    v0 = lw(gp + 0xD0C);                                // Load from: gPlayerNum (800782EC)
    v0++;
    sw(v0, gp + 0xD0C);                                 // Store to: gPlayerNum (800782EC)
    v0 = (i32(v0) < 2);
    s0 += 0x12C;
    if (v0 != 0) goto loc_80029574;
    v0 = *gGameAction;
    ra = lw(sp + 0x1C);
    s2 = lw(sp + 0x18);
    s1 = lw(sp + 0x14);
    s0 = lw(sp + 0x10);
    sp += 0x20;
    return;
}

void P_Drawer() noexcept {
    sp -= 0x18;
    sw(ra, sp + 0x10);
    I_IncDrawnFrameCount();
    v0 = *gCurPlayerIndex;
    v1 = v0 << 2;
    v1 += v0;
    v0 = v1 << 4;
    v0 -= v1;
    v0 <<= 2;
    at = 0x800B0000;                                    // Result = 800B0000
    at -= 0x76F0;                                       // Result = gPlayer1[49] (800A8910)
    at += v0;
    v0 = lw(at);
    v0 &= 1;
    if (v0 == 0) goto loc_8002965C;
    AM_Drawer();
    goto loc_80029664;
loc_8002965C:
    R_RenderPlayerView();
loc_80029664:
    ST_Drawer();
    I_SubmitGpuCmds();
    ra = lw(sp + 0x10);
    sp += 0x18;
    return;
}

void P_Start() noexcept {
    sp -= 0x28;
    sw(s0, sp + 0x20);
    s0 = 1;                                             // Result = 00000001
    sw(ra, sp + 0x24);
    *gbGamePaused = false;
    at = 0x80070000;                                    // Result = 80070000
    sw(s0, at + 0x7BC4);                                // Store to: gValidCount (80077BC4)
    AM_Start();
    M_ClearRandom();
    v0 = 0x80080000;                                    // Result = 80080000
    v0 = *gbDemoPlayback;
    *gbIsLevelDataCached = (s0 != 0);
    a2 = 0;                                             // Result = 00000000
    if (v0 == 0) goto loc_80029700;
    v1 = 0x80070000;                                    // Result = 80070000
    v1 += 0x3E54;                                       // Result = CDTrackNum_Credits_Demo (80073E54)
    a1 = 0x80070000;                                    // Result = 80070000
    a1 = lw(a1 + 0x75F8);                               // Load from: gCdMusicVol (800775F8)
    v0 = lw(v1);                                        // Load from: CDTrackNum_Credits_Demo (80073E54)
    sw(0, sp + 0x18);
    sw(0, sp + 0x1C);
    sw(v0, sp + 0x10);
    sw(a1, sp + 0x14);
    a0 = lw(v1);                                        // Load from: CDTrackNum_Credits_Demo (80073E54)
    a3 = 0;                                             // Result = 00000000
    psxcd_play_at_andloop();
    goto loc_80029708;
loc_80029700:
    S_StartMusicSequence();
loc_80029708:
    ra = lw(sp + 0x24);
    s0 = lw(sp + 0x20);
    sp += 0x28;
    return;
}

void P_Stop() noexcept {
    sp -= 0x20;
    a0 = 0;                                             // Result = 00000000
    sw(ra, sp + 0x18);
    sw(s1, sp + 0x14);
    sw(s0, sp + 0x10);
    LIBGPU_DrawSync();
    s0 = 0;                                             // Result = 00000000
    S_Clear();
    psxcd_stop();
    S_StopMusicSequence();
    s1 = 0x80080000;                                    // Result = 80080000
    s1 -= 0x7F54;                                       // Result = gbPlayerInGame[0] (800780AC)
    *gbGamePaused = false;
    *gbIsLevelDataCached = false;
loc_80029760:
    v0 = lw(s1);
    s1 += 4;
    if (v0 == 0) goto loc_80029778;
    a0 = s0;
    G_PlayerFinishLevel();
loc_80029778:
    s0++;
    v0 = (i32(s0) < 2);
    if (v0 != 0) goto loc_80029760;
    ra = lw(sp + 0x18);
    s1 = lw(sp + 0x14);
    s0 = lw(sp + 0x10);
    sp += 0x20;
    return;
}

#pragma once

//------------------------------------------------------------------------------------------------------------------------------------------
// Module containing info on map object finite state machine states, properties and sprite names.
// Contains all of the data for all of the objects that inhabit the game world, essentially.
// On PC this would be the same data that you would edit via 'DeHackEd' patches.
//------------------------------------------------------------------------------------------------------------------------------------------
#include "Doom/doomdef.h"
#include "SmallString.h"

#include <cstddef>

enum sfxenum_t : int32_t;

// PsyDoom: Helper typedefs for the two possible forms of state_t function: one for map objects (things) and one for player sprites (weapons)
typedef void (*statefn_mobj_t)(mobj_t&) noexcept;
typedef void (*statefn_pspr_t)(player_t&, pspdef_t&) noexcept;

// Indexes into the array of sprite lump names
enum spritenum_t : int32_t {
    SPR_TROO,
    SPR_SHTG,
    SPR_PUNG,
    SPR_PISG,
    SPR_PISF,
    SPR_SHTF,
    SPR_SHT2,
    SPR_CHGG,
    SPR_CHGF,
    SPR_MISG,
    SPR_MISF,
    SPR_SAWG,
    SPR_PLSG,
    SPR_PLSF,
    SPR_BFGG,
    SPR_BFGF,
    SPR_BLUD,
    SPR_PUFF,
    SPR_BAL1,
    SPR_BAL2,
    SPR_BAL7,
    SPR_PLSS,
    SPR_PLSE,
    SPR_MISL,
    SPR_BFS1,
    SPR_BFE1,
    SPR_BFE2,
    SPR_TFOG,
    SPR_IFOG,
    SPR_PLAY,
    SPR_POSS,
    SPR_SPOS,
    SPR_FATB,
    SPR_FBXP,
    SPR_SKEL,
    SPR_MANF,
    SPR_FATT,
    SPR_CPOS,
    SPR_SARG,
    SPR_HEAD,
    SPR_BOSS,
    SPR_BOS2,
    SPR_SKUL,
    SPR_SPID,
    SPR_BSPI,
    SPR_APLS,
    SPR_APBX,
    SPR_CYBR,
    SPR_PAIN,
    SPR_ARM1,
    SPR_ARM2,
    SPR_BAR1,
    SPR_BEXP,
    SPR_FCAN,
    SPR_BON1,
    SPR_BON2,
    SPR_BKEY,
    SPR_RKEY,
    SPR_YKEY,
    SPR_BSKU,
    SPR_RSKU,
    SPR_YSKU,
    SPR_STIM,
    SPR_MEDI,
    SPR_SOUL,
    SPR_PINV,
    SPR_PSTR,
    SPR_PINS,
    SPR_MEGA,
    SPR_SUIT,
    SPR_PMAP,
    SPR_PVIS,
    SPR_CLIP,
    SPR_AMMO,
    SPR_ROCK,
    SPR_BROK,
    SPR_CELL,
    SPR_CELP,
    SPR_SHEL,
    SPR_SBOX,
    SPR_BPAK,
    SPR_BFUG,
    SPR_MGUN,
    SPR_CSAW,
    SPR_LAUN,
    SPR_PLAS,
    SPR_SHOT,
    SPR_SGN2,
    SPR_COLU,
    SPR_SMT2,
    SPR_POL2,
    SPR_POL5,
    SPR_POL4,
    SPR_POL1,
    SPR_GOR2,
    SPR_GOR3,
    SPR_GOR4,
    SPR_GOR5,
    SPR_SMIT,
    SPR_COL1,
    SPR_COL2,
    SPR_COL3,
    SPR_COL4,
    SPR_COL6,
    SPR_CAND,
    SPR_CBRA,
    SPR_TRE1,
    SPR_ELEC,
    SPR_FSKU,
    SPR_SMBT,
    SPR_SMGT,
    SPR_SMRT,
    SPR_HANC,
    SPR_BLCH,
    SPR_HANL,
    SPR_DED1,
    SPR_DED2,
    SPR_DED3,
    SPR_DED4,
    SPR_DED5,
    SPR_DED6,
    SPR_TLMP,
    SPR_TLP2,
    SPR_COL5,
    SPR_CEYE,
    SPR_TBLU,
    SPR_TGRN,
    SPR_TRED,
    SPR_GOR1,
    SPR_POL3,
    SPR_POL6,
    SPR_TRE2,
    SPR_HDB1,
    SPR_HDB2,
    SPR_HDB3,
    SPR_HDB4,
    SPR_HDB5,
    SPR_HDB6,
    SPR_POB1,
    SPR_POB2,
    SPR_BRS1,
// PsyDoom: adding missing PC DOOM II sprites back in
#if PSYDOOM_MODS
    SPR_BBRN,       // The 'Icon Of Sin'
    SPR_BOSF,       // Icon Of Sin spawner box
    SPR_FIRE,       // Arch-vile fire
    SPR_KEEN,       // Hanging Commander Keen
    SPR_SSWV,       // Wolfstein SS
    SPR_VILE,       // This one needs no introduction...
#endif
    BASE_NUM_SPRITES    // PsyDoom: renamed this from 'NUMSPRITES' to 'BASE_NUM_SPRITES' because it's now just a count of the number of built-in sprites
};

// Indexes into the array of states
enum statenum_t : int32_t {
    S_NULL,
    S_LIGHTDONE,
    S_PUNCH,
    S_PUNCHDOWN,
    S_PUNCHUP,
    S_PUNCH1,
    S_PUNCH2,
    S_PUNCH3,
    S_PUNCH4,
    S_PUNCH5,
    S_PISTOL,
    S_PISTOLDOWN,
    S_PISTOLUP,
    S_PISTOL1,
    S_PISTOL2,
    S_PISTOL3,
    S_PISTOL4,
    S_PISTOLFLASH,
    S_SGUN,
    S_SGUNDOWN,
    S_SGUNUP,
    S_SGUN1,
    S_SGUN2,
    S_SGUN3,
    S_SGUN4,
    S_SGUN5,
    S_SGUN6,
    S_SGUN7,
    S_SGUN8,
    S_SGUN9,
    S_SGUNFLASH1,
    S_SGUNFLASH2,
    S_DSGUN,
    S_DSGUNDOWN,
    S_DSGUNUP,
    S_DSGUN1,
    S_DSGUN2,
    S_DSGUN3,
    S_DSGUN4,
    S_DSGUN5,
    S_DSGUN6,
    S_DSGUN7,
    S_DSGUN8,
    S_DSGUN9,
    S_DSGUN10,
    S_DSNR1,
    S_DSNR2,
    S_DSGUNFLASH1,
    S_DSGUNFLASH2,
    S_CHAIN,
    S_CHAINDOWN,
    S_CHAINUP,
    S_CHAIN1,
    S_CHAIN2,
    S_CHAIN3,
    S_CHAINFLASH1,
    S_CHAINFLASH2,
    S_MISSILE,
    S_MISSILEDOWN,
    S_MISSILEUP,
    S_MISSILE1,
    S_MISSILE2,
    S_MISSILE3,
    S_MISSILEFLASH1,
    S_MISSILEFLASH2,
    S_MISSILEFLASH3,
    S_MISSILEFLASH4,
    S_SAW,
    S_SAWB,
    S_SAWDOWN,
    S_SAWUP,
    S_SAW1,
    S_SAW2,
    S_SAW3,
    S_PLASMA,
    S_PLASMADOWN,
    S_PLASMAUP,
    S_PLASMA1,
    S_PLASMA2,
    S_PLASMAFLASH1,
    S_PLASMAFLASH2,
    S_BFG,
    S_BFGDOWN,
    S_BFGUP,
    S_BFG1,
    S_BFG2,
    S_BFG3,
    S_BFG4,
    S_BFGFLASH1,
    S_BFGFLASH2,
    S_BLOOD1,
    S_BLOOD2,
    S_BLOOD3,
    S_PUFF1,
    S_PUFF2,
    S_PUFF3,
    S_PUFF4,
    S_TBALL1,
    S_TBALL2,
    S_TBALLX1,
    S_TBALLX2,
    S_TBALLX3,
    S_RBALL1,
    S_RBALL2,
    S_RBALLX1,
    S_RBALLX2,
    S_RBALLX3,
    S_BRBALL1,
    S_BRBALL2,
    S_BRBALLX1,
    S_BRBALLX2,
    S_BRBALLX3,
    S_PLASBALL,
    S_PLASBALL2,
    S_PLASEXP,
    S_PLASEXP2,
    S_PLASEXP3,
    S_PLASEXP4,
    S_PLASEXP5,
    S_ROCKET,
    S_BFGSHOT,
    S_BFGSHOT2,
    S_BFGLAND,
    S_BFGLAND2,
    S_BFGLAND3,
    S_BFGLAND4,
    S_BFGLAND5,
    S_BFGLAND6,
    S_BFGEXP,
    S_BFGEXP2,
    S_BFGEXP3,
    S_BFGEXP4,
    S_EXPLODE1,
    S_EXPLODE2,
    S_EXPLODE3,
    S_TFOG,
    S_TFOG01,
    S_TFOG02,
    S_TFOG2,
    S_TFOG3,
    S_TFOG4,
    S_TFOG5,
    S_TFOG6,
    S_TFOG7,
    S_TFOG8,
    S_TFOG9,
    S_TFOG10,
    S_IFOG,
    S_IFOG01,
    S_IFOG02,
    S_IFOG2,
    S_IFOG3,
    S_IFOG4,
    S_IFOG5,
    S_PLAY,
    S_PLAY_RUN1,
    S_PLAY_RUN2,
    S_PLAY_RUN3,
    S_PLAY_RUN4,
    S_PLAY_ATK1,
    S_PLAY_ATK2,
    S_PLAY_PAIN,
    S_PLAY_PAIN2,
    S_PLAY_DIE1,
    S_PLAY_DIE2,
    S_PLAY_DIE3,
    S_PLAY_DIE4,
    S_PLAY_DIE5,
    S_PLAY_DIE6,
    S_PLAY_DIE7,
    S_PLAY_XDIE1,
    S_PLAY_XDIE2,
    S_PLAY_XDIE3,
    S_PLAY_XDIE4,
    S_PLAY_XDIE5,
    S_PLAY_XDIE6,
    S_PLAY_XDIE7,
    S_PLAY_XDIE8,
    S_PLAY_XDIE9,
    S_POSS_STND,
    S_POSS_STND2,
    S_POSS_RUN1,
    S_POSS_RUN2,
    S_POSS_RUN3,
    S_POSS_RUN4,
    S_POSS_RUN5,
    S_POSS_RUN6,
    S_POSS_RUN7,
    S_POSS_RUN8,
    S_POSS_ATK1,
    S_POSS_ATK2,
    S_POSS_ATK3,
    S_POSS_PAIN,
    S_POSS_PAIN2,
    S_POSS_DIE1,
    S_POSS_DIE2,
    S_POSS_DIE3,
    S_POSS_DIE4,
    S_POSS_DIE5,
    S_POSS_XDIE1,
    S_POSS_XDIE2,
    S_POSS_XDIE3,
    S_POSS_XDIE4,
    S_POSS_XDIE5,
    S_POSS_XDIE6,
    S_POSS_XDIE7,
    S_POSS_XDIE8,
    S_POSS_XDIE9,
// PsyDoom: reintroducing the Arch-vile (resurrection states)
#if PSYDOOM_MODS
    S_POSS_RAISE1,
    S_POSS_RAISE2,
    S_POSS_RAISE3,
    S_POSS_RAISE4,
#endif
    S_SPOS_STND,
    S_SPOS_STND2,
    S_SPOS_RUN1,
    S_SPOS_RUN2,
    S_SPOS_RUN3,
    S_SPOS_RUN4,
    S_SPOS_RUN5,
    S_SPOS_RUN6,
    S_SPOS_RUN7,
    S_SPOS_RUN8,
    S_SPOS_ATK1,
    S_SPOS_ATK2,
    S_SPOS_ATK3,
    S_SPOS_PAIN,
    S_SPOS_PAIN2,
    S_SPOS_DIE1,
    S_SPOS_DIE2,
    S_SPOS_DIE3,
    S_SPOS_DIE4,
    S_SPOS_DIE5,
    S_SPOS_XDIE1,
    S_SPOS_XDIE2,
    S_SPOS_XDIE3,
    S_SPOS_XDIE4,
    S_SPOS_XDIE5,
    S_SPOS_XDIE6,
    S_SPOS_XDIE7,
    S_SPOS_XDIE8,
    S_SPOS_XDIE9,
// PsyDoom: reintroducing the Arch-vile (resurrection states)
#if PSYDOOM_MODS
    S_SPOS_RAISE1,
    S_SPOS_RAISE2,
    S_SPOS_RAISE3,
    S_SPOS_RAISE4,
    S_SPOS_RAISE5,
#endif
    S_SMOKE1,
    S_SMOKE2,
    S_SMOKE3,
    S_SMOKE4,
    S_SMOKE5,
    S_TRACER,
    S_TRACER2,
    S_TRACEEXP1,
    S_TRACEEXP2,
    S_TRACEEXP3,
    S_SKEL_STND,
    S_SKEL_STND2,
    S_SKEL_RUN1,
    S_SKEL_RUN2,
    S_SKEL_RUN3,
    S_SKEL_RUN4,
    S_SKEL_RUN5,
    S_SKEL_RUN6,
    S_SKEL_RUN7,
    S_SKEL_RUN8,
    S_SKEL_RUN9,
    S_SKEL_RUN10,
    S_SKEL_RUN11,
    S_SKEL_RUN12,
    S_SKEL_FIST1,
    S_SKEL_FIST2,
    S_SKEL_FIST3,
    S_SKEL_FIST4,
    S_SKEL_MISS1,
    S_SKEL_MISS2,
    S_SKEL_MISS3,
    S_SKEL_MISS4,
    S_SKEL_PAIN,
    S_SKEL_PAIN2,
    S_SKEL_DIE1,
    S_SKEL_DIE2,
    S_SKEL_DIE3,
    S_SKEL_DIE4,
    S_SKEL_DIE5,
    S_SKEL_DIE6,
// PsyDoom: reintroducing the Arch-vile (resurrection states)
#if PSYDOOM_MODS
    S_SKEL_RAISE1,
    S_SKEL_RAISE2,
    S_SKEL_RAISE3,
    S_SKEL_RAISE4,
    S_SKEL_RAISE5,
    S_SKEL_RAISE6,
#endif
    S_FATSHOT1,
    S_FATSHOT2,
    S_FATSHOTX1,
    S_FATSHOTX2,
    S_FATSHOTX3,
    S_FATT_STND,
    S_FATT_STND2,
    S_FATT_RUN1,
    S_FATT_RUN2,
    S_FATT_RUN3,
    S_FATT_RUN4,
    S_FATT_RUN5,
    S_FATT_RUN6,
    S_FATT_RUN7,
    S_FATT_RUN8,
    S_FATT_RUN9,
    S_FATT_RUN10,
    S_FATT_RUN11,
    S_FATT_RUN12,
    S_FATT_ATK1,
    S_FATT_ATK2,
    S_FATT_ATK3,
    S_FATT_ATK4,
    S_FATT_ATK5,
    S_FATT_ATK6,
    S_FATT_ATK7,
    S_FATT_ATK8,
    S_FATT_ATK9,
    S_FATT_ATK10,
    S_FATT_PAIN,
    S_FATT_PAIN2,
    S_FATT_DIE1,
    S_FATT_DIE2,
    S_FATT_DIE3,
    S_FATT_DIE4,
    S_FATT_DIE5,
    S_FATT_DIE6,
    S_FATT_DIE7,
    S_FATT_DIE8,
    S_FATT_DIE9,
    S_FATT_DIE10,
// PsyDoom: reintroducing the Arch-vile (resurrection states)
#if PSYDOOM_MODS
    S_FATT_RAISE1,
    S_FATT_RAISE2,
    S_FATT_RAISE3,
    S_FATT_RAISE4,
    S_FATT_RAISE5,
    S_FATT_RAISE6,
    S_FATT_RAISE7,
    S_FATT_RAISE8,
#endif
    S_CPOS_STND,
    S_CPOS_STND2,
    S_CPOS_RUN1,
    S_CPOS_RUN2,
    S_CPOS_RUN3,
    S_CPOS_RUN4,
    S_CPOS_RUN5,
    S_CPOS_RUN6,
    S_CPOS_RUN7,
    S_CPOS_RUN8,
    S_CPOS_ATK1,
    S_CPOS_ATK2,
    S_CPOS_ATK3,
    S_CPOS_ATK4,
    S_CPOS_PAIN,
    S_CPOS_PAIN2,
    S_CPOS_DIE1,
    S_CPOS_DIE2,
    S_CPOS_DIE3,
    S_CPOS_DIE4,
    S_CPOS_DIE5,
    S_CPOS_DIE6,
    S_CPOS_DIE7,
    S_CPOS_XDIE1,
    S_CPOS_XDIE2,
    S_CPOS_XDIE3,
    S_CPOS_XDIE4,
    S_CPOS_XDIE5,
    S_CPOS_XDIE6,
// PsyDoom: reintroducing the Arch-vile (resurrection states)
#if PSYDOOM_MODS
    S_CPOS_RAISE1,
    S_CPOS_RAISE2,
    S_CPOS_RAISE3,
    S_CPOS_RAISE4,
    S_CPOS_RAISE5,
    S_CPOS_RAISE6,
    S_CPOS_RAISE7,
#endif
    S_TROO_STND,
    S_TROO_STND2,
    S_TROO_RUN1,
    S_TROO_RUN2,
    S_TROO_RUN3,
    S_TROO_RUN4,
    S_TROO_RUN5,
    S_TROO_RUN6,
    S_TROO_RUN7,
    S_TROO_RUN8,
    S_TROO_ATK1,
    S_TROO_ATK2,
    S_TROO_ATK3,
    S_TROO_PAIN,
    S_TROO_PAIN2,
    S_TROO_DIE1,
    S_TROO_DIE2,
    S_TROO_DIE3,
    S_TROO_DIE4,
    S_TROO_DIE5,
    S_TROO_XDIE1,
    S_TROO_XDIE2,
    S_TROO_XDIE3,
    S_TROO_XDIE4,
    S_TROO_XDIE5,
    S_TROO_XDIE6,
    S_TROO_XDIE7,
    S_TROO_XDIE8,
// PsyDoom: reintroducing the Arch-vile (resurrection states)
#if PSYDOOM_MODS
    S_TROO_RAISE1,
    S_TROO_RAISE2,
    S_TROO_RAISE3,
    S_TROO_RAISE4,
    S_TROO_RAISE5,
#endif
    S_SARG_STND,
    S_SARG_STND2,
    S_SARG_RUN1,
    S_SARG_RUN2,
    S_SARG_RUN3,
    S_SARG_RUN4,
    S_SARG_RUN5,
    S_SARG_RUN6,
    S_SARG_RUN7,
    S_SARG_RUN8,
    S_SARG_ATK1,
    S_SARG_ATK2,
    S_SARG_ATK3,
    S_SARG_PAIN,
    S_SARG_PAIN2,
    S_SARG_DIE1,
    S_SARG_DIE2,
    S_SARG_DIE3,
    S_SARG_DIE4,
    S_SARG_DIE5,
    S_SARG_DIE6,
// PsyDoom: reintroducing the Arch-vile (resurrection states)
#if PSYDOOM_MODS
    S_SARG_RAISE1,
    S_SARG_RAISE2,
    S_SARG_RAISE3,
    S_SARG_RAISE4,
    S_SARG_RAISE5,
    S_SARG_RAISE6,
#endif
    S_HEAD_STND,
    S_HEAD_RUN1,
    S_HEAD_ATK1,
    S_HEAD_ATK2,
    S_HEAD_ATK3,
    S_HEAD_PAIN,
    S_HEAD_PAIN2,
    S_HEAD_PAIN3,
    S_HEAD_DIE1,
    S_HEAD_DIE2,
    S_HEAD_DIE3,
    S_HEAD_DIE4,
    S_HEAD_DIE5,
    S_HEAD_DIE6,
// PsyDoom: reintroducing the Arch-vile (resurrection states)
#if PSYDOOM_MODS
    S_HEAD_RAISE1,
    S_HEAD_RAISE2,
    S_HEAD_RAISE3,
    S_HEAD_RAISE4,
    S_HEAD_RAISE5,
    S_HEAD_RAISE6,
#endif
    S_BOSS_STND,
    S_BOSS_STND2,
    S_BOSS_RUN1,
    S_BOSS_RUN2,
    S_BOSS_RUN3,
    S_BOSS_RUN4,
    S_BOSS_RUN5,
    S_BOSS_RUN6,
    S_BOSS_RUN7,
    S_BOSS_RUN8,
    S_BOSS_ATK1,
    S_BOSS_ATK2,
    S_BOSS_ATK3,
    S_BOSS_PAIN,
    S_BOSS_PAIN2,
    S_BOSS_DIE1,
    S_BOSS_DIE2,
    S_BOSS_DIE3,
    S_BOSS_DIE4,
    S_BOSS_DIE5,
    S_BOSS_DIE6,
    S_BOSS_DIE7,
// PsyDoom: reintroducing the Arch-vile (resurrection states)
#if PSYDOOM_MODS
    S_BOSS_RAISE1,
    S_BOSS_RAISE2,
    S_BOSS_RAISE3,
    S_BOSS_RAISE4,
    S_BOSS_RAISE5,
    S_BOSS_RAISE6,
    S_BOSS_RAISE7,
#endif
    S_BOS2_STND,
    S_BOS2_STND2,
    S_BOS2_RUN1,
    S_BOS2_RUN2,
    S_BOS2_RUN3,
    S_BOS2_RUN4,
    S_BOS2_RUN5,
    S_BOS2_RUN6,
    S_BOS2_RUN7,
    S_BOS2_RUN8,
    S_BOS2_ATK1,
    S_BOS2_ATK2,
    S_BOS2_ATK3,
    S_BOS2_PAIN,
    S_BOS2_PAIN2,
    S_BOS2_DIE1,
    S_BOS2_DIE2,
    S_BOS2_DIE3,
    S_BOS2_DIE4,
    S_BOS2_DIE5,
    S_BOS2_DIE6,
    S_BOS2_DIE7,
// PsyDoom: reintroducing the Arch-vile (resurrection states)
#if PSYDOOM_MODS
    S_BOS2_RAISE1,
    S_BOS2_RAISE2,
    S_BOS2_RAISE3,
    S_BOS2_RAISE4,
    S_BOS2_RAISE5,
    S_BOS2_RAISE6,
    S_BOS2_RAISE7,
#endif
    S_SKULL_STND,
    S_SKULL_STND2,
    S_SKULL_RUN1,
    S_SKULL_RUN2,
    S_SKULL_ATK1,
    S_SKULL_ATK2,
    S_SKULL_ATK3,
    S_SKULL_ATK4,
    S_SKULL_PAIN,
    S_SKULL_PAIN2,
    S_SKULL_DIE1,
    S_SKULL_DIE2,
    S_SKULL_DIE3,
    S_SKULL_DIE4,
    S_SKULL_DIE5,
    S_SKULL_DIE6,
    S_SPID_STND,
    S_SPID_STND2,
    S_SPID_RUN1,
    S_SPID_RUN2,
    S_SPID_RUN3,
    S_SPID_RUN4,
    S_SPID_RUN5,
    S_SPID_RUN6,
    S_SPID_RUN7,
    S_SPID_RUN8,
    S_SPID_RUN9,
    S_SPID_RUN10,
    S_SPID_RUN11,
    S_SPID_RUN12,
    S_SPID_ATK1,
    S_SPID_ATK2,
    S_SPID_ATK3,
    S_SPID_ATK4,
    S_SPID_PAIN,
    S_SPID_PAIN2,
    S_SPID_DIE1,
    S_SPID_DIE2,
    S_SPID_DIE3,
    S_SPID_DIE4,
    S_SPID_DIE5,
    S_SPID_DIE6,
    S_SPID_DIE7,
    S_SPID_DIE8,
    S_SPID_DIE9,
    S_SPID_DIE10,
    S_SPID_DIE11,
    S_BSPI_STND,
    S_BSPI_STND2,
    S_BSPI_SIGHT,
    S_BSPI_RUN1,
    S_BSPI_RUN2,
    S_BSPI_RUN3,
    S_BSPI_RUN4,
    S_BSPI_RUN5,
    S_BSPI_RUN6,
    S_BSPI_RUN7,
    S_BSPI_RUN8,
    S_BSPI_RUN9,
    S_BSPI_RUN10,
    S_BSPI_RUN11,
    S_BSPI_RUN12,
    S_BSPI_ATK1,
    S_BSPI_ATK2,
    S_BSPI_ATK3,
    S_BSPI_ATK4,
    S_BSPI_PAIN,
    S_BSPI_PAIN2,
    S_BSPI_DIE1,
    S_BSPI_DIE2,
    S_BSPI_DIE3,
    S_BSPI_DIE4,
    S_BSPI_DIE5,
    S_BSPI_DIE6,
    S_BSPI_DIE7,
// PsyDoom: reintroducing the Arch-vile (resurrection states)
#if PSYDOOM_MODS
    S_BSPI_RAISE1,
    S_BSPI_RAISE2,
    S_BSPI_RAISE3,
    S_BSPI_RAISE4,
    S_BSPI_RAISE5,
    S_BSPI_RAISE6,
    S_BSPI_RAISE7,
#endif
    S_ARACH_PLAZ,
    S_ARACH_PLAZ2,
    S_ARACH_PLEX,
    S_ARACH_PLEX2,
    S_ARACH_PLEX3,
    S_ARACH_PLEX4,
    S_ARACH_PLEX5,
    S_CYBER_STND,
    S_CYBER_STND2,
    S_CYBER_RUN1,
    S_CYBER_RUN2,
    S_CYBER_RUN3,
    S_CYBER_RUN4,
    S_CYBER_RUN5,
    S_CYBER_RUN6,
    S_CYBER_RUN7,
    S_CYBER_RUN8,
    S_CYBER_ATK1,
    S_CYBER_ATK2,
    S_CYBER_ATK3,
    S_CYBER_ATK4,
    S_CYBER_ATK5,
    S_CYBER_ATK6,
    S_CYBER_PAIN,
    S_CYBER_DIE1,
    S_CYBER_DIE2,
    S_CYBER_DIE3,
    S_CYBER_DIE4,
    S_CYBER_DIE5,
    S_CYBER_DIE6,
    S_CYBER_DIE7,
    S_CYBER_DIE8,
    S_CYBER_DIE9,
    S_CYBER_DIE10,
    S_PAIN_STND,
    S_PAIN_RUN1,
    S_PAIN_RUN2,
    S_PAIN_RUN3,
    S_PAIN_RUN4,
    S_PAIN_RUN5,
    S_PAIN_RUN6,
    S_PAIN_ATK1,
    S_PAIN_ATK2,
    S_PAIN_ATK3,
    S_PAIN_ATK4,
    S_PAIN_PAIN,
    S_PAIN_PAIN2,
    S_PAIN_DIE1,
    S_PAIN_DIE2,
    S_PAIN_DIE3,
    S_PAIN_DIE4,
    S_PAIN_DIE5,
    S_PAIN_DIE6,
    S_ARM1,
    S_ARM1A,
    S_ARM2,
    S_ARM2A,
    S_BAR1,
    S_BAR2,
    S_BEXP,
    S_BEXP2,
    S_BEXP3,
    S_BEXP4,
    S_BEXP5,
    S_BBAR1,
    S_BBAR2,
    S_BBAR3,
    S_BON1,
    S_BON1A,
    S_BON1B,
    S_BON1C,
    S_BON1D,
    S_BON1E,
    S_BON2,
    S_BON2A,
    S_BON2B,
    S_BON2C,
    S_BON2D,
    S_BON2E,
    S_BKEY,
    S_BKEY2,
    S_RKEY,
    S_RKEY2,
    S_YKEY,
    S_YKEY2,
    S_BSKULL,
    S_BSKULL2,
    S_RSKULL,
    S_RSKULL2,
    S_YSKULL,
    S_YSKULL2,
    S_STIM,
    S_MEDI,
    S_SOUL,
    S_SOUL2,
    S_SOUL3,
    S_SOUL4,
    S_SOUL5,
    S_SOUL6,
    S_PINV,
    S_PINV2,
    S_PINV3,
    S_PINV4,
    S_PSTR,
    S_PINS,
    S_PINS2,
    S_PINS3,
    S_PINS4,
    S_MEGA,
    S_MEGA2,
    S_MEGA3,
    S_MEGA4,
    S_SUIT,
    S_PMAP,
    S_PMAP2,
    S_PMAP3,
    S_PMAP4,
    S_PMAP5,
    S_PMAP6,
    S_PVIS,
    S_PVIS2,
    S_CLIP,
    S_AMMO,
    S_ROCK,
    S_BROK,
    S_CELL,
    S_CELP,
    S_SHEL,
    S_SBOX,
    S_BPAK,
    S_BFUG,
    S_MGUN,
    S_CSAW,
    S_LAUN,
    S_PLAS,
    S_SHOT,
    S_SHOT2,
    S_COLU,
    S_STALAG,
    S_DEADTORSO,
    S_DEADBOTTOM,
    S_HEADSONSTICK,
    S_GIBS,
    S_HEADONASTICK,
    S_DEADSTICK,
    S_MEAT2,
    S_MEAT3,
    S_MEAT4,
    S_MEAT5,
    S_STALAGTITE,
    S_TALLGRNCOL,
    S_SHRTGRNCOL,
    S_TALLREDCOL,
    S_SHRTREDCOL,
    S_SKULLCOL,
    S_CANDLESTIK,
    S_CANDELABRA,
    S_TORCHTREE,
    S_TECHPILLAR,
    S_FLOATSKULL,
    S_FLOATSKULL2,
    S_FLOATSKULL3,
    S_BTORCHSHRT,
    S_BTORCHSHRT2,
    S_BTORCHSHRT3,
    S_BTORCHSHRT4,
    S_GTORCHSHRT,
    S_GTORCHSHRT2,
    S_GTORCHSHRT3,
    S_GTORCHSHRT4,
    S_RTORCHSHRT,
    S_RTORCHSHRT2,
    S_RTORCHSHRT3,
    S_RTORCHSHRT4,
    S_HANGCHAIN,
    S_BLOODCHAIN,
    S_HANGLAMP,
    S_DEAD1,
    S_DEAD2,
    S_DEAD3,
    S_DEAD4,
    S_DEAD5,
    S_DEAD6,
    S_TECHLAMP,
    S_TECHLAMP2,
    S_TECHLAMP3,
    S_TECHLAMP4,
    S_TECH2LAMP,
    S_TECH2LAMP2,
    S_TECH2LAMP3,
    S_TECH2LAMP4,
    S_HEARTCOL,
    S_HEARTCOL2,
    S_EVILEYE,
    S_EVILEYE2,
    S_EVILEYE3,
    S_EVILEYE4,
    S_BLUETORCH,
    S_BLUETORCH2,
    S_BLUETORCH3,
    S_BLUETORCH4,
    S_GREENTORCH,
    S_GREENTORCH2,
    S_GREENTORCH3,
    S_GREENTORCH4,
    S_REDTORCH,
    S_REDTORCH2,
    S_REDTORCH3,
    S_REDTORCH4,
    S_BLOODYTWITCH,
    S_BLOODYTWITCH2,
    S_BLOODYTWITCH3,
    S_BLOODYTWITCH4,
    S_HEADCANDLES,
    S_HEADCANDLES2,
    S_LIVESTICK,
    S_LIVESTICK2,
    S_BIGTREE,
    S_HANGNOGUTS,
    S_HANGBNOBRAIN,
    S_HANGTLOOKDN,
    S_HANGTSKULL,
    S_HANGTLOOKUP,
    S_HANGTNOBRAIN,
    S_COLONGIBS,
    S_SMALLPOOL,
    S_BRAINSTEM,
// PsyDoom: adding support for missing PC Doom II actors
#if PSYDOOM_MODS
    S_VILE_STND,
    S_VILE_STND2,
    S_VILE_RUN1,
    S_VILE_RUN2,
    S_VILE_RUN3,
    S_VILE_RUN4,
    S_VILE_RUN5,
    S_VILE_RUN6,
    S_VILE_RUN7,
    S_VILE_RUN8,
    S_VILE_RUN9,
    S_VILE_RUN10,
    S_VILE_RUN11,
    S_VILE_RUN12,
    S_VILE_ATK1,
    S_VILE_ATK2,
    S_VILE_ATK3,
    S_VILE_ATK4,
    S_VILE_ATK5,
    S_VILE_ATK6,
    S_VILE_ATK7,
    S_VILE_ATK8,
    S_VILE_ATK9,
    S_VILE_ATK10,
    S_VILE_ATK11,
    S_VILE_HEAL1,
    S_VILE_HEAL2,
    S_VILE_HEAL3,
    S_VILE_PAIN,
    S_VILE_PAIN2,
    S_VILE_DIE1,
    S_VILE_DIE2,
    S_VILE_DIE3,
    S_VILE_DIE4,
    S_VILE_DIE5,
    S_VILE_DIE6,
    S_VILE_DIE7,
    S_VILE_DIE8,
    S_VILE_DIE9,
    S_VILE_DIE10,
    S_FIRE1,
    S_FIRE2,
    S_FIRE3,
    S_FIRE4,
    S_FIRE5,
    S_FIRE6,
    S_FIRE7,
    S_FIRE8,
    S_FIRE9,
    S_FIRE10,
    S_FIRE11,
    S_FIRE12,
    S_FIRE13,
    S_FIRE14,
    S_FIRE15,
    S_FIRE16,
    S_FIRE17,
    S_FIRE18,
    S_FIRE19,
    S_FIRE20,
    S_FIRE21,
    S_FIRE22,
    S_FIRE23,
    S_FIRE24,
    S_FIRE25,
    S_FIRE26,
    S_FIRE27,
    S_FIRE28,
    S_FIRE29,
    S_FIRE30,
    S_SSWV_STND,
    S_SSWV_STND2,
    S_SSWV_RUN1,
    S_SSWV_RUN2,
    S_SSWV_RUN3,
    S_SSWV_RUN4,
    S_SSWV_RUN5,
    S_SSWV_RUN6,
    S_SSWV_RUN7,
    S_SSWV_RUN8,
    S_SSWV_ATK1,
    S_SSWV_ATK2,
    S_SSWV_ATK3,
    S_SSWV_ATK4,
    S_SSWV_ATK5,
    S_SSWV_ATK6,
    S_SSWV_PAIN,
    S_SSWV_PAIN2,
    S_SSWV_DIE1,
    S_SSWV_DIE2,
    S_SSWV_DIE3,
    S_SSWV_DIE4,
    S_SSWV_DIE5,
    S_SSWV_XDIE1,
    S_SSWV_XDIE2,
    S_SSWV_XDIE3,
    S_SSWV_XDIE4,
    S_SSWV_XDIE5,
    S_SSWV_XDIE6,
    S_SSWV_XDIE7,
    S_SSWV_XDIE8,
    S_SSWV_XDIE9,
    S_SSWV_RAISE1,
    S_SSWV_RAISE2,
    S_SSWV_RAISE3,
    S_SSWV_RAISE4,
    S_SSWV_RAISE5,
    S_KEENSTND,
    S_COMMKEEN,
    S_COMMKEEN2,
    S_COMMKEEN3,
    S_COMMKEEN4,
    S_COMMKEEN5,
    S_COMMKEEN6,
    S_COMMKEEN7,
    S_COMMKEEN8,
    S_COMMKEEN9,
    S_COMMKEEN10,
    S_COMMKEEN11,
    S_COMMKEEN12,
    S_KEENPAIN,
    S_KEENPAIN2,
    S_BRAIN,
    S_BRAIN_PAIN,
    S_BRAIN_DIE1,
    S_BRAIN_DIE2,
    S_BRAIN_DIE3,
    S_BRAIN_DIE4,
    S_BRAINEYE,
    S_BRAINEYESEE,
    S_BRAINEYE1,
    S_SPAWN1,
    S_SPAWN2,
    S_SPAWN3,
    S_SPAWN4,
    S_SPAWNFIRE1,
    S_SPAWNFIRE2,
    S_SPAWNFIRE3,
    S_SPAWNFIRE4,
    S_SPAWNFIRE5,
    S_SPAWNFIRE6,
    S_SPAWNFIRE7,
    S_SPAWNFIRE8,
    S_BRAINEXPLODE1,
    S_BRAINEXPLODE2,
    S_BRAINEXPLODE3,
#endif
    BASE_NUM_STATES     // PsyDoom: renamed this from 'NUMSTATES' to 'BASE_NUM_STATES' because it's now just a count of the number of built-in states
};

// Indexes into the array of map object info structs
enum mobjtype_t : int32_t {
    MT_PLAYER,              // Player
    MT_POSSESSED,           // Former Human
    MT_SHOTGUY,             // Former Sergeant
    MT_UNDEAD,              // Revenant
    MT_TRACER,              // Revenant homing missile
    MT_SMOKE,               // Smoke from Revenant missile
    MT_FATSO,               // Mancubus
    MT_FATSHOT,             // Mancubus fireball
    MT_CHAINGUY,            // Chaingunner
    MT_TROOP,               // Imp
    MT_SERGEANT,            // Demon
    MT_HEAD,                // Cacodemon
    MT_BRUISER,             // Baron of Hell
    MT_KNIGHT,              // Hell Knight
    MT_SKULL,               // Lost Soul
    MT_SPIDER,              // Spider Mastermind
    MT_BABY,                // Arachnotron
    MT_CYBORG,              // Cyberdemon
    MT_PAIN,                // Pain Elemental
    MT_BARREL,              // Barrel
    MT_TROOPSHOT,           // Imp fireball
    MT_HEADSHOT,            // Cacodemon fireball
    MT_BRUISERSHOT,         // Baron/Knight fireball
    MT_ROCKET,              // Rocket (in flight)
    MT_PLASMA,              // Plasma fireball
    MT_BFG,                 // BFG main blast
    MT_ARACHPLAZ,           // Arachnotron plasma ball
    MT_PUFF,                // Smoke puff
    MT_BLOOD,               // Blood puff
    MT_TFOG,                // Teleport fog
    MT_IFOG,                // Item respawn fog
    MT_TELEPORTMAN,         // Teleport destination marker
    MT_EXTRABFG,            // Smaller BFG explosion on enemies
    MT_MISC0,               // Green armor
    MT_MISC1,               // Blue armor
    MT_MISC2,               // Health bonus
    MT_MISC3,               // Armor bonus
    MT_MISC4,               // Blue keycard
    MT_MISC5,               // Red keycard
    MT_MISC6,               // Yellow keycard
    MT_MISC7,               // Yellow skull key
    MT_MISC8,               // Red skull key
    MT_MISC9,               // Blue skull key
    MT_MISC10,              // Stimpack
    MT_MISC11,              // Medikit
    MT_MISC12,              // Soulsphere
    MT_INV,                 // Invulnerability
    MT_MISC13,              // Berserk
    MT_INS,                 // Invisibility
    MT_MISC14,              // Radiation Suit
    MT_MISC15,              // Computer Map
    MT_MISC16,              // Light Amplification Goggles
    MT_MEGA,                // Megasphere
    MT_CLIP,                // Ammo clip
    MT_MISC17,              // Box of Ammo
    MT_MISC18,              // Rocket
    MT_MISC19,              // Box of Rockets
    MT_MISC20,              // Cell Charge
    MT_MISC21,              // Cell Charge Pack
    MT_MISC22,              // Shotgun shells
    MT_MISC23,              // Box of Shells
    MT_MISC24,              // Backpack
    MT_MISC25,              // BFG9000
    MT_CHAINGUN,            // Chaingun
    MT_MISC26,              // Chainsaw
    MT_MISC27,              // Rocket Launcher
    MT_MISC28,              // Plasma Gun
    MT_SHOTGUN,             // Shotgun
    MT_SUPERSHOTGUN,        // Super Shotgun
    MT_MISC29,              // Tall techno floor lamp
    MT_MISC30,              // Short techno floor lamp
    MT_MISC31,              // Floor lamp
    MT_MISC32,              // Tall green pillar
    MT_MISC33,              // Short green pillar
    MT_MISC34,              // Tall red pillar
    MT_MISC35,              // Short red pillar
    MT_MISC36,              // Short red pillar (skull)
    MT_MISC37,              // Short green pillar (beating heart)
    MT_MISC38,              // Evil Eye
    MT_MISC39,              // Floating skull rock
    MT_MISC40,              // Gray tree
    MT_MISC41,              // Tall blue firestick
    MT_MISC42,              // Tall green firestick
    MT_MISC43,              // Tall red firestick
    MT_MISC44,              // Short blue firestick
    MT_MISC45,              // Short green firestick
    MT_MISC46,              // Short red firestick
    MT_MISC47,              // Stalagmite
    MT_MISC48,              // Tall techno pillar
    MT_MISC49,              // Candle
    MT_MISC50,              // Candelabra
    MT_MISC51,              // Hanging victim, twitching (blocking)
    MT_MISC52,              // Hanging victim, arms out (blocking)
    MT_MISC53,              // Hanging victim, 1-legged (blocking)
    MT_MISC54,              // Hanging pair of legs (blocking)
    MT_MISC56,              // Hanging victim, arms out
    MT_MISC57,              // Hanging pair of legs
    MT_MISC58,              // Hanging victim, 1-legged
    MT_MISC55,              // Hanging leg (blocking)
    MT_MISC59,              // Hanging leg
    MT_MISC60,              // Hook Chain
    MT_MISC_BLOODHOOK,      // Chain Hook With Blood
    MT_MISC_HANG_LAMP,      // UNUSED: points to hanging lamp sprite, but DoomEd number is Chaingunner... 
    MT_MISC61,              // Dead cacodemon
    MT_MISC63,              // Dead former human
    MT_MISC64,              // Dead demon
    MT_MISC66,              // Dead imp
    MT_MISC67,              // Dead former sergeant
    MT_MISC68,              // Bloody mess 1
    MT_MISC69,              // Bloody mess 2
    MT_MISC70,              // 5 skulls shish kebob
    MT_MISC73,              // Pile of skulls and candles
    MT_MISC71,              // Pool of blood and bones
    MT_MISC72,              // Skull on a pole
    MT_MISC74,              // Impaled human
    MT_MISC75,              // Twitching impaled human
    MT_MISC76,              // Large brown tree
    MT_MISC77,              // Burning barrel
    MT_MISC78,              // Hanging victim, guts removed
    MT_MISC79,              // Hanging victim, guts and brain removed
    MT_MISC80,              // Hanging torso, looking down
    MT_MISC81,              // Hanging torso, open skull
    MT_MISC82,              // Hanging torso, looking up
    MT_MISC83,              // Hanging torso, brain removed
    MT_MISC84,              // Pool of blood and guts
    MT_MISC85,              // Pool of blood
    MT_MISC86,              // Pool of brains

// PsyDoom: adding support for the unused hanging lamp sprite which is used in the GEC Master Edition.
// Also adding support for some actors from PC Doom II which were missing from the PlayStation version.
#if PSYDOOM_MODS
    MT_MISC87,              // Unused hanging lamp sprite
    MT_VILE,                // Arch-vile
    MT_FIRE,                // Arch-vile's flame
    MT_WOLFSS,              // Wolfenstein SS officer
    MT_KEEN,                // Commander Keen easter egg
    MT_BOSSBRAIN,           // Icon Of Sin head on a stick
    MT_BOSSSPIT,            // Icon Of Sin box shooter
    MT_BOSSTARGET,          // Icon Of Sin box target
    MT_SPAWNSHOT,           // Icon Of Sin box
    MT_SPAWNFIRE,           // Icon Of Sin spawn fire
#endif

// PsyDoom: new generic 'markers' which are intended to be used in scripts.
// They can be used for various purposes.
#if PSYDOOM_MODS
    MT_MARKER1,
    MT_MARKER2,
    MT_MARKER3,
    MT_MARKER4,
    MT_MARKER5,
    MT_MARKER6,
    MT_MARKER7,
    MT_MARKER8,
    MT_MARKER9,
    MT_MARKER10,
    MT_MARKER11,
    MT_MARKER12,
    MT_MARKER13,
    MT_MARKER14,
    MT_MARKER15,
    MT_MARKER16,
#endif

    BASE_NUM_MOBJ_TYPES     // PsyDoom: renamed this from 'NUMMOBJTYPES' to 'BASE_NUM_MOBJ_TYPES' because it's now just a count of the number of built-in map object types
};

// PsyDoom helper: represents a state action function.
// Can accept one of two different function formats, one for things and another for player sprite (player weapon) states.
struct statefn_t {
    union {
        statefn_mobj_t  mobjFn;
        statefn_pspr_t  psprFn;
    };

    inline statefn_t() noexcept : mobjFn(nullptr) {}
    inline statefn_t(const std::nullptr_t) noexcept : mobjFn(nullptr) {}
    inline statefn_t(const statefn_mobj_t mobjFn) noexcept : mobjFn(mobjFn) {}
    inline statefn_t(const statefn_pspr_t psprFn) noexcept : psprFn(psprFn) {}

    // Tell if the function is defined or not
    inline operator bool () const noexcept { return (mobjFn != nullptr); }

    // Invoke the function using one of the allowed function formats
    inline void operator()(mobj_t& mobj) const noexcept { mobjFn(mobj); }
    inline void operator()(player_t& player, pspdef_t& sprite) const noexcept { psprFn(player, sprite); }
};

// Sprite frame number flags and masks.
// The flags are encoded in the frame number itself:
static constexpr uint32_t FF_FULLBRIGHT = 0x8000;       // If set the sprite is always displayed fullbright, in spite of lighting conditions.
static constexpr uint32_t FF_FRAMEMASK  = 0x7FFF;       // Mask to retrieve the actual frame number itself.

// Defines a finite state machine state that a map object or player sprite (weapon) can be in
struct state_t {
    spritenum_t     sprite;         // Sprite number to use for the state
    int32_t         frame;          // What frame of the state to display
    int32_t         tics;           // Number of tics to remain in this state, or -1 if infinite
    statefn_t       action;         // Action function to call upon entering the state, may have 1 or 2 parameters depending on context (map object vs player sprite).
    statenum_t      nextstate;      // State number to goto after this state
    int32_t         misc1;          // State specific info 1: appears unused in this version of the game
    int32_t         misc2;          // State specific info 2: appears unused in this version of the game
};

// Defines properties and behavior for a map object type
struct mobjinfo_t {
    int32_t         doomednum;
    statenum_t      spawnstate;
    int32_t         spawnhealth;
    statenum_t      seestate;
    sfxenum_t       seesound;
    int32_t         reactiontime;
    sfxenum_t       attacksound;
    statenum_t      painstate;
    int32_t         painchance;
    sfxenum_t       painsound;
    statenum_t      meleestate;
    statenum_t      missilestate;
    statenum_t      deathstate;
    statenum_t      xdeathstate;
    sfxenum_t       deathsound;
    int32_t         speed;              // Object movement speed. Note: sometimes a fixed point number, sometimes an normal integer.
    fixed_t         radius;
    fixed_t         height;
    int32_t         mass;
    int32_t         damage;
    sfxenum_t       activesound;
    uint32_t        flags;
// PsyDoom: adding back in the 'raise' state used by the Arch-vile when resurrecting enemies
#if PSYDOOM_MODS
    statenum_t      raisestate;
#endif
};

// PsyDoom: sprite names are now represented as a single 32-bit integer for easier and faster comparison
#if PSYDOOM_MODS
    typedef String4 sprname_t;
#else
    typedef const char* sprname_t;
#endif

// The arrays of sprite names, state definitions and map object definitions.
// PsyDoom: renamed, made const and added the 'base' prefix here to the arrays to signify that these are just the built-in lists; also adding non hardcoded counts.
extern const sprname_t      gBaseSprNames[BASE_NUM_SPRITES];
extern const state_t        gBaseStates[BASE_NUM_STATES];
extern const mobjinfo_t     gBaseMobjInfo[BASE_NUM_MOBJ_TYPES];

#if PSYDOOM_MODS
    extern state_t*     gStates;
    extern int32_t      gNumStates;
    extern mobjinfo_t*  gMobjInfo;
    extern int32_t      gNumMobjInfo;
#else
    #define gSprNames   gBaseSprNames
    #define gStates     gBaseStates
    #define gMobjInfo   gBaseMobjInfo
#endif

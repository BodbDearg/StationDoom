#include "s_sound.h"

#include "Doom/cdmaptbl.h"
#include "Doom/Game/doomdata.h"
#include "Doom/Game/g_game.h"
#include "Doom/Renderer/r_local.h"
#include "Doom/Renderer/r_main.h"
#include "EngineLimits.h"
#include "FatalErrors.h"
#include "i_main.h"
#include "m_fixed.h"
#include "PcPsx/Config.h"
#include "PcPsx/Game.h"
#include "PcPsx/ProgArgs.h"
#include "PcPsx/Utils.h"
#include "sounds.h"
#include "Wess/lcdload.h"
#include "Wess/psxspu.h"
#include "Wess/seqload.h"
#include "Wess/seqload_r.h"
#include "Wess/wessapi.h"
#include "Wess/wessapi_m.h"
#include "Wess/wessapi_p.h"
#include "Wess/wessapi_t.h"
#include "Wess/wessarc.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

// Sound settings for the WESS PSX sound driver
static const int32_t gSound_PSXSettings[SNDHW_TAG_MAX * 2] = {
    SNDHW_TAG_DRIVER_ID,        PSX_ID,
    SNDHW_TAG_SOUND_EFFECTS,    1,
    SNDHW_TAG_MUSIC,            1,
    SNDHW_TAG_DRUMS,            1,
    SNDHW_TAG_END,              0
};

// What sound settings to use to when loading the .WMD file
static const int32_t* gSound_SettingsLists[2] = {
    gSound_PSXSettings,
    nullptr
};

// What CD audio track each piece of CD music uses
const uint32_t gCDTrackNum[NUM_CD_MUSIC_TRACKS] = {
    2,      // cdmusic_title_screen
    3,      // cdmusic_main_menu
    4,      // cdmusic_credits_demo
    5,      // cdmusic_intermission
    6,      // cdmusic_club_doom
    7,      // cdmusic_finale_doom1_final_doom
    8,      // cdmusic_finale_doom2
};

// Defines the settings for one music sequencer track
struct musicseq_t {
    CdFileId        lcdFile;            // LCD file containing the samples used by the music track
    int32_t         seqIdx;             // What sequence to trigger for the track (from the module file)
    SpuReverbMode   reverbMode;         // What type of reverb to use
    int16_t         reverbDepthL;       // Reverb depth: left
    int16_t         reverbDepthR;       // Reverb depth: right
};

#if PSYDOOM_MODS
    // PsyDoom: a sound queued and waiting to be played when the tick ends.
    // PsyDoom queues sounds and only plays them at the end of a tick in order to avoid duplicate sounds.
    struct queued_sound_t {
        mobj_t*     pOrigin;    // The origin of the sound (nullptr if not triggered by a source in the world)
        sfxenum_t   soundId;    // Which sound to play
        uint8_t     volume;     // Volume level (0-127)
        uint8_t     pan;        // Pan amount (WESS_PAN_CENTER is the center)
        uint8_t     reverb;     // How much reverb to apply (0-127)
    };

    // A list of sounds queued to be played on the next call to 'S_UpdateSounds'
    static std::vector<queued_sound_t> gQueuedSounds;
    
    // Type for an iterator to a queued sound
    typedef decltype(gQueuedSounds.begin()) queued_sound_iter;
#endif

// Definitions for all of the available music sequences in the game: Doom and Final Doom.
// Note that not all of these are unique songs, some of the tracks are simply with different reverb settings.
static const musicseq_t gMusicSeqDefs_Doom[31] = {
    { CdFileId{},               0,      SPU_REV_MODE_OFF,       0x0000, 0x0000 },
    { CdFileId::MUSLEV1_LCD,    90,     SPU_REV_MODE_SPACE,     0x0FFF, 0x0FFF },
    { CdFileId::MUSLEV2_LCD,    91,     SPU_REV_MODE_SPACE,     0x0FFF, 0x0FFF },
    { CdFileId::MUSLEV3_LCD,    92,     SPU_REV_MODE_STUDIO_B,  0x27FF, 0x27FF },
    { CdFileId::MUSLEV4_LCD,    93,     SPU_REV_MODE_HALL,      0x17FF, 0x17FF },
    { CdFileId::MUSLEV5_LCD,    94,     SPU_REV_MODE_STUDIO_A,  0x23FF, 0x23FF },
    { CdFileId::MUSLEV6_LCD,    95,     SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF },
    { CdFileId::MUSLEV7_LCD,    96,     SPU_REV_MODE_STUDIO_C,  0x26FF, 0x26FF },
    { CdFileId::MUSLEV8_LCD,    97,     SPU_REV_MODE_STUDIO_B,  0x2DFF, 0x2DFF },
    { CdFileId::MUSLEV9_LCD,    98,     SPU_REV_MODE_STUDIO_C,  0x2FFF, 0x2FFF },
    { CdFileId::MUSLEV10_LCD,   99,     SPU_REV_MODE_SPACE,     0x0FFF, 0x0FFF },
    { CdFileId::MUSLEV11_LCD,   100,    SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF },
    { CdFileId::MUSLEV12_LCD,   101,    SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF },
    { CdFileId::MUSLEV13_LCD,   102,    SPU_REV_MODE_SPACE,     0x0FFF, 0x0FFF },
    { CdFileId::MUSLEV14_LCD,   103,    SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF },
    { CdFileId::MUSLEV15_LCD,   104,    SPU_REV_MODE_STUDIO_B,  0x27FF, 0x27FF },
    { CdFileId::MUSLEV16_LCD,   105,    SPU_REV_MODE_SPACE,     0x0FFF, 0x0FFF },
    { CdFileId::MUSLEV17_LCD,   106,    SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF },
    { CdFileId::MUSLEV18_LCD,   107,    SPU_REV_MODE_SPACE,     0x0FFF, 0x0FFF },
    { CdFileId::MUSLEV19_LCD,   108,    SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF },
    { CdFileId::MUSLEV20_LCD,   109,    SPU_REV_MODE_STUDIO_C,  0x2FFF, 0x2FFF },
    { CdFileId::MUSLEV19_LCD,   108,    SPU_REV_MODE_STUDIO_C,  0x2FFF, 0x2FFF },
    { CdFileId::MUSLEV2_LCD,    91,     SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF },
    { CdFileId::MUSLEV1_LCD,    90,     SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF },
    { CdFileId::MUSLEV13_LCD,   102,    SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF },
    { CdFileId::MUSLEV7_LCD,    96,     SPU_REV_MODE_STUDIO_B,  0x27FF, 0x27FF },
    { CdFileId::MUSLEV16_LCD,   105,    SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF },
    { CdFileId::MUSLEV5_LCD,    94,     SPU_REV_MODE_STUDIO_C,  0x2FFF, 0x2FFF },
    { CdFileId::MUSLEV1_LCD,    90,     SPU_REV_MODE_STUDIO_C,  0x2FFF, 0x2FFF },
    { CdFileId::MUSLEV17_LCD,   106,    SPU_REV_MODE_STUDIO_C,  0x2FFF, 0x2FFF },
    { CdFileId{},               0,      SPU_REV_MODE_OFF,       0x0000, 0x0000 }
};

static const musicseq_t gMusicSeqDefs_FinalDoom[32] = {
    { CdFileId{},               0,      SPU_REV_MODE_OFF,       0x0000, 0x0000  },
    { CdFileId::MUSLEV23_LCD,   112,    SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF  },
    { CdFileId::MUSLEV29_LCD,   118,    SPU_REV_MODE_STUDIO_C,  0x26FF, 0x26FF  },
    { CdFileId::MUSLEV24_LCD,   113,    SPU_REV_MODE_SPACE,     0x1FFF, 0x1FFF  },
    { CdFileId::MUSLEV30_LCD,   119,    SPU_REV_MODE_SPACE,     0x0FFF, 0x0FFF  },
    { CdFileId::MUSLEV21_LCD,   110,    SPU_REV_MODE_SPACE,     0x1FFF, 0x1FFF  },
    { CdFileId::MUSLEV27_LCD,   116,    SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF  },
    { CdFileId::MUSLEV25_LCD,   114,    SPU_REV_MODE_SPACE,     0x0FFF, 0x0FFF  },
    { CdFileId::MUSLEV28_LCD,   117,    SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF  },
    { CdFileId::MUSLEV22_LCD,   111,    SPU_REV_MODE_SPACE,     0x1FFF, 0x1FFF  },
    { CdFileId::MUSLEV26_LCD,   115,    SPU_REV_MODE_SPACE,     0x0FFF, 0x0FFF  },
    { CdFileId::MUSLEV1_LCD,    90,     SPU_REV_MODE_SPACE,     0x0FFF, 0x0FFF  },
    { CdFileId::MUSLEV2_LCD,    91,     SPU_REV_MODE_SPACE,     0x0FFF, 0x0FFF  },
    { CdFileId::MUSLEV3_LCD,    92,     SPU_REV_MODE_STUDIO_B,  0x27FF, 0x27FF  },
    { CdFileId::MUSLEV4_LCD,    93,     SPU_REV_MODE_HALL,      0x17FF, 0x17FF  },
    { CdFileId::MUSLEV5_LCD,    94,     SPU_REV_MODE_STUDIO_A,  0x23FF, 0x23FF  },
    { CdFileId::MUSLEV6_LCD,    95,     SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF  },
    { CdFileId::MUSLEV7_LCD,    96,     SPU_REV_MODE_STUDIO_C,  0x26FF, 0x26FF  },
    { CdFileId::MUSLEV8_LCD,    97,     SPU_REV_MODE_STUDIO_B,  0x2DFF, 0x2DFF  },
    { CdFileId::MUSLEV9_LCD,    98,     SPU_REV_MODE_STUDIO_C,  0x2FFF, 0x2FFF  },
    { CdFileId::MUSLEV10_LCD,   99,     SPU_REV_MODE_STUDIO_B,  0x27FF, 0x27FF  },
    { CdFileId::MUSLEV11_LCD,   100,    SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF  },
    { CdFileId::MUSLEV12_LCD,   101,    SPU_REV_MODE_STUDIO_C,  0x2FFF, 0x2FFF  },
    { CdFileId::MUSLEV13_LCD,   102,    SPU_REV_MODE_SPACE,     0x0FFF, 0x0FFF  },
    { CdFileId::MUSLEV14_LCD,   103,    SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF  },
    { CdFileId::MUSLEV15_LCD,   104,    SPU_REV_MODE_STUDIO_B,  0x27FF, 0x27FF  },
    { CdFileId::MUSLEV16_LCD,   105,    SPU_REV_MODE_SPACE,     0x0FFF, 0x0FFF  },
    { CdFileId::MUSLEV17_LCD,   106,    SPU_REV_MODE_HALL,      0x1FFF, 0x1FFF  },
    { CdFileId::MUSLEV18_LCD,   107,    SPU_REV_MODE_SPACE,     0x0FFF, 0x0FFF  },
    { CdFileId::MUSLEV22_LCD,   111,    SPU_REV_MODE_STUDIO_C,  0x2FFF, 0x2FFF  },
    { CdFileId::MUSLEV26_LCD,   115,    SPU_REV_MODE_STUDIO_C,  0x2FFF, 0x2FFF  },
    { CdFileId{},               0,      SPU_REV_MODE_OFF,       0x0000, 0x0000  },
};

static constexpr uint32_t NUM_MUSIC_SEQS_DOOM       = 20;   // How many music sequence tracks there are in the game: Doom
static constexpr uint32_t NUM_MUSIC_SEQS_FINAL_DOOM = 30;   // How many music sequence tracks there are in the game: Final Doom

// Defines what LCD file to load for a map and what music sequence definition to use
struct mapaudiodef_t {
    CdFileId    sfxLcdFile;     // LCD file SFX for monsters on the map
    uint32_t    musicSeqIdx;    // Index of the music sequence to use
};

// What sound LCD file and music track to use for all maps in the game: Doom
static const mapaudiodef_t gMapAudioDefs_Doom[62] = {
    { CdFileId{},           0   },
    { CdFileId::MAP01_LCD,  1   },
    { CdFileId::MAP02_LCD,  2   },
    { CdFileId::MAP03_LCD,  3   },
    { CdFileId::MAP04_LCD,  4   },
    { CdFileId::MAP05_LCD,  5   },
    { CdFileId::MAP06_LCD,  6   },
    { CdFileId::MAP07_LCD,  7   },
    { CdFileId::MAP08_LCD,  8   },
    { CdFileId::MAP09_LCD,  11  },
    { CdFileId::MAP10_LCD,  9   },
    { CdFileId::MAP11_LCD,  15  },
    { CdFileId::MAP12_LCD,  10  },
    { CdFileId::MAP13_LCD,  21  },
    { CdFileId::MAP14_LCD,  22  },
    { CdFileId::MAP15_LCD,  23  },
    { CdFileId::MAP16_LCD,  12  },
    { CdFileId::MAP17_LCD,  16  },
    { CdFileId::MAP18_LCD,  17  },
    { CdFileId::MAP19_LCD,  6   },
    { CdFileId::MAP20_LCD,  18  },
    { CdFileId::MAP21_LCD,  24  },
    { CdFileId::MAP22_LCD,  14  },
    { CdFileId::MAP23_LCD,  3   },
    { CdFileId::MAP24_LCD,  20  },
    { CdFileId::MAP25_LCD,  11  },
    { CdFileId::MAP26_LCD,  25  },
    { CdFileId::MAP27_LCD,  4   },
    { CdFileId::MAP28_LCD,  5   },
    { CdFileId::MAP29_LCD,  10  },
    { CdFileId::MAP30_LCD,  19  },
    { CdFileId::MAP31_LCD,  1   },
    { CdFileId::MAP32_LCD,  9   },
    { CdFileId::MAP33_LCD,  14  },
    { CdFileId::MAP34_LCD,  12  },
    { CdFileId::MAP35_LCD,  8   },
    { CdFileId::MAP36_LCD,  13  },
    { CdFileId::MAP37_LCD,  18  },
    { CdFileId::MAP38_LCD,  20  },
    { CdFileId::MAP39_LCD,  15  },
    { CdFileId::MAP40_LCD,  19  },
    { CdFileId::MAP41_LCD,  11  },
    { CdFileId::MAP42_LCD,  26  },
    { CdFileId::MAP43_LCD,  12  },
    { CdFileId::MAP44_LCD,  29  },
    { CdFileId::MAP45_LCD,  6   },
    { CdFileId::MAP46_LCD,  27  },
    { CdFileId::MAP47_LCD,  9   },
    { CdFileId::MAP48_LCD,  22  },
    { CdFileId::MAP49_LCD,  3   },
    { CdFileId::MAP50_LCD,  28  },
    { CdFileId::MAP51_LCD,  7   },
    { CdFileId::MAP52_LCD,  8   },
    { CdFileId::MAP53_LCD,  15  },
    { CdFileId::MAP54_LCD,  4   },
    { CdFileId::MAP55_LCD,  17  },
    { CdFileId::MAP56_LCD,  18  },
    { CdFileId::MAP57_LCD,  10  },
    { CdFileId::MAP58_LCD,  16  },
    { CdFileId::MAP59_LCD,  13  },
    { CdFileId::MAP60_LCD,  0   },
    { CdFileId{},           0   }
};

// What sound LCD file and music track to use for all maps in the game: Final Doom
static mapaudiodef_t gMapAudioDefs_FinalDoom[33] = {
    { CdFileId{},           0   },
    { CdFileId::MAP01_LCD,  1   },
    { CdFileId::MAP02_LCD,  2   },
    { CdFileId::MAP03_LCD,  3   },
    { CdFileId::MAP04_LCD,  4   },
    { CdFileId::MAP05_LCD,  5   },
    { CdFileId::MAP06_LCD,  6   },
    { CdFileId::MAP07_LCD,  7   },
    { CdFileId::MAP08_LCD,  8   },
    { CdFileId::MAP09_LCD,  9   },
    { CdFileId::MAP10_LCD,  10  },
    { CdFileId::MAP11_LCD,  11  },
    { CdFileId::MAP12_LCD,  12  },
    { CdFileId::MAP13_LCD,  13  },
    { CdFileId::MAP14_LCD,  14  },
    { CdFileId::MAP15_LCD,  15  },
    { CdFileId::MAP16_LCD,  16  },
    { CdFileId::MAP17_LCD,  17  },
    { CdFileId::MAP18_LCD,  18  },
    { CdFileId::MAP19_LCD,  19  },
    { CdFileId::MAP20_LCD,  20  },
    { CdFileId::MAP21_LCD,  21  },
    { CdFileId::MAP22_LCD,  22  },
    { CdFileId::MAP23_LCD,  23  },
    { CdFileId::MAP24_LCD,  24  },
    { CdFileId::MAP25_LCD,  25  },
    { CdFileId::MAP26_LCD,  26  },
    { CdFileId::MAP27_LCD,  27  },
    { CdFileId::MAP28_LCD,  28  },
    { CdFileId::MAP29_LCD,  29  },
    { CdFileId::MAP30_LCD,  30  },
    { CdFileId::MAP60_LCD,  0   },
    { CdFileId{},           0   }
};

static constexpr fixed_t S_CLIPPING_DIST    = 1124 * FRACUNIT;      // Distance at which (or after) sounds stop playing
static constexpr fixed_t S_CLOSE_DIST       = 100 * FRACUNIT;       // When sounds get this close, play them at max volume
static constexpr fixed_t S_STEREO_SWING     = 96 * FRACUNIT;        // Stereo separation amount

// Divider for figuring out how fast sound fades
static constexpr int32_t S_ATTENUATOR = d_fixed_to_int(S_CLIPPING_DIST - S_CLOSE_DIST);

// Current volume cd music is played back at
int32_t gCdMusicVol = PSXSPU_MAX_CD_VOL;

// The buffer used to hold the master status structure created by loading the .WMD file. Also holds sequence data for current level music.
// PsyDoom limit removing: we now dynamically make this bigger if needed, based on the size of the .WMD file.
#if PSYDOOM_LIMIT_REMOVING
    static std::unique_ptr<uint8_t[]>   gSound_WmdMem;
    static uint32_t                     gSound_WmdMemSize;
#else
    static uint8_t                      gSound_WmdMem[WMD_MEM_SIZE];
#endif

// The start pointer within 'gSound_WmdMem' where map music sequences can be loaded to.
// Anything at this address or after in the buffer can be used for that purpose.
static uint8_t* gpSound_MusicSeqData;

// Which of the music sequence definitions is currently playing
static uint32_t gCurMusicSeqIdx;

// What map we have sound and music currently loaded for
static int32_t gLoadedSoundAndMusMapNum;

// The start address in SPU RAM where map sound effects (i.e enemies) and music instrument samples can be loaded to.
// Addresses before this are for SPU reserved RAM (roughly 4 KiB) and for the common/base sfx LCD file, 'DOOMSFX.LCD'.
// That LCD contains UI and player sounds and is almost always kept in SPU memory and not unloaded, except during the finale.
static uint32_t gSound_MapLcdSpuStartAddr = SPU_RAM_APP_BASE;

// Sample blocks for general DOOM sounds and map specific music and sfx sounds
static SampleBlock gDoomSndBlock;
static SampleBlock gMapSndBlock;

// Is the DOOMSFX.LCD file loaded? (main sound effect samples)
static bool gbDidLoadDoomSfxLcd;

// Used to save the state of voices when pausing
static SavedVoiceList gPausedMusVoiceState;

// Unused tick count for how many sound 'updates' ticks were done.
// PsyDoom: don't need this - remove.
#if !PSYDOOM_MODS
    static uint32_t gNumSoundTics;
#endif

//------------------------------------------------------------------------------------------------------------------------------------------
// PsyDoom Helper: converts from a DOOM volume level (0-100) to a WESS API volume level (0-127)
//------------------------------------------------------------------------------------------------------------------------------------------
int32_t doomToWessVol(const int32_t doomVol) noexcept {
    return (doomVol * WESS_MAX_MASTER_VOL) / S_MAX_VOL;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// PsyDoom Helper: converts from a DOOM volume level (0-100) to a WESS PSXSPU API volume level (0-127)
//------------------------------------------------------------------------------------------------------------------------------------------
int32_t doomToPsxSpuVol(const int32_t doomVol) noexcept {
    return (doomVol * WESS_MAX_MASTER_VOL) / S_MAX_VOL;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Sets the master sound effects volume
//------------------------------------------------------------------------------------------------------------------------------------------
void S_SetSfxVolume(int32_t sfxVol) noexcept {
    wess_master_sfx_vol_set((uint8_t) sfxVol);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Sets the master music volume
//------------------------------------------------------------------------------------------------------------------------------------------
void S_SetMusicVolume(const int32_t musVol) noexcept {
    // Set the volume for both the sound sequencer and for cd audio
    wess_master_mus_vol_set((uint8_t) musVol);
    const int32_t cdVol = std::min(musVol * 128, (int32_t) PSXSPU_MAX_CD_VOL);
    psxspu_set_cd_vol(cdVol);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Stop the currently playing music track
//------------------------------------------------------------------------------------------------------------------------------------------
void S_StopMusic() noexcept {
    if (gCurMusicSeqIdx != 0) {
        const musicseq_t& musicSeqDef = (Game::isFinalDoom()) ? gMusicSeqDefs_FinalDoom[gCurMusicSeqIdx] : gMusicSeqDefs_Doom[gCurMusicSeqIdx];
        wess_seq_stop(musicSeqDef.seqIdx);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Start playing the selected music track (stops if currently playing)
//------------------------------------------------------------------------------------------------------------------------------------------
void S_StartMusic() noexcept {
    // PsyDoom: ignore this command in headless mode
    #if PSYDOOM_MODS
        if (ProgArgs::gbHeadlessMode)
            return;
    #endif

    S_StopMusic();

    if (gCurMusicSeqIdx != 0) {
        const musicseq_t& musicSeqDef = (Game::isFinalDoom()) ? gMusicSeqDefs_FinalDoom[gCurMusicSeqIdx] : gMusicSeqDefs_Doom[gCurMusicSeqIdx];
        wess_seq_trigger(musicSeqDef.seqIdx);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Initializes the given sample block
//------------------------------------------------------------------------------------------------------------------------------------------
void S_InitSampleBlock(SampleBlock& block) noexcept {
    block.num_samples = 0;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Unload all loaded samples in the given sample block
//------------------------------------------------------------------------------------------------------------------------------------------
void S_UnloadSampleBlock(SampleBlock& sampleBlock) noexcept {
    // Zero the patch address in SPU RAM for every single loaded sample:
    while (sampleBlock.num_samples > 0) {
        sampleBlock.num_samples--;
        const uint32_t patchSampleIdx = sampleBlock.patch_sample_idx[sampleBlock.num_samples];
        wess_dig_set_sample_position(patchSampleIdx, 0);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Loads all sound and music for the given map number (Note: '0' if menu, '60' if finale)
//------------------------------------------------------------------------------------------------------------------------------------------
void S_LoadMapSoundAndMusic(const int32_t mapIdx) noexcept {
    // Final Doom has different music and map sounds
    const musicseq_t* const pMusicSeqDefs = (Game::isFinalDoom()) ? gMusicSeqDefs_FinalDoom : gMusicSeqDefs_Doom;

    // PsyDoom: ignore this command in headless mode
    #if PSYDOOM_MODS
        if (ProgArgs::gbHeadlessMode)
            return;
    #endif

    // If sound and music is already loaded then bail out
    if (gLoadedSoundAndMusMapNum == mapIdx)
        return;

    // Stop current map music, free all music sequences and unload map SFX
    if (gLoadedSoundAndMusMapNum != 0) {
        if (gCurMusicSeqIdx != 0) {
            S_StopMusic();

            // PsyDoom: waiting for music to end now be optionally skipped if fast loading is enabled.
            #if PSYDOOM_MODS
                const bool bWaitForMusicToEnd = (!Config::gbUseFastLoading);
            #else
                const bool bWaitForMusicToEnd = true;
            #endif

            if (bWaitForMusicToEnd) {
                Utils::waitUntilSeqEnteredStatus(pMusicSeqDefs[gCurMusicSeqIdx].seqIdx, SequenceStatus::SEQUENCE_INACTIVE);
            }

            wess_seq_range_free(0 + NUMSFX, (Game::isFinalDoom()) ? NUM_MUSIC_SEQS_FINAL_DOOM : NUM_MUSIC_SEQS_DOOM);
        }

        S_UnloadSampleBlock(gMapSndBlock);
    }

    // Either load or unload the main Doom SFX LCD
    const int32_t finaleMapNum = Game::getNumMaps() + 1;

    if (mapIdx == finaleMapNum) {
        // For the finale unload this LCD to free up SPU RAM, since a lot will be needed to hold all monster sounds
        S_UnloadSampleBlock(gDoomSndBlock);
        gbDidLoadDoomSfxLcd = false;
        gSound_MapLcdSpuStartAddr = SPU_RAM_APP_BASE;
    } else {
        // In all other cases ensure it is loaded
        if (!gbDidLoadDoomSfxLcd) {
            gSound_MapLcdSpuStartAddr = SPU_RAM_APP_BASE + wess_dig_lcd_load(CdFileId::DOOMSFX_LCD, SPU_RAM_APP_BASE, &gDoomSndBlock, false);
            gbDidLoadDoomSfxLcd = true;
        }
    }

    // Load the music sequence and lcd file for the map music.
    // Also initialize the reverb mode depending on the music.
    const mapaudiodef_t& mapAudioDef = (Game::isFinalDoom()) ? gMapAudioDefs_FinalDoom[mapIdx] : gMapAudioDefs_Doom[mapIdx];
    gCurMusicSeqIdx = mapAudioDef.musicSeqIdx;
    uint32_t destSpuAddr = gSound_MapLcdSpuStartAddr;

    if (gCurMusicSeqIdx == 0) {
        // No music sequences for this map - turn off reverb
        psxspu_init_reverb(SPU_REV_MODE_OFF, 0, 0, 0, 0);
    } else {
        // Normal case: playing a map music sequence and initializing reverb.
        // Note: incorporating a change Final Doom made for all versions here, mute all audio first to prevent artifacts when initializing reverb:
        const int32_t masterVol = psxspu_get_master_vol();
        psxspu_set_master_vol(0);

        const musicseq_t& musicseq = pMusicSeqDefs[gCurMusicSeqIdx];
        psxspu_init_reverb(musicseq.reverbMode, musicseq.reverbDepthL, musicseq.reverbDepthR, 0, 0);
        wess_seq_load(musicseq.seqIdx, gpSound_MusicSeqData);
        destSpuAddr += wess_dig_lcd_load(musicseq.lcdFile, destSpuAddr, &gMapSndBlock, false);

        // Restore the master volume to what it was.
        // Again, this mute/restore was only added in Final Doom, but I'm going to do it for all Doom versions:
        psxspu_set_master_vol(masterVol);
    }

    // Remember what map we have loaded sound and music for
    gLoadedSoundAndMusMapNum = mapIdx;

    // Load the sound LCD file for the map
    if (mapAudioDef.sfxLcdFile != CdFileId{}) {
        wess_dig_lcd_load(mapAudioDef.sfxLcdFile, destSpuAddr, &gMapSndBlock, false);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Stops all playing sounds and sequencer music: music state is also saved for later restoring
//------------------------------------------------------------------------------------------------------------------------------------------
void S_Pause() noexcept {
    wess_seq_pauseall(true, &gPausedMusVoiceState);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Resume playing previously paused music
//------------------------------------------------------------------------------------------------------------------------------------------
void S_Resume() noexcept {
    wess_seq_restartall(&gPausedMusVoiceState);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Stop playing the specified sound
//------------------------------------------------------------------------------------------------------------------------------------------
void S_StopSound(const mobj_t* pOrigin) noexcept {
    wess_seq_stoptype((uintptr_t) pOrigin);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Stops all playing sounds and sequencer music
//------------------------------------------------------------------------------------------------------------------------------------------
void S_StopAll() noexcept {
    wess_seq_stopall();
}

#if PSYDOOM_MODS
//------------------------------------------------------------------------------------------------------------------------------------------
// Platform implementation for starting a sound.
// PsyDoom: This is a complete rewrite of the original 'I_StartSound' and only starts a previously queued sound.
// For the original implementation, see the 'old' code folder.
//------------------------------------------------------------------------------------------------------------------------------------------
static void I_StartSound(const queued_sound_t& sound) noexcept {
    TriggerPlayAttr sndAttribs = {};

    sndAttribs.attribs_mask = TRIGGER_VOLUME | TRIGGER_PAN | TRIGGER_REVERB;
    sndAttribs.volume_cntrl = sound.volume;
    sndAttribs.pan_cntrl = sound.pan;
    sndAttribs.reverb = sound.reverb;

    wess_seq_trigger_type_special(sound.soundId, (uintptr_t) sound.pOrigin, &sndAttribs);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// New for PsyDoom: queue a sound to play later.
// This is an adaption of the orginal 'I_StartSound' (found in the 'old' code folder) but queues the sound instead of playing.
// Queuing allows us to de-duplicate the same sound playing on the same frame multiple times.
//------------------------------------------------------------------------------------------------------------------------------------------
static void I_QueueSound(mobj_t* const pOrigin, const sfxenum_t soundId) noexcept {
    // Ignore this command in headless mode
    if (ProgArgs::gbHeadlessMode)
        return;

    // Ignore the request if the sound sequence number is invalid
    if (soundId >= NUMSFX)
        return;

    // Grab the listener (player) and default the pan/volume for now
    mobj_t* pListener = gPlayers[gCurPlayerIndex].mo;
    int32_t vol = WESS_MAX_MASTER_VOL;
    int32_t pan = WESS_PAN_CENTER;

    const bool bAttenuateSound = (pOrigin && pListener && (pOrigin != pListener));

    if (bAttenuateSound) {
        // Figure out the approximate distance to the sound source and don't play if too far away
        const fixed_t dx = std::abs(pListener->x - pOrigin->x);
        const fixed_t dy = std::abs(pListener->y - pOrigin->y);
        const fixed_t approxDist = dx + dy - d_rshift<1>(std::min(dx, dy));

        if (approxDist > S_CLIPPING_DIST)
            return;

        // Figure out the relative angle to the player.
        // Not sure what the addition of UINT32_MAX is about, was in Linux Doom also but not commented.
        angle_t angle = R_PointToAngle2(pListener->x, pListener->y, pOrigin->x, pOrigin->y);

        if (angle <= pListener->angle) {
            angle += UINT32_MAX;
        }

        angle -= pListener->angle;

        // Figure out pan amount based on the angle
        {
            const fixed_t sina = gFineSine[angle >> ANGLETOFINESHIFT];
            pan = WESS_PAN_CENTER - d_rshift<1>(d_fixed_to_int(FixedMul(sina, S_STEREO_SWING)));
        }

        // Figure out volume level
        if (approxDist < S_CLOSE_DIST) {
            vol = WESS_MAX_MASTER_VOL;
        } else {
            vol = (d_fixed_to_int(S_CLIPPING_DIST - approxDist) * WESS_MAX_MASTER_VOL) / S_ATTENUATOR;
        }

        // If the origin is exactly where the player is then do no pan
        if ((pOrigin->x == pListener->x) && (pOrigin->y == pListener->y)) {
            pan = WESS_PAN_CENTER;
        }

        // If volume is zero then don't play
        if (vol <= 0)
            return;
    }

    // Disable reverb if the origin is in a sector that forbids it
    int32_t reverb;

    if (pOrigin && (pOrigin->subsector->sector->flags & SF_NO_REVERB)) {
        reverb = 0;
    } else {
        reverb = WESS_MAX_REVERB_DEPTH;
    }

    // Queue the sound
    queued_sound_t& sound = gQueuedSounds.emplace_back();
    sound.pOrigin = pOrigin;
    sound.soundId = soundId;
    sound.volume = (uint8_t) std::min<int32_t>(vol, WESS_MAX_MASTER_VOL);
    sound.pan = (uint8_t) std::clamp<int32_t>(pan, WESS_PAN_LEFT, WESS_PAN_RIGHT);
    sound.reverb = (uint8_t) std::clamp<int32_t>(reverb, 0, WESS_MAX_REVERB_DEPTH);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Sorts the queued sounds by sound id
//------------------------------------------------------------------------------------------------------------------------------------------
static void I_SortQueuedSounds() noexcept {
    std::sort(
        gQueuedSounds.begin(),
        gQueuedSounds.end(),
        [](const queued_sound_t& s1, const queued_sound_t& s2) noexcept {
            return (s1.soundId < s2.soundId);
        }
    );
}

//------------------------------------------------------------------------------------------------------------------------------------------
// PsyDoom: plays a single sound type from the currently queued sounds list.
// Takes an iterator to the sound to play and returns the iterator to the next sound to play.
// Merges the same sounds together so that duplicates are not played and assumes the sound list is sorted by sound id.
//------------------------------------------------------------------------------------------------------------------------------------------
static queued_sound_iter I_PlayQueuedSound(queued_sound_iter curIter) noexcept {
    // No more sounds to play?
    if (curIter == gQueuedSounds.end())
        return curIter;

    // Initially just playing the first sound pointed to.
    // Note that reverb and pan levels are weighted based on sound volume.
    const sfxenum_t soundId = curIter->soundId;
    double vol = curIter->volume;
    double avgPan = curIter->pan * vol;
    double avgReverb = curIter->reverb * vol;

    // We score the sounds in order to decide which pointer to the sound origin to use.
    // Prefer louder sounds that have less panning.
    auto scoreSound = [](const queued_sound_t& sound) noexcept {
        const int32_t volumeScore = sound.volume * sound.volume;
        const int32_t centeredPan = (int32_t) sound.pan - (int32_t) WESS_PAN_CENTER;
        const int32_t panScore = -(centeredPan * centeredPan);
        return volumeScore + panScore;
    };

    int32_t bestSoundScore = scoreSound(*curIter);
    mobj_t* pSoundOrigin = curIter->pOrigin;

    // Merge all other sounds with the same id
    ++curIter;

    while (curIter != gQueuedSounds.end()) {
        // Different sound?
        if (curIter->soundId != soundId)
            break;

        // Accumulate this sound so it will be merged and weight pan/reverb by volume level
        const double curSoundVol = curIter->volume;
        vol += curSoundVol;
        avgPan += curIter->pan * curSoundVol;
        avgReverb += curIter->reverb * curSoundVol;

        // If this sound is preferred then use it's origin as the merged origin
        const int32_t soundScore = scoreSound(*curIter);

        if (soundScore > bestSoundScore) {
            pSoundOrigin = curIter->pOrigin;
            bestSoundScore = soundScore;
        }

        ++curIter;
    }

    avgPan /= vol;
    avgPan = std::clamp(avgPan, (double) WESS_PAN_LEFT, (double) WESS_PAN_RIGHT);
    avgReverb /= vol;
    avgReverb = std::min(avgReverb, (double) WESS_MAX_REVERB_DEPTH);

    vol = std::min(vol, (double) WESS_MAX_MASTER_VOL);

    // Play the merged sound
    queued_sound_t mergedSound = {};
    mergedSound.pOrigin = pSoundOrigin;
    mergedSound.soundId = soundId;
    mergedSound.volume = (uint8_t) vol;
    mergedSound.pan = (uint8_t) std::round(avgPan);
    mergedSound.reverb = (uint8_t) std::round(avgReverb);

    I_StartSound(mergedSound);

    // Return the iterator to the next sound to play
    return curIter;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Play the sound effect with the given id.
// The origin parameter is optional and helps determine panning/attenuation.
// PsyDoom: this is a complete rewrite of the original 'S_StartSound', for the original implementation, see the 'old' code folder.
//------------------------------------------------------------------------------------------------------------------------------------------
void S_StartSound(mobj_t* const pOrigin, const sfxenum_t soundId) noexcept {
    I_QueueSound(pOrigin, soundId);
}

#endif  // #if PSYDOOM_MODS

//------------------------------------------------------------------------------------------------------------------------------------------
// Placeholder where sound update logic per tick can be done.
// This didn't do anything for PSX DOOM other than incrementing this global sound tick counter.
// Updates to the sequencer system were instead driven by hardware timer interrupts, firing approximately 120 times a second.
// 
// PsyDoom: it now has a purpose, playing previously queued sounds!
//------------------------------------------------------------------------------------------------------------------------------------------
void S_UpdateSounds() noexcept {
    // PsyDoom: removed this tick counter as it was not needed
    #if !PSYDOOM_MODS
        gNumSoundTics++;
    #endif

    // PsyDoom: play all previously queued sounds and clear the queue.
    // Prior to playing, sort the queue so that the same sounds played multiple times can be merged.
    #if PSYDOOM_MODS
        I_SortQueuedSounds();

        for (queued_sound_iter iter = gQueuedSounds.begin(); iter != gQueuedSounds.end();) {
            iter = I_PlayQueuedSound(iter);
        }

        gQueuedSounds.clear();
    #endif
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Initialize the PlayStation sound system.
// The given temporary buffer is used to hold the .WMD file while it is being processed.
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSoundInit(const int32_t sfxVol, const int32_t musVol, void* const pTmpWmdLoadBuffer) noexcept {
    // Initialize the WESS API and low level CDROM utilities
    wess_init();
    psxcd_init();

    // Read the WMD file into the given buffer (assumes it is big enough)
    PsxCd_File* const pFile = psxcd_open(CdFileId::DOOMSND_WMD);

    #if PSYDOOM_MODS
        if ((!pFile) || (pFile->size <= 0)) {
            FatalErrors::raise("Failed to open DOOMSFX.WMD!");
        }
    #endif

    // PsyDoom: the 'PsxCd_File' struct has changed layout & contents
    #if PSYDOOM_MODS
        const int32_t wmdFileSize = pFile->size;
        psxcd_read(pTmpWmdLoadBuffer, wmdFileSize, *pFile);
    #else
        psxcd_read(pTmpWmdLoadBuffer, pFile->file.size, *pFile);
    #endif

    psxcd_close(*pFile);

    // Initialize the sample blocks used to keep track what sounds are uploaded to where in SPU RAM
    S_InitSampleBlock(gDoomSndBlock);
    S_InitSampleBlock(gMapSndBlock);

    // Load the main module (.WMD) file.
    // This initializes common stuff used for all music and sounds played in the game.
    //
    // PsyDoom limit removing: dynamically size the available amount of WMD memory based on the .WMD file size.
    // Use a very conservative 2x factor for this, the original game had about 1/3 of the .WMD file size reserved (Final Doom, the largest .WMD).
    // This should allow memory to grow if custom mods have very complex music tracks that consume lots of memory.
    #if PSYDOOM_LIMIT_REMOVING
        gSound_WmdMemSize = std::max<uint32_t>(WMD_MIN_MEM_SIZE, wmdFileSize * 2);
        gSound_WmdMem.reset(new uint8_t[gSound_WmdMemSize]);

        wess_load_module(pTmpWmdLoadBuffer, gSound_WmdMem.get(), gSound_WmdMemSize, gSound_SettingsLists);
    #else
        wess_load_module(pTmpWmdLoadBuffer, gSound_WmdMem, WMD_MEM_SIZE, gSound_SettingsLists);
    #endif

    // Initialize the music sequence and LCD (samples file) loaders
    master_status_structure& mstat = *wess_get_master_status();
    wess_dig_lcd_loader_init(&mstat);
    wess_seq_loader_init(&mstat, CdFileId::DOOMSND_WMD, true);

    // Load all of the very simple 'music sequences' for sound effects in the game.
    // These are kept loaded at all times since they are small.
    const int32_t sfxSequenceBytes = wess_seq_range_load(0, NUMSFX, wess_get_wmd_end());

    // We load map music sequences to the start of the remaining bytes in WMD memory buffer
    gpSound_MusicSeqData = wess_get_wmd_end() + sfxSequenceBytes;

    // Init master volume levels
    S_SetSfxVolume(sfxVol);
    S_SetMusicVolume(musVol);

    // Load the main SFX LCD: this is required for the main menu.
    // Also set the location where map sounds will be uploaded to in SPU RAM, after the memory occupied by this LCD.
    gbDidLoadDoomSfxLcd = false;
    gSound_MapLcdSpuStartAddr = SPU_RAM_APP_BASE + wess_dig_lcd_load(CdFileId::DOOMSFX_LCD, SPU_RAM_APP_BASE, &gDoomSndBlock, false);
    gbDidLoadDoomSfxLcd = true;

    // PsyDoom: reserve memory in the queued sounds list
    #if PSYDOOM_MODS
        gQueuedSounds.reserve(128);
    #endif
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Shutdown the PlayStation sound system
//------------------------------------------------------------------------------------------------------------------------------------------
void PsxSoundExit() noexcept {
    // Did nothing - not required for a PS1 game...
}

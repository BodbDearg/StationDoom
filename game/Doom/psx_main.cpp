#include "psx_main.h"

#include "Base/i_main.h"
#include "cdmaptbl.h"
#include "PcPsx/Config.h"
#include "PcPsx/Game.h"
#include "PcPsx/Input.h"
#include "PcPsx/ModMgr.h"
#include "PcPsx/ProgArgs.h"
#include "PcPsx/PsxVm.h"
#include "PcPsx/Video.h"

//------------------------------------------------------------------------------------------------------------------------------------------
// This was the old reverse engineered entrypoint for PSXDOOM.EXE, which executed before 'main()' was called.
//
// The code that was previously here was generated by the compiler and/or toolchain and did stuff which the user application could not do
// such as deciding on the value of the 'gp' register, zero initializing the BSS section global variables and so on. Basically all of the
// bootstrap stuff before the C program can run.
//
// There used to be other actual PSX code in here, but as layers of emulation were removed this function had less and less purpose.
// Now I'm using it as a place to initialize all of PsyDoom's systems before launching the original 'main()' of PSXDOOM.EXE.
//
// Code which was removed includes:
//  (1) Save and restore the current return address register (ra) for returning control to operating system after the program is done.
//  (2) Setting the value of the 'global pointer' (gp) register which is used as a base to reference many global variables.
//      For the 'Greatest Hits' edition of PlayStation Doom (US) 'gp' was set to: 0x800775E0
//  (3) Setting the value of the 'frame pointer' (fp) register.
//  (4) Setting the value of the 'stack pointer' (sp) register.
//  (5) Zero initializing the BSS section globals. These are globals with a defaulted (0) value.
//  (6) Calling 'LIBAPI_InitHeap' to initialize the PsyQ SDK Heap.
//      Note that since Doom used it's own zone memory management system, that call was effectively useless.
//      It never once asked for memory from the PsyQ SDK, nor did any of the PsyQ functions used.
//      Doom's own heap would have been corrupted if LIBAPI's heap was in conflicting use of the same memory.
//------------------------------------------------------------------------------------------------------------------------------------------
int psx_main(const int argc, const char** const argv) noexcept {
    // PC-PSX: setup logic for the new game host environment
    #if PC_PSX_DOOM_MODS
        // Parse command line arguments and configuration and initialize input systems
        ProgArgs::init(argc, argv);
        Config::init();
        Input::init();

        // Initialize the emulated PSX components using the PSX Doom disc (supplied as a .cue file).
        // TODO: make this path configurable.
        if (!PsxVm::init("Doom.cue"))
            return 1;

        // Initialize the table of files on the CD from the file system and determine the game type
        CdMapTbl_Init();
        Game::determineGameTypeAndVariant();
        
        // Initialize the display and the modding manager
        Video::initVideo();
        ModMgr::init();
    #endif

    // Call the original PSX Doom 'main()' function
    I_Main();

    // PC-PSX: cleanup logic after Doom itself is done
    #if PC_PSX_DOOM_MODS
        PsxVm::shutdown();
        ModMgr::shutdown();
        Input::shutdown();
        Config::shutdown();
        ProgArgs::shutdown();
    #endif

    return 0;
}

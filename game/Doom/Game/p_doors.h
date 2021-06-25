#pragma once

#include "Doom/doomdef.h"

enum sfxenum_t : int32_t;
struct line_t;
struct mobj_t;
struct sector_t;

// Enum representing a door type
enum vldoor_e : int32_t {
    Normal              = 0,
    Close30ThenOpen     = 1,
    Close               = 2,
    Open                = 3,
    RaiseIn5Mins        = 4,
    BlazeRaise          = 5,
    BlazeOpen           = 6,
    BlazeClose          = 7
};

// PsyDoom: definition for a custom cdoor.
// The constructor tries to construct with reasonable default settings.
#if PSYDOOM_MODS
    struct CustomDoorDef {
        CustomDoorDef() noexcept;

        bool        bOpen;                      // Does the door open ('true') or close ('false') initially?
        bool        bDoReturn;                  // If 'true' then return the door to it's prior open/closed state before the door is done
        bool        bBlockable;                 // If 'true' then the door goes back up if closing and something is underneath it
        bool        bDoFinishScript;            // Call the finish script action when the door is done?
        fixed_t     minHeight;                  // Minimum (closed) ceiling height for the door
        fixed_t     maxHeight;                  // Maximum (open) ceiling height for the door
        fixed_t     speed;                      // Speed of the door
        int32_t     waitTime;                   // How long the door waits (in tics) before trying to return to it's previous state (if returning at all)
        sfxenum_t   openSound;                  // Sound to make when opening ('sfx_None' if none)
        sfxenum_t   closeSound;                 // Sound to make when closing ('sfx_None' if none)
        int32_t     finishScriptActionNum;      // If enabled, a script action to execute when the door has finished up completely
        int32_t     finishScriptUserdata;       // Userdata to pass to the 'finish' script action
    };
#endif

bool P_CheckKeyLock(line_t& line, mobj_t& user) noexcept;
bool EV_DoDoor(line_t& line, const vldoor_e doorType) noexcept;
void EV_VerticalDoor(line_t& line, mobj_t& mobj) noexcept;
void P_SpawnDoorCloseIn30(sector_t& sector) noexcept;
void P_SpawnDoorRaiseIn5Mins(sector_t& sector, [[maybe_unused]] const int32_t secNum) noexcept;

#if PSYDOOM_MODS
    bool EV_DoCustomDoor(sector_t& sector, const CustomDoorDef& def) noexcept;
#endif

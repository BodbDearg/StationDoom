#pragma once

#include "Doom/doomdef.h"

struct line_t;

// Represents the result of moving a floor/ceiling
enum result_e : uint32_t {
    ok          = 0,    // Movement for the floor/ceiling was fully OK
    crushed     = 1,    // The floor/ceiling is crushing things and may not have moved because of this
    pastdest    = 2     // Plane has reached its destination or very close to it (sometimes stops just before if crushing things)
};

// Enum for a moving floor type
enum floor_e : uint32_t {
    lowerFloor              = 0,    // Lower floor to highest surrounding floor
    lowerFloorToLowest      = 1,    // Lower floor to lowest surrounding floor
    turboLower              = 2,    // Lower floor to highest surrounding floor VERY FAST
    raiseFloor              = 3,    // Raise floor to lowest surrounding CEILING
    raiseFloorToNearest     = 4,    // Raise floor to next highest surrounding floor
    raiseToTexture          = 5,    // Raise floor to shortest height texture around it
    lowerAndChange          = 6,    // Lower floor to lowest surrounding floor and change the floorpic
    raiseFloor24            = 7,
    raiseFloor24AndChange   = 8,
    raiseFloorCrush         = 9,
    donutRaise              = 10
};

// Holds the state and settings for a moving floor
struct floormove_t {
    thinker_t       thinker;
    floor_e         type;
    bool32_t        crush;              // Does the floor movement cause crushing?
    VmPtr<sector_t> sector;             // The sector affected
    int32_t         direction;          // 1 = up, -1 = down
    int32_t         newspecial;         // TODO: comment
    int16_t         texture;            // TODO: comment
    int16_t         _pad;               // Unused padding
    fixed_t         floordestheight;
    fixed_t         speed;
};

static_assert(sizeof(floormove_t) == 44);

result_e T_MovePlane(
    sector_t& sector,
    const fixed_t speed,
    const fixed_t destHeight,
    const bool bCrush,
    const int32_t floorOrCeiling,
    const int32_t direction
) noexcept;

bool EV_DoFloor(line_t& line, const floor_e floorType) noexcept;
void EV_BuildStairs() noexcept;

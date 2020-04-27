#include "p_mobj.h"

#include "Doom/Base/i_main.h"
#include "Doom/Base/i_misc.h"
#include "Doom/Base/m_random.h"
#include "Doom/Base/s_sound.h"
#include "Doom/Base/sounds.h"
#include "Doom/Base/z_zone.h"
#include "Doom/d_main.h"
#include "Doom/Renderer/r_local.h"
#include "Doom/Renderer/r_main.h"
#include "Doom/UI/pw_main.h"
#include "Doom/UI/st_main.h"
#include "doomdata.h"
#include "g_game.h"
#include "info.h"
#include "p_local.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_password.h"
#include "p_pspr.h"
#include "p_setup.h"
#include "p_tick.h"
#include "PsxVm/PsxVm.h"

// Item respawn queue
static constexpr int32_t ITEMQUESIZE = 64;

const VmPtr<int32_t>    gItemRespawnQueueHead(0x80078138);      // Head of the circular queue
const VmPtr<int32_t>    gItemRespawnQueueTail(0x80078180);      // Tail of the circular queue

static const VmPtr<int32_t[ITEMQUESIZE]>        gItemRespawnTime(0x80097910);       // When each item in the respawn queue began the wait to respawn
static const VmPtr<mapthing_t[ITEMQUESIZE]>     gItemRespawnQueue(0x8008612C);      // Details for the things to be respawned

// Object kill tracking
const VmPtr<int32_t> gNumMObjKilled(0x80078010);

//------------------------------------------------------------------------------------------------------------------------------------------
// Removes the given map object from the game
//------------------------------------------------------------------------------------------------------------------------------------------
void P_RemoveMObj(mobj_t& mobj) noexcept {  
    // Respawn the item later it's the right type
    const bool bRespawn = (
        (mobj.flags & MF_SPECIAL) && 
        ((mobj.flags & MF_DROPPED) == 0) &&
        (mobj.type != MT_INV) &&
        (mobj.type != MT_INS)
    );

    if (bRespawn) {
        // Remember the item details for later respawning and occupy one queue slot
        const int32_t slotIdx = (*gItemRespawnQueueHead) & (ITEMQUESIZE - 1);

        gItemRespawnTime[slotIdx] = *gTicCon;
        gItemRespawnQueue[slotIdx].x = mobj.spawnx;
        gItemRespawnQueue[slotIdx].y = mobj.spawny;
        gItemRespawnQueue[slotIdx].type = mobj.spawntype;
        gItemRespawnQueue[slotIdx].angle = mobj.spawnangle;

        *gItemRespawnQueueHead += 1;
    }

    // Remove the thing from sector thing lists and the blockmap
    P_UnsetThingPosition(mobj);

    // Remove from the global linked list of things and deallocate
    mobj.next->prev = mobj.prev;
    mobj.prev->next = mobj.next;
    Z_Free2(*gpMainMemZone->get(), &mobj);
}

// TODO: remove eventually. Needed at the minute due to 'latecall' function pointer invocations of this function.
void _thunk_P_RemoveMObj() noexcept {
    P_RemoveMObj(*vmAddrToPtr<mobj_t>(a0));
}

void P_RespawnSpecials() noexcept {
loc_8001C838:
    sp -= 0x20;
    v1 = *gNetGame;
    v0 = 2;                                             // Result = 00000002
    sw(ra, sp + 0x1C);
    sw(s2, sp + 0x18);
    sw(s1, sp + 0x14);
    sw(s0, sp + 0x10);
    if (v1 != v0) goto loc_8001C9FC;
    v1 = *gItemRespawnQueueHead;
    v0 = *gItemRespawnQueueTail;
    {
        const bool bJump = (v1 == v0);
        v0 = v1 - v0;
        if (bJump) goto loc_8001C9FC;
    }
    v0 = (i32(v0) < 0x41);
    {
        const bool bJump = (v0 != 0);
        v0 = v1 - 0x40;
        if (bJump) goto loc_8001C880;
    }
    *gItemRespawnQueueTail = v0;
loc_8001C880:
    v0 = *gItemRespawnQueueTail;
    a1 = v0 & 0x3F;
    a0 = a1 << 2;
    v0 = *gTicCon;
    at = 0x80090000;                                    // Result = 80090000
    at += 0x7910;                                       // Result = gItemRespawnTime[0] (80097910)
    at += a0;
    v1 = lw(at);
    v0 -= v1;
    v0 = (i32(v0) < 0x708);
    {
        const bool bJump = (v0 != 0);
        v0 = a0 + a1;
        if (bJump) goto loc_8001C9FC;
    }
    v0 <<= 1;
    v1 = 0x80080000;                                    // Result = 80080000
    v1 += 0x612C;                                       // Result = gItemRespawnQueue[0] (8008612C)
    s0 = v0 + v1;
    v0 = lh(s0);
    s2 = v0 << 16;
    v0 = lh(s0 + 0x2);
    a0 = s2;
    s1 = v0 << 16;
    a1 = s1;
    _thunk_R_PointInSubsector();
    a0 = s2;
    v0 = lw(v0);
    a1 = s1;
    a2 = lw(v0);
    a3 = 0x1E;
    v0 = ptrToVmAddr(P_SpawnMObj(a0, a1, a2, (mobjtype_t) a3));
    a0 = v0;
    a1 = sfx_itmbk;
    S_StartSound(vmAddrToPtr<mobj_t>(a0), (sfxenum_t) a1);
    a3 = 0;
    v1 = 0;
    a0 = lh(s0 + 0x6);
loc_8001C91C:
    at = 0x80060000;                                    // Result = 80060000
    at -= 0x1FC4;                                       // Result = MObjInfo_MT_PLAYER[0] (8005E03C)
    at += v1;
    v0 = lw(at);
    a2 = 0x80000000;                                    // Result = 80000000
    if (a0 == v0) goto loc_8001C948;
    a3++;
    v0 = (i32(a3) < 0x7F);
    v1 += 0x58;
    if (v0 != 0) goto loc_8001C91C;
loc_8001C948:
    v0 = a3 << 1;
    v0 += a3;
    v0 <<= 2;
    v0 -= a3;
    v0 <<= 3;
    at = 0x80060000;                                    // Result = 80060000
    at -= 0x1F70;                                       // Result = MObjInfo_MT_PLAYER[15] (8005E090)
    at += v0;
    v0 = lw(at);
    v0 &= 0x100;
    a0 = s2;
    if (v0 == 0) goto loc_8001C984;
    a2 = 0x7FFF0000;                                    // Result = 7FFF0000
    a2 |= 0xFFFF;                                       // Result = 7FFFFFFF
loc_8001C984:
    a1 = s1;
    v0 = ptrToVmAddr(P_SpawnMObj(a0, a1, a2, (mobjtype_t) a3));
    a1 = 0xB60B0000;                                    // Result = B60B0000
    v1 = lhu(s0 + 0x4);
    a1 |= 0x60B7;                                       // Result = B60B60B7
    v1 <<= 16;
    a0 = u32(i32(v1) >> 16);
    mult(a0, a1);
    a1 = v0;
    v1 = u32(i32(v1) >> 31);
    v0 = hi;
    v0 += a0;
    v0 = u32(i32(v0) >> 5);
    v0 -= v1;
    v0 <<= 29;
    sw(v0, a1 + 0x24);
    v0 = lhu(s0);
    sh(v0, a1 + 0x88);
    v0 = lhu(s0 + 0x2);
    sh(v0, a1 + 0x8A);
    v0 = lhu(s0 + 0x6);
    sh(v0, a1 + 0x8C);
    v0 = *gItemRespawnQueueTail;
    v1 = lhu(s0 + 0x4);
    v0++;
    *gItemRespawnQueueTail = v0;
    sh(v1, a1 + 0x8E);
loc_8001C9FC:
    ra = lw(sp + 0x1C);
    s2 = lw(sp + 0x18);
    s1 = lw(sp + 0x14);
    s0 = lw(sp + 0x10);
    sp += 0x20;
    return;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Sets the state for the given map object.
// Returns 'true' if the map object is still present and hasn't been removed - it's removed if the state switch is to 'S_NULL'.
//------------------------------------------------------------------------------------------------------------------------------------------
bool P_SetMObjState(mobj_t& mobj, const statenum_t stateNum) noexcept {
    // Remove the map object if the state is null
    if (stateNum == S_NULL) {
        mobj.state = nullptr;
        P_RemoveMObj(mobj);
        return false;
    }

    // Set the new state and call the action function for the state (if any)
    state_t& state = gStates[stateNum];

    mobj.state = &state;
    mobj.tics = state.tics;
    mobj.sprite = state.sprite;
    mobj.frame = state.frame;

    if (state.action) {
        // FIXME: convert to native call
        const VmFunc func = PsxVm::getVmFuncForAddr(state.action);
        a0 = ptrToVmAddr(&mobj);
        func();
    }

    // This request gets cleared on state switch
    mobj.latecall = nullptr;
    return true;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Explodes the given missile: sends it into the death state, plays the death sound and removes the MF_MISSILE flag
//------------------------------------------------------------------------------------------------------------------------------------------
void P_ExplodeMissile(mobj_t& mobj) noexcept {
    // Kill all momentum and enter death state
    mobj.momz = 0;
    mobj.momy = 0;
    mobj.momx = 0;
    P_SetMObjState(mobj, gMObjInfo[mobj.type].deathstate);

    // Some random state tic variation
    mobj.tics -= P_Random() & 1;
    
    if (mobj.tics < 1) {
        mobj.tics = 1;
    }

    // No longer a missile
    mobj.flags &= (~MF_MISSILE);

    // Stop the missile sound and start the explode sound
    if (mobj.info->deathsound != sfx_None) {
        S_StopSound(mobj.target);
        S_StartSound(&mobj, mobj.info->deathsound);
    }
}

// TODO: remove eventually. Needed at the minute due to 'latecall' function pointer invocations of this function.
void _thunk_P_ExplodeMissile() noexcept {
    P_ExplodeMissile(*vmAddrToPtr<mobj_t>(a0));
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Spawn a thing with the specified type at the given location in space
//------------------------------------------------------------------------------------------------------------------------------------------
mobj_t* P_SpawnMObj(const fixed_t x, const fixed_t y, const fixed_t z, const mobjtype_t type) noexcept {
    // Alloc and zero initialize the map object
    mobj_t& mobj = *(mobj_t*) Z_Malloc(*gpMainMemZone->get(), sizeof(mobj_t), PU_LEVEL, nullptr);
    D_memset(&mobj, (std::byte) 0, sizeof(mobj_t));

    // Fill in basic fields
    mobjinfo_t& info = gMObjInfo[type];
    mobj.type = type;
    mobj.info = &info;
    mobj.x = x;
    mobj.y = y;
    mobj.radius = info.radius;
    mobj.height = info.height;
    mobj.flags = info.flags;
    mobj.health = info.spawnhealth;
    mobj.reactiontime = info.reactiontime;

    // Set initial state and state related stuff.
    // Note: can't set state with P_SetMobjState, because actions can't be called yet (thing not fully initialized).
    state_t& state = gStates[info.spawnstate];
    mobj.state = &state;
    mobj.tics = state.tics;
    mobj.sprite = state.sprite;
    mobj.frame = state.frame;
    
    // Add to the sector thing list and the blockmap
    P_SetThingPosition(mobj);
    
    // Decide on the z position for the thing (specified z, or floor/ceiling)
    sector_t& sec = *mobj.subsector->sector.get();
    mobj.floorz = sec.floorheight;
    mobj.ceilingz = sec.ceilingheight;
    
    if (z == ONFLOORZ) {
        mobj.z = mobj.floorz;
    } else if (z == ONCEILINGZ) {
        mobj.z = sec.ceilingheight - mobj.info->height;
    } else {
        mobj.z = z;
    }

    // Add into the linked list of things
    gMObjHead->prev->next = &mobj;
    mobj.next = gMObjHead.get();
    mobj.prev = gMObjHead->prev;
    gMObjHead->prev = &mobj;
    return &mobj;
}

void P_SpawnPlayer() noexcept {
loc_8001CE40:
    sp -= 0x38;
    sw(s4, sp + 0x28);
    s4 = a0;
    sw(ra, sp + 0x30);
    sw(s5, sp + 0x2C);
    sw(s3, sp + 0x24);
    sw(s2, sp + 0x20);
    sw(s1, sp + 0x1C);
    sw(s0, sp + 0x18);
    a0 = lh(s4 + 0x6);
    v1 = a0 << 2;
    at = *gpSectors;
    at += v1;
    v0 = lw(at);
    v1 += a0;
    if (v0 != 0) goto loc_8001CE94;
    v0 = 0;                                             // Result = 00000000
    goto loc_8001D15C;
loc_8001CE94:
    v0 = v1 << 4;
    v0 -= v1;
    v0 <<= 2;
    v1 = 0x800B0000;                                    // Result = 800B0000
    v1 -= 0x7940;                                       // Result = gTmpWadLumpBuffer[3FDE] (800A86C0)
    s3 = v0 + v1;
    v0 = lw(s3 + 0x4);
    s5 = 2;                                             // Result = 00000002
    a1 = 0x94;                                          // Result = 00000094
    if (v0 != s5) goto loc_8001CEC8;
    a0--;
    G_PlayerReborn(a0);
    a1 = 0x94;                                          // Result = 00000094
loc_8001CEC8:
    a2 = 2;                                             // Result = 00000002
    a3 = 0;                                             // Result = 00000000
    a0 = *gpMainMemZone;
    s1 = lh(s4);
    s2 = lh(s4 + 0x2);
    s1 <<= 16;
    s2 <<= 16;
    _thunk_Z_Malloc();
    s0 = v0;
    a0 = s0;
    a1 = 0;                                             // Result = 00000000
    a2 = 0x94;                                          // Result = 00000094
    _thunk_D_memset();
    v0 = 0x80060000;                                    // Result = 80060000
    v0 -= 0x1FC4;                                       // Result = MObjInfo_MT_PLAYER[0] (8005E03C)
    sw(0, s0 + 0x54);
    sw(v0, s0 + 0x58);
    sw(s1, s0);
    sw(s2, s0 + 0x4);
    v0 = 0x80060000;                                    // Result = 80060000
    v0 = lw(v0 - 0x1F84);                               // Load from: MObjInfo_MT_PLAYER[10] (8005E07C)
    sw(v0, s0 + 0x40);
    v0 = 0x80060000;                                    // Result = 80060000
    v0 = lw(v0 - 0x1F80);                               // Load from: MObjInfo_MT_PLAYER[11] (8005E080)
    sw(v0, s0 + 0x44);
    v0 = 0x80060000;                                    // Result = 80060000
    v0 = lw(v0 - 0x1F70);                               // Load from: MObjInfo_MT_PLAYER[15] (8005E090)
    sw(v0, s0 + 0x64);
    v0 = 0x80060000;                                    // Result = 80060000
    v0 = lw(v0 - 0x1FBC);                               // Load from: MObjInfo_MT_PLAYER[2] (8005E044)
    sw(v0, s0 + 0x68);
    v0 = 0x80060000;                                    // Result = 80060000
    v0 = lw(v0 - 0x1FB0);                               // Load from: MObjInfo_MT_PLAYER[5] (8005E050)
    sw(v0, s0 + 0x78);
    v1 = 0x80060000;                                    // Result = 80060000
    v1 = lw(v1 - 0x1FC0);                               // Load from: MObjInfo_MT_PLAYER[1] (8005E040)
    v0 = v1 << 3;
    v0 -= v1;
    v0 <<= 2;
    v1 = 0x80060000;                                    // Result = 80060000
    v1 -= 0x7274;                                       // Result = State_S_NULL[0] (80058D8C)
    v0 += v1;
    sw(v0, s0 + 0x60);
    v1 = lw(v0 + 0x8);
    sw(v1, s0 + 0x5C);
    v1 = lw(v0);
    sw(v1, s0 + 0x28);
    v0 = lw(v0 + 0x4);
    a0 = s0;
    sw(v0, s0 + 0x2C);
    P_SetThingPosition(*vmAddrToPtr<mobj_t>(a0));
    v0 = lw(s0 + 0xC);
    v0 = lw(v0);
    v1 = lw(s0 + 0xC);
    v0 = lw(v0);
    sw(v0, s0 + 0x38);
    v0 = lw(v1);
    v1 = lw(s0 + 0x38);
    v0 = lw(v0 + 0x4);
    sw(v1, s0 + 0x8);
    sw(v0, s0 + 0x3C);
    v0 = 0x800B0000;                                    // Result = 800B0000
    v0 = lw(v0 - 0x7160);                               // Load from: gMObjHead[4] (800A8EA0)
    sw(s0, v0 + 0x14);
    v0 = 0x800B0000;                                    // Result = 800B0000
    v0 -= 0x7170;                                       // Result = gMObjHead[0] (800A8E90)
    sw(v0, s0 + 0x14);
    v0 = 0x800B0000;                                    // Result = 800B0000
    v0 = lw(v0 - 0x7160);                               // Load from: gMObjHead[4] (800A8EA0)
    a1 = 0xB60B0000;                                    // Result = B60B0000
    sw(v0, s0 + 0x10);
    at = 0x800B0000;                                    // Result = 800B0000
    sw(s0, at - 0x7160);                                // Store to: gMObjHead[4] (800A8EA0)
    v1 = lhu(s4 + 0x4);
    a1 |= 0x60B7;                                       // Result = B60B60B7
    v1 <<= 16;
    a0 = u32(i32(v1) >> 16);
    mult(a0, a1);
    sw(s3, s0 + 0x80);
    v1 = u32(i32(v1) >> 31);
    v0 = hi;
    v0 += a0;
    v0 = u32(i32(v0) >> 5);
    v0 -= v1;
    v0 <<= 29;
    sw(v0, s0 + 0x24);
    v0 = lw(s3 + 0x24);
    v1 = 0x290000;                                      // Result = 00290000
    sw(v0, s0 + 0x68);
    v0 = 0x24;                                          // Result = 00000024
    sw(s0, s3);
    sw(0, s3 + 0x4);
    sw(0, s3 + 0xC4);
    sw(0, s3 + 0xD4);
    sw(0, s3 + 0xD8);
    sw(0, s3 + 0xDC);
    sw(0, s3 + 0xE4);
    sw(0, s3 + 0xE8);
    sw(v1, s3 + 0x18);
    sw(v0, s3 + 0x120);
    v0 = lw(s0 + 0x8);
    v0 += v1;
    sw(v0, s3 + 0x14);
    a0 = lh(s4 + 0x6);
    a0--;
    P_SetupPsprites();
    v0 = *gNetGame;
    if (v0 != s5) goto loc_8001D0DC;
    a0 = 1;                                             // Result = 00000001
    v1 = 5;                                             // Result = 00000005
    v0 = s3 + 0x14;
loc_8001D0C4:
    sw(a0, v0 + 0x48);
    v1--;
    v0 -= 4;
    if (i32(v1) >= 0) goto loc_8001D0C4;
    v0 = *gNetGame;
loc_8001D0DC:
    if (v0 != 0) goto loc_8001D130;
    v0 = lh(s4 + 0x6);
    v1 = *gCurPlayerIndex;
    v0--;
    {
        const bool bJump = (v0 != v1);
        v0 = s0;
        if (bJump) goto loc_8001D15C;
    }
    v0 = *gbUsingAPassword;
    a1 = sp + 0x10;
    if (v0 == 0) goto loc_8001D130;
    a0 = 0x80090000;                                    // Result = 80090000
    a0 += 0x6560;                                       // Result = gPasswordCharBuffer[0] (80096560)
    a2 = a1;
    a3 = s3;
    P_ProcessPassword();
    *gbUsingAPassword = false;
loc_8001D130:
    v0 = lh(s4 + 0x6);
    v1 = *gCurPlayerIndex;
    v0--;
    {
        const bool bJump = (v0 != v1);
        v0 = s0;
        if (bJump) goto loc_8001D15C;
    }
    ST_InitEveryLevel();
    I_UpdatePalette();
    v0 = s0;
loc_8001D15C:
    ra = lw(sp + 0x30);
    s5 = lw(sp + 0x2C);
    s4 = lw(sp + 0x28);
    s3 = lw(sp + 0x24);
    s2 = lw(sp + 0x20);
    s1 = lw(sp + 0x1C);
    s0 = lw(sp + 0x18);
    sp += 0x38;
    return;
}

void P_SpawnMapThing() noexcept {
loc_8001D184:
    sp -= 0x30;
    sw(s2, sp + 0x18);
    s2 = a0;
    sw(ra, sp + 0x2C);
    sw(s6, sp + 0x28);
    sw(s5, sp + 0x24);
    sw(s4, sp + 0x20);
    sw(s3, sp + 0x1C);
    sw(s1, sp + 0x14);
    sw(s0, sp + 0x10);
    v1 = lh(s2 + 0x6);
    v0 = (i32(v1) < 3);
    {
        const bool bJump = (v0 == 0);
        v0 = v1 << 2;
        if (bJump) goto loc_8001D23C;
    }
    v0 += v1;
    v0 <<= 1;
    v1 = lwl(v1, s2 + 0x3);
    v1 = lwr(v1, s2);
    a0 = lwl(a0, s2 + 0x7);
    a0 = lwr(a0, s2 + 0x4);
    a1 = lh(s2 + 0x8);
    at = 0x800B0000;                                    // Result = 800B0000
    at -= 0x718B;                                       // Result = gPlayerStarts_-1[1] (800A8E75)
    at += v0;
    swl(v1, at);
    at = 0x800B0000;                                    // Result = 800B0000
    at -= 0x718E;                                       // Result = gPlayerStarts_-1[0] (800A8E72)
    at += v0;
    swr(v1, at);
    at = 0x800B0000;                                    // Result = 800B0000
    at -= 0x7187;                                       // Result = gPlayerStarts_-1[3] (800A8E79)
    at += v0;
    swl(a0, at);
    at = 0x800B0000;                                    // Result = 800B0000
    at -= 0x718A;                                       // Result = gPlayerStarts_-1[2] (800A8E76)
    at += v0;
    swr(a0, at);
    at = 0x800B0000;                                    // Result = 800B0000
    at -= 0x7186;                                       // Result = gPlayerStarts_-1[4] (800A8E7A)
    at += v0;
    sh(a1, at);
    goto loc_8001D6D8;
loc_8001D23C:
    v0 = 0xB;                                           // Result = 0000000B
    a0 = 2;                                             // Result = 00000002
    if (v1 != v0) goto loc_8001D28C;
    a0 = *gpDeathmatchP;
    v0 = 0x800A0000;                                    // Result = 800A0000
    v0 -= 0x7F30;                                       // Result = 800980D0
    v0 = (a0 < v0);
    a1 = s2;
    if (v0 == 0) goto loc_8001D6D8;
    a2 = 0xA;                                           // Result = 0000000A
    _thunk_D_memcpy();
    v0 = *gpDeathmatchP;
    v0 += 0xA;
    *gpDeathmatchP = v0;
    goto loc_8001D6D8;
loc_8001D28C:
    v0 = *gNetGame;
    if (v0 == a0) goto loc_8001D2B4;
    v0 = lhu(s2 + 0x8);
    v0 &= 0x10;
    if (v0 != 0) goto loc_8001D6D8;
loc_8001D2B4:
    v1 = *gGameSkill;
    v0 = (v1 < 2);
    if (v0 == 0) goto loc_8001D2D4;
    a1 = 1;                                             // Result = 00000001
    goto loc_8001D2F4;
loc_8001D2D4:
    v0 = v1 - 3;
    if (v1 != a0) goto loc_8001D2E4;
    a1 = 2;                                             // Result = 00000002
    goto loc_8001D2F4;
loc_8001D2E4:
    v0 = (v0 < 2);
    if (v0 == 0) goto loc_8001D2F4;
    a1 = 4;                                             // Result = 00000004
loc_8001D2F4:
    v0 = lh(s2 + 0x8);
    v0 &= a1;
    s1 = 0;                                             // Result = 00000000
    if (v0 == 0) goto loc_8001D6D8;
    a1 = lh(s2 + 0x6);
    v1 = 0;                                             // Result = 00000000
loc_8001D310:
    at = 0x80060000;                                    // Result = 80060000
    at -= 0x1FC4;                                       // Result = MObjInfo_MT_PLAYER[0] (8005E03C)
    at += v1;
    v0 = lw(at);
    {
        const bool bJump = (a1 == v0);
        v0 = 0x7F;                                      // Result = 0000007F
        if (bJump) goto loc_8001D340;
    }
    s1++;
    v0 = (i32(s1) < 0x7F);
    v1 += 0x58;
    if (v0 != 0) goto loc_8001D310;
    v0 = 0x7F;                                          // Result = 0000007F
loc_8001D340:
    if (s1 != v0) goto loc_8001D360;
    I_Error("P_SpawnMapThing: Unknown doomednum %d at (%d, %d)", (int32_t) a1, (int32_t) a2, (int32_t) a3);
loc_8001D360:
    v1 = *gNetGame;
    v0 = 2;
    s3 = 0x80000000;
    if (v1 != v0) goto loc_8001D3A8;
    v0 = s1 << 1;
    v0 += s1;
    v0 <<= 2;
    v0 -= s1;
    v0 <<= 3;
    at = 0x80060000;                                    // Result = 80060000
    at -= 0x1F70;                                       // Result = MObjInfo_MT_PLAYER[15] (8005E090)
    at += v0;
    v0 = lw(at);
    v1 = 0x2400000;                                     // Result = 02400000
    v0 &= v1;
    if (v0 != 0) goto loc_8001D6D8;
loc_8001D3A8:
    v0 = lh(s2);
    v1 = lh(s2 + 0x2);
    s6 = v0 << 16;
    v0 = s1 << 1;
    v0 += s1;
    v0 <<= 2;
    v0 -= s1;
    s4 = v0 << 3;
    at = 0x80060000;                                    // Result = 80060000
    at -= 0x1F70;                                       // Result = MObjInfo_MT_PLAYER[15] (8005E090)
    at += s4;
    v0 = lw(at);
    v0 &= 0x100;
    s5 = v1 << 16;
    if (v0 == 0) goto loc_8001D3F0;
    s3 = 0x7FFF0000;                                    // Result = 7FFF0000
    s3 |= 0xFFFF;                                       // Result = 7FFFFFFF
loc_8001D3F0:
    a1 = 0x94;                                          // Result = 00000094
    a2 = 2;                                             // Result = 00000002
    a0 = *gpMainMemZone;
    a3 = 0;                                             // Result = 00000000
    _thunk_Z_Malloc();
    s0 = v0;
    a0 = s0;
    a1 = 0;                                             // Result = 00000000
    a2 = 0x94;                                          // Result = 00000094
    _thunk_D_memset();
    v0 = 0x80060000;                                    // Result = 80060000
    v0 -= 0x1FC4;                                       // Result = MObjInfo_MT_PLAYER[0] (8005E03C)
    v0 += s4;                                           // Result = MObjInfo_MT_PLAYER[0] (8005E03C)
    sw(s1, s0 + 0x54);
    sw(v0, s0 + 0x58);
    sw(s6, s0);
    sw(s5, s0 + 0x4);
    v1 = lw(v0 + 0x40);                                 // Load from: MObjInfo_MT_PLAYER[10] (8005E07C)
    sw(v1, s0 + 0x40);
    v1 = lw(v0 + 0x44);                                 // Load from: MObjInfo_MT_PLAYER[11] (8005E080)
    sw(v1, s0 + 0x44);
    v1 = lw(v0 + 0x54);                                 // Load from: MObjInfo_MT_PLAYER[15] (8005E090)
    sw(v1, s0 + 0x64);
    v1 = lw(v0 + 0x8);                                  // Load from: MObjInfo_MT_PLAYER[2] (8005E044)
    sw(v1, s0 + 0x68);
    v1 = lw(v0 + 0x14);                                 // Load from: MObjInfo_MT_PLAYER[5] (8005E050)
    sw(v1, s0 + 0x78);
    v1 = lw(v0 + 0x4);                                  // Load from: MObjInfo_MT_PLAYER[1] (8005E040)
    v0 = v1 << 3;
    v0 -= v1;
    v0 <<= 2;
    v1 = 0x80060000;                                    // Result = 80060000
    v1 -= 0x7274;                                       // Result = State_S_NULL[0] (80058D8C)
    v0 += v1;
    sw(v0, s0 + 0x60);
    v1 = lw(v0 + 0x8);
    sw(v1, s0 + 0x5C);
    v1 = lw(v0);
    sw(v1, s0 + 0x28);
    v0 = lw(v0 + 0x4);
    a0 = s0;
    sw(v0, s0 + 0x2C);
    P_SetThingPosition(*vmAddrToPtr<mobj_t>(a0));
    v0 = lw(s0 + 0xC);
    v0 = lw(v0);
    v1 = lw(s0 + 0xC);
    v0 = lw(v0);
    sw(v0, s0 + 0x38);
    v0 = lw(v1);
    v1 = lw(v0 + 0x4);
    v0 = 0x80000000;                                    // Result = 80000000
    sw(v1, s0 + 0x3C);
    if (s3 != v0) goto loc_8001D500;
    v0 = lw(s0 + 0x38);
    sw(v0, s0 + 0x8);
    goto loc_8001D530;
loc_8001D500:
    v0 = 0x7FFF0000;                                    // Result = 7FFF0000
    v0 |= 0xFFFF;                                       // Result = 7FFFFFFF
    if (s3 != v0) goto loc_8001D52C;
    v0 = lw(s0 + 0x58);
    v0 = lw(v0 + 0x44);
    v0 = v1 - v0;
    sw(v0, s0 + 0x8);
    goto loc_8001D530;
loc_8001D52C:
    sw(s3, s0 + 0x8);
loc_8001D530:
    v0 = 0x800B0000;                                    // Result = 800B0000
    v0 = lw(v0 - 0x7160);                               // Load from: gMObjHead[4] (800A8EA0)
    sw(s0, v0 + 0x14);
    v0 = 0x800B0000;                                    // Result = 800B0000
    v0 -= 0x7170;                                       // Result = gMObjHead[0] (800A8E90)
    sw(v0, s0 + 0x14);
    v0 = 0x800B0000;                                    // Result = 800B0000
    v0 = lw(v0 - 0x7160);                               // Load from: gMObjHead[4] (800A8EA0)
    sw(v0, s0 + 0x10);
    at = 0x800B0000;                                    // Result = 800B0000
    sw(s0, at - 0x7160);                                // Store to: gMObjHead[4] (800A8EA0)
    v0 = lw(s0 + 0x5C);
    if (i32(v0) <= 0) goto loc_8001D5BC;
    _thunk_P_Random();
    v1 = lw(s0 + 0x5C);
    div(v0, v1);
    if (v1 != 0) goto loc_8001D594;
    _break(0x1C00);
loc_8001D594:
    at = -1;                                            // Result = FFFFFFFF
    {
        const bool bJump = (v1 != at);
        at = 0x80000000;                                // Result = 80000000
        if (bJump) goto loc_8001D5AC;
    }
    if (v0 != at) goto loc_8001D5AC;
    tge(zero, zero, 0x5D);
loc_8001D5AC:
    v1 = hi;
    v1++;
    sw(v1, s0 + 0x5C);
loc_8001D5BC:
    v0 = lw(s0 + 0x64);
    v1 = 0x400000;                                      // Result = 00400000
    v0 &= v1;
    v1 = 0x800000;                                      // Result = 00800000
    if (v0 == 0) goto loc_8001D5E8;
    v0 = *gTotalKills;
    v0++;
    *gTotalKills = v0;
loc_8001D5E8:
    v0 = lw(s0 + 0x64);
    v0 &= v1;
    {
        const bool bJump = (v0 == 0);
        v0 = 0xB60B0000;                                // Result = B60B0000
        if (bJump) goto loc_8001D618;
    }
    v0 = *gTotalItems;
    v0++;
    *gTotalItems = v0;
    v0 = 0xB60B0000;                                    // Result = B60B0000
loc_8001D618:
    v1 = lhu(s2 + 0x4);
    v0 |= 0x60B7;                                       // Result = B60B60B7
    v1 <<= 16;
    a0 = u32(i32(v1) >> 16);
    mult(a0, v0);
    v1 = u32(i32(v1) >> 31);
    v0 = hi;
    v0 += a0;
    v0 = u32(i32(v0) >> 5);
    v0 -= v1;
    v0 <<= 29;
    sw(v0, s0 + 0x24);
    v0 = lhu(s2);
    sh(v0, s0 + 0x88);
    v0 = lhu(s2 + 0x2);
    sh(v0, s0 + 0x8A);
    v0 = lhu(s2 + 0x6);
    sh(v0, s0 + 0x8C);
    v0 = lhu(s2 + 0x4);
    sh(v0, s0 + 0x8E);
    v0 = lhu(s2 + 0x8);
    v0 &= 8;
    if (v0 == 0) goto loc_8001D69C;
    v0 = lw(s0 + 0x64);
    v0 |= 0x20;
    sw(v0, s0 + 0x64);
loc_8001D69C:
    v0 = lhu(s2 + 0x8);
    v1 = lw(s0 + 0x64);
    v0 &= 0xE0;
    v0 <<= 23;
    v0 |= v1;
    v1 = 0x70000000;                                    // Result = 70000000
    sw(v0, s0 + 0x64);
    v0 &= v1;
    v1 = 0x50000000;                                    // Result = 50000000
    if (v0 != v1) goto loc_8001D6D8;
    v0 = lw(s0 + 0x68);
    v0 <<= 1;
    sw(v0, s0 + 0x68);
loc_8001D6D8:
    ra = lw(sp + 0x2C);
    s6 = lw(sp + 0x28);
    s5 = lw(sp + 0x24);
    s4 = lw(sp + 0x20);
    s3 = lw(sp + 0x1C);
    s2 = lw(sp + 0x18);
    s1 = lw(sp + 0x14);
    s0 = lw(sp + 0x10);
    sp += 0x30;
    return;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Spawns a puff particle effect at the given location
//------------------------------------------------------------------------------------------------------------------------------------------
void P_SpawnPuff(const fixed_t x, const fixed_t y, const fixed_t z) noexcept {
    // Spawn the puff and randomly adjust its height
    const fixed_t spawnZ = z + ((P_Random() - P_Random()) << 10);
    mobj_t& mobj = *P_SpawnMObj(x, y, spawnZ, MT_PUFF);

    // Give some upward momentum and randomly adjust tics left
    mobj.momz = FRACUNIT;
    mobj.tics -= P_Random() & 1;

    if (mobj.tics < 1) {
        mobj.tics = 1;
    }
    
    // Don't do sparks if punching the wall
    if (*gAttackRange == MELEERANGE) {
        P_SetMObjState(mobj, S_PUFF3);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Spawns a blood particle effect at the given location
//------------------------------------------------------------------------------------------------------------------------------------------
void P_SpawnBlood(const fixed_t x, const fixed_t y, const fixed_t z, const int32_t damage) noexcept {
    // Spawn the puff and randomly adjust its height
    const fixed_t spawnZ = z + ((P_Random() - P_Random()) << 10);
    mobj_t& mobj = *P_SpawnMObj(x, y, spawnZ, MT_BLOOD);

    // Give some upward momentum and randomly adjust tics left
    mobj.momz = 2 * FRACUNIT;
    mobj.tics -= P_Random() & 1;
    
    if (mobj.tics < 1) {
        mobj.tics = 1;
    }
    
    // Adjust the type of blood, based on the damage amount
    if ((damage >= 9) && (damage <= 12)) {
        P_SetMObjState(mobj, S_BLOOD2);
    } else if (damage < 9) {
        P_SetMObjState(mobj, S_BLOOD3);
    }
}

void P_CheckMissileSpawn() noexcept {
    sp -= 0x18;
    sw(s0, sp + 0x10);
    s0 = a0;
    sw(ra, sp + 0x14);
    v0 = lw(s0 + 0x48);
    v1 = lw(s0);
    a2 = lw(s0 + 0x4);
    v0 = u32(i32(v0) >> 1);
    v0 += v1;
    sw(v0, s0);
    v0 = lw(s0 + 0x4C);
    a1 = lw(s0);
    v1 = lw(s0 + 0x50);
    v0 = u32(i32(v0) >> 1);
    v0 += a2;
    v1 = u32(i32(v1) >> 1);
    sw(v0, s0 + 0x4);
    v0 = lw(s0 + 0x8);
    a2 = lw(s0 + 0x4);
    v1 += v0;
    sw(v1, s0 + 0x8);
    P_TryMove();
    if (v0 != 0) goto loc_8001DC80;
    v1 = lw(s0 + 0x54);
    sw(0, s0 + 0x50);
    sw(0, s0 + 0x4C);
    sw(0, s0 + 0x48);
    v0 = v1 << 1;
    v0 += v1;
    v0 <<= 2;
    v0 -= v1;
    v0 <<= 3;
    at = 0x80060000;                                    // Result = 80060000
    at -= 0x1F94;                                       // Result = MObjInfo_MT_PLAYER[C] (8005E06C)
    at += v0;
    a1 = lw(at);
    a0 = s0;
    v0 = P_SetMObjState(*vmAddrToPtr<mobj_t>(a0), (statenum_t) a1);
    _thunk_P_Random();
    v1 = lw(s0 + 0x5C);
    v0 &= 1;
    v1 -= v0;
    sw(v1, s0 + 0x5C);
    if (i32(v1) > 0) goto loc_8001DC38;
    v0 = 1;                                             // Result = 00000001
    sw(v0, s0 + 0x5C);
loc_8001DC38:
    a0 = 0xFFFE0000;                                    // Result = FFFE0000
    a0 |= 0xFFFF;                                       // Result = FFFEFFFF
    v0 = lw(s0 + 0x64);
    v1 = lw(s0 + 0x58);
    v0 &= a0;
    sw(v0, s0 + 0x64);
    v0 = lw(v1 + 0x38);
    if (v0 == 0) goto loc_8001DC80;
    a0 = lw(s0 + 0x74);
    S_StopSound((sfxenum_t) a0);
    v0 = lw(s0 + 0x58);
    a1 = lw(v0 + 0x38);
    a0 = s0;
    S_StartSound(vmAddrToPtr<mobj_t>(a0), (sfxenum_t) a1);
loc_8001DC80:
    ra = lw(sp + 0x14);
    s0 = lw(sp + 0x10);
    sp += 0x18;
    return;
}

void P_SpawnMissile() noexcept {
loc_8001DC94:
    sp -= 0x30;
    sw(s4, sp + 0x20);
    s4 = a0;
    sw(s6, sp + 0x28);
    s6 = a1;
    sw(s0, sp + 0x10);
    s0 = a2;
    a1 = 0x94;                                          // Result = 00000094
    a2 = 2;                                             // Result = 00000002
    a3 = 0;                                             // Result = 00000000
    a0 = *gpMainMemZone;
    v0 = 0x200000;                                      // Result = 00200000
    sw(ra, sp + 0x2C);
    sw(s5, sp + 0x24);
    sw(s3, sp + 0x1C);
    sw(s2, sp + 0x18);
    sw(s1, sp + 0x14);
    v1 = lw(s4 + 0x8);
    s1 = lw(s4);
    s2 = lw(s4 + 0x4);
    s5 = v1 + v0;
    _thunk_Z_Malloc();
    s3 = v0;
    a0 = s3;
    a1 = 0;                                             // Result = 00000000
    a2 = 0x94;                                          // Result = 00000094
    _thunk_D_memset();
    v0 = s0 << 1;
    v0 += s0;
    v0 <<= 2;
    v0 -= s0;
    v0 <<= 3;
    v1 = 0x80060000;                                    // Result = 80060000
    v1 -= 0x1FC4;                                       // Result = MObjInfo_MT_PLAYER[0] (8005E03C)
    v0 += v1;
    sw(s0, s3 + 0x54);
    sw(v0, s3 + 0x58);
    sw(s1, s3);
    sw(s2, s3 + 0x4);
    v1 = lw(v0 + 0x40);
    sw(v1, s3 + 0x40);
    v1 = lw(v0 + 0x44);
    sw(v1, s3 + 0x44);
    v1 = lw(v0 + 0x54);
    sw(v1, s3 + 0x64);
    v1 = lw(v0 + 0x8);
    sw(v1, s3 + 0x68);
    v1 = lw(v0 + 0x14);
    sw(v1, s3 + 0x78);
    v1 = lw(v0 + 0x4);
    v0 = v1 << 3;
    v0 -= v1;
    v0 <<= 2;
    v1 = 0x80060000;                                    // Result = 80060000
    v1 -= 0x7274;                                       // Result = State_S_NULL[0] (80058D8C)
    v0 += v1;
    sw(v0, s3 + 0x60);
    v1 = lw(v0 + 0x8);
    sw(v1, s3 + 0x5C);
    v1 = lw(v0);
    sw(v1, s3 + 0x28);
    v0 = lw(v0 + 0x4);
    a0 = s3;
    sw(v0, s3 + 0x2C);
    P_SetThingPosition(*vmAddrToPtr<mobj_t>(a0));
    v0 = lw(s3 + 0xC);
    v0 = lw(v0);
    v1 = lw(s3 + 0xC);
    v0 = lw(v0);
    sw(v0, s3 + 0x38);
    v0 = lw(v1);
    v1 = lw(v0 + 0x4);
    v0 = 0x80000000;                                    // Result = 80000000
    sw(v1, s3 + 0x3C);
    if (s5 != v0) goto loc_8001DDFC;
    v0 = lw(s3 + 0x38);
    sw(v0, s3 + 0x8);
    goto loc_8001DE2C;
loc_8001DDFC:
    v0 = 0x7FFF0000;                                    // Result = 7FFF0000
    v0 |= 0xFFFF;                                       // Result = 7FFFFFFF
    if (s5 != v0) goto loc_8001DE28;
    v0 = lw(s3 + 0x58);
    v0 = lw(v0 + 0x44);
    v0 = v1 - v0;
    sw(v0, s3 + 0x8);
    goto loc_8001DE2C;
loc_8001DE28:
    sw(s5, s3 + 0x8);
loc_8001DE2C:
    v0 = 0x800B0000;                                    // Result = 800B0000
    v0 = lw(v0 - 0x7160);                               // Load from: gMObjHead[4] (800A8EA0)
    sw(s3, v0 + 0x14);
    v0 = 0x800B0000;                                    // Result = 800B0000
    v0 -= 0x7170;                                       // Result = gMObjHead[0] (800A8E90)
    sw(v0, s3 + 0x14);
    v0 = 0x800B0000;                                    // Result = 800B0000
    v0 = lw(v0 - 0x7160);                               // Load from: gMObjHead[4] (800A8EA0)
    s1 = s3;
    sw(v0, s3 + 0x10);
    at = 0x800B0000;                                    // Result = 800B0000
    sw(s3, at - 0x7160);                                // Store to: gMObjHead[4] (800A8EA0)
    v0 = lw(s1 + 0x58);
    a1 = lw(v0 + 0x10);
    if (a1 == 0) goto loc_8001DE80;
    a0 = s4;
    S_StartSound(vmAddrToPtr<mobj_t>(a0), (sfxenum_t) a1);
loc_8001DE80:
    sw(s4, s1 + 0x74);
    a0 = lw(s4);
    a1 = lw(s4 + 0x4);
    a2 = lw(s6);
    a3 = lw(s6 + 0x4);
    _thunk_R_PointToAngle2();
    s2 = v0;
    v0 = lw(s6 + 0x64);
    v1 = 0x70000000;                                    // Result = 70000000
    v0 &= v1;
    if (v0 == 0) goto loc_8001DED0;
    _thunk_P_Random();
    s0 = v0;
    _thunk_P_Random();
    s0 -= v0;
    s0 <<= 20;
    s2 += s0;
loc_8001DED0:
    sw(s2, s1 + 0x24);
    s2 >>= 19;
    a0 = s2 << 2;
    v0 = 0x80070000;                                    // Result = 80070000
    v0 = lw(v0 + 0x7BD0);                               // Load from: gpFineCosine (80077BD0)
    v1 = lw(s1 + 0x58);
    v0 += a0;
    v1 = lh(v1 + 0x3E);
    v0 = lw(v0);
    mult(v1, v0);
    v0 = lo;
    sw(v0, s1 + 0x48);
    at = 0x80060000;                                    // Result = 80060000
    at += 0x7958;                                       // Result = FineSine[0] (80067958)
    at += a0;
    v0 = lw(at);
    mult(v1, v0);
    v0 = lo;
    sw(v0, s1 + 0x4C);
    v1 = lw(s6);
    a0 = lw(s4);
    v0 = lw(s6 + 0x4);
    a1 = lw(s4 + 0x4);
    a0 = v1 - a0;
    a1 = v0 - a1;
    v0 = P_AproxDistance(a0, a1);
    v1 = lw(s1 + 0x58);
    v1 = lw(v1 + 0x3C);
    div(v0, v1);
    if (v1 != 0) goto loc_8001DF60;
    _break(0x1C00);
loc_8001DF60:
    at = -1;                                            // Result = FFFFFFFF
    {
        const bool bJump = (v1 != at);
        at = 0x80000000;                                // Result = 80000000
        if (bJump) goto loc_8001DF78;
    }
    if (v0 != at) goto loc_8001DF78;
    tge(zero, zero, 0x5D);
loc_8001DF78:
    v1 = lo;
    a0 = s1;
    if (i32(v1) > 0) goto loc_8001DF8C;
    v1 = 1;                                             // Result = 00000001
loc_8001DF8C:
    a3 = lw(s6 + 0x8);
    v0 = lw(s4 + 0x8);
    a3 -= v0;
    div(a3, v1);
    if (v1 != 0) goto loc_8001DFAC;
    _break(0x1C00);
loc_8001DFAC:
    at = -1;                                            // Result = FFFFFFFF
    {
        const bool bJump = (v1 != at);
        at = 0x80000000;                                // Result = 80000000
        if (bJump) goto loc_8001DFC4;
    }
    if (a3 != at) goto loc_8001DFC4;
    tge(zero, zero, 0x5D);
loc_8001DFC4:
    a3 = lo;
    v0 = lw(s1 + 0x48);
    v1 = lw(s1);
    v0 = u32(i32(v0) >> 1);
    v0 += v1;
    sw(v0, s1);
    a1 = lw(s1);
    v0 = lw(s1 + 0x4C);
    v1 = lw(s1 + 0x4);
    v0 = u32(i32(v0) >> 1);
    v0 += v1;
    sw(v0, s1 + 0x4);
    a2 = lw(s1 + 0x4);
    sw(a3, s1 + 0x50);
    v0 = lw(s1 + 0x50);
    v1 = lw(s1 + 0x8);
    v0 = u32(i32(v0) >> 1);
    v0 += v1;
    sw(v0, s1 + 0x8);
    P_TryMove();
    {
        const bool bJump = (v0 != 0);
        v0 = s1;
        if (bJump) goto loc_8001E0C8;
    }
    v1 = lw(s1 + 0x54);
    sw(0, s1 + 0x50);
    sw(0, s1 + 0x4C);
    sw(0, s1 + 0x48);
    v0 = v1 << 1;
    v0 += v1;
    v0 <<= 2;
    v0 -= v1;
    v0 <<= 3;
    at = 0x80060000;                                    // Result = 80060000
    at -= 0x1F94;                                       // Result = MObjInfo_MT_PLAYER[C] (8005E06C)
    at += v0;
    a1 = lw(at);
    a0 = s1;
    v0 = P_SetMObjState(*vmAddrToPtr<mobj_t>(a0), (statenum_t) a1);
    _thunk_P_Random();
    v1 = lw(s1 + 0x5C);
    v0 &= 1;
    v1 -= v0;
    sw(v1, s1 + 0x5C);
    if (i32(v1) > 0) goto loc_8001E07C;
    v0 = 1;                                             // Result = 00000001
    sw(v0, s1 + 0x5C);
loc_8001E07C:
    a0 = 0xFFFE0000;                                    // Result = FFFE0000
    a0 |= 0xFFFF;                                       // Result = FFFEFFFF
    v0 = lw(s1 + 0x64);
    v1 = lw(s1 + 0x58);
    v0 &= a0;
    sw(v0, s1 + 0x64);
    v0 = lw(v1 + 0x38);
    {
        const bool bJump = (v0 == 0);
        v0 = s1;
        if (bJump) goto loc_8001E0C8;
    }
    a0 = lw(s1 + 0x74);
    S_StopSound((sfxenum_t) a0);
    v0 = lw(s1 + 0x58);
    a1 = lw(v0 + 0x38);
    a0 = s1;
    S_StartSound(vmAddrToPtr<mobj_t>(a0), (sfxenum_t) a1);
    v0 = s1;
loc_8001E0C8:
    ra = lw(sp + 0x2C);
    s6 = lw(sp + 0x28);
    s5 = lw(sp + 0x24);
    s4 = lw(sp + 0x20);
    s3 = lw(sp + 0x1C);
    s2 = lw(sp + 0x18);
    s1 = lw(sp + 0x14);
    s0 = lw(sp + 0x10);
    sp += 0x30;
    return;
}

void P_SpawnPlayerMissile() noexcept {
loc_8001E0F4:
    sp -= 0x38;
    sw(s3, sp + 0x1C);
    s3 = a0;
    sw(s5, sp + 0x24);
    s5 = a1;
    sw(ra, sp + 0x30);
    sw(s7, sp + 0x2C);
    sw(s6, sp + 0x28);
    sw(s4, sp + 0x20);
    sw(s2, sp + 0x18);
    sw(s1, sp + 0x14);
    sw(s0, sp + 0x10);
    s4 = lw(s3 + 0x24);
    a2 = 0x4000000;                                     // Result = 04000000
    a1 = s4;
    P_AimLineAttack();
    v1 = 0x80070000;                                    // Result = 80070000
    v1 = lw(v1 + 0x7EE8);                               // Load from: gpLineTarget (80077EE8)
    s7 = v0;
    if (v1 != 0) goto loc_8001E1A8;
    v0 = 0x4000000;                                     // Result = 04000000
    s4 += v0;
    a0 = s3;
    a1 = s4;
    a2 = 0x4000000;                                     // Result = 04000000
    P_AimLineAttack();
    v1 = 0x80070000;                                    // Result = 80070000
    v1 = lw(v1 + 0x7EE8);                               // Load from: gpLineTarget (80077EE8)
    s7 = v0;
    if (v1 != 0) goto loc_8001E1A8;
    v0 = 0xF8000000;                                    // Result = F8000000
    s4 += v0;
    a0 = s3;
    a1 = s4;
    a2 = 0x4000000;                                     // Result = 04000000
    P_AimLineAttack();
    v1 = 0x80070000;                                    // Result = 80070000
    v1 = lw(v1 + 0x7EE8);                               // Load from: gpLineTarget (80077EE8)
    s7 = v0;
    if (v1 != 0) goto loc_8001E1A8;
    s4 = lw(s3 + 0x24);
    s7 = 0;                                             // Result = 00000000
loc_8001E1A8:
    a1 = 0x94;                                          // Result = 00000094
    a2 = 2;                                             // Result = 00000002
    v0 = 0x200000;                                      // Result = 00200000
    a3 = 0;                                             // Result = 00000000
    a0 = *gpMainMemZone;
    v1 = lw(s3 + 0x8);
    s0 = lw(s3);
    s1 = lw(s3 + 0x4);
    s6 = v1 + v0;
    _thunk_Z_Malloc();
    s2 = v0;
    a0 = s2;
    a1 = 0;                                             // Result = 00000000
    a2 = 0x94;                                          // Result = 00000094
    _thunk_D_memset();
    v0 = s5 << 1;
    v0 += s5;
    v0 <<= 2;
    v0 -= s5;
    v0 <<= 3;
    v1 = 0x80060000;                                    // Result = 80060000
    v1 -= 0x1FC4;                                       // Result = MObjInfo_MT_PLAYER[0] (8005E03C)
    v0 += v1;
    sw(s5, s2 + 0x54);
    sw(v0, s2 + 0x58);
    sw(s0, s2);
    sw(s1, s2 + 0x4);
    v1 = lw(v0 + 0x40);
    sw(v1, s2 + 0x40);
    v1 = lw(v0 + 0x44);
    sw(v1, s2 + 0x44);
    v1 = lw(v0 + 0x54);
    sw(v1, s2 + 0x64);
    v1 = lw(v0 + 0x8);
    sw(v1, s2 + 0x68);
    v1 = lw(v0 + 0x14);
    sw(v1, s2 + 0x78);
    v1 = lw(v0 + 0x4);
    v0 = v1 << 3;
    v0 -= v1;
    v0 <<= 2;
    v1 = 0x80060000;                                    // Result = 80060000
    v1 -= 0x7274;                                       // Result = State_S_NULL[0] (80058D8C)
    v0 += v1;
    sw(v0, s2 + 0x60);
    v1 = lw(v0 + 0x8);
    sw(v1, s2 + 0x5C);
    v1 = lw(v0);
    sw(v1, s2 + 0x28);
    v0 = lw(v0 + 0x4);
    a0 = s2;
    sw(v0, s2 + 0x2C);
    P_SetThingPosition(*vmAddrToPtr<mobj_t>(a0));
    v0 = lw(s2 + 0xC);
    v0 = lw(v0);
    v1 = lw(s2 + 0xC);
    v0 = lw(v0);
    sw(v0, s2 + 0x38);
    v0 = lw(v1);
    v1 = lw(v0 + 0x4);
    v0 = 0x80000000;                                    // Result = 80000000
    sw(v1, s2 + 0x3C);
    if (s6 != v0) goto loc_8001E2E0;
    v0 = lw(s2 + 0x38);
    sw(v0, s2 + 0x8);
    goto loc_8001E310;
loc_8001E2E0:
    v0 = 0x7FFF0000;                                    // Result = 7FFF0000
    v0 |= 0xFFFF;                                       // Result = 7FFFFFFF
    if (s6 != v0) goto loc_8001E30C;
    v0 = lw(s2 + 0x58);
    v0 = lw(v0 + 0x44);
    v0 = v1 - v0;
    sw(v0, s2 + 0x8);
    goto loc_8001E310;
loc_8001E30C:
    sw(s6, s2 + 0x8);
loc_8001E310:
    v0 = 0x800B0000;                                    // Result = 800B0000
    v0 = lw(v0 - 0x7160);                               // Load from: gMObjHead[4] (800A8EA0)
    sw(s2, v0 + 0x14);
    v0 = 0x800B0000;                                    // Result = 800B0000
    v0 -= 0x7170;                                       // Result = gMObjHead[0] (800A8E90)
    sw(v0, s2 + 0x14);
    v0 = 0x800B0000;                                    // Result = 800B0000
    v0 = lw(v0 - 0x7160);                               // Load from: gMObjHead[4] (800A8EA0)
    sw(v0, s2 + 0x10);
    at = 0x800B0000;                                    // Result = 800B0000
    sw(s2, at - 0x7160);                                // Store to: gMObjHead[4] (800A8EA0)
    v0 = lw(s2 + 0x58);
    a1 = lw(v0 + 0x10);
    v1 = s4 >> 19;
    if (a1 == 0) goto loc_8001E368;
    a0 = s3;
    S_StartSound(vmAddrToPtr<mobj_t>(a0), (sfxenum_t) a1);
    v1 = s4 >> 19;
loc_8001E368:
    v0 = 0x80070000;                                    // Result = 80070000
    v0 = lw(v0 + 0x7BD0);                               // Load from: gpFineCosine (80077BD0)
    a0 = lw(s2 + 0x58);
    v1 <<= 2;
    sw(s3, s2 + 0x74);
    sw(s4, s2 + 0x24);
    v0 += v1;
    a0 = lh(a0 + 0x3E);
    v0 = lw(v0);
    mult(a0, v0);
    v0 = lo;
    sw(v0, s2 + 0x48);
    at = 0x80060000;                                    // Result = 80060000
    at += 0x7958;                                       // Result = FineSine[0] (80067958)
    at += v1;
    v0 = lw(at);
    mult(a0, v0);
    v1 = lw(s2);
    v0 = lw(s2 + 0x48);
    a2 = lo;
    v0 = u32(i32(v0) >> 1);
    v0 += v1;
    mult(a0, s7);
    sw(v0, s2);
    a1 = lw(s2);
    v1 = lw(s2 + 0x4);
    sw(a2, s2 + 0x4C);
    v0 = lw(s2 + 0x4C);
    a0 = s2;
    v0 = u32(i32(v0) >> 1);
    v0 += v1;
    sw(v0, s2 + 0x4);
    a2 = lw(s2 + 0x4);
    v0 = lo;
    sw(v0, s2 + 0x50);
    v0 = lw(s2 + 0x50);
    v1 = lw(s2 + 0x8);
    v0 = u32(i32(v0) >> 1);
    v0 += v1;
    sw(v0, s2 + 0x8);
    P_TryMove();
    if (v0 != 0) goto loc_8001E4C4;
    v1 = lw(s2 + 0x54);
    sw(0, s2 + 0x50);
    sw(0, s2 + 0x4C);
    sw(0, s2 + 0x48);
    v0 = v1 << 1;
    v0 += v1;
    v0 <<= 2;
    v0 -= v1;
    v0 <<= 3;
    at = 0x80060000;                                    // Result = 80060000
    at -= 0x1F94;                                       // Result = MObjInfo_MT_PLAYER[C] (8005E06C)
    at += v0;
    a1 = lw(at);
    a0 = s2;
    v0 = P_SetMObjState(*vmAddrToPtr<mobj_t>(a0), (statenum_t) a1);
    _thunk_P_Random();
    v1 = lw(s2 + 0x5C);
    v0 &= 1;
    v1 -= v0;
    sw(v1, s2 + 0x5C);
    if (i32(v1) > 0) goto loc_8001E47C;
    v0 = 1;                                             // Result = 00000001
    sw(v0, s2 + 0x5C);
loc_8001E47C:
    a0 = 0xFFFE0000;                                    // Result = FFFE0000
    a0 |= 0xFFFF;                                       // Result = FFFEFFFF
    v0 = lw(s2 + 0x64);
    v1 = lw(s2 + 0x58);
    v0 &= a0;
    sw(v0, s2 + 0x64);
    v0 = lw(v1 + 0x38);
    if (v0 == 0) goto loc_8001E4C4;
    a0 = lw(s2 + 0x74);
    S_StopSound((sfxenum_t) a0);
    v0 = lw(s2 + 0x58);
    a1 = lw(v0 + 0x38);
    a0 = s2;
    S_StartSound(vmAddrToPtr<mobj_t>(a0), (sfxenum_t) a1);
loc_8001E4C4:
    ra = lw(sp + 0x30);
    s7 = lw(sp + 0x2C);
    s6 = lw(sp + 0x28);
    s5 = lw(sp + 0x24);
    s4 = lw(sp + 0x20);
    s3 = lw(sp + 0x1C);
    s2 = lw(sp + 0x18);
    s1 = lw(sp + 0x14);
    s0 = lw(sp + 0x10);
    sp += 0x38;
    return;
}

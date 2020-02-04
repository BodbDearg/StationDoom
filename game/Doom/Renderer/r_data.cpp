#include "r_data.h"

#include "Doom/Base/i_main.h"
#include "Doom/Base/w_wad.h"
#include "Doom/Base/z_zone.h"
#include "Doom/Game/doomdata.h"
#include "PcPsx/Endian.h"
#include "PsxVm/PsxVm.h"
#include "PsyQ/LIBGPU.h"

// Details about all of the textures in the game and the sky texture
const VmPtr<VmPtr<texture_t>>   gpTextures(0x80078128);
const VmPtr<VmPtr<texture_t>>   gpFlatTextures(0x80078124);
const VmPtr<VmPtr<texture_t>>   gpSpriteTextures(0x80077EC4);
const VmPtr<VmPtr<texture_t>>   gpSkyTexture(0x80078050);

// Texture translation: converts from an input texture index to the actual texture index to render for that input index.
// Used to implement animated textures whereby the translation is simply updated as the texture animates.
const VmPtr<VmPtr<int32_t>>     gpTextureTranslation(0x80077F6C);

// Flat translation: similar function to texture translation except for flats rather than wall textures.
const VmPtr<VmPtr<int32_t>>     gpFlatTranslation(0x80077F60);

// The loaded lights lump
const VmPtr<VmPtr<light_t>>     gpLightsLump(0x80078068);

// Palette stuff
const VmPtr<uint16_t>   gPaletteClutId_Main(0x800A9084);        // The regular in-game palette
const VmPtr<uint16_t>   g3dViewPaletteClutId(0x80077F7C);       // Currently active in-game palette. Changes as effects are applied in the game.

// Lump number ranges
const VmPtr<int32_t>    gFirstTexLumpNum(0x800782E0);
const VmPtr<int32_t>    gLastTexLumpNum(0x8007819C);
const VmPtr<int32_t>    gNumTexLumps(0x800781D4);
const VmPtr<int32_t>    gFirstFlatLumpNum(0x800782B8);
const VmPtr<int32_t>    gLastFlatLumpNum(0x80078170);
const VmPtr<int32_t>    gNumFlatLumps(0x800781C0);
const VmPtr<int32_t>    gFirstSpriteLumpNum(0x80078014);
const VmPtr<int32_t>    gLastSpriteLumpNum(0x80077F38);
const VmPtr<int32_t>    gNumSpriteLumps(0x80077F5C);

//------------------------------------------------------------------------------------------------------------------------------------------
// Initialize the palette and asset management for various draw assets
//------------------------------------------------------------------------------------------------------------------------------------------
void R_InitData() noexcept {
    R_InitPalette();
    R_InitTextures();
    R_InitFlats();
    R_InitSprites();
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Initialize the global wall textures list and load texture size metadata.
// Also initialize the texture translation table for animated wall textures.
//------------------------------------------------------------------------------------------------------------------------------------------
void R_InitTextures() noexcept {
    // Determine basic texture list stats
    *gFirstTexLumpNum = W_GetNumForName("T_START") + 1;
    *gLastTexLumpNum = W_GetNumForName("T_END") - 1;
    *gNumTexLumps = *gLastTexLumpNum - *gFirstTexLumpNum + 1;

    // Alloc the list of textures and texture translation table
    {
        std::byte* const pAlloc = (std::byte*) Z_Malloc(
            **gpMainMemZone,
            (*gNumTexLumps) * (sizeof(texture_t) + sizeof(int32_t)),
            PU_STATIC,
            nullptr
        );

        *gpTextures = (texture_t*) pAlloc;
        *gpTextureTranslation = (int32_t*)(pAlloc + (*gNumTexLumps) * sizeof(texture_t));
    }

    // Load the texture metadata lump and process each associated texture entry with the loaded size info
    {
        maptexture_t* const pMapTextures = (maptexture_t*) W_CacheLumpName("TEXTURE1", PU_CACHE, true);
        maptexture_t* pMapTex = pMapTextures;

        texture_t* pTex = gpTextures->get();
    
        for (int32_t lumpNum = *gFirstTexLumpNum; lumpNum < *gLastTexLumpNum; ++lumpNum, ++pMapTex, ++pTex) {
            pTex->lumpNum = (uint16_t) lumpNum;
            pTex->texPageId = 0;            
            pTex->width = Endian::littleToHost(pMapTex->width);
            pTex->height = Endian::littleToHost(pMapTex->height);

            if (pTex->width + 15 >= 0) {
                pTex->width16 = (pTex->width + 15) / 16;
            } else {
                pTex->width16 = (pTex->width + 30) / 16;    // This case is never hit. Not sure why texture sizes would be negative? What does that mean?
            }
            
            if (pTex->height + 15 >= 0) {
                pTex->height16 = (pTex->height + 15) / 16;
            } else {
                pTex->height16 = (pTex->height + 30) / 16;  // This case is never hit. Not sure why texture sizes would be negative? What does that mean?
            }
        }

        // Cleanup this after we are done
        Z_Free2(**gpMainMemZone, pMapTextures);
    }
    
    // Init the texture translation table: initially all textures translate to themselves
    int32_t* const pTexTranslation = gpTextureTranslation->get();

    for (int32_t texIdx = 0; texIdx < *gNumTexLumps; ++texIdx) {
        pTexTranslation[texIdx] = texIdx;
    }
    
    // Clear out any blocks marked as 'cache' which can be evicted.
    // This call is here probably to try and avoid spikes in memory load.
    Z_FreeTags(**gpMainMemZone, PU_CACHE);
}

void R_InitFlats() noexcept {
loc_8002BB50:
    sp -= 0x20;
    a0 = 0x80070000;                                    // Result = 80070000
    a0 += 0x7B7C;                                       // Result = STR_LumpName_F_START[0] (80077B7C)
    sw(ra, sp + 0x18);
    v0 = W_GetNumForName(vmAddrToPtr<const char>(a0));
    a0 = 0x80070000;                                    // Result = 80070000
    a0 += 0x7B84;                                       // Result = STR_LumpName_F_END[0] (80077B84)
    v0++;
    *gFirstFlatLumpNum = v0;
    v0 = W_GetNumForName(vmAddrToPtr<const char>(a0));
    v0--;
    a2 = 1;
    a0 = *gpMainMemZone;
    v1 = *gFirstFlatLumpNum;
    a3 = 0;
    *gLastFlatLumpNum = v0;
    v0 -= v1;
    v0++;
    a1 = v0 << 5;
    *gNumFlatLumps = v0;
    v0 <<= 2;
    a1 += v0;
    _thunk_Z_Malloc();
    a0 = *gFirstFlatLumpNum;
    v1 = *gNumFlatLumps;
    a1 = *gLastFlatLumpNum;
    *gpFlatTextures = v0;
    v1 <<= 5;
    v1 += v0;
    *gpFlatTranslation = v1;
    v1 = v0;
    v0 = (i32(a1) < i32(a0));
    if (v0 != 0) goto loc_8002BC18;
    a3 = 0x40;                                          // Result = 00000040
    a2 = 4;                                             // Result = 00000004
    v1 += 0x10;
loc_8002BBF0:
    sh(a0, v1);
    a0++;
    sh(a3, v1 - 0xC);
    sh(a3, v1 - 0xA);
    sh(0, v1 - 0x6);
    sh(a2, v1 - 0x4);
    sh(a2, v1 - 0x2);
    v0 = (i32(a1) < i32(a0));
    v1 += 0x20;
    if (v0 == 0) goto loc_8002BBF0;
loc_8002BC18:
    a1 = *gNumFlatLumps;
    a0 = 0;
    if (i32(a1) <= 0) goto loc_8002BC44;
    v1 = *gpFlatTranslation;
loc_8002BC2C:
    sw(a0, v1);
    a0++;
    v0 = (i32(a0) < i32(a1));
    v1 += 4;
    if (v0 != 0) goto loc_8002BC2C;
loc_8002BC44:
    ra = lw(sp + 0x18);
    sp += 0x20;
    return;
}

void R_InitSprites() noexcept {
loc_8002BC54:
    sp -= 0x18;
    a0 = 0x80070000;                                    // Result = 80070000
    a0 += 0x7B8C;                                       // Result = STR_LumpName_S_START[0] (80077B8C)
    sw(ra, sp + 0x10);
    v0 = W_GetNumForName(vmAddrToPtr<const char>(a0));
    a0 = 0x80070000;                                    // Result = 80070000
    a0 += 0x7B94;                                       // Result = STR_LumpName_S_END[0] (80077B94)
    v0++;
    *gFirstSpriteLumpNum = v0;
    v0 = W_GetNumForName(vmAddrToPtr<const char>(a0));
    a2 = 1;
    v0--;
    a0 = *gpMainMemZone;
    v1 = *gFirstSpriteLumpNum;
    a3 = 0;
    *gLastSpriteLumpNum = v0;
    v0 -= v1;
    v0++;
    *gNumSpriteLumps = v0;
    a1 = v0 << 5;
    _thunk_Z_Malloc();
    a0 = 0x80070000;                                    // Result = 80070000
    a0 += 0x7B9C;                                       // Result = STR_LumpName_SPRITE1[0] (80077B9C)
    a1 = 0x20;                                          // Result = 00000020
    *gpSpriteTextures = v0;
    a2 = 1;                                             // Result = 00000001
    _thunk_W_CacheLumpName();
    a1 = v0;
    t0 = *gFirstSpriteLumpNum;
    v1 = *gLastSpriteLumpNum;
    t1 = *gpSpriteTextures;
    v0 = (i32(v1) < i32(t0));
    t2 = a1;
    if (v0 != 0) goto loc_8002BD74;
    t3 = v1;
    a3 = a1 + 6;
    a0 = t1 + 0x10;
loc_8002BCF4:
    v0 = lhu(t2);
    sh(v0, t1);
    v0 = lhu(a3 - 0x4);
    sh(v0, a0 - 0xE);
    v0 = lhu(a3 - 0x2);
    sh(v0, a0 - 0xC);
    v0 = lhu(a3);
    a2 = lh(a0 - 0xC);
    sh(0, a0 - 0x6);
    v1 = a2 + 0xF;
    sh(v0, a0 - 0xA);
    if (i32(v1) >= 0) goto loc_8002BD34;
    v1 = a2 + 0x1E;
loc_8002BD34:
    a2 = lh(a0 - 0xA);
    v0 = u32(i32(v1) >> 4);
    sh(v0, a0 - 0x4);
    v0 = a2 + 0xF;
    sh(t0, a0);
    if (i32(v0) >= 0) goto loc_8002BD50;
    v0 = a2 + 0x1E;
loc_8002BD50:
    t0++;
    v0 = u32(i32(v0) >> 4);
    sh(v0, a0 - 0x2);
    a0 += 0x20;
    t1 += 0x20;
    a3 += 8;
    v0 = (i32(t3) < i32(t0));
    t2 += 8;
    if (v0 == 0) goto loc_8002BCF4;
loc_8002BD74:
    a0 = *gpMainMemZone;
    _thunk_Z_Free2();
    
    Z_FreeTags(**gpMainMemZone, PU_CACHE);

    ra = lw(sp + 0x10);
    sp += 0x18;
    return;
}

void R_TextureNumForName() noexcept {
loc_8002BDA4:
    sp -= 0x10;
    a1 = sp;
    a2 = sp + 8;
    sw(0, sp);
    sw(0, sp + 0x4);
loc_8002BDB8:
    v0 = lbu(a0);
    v1 = v0;
    if (v0 == 0) goto loc_8002BDF0;
    v0 = v1 - 0x61;
    v0 = (v0 < 0x1A);
    a0++;
    if (v0 == 0) goto loc_8002BDDC;
    v1 -= 0x20;
loc_8002BDDC:
    sb(v1, a1);
    a1++;
    v0 = (i32(a1) < i32(a2));
    if (v0 != 0) goto loc_8002BDB8;
loc_8002BDF0:
    v0 = *gFirstTexLumpNum;
    v1 = 0x80080000;                                    // Result = 80080000
    v1 = lw(v1 - 0x7E3C);                               // Load from: gpLumpInfo (800781C4)
    a3 = lw(sp);
    v0 <<= 4;
    v0 += v1;
    v1 = *gNumTexLumps;
    a2 = lw(sp + 0x4);
    a0 = 0;
    if (i32(v1) <= 0) goto loc_8002BE58;
    t0 = -0x81;                                         // Result = FFFFFF7F
    a1 = v1;
    v1 = v0 + 8;
loc_8002BE24:
    v0 = lw(v1 + 0x4);
    if (v0 != a2) goto loc_8002BE48;
    v0 = lw(v1);
    v0 &= t0;
    {
        const bool bJump = (v0 == a3);
        v0 = a0;
        if (bJump) goto loc_8002BE5C;
    }
loc_8002BE48:
    a0++;
    v0 = (i32(a0) < i32(a1));
    v1 += 0x10;
    if (v0 != 0) goto loc_8002BE24;
loc_8002BE58:
    v0 = -1;                                            // Result = FFFFFFFF
loc_8002BE5C:
    sp += 0x10;
    return;
}

void R_FlatNumForName() noexcept {
loc_8002BE68:
    sp -= 0x10;
    a1 = sp;
    a2 = sp + 8;
    sw(0, sp);
    sw(0, sp + 0x4);
loc_8002BE7C:
    v0 = lbu(a0);
    v1 = v0;
    if (v0 == 0) goto loc_8002BEB4;
    v0 = v1 - 0x61;
    v0 = (v0 < 0x1A);
    a0++;
    if (v0 == 0) goto loc_8002BEA0;
    v1 -= 0x20;
loc_8002BEA0:
    sb(v1, a1);
    a1++;
    v0 = (i32(a1) < i32(a2));
    if (v0 != 0) goto loc_8002BE7C;
loc_8002BEB4:
    v0 = *gFirstFlatLumpNum;
    v1 = 0x80080000;                                    // Result = 80080000
    v1 = lw(v1 - 0x7E3C);                               // Load from: gpLumpInfo (800781C4)
    a3 = lw(sp);
    v0 <<= 4;
    v0 += v1;
    v1 = *gNumFlatLumps;
    a2 = lw(sp + 0x4);
    a0 = 0;
    if (i32(v1) <= 0) goto loc_8002BF1C;
    t0 = -0x81;                                         // Result = FFFFFF7F
    a1 = v1;
    v1 = v0 + 8;
loc_8002BEE8:
    v0 = lw(v1 + 0x4);
    if (v0 != a2) goto loc_8002BF0C;
    v0 = lw(v1);
    v0 &= t0;
    {
        const bool bJump = (v0 == a3);
        v0 = a0;
        if (bJump) goto loc_8002BF20;
    }
loc_8002BF0C:
    a0++;
    v0 = (i32(a0) < i32(a1));
    v1 += 0x10;
    if (v0 != 0) goto loc_8002BEE8;
loc_8002BF1C:
    v0 = 0;                                             // Result = 00000000
loc_8002BF20:
    sp += 0x10;
    return;
}

void R_InitPalette() noexcept {
loc_8002BF2C:
    sp -= 0x28;
    a0 = 0x80070000;                                    // Result = 80070000
    a0 += 0x7BA4;                                       // Result = STR_LumpName_LIGHTS[0] (80077BA4)
    a1 = 1;                                             // Result = 00000001
    a2 = 1;                                             // Result = 00000001
    sw(ra, sp + 0x24);
    sw(s2, sp + 0x20);
    sw(s1, sp + 0x1C);
    sw(s0, sp + 0x18);
    _thunk_W_CacheLumpName();
    v1 = 0xFF;                                          // Result = 000000FF
    *gpLightsLump = v0;
    sb(v1, v0);
    v0 = *gpLightsLump;
    sb(v1, v0 + 0x1);
    v0 = *gpLightsLump;
    a0 = 0x80070000;                                    // Result = 80070000
    a0 += 0x7BAC;                                       // Result = STR_LumpName_PLAYPAL[0] (80077BAC)
    sb(v1, v0 + 0x2);
    v0 = W_GetNumForName(vmAddrToPtr<const char>(a0));
    s0 = v0;
    a0 = s0;
    a1 = 0x20;                                          // Result = 00000020
    a2 = 1;                                             // Result = 00000001
    _thunk_W_CacheLumpNum();
    s1 = v0;
    v0 = W_LumpLength((int32_t) s0);
    v0 >>= 9;
    v1 = 0x14;
    s0 = 0;
    if (v0 == v1) goto loc_8002BFCC;
    I_Error("R_InitPalettes: palette foulup\n");
loc_8002BFCC:
    s2 = gPaletteClutId_Main;
    v0 = 0x100;                                         // Result = 00000100
    sh(v0, sp + 0x14);
    v0 = 1;                                             // Result = 00000001
    sh(v0, sp + 0x16);
loc_8002BFE4:
    v0 = s0;
    if (i32(s0) >= 0) goto loc_8002BFF0;
    v0 = s0 + 0xF;
loc_8002BFF0:
    a0 = sp + 0x10;
    a1 = s1;
    v0 = u32(i32(v0) >> 4);
    v1 = v0 << 8;
    v0 <<= 4;
    v0 = s0 - v0;
    v0 += 0xF0;
    sh(v1, sp + 0x10);
    sh(v0, sp + 0x12);
    _thunk_LIBGPU_LoadImage();
    s1 += 0x200;
    a0 = lh(sp + 0x10);
    a1 = lh(sp + 0x12);
    s0++;
    LIBGPU_GetClut();
    sh(v0, s2);
    v0 = (i32(s0) < 0x14);
    s2 += 2;
    if (v0 != 0) goto loc_8002BFE4;
    *g3dViewPaletteClutId = *gPaletteClutId_Main;
    
    Z_FreeTags(**gpMainMemZone, PU_CACHE);

    ra = lw(sp + 0x24);
    s2 = lw(sp + 0x20);
    s1 = lw(sp + 0x1C);
    s0 = lw(sp + 0x18);
    sp += 0x28;
    return;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// PC-PSX addition: helper that consolidates some commonly used graphics logic in one function.
//
// Given the specified texture object which is assumed to be 8-bits per pixel (i.e color indexed), returns the 'RECT' in VRAM where the
// texture would be placed. Useful for getting the texture's VRAM position prior to uploading to the GPU.
// 
// I decided to make this a helper rather than duplicate the same code everywhere.
// Produces the same result as the original inline logic, but cleaner.
//------------------------------------------------------------------------------------------------------------------------------------------
RECT getTextureVramRect(const texture_t& tex) noexcept {
    const int16_t texPageCoordX = (int16_t)(uint16_t) tex.texPageCoordX;    // N.B: make sure to zero extend!
    const int16_t texPageCoordY = (int16_t)(uint16_t) tex.texPageCoordY;
    const int16_t texPageId = (int16_t) tex.texPageId;
    
    // Note: all x dimension coordinates get divided by 2 because 'RECT' coords are for 16-bit mode.
    // This function assumes all textures are 8 bits per pixel.
    RECT rect;
    rect.x = texPageCoordX / 2 + (texPageId & 0xF) * 64;
    rect.y = (texPageId & 0x10) * 16 + texPageCoordY;
    rect.w = tex.width / 2;
    rect.h = tex.height;

    return rect;
}

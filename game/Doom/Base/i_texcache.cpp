//------------------------------------------------------------------------------------------------------------------------------------------
// A module implementing all VRAM memory management and texture caching (uploading to VRAM) functionality.
// 
// This code was originally in 'i_main.cpp' but it has since been extensively modified and changed to support VRAM sizes of up to 8192x8192
// with 1020 individual usable texture 'pages', among other features. As a result the new code now lives separately in this module.
// For the original version of this code, see the 'Old' code folder.
// 
// For reference, VRAM for PSX Doom was originally managed as follows:
//
//  (1) There is 1 MiB in total of VRAM split into 16 texture 'pages'.
//  (2) Each texture page is 256x256 when interpreted as 8 bits per pixel data and 64 KiB in size.
//  (3) The first 4 texture pages are reserved for the front and back framebuffer.
//      Each framebuffer is 256x256 pixels (note: some vertical space is unused) but since the data is
//      16 bits per pixel rather than 8 bits, each screen occupies 2 texture pages rather than 1.
//  (4) Following that, 11 pages are free to be used in any way for map textures and sprites.
//      This is what is referred to as the 'texture cache'.
//  (5) 1 texture page at the end appears to be reserved/unused during gameplay.
//      Perhaps it was held off limits in case memory got tight and a quick fix was needed, or kept reserved for debug stuff?
//  (6) Each of the 11 256x256 pages are broken up into a grid of 16x16 cells, where each cell is 16 pixels wide and tall.
//      Each cell stores a pointer to the 'texture_t' occupying the cell, if any.
//  (7) When adding a sprite or texture to the cache, the code will search through the cells for a free space
//      large enough to accommodate the given resource size, rounded up to 16 pixel increments. The search starts after
//      where the last texture was placed and proceeds across and downwards (in that order) to fill the current texture page.
//      If the current texture page has been filled then the algorithm moves around to the next page, potentially wrapping
//      around back to the first.
//  (8) If a request to add a texture to the cache comes across occupied VRAM cells with textures uploaded in the current frame,
//      then a 'Texture Cache Overflow' fatal error is declared. This means that the algorithm has looped around once already and
//      filled the cache entirely in the same frame. Uploading any more textures would overwrite textures needed for current drawing
//      so that this point the renderer gives up and dies. Perhaps a more robust fix for this problem might have been to simply ignore
//      the request, or flush all pending drawing ops and clear the cache - but perhaps those solutions were deemed unacceptable...
//      PsyDoom: texture cache overflows are now a warning instead of a fatal error.
//  (9) One other small detail also is that certain texture pages can be marked as 'locked'.
//      These 'locked' pages are left alone when clearing or adding textures to the cache.
//      Map floor and wall textures are placed in 'locked' pages so that they are never unloaded during level gameplay.
//  (10) Lastly it is worth mentioning that since textures in unlocked pages can be evicted at any time, these textures must also be
//       backed up and retained in main RAM. So essentially the renderer needs to keep a copy of all sprite data in main RAM also.
//
// This manager largely works in the same way except the number of pages is dynamic depending on the size of VRAM and there can be up to
// 1020 usable texture pages. The unused page mentioned above is also reclaimed by this new manager.
// The manager is also allowed to check more cells on wraparound before giving up and concluding that the cache is full.
//------------------------------------------------------------------------------------------------------------------------------------------
#include "i_texcache.h"

#include "Asserts.h"
#include "Doom/Game/doomdata.h"
#include "Doom/Game/p_setup.h"
#include "Doom/Renderer/r_data.h"
#include "Doom/UI/st_main.h"
#include "Gpu.h"
#include "i_drawcmds.h"
#include "i_main.h"
#include "PcPsx/PsxVm.h"
#include "PsyQ/LIBGPU.h"
#include "w_wad.h"
#include "z_zone.h"

#include <algorithm>
#include <vector>

#if PSYDOOM_MODS

// Simple pointer to texture data and its size
struct texdata_t {
    const std::byte*    pBytes;
    const size_t        size;
};

// Holds info for a single page in the texture cache.
// Describes the pixel location in VRAM where the page is located (in 16-bit pixel coords) and the occupying textures for each cell.
// Also has a boolean variable indicating whether the page is 'locked' for modification.
struct tcachepage_t {
    bool        bIsLocked;
    uint16_t    vramX;
    uint16_t    vramY;
    texture_t*  cells[TCACHE_CELLS_Y][TCACHE_CELLS_X];
};

// All of the texture pages available to use.
// Pages reserved for the framebuffer are NOT included in this list.
static std::vector<tcachepage_t> gTCachePages;

// Texture cache fill variables.
// Describes where we are filling in the cache next and the current maximum height of the row being filled (in cells).
static uint32_t gTCacheFillPage;
static uint32_t gTCacheFillCellX;
static uint32_t gTCacheFillCellY;
static uint32_t gTCacheFillRowCellH;

//------------------------------------------------------------------------------------------------------------------------------------------
// Checks to see if the texture can be placed on the given page at the current fill location.
// Returns 'false' if this action is not possible due to other textures that are currently occupying the cells.
// Also evicts any textures encountered that can be evicted (in the proposed fill area) which are not for the current frame.
//------------------------------------------------------------------------------------------------------------------------------------------
static bool TC_PrepFillLocationCells(tcachepage_t& texPage, const texture_t& tex) noexcept {
    // Get the range of cells to be checked
    const uint16_t texW16 = tex.width16;
    const uint16_t texH16 = tex.height16;
    const uint16_t xBeg = (uint16_t) gTCacheFillCellX;
    const uint16_t yBeg = (uint16_t) gTCacheFillCellY;
    const uint16_t xEnd = xBeg + texW16;
    const uint16_t yEnd = yBeg + texH16;

    ASSERT(xEnd <= TCACHE_CELLS_X);
    ASSERT(yEnd <= TCACHE_CELLS_Y);

    // Check all of the cells to see if there is anything blocking placing this texture here
    for (uint16_t y = yBeg; y < yEnd; ++y) {
        for (uint16_t x = xBeg; x < xEnd; ++x) {
            // If the cell is empty then all is good, skip over
            texture_t* const pTex = texPage.cells[y][x];

            if (!pTex)
                continue;

            // Cell is not empty! If the texture in the cell is in use for this frame then we can't evict it.
            // In this case skip past the texture and adjust/expand the current fill row height accordingly:
            if (pTex->uploadFrameNum == gNumFramesDrawn) {
                gTCacheFillCellX += pTex->width16;
                gTCacheFillRowCellH = std::max<uint32_t>(gTCacheFillRowCellH, pTex->height16);
                return false;
            }

            // The cell is not empty but we can evict the texture, do that now:
            I_RemoveTexCacheEntry(*pTex);
        }
    }

    // If we get to here then the area where the texture would be placed is OK
    return true;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Tries to move to a valid fill location in the current texture page for the specified texture.
// Returns 'false' on failure to find a valid fill location.
// May evict stale textures (not for the current frame) along the way.
//------------------------------------------------------------------------------------------------------------------------------------------
static bool TC_MoveToPageFillLocation(const texture_t& tex) noexcept {
    // If the texture page is locked then no location on the page is valid
    tcachepage_t& texPage = gTCachePages[gTCacheFillPage];

    if (texPage.bIsLocked)
        return false;

    // Continue trying to find a valid location on the page to place the texture at until we reach the end
    const uint16_t texW16 = tex.width16;
    const uint16_t texH16 = tex.height16;

    while (true) {
        // Is there enough room on this row for the texture? Move onto the next row if there is not:
        ASSERT(texW16 <= TCACHE_CELLS_X);
        ASSERT(texH16 <= TCACHE_CELLS_Y);

        if (gTCacheFillCellX + texW16 > TCACHE_CELLS_X) {
            gTCacheFillCellX = 0;
            gTCacheFillCellY += gTCacheFillRowCellH;
            gTCacheFillRowCellH = 0;
        }

        // Have we reached the end of the page? Abort the search for a valid fill location if so:
        if (gTCacheFillCellY + texH16 > TCACHE_CELLS_Y)
            break;

        // Check to see if the cells occupied are OK and evict any stale textures.
        // If this succeeds then we are done, otherwise we loop around and try again until we reach the end of the page.
        if (TC_PrepFillLocationCells(texPage, tex))
            return true;
    }

    // Failed to find a valid fill location on this page if we reached here
    return false;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Moves to a fill location in the texture cache where the specified texture can be placed.
// Returns 'false' on failure to find such a location and issues a warning.
// May evict stale textures (not for the current frame) along the way.
//------------------------------------------------------------------------------------------------------------------------------------------
static bool TC_MoveToFillLocation(const texture_t& tex) noexcept {
    // Sanity checks
    ASSERT(gTCachePages.size() > 0);

    // Try to place the current texture on every single page, including the current one rewound to the very beginning.
    // If all that fails then simply give up.
    const uint32_t numTCachePages = (uint32_t) gTCachePages.size();

    for (uint32_t numAttempts = numTCachePages + 1; numAttempts != 0; --numAttempts) {
        // Try move to a valid fill location in the current page, otherwise try the next one
        if (TC_MoveToPageFillLocation(tex))
            return true;

        I_SetTexCacheFillPage((gTCacheFillPage + 1) % numTCachePages);
    }

    // If here is reached then the operation failed and the texture cache overflowed.
    // In this circumstance issue a warning and accept any graphical glitches that follow.
    // The original game crashed with a hard error, this way at least allows the player to continue and recover.
    gStatusBar.message = "W: Texture Cache Overflow!";
    gStatusBar.messageTicsLeft = 60;
    return false;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Frees a rectangular area of texture cache cells in the given texture page.
// Assumes all coordinates are in bounds.
//------------------------------------------------------------------------------------------------------------------------------------------
static void TC_FreePageCells(const tcachepage_t& texPage, const uint32_t x, const uint32_t y, const uint32_t w, const uint32_t h) noexcept {
    ASSERT(x + w <= TCACHE_CELLS_X);
    ASSERT(y + h <= TCACHE_CELLS_Y);

    const uint32_t xEnd = x + w;
    const uint32_t yEnd = y + h;

    for (uint32_t yCur = y; yCur < yEnd; ++yCur) {
        for (uint32_t xCur = x; xCur < xEnd; ++xCur) {
            texture_t* const pTex = texPage.cells[yCur][xCur];

            if (pTex) {
                I_RemoveTexCacheEntry(*pTex);
            }
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Frees all cells in the given texture page
//------------------------------------------------------------------------------------------------------------------------------------------
static void TC_FreePageCells(const tcachepage_t& texPage) noexcept {
    TC_FreePageCells(texPage, 0, 0, TCACHE_CELLS_X, TCACHE_CELLS_Y);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Fills a rectangular area of texture cache cells in the given texture page with references to the specified texture.
// Also sets the location in the texture cache where the specified texture is found (top left coord).
// Assumes all coordinates are in bounds and that the affected cells are already freed.
//------------------------------------------------------------------------------------------------------------------------------------------
static void TC_FillCacheCells(tcachepage_t& texPage, texture_t& tex) noexcept {
    const uint32_t xBeg = gTCacheFillCellX;
    const uint32_t yBeg = gTCacheFillCellY;
    const uint32_t xEnd = xBeg + tex.width16;
    const uint32_t yEnd = yBeg + tex.height16;

    ASSERT(xEnd <= TCACHE_CELLS_X);
    ASSERT(yEnd <= TCACHE_CELLS_Y);

    for (uint32_t yCur = yBeg; yCur < yEnd; ++yCur) {
        for (uint32_t xCur = xBeg; xCur < xEnd; ++xCur) {
            ASSERT(!texPage.cells[yCur][xCur]);
            texPage.cells[yCur][xCur] = &tex;
        }
    }

    tex.ppTexCacheEntries = &texPage.cells[yBeg][xBeg];
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Caches and decompresses the data for the specified texture and returns the pointer to the texture bytes and its size
//------------------------------------------------------------------------------------------------------------------------------------------
static texdata_t TC_CacheTexData(const texture_t& tex) {
    // Make sure the texture's lump is loaded and get the bytes
    const std::byte* pTexBytes = (const std::byte*) W_CacheLumpNum(tex.lumpNum, PU_CACHE, false);
    ASSERT(pTexBytes);

    // Do we need to decompress the texture first?
    const bool bIsTexCompressed = (!gpbIsUncompressedLump[tex.lumpNum]);

    if (bIsTexCompressed) {
        // Compressed texture, must decompress to the temporary buffer first
        const uint32_t texSize = getDecodedSize(pTexBytes);

        #if PSYDOOM_LIMIT_REMOVING
            gTmpBuffer.ensureSize(texSize);
            decode(pTexBytes, gTmpBuffer.bytes());
            pTexBytes = gTmpBuffer.bytes();
        #else
            // PsyDoom: check for buffer overflows and issue an error if we exceed the limits
            #if PSYDOOM_MODS
                if (texSize > TMP_BUFFER_SIZE) {
                    I_Error("I_CacheTex: lump %d size > 64 KiB!", tex.lumpNum);
                }
            #endif

            decode(pTexBytes, gTmpBuffer);
            pTexBytes = gTmpBuffer;
        #endif

        return { pTexBytes, texSize };
    } else {
        // Uncompressed texture, can just return the bytes as-is
        const uint32_t texSize = W_LumpLength(tex.lumpNum);
        return { pTexBytes, texSize };
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Uploads the texture to VRAM and saves the details of where it is located.
// The texture is uploaded to the specified page at the current fill location.
//------------------------------------------------------------------------------------------------------------------------------------------
static void TC_UploadTexToVram(texture_t& tex, const texlump_header_t& texInfo, const texdata_t& texData, const tcachepage_t& texPage) {
    // How big is the texture data expected to be from the dimensions?
    // If the data stream is too small then do not upload and issue a warning.
    // 
    // Note: we can only upload 16-bit pixels at a time, so the width dimension must be halved.
    // This means that odd dimensioned sprites will be truncated by 1 pixel. I checked the PSX graphics however and most
    // sprites with odd dimensions seem to have at least 1px of empty space padding around them, so not an issue in practice?
    const uint32_t expectedTexSize = sizeof(texlump_header_t) + (texInfo.width / 2) * texInfo.height * sizeof(uint16_t);

    const uint16_t tpageU = texPage.vramX;
    const uint16_t tpageV = texPage.vramY;

    if (texData.size >= expectedTexSize) {
        // Upload the texture to VRAM at the current fill location
        RECT dstVramRect;
        LIBGPU_setRECT(
            dstVramRect,
            (int16_t)(tpageU + gTCacheFillCellX * TCACHE_CELL_SIZE / 2),
            (int16_t)(tpageV + gTCacheFillCellY * TCACHE_CELL_SIZE),
            tex.width / 2,
            tex.height
        );

        LIBGPU_LoadImage(dstVramRect, (uint16_t*)(texData.pBytes + sizeof(texlump_header_t)));
    }
    else {
        // Not enough data in the lump to load the texture, issue a warning.
        // Hack: use the level startup warning buffer for this purpose (it's always around).
        std::snprintf(gLevelStartupWarning, C_ARRAY_SIZE(gLevelStartupWarning), "W:bad tex data for lump %d!", tex.lumpNum);
        gStatusBar.message = gLevelStartupWarning;
        gStatusBar.messageTicsLeft = 60;
    }

    // Save the texture's page coordinate
    tex.texPageCoordX = (uint8_t)(gTCacheFillCellX * TCACHE_CELL_SIZE);
    tex.texPageCoordY = (uint8_t)(gTCacheFillCellY * TCACHE_CELL_SIZE);

    // Get and save the texture page id.
    // Note that format '1' = '8 bpp (indexed)' and that transparency bits are added later during rendering.
    tex.texPageId = LIBGPU_GetTPage(1, 0, texPage.vramX, texPage.vramY);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Initializes the texture cache and creates the data structures needed to manage it
//------------------------------------------------------------------------------------------------------------------------------------------
void I_InitTexCache() noexcept {
    // Create texture cache pages for every 128x256 pixel region.
    // The pages are really 256x256 pixels at 8 bpp but in terms of 16 bpp coords that is 128x256.
    // Note: we skip the region 0,0 - 511,255 (4 pages) since it is reserved for the PSX framebuffer.
    gTCachePages.clear();

    const Gpu::Core& gpu = PsxVm::gGpu;
    const uint16_t vramW = gpu.ramPixelW;
    const uint16_t vramH = gpu.ramPixelH;

    gTCachePages.reserve((vramW / 128) * (vramH / 256));

    for (uint16_t vramY = 0; vramY < vramH; vramY += 256) {
        for (uint16_t vramX = 0; vramX < vramW; vramX += 128) {
            // Skip PSX framebuffer reserved regions
            if ((vramY == 0) && (vramX < 512))
                continue;

            // Init this texture page
            tcachepage_t& texPage = gTCachePages.emplace_back();
            texPage.vramX = vramX;
            texPage.vramY = vramY;
        }
    }

    ASSERT(gTCachePages.size() > 0);

    // Initially fill from page 0
    I_SetTexCacheFillPage(0);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Return how many texture cache pages there are
//------------------------------------------------------------------------------------------------------------------------------------------
uint32_t I_GetNumTexCachePages() noexcept {
    return (uint32_t) gTCachePages.size();
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Get the current page being filled in the texture cache
//------------------------------------------------------------------------------------------------------------------------------------------
uint32_t I_GetCurTexCacheFillPage() noexcept {
    return gTCacheFillPage;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Moves the texture cache's fill marker so it starts populating the cache next at the specified page
//------------------------------------------------------------------------------------------------------------------------------------------
void I_SetTexCacheFillPage(const uint32_t pageIdx) noexcept {
    ASSERT(pageIdx < gTCachePages.size());

    gTCacheFillPage = pageIdx;
    gTCacheFillCellX = 0;
    gTCacheFillCellY = 0;
    gTCacheFillRowCellH = 0;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Locks the specified texture cache page
//------------------------------------------------------------------------------------------------------------------------------------------
void I_LockTexCachePage(const uint32_t pageIdx) noexcept {
    ASSERT(pageIdx < gTCachePages.size());
    gTCachePages[pageIdx].bIsLocked = true;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Unlocks all texture cache pages
//------------------------------------------------------------------------------------------------------------------------------------------
void I_UnlockAllTexCachePages() noexcept {
    for (tcachepage_t& page : gTCachePages) {
        page.bIsLocked = false;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Removes the given texture from the cache and frees any cells that it is using.
// Note this should only be called if the texture is actually in the cache!
//------------------------------------------------------------------------------------------------------------------------------------------
void I_RemoveTexCacheEntry(texture_t& tex) noexcept {
    // Sanity checks: make sure the texture has a valid texture cache entry and size
    ASSERT(tex.ppTexCacheEntries);
    ASSERT((tex.width16 > 0) && (tex.width16 <= TCACHE_CELLS_X));
    ASSERT((tex.height16 > 0) && (tex.height16 <= TCACHE_CELLS_Y));

    // Clear any cells the texture occupies
    texture_t** pCacheEntry = tex.ppTexCacheEntries;

    for (int32_t y = 0; y < tex.height16; ++y) {
        for (int32_t x = 0; x < tex.width16; ++x) {
            *pCacheEntry = nullptr;
            ++pCacheEntry;
        }

        pCacheEntry += TCACHE_CELLS_X - tex.width16;    // Next row
    }

    // Wipe out the texture cache details
    tex.texPageCoordX = 0;
    tex.texPageCoordY = 0;
    tex.texPageId = 0;
    tex.ppTexCacheEntries = nullptr;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Upload the specified texture into VRAM if it's not already resident.
// If there's no more room for textures in VRAM for this frame then the game will die with an error.
//------------------------------------------------------------------------------------------------------------------------------------------
void I_CacheTex(texture_t& tex) noexcept {
    // Update which frame the texture was added to the cache in, for tracking texture cache overflows
    tex.uploadFrameNum = gNumFramesDrawn;

    // If the texture is already in the cache then there is nothing else to do
    if (tex.texPageId != 0)
        return;

    // Move to a valid fill location for the texture and abort if failed.
    // This will also evict textures from previous frames along the way.
    if (!TC_MoveToFillLocation(tex))
        return;

    // Fill the cells that this texture will occupy with references to the texture
    tcachepage_t& texPage = gTCachePages[gTCacheFillPage];
    TC_FillCacheCells(texPage, tex);

    // Load the texture data and get the dimensions of the texture from the data
    const texdata_t texData = TC_CacheTexData(tex);
    const texlump_header_t texInfo = *(const texlump_header_t*) texData.pBytes;

    // Upload the texture to vram
    TC_UploadTexToVram(tex, texInfo, texData, texPage);

    // Advance the fill position in the texture cache.
    // Also expand the fill row height if this texture is taller than the current height.
    gTCacheFillCellX += tex.width16;
    gTCacheFillRowCellH = std::max<uint32_t>(gTCacheFillRowCellH, tex.height16);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Evicts all textures in non-locked pages in the texture cache.
// Also resets the next position that we will populate the cache at.
//------------------------------------------------------------------------------------------------------------------------------------------
void I_PurgeTexCache() noexcept {
    for (tcachepage_t& page : gTCachePages) {
        if (!page.bIsLocked) {
            TC_FreePageCells(page);
        }
    }

    I_SetTexCacheFillPage(0);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Draws the 'vram viewer' screen that was only enabled in development builds of PSX DOOM
//------------------------------------------------------------------------------------------------------------------------------------------
void I_VramViewerDraw(const uint32_t texPageIdx) noexcept {
    ASSERT(texPageIdx < gTCachePages.size());
    const tcachepage_t& texPage = gTCachePages[texPageIdx];

    // PsyDoom: adjust the bottom edge of the VRAM view to account for overscan of 8-pixels (was '240')
    const int16_t BY = 232;

    // Draw a sprite covering the screen which covers the entire vram page
    {
        POLY_FT4 polyPrim = {};
        LIBGPU_SetPolyFT4(polyPrim);
        LIBGPU_setRGB0(polyPrim, 128, 128, 128);

        // PsyDoom: fix these right and bottom UVs now that we can do 16-bit coordinates.
        // This fixes slight pixel stretching issues in the VRAM viewer.
        #if PSYDOOM_LIMIT_REMOVING
            constexpr LibGpuUV BUV = 256;
            constexpr LibGpuUV RUV = 256;
        #else
            constexpr LibGpuUV BUV = 255;
            constexpr LibGpuUV RUV = 255;
        #endif

        LIBGPU_setUV4(polyPrim,
            0,      0,
            RUV,    0,
            0,      BUV,
            RUV,    BUV
        );

        LIBGPU_setXY4(polyPrim,
            0,      0,
            256,    0,
            0,      BY,
            256,    BY
        );

        polyPrim.clut = gPaletteClutIds[MAINPAL];
        polyPrim.tpage = LIBGPU_GetTPage(1, 0, texPage.vramX, texPage.vramY);   // Format 1 = 8bpp and don't care about transparency
        I_AddPrim(polyPrim);
    }

    // Draw red lines around all entries on this texture cache page
    const texture_t* const* pCacheEntry = &texPage.cells[0][0];

    for (int32_t entryIdx = 0; entryIdx < TCACHE_CELLS_X * TCACHE_CELLS_Y; ++entryIdx, ++pCacheEntry) {
        // Skip past this texture cache cell if there's no texture occupying it
        const texture_t* pTex = *pCacheEntry;

        if (!pTex)
            continue;

        // Setup some stuff for all lines drawn
        LINE_F2 linePrim = {};
        LIBGPU_SetLineF2(linePrim);
        LIBGPU_setRGB0(linePrim, 255, 0, 0);

        // Figure out the y position and height for the texture cache entry.
        // Have to rescale in using a 24.8 fixed point format because the screen is 240 pixels high, and the texture cache is 256 pixels high.
        uint32_t y = (d_lshift<8>((uint32_t) pTex->texPageCoordY)) * BY / TCACHE_PAGE_SIZE;
        uint32_t h = (d_lshift<8>((uint32_t) pTex->height))        * BY / TCACHE_PAGE_SIZE;
        y = d_rshift<8>(y);
        h = d_rshift<8>(h);

        const uint16_t x = pTex->texPageCoordX;
        const uint16_t w = pTex->width;

        // Draw all the box lines for the cache entry
        LIBGPU_setXY2(linePrim, x, (int16_t) y, x + w, (int16_t) y);            // Top
        I_AddPrim(linePrim);

        LIBGPU_setXY2(linePrim, x + w, (int16_t) y, x + w, (int16_t)(y + h));   // Right
        I_AddPrim(linePrim);

        LIBGPU_setXY2(linePrim, x + w, (int16_t)(y + h), x, (int16_t)(y + h));  // Bottom
        I_AddPrim(linePrim);

        LIBGPU_setXY2(linePrim, x, (int16_t)(y + h), x, (int16_t) y);           // Left
        I_AddPrim(linePrim);
    }
}

#endif  // #if PSYDOOM_MODS

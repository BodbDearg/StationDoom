#pragma once

#include "PsxVm/VmPtr.h"

// Maximum number of tracks on a CD
static constexpr int32_t CdlMAXTOC = 100;

// Describes a track location or file position on a CD.
// The location is described in audio terms.
struct CdlLOC {
    uint8_t minute;     // Note: in binary coded decimal form
    uint8_t second;     // Note: in binary coded decimal form
    uint8_t sector;     // Note: in binary coded decimal form
    uint8_t track;      // Unused in this PsyQ SDK version: normally '0'
    
    // Conversion to and from a single 32-bit int
    void operator = (const uint32_t loc32) noexcept {
        minute = (uint8_t)(loc32 >> 0);
        second = (uint8_t)(loc32 >> 8);
        sector = (uint8_t)(loc32 >> 16);
        track = (uint8_t)(loc32 >> 24);
    }

    inline operator uint32_t () const noexcept {
        return (
            ((uint32_t) minute << 0 ) |
            ((uint32_t) second << 8 ) |
            ((uint32_t) sector << 16) |
            ((uint32_t) track  << 24)
        );
    }
};

static_assert(sizeof(CdlLOC) == 4);

// Describes a file on a CD, including filename, location on disc and size
struct CdlFILE {
    CdlLOC      pos;
    uint32_t    size;
    char        name[16];
};

static_assert(sizeof(CdlFILE) == 24);

// Callback for 'CdReadCallback', 'CdReadyCallback', and 'CdSyncCallback'
typedef VmPtr<void (*)(const int32_t status, const uint8_t* pResult)> CdlCB;

void LIBCD_CdInit() noexcept;
void LIBCD_EVENT_def_cbsync() noexcept;
void LIBCD_EVENT_def_cbready() noexcept;
void LIBCD_EVENT_def_cbread() noexcept;
void LIBCD_CdReset() noexcept;
void LIBCD_CdFlush() noexcept;
void LIBCD_CdSync() noexcept;
void LIBCD_CdReady() noexcept;
void LIBCD_CdSyncCallback() noexcept;
void LIBCD_CdReadyCallback() noexcept;
void LIBCD_CdReadCallback() noexcept;
void LIBCD_CdControl() noexcept;
void LIBCD_CdControlF() noexcept;
void LIBCD_CdMix() noexcept;

int32_t LIBCD_CdGetSector(void* const pDst, const int32_t sizeInWords) noexcept;
void _thunk_LIBCD_CdGetSector() noexcept;

CdlLOC& LIBCD_CdIntToPos(const int32_t sectorNum, CdlLOC& pos) noexcept;
void _thunk_LIBCD_CdIntToPos() noexcept;

int32_t LIBCD_CdPosToInt(const CdlLOC& pos) noexcept;
void _thunk_LIBCD_CdPosToInt() noexcept;

void LIBCD_BIOS_getintr() noexcept;
void LIBCD_CD_sync() noexcept;
void LIBCD_CD_ready() noexcept;
void LIBCD_CD_cw() noexcept;
void LIBCD_CD_vol() noexcept;
void LIBCD_CD_shell() noexcept;
void LIBCD_CD_flush() noexcept;
void LIBCD_CD_init() noexcept;
void LIBCD_CD_initvol() noexcept;
void LIBCD_BIOS_cd_read_retry() noexcept;
void LIBCD_CD_datasync() noexcept;
void LIBCD_CD_getsector() noexcept;
void LIBCD_BIOS_callback() noexcept;
void LIBCD_BIOS_cb_read() noexcept;
int32_t LIBCD_CdGetToc(CdlLOC trackLocs[CdlMAXTOC]) noexcept;

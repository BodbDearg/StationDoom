#pragma once

#if PSYDOOM_VULKAN_RENDERER

#include "Macros.h"

#include <cstdint>

namespace vgl {
    struct VkFuncs;
}

BEGIN_NAMESPACE(VRenderer)

// Min/max supported depth ranges for the renderer
static constexpr float MIN_DEPTH = 1.0f;
static constexpr float MAX_DEPTH = 32768.0f;

extern bool             gbUsePsxRenderer;
extern bool             gbRequestRendererToggle;
extern vgl::VkFuncs     gVkFuncs;
extern uint32_t         gFramebufferW;
extern uint32_t         gFramebufferH;
extern int32_t          gClassicFramebufferX;
extern int32_t          gClassicFramebufferY;
extern uint32_t         gClassicFramebufferW;
extern uint32_t         gClassicFramebufferH;
extern float            gNdcToPsxScaleX, gNdcToPsxScaleY;
extern float            gPsxNdcOffsetX, gPsxNdcOffsetY;

void init() noexcept;
void destroy() noexcept;
bool beginFrame() noexcept;
bool canSubmitDrawCmds() noexcept;
void endFrame() noexcept;
void pushPsxVramUpdates(const uint16_t rectLx, const uint16_t rectRx, const uint16_t rectTy, const uint16_t rectBy) noexcept;

END_NAMESPACE(VRenderer)

#endif  // #if PSYDOOM_VULKAN_RENDERER
//------------------------------------------------------------------------------------------------------------------------------------------
// This module is the PsyDoom re-implementation/equivalent of 'r_bsp' in PSX Doom.
// It concerns itself with traversing the BSP tree according to the viewpoint to build an ordered list of draw subsectors.
// The draw subsectors list generated is ordered front to back.
//------------------------------------------------------------------------------------------------------------------------------------------
#include "rv_bsp.h"

#if PSYDOOM_VULKAN_RENDERER

#include "Doom/Game/doomdata.h"
#include "Doom/Game/p_setup.h"
#include "Doom/Renderer/r_local.h"
#include "Doom/Renderer/r_main.h"
#include "rv_data.h"
#include "rv_main.h"
#include "rv_occlusion.h"
#include "rv_utils.h"

// This is the list of subsectors to be drawn by the Vulkan renderer, in front to back order
std::vector<subsector_t*> gRvDrawSubsecs;

//------------------------------------------------------------------------------------------------------------------------------------------
// Tells if the given line segment is occluding for the purposes of visibility testing.
// Occluding segs should mark out areas of the screen that they cover, so that nothing behind draws.
//------------------------------------------------------------------------------------------------------------------------------------------
static bool RV_IsOccludingSeg(const rvseg_t& seg, const sector_t& frontSector) noexcept {
    // If the linedef specifies the 'ML_VOID' flag (has see through 'void' parts) then ignore it for occlusion
    if (seg.linedef->flags & ML_VOID)
        return false;

    // One sided lines are always occluding
    if (!seg.backsector)
        return true;

    // Get the mid-wall gap between the front and back sectors.
    // Also update the height that the back sector floor is to be drawn at, before we compare.
    sector_t& backSector = *seg.backsector;
    R_UpdateFloorDrawHeight(backSector);

    const fixed_t fty = frontSector.ceilingheight;
    const fixed_t fby = frontSector.floorDrawHeight;
    const fixed_t bty = backSector.ceilingheight;
    const fixed_t bby = backSector.floorDrawHeight;
    const fixed_t midTy = std::min(fty, bty);
    const fixed_t midBy = std::max(fby, bby);

    // If there is a gap then the seg cannot be occluding
    if (midTy > midBy)
        return false;

    // Are there lower or upper walls?
    // If there is just a lower or just an upper wall (1 wall) then the seg always occludes, even the wall is sky.
    const bool bHasLowerWall = (midBy > fby);
    const bool bHasUpperWall = (midTy < fty);

    if (bHasUpperWall != bHasLowerWall)
        return true;

    // Does the back sector have sky ceilings or floors?
    // The seg occludes if it's all sky walls or all solid walls.
    const bool bBackSkyFloor = (backSector.floorpic == -1);
    const bool bBackSkyCeil = (backSector.ceilingpic == -1);
    return (bBackSkyFloor == bBackSkyCeil);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Check if the bounding box for the specified node is visible
//------------------------------------------------------------------------------------------------------------------------------------------
static bool RV_NodeBBVisible(const fixed_t boxCoords[4]) noexcept {
    // Get the bounds of the box in normalized device coords to see what area of the screen it occupies
    const float nodeTy = RV_FixedToFloat(boxCoords[BOXTOP]);
    const float nodeBy = RV_FixedToFloat(boxCoords[BOXBOTTOM]);
    const float nodeLx = RV_FixedToFloat(boxCoords[BOXLEFT]);
    const float nodeRx = RV_FixedToFloat(boxCoords[BOXRIGHT]);

    float nodeMinX = +1.0f;
    float nodeMaxX = -1.0f;

    const auto addNodeLineToBounds = [&](const float x1, const float y1, const float x2, const float y2) noexcept {
        float lx = {};
        float rx = {};

        if (RV_GetLineNdcBounds(x1, y1, x2, y2, lx, rx)) {
            nodeMinX = std::min(lx, nodeMinX);
            nodeMaxX = std::max(rx, nodeMaxX);
        }
    };

    addNodeLineToBounds(nodeLx, nodeTy, nodeRx, nodeTy);
    addNodeLineToBounds(nodeRx, nodeTy, nodeRx, nodeBy);
    addNodeLineToBounds(nodeRx, nodeBy, nodeLx, nodeBy);
    addNodeLineToBounds(nodeLx, nodeBy, nodeLx, nodeTy);

    // Regard the node as visible if any of that area is unobscured
    return RV_IsRangeVisible(nodeMinX, nodeMaxX);
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Visit a subsector during BSP traversal.
// Adds it to the list of subsectors to be drawn, and marks the areas that its fully solid walls occlude.
//------------------------------------------------------------------------------------------------------------------------------------------
static void RV_VisitSubsec(const int32_t subsecIdx) noexcept {
    // Update the height that the sector floor is to be rendered at
    subsector_t& subsec = gpSubsectors[subsecIdx];
    sector_t& frontSector = *subsec.sector;
    R_UpdateFloorDrawHeight(frontSector);

    // Assume the subsector can have its flats batched initially
    subsec.bVkCanBatchFlats = true;

    // Run through all of the segs for the subsector and mark out areas of the screen that they fully occlude.
    // Also determine whether each seg is visible and backfacing while we are at it.
    rvseg_t* const pSegs = gpRvSegs.get() + subsec.firstseg;
    const uint32_t numSegs = subsec.numsegs;

    for (uint32_t segIdx = 0; segIdx < numSegs; ++segIdx) {
        // Firstly, clear the line segment flags
        rvseg_t& seg = pSegs[segIdx];
        seg.flags = 0;

        // Determine whether the segment is backfacing so we can re-use the result later
        const float p1f[2] = { seg.v1x, seg.v1y };
        const float p2f[2] = { seg.v2x, seg.v2y };

        {
            const float viewDx = gViewXf - p1f[0];
            const float viewDy = gViewYf - p1f[1];
            const float edgeDx = p2f[0] - p1f[0];
            const float edgeDy = p2f[1] - p1f[1];

            if (edgeDx * viewDy > edgeDy * viewDx) {
                seg.flags |= SGF_BACKFACING;
            }
        }

        // Determine if the seg is visible, ignoring whether it is backfacing or not.
        // First get the area of the screen that the seg covers in normalized device coords, if it's onscreen.
        // Then check if that range is actually visible if it is onscreen.
        float segLx = {};
        float segRx = {};

        if (RV_GetLineNdcBounds(p1f[0], p1f[1], p2f[0], p2f[1], segLx, segRx)) {
            if (RV_IsRangeVisible(segLx, segRx)) {
                seg.flags |= SGF_VISIBLE_COLS;
            }
        }

        // Make the seg occlude if it's the type of seg that occludes, it's not backfacing (so we can see via noclip) and if it's visible
        const bool bMakeSegOcclude = (
            ((seg.flags & SGF_BACKFACING) == 0) &&
            (seg.flags & SGF_VISIBLE_COLS) &&
            RV_IsOccludingSeg(seg, frontSector)
        );

        if (bMakeSegOcclude) {
            RV_OccludeRange(segLx, segRx);
        }

        // Check to see if the seg has a situation which might cause this subsector's flats to not be batchable with other flats.
        // If the upper wall goes past the floor height or the lower wall goes past the ceiling height then the subsector flat MUST
        // be drawn immediately after it's walls are rendered for correct layering. Because of this strict ordering requirement, the
        // subsector's flats would not be batchable with other flats in that scenario.
        const sector_t* const pBackSec = seg.backsector;

        if (pBackSec) {
            const bool bLowerWallBreaksBatches = (pBackSec->floorDrawHeight > frontSector.ceilingheight);
            const bool bUpperWallBreaksBatches = (pBackSec->ceilingheight < frontSector.floorDrawHeight);

            if (bLowerWallBreaksBatches || bUpperWallBreaksBatches) {
                subsec.bVkCanBatchFlats = false;
            }
        }
    }

    // Add the subsector to the draw list and set it's draw index.
    // If the sector has a sky ceiling or floor (new engine feature) then also mark the sky as visible.
    subsec.vkDrawSubsecIdx = (int32_t) gRvDrawSubsecs.size();
    gRvDrawSubsecs.push_back(&subsec);

    if (frontSector.ceilingpic == -1) {
        gbIsSkyVisible = true;
    }

    if (frontSector.floorpic == -1) {
        gbIsSkyVisible = true;
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Does recursive traversal of the BSP tree to prepare a list of subsectors to draw, starting at the given node
//------------------------------------------------------------------------------------------------------------------------------------------
static void RV_VisitBspNode(const int32_t nodeIdx) noexcept {
    // Is this node number a subsector?
    // If so then process it for potential later rendering:
    if (nodeIdx & NF_SUBSECTOR) {
        // Note: this strange check is in the PC engine too...
        // Under what circumstances can the node number be '-1'?
        if (nodeIdx == -1) {
            RV_VisitSubsec(0);
        } else {
            RV_VisitSubsec(nodeIdx & (~NF_SUBSECTOR));
        }
    } else {
        // This is not a subsector, continue traversing the BSP tree.
        // Only stop when a particular node is determined to be not visible.
        node_t& node = gpBspNodes[nodeIdx];

        // Compute which side of the line the point is on using the cross product.
        // This is pretty much the same code found in 'R_PointOnSide':
        const float dx = gViewXf - RV_FixedToFloat(node.line.x);
        const float dy = gViewYf - RV_FixedToFloat(node.line.y);
        const float lprod = RV_FixedToFloat(node.line.dx) * dy;
        const float rprod = RV_FixedToFloat(node.line.dy) * dx;

        // Depending on which side of the halfspace we are on, reverse the traversal order:
        if (lprod < rprod) {
            if (RV_NodeBBVisible(node.bbox[0])) {
                RV_VisitBspNode(node.children[0]);
            }

            if (RV_NodeBBVisible(node.bbox[1])) {
                RV_VisitBspNode(node.children[1]);
            }
        } else {
            if (RV_NodeBBVisible(node.bbox[1])) {
                RV_VisitBspNode(node.children[1]);
            }

            if (RV_NodeBBVisible(node.bbox[0])) {
                RV_VisitBspNode(node.children[0]);
            }
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Start traversing the BSP tree from the root using the current viewpoint and find what subsectors are to be drawn
//------------------------------------------------------------------------------------------------------------------------------------------
void RV_BuildDrawSubsecList() noexcept {
    // Prepare the draw subsectors list and prealloc enough memory
    gRvDrawSubsecs.clear();
    gRvDrawSubsecs.reserve(gNumSubsectors);

    // Initially assume the sky is not visible
    gbIsSkyVisible = false;

    // Traverse the BSP tree, starting at the root
    const int32_t bspRootNodeIdx = gNumBspNodes - 1;
    RV_VisitBspNode(bspRootNodeIdx);
}

#endif  // #if PSYDOOM_VULKAN_RENDERER

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
#include "rv_main.h"
#include "rv_utils.h"

// This is the list of subsectors to be drawn by the Vulkan renderer, in front to back order
std::vector<subsector_t*> gRVDrawSubsecs;

//------------------------------------------------------------------------------------------------------------------------------------------
// Tells if the given line segment is occluding for the purposes of visibility testing.
// Occluding segs should mark out areas of the screen that they cover, so that nothing behind draws.
//------------------------------------------------------------------------------------------------------------------------------------------
static bool RV_IsOccludingSeg(const seg_t& seg, const sector_t& frontSector) noexcept {
    // One sided lines are always occluding
    if (!seg.backsector)
        return true;

    sector_t& backSector = *seg.backsector;

    // If there is a zero sized gap between the sector in front of the line and behind it then count as occluding.
    // This will happen most typically for closed doors:
    return (
        (frontSector.floorheight >= backSector.ceilingheight) ||
        (frontSector.ceilingheight <= backSector.floorheight)
    );
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Check if the bounding box for the specified node is visible
//------------------------------------------------------------------------------------------------------------------------------------------
static bool RV_NodeBBVisible(const fixed_t boxCoords[4]) noexcept {
    // TODO....
    return true;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Visit a subsector during BSP traversal.
// Adds it to the list of subsectors to be drawn, and marks the areas that its fully solid walls occlude.
//------------------------------------------------------------------------------------------------------------------------------------------
static void RV_VisitSubsec(const int32_t subsecIdx) noexcept {
    // This is the screen space 'X' coordinate range of the subsector, in normalized device coordinates
    float subsecXMin = +1.0f;
    float subsecXMax = -1.0f;

    // Run through all leaf edges for the subsector and see if any are visible; if no edges are visible then skip drawing the subsector entirely.
    // While we are at it add occluding segs to the area of the screen occluded.
    subsector_t& subsec = gpSubsectors[subsecIdx];
    sector_t& frontSector = *subsec.sector;

    const leafedge_t* const pLeafEdges = gpLeafEdges + subsec.firstLeafEdge;
    const uint32_t numLeafEdges = subsec.numLeafEdges;
    
    for (uint32_t edgeIdx = 0; edgeIdx < numLeafEdges; ++edgeIdx) {
        // Get the edge points in float format
        const vertex_t& p1 = *pLeafEdges[edgeIdx].vertex;
        const vertex_t& p2 = *pLeafEdges[(edgeIdx + 1) % numLeafEdges].vertex;
        const float p1f[2] = { RV_FixedToFloat(p1.x), RV_FixedToFloat(p1.y) };
        const float p2f[2] = { RV_FixedToFloat(p2.x), RV_FixedToFloat(p2.y) };

        // Get the normalized device x bounds of the segment and skip if offscreen
        float segLx = {};
        float segRx = {};

        if (!RV_GetLineNdcBounds(p1f[0], p1f[1], p2f[0], p2f[1], segLx, segRx))
            continue;

        subsecXMin = std::min(subsecXMin, segLx);
        subsecXMax = std::max(subsecXMax, segRx);

        // TODO: check if seg occluding
        // TODO: add seg to occluders if occluding
    }

    // Ignore the subsector if completely offscreen or if occluded
    bool bHasVisibleSegs = false;

    if (subsecXMin < subsecXMax) {
        // TODO: check if subsector range fully occluded
        bHasVisibleSegs = true;
    }

    // Add the subsector to the draw list if any segs are visible.
    // If the sector has a sky then also mark the sky as visible.
    if (bHasVisibleSegs) {
        gRVDrawSubsecs.push_back(&subsec);

        if (frontSector.ceilingpic < 0) {
            gbIsSkyVisible = true;
        }
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
    gRVDrawSubsecs.clear();
    gRVDrawSubsecs.reserve(gNumSubsectors);

    // Initially assume the sky is not visible
    gbIsSkyVisible = false;

    // Traverse the BSP tree, starting at the root
    const int32_t bspRootNodeIdx = gNumBspNodes - 1;
    RV_VisitBspNode(bspRootNodeIdx);
}

#endif  // #if PSYDOOM_VULKAN_RENDERER

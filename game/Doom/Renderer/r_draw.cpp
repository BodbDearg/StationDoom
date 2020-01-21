#include "r_draw.h"

#include "Doom/Base/i_main.h"
#include "Doom/Game/p_setup.h"
#include "PcPsx/Finally.h"
#include "PsxVm/PsxVm.h"
#include "PsyQ/LIBETC.h"
#include "PsyQ/LIBGTE.h"
#include "r_local.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_segs.h"
#include "r_things.h"

// The maximum number of new vertices that can be added to leafs by clipping operations.
// If we happen to emit more than this then engine will fail with an error.
static constexpr int32_t MAX_NEW_CLIP_VERTS = 32;

// How many new vertices were generated by clipping operations and a list of those new vertices
static VmPtr<int32_t>                           gNumNewClipVerts(0x80078220);
static VmPtr<vertex_t[MAX_NEW_CLIP_VERTS]>      gNewClipVerts(0x80097CEC);

//------------------------------------------------------------------------------------------------------------------------------------------
// Draws everything in the subsector: floors, ceilings, walls and things
//------------------------------------------------------------------------------------------------------------------------------------------
void R_DrawSubsector(subsector_t& subsec) noexcept {
    // The PSX scratchpad is used to store 2 leafs, grab that memory here.
    // The code below ping-pongs between both leafs, using them as either input or output leafs for each clipping operation
    // I don't know why this particular address is used though...
    leaf_t* const pLeafs = (leaf_t*) getScratchAddr(42);
    leaf_t& leaf1 = pLeafs[0];
    
    // Cache the entire leaf for the subsector to the scratchpad.
    // Also transform any leaf vertices that were not yet transformed up until this point.
    {
        const leafedge_t* pSrcEdge = gpLeafEdges->get() + subsec.firstLeafEdge;
        leafedge_t* pDstEdge = leaf1.edges;
        
        for (int32_t edgeIdx = 0; edgeIdx < subsec.numLeafEdges; ++edgeIdx, ++pSrcEdge, ++pDstEdge) {
            // Cache the leaf edge
            vertex_t& vert = *pSrcEdge->vertex;
            
            pDstEdge->vertex = &vert;
            pDstEdge->seg = pSrcEdge->seg;
            
            // Transform this leaf edge's vertexes if they need to be transformed
            if (vert.frameUpdated != *gNumFramesDrawn) {
                const SVECTOR viewToPt = {
                    (int16_t)((vert.x - *gViewX) >> 16),
                    0,
                    (int16_t)((vert.y - *gViewY) >> 16)
                };
                
                VECTOR viewVec;
                int32_t rotFlags;
                LIBGTE_RotTrans(viewToPt, viewVec, rotFlags);
                
                vert.viewx = viewVec.vx;
                vert.viewy = viewVec.vz;
                
                if (viewVec.vz > 3) {
                    vert.scale = (HALF_SCREEN_W * FRACUNIT) / viewVec.vz;
                    vert.screenx = ((vert.scale * vert.viewx) >> FRACBITS) + HALF_SCREEN_W;
                }
                
                vert.frameUpdated = *gNumFramesDrawn;
            }
        }
        
        leaf1.numEdges = subsec.numLeafEdges;
    }
    
    // Begin the process of clipping the leaf.
    // Ping pong between the two leaf buffers for input and output..
    uint32_t curLeafIdx = 0;
    *gNumNewClipVerts = 0;
    
    // Clip the leaf against the front plane if required
    {
        leafedge_t* pEdge = leaf1.edges;
        
        for (int32_t edgeIdx = 0; edgeIdx < subsec.numLeafEdges; ++edgeIdx, ++pEdge) {
            if (pEdge->vertex->viewy <= NEAR_CLIP_DIST + 1) {
                R_FrontZClip(pLeafs[curLeafIdx], pLeafs[curLeafIdx ^ 1]);
                curLeafIdx ^= 1;
                break;
            }
        }
    }
    
    // Check to see what side of the left view frustrum plane the leaf's points are on.
    // Clip the leaf if required, or discard if all the points are offscreen.
    const int32_t leftPlaneSide = R_CheckLeafSide(false, pLeafs[curLeafIdx]);
    
    if (leftPlaneSide < 0)
        return;
    
    if (leftPlaneSide > 0) {
        a0 = ptrToVmAddr(pLeafs + curLeafIdx);
        a1 = ptrToVmAddr(pLeafs + (curLeafIdx ^ 1));
        R_LeftEdgeClip();
        curLeafIdx ^= 1;
        
        // TODO: comment/handle return value
        if (i32(v0) < 3)
            return;
    }
    
    // Check to see what side of the right view frustrum plane the leaf's points are on.
    // Clip the leaf if required, or discard if all the points are offscreen.
    const int32_t rightPlaneSide = R_CheckLeafSide(true, pLeafs[curLeafIdx]);
    
    if (rightPlaneSide < 0)
        return;
    
    // Clip the leaf against the right view frustrum plane if required
    if (rightPlaneSide > 0) {
        a0 = ptrToVmAddr(pLeafs + curLeafIdx);
        a1 = ptrToVmAddr(pLeafs + (curLeafIdx ^ 1));
        R_RightEdgeClip();
        curLeafIdx ^= 1;
        
        // TODO: comment/handle return value
        if (i32(v0) < 3)
            return;
    }
    
    // Terminate the list of leaf edges by putting the first edge past the end of the list.
    // This allows the renderer to implicitly wraparound to the beginning of the list when accessing 1 past the end.
    // This is useful for when working with edges as it saves checks!
    leaf_t& drawleaf = pLeafs[curLeafIdx];
    drawleaf.edges[drawleaf.numEdges] = drawleaf.edges[0];
    
    // Draw all visible walls/segs in the leaf
    {
        leafedge_t* pEdge = drawleaf.edges;
        
        for (int32_t edgeIdx = 0; edgeIdx < drawleaf.numEdges; ++edgeIdx, ++pEdge) {
            // Only draw the seg if its marked as visible
            seg_t* const pSeg = pEdge->seg.get();
            
            if (pSeg && (pSeg->flags & SGF_VISIBLE_COLS)) {
                a0 = ptrToVmAddr(pEdge);
                R_DrawSubsectorSeg();       // TODO: RENAME - it's drawing a leaf edge
            }
        }
    }
    
    // Draw the floor if above it
    sector_t& drawsec = **gpCurDrawSector;
    
    if (*gViewZ > drawsec.floorheight) {
        a0 = ptrToVmAddr(&drawleaf);
        a1 = 0;
        R_DrawSubsectorFlat();
    }
    
    // Draw the ceiling if below it and it is not a sky ceiling
    if ((drawsec.ceilingpic != -1) && (*gViewZ < drawsec.ceilingheight)) {
        a0 = ptrToVmAddr(&drawleaf);
        a1 = 1;
        R_DrawSubsectorFlat();
    }
    
    // Draw all sprites in the subsector
    a0 = ptrToVmAddr(&subsec);
    R_DrawSubsectorSprites();
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Clip the given input leaf against the front view frustrum plane.
// Returns the result in the given output leaf.
//------------------------------------------------------------------------------------------------------------------------------------------
void R_FrontZClip(const leaf_t& inLeaf, leaf_t& outLeaf) noexcept {
    // For some reason front plane clipping of leafs is much more aggressive than earlier stages of the rendering pipeline.
    // This is the clipping distance used here - twice the amount used elsewhere.
    constexpr int32_t CLIP_DIST = NEAR_CLIP_DIST * 2;
    
    // Run through all the source edges in the given leaf and see if each edge needs to be clipped, skipped or stored as-is
    const leafedge_t* pSrcEdge = inLeaf.edges;
    const int32_t numSrcEdges = inLeaf.numEdges;
    
    leafedge_t* pDstEdge = outLeaf.edges;
    int32_t numDstEdges = 0;
    
    for (int32_t srcEdgeIdx = 0; srcEdgeIdx < numSrcEdges; ++srcEdgeIdx, ++pSrcEdge) {
        // Grab the next edge after this and wraparound if required
        const leafedge_t* pNextSrcEdge = pSrcEdge + 1;
        
        if (srcEdgeIdx == numSrcEdges - 1) {
            pNextSrcEdge = inLeaf.edges;
        }

        // Get the 2 points in this edge and their signed distance to the clipping plane
        vertex_t& srcVert1 = *pSrcEdge->vertex;
        vertex_t& srcVert2 = *pNextSrcEdge->vertex;
        
        int32_t planeDist1 = CLIP_DIST - srcVert1.viewy;
        int32_t planeDist2 = CLIP_DIST - srcVert2.viewy;
        
        // See if we need to clip or not.
        // Generate a new edge and vertex if required.
        if (planeDist1 == 0) {
            // Rare case: if the 1st point is exactly on the plane then just emit the edge as-is and don't clip
            pDstEdge->vertex = pSrcEdge->vertex;
            pDstEdge->seg = pSrcEdge->seg;
        } else {
            // If the 1st point is on the inside of the clipping plane, emit this edge as-is to begin with
            if (planeDist1 < 0) {                
                pDstEdge->vertex = pSrcEdge->vertex;
                pDstEdge->seg = pSrcEdge->seg;
                ++numDstEdges;
                ++pDstEdge;
                
                if (numDstEdges > MAX_LEAF_EDGES) {
                    I_Error("FrontZClip: Point Overflow");
                }
            }
            
            // Rare case: if the 2nd point is exactly on the plane then do not clip, will emit it's edge in the next iteration
            if (planeDist2 == 0)
                continue;
            
            // If both points are on the same side of the clipping plane then we do not clip
            if ((planeDist1 < 0) == (planeDist2 < 0))
                continue;
            
            // Clipping required: will make a new vertex because of the clipping operation:
            vertex_t& newVert = gNewClipVerts[*gNumNewClipVerts];
            *gNumNewClipVerts += 1;
            
            if (*gNumNewClipVerts >= MAX_NEW_CLIP_VERTS) {
                I_Error("FrontZClip: exceeded max new vertexes\n");
            }
            
            // Compute the intersection time of the edge against the plane.
            // Use the same method described in more detail in 'R_CheckBBox':
            fixed_t intersectT;

            {
                const int32_t a = planeDist1;
                const int32_t b = -planeDist2;
                intersectT = (a << FRACBITS) / (a + b);
            }

            // Compute & set the view x/y values for the clipped edge vertex
            {
                const int32_t dviewx = srcVert2.viewx - srcVert1.viewx;
                newVert.viewx = ((dviewx * intersectT) >> FRACBITS) + srcVert1.viewx;
                newVert.viewy = CLIP_DIST;
            }

            // Compute the world x/y values for the clipped edge vertex
            {
                const int32_t dx = (srcVert2.x - srcVert1.x) >> FRACBITS;
                const int32_t dy = (srcVert2.y - srcVert1.y) >> FRACBITS;
                newVert.x = dx * intersectT + srcVert1.x;
                newVert.y = dy * intersectT + srcVert1.y;
            }

            // Re-do perspective projection to compute screen x and scale for the vertex
            newVert.scale = (HALF_SCREEN_W * FRACUNIT) / newVert.viewy;
            newVert.screenx = ((newVert.viewx * newVert.scale) >> FRACBITS) + HALF_SCREEN_W;

            // Mark the new vertex as having up-to-date transforms and populate the new edge created.
            // Note that the new edge will only have a seg it doesn't run along the clip plane.
            newVert.frameUpdated = *gNumFramesDrawn;
            
            if (planeDist1 > 0 && planeDist2 < 0) {
                pDstEdge->seg = pSrcEdge->seg;
            } else {
                pDstEdge->seg = nullptr;    // New edge will run along the clip plane, this is not associated with any seg
            }
            
            pDstEdge->vertex = &newVert;
        }
        
        // If we get to here then we stored an edge, move along to the next edge
        ++numDstEdges;
        ++pDstEdge;
        
        if (numDstEdges > MAX_LEAF_EDGES) {
            I_Error("FrontZClip: Point Overflow");
        }
    }
    
    // Before we finish up, save the new number of edges after clipping
    outLeaf.numEdges = numDstEdges;
}

//------------------------------------------------------------------------------------------------------------------------------------------
// Check to see what side of left or right view frustrum plane all points in the leaf are on.
// Also stores what side each point is on at the start of scratchpad memory as a bool32_t.
//
// Returns:
//      -1  : If all leaf points are on the back side of the plane (leaf can be discarded completely)
//       0  : If all leaf points are on the front side of the plane (leaf does not need view frustrum clipping)
//      +1  : If some leaf points are on the inside and some on the outside of the plane (leaf needs to be clipped)
//------------------------------------------------------------------------------------------------------------------------------------------
int32_t R_CheckLeafSide(const bool bRightViewPlane, const leaf_t& leaf) noexcept {
    // Loop vars for iterating through the leaf
    const leafedge_t* pLeafEdge = leaf.edges;
    const int32_t numLeafEdges = leaf.numEdges;
    
    // Store whether each point is on the inside or outside of the leaf in scratchpad memory
    bool32_t* const pbPointsOnOutside = (bool32_t*) getScratchAddr(0);
    bool32_t* pbPointOnOutside = pbPointsOnOutside;
    
    // Track how many points are on the inside or outside of the plane here.
    // For each point on the inside, increment - otherwise decrement.
    int32_t insideOutsideCount = 0;
    
    // See which plane we are checking against, left or right view frustrum plane
    if (!bRightViewPlane) {
        for (int32_t edgeIdx = 0; edgeIdx < numLeafEdges; ++edgeIdx, ++pLeafEdge, ++pbPointOnOutside) {
            vertex_t& vert = *pLeafEdge->vertex;
            
            if (-vert.viewx > vert.viewy) {
                *pbPointOnOutside = true;
                --insideOutsideCount;
            } else {
                *pbPointOnOutside = false;
                ++insideOutsideCount;
            }
        }
    } else {
        for (int32_t edgeIdx = 0; edgeIdx < numLeafEdges; ++edgeIdx, ++pLeafEdge, ++pbPointOnOutside) {
            vertex_t& vert = *pLeafEdge->vertex;
            
            if (vert.viewx > vert.viewy) {
                *pbPointOnOutside = true;
                --insideOutsideCount;
            } else {
                *pbPointOnOutside = false;
                ++insideOutsideCount;
            }
        }
    }
    
    // Terminate the list of whether each leaf point is on the front side of the plane or not by duplicating
    // the first entry in the list at the end. This allows the renderer to wraparound automatically to the
    // beginning of the list when accessing 1 past the end. This saves on checks when working with edges!
    *pbPointOnOutside = pbPointsOnOutside[0];
    
    // Return what the renderer should do with the leaf
    if (insideOutsideCount == numLeafEdges) {
        // All points are on the inside, no clipping required
        return 0;
    } else if (insideOutsideCount == -numLeafEdges) {
        // All points are on the outside, leaf should be completely discarded
        return -1;
    } else {
        // Some points are on the inside, some on the outside - clipping is required
        return 1;
    }
}

void R_LeftEdgeClip() noexcept {
loc_8002CE68:
    sp -= 0x48;
    sw(s3, sp + 0x2C);
    sw(a1, sp + 0x10);
    s3 = a1 + 4;
    sw(s6, sp + 0x38);
    s6 = 0;                                             // Result = 00000000
    sw(s4, sp + 0x30);
    s4 = 0;                                             // Result = 00000000
    sw(ra, sp + 0x44);
    sw(fp, sp + 0x40);
    sw(s7, sp + 0x3C);
    sw(s5, sp + 0x34);
    sw(s2, sp + 0x28);
    sw(s1, sp + 0x24);
    sw(s0, sp + 0x20);
    s7 = lw(a0);
    fp = a0 + 4;
    if (i32(s7) <= 0) goto loc_8002D0CC;
    s1 = fp;
    s5 = 0x1F800000;                                    // Result = 1F800000
loc_8002CEC0:
    v0 = lw(s5);
    if (v0 != 0) goto loc_8002CEEC;
    v0 = lw(s1);
    v1 = lw(s1 + 0x4);
    sw(v0, s3);
    sw(v1, s3 + 0x4);
    s3 += 8;
    s4++;                                               // Result = 00000001
    v0 = lw(s5);
loc_8002CEEC:
    v1 = lw(s5 + 0x4);
    {
        const bool bJump = (v0 != 0);
        v0 = 1;                                         // Result = 00000001
        if (bJump) goto loc_8002CF08;
    }
    {
        const bool bJump = (v1 != v0);
        v0 = s7 - 1;
        if (bJump) goto loc_8002D0B8;
    }
    goto loc_8002CF10;
loc_8002CF08:
    v0 = s7 - 1;
    if (v1 != 0) goto loc_8002D0B8;
loc_8002CF10:
    v0 = (i32(s6) < i32(v0));
    s2 = fp;
    if (v0 == 0) goto loc_8002CF20;
    s2 = s1 + 8;
loc_8002CF20:
    v1 = *gNumNewClipVerts;
    a0 = v1 + 1;
    v0 = v1 << 3;
    v0 -= v1;
    v0 <<= 2;
    v1 = gNewClipVerts;
    *gNumNewClipVerts = a0;
    a0 = (i32(a0) < 0x20);
    s0 = v0 + v1;
    if (a0 != 0) goto loc_8002CF60;
    I_Error("LeftEdgeClip: exceeded max new vertexes\n");
loc_8002CF60:
    v1 = lw(s1);
    a0 = lw(s2);
    v0 = lw(v1 + 0xC);
    a1 = lw(v1 + 0x10);
    v1 = lw(a0 + 0xC);
    a0 = lw(a0 + 0x10);
    v0 += a1;
    v1 += a0;
    a2 = v0 << 16;
    v0 -= v1;
    div(a2, v0);
    if (v0 != 0) goto loc_8002CF98;
    _break(0x1C00);
loc_8002CF98:
    at = -1;                                            // Result = FFFFFFFF
    {
        const bool bJump = (v0 != at);
        at = 0x80000000;                                // Result = 80000000
        if (bJump) goto loc_8002CFB0;
    }
    if (a2 != at) goto loc_8002CFB0;
    tge(zero, zero, 0x5D);
loc_8002CFB0:
    a2 = lo;
    a0 -= a1;
    mult(a2, a0);
    v0 = lo;
    v0 = u32(i32(v0) >> 16);
    v0 += a1;
    sw(v0, s0 + 0x10);
    v0 = -v0;
    sw(v0, s0 + 0xC);
    v0 = lw(s2);
    v1 = lw(s1);
    v0 = lw(v0);
    v1 = lw(v1);
    v0 -= v1;
    v0 = u32(i32(v0) >> 16);
    mult(a2, v0);
    v0 = lo;
    v0 += v1;
    sw(v0, s0);
    v0 = lw(s2);
    v1 = lw(s1);
    v0 = lw(v0 + 0x4);
    a1 = lw(v1 + 0x4);
    v0 -= a1;
    v0 = u32(i32(v0) >> 16);
    mult(a2, v0);
    a0 = lo;
    v0 = lw(s0 + 0x10);
    v1 = 0x800000;                                      // Result = 00800000
    div(v1, v0);
    if (v0 != 0) goto loc_8002D03C;
    _break(0x1C00);
loc_8002D03C:
    at = -1;                                            // Result = FFFFFFFF
    {
        const bool bJump = (v0 != at);
        at = 0x80000000;                                // Result = 80000000
        if (bJump) goto loc_8002D054;
    }
    if (v1 != at) goto loc_8002D054;
    tge(zero, zero, 0x5D);
loc_8002D054:
    v1 = lo;
    v0 = lw(s0 + 0xC);
    mult(v1, v0);
    v0 = *gNumFramesDrawn;
    a0 += a1;
    sw(a0, s0 + 0x4);
    sw(v1, s0 + 0x8);
    sw(v0, s0 + 0x18);
    v0 = lo;
    v0 = u32(i32(v0) >> 16);
    v0 += 0x80;
    sw(v0, s0 + 0x14);
    sw(s0, s3);
    v0 = lw(s1 + 0x4);
    s4++;                                               // Result = 00000001
    sw(v0, s3 + 0x4);
    v0 = (i32(s4) < 0x15);                              // Result = 00000001
    s3 += 8;
    if (v0 != 0) goto loc_8002D0B8;
    I_Error("LeftEdgeClip: Point Overflow");
loc_8002D0B8:
    s1 += 8;
    s6++;
    v0 = (i32(s6) < i32(s7));
    s5 += 4;
    if (v0 != 0) goto loc_8002CEC0;
loc_8002D0CC:
    a3 = lw(sp + 0x10);
    v0 = s4;                                            // Result = 00000000
    sw(s4, a3);
    ra = lw(sp + 0x44);
    fp = lw(sp + 0x40);
    s7 = lw(sp + 0x3C);
    s6 = lw(sp + 0x38);
    s5 = lw(sp + 0x34);
    s4 = lw(sp + 0x30);
    s3 = lw(sp + 0x2C);
    s2 = lw(sp + 0x28);
    s1 = lw(sp + 0x24);
    s0 = lw(sp + 0x20);
    sp += 0x48;
    return;
}

void R_RightEdgeClip() noexcept {
loc_8002D10C:
    sp -= 0x48;
    sw(s3, sp + 0x2C);
    sw(a1, sp + 0x10);
    s3 = a1 + 4;
    sw(s6, sp + 0x38);
    s6 = 0;                                             // Result = 00000000
    sw(s4, sp + 0x30);
    s4 = 0;                                             // Result = 00000000
    sw(ra, sp + 0x44);
    sw(fp, sp + 0x40);
    sw(s7, sp + 0x3C);
    sw(s5, sp + 0x34);
    sw(s2, sp + 0x28);
    sw(s1, sp + 0x24);
    sw(s0, sp + 0x20);
    s7 = lw(a0);
    fp = a0 + 4;
    if (i32(s7) <= 0) goto loc_8002D36C;
    s1 = fp;
    s5 = 0x1F800000;                                    // Result = 1F800000
loc_8002D164:
    v0 = lw(s5);
    if (v0 != 0) goto loc_8002D190;
    v0 = lw(s1);
    v1 = lw(s1 + 0x4);
    sw(v0, s3);
    sw(v1, s3 + 0x4);
    s3 += 8;
    s4++;                                               // Result = 00000001
    v0 = lw(s5);
loc_8002D190:
    v1 = lw(s5 + 0x4);
    {
        const bool bJump = (v0 != 0);
        v0 = 1;                                         // Result = 00000001
        if (bJump) goto loc_8002D1AC;
    }
    {
        const bool bJump = (v1 != v0);
        v0 = s7 - 1;
        if (bJump) goto loc_8002D358;
    }
    goto loc_8002D1B4;
loc_8002D1AC:
    v0 = s7 - 1;
    if (v1 != 0) goto loc_8002D358;
loc_8002D1B4:
    v0 = (i32(s6) < i32(v0));
    s2 = fp;
    if (v0 == 0) goto loc_8002D1C4;
    s2 = s1 + 8;
loc_8002D1C4:
    v1 = *gNumNewClipVerts;
    a0 = v1 + 1;
    v0 = v1 << 3;
    v0 -= v1;
    v0 <<= 2;
    v1 = gNewClipVerts;
    *gNumNewClipVerts = a0;
    a0 = (i32(a0) < 0x20);
    s0 = v0 + v1;
    if (a0 != 0) goto loc_8002D204;
    I_Error("RightEdgeClip: exceeded max new vertexes\n");
loc_8002D204:
    v1 = lw(s1);
    a0 = lw(s2);
    v0 = lw(v1 + 0xC);
    a1 = lw(v1 + 0x10);
    v1 = lw(a0 + 0xC);
    a0 = lw(a0 + 0x10);
    v0 -= a1;
    v1 -= a0;
    a2 = v0 << 16;
    v0 -= v1;
    div(a2, v0);
    if (v0 != 0) goto loc_8002D23C;
    _break(0x1C00);
loc_8002D23C:
    at = -1;                                            // Result = FFFFFFFF
    {
        const bool bJump = (v0 != at);
        at = 0x80000000;                                // Result = 80000000
        if (bJump) goto loc_8002D254;
    }
    if (a2 != at) goto loc_8002D254;
    tge(zero, zero, 0x5D);
loc_8002D254:
    a2 = lo;
    a0 -= a1;
    mult(a2, a0);
    v0 = lo;
    v0 = u32(i32(v0) >> 16);
    v0 += a1;
    sw(v0, s0 + 0x10);
    sw(v0, s0 + 0xC);
    v0 = lw(s2);
    v1 = lw(s1);
    v0 = lw(v0);
    v1 = lw(v1);
    v0 -= v1;
    v0 = u32(i32(v0) >> 16);
    mult(a2, v0);
    v0 = lo;
    v0 += v1;
    sw(v0, s0);
    v0 = lw(s2);
    v1 = lw(s1);
    v0 = lw(v0 + 0x4);
    a1 = lw(v1 + 0x4);
    v0 -= a1;
    v0 = u32(i32(v0) >> 16);
    mult(a2, v0);
    a0 = lo;
    v0 = lw(s0 + 0x10);
    v1 = 0x800000;                                      // Result = 00800000
    div(v1, v0);
    if (v0 != 0) goto loc_8002D2DC;
    _break(0x1C00);
loc_8002D2DC:
    at = -1;                                            // Result = FFFFFFFF
    {
        const bool bJump = (v0 != at);
        at = 0x80000000;                                // Result = 80000000
        if (bJump) goto loc_8002D2F4;
    }
    if (v1 != at) goto loc_8002D2F4;
    tge(zero, zero, 0x5D);
loc_8002D2F4:
    v1 = lo;
    v0 = lw(s0 + 0xC);
    v1++;
    mult(v1, v0);
    v0 = *gNumFramesDrawn;
    a0 += a1;
    sw(a0, s0 + 0x4);
    sw(v1, s0 + 0x8);
    sw(v0, s0 + 0x18);
    v0 = lo;
    v0 = u32(i32(v0) >> 16);
    v0 += 0x80;
    sw(v0, s0 + 0x14);
    sw(s0, s3);
    v0 = lw(s1 + 0x4);
    s4++;                                               // Result = 00000001
    sw(v0, s3 + 0x4);
    v0 = (i32(s4) < 0x15);                              // Result = 00000001
    s3 += 8;
    if (v0 != 0) goto loc_8002D358;
    I_Error("RightEdgeClip: Point Overflow");
loc_8002D358:
    s1 += 8;
    s6++;
    v0 = (i32(s6) < i32(s7));
    s5 += 4;
    if (v0 != 0) goto loc_8002D164;
loc_8002D36C:
    a3 = lw(sp + 0x10);
    v0 = s4;                                            // Result = 00000000
    sw(s4, a3);
    ra = lw(sp + 0x44);
    fp = lw(sp + 0x40);
    s7 = lw(sp + 0x3C);
    s6 = lw(sp + 0x38);
    s5 = lw(sp + 0x34);
    s4 = lw(sp + 0x30);
    s3 = lw(sp + 0x2C);
    s2 = lw(sp + 0x28);
    s1 = lw(sp + 0x24);
    s0 = lw(sp + 0x20);
    sp += 0x48;
    return;
}

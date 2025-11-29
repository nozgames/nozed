# Knife Cut Actions Plan

This document outlines the methodical approach to implementing knife cuts by first classifying the cut path into distinct actions, then processing each action appropriately.

## Overview

The knife tool records a series of `KnifeCut` points as the user clicks. Each point has:
- `position` - world position of the click
- `vertex_index` - index of existing vertex if clicked on one, else -1
- `edge_index` - index of edge if clicked on one, else -1
- `face_index` - index of face if clicked inside one (and not on vertex/edge), else -1

Before executing any mesh modifications, we need to:
1. Analyze the cut path to find edge intersections between consecutive points
2. Build a complete path with all points (user clicks + edge intersections)
3. Classify the path into discrete **actions**
4. Execute each action in order

---

## Point Types

Each point in the final path can be classified as:

| Type | Description | Detection |
|------|-------------|-----------|
| **Vertex** | Clicked on an existing mesh vertex | `vertex_index >= 0` |
| **Edge** | Clicked on an edge (or path crossed an edge) | `edge_index >= 0` (or detected intersection) |
| **Face** | Clicked inside a face | `face_index >= 0` and not vertex/edge |

---

## Action Types

After building the complete path (user clicks + edge intersections), we segment it into **actions**. Each action is a contiguous run of points that performs a single logical operation.

### 1. Edge Split
**Description:** Adds a new vertex to an edge without splitting any faces. Can occur in two ways:

**Case A - Direct click on edge:**
- Path contains exactly 1 point
- That point is on an edge (not a vertex)

**Case B - Path crosses edge (no boundary anchors):**
- Path has no vertex/edge clicks (start and end are outside mesh or in faces)
- But the path crosses one or more edges
- Each crossing creates an edge split

**Detection:**
- Single edge point with no other boundary points, OR
- Path segment crosses edges but has no boundary anchors on either end

**Result:**
- Create new vertex at edge position (or each crossing position)
- Insert vertex into all faces that share this edge

---

### 2. Face Split (Edge-to-Edge)
**Description:** A cut that starts on one edge/vertex of a face and ends on a different edge/vertex of the same face. Divides the face into two faces.

**Detection:**
- Path segment starts with an edge or vertex point
- Path segment ends with a different edge or vertex point
- Start and end are on the boundary of the same face
- May have internal (face) points between them

**Result:**
- Create vertices for any edge points
- Split the face along the path
- Both resulting faces share the cut vertices

---

### 3. Face Split (Vertex-to-Vertex)
**Description:** Special case of face split where both endpoints are existing vertices of the face.

**Detection:**
- Same as Edge-to-Edge but both endpoints are vertices
- Vertices must be non-adjacent in the face (otherwise no split needed)

**Result:**
- Split face along the path connecting the two vertices
- No new edge vertices needed

---

### 4. Inner Face (Closed Loop)
**Description:** User draws a closed loop entirely inside a face (returns to starting point). Creates a hole/island in the face.

**Detection:**
- First point equals last point (closed loop)
- All points are face points (internal) except possibly the start/end
- No edge crossings in the path

**Result:**
- Create new face from the loop vertices
- Original face gets the loop as an inner boundary (hole)
- OR: Create inner face and stitch to nearest boundary vertex

---

### 5. Inner Slit (Open Chain)
**Description:** User draws an open path entirely inside a face without crossing any edges. Creates a "slit" or "cut" into the face.

**Detection:**
- First point != last point (open path)
- All points are face points (internal)
- No edge crossings

**Result:**
- Create vertices for all points
- Stitch into face: find closest boundary vertex and create a path that goes out and back along the slit

---

### 6. Multi-Face Split
**Description:** Cut path crosses multiple faces, splitting each one.

**Detection:**
- Path crosses 2+ edges that belong to different faces
- Each edge crossing marks a face transition

**Result:**
- Segment the path at each edge crossing
- Process each segment as a Face Split action
- Ensure vertex sharing at the cr
- ossing points

---

### 7. Partial Cuts (Reduced to Edge Split)
**Description:** Click in face → cross edge → click in other face/outside. Since there's no second boundary point on the *same* face, this can't split a face. The only geometry change is the edge crossing.

**Detection:**
- Boundary points exist but they don't share a common face
- OR only one boundary point with face points around it

**Result:**
- Treat edge crossings as EDGE_SPLIT actions
- Ignore the face points (they don't contribute geometry)

---

### 8. No-Op Cases
**Description:** Cases where no mesh modification is needed.

**Detection:**
- Single point on existing vertex (no new geometry)
- Path with only vertex points that are adjacent (already connected)
- Path entirely outside mesh bounds

**Result:**
- Do nothing

---

## Algorithm: Segmenting Path into Actions

```
1. Build complete path:
   - Start with user clicks
   - For each consecutive pair, find edge intersections
   - Insert intersection points in order by parameter t
   - Classify each point as Vertex/Edge/Face

2. Find boundary points (Vertex or Edge points):
   - These are "anchors" that divide the path into segments

3. Segment the path:
   - If no boundary points: entire path is Inner Face or Inner Slit
   - If 1 boundary point: Partial Cut or Edge Split
   - If 2+ boundary points: split at each boundary point

4. Classify each segment:
   - Check start/end point types
   - Check if closed loop
   - Match to action type from table above

5. Build action list with:
   - Action type
   - Involved face(s)
   - Vertex indices (or positions for new vertices)
   - Edge information for splits
```

---

## Implementation Steps

### Phase 1: Path Building
- [x] Collect user clicks (already done in UpdateKnifeTool)
- [ ] Find all edge intersections between consecutive clicks
- [ ] Build ordered path with all points
- [ ] Classify each point (Vertex/Edge/Face)

### Phase 2: Action Segmentation
- [ ] Identify all boundary points in path
- [ ] Segment path into runs between boundary points
- [ ] Classify each segment as an action type
- [ ] Handle special cases (loops, single points)

### Phase 3: Action Execution
- [ ] Implement Edge Split
- [ ] Implement Face Split (Edge-to-Edge)
- [ ] Implement Face Split (Vertex-to-Vertex)
- [ ] Implement Inner Face (Closed Loop)
- [ ] Implement Inner Slit
- [ ] Implement Partial Cut
- [ ] Implement Multi-Face Split

### Phase 4: Cleanup
- [ ] Rebuild edges after all modifications
- [ ] Validate mesh integrity
- [ ] Mark mesh as dirty/modified

---

## Data Structures

```cpp
enum KnifePointType {
    KNIFE_POINT_VERTEX,    // On existing vertex
    KNIFE_POINT_EDGE,      // On edge (new or intersection)
    KNIFE_POINT_FACE       // Inside face
};

enum KnifeActionType {
    KNIFE_ACTION_NONE,
    KNIFE_ACTION_EDGE_SPLIT,
    KNIFE_ACTION_FACE_SPLIT,
    KNIFE_ACTION_INNER_FACE,
    KNIFE_ACTION_INNER_SLIT
};

struct KnifePathPoint {
    Vec2 position;
    KnifePointType type;
    int index;           // vertex_index, edge_index, or face_index depending on type
    int edge_v0, edge_v1; // if type == EDGE, the edge endpoints
    float edge_t;         // parameter along edge (0-1)
};

struct KnifeAction {
    KnifeActionType type;
    int start_index;     // index into path array
    int end_index;       // index into path array (inclusive)
    int face_index;      // primary face this action affects
    bool is_closed;      // true if this is a closed loop
};
```

---

## Edge Cases to Consider

1. **Clicking same point twice** - Should be treated as single point
2. **Very short segments** - Points too close together should be merged
3. **Crossing at vertex** - Path might pass through existing vertex
4. **Tangent edges** - Path might be parallel/tangent to edge
5. **Multiple edge crossings on same segment** - Sort by t parameter
6. **Degenerate faces** - Resulting faces with < 3 vertices
7. **Self-intersecting paths** - Path crosses itself
8. **Clicks outside mesh** - Points outside should be ignored, but edge crossings between them still count
9. **Face-to-face across edge** - Click in one face, cross edge, click in adjacent face = edge split only

---

## Next Steps

1. Implement `BuildKnifePath()` - converts cuts to ordered path with intersections
2. Implement `SegmentPathIntoActions()` - classifies path segments
3. Implement each action type handler
4. Test with simple cases first (single face splits)

//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

constexpr float KNIFE_HIT_TOLERANCE = 0.25f;

struct KnifePoint {
    Vec2 position;
    int vertex_index;
    int face_index;
    int edge_index;
};

struct KnifeCut {
    Vec2 position;
    int vertex_index;
    int face_index;
    int edge_index;
};

struct KnifeTool {
    KnifeCut cuts[256];
    int cut_count = 0;
    MeshData* mesh;
    Vec2 vertices[MAX_VERTICES];
    int vertex_count;
};

enum KnifePointType {
    KNIFE_POINT_NONE,      // Outside mesh / invalid
    KNIFE_POINT_VERTEX,    // On existing vertex
    KNIFE_POINT_EDGE,      // On edge (clicked or intersection)
    KNIFE_POINT_FACE,      // Inside face
    KNIFE_POINT_CLOSE      // Closing the loop (back to first point)
};

enum KnifeActionType {
    KNIFE_ACTION_NONE,
    KNIFE_ACTION_EDGE_SPLIT,      // Just split edge(s), no face split
    KNIFE_ACTION_FACE_SPLIT,      // Split face from boundary to boundary
    KNIFE_ACTION_INNER_FACE,      // Closed loop inside face (hole)
    KNIFE_ACTION_INNER_SLIT       // Open path inside face
};

struct KnifePathPoint {
    Vec2 position;
    KnifePointType type;
    int vertex_index;     // if type == VERTEX
    int face_index;       // if type == FACE
    int edge_v0, edge_v1; // if type == EDGE, the edge endpoints
    float edge_t;         // parameter along edge (0-1)
    float path_t;         // parameter along segment for sorting intersections
};

struct KnifeAction {
    KnifeActionType type;
    int start_index;     // index into path array
    int end_index;       // index into path array (inclusive)
    int face_index;      // primary face this action affects
};

static KnifeTool g_knife_tool = {};

static void EnsureEdgeVertexInFace(MeshData* m, int face_index, KnifePathPoint& pt);

static int FindFaceContainingPoint(MeshData* m, const Vec2& point) {
    for (int face_index = 0; face_index < m->face_count; face_index++) {
        FaceData& f = m->faces[face_index];

        // Build polygon from face vertices and test point containment
        bool inside = false;
        for (int i = 0, j = f.vertex_count - 1; i < f.vertex_count; j = i++) {
            Vec2 vi = m->vertices[f.vertices[i]].position;
            Vec2 vj = m->vertices[f.vertices[j]].position;

            if (((vi.y > point.y) != (vj.y > point.y)) &&
                (point.x < (vj.x - vi.x) * (point.y - vi.y) / (vj.y - vi.y) + vi.x)) {
                inside = !inside;
            }
        }

        if (inside)
            return face_index;
    }
    return -1;
}

static int FindVertexInFace(const FaceData& f, int vertex_index) {
    for (int i = 0; i < f.vertex_count; i++)
        if (f.vertices[i] == vertex_index)
            return i;

    return -1;
}

static int FindVertexAtPosition(MeshData* m, const Vec2& position, float tolerance = 0.001f) {
    for (int i = 0; i < m->vertex_count; i++)
        if (Length(m->vertices[i].position - position) < tolerance)
            return i;

    return -1;
}

static int AddKnifeVertex(MeshData* m, const Vec2& position) {
    int existing = FindVertexAtPosition(m, position);
    if (existing != -1)
        return existing;

    if (m->vertex_count >= MAX_VERTICES)
        return -1;

    int index = m->vertex_count++;
    VertexData& v = m->vertices[index];
    v.position = position;
    v.edge_normal = VEC2_ZERO;
    v.edge_size = 1.0f;
    v.selected = false;
    v.ref_count = 0;
    v.gradient = 0.0f;
    return index;
}

static bool InsertVertexInFace(MeshData* m, int face_index, int insert_pos, int vertex_index, bool allow_duplicates = false) {
    FaceData& f = m->faces[face_index];

    if (!allow_duplicates && FindVertexInFace(f, vertex_index) != -1)
        return false;

    if (f.vertex_count >= MAX_FACE_VERTICES)
        return false;

    for (int i = f.vertex_count; i > insert_pos; i--)
        f.vertices[i] = f.vertices[i - 1];

    f.vertices[insert_pos] = vertex_index;
    f.vertex_count++;
    return true;
}

static int SplitFaceAtPositions(MeshData* m, int face_index, int pos0, int pos1, const int* cut_vertices, int cut_vertex_count) {
    if (m->face_count >= MAX_FACES)
        return -1;

    FaceData& old_face = m->faces[face_index];

    if (pos0 < 0 || pos0 >= old_face.vertex_count || pos1 < 0 || pos1 >= old_face.vertex_count)
        return -1;

    // Copy the old face vertices
    int old_vertices[MAX_FACE_VERTICES];
    int old_count = old_face.vertex_count;
    for (int i = 0; i < old_count; i++)
        old_vertices[i] = old_face.vertices[i];

    // Create new face
    FaceData& new_face = m->faces[m->face_count];
    new_face.color = old_face.color;
    new_face.gradient_color = old_face.gradient_color;
    new_face.gradient_dir = old_face.gradient_dir;
    new_face.gradient_offset = old_face.gradient_offset;
    new_face.normal = old_face.normal;
    new_face.selected = false;
    new_face.vertex_count = 0;

    // New face: from pos0 to pos1 (forward), then cut vertices reversed
    int dist_forward = (pos1 - pos0 + old_count) % old_count;
    for (int i = 0; i <= dist_forward; i++) {
        new_face.vertices[new_face.vertex_count++] = old_vertices[(pos0 + i) % old_count];
    }
    for (int i = cut_vertex_count - 1; i >= 0; i--) {
        new_face.vertices[new_face.vertex_count++] = cut_vertices[i];
    }

    // Old face: from pos1 to pos0 (forward, which is backward from original), then cut vertices forward
    old_face.vertex_count = 0;
    int dist_backward = (pos0 - pos1 + old_count) % old_count;
    for (int i = 0; i <= dist_backward; i++) {
        old_face.vertices[old_face.vertex_count++] = old_vertices[(pos1 + i) % old_count];
    }
    for (int i = 0; i < cut_vertex_count; i++) {
        old_face.vertices[old_face.vertex_count++] = cut_vertices[i];
    }

    return m->face_count++;
}

// Count how many edges are shared between two faces
static int CountSharedEdges(MeshData* m, int face_index0, int face_index1) {
    // Ensure face_index0 < face_index1 for consistent edge lookup
    if (face_index0 > face_index1) {
        int tmp = face_index0;
        face_index0 = face_index1;
        face_index1 = tmp;
    }

    int shared_edge_count = 0;
    for (int edge_index = 0; edge_index < m->edge_count; edge_index++) {
        EdgeData& ee = m->edges[edge_index];
        if (ee.face_count != 2)
            continue;

        if (ee.face_index[0] == face_index0 && ee.face_index[1] == face_index1)
            shared_edge_count++;
    }

    return shared_edge_count;
}

// Find all faces that contain an edge
static int GetFacesWithEdge(MeshData* m, int v0, int v1, int faces[2]) {
    int count = 0;
    for (int fi = 0; fi < m->face_count && count < 2; fi++) {
        FaceData& f = m->faces[fi];
        for (int vi = 0; vi < f.vertex_count; vi++) {
            int fv0 = f.vertices[vi];
            int fv1 = f.vertices[(vi + 1) % f.vertex_count];
            if ((fv0 == v0 && fv1 == v1) || (fv0 == v1 && fv1 == v0)) {
                faces[count++] = fi;
                break;
            }
        }
    }
    return count;
}

static const char* GetPointTypeName(KnifePointType type) {
    switch (type) {
        case KNIFE_POINT_NONE:   return "NONE";
        case KNIFE_POINT_VERTEX: return "VERTEX";
        case KNIFE_POINT_EDGE:   return "EDGE";
        case KNIFE_POINT_FACE:   return "FACE";
        case KNIFE_POINT_CLOSE:  return "CLOSE";
    }
    return "???";
}

static const char* GetActionTypeName(KnifeActionType type) {
    switch (type) {
        case KNIFE_ACTION_NONE:        return "NONE";
        case KNIFE_ACTION_EDGE_SPLIT:  return "EDGE_SPLIT";
        case KNIFE_ACTION_FACE_SPLIT:  return "FACE_SPLIT";
        case KNIFE_ACTION_INNER_FACE:  return "INNER_FACE";
        case KNIFE_ACTION_INNER_SLIT:  return "INNER_SLIT";
    }
    return "???";
}

static int BuildKnifePath(MeshData* m, KnifePathPoint* path) {
    int path_count = 0;

    for (int cut_i = 0; cut_i < g_knife_tool.cut_count; cut_i++) {
        KnifeCut& cut = g_knife_tool.cuts[cut_i];

        // Add intersection points from previous cut to this one
        if (cut_i > 0) {
            Vec2 seg_start = g_knife_tool.cuts[cut_i - 1].position;
            Vec2 seg_end = cut.position;

            // Collect edge intersections
            struct EdgeHit {
                Vec2 position;
                int edge_v0, edge_v1;
                float t;
            };
            EdgeHit hits[64];
            int hit_count = 0;

            for (int edge_i = 0; edge_i < m->edge_count; edge_i++) {
                EdgeData& e = m->edges[edge_i];
                Vec2 ev0 = m->vertices[e.v0].position;
                Vec2 ev1 = m->vertices[e.v1].position;

                Vec2 intersection;
                if (!OverlapLine(seg_start, seg_end, ev0, ev1, &intersection))
                    continue;

                // Calculate t along segment
                Vec2 seg_dir = seg_end - seg_start;
                float seg_len = Length(seg_dir);
                if (seg_len < F32_EPSILON) continue;

                float t = Length(intersection - seg_start) / seg_len;

                // Skip if too close to segment endpoints (will be added as click points)
                if (t < 0.01f || t > 0.99f)
                    continue;

                // Calculate t along edge
                Vec2 edge_dir = ev1 - ev0;
                float edge_len = Length(edge_dir);
                float edge_t = (edge_len > F32_EPSILON) ? Length(intersection - ev0) / edge_len : 0.0f;

                // Skip if at edge endpoints (would be a vertex hit)
                if (edge_t < 0.01f || edge_t > 0.99f)
                    continue;

                hits[hit_count++] = { intersection, e.v0, e.v1, t };
            }

            // Sort hits by t
            for (int i = 0; i < hit_count - 1; i++) {
                for (int j = i + 1; j < hit_count; j++) {
                    if (hits[j].t < hits[i].t) {
                        EdgeHit temp = hits[i];
                        hits[i] = hits[j];
                        hits[j] = temp;
                    }
                }
            }

            // Add hits to path
            for (int i = 0; i < hit_count; i++) {
                path[path_count++] = {
                    .position = hits[i].position,
                    .type = KNIFE_POINT_EDGE,
                    .vertex_index = -1,
                    .face_index = -1,
                    .edge_v0 = hits[i].edge_v0,
                    .edge_v1 = hits[i].edge_v1,
                    .edge_t = 0,
                    .path_t = hits[i].t
                };
            }
        }

        // Add the click point itself
        KnifePathPoint pp = {};
        pp.position = cut.position;
        pp.vertex_index = -1;
        pp.face_index = -1;
        pp.edge_v0 = -1;
        pp.edge_v1 = -1;

        if (cut.vertex_index == -2) {
            // Special marker for closing the loop
            pp.type = KNIFE_POINT_CLOSE;
            pp.face_index = cut.face_index;
            if (cut.edge_index >= 0) {
                EdgeData& e = m->edges[cut.edge_index];
                pp.edge_v0 = e.v0;
                pp.edge_v1 = e.v1;
            }
        } else if (cut.vertex_index >= 0) {
            pp.type = KNIFE_POINT_VERTEX;
            pp.vertex_index = cut.vertex_index;
        } else if (cut.edge_index >= 0) {
            pp.type = KNIFE_POINT_EDGE;
            EdgeData& e = m->edges[cut.edge_index];
            pp.edge_v0 = e.v0;
            pp.edge_v1 = e.v1;
        } else if (cut.face_index >= 0) {
            pp.type = KNIFE_POINT_FACE;
            pp.face_index = cut.face_index;
        } else {
            // HitTestFace may have failed - try FindFaceContainingPoint as fallback
            int face = FindFaceContainingPoint(m, cut.position);
            if (face >= 0) {
                pp.type = KNIFE_POINT_FACE;
                pp.face_index = face;
            } else {
                pp.type = KNIFE_POINT_NONE;
            }
        }

        path[path_count++] = pp;
    }

    return path_count;
}

static int FindBoundaryPoints(KnifePathPoint* path, int path_count, int* boundary_indices) {
    int boundary_count = 0;
    for (int i = 0; i < path_count; i++) {
        KnifePointType type = path[i].type;
        if (type == KNIFE_POINT_VERTEX || type == KNIFE_POINT_EDGE) {
            boundary_indices[boundary_count++] = i;
        } else if (type == KNIFE_POINT_CLOSE && path[i].edge_v0 >= 0) {
            // CLOSE point that was on an edge is also a boundary
            boundary_indices[boundary_count++] = i;
        }
    }
    return boundary_count;
}

static int FindCommonFace(MeshData* m, KnifePathPoint* path, int start_bi, int end_bi) {
    KnifePathPoint& start_pt = path[start_bi];
    KnifePathPoint& end_pt = path[end_bi];

    // Check internal points first
    for (int j = start_bi + 1; j < end_bi; j++) {
        if (path[j].type == KNIFE_POINT_FACE) {
            return path[j].face_index;
        }
    }

    // Get faces for start point
    int start_faces[8];
    int start_face_count = 0;

    if (start_pt.type == KNIFE_POINT_VERTEX) {
        for (int fi = 0; fi < m->face_count && start_face_count < 8; fi++) {
            if (FindVertexInFace(m->faces[fi], start_pt.vertex_index) != -1) {
                start_faces[start_face_count++] = fi;
            }
        }
    } else if (start_pt.type == KNIFE_POINT_EDGE) {
        start_face_count = GetFacesWithEdge(m, start_pt.edge_v0, start_pt.edge_v1, start_faces);
    }

    // Find which start face also contains end point
    for (int k = 0; k < start_face_count; k++) {
        int fi = start_faces[k];
        FaceData& f = m->faces[fi];

        if (end_pt.type == KNIFE_POINT_VERTEX) {
            if (FindVertexInFace(f, end_pt.vertex_index) != -1) {
                return fi;
            }
        } else if (end_pt.type == KNIFE_POINT_EDGE) {
            for (int vi = 0; vi < f.vertex_count; vi++) {
                int fv0 = f.vertices[vi];
                int fv1 = f.vertices[(vi + 1) % f.vertex_count];
                if ((fv0 == end_pt.edge_v0 && fv1 == end_pt.edge_v1) ||
                    (fv0 == end_pt.edge_v1 && fv1 == end_pt.edge_v0)) {
                    return fi;
                }
            }
        }
    }

    return -1;
}

static bool IsEdgePoint(KnifePathPoint& pt) {
    return pt.type == KNIFE_POINT_EDGE ||
           (pt.type == KNIFE_POINT_CLOSE && pt.edge_v0 >= 0);
}

static bool IsSameEdge(KnifePathPoint& a, KnifePathPoint& b) {
    if (!IsEdgePoint(a) || !IsEdgePoint(b))
        return false;

    return (a.edge_v0 == b.edge_v0 && a.edge_v1 == b.edge_v1) ||
           (a.edge_v0 == b.edge_v1 && a.edge_v1 == b.edge_v0);
}

static KnifeActionType DetermineActionType(KnifePathPoint* path, int start_bi, int end_bi) {
    KnifePathPoint& start_pt = path[start_bi];
    KnifePathPoint& end_pt = path[end_bi];

    // Check if end is CLOSE and same position as start (single point loop)
    if (end_pt.type == KNIFE_POINT_CLOSE &&
        Length(start_pt.position - end_pt.position) < 0.01f) {
        // Single point closed loop - edge split if on edge, otherwise nothing
        if (IsEdgePoint(start_pt)) {
            return KNIFE_ACTION_EDGE_SPLIT;
        }
        return KNIFE_ACTION_NONE;
    }

    // Check if start and end are on the same edge
    bool same_edge = IsSameEdge(start_pt, end_pt);

    if (!same_edge) {
        return KNIFE_ACTION_FACE_SPLIT;
    }

    // Same edge - check if there are face points between
    for (int j = start_bi + 1; j < end_bi; j++) {
        if (path[j].type == KNIFE_POINT_FACE) {
            // Enter and exit same edge with face points in between = inner slit (pocket)
            return KNIFE_ACTION_INNER_SLIT;
        }
    }

    // Same edge, no face points = edge split
    return KNIFE_ACTION_EDGE_SPLIT;
}

static int BuildActionsNoBoundary(KnifePathPoint* path, int path_count, KnifeAction* actions) {
    if (path_count == 0)
        return 0;

    // Find which face contains this path
    int face = -1;
    for (int i = 0; i < path_count && face == -1; i++) {
        if (path[i].type == KNIFE_POINT_FACE || path[i].type == KNIFE_POINT_CLOSE) {
            face = path[i].face_index;
        }
    }

    if (face < 0)
        return 0;

    // Check if this is a closed loop (last point is CLOSE type)
    bool is_closed_loop = path_count >= 3 && path[path_count-1].type == KNIFE_POINT_CLOSE;

    actions[0] = {
        .type = is_closed_loop ? KNIFE_ACTION_INNER_FACE : KNIFE_ACTION_INNER_SLIT,
        .start_index = 0,
        .end_index = path_count - 1,
        .face_index = face
    };
    return 1;
}

static int BuildActionsSingleBoundary(KnifePathPoint* path, int boundary_index, KnifeAction* actions) {
    if (path[boundary_index].type != KNIFE_POINT_EDGE)
        return 0;

    actions[0] = {
        .type = KNIFE_ACTION_EDGE_SPLIT,
        .start_index = boundary_index,
        .end_index = boundary_index,
        .face_index = -1
    };
    return 1;
}

static int BuildActionsMultipleBoundary(MeshData* m, KnifePathPoint* path, int path_count, int* boundary_indices, int boundary_count, KnifeAction* actions) {
    int action_count = 0;

    for (int i = 0; i < boundary_count - 1; i++) {
        int start_bi = boundary_indices[i];
        int end_bi = boundary_indices[i + 1];

        int face = FindCommonFace(m, path, start_bi, end_bi);
        KnifeActionType action_type = DetermineActionType(path, start_bi, end_bi);

        if (action_type == KNIFE_ACTION_EDGE_SPLIT) {
            // Same edge, no face points between = the points are just edge crossings
            // Only create edge split actions for points that aren't used by adjacent non-edge-split actions

            // Check if start and end are the same point (closed loop on single point)
            bool same_point = Length(path[start_bi].position - path[end_bi].position) < 0.01f;

            // Check if start point is used by previous action
            bool start_used = (i > 0) &&
                (DetermineActionType(path, boundary_indices[i-1], start_bi) != KNIFE_ACTION_EDGE_SPLIT);

            // Check if end point is used by next action
            bool end_used = (i < boundary_count - 2) &&
                (DetermineActionType(path, end_bi, boundary_indices[i+2]) != KNIFE_ACTION_EDGE_SPLIT);

            if (!start_used) {
                actions[action_count++] = {
                    .type = KNIFE_ACTION_EDGE_SPLIT,
                    .start_index = start_bi,
                    .end_index = start_bi,
                    .face_index = face
                };
            }

            // Don't add end point if it's the same as start point
            if (!same_point && !end_used) {
                actions[action_count++] = {
                    .type = KNIFE_ACTION_EDGE_SPLIT,
                    .start_index = end_bi,
                    .end_index = end_bi,
                    .face_index = face
                };
            }
        } else if (action_type != KNIFE_ACTION_NONE) {
            actions[action_count++] = {
                .type = action_type,
                .start_index = start_bi,
                .end_index = end_bi,
                .face_index = face
            };
        }
    }

    // Check for face points after the last boundary (inner slit trailing into a face)
    int last_bi = boundary_indices[boundary_count - 1];
    for (int i = last_bi + 1; i < path_count; i++) {
        if (path[i].type == KNIFE_POINT_FACE) {
            // Found face points after last boundary - create inner slit
            int face = path[i].face_index;
            actions[action_count++] = {
                .type = KNIFE_ACTION_INNER_SLIT,
                .start_index = last_bi,
                .end_index = path_count - 1,
                .face_index = face
            };
            break;
        }
    }

    return action_count;
}

static int BuildKnifeActions(MeshData* m, KnifePathPoint* path, int path_count, KnifeAction* actions) {
    int boundary_indices[256];
    int boundary_count = FindBoundaryPoints(path, path_count, boundary_indices);

    LogInfo("Found %d boundary points:", boundary_count);
    for (int i = 0; i < boundary_count; i++) {
        int bi = boundary_indices[i];
        LogInfo("  boundary[%d] = path[%d] type=%s", i, bi, GetPointTypeName(path[bi].type));
    }

    if (boundary_count == 0)
        return BuildActionsNoBoundary(path, path_count, actions);

    if (boundary_count == 1)
        return BuildActionsSingleBoundary(path, boundary_indices[0], actions);

    return BuildActionsMultipleBoundary(m, path, path_count, boundary_indices, boundary_count, actions);
}

static void LogKnifePath(MeshData* m, KnifePathPoint* path, int path_count) {
    LogInfo("=== Knife Path (%d points) ===", path_count);
    for (int i = 0; i < path_count; i++) {
        KnifePathPoint& pp = path[i];
        if (pp.type == KNIFE_POINT_EDGE || (pp.type == KNIFE_POINT_CLOSE && pp.edge_v0 >= 0)) {
            int faces[2];
            int face_count = GetFacesWithEdge(m, pp.edge_v0, pp.edge_v1, faces);
            if (face_count == 2) {
                LogInfo("  [%d] %s at (%.3f, %.3f) edge %d-%d faces [%d, %d]",
                    i, GetPointTypeName(pp.type), pp.position.x, pp.position.y,
                    pp.edge_v0, pp.edge_v1, faces[0], faces[1]);
            } else if (face_count == 1) {
                LogInfo("  [%d] %s at (%.3f, %.3f) edge %d-%d faces [%d]",
                    i, GetPointTypeName(pp.type), pp.position.x, pp.position.y,
                    pp.edge_v0, pp.edge_v1, faces[0]);
            } else {
                LogInfo("  [%d] %s at (%.3f, %.3f) edge %d-%d faces []",
                    i, GetPointTypeName(pp.type), pp.position.x, pp.position.y,
                    pp.edge_v0, pp.edge_v1);
            }
        } else if (pp.type == KNIFE_POINT_VERTEX) {
            LogInfo("  [%d] %s at (%.3f, %.3f) vertex %d",
                i, GetPointTypeName(pp.type), pp.position.x, pp.position.y,
                pp.vertex_index);
        } else if (pp.type == KNIFE_POINT_FACE) {
            LogInfo("  [%d] %s at (%.3f, %.3f) face %d",
                i, GetPointTypeName(pp.type), pp.position.x, pp.position.y,
                pp.face_index);
        } else {
            LogInfo("  [%d] %s at (%.3f, %.3f)",
                i, GetPointTypeName(pp.type), pp.position.x, pp.position.y);
        }
    }
}

static void LogKnifeActions(KnifeAction* actions, int action_count) {
    LogInfo("=== Knife Actions (%d) ===", action_count);
    for (int i = 0; i < action_count; i++) {
        KnifeAction& a = actions[i];
        LogInfo("  [%d] %s: path[%d..%d] face=%d",
            i, GetActionTypeName(a.type), a.start_index, a.end_index, a.face_index);
    }
}

static void ExecuteEdgeSplit(MeshData* m, KnifePathPoint* path, KnifeAction& action) {
    KnifePathPoint& pt = path[action.start_index];

    if (pt.type != KNIFE_POINT_EDGE)
        return;

    // Create the new vertex
    int new_vertex = AddKnifeVertex(m, pt.position);
    if (new_vertex < 0)
        return;

    // Find all faces that contain this edge and insert the vertex
    int faces[2];
    int face_count = GetFacesWithEdge(m, pt.edge_v0, pt.edge_v1, faces);

    for (int i = 0; i < face_count; i++) {
        FaceData& f = m->faces[faces[i]];

        // Find the edge in this face
        for (int vi = 0; vi < f.vertex_count; vi++) {
            int fv0 = f.vertices[vi];
            int fv1 = f.vertices[(vi + 1) % f.vertex_count];

            if ((fv0 == pt.edge_v0 && fv1 == pt.edge_v1) ||
                (fv0 == pt.edge_v1 && fv1 == pt.edge_v0)) {
                // Insert new vertex after fv0 (between fv0 and fv1)
                InsertVertexInFace(m, faces[i], vi + 1, new_vertex);
                break;
            }
        }
    }

    // Store the vertex index back in the path point for use by other actions
    pt.vertex_index = new_vertex;
}

static int GetOrCreateVertex(MeshData* m, KnifePathPoint& pt) {
    // If it's already a vertex, just return it
    if (pt.type == KNIFE_POINT_VERTEX)
        return pt.vertex_index;

    // If we already created a vertex for this point, return it
    if (pt.vertex_index >= 0)
        return pt.vertex_index;

    // Create a new vertex
    int new_vertex = AddKnifeVertex(m, pt.position);
    pt.vertex_index = new_vertex;
    return new_vertex;
}

static void EnsureEdgeVertexInAllFaces(MeshData* m, KnifePathPoint& pt) {
    if (!IsEdgePoint(pt))
        return;

    int vertex = pt.vertex_index;
    if (vertex < 0)
        return;

    // Find all faces that contain this edge (or a sub-edge of it)
    // We check all faces since the original edge may have been subdivided
    for (int face_index = 0; face_index < m->face_count; face_index++) {
        EnsureEdgeVertexInFace(m, face_index, pt);
    }
}

void EnsureEdgeVertexInFace(MeshData* m, int face_index, KnifePathPoint& pt) {
    if (!IsEdgePoint(pt))
        return;

    FaceData& f = m->faces[face_index];
    int vertex = pt.vertex_index;

    // Check if already in face
    if (FindVertexInFace(f, vertex) != -1)
        return;

    // Find the edge and insert
    // The edge might be the original (v0-v1) or a sub-edge if another vertex was already inserted
    for (int vi = 0; vi < f.vertex_count; vi++) {
        int fv0 = f.vertices[vi];
        int fv1 = f.vertices[(vi + 1) % f.vertex_count];

        // Check if this is the original edge
        if ((fv0 == pt.edge_v0 && fv1 == pt.edge_v1) ||
            (fv0 == pt.edge_v1 && fv1 == pt.edge_v0)) {
            InsertVertexInFace(m, face_index, vi + 1, vertex);
            return;
        }

        // Check if this is a sub-edge (one endpoint matches, and the vertex is on this segment)
        bool has_v0 = (fv0 == pt.edge_v0 || fv1 == pt.edge_v0);
        bool has_v1 = (fv0 == pt.edge_v1 || fv1 == pt.edge_v1);
        if (has_v0 || has_v1) {
            // Check if the point lies on this edge segment
            Vec2 p0 = m->vertices[fv0].position;
            Vec2 p1 = m->vertices[fv1].position;
            Vec2 edge_dir = p1 - p0;
            float edge_len = Length(edge_dir);
            if (edge_len < F32_EPSILON) continue;

            Vec2 to_pt = pt.position - p0;
            float proj = Dot(to_pt, edge_dir / edge_len);

            // Check if projection is within edge and point is close to edge
            if (proj > 0.0f && proj < edge_len) {
                Vec2 closest = p0 + (edge_dir / edge_len) * proj;
                if (Length(pt.position - closest) < 0.01f) {
                    InsertVertexInFace(m, face_index, vi + 1, vertex);
                    return;
                }
            }
        }
    }
}

static void ExecuteFaceSplit(MeshData* m, KnifePathPoint* path, KnifeAction& action) {
    if (action.face_index < 0)
        return;

    KnifePathPoint& start_pt = path[action.start_index];
    KnifePathPoint& end_pt = path[action.end_index];

    // Handle closed loop with same start/end point
    if (action.start_index == action.end_index ||
        Length(start_pt.position - end_pt.position) < 0.01f) {
        // Same point - only do edge split if it's an edge point
        if (IsEdgePoint(start_pt)) {
            int v = GetOrCreateVertex(m, start_pt);
            if (v >= 0) {
                EnsureEdgeVertexInAllFaces(m, start_pt);
            }
        }
        // Face or vertex points are ignored
        return;
    }

    // Get or create vertices for start and end
    int v0 = GetOrCreateVertex(m, start_pt);
    int v1 = GetOrCreateVertex(m, end_pt);

    if (v0 < 0 || v1 < 0)
        return;

    // If start/end are on edges, insert into target face and any truly adjacent face.
    // A face is truly adjacent if it shares ONLY this edge with the target face.
    // If it shares more edges (like an inner face shares all slit edges), skip it.
    if (IsEdgePoint(start_pt)) {
        int faces[2];
        int face_count = GetFacesWithEdge(m, start_pt.edge_v0, start_pt.edge_v1, faces);
        for (int i = 0; i < face_count; i++) {
            if (faces[i] == action.face_index) {
                EnsureEdgeVertexInFace(m, faces[i], start_pt);
            } else {
                // Check if this face is truly adjacent (shares only this edge, not multiple)
                int shared = CountSharedEdges(m, action.face_index, faces[i]);
                if (shared == 1) {
                    EnsureEdgeVertexInFace(m, faces[i], start_pt);
                }
            }
        }
    }
    if (IsEdgePoint(end_pt)) {
        int faces[2];
        int face_count = GetFacesWithEdge(m, end_pt.edge_v0, end_pt.edge_v1, faces);
        for (int i = 0; i < face_count; i++) {
            if (faces[i] == action.face_index) {
                EnsureEdgeVertexInFace(m, faces[i], end_pt);
            } else {
                int shared = CountSharedEdges(m, action.face_index, faces[i]);
                if (shared == 1) {
                    EnsureEdgeVertexInFace(m, faces[i], end_pt);
                }
            }
        }
    }

    // Collect internal vertices (face points between start and end)
    int cut_vertices[128];
    int cut_count = 0;

    for (int i = action.start_index + 1; i < action.end_index; i++) {
        KnifePathPoint& pt = path[i];
        if (pt.type == KNIFE_POINT_FACE) {
            int v = GetOrCreateVertex(m, pt);
            if (v >= 0) {
                cut_vertices[cut_count++] = v;
            }
        }
    }

    // Find all positions of v0 and v1 in the face (they may appear multiple times in slit faces)
    FaceData& f = m->faces[action.face_index];
    int v0_positions[8];
    int v0_pos_count = 0;
    int v1_positions[8];
    int v1_pos_count = 0;

    for (int i = 0; i < f.vertex_count; i++) {
        if (f.vertices[i] == v0 && v0_pos_count < 8)
            v0_positions[v0_pos_count++] = i;
        if (f.vertices[i] == v1 && v1_pos_count < 8)
            v1_positions[v1_pos_count++] = i;
    }

    if (v0_pos_count == 0 || v1_pos_count == 0)
        return;

    // Check if this face has a slit (any vertex appears more than once)
    bool has_slit = false;
    for (int i = 0; i < f.vertex_count && !has_slit; i++) {
        for (int j = i + 1; j < f.vertex_count; j++) {
            if (f.vertices[i] == f.vertices[j]) {
                has_slit = true;
                break;
            }
        }
    }

    int best_pos0 = v0_positions[0];
    int best_pos1 = v1_positions[0];

    if (has_slit) {
        // For slit faces, find positions where going BACKWARD gives the shorter path
        // This keeps the outer boundary together
        int best_back_dist = (best_pos0 - best_pos1 + f.vertex_count) % f.vertex_count;

        for (int i = 0; i < v0_pos_count; i++) {
            for (int j = 0; j < v1_pos_count; j++) {
                int back_dist = (v0_positions[i] - v1_positions[j] + f.vertex_count) % f.vertex_count;
                if (back_dist > 0 && back_dist < best_back_dist) {
                    best_back_dist = back_dist;
                    best_pos0 = v0_positions[i];
                    best_pos1 = v1_positions[j];
                }
            }
        }
    } else {
        // For normal faces, find the pair with shortest forward distance
        int best_dist = (best_pos1 - best_pos0 + f.vertex_count) % f.vertex_count;

        for (int i = 0; i < v0_pos_count; i++) {
            for (int j = 0; j < v1_pos_count; j++) {
                int dist = (v1_positions[j] - v0_positions[i] + f.vertex_count) % f.vertex_count;
                if (dist > 0 && dist < best_dist) {
                    best_dist = dist;
                    best_pos0 = v0_positions[i];
                    best_pos1 = v1_positions[j];
                }
            }
        }
    }

    LogInfo("Split face %d: v0=%d (pos %d), v1=%d (pos %d), has_slit=%d",
        action.face_index, v0, best_pos0, v1, best_pos1, has_slit ? 1 : 0);

    // Split the face using positions
    SplitFaceAtPositions(m, action.face_index, best_pos0, best_pos1, cut_vertices, cut_count);
}

static void ExecuteInnerFace(MeshData* m, KnifePathPoint* path, KnifeAction& action) {
    if (action.face_index < 0)
        return;

    FaceData& f = m->faces[action.face_index];

    // Collect all vertices in the loop first
    int loop_vertices[128];
    int loop_count = 0;

    for (int i = action.start_index; i <= action.end_index; i++) {
        KnifePathPoint& pt = path[i];
        // Skip the closing CLOSE point if it's the same as the start
        if (i == action.end_index && pt.type == KNIFE_POINT_CLOSE)
            continue;

        int v = GetOrCreateVertex(m, pt);
        if (v >= 0) {
            loop_vertices[loop_count++] = v;
        }
    }

    if (loop_count < 3)
        return;

    // Find the closest boundary vertex to any point in the loop
    int closest_boundary_v = -1;
    int closest_loop_idx = 0;
    float min_dist = 1e30f;

    for (int fi = 0; fi < f.vertex_count; fi++) {
        int vi = f.vertices[fi];
        Vec2 boundary_pos = m->vertices[vi].position;

        for (int li = 0; li < loop_count; li++) {
            float d = Length(m->vertices[loop_vertices[li]].position - boundary_pos);
            if (d < min_dist) {
                min_dist = d;
                closest_boundary_v = vi;
                closest_loop_idx = li;
            }
        }
    }

    if (closest_boundary_v < 0)
        return;

    // Find position of boundary vertex in face
    int boundary_pos_in_face = FindVertexInFace(f, closest_boundary_v);
    if (boundary_pos_in_face < 0)
        return;

    // Create two faces:
    // 1. Outer face: original boundary with slit to inner loop (goes around loop in one direction)
    // 2. Inner face: the loop itself (goes around in opposite direction)

    // First, create the inner face (the cut-out piece)
    // The inner face winds opposite to the outer, so we go backwards around the loop
    if (m->face_count >= MAX_FACES)
        return;

    FaceData& inner_face = m->faces[m->face_count];
    inner_face.color = f.color;
    inner_face.gradient_color = f.gradient_color;
    inner_face.gradient_dir = f.gradient_dir;
    inner_face.gradient_offset = f.gradient_offset;
    inner_face.normal = f.normal;
    inner_face.selected = false;
    inner_face.vertex_count = 0;

    // Inner face winds in forward order (same as original loop)
    for (int i = 0; i < loop_count; i++) {
        int idx = (closest_loop_idx + i) % loop_count;
        inner_face.vertices[inner_face.vertex_count++] = loop_vertices[idx];
    }
    m->face_count++;

    // Now rebuild the outer face with the slit:
    // ... -> boundary_v -> loop[closest] -> loop[closest-1] -> ... -> loop[closest] -> boundary_v -> ...
    // The inner loop goes in REVERSE to create a proper hole (winding cancels out)
    int new_vertices[MAX_FACE_VERTICES];
    int new_count = 0;

    // Add vertices up to and including boundary vertex
    for (int i = 0; i <= boundary_pos_in_face; i++) {
        new_vertices[new_count++] = f.vertices[i];
    }

    // Add the loop starting from closest_loop_idx, going BACKWARDS around to it
    // This creates the proper winding for a hole
    for (int i = 0; i <= loop_count; i++) {
        int idx = (closest_loop_idx - i + loop_count) % loop_count;
        new_vertices[new_count++] = loop_vertices[idx];
    }

    // Return to boundary vertex (the bridge back)
    new_vertices[new_count++] = closest_boundary_v;

    // Add remaining boundary vertices after boundary_v
    for (int i = boundary_pos_in_face + 1; i < f.vertex_count; i++) {
        new_vertices[new_count++] = f.vertices[i];
    }

    // Update the outer face
    f.vertex_count = new_count;
    for (int i = 0; i < new_count; i++) {
        f.vertices[i] = new_vertices[i];
    }
}

static void ExecuteInnerSlit(MeshData* m, KnifePathPoint* path, KnifeAction& action) {
    if (action.face_index < 0)
        return;

    KnifePathPoint& start_pt = path[action.start_index];
    KnifePathPoint& end_pt = path[action.end_index];

    // Both start and end should be on edges (same edge for inner slit)
    if (!IsEdgePoint(start_pt) || !IsEdgePoint(end_pt))
        return;

    // Get or create vertices for start and end
    int v0 = GetOrCreateVertex(m, start_pt);
    int v1 = GetOrCreateVertex(m, end_pt);

    if (v0 < 0 || v1 < 0)
        return;

    // Insert edge vertices into the target face
    EnsureEdgeVertexInFace(m, action.face_index, start_pt);
    EnsureEdgeVertexInFace(m, action.face_index, end_pt);

    // Collect internal vertices (face points between start and end)
    int cut_vertices[128];
    int cut_count = 0;

    for (int i = action.start_index + 1; i < action.end_index; i++) {
        KnifePathPoint& pt = path[i];
        if (pt.type == KNIFE_POINT_FACE) {
            int v = GetOrCreateVertex(m, pt);
            if (v >= 0) {
                cut_vertices[cut_count++] = v;
            }
        }
    }

    // Now use face split logic: split from v0 to v1 along the cut vertices
    // This creates two faces:
    // 1. The inner triangle/notch (v0 -> cut_vertices -> v1)
    // 2. The outer face with the notch cut out

    FaceData& f = m->faces[action.face_index];

    // Find positions of v0 and v1 in the face
    // For inner slit, they should be adjacent on the same edge, so we need to find
    // the pair where v0 comes right before v1 (or vice versa)
    int pos0 = -1, pos1 = -1;

    for (int i = 0; i < f.vertex_count; i++) {
        if (f.vertices[i] == v0) {
            int next = (i + 1) % f.vertex_count;
            if (f.vertices[next] == v1) {
                pos0 = i;
                pos1 = next;
                break;
            }
        }
        if (f.vertices[i] == v1) {
            int next = (i + 1) % f.vertex_count;
            if (f.vertices[next] == v0) {
                pos0 = next;
                pos1 = i;
                break;
            }
        }
    }

    if (pos0 < 0 || pos1 < 0) {
        LogInfo("ExecuteInnerSlit: Could not find adjacent v0=%d, v1=%d in face %d", v0, v1, action.face_index);
        return;
    }

    LogInfo("ExecuteInnerSlit: v0=%d (pos %d), v1=%d (pos %d), cut_count=%d", v0, pos0, v1, pos1, cut_count);

    // Split the face using positions
    SplitFaceAtPositions(m, action.face_index, pos0, pos1, cut_vertices, cut_count);
}

static void ExecuteKnifeActions(MeshData* m, KnifePathPoint* path, int path_count, KnifeAction* actions, int action_count) {
    (void)path_count;

    for (int i = 0; i < action_count; i++) {
        KnifeAction& action = actions[i];

        switch (action.type) {
            case KNIFE_ACTION_EDGE_SPLIT:
                ExecuteEdgeSplit(m, path, action);
                break;

            case KNIFE_ACTION_FACE_SPLIT:
                ExecuteFaceSplit(m, path, action);
                break;

            case KNIFE_ACTION_INNER_FACE:
                ExecuteInnerFace(m, path, action);
                break;

            case KNIFE_ACTION_INNER_SLIT:
                ExecuteInnerSlit(m, path, action);
                break;

            case KNIFE_ACTION_NONE:
                break;
        }
    }
}

static void LogMesh(MeshData* m, const char* label) {
    LogInfo("=== Mesh %s ===", label);
    LogInfo("Vertices (%d):", m->vertex_count);
    for (int i = 0; i < m->vertex_count; i++) {
        LogInfo("  [%d] (%.3f, %.3f)", i, m->vertices[i].position.x, m->vertices[i].position.y);
    }
    LogInfo("Faces (%d):", m->face_count);
    for (int i = 0; i < m->face_count; i++) {
        FaceData& f = m->faces[i];
        char buf[256] = {};
        int len = 0;
        for (int j = 0; j < f.vertex_count; j++) {
            len += snprintf(buf + len, sizeof(buf) - len, "%d ", f.vertices[j]);
        }
        LogInfo("  [%d] verts: %s", i, buf);
    }
    LogInfo("Edges (%d):", m->edge_count);
    for (int i = 0; i < m->edge_count; i++) {
        EdgeData& e = m->edges[i];
        char buf[64] = {};
        int len = 0;
        for (int j = 0; j < e.face_count; j++) {
            len += snprintf(buf + len, sizeof(buf) - len, "%d ", e.face_index[j]);
        }
        LogInfo("  [%d] %d-%d faces: %s", i, e.v0, e.v1, buf);
    }
}

static void CommitKnifeCuts() {
    MeshData* m = g_knife_tool.mesh;
    if (g_knife_tool.cut_count < 1)
        return;

    // Phase 1: Build complete path with edge intersections
    KnifePathPoint path[512];
    int path_count = BuildKnifePath(m, path);
    LogKnifePath(m, path, path_count);

    // Phase 2: Segment path into actions
    KnifeAction actions[128];
    int action_count = BuildKnifeActions(m, path, path_count, actions);
    LogKnifeActions(actions, action_count);

    LogMesh(m, "BEFORE");

    // Phase 3: Execute actions
    ExecuteKnifeActions(m, path, path_count, actions, action_count);

    UpdateEdges(m);
    MarkDirty(m);

    LogMesh(m, "AFTER");
}

static void EndKnifeTool(bool commit) {
    if (commit) {
        LogInfo("%d knife cuts to commit:", g_knife_tool.cut_count);
        for (int i=0; i<g_knife_tool.cut_count; i++) {
            LogInfo("v %d: (%.3f, %.3f)", i,
                g_knife_tool.cuts[i].position.x,
                g_knife_tool.cuts[i].position.y);
        }

        RecordUndo(g_knife_tool.mesh);
        CommitKnifeCuts();
        //CommitKnifeCutsNew();
        MarkModified(g_knife_tool.mesh);
    }

    g_knife_tool.mesh = nullptr;
    EndTool();
}

static void DrawKnifeTool() {
    BindColor(COLOR_VERTEX_SELECTED);
    for (int i=0; i<g_knife_tool.cut_count-1; i++) {
        DrawDashedLine(
            g_knife_tool.cuts[i].position + g_knife_tool.mesh->position,
            g_knife_tool.cuts[i+1].position + g_knife_tool.mesh->position);
    }

    for (int i=0; i<g_knife_tool.cut_count; i++) {
        DrawVertex(g_knife_tool.cuts[i].position + g_knife_tool.mesh->position);
    }

    BindColor(COLOR_GREEN);
    for (int i=0; i<g_knife_tool.vertex_count; i++) {
        DrawVertex(g_knife_tool.vertices[i] + g_knife_tool.mesh->position);
    }
}

static void UpdateKnifeTool() {
    if (WasButtonPressed(KEY_ESCAPE)) {
        EndKnifeTool(false);
        return;
    }

    if (WasButtonPressed(KEY_ENTER)) {
        EndKnifeTool(true);
        return;
    }

    if (WasButtonPressed(MOUSE_LEFT)) {
        int vertex_index = HitTestVertex(g_knife_tool.mesh, g_view.mouse_world_position, KNIFE_HIT_TOLERANCE);
        float edge_hit = 0.0f;
        int edge_index = vertex_index == -1
            ? HitTestEdge(g_knife_tool.mesh, g_view.mouse_world_position, &edge_hit, KNIFE_HIT_TOLERANCE)
            : -1;
        int face_index = (vertex_index == -1 && edge_index == -1)
            ? HitTestFace(g_knife_tool.mesh, Translate(g_knife_tool.mesh->position), g_view.mouse_world_position)
            : -1;

        // Check if clicking on the start point to close the loop
        if (g_knife_tool.cut_count >= 1 &&
            HitTestVertex(g_knife_tool.cuts[0].position + g_knife_tool.mesh->position, g_view.mouse_world_position, 1.0f)) {
            // Add the closing cut point with CLOSE marker
            g_knife_tool.cuts[g_knife_tool.cut_count++] = {
                .position = g_knife_tool.cuts[0].position,
                .vertex_index = -2, // Special marker for CLOSE
                .face_index = g_knife_tool.cuts[0].face_index,
                .edge_index = g_knife_tool.cuts[0].edge_index,
            };
            // Auto-commit
            EndKnifeTool(true);
            return;
        }

        // Check if clicking on any other existing cut point (reject duplicates)
        for (int i = 1; i < g_knife_tool.cut_count; i++) {
            if (HitTestVertex(g_knife_tool.cuts[i].position + g_knife_tool.mesh->position, g_view.mouse_world_position, 1.0f)) {
                // Ignore duplicate click
                return;
            }
        }

        Vec2 position = g_view.mouse_world_position - g_knife_tool.mesh->position;
        if (vertex_index != -1)
            position = GetVertexPoint(g_knife_tool.mesh, vertex_index);
        else if (edge_index != -1)
            position = GetEdgePoint(g_knife_tool.mesh, edge_index, edge_hit);

        g_knife_tool.cuts[g_knife_tool.cut_count++] = {
            .position = position,
            .vertex_index = vertex_index,
            .face_index = face_index,
            .edge_index = edge_index,
        };

        if (edge_index != -1 || vertex_index != -1)
            g_knife_tool.vertices[g_knife_tool.vertex_count++] = position;

        // if the new cut crosses any edges, create vertices at the intersections
        if (g_knife_tool.cut_count <= 1)
            return;

        for (int i=0; i<g_knife_tool.mesh->edge_count; i++) {
            EdgeData& e = g_knife_tool.mesh->edges[i];
            Vec2 v0 = g_knife_tool.mesh->vertices[e.v0].position;
            Vec2 v1 = g_knife_tool.mesh->vertices[e.v1].position;
            Vec2 intersection;
            if (!OverlapLine(
                g_knife_tool.cuts[g_knife_tool.cut_count-2].position,
                g_knife_tool.cuts[g_knife_tool.cut_count-1].position,
                v0,
                v1,
                &intersection))
                continue;

            g_knife_tool.vertices[g_knife_tool.vertex_count++] = intersection;
        }

        return;
    }
}

void BeginKnifeTool(MeshData* mesh) {
    static ToolVtable vtable = {
        .update = UpdateKnifeTool,
        .draw = DrawKnifeTool
    };

    BeginTool({
        .type = TOOL_TYPE_SELECT,
        .vtable = vtable,
        .input = g_view.input_tool,
        .hide_selected = true
    });

    g_knife_tool.mesh = mesh;
    g_knife_tool.cut_count = 0;
    g_knife_tool.vertex_count = 0;

    SetCursor(SYSTEM_CURSOR_SELECT);
}

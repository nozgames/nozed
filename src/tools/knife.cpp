//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

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

// Point along a knife cut segment - either an existing vertex, edge intersection, or face point
struct KnifeCutPoint {
    Vec2 position;
    int vertex_index;     // -1 if not on an existing vertex
    int edge_index;       // -1 if not on an edge
    int face_index;       // face this point is in/on
    float edge_t;         // parameter along edge (0-1) if on edge
};

struct KnifeTool {
    KnifeCut cuts[256];
    int cut_count = 0;
    MeshData* mesh;
    Vec2 vertices[MAX_VERTICES];
    int vertex_count;
};

static KnifeTool g_knife_tool = {};

// Find which face a point lies in (or -1 if none)
int FindFaceContainingPoint(MeshData* m, const Vec2& point) {
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

// Find vertex index in face's vertex list, or -1 if not found
int FindVertexInFace(const FaceData& f, int vertex_index) {
    for (int i = 0; i < f.vertex_count; i++) {
        if (f.vertices[i] == vertex_index)
            return i;
    }
    return -1;
}

// Find an existing vertex at position, or return -1
int FindVertexAtPosition(MeshData* m, const Vec2& position, float tolerance = 0.001f) {
    for (int i = 0; i < m->vertex_count; i++) {
        if (Length(m->vertices[i].position - position) < tolerance)
            return i;
    }
    return -1;
}

// Add a new vertex to the mesh and return its index, or return existing vertex if one exists at position
int AddKnifeVertex(MeshData* m, const Vec2& position) {
    // First check if there's already a vertex at this position
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

// Insert a vertex into a face at the given position in the vertex list
// Returns true if inserted, false if vertex was already in face
bool InsertVertexInFace(MeshData* m, int face_index, int insert_pos, int vertex_index, bool allow_duplicates = false) {
    FaceData& f = m->faces[face_index];

    // Check if vertex is already in face
    if (!allow_duplicates && FindVertexInFace(f, vertex_index) != -1)
        return false;

    if (f.vertex_count >= MAX_FACE_VERTICES)
        return false;

    // Shift vertices after insert_pos
    for (int i = f.vertex_count; i > insert_pos; i--)
        f.vertices[i] = f.vertices[i - 1];

    f.vertices[insert_pos] = vertex_index;
    f.vertex_count++;
    return true;
}

// Split a face along a cut line defined by two vertex indices on the face boundary
// and a list of internal vertices that lie along the cut.
// v0 and v1 must be vertices currently in the face (on the boundary).
// cut_vertices is a list of vertices between v0 and v1 (exclusive).
int SplitFaceAlongVertices(MeshData* m, int face_index, int v0, int v1, const int* cut_vertices, int cut_vertex_count) {
    if (m->face_count >= MAX_FACES)
        return -1;

    FaceData& old_face = m->faces[face_index];

    int pos0 = FindVertexInFace(old_face, v0);
    int pos1 = FindVertexInFace(old_face, v1);

    if (pos0 == -1 || pos1 == -1)
        return -1;

    // Copy the old face vertices
    int old_vertices[MAX_FACE_VERTICES];
    int old_count = old_face.vertex_count;
    for (int i = 0; i < old_count; i++)
        old_vertices[i] = old_face.vertices[i];

    // Create new face (inner/right face)
    // Vertices: v0 -> ... -> v1 (along face boundary) -> cut_vertices (reversed) -> v0
    FaceData& new_face = m->faces[m->face_count];
    new_face.color = old_face.color;
    new_face.gradient_color = old_face.gradient_color;
    new_face.gradient_dir = old_face.gradient_dir;
    new_face.gradient_offset = old_face.gradient_offset;
    new_face.normal = old_face.normal;
    new_face.selected = false;
    new_face.vertex_count = 0;

    // Add boundary vertices from v0 to v1
    int dist_forward = (pos1 - pos0 + old_count) % old_count;
    
    // Special case for detached hole: v0==v1, cut starts away from v0
    bool is_detached_hole = (v0 == v1 && cut_vertex_count > 0 && cut_vertices[0] != v0);

    if (!is_detached_hole) {
        for (int i = 0; i <= dist_forward; i++) {
            new_face.vertices[new_face.vertex_count++] = old_vertices[(pos0 + i) % old_count];
        }
    }

    // Add cut vertices in reverse order (from v1 back to v0)
    int stop_index = (is_detached_hole && cut_vertices[0] == cut_vertices[cut_vertex_count - 1]) ? 1 : 0;
    for (int i = cut_vertex_count - 1; i >= stop_index; i--) {
        new_face.vertices[new_face.vertex_count++] = cut_vertices[i];
    }

    // Update old face (outer/left face)
    // Vertices: v1 -> ... -> v0 (along face boundary) -> cut_vertices (forward) -> v1
    old_face.vertex_count = 0;
    int dist_backward = (pos0 == pos1) ? old_count : (pos0 - pos1 + old_count) % old_count;
    for (int i = 0; i <= dist_backward; i++) {
        old_face.vertices[old_face.vertex_count++] = old_vertices[(pos1 + i) % old_count];
    }

    // Add cut vertices in forward order (from v0 to v1)
    for (int i = 0; i < cut_vertex_count; i++) {
        old_face.vertices[old_face.vertex_count++] = cut_vertices[i];
    }

    return m->face_count++;
}

// A cut point is either a vertex, a point on an edge, or an edge intersection
struct CutPoint {
    Vec2 position;
    int vertex_index;     // >= 0 if this is an existing vertex
    int edge_v0, edge_v1; // edge vertices if on an edge (both -1 if existing vertex)
    float t;              // parameter along cut segment (0 = start, 1 = end)
};

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

// Find the edge that contains a point (by position), returns edge vertices in out_v0/out_v1
// Returns true if found
bool FindEdgeContainingPoint(MeshData* m, const Vec2& point, float tolerance, int* out_v0, int* out_v1) {
    for (int fi = 0; fi < m->face_count; fi++) {
        FaceData& f = m->faces[fi];
        for (int vi = 0; vi < f.vertex_count; vi++) {
            int fv0 = f.vertices[vi];
            int fv1 = f.vertices[(vi + 1) % f.vertex_count];
            Vec2 p0 = m->vertices[fv0].position;
            Vec2 p1 = m->vertices[fv1].position;

            // Check if point is on this edge
            Vec2 edge_dir = p1 - p0;
            float edge_len = Length(edge_dir);
            if (edge_len < F32_EPSILON)
                continue;

            edge_dir = edge_dir / edge_len;
            Vec2 to_point = point - p0;
            float proj = Dot(to_point, edge_dir);

            // Check if projection is within edge bounds
            if (proj < -tolerance || proj > edge_len + tolerance)
                continue;

            // Check distance from edge
            Vec2 closest = p0 + edge_dir * Clamp(proj, 0.0f, edge_len);
            if (Length(point - closest) < tolerance) {
                *out_v0 = fv0;
                *out_v1 = fv1;
                return true;
            }
        }
    }
    return false;
}

#if 0
static void CommitKnifeCuts(MeshData* m) {
    if (g_knife_tool.cut_count < 1)
        return;

    // PHASE 1: Build the complete cut path with all vertices in order
    // Include: user click points (if on mesh) and edge intersections

    struct PathPoint {
        Vec2 position;
        int original_vertex;   // -1 if new vertex needed
        int edge_v0, edge_v1;  // Original edge vertices (-1 if not on edge)
        bool on_edge;
        bool on_vertex;
        bool internal;
        int vertex_index;      // Will be filled in after creating vertices
    };

    PathPoint path[512];
    int path_length = 0;

    if (g_knife_tool.cut_count == 1) {
        // Single point cut
        Vec2 pos = g_knife_tool.cuts[0].position;
        PathPoint pp = {};
        pp.position = pos;
        pp.original_vertex = FindVertexAtPosition(m, pos);
        pp.on_vertex = (pp.original_vertex != -1);
        pp.on_edge = false;
        pp.internal = false;
        pp.edge_v0 = -1;
        pp.edge_v1 = -1;

        if (!pp.on_vertex) {
            pp.on_edge = FindEdgeContainingPoint(m, pos, 0.01f, &pp.edge_v0, &pp.edge_v1);
        }
        if (!pp.on_vertex && !pp.on_edge) {
            pp.internal = (FindFaceContainingPoint(m, pos) != -1);
        }

        if (pp.on_vertex || pp.on_edge || pp.internal) {
            path[path_length++] = pp;
        }
    } else {
        // Process each segment
        for (int seg = 0; seg < g_knife_tool.cut_count - 1; seg++) {
            Vec2 seg_start = g_knife_tool.cuts[seg].position;
            Vec2 seg_end = g_knife_tool.cuts[seg + 1].position;

            // Add start point of segment (only for first segment, or if it's on the mesh)
            if (seg == 0 || path_length == 0) {
                PathPoint pp = {};
                pp.position = seg_start;
                pp.original_vertex = FindVertexAtPosition(m, seg_start);
                pp.on_vertex = (pp.original_vertex != -1);
                pp.on_edge = false;
                pp.internal = false;
                pp.edge_v0 = -1;
                pp.edge_v1 = -1;

                if (!pp.on_vertex) {
                    pp.on_edge = FindEdgeContainingPoint(m, seg_start, 0.01f, &pp.edge_v0, &pp.edge_v1);
                }
                if (!pp.on_vertex && !pp.on_edge) {
                    pp.internal = (FindFaceContainingPoint(m, seg_start) != -1);
                }

                if (pp.on_vertex || pp.on_edge || pp.internal) {
                    path[path_length++] = pp;
                }
            }

            // Collect edge intersections for this segment
            struct EdgeHit {
                Vec2 position;
                int edge_v0, edge_v1;
                float t;
            };
            EdgeHit edge_hits[64];
            int edge_hit_count = 0;

            for (int edge_i = 0; edge_i < m->edge_count; edge_i++) {
                EdgeData& e = m->edges[edge_i];
                Vec2 ev0 = m->vertices[e.v0].position;
                Vec2 ev1 = m->vertices[e.v1].position;

                Vec2 intersection_point;
                if (!OverlapLine(seg_start, seg_end, ev0, ev1, &intersection_point))
                    continue;

                // Skip if at segment endpoints
                if (Length(intersection_point - seg_start) < 0.01f ||
                    Length(intersection_point - seg_end) < 0.01f)
                    continue;

                // Calculate t along segment
                Vec2 seg_dir = seg_end - seg_start;
                float seg_len = Length(seg_dir);
                float t = (seg_len > F32_EPSILON) ? Length(intersection_point - seg_start) / seg_len : 0.0f;

                // Skip if too close to endpoints
                if (t < 0.01f || t > 0.99f)
                    continue;

                // Calculate t along edge to check if at edge endpoints
                Vec2 edge_dir = ev1 - ev0;
                float edge_len = Length(edge_dir);
                float edge_t = (edge_len > F32_EPSILON) ? Length(intersection_point - ev0) / edge_len : 0.0f;

                // Skip if at edge endpoints
                if (edge_t < 0.01f || edge_t > 0.99f)
                    continue;

                edge_hits[edge_hit_count++] = {
                    .position = intersection_point,
                    .edge_v0 = e.v0,
                    .edge_v1 = e.v1,
                    .t = t
                };
            }

            // Sort edge hits by t
            for (int i = 0; i < edge_hit_count - 1; i++) {
                for (int j = i + 1; j < edge_hit_count; j++) {
                    if (edge_hits[j].t < edge_hits[i].t) {
                        EdgeHit temp = edge_hits[i];
                        edge_hits[i] = edge_hits[j];
                        edge_hits[j] = temp;
                    }
                }
            }

            // Add edge intersections to path
            for (int i = 0; i < edge_hit_count; i++) {
                PathPoint pp = {};
                pp.position = edge_hits[i].position;
                pp.original_vertex = -1;
                pp.edge_v0 = edge_hits[i].edge_v0;
                pp.edge_v1 = edge_hits[i].edge_v1;
                pp.on_edge = true;
                pp.on_vertex = false;
                pp.internal = false;
                path[path_length++] = pp;
            }

            // Add end point of segment
            PathPoint pp = {};
            pp.position = seg_end;
            pp.original_vertex = FindVertexAtPosition(m, seg_end);
            pp.on_vertex = (pp.original_vertex != -1);
            pp.on_edge = false;
            pp.internal = false;
            pp.edge_v0 = -1;
            pp.edge_v1 = -1;

            if (!pp.on_vertex) {
                pp.on_edge = FindEdgeContainingPoint(m, seg_end, 0.01f, &pp.edge_v0, &pp.edge_v1);
            }
            if (!pp.on_vertex && !pp.on_edge) {
                pp.internal = (FindFaceContainingPoint(m, seg_end) != -1);
            }

            if (pp.on_vertex || pp.on_edge || pp.internal) {
                // Check if we already have this point (from previous edge intersection)
                bool duplicate = false;
                if (path_length > 0 && Length(path[path_length-1].position - pp.position) < 0.01f) {
                    duplicate = true;
                }
                if (!duplicate) {
                    path[path_length++] = pp;
                }
            }
        }
    }

    if (path_length < 1)
        return;

    // PHASE 2: Create all vertices (but skip unused internal points)
    // First, determine if we have boundary points
    int first_boundary = -1;
    int last_boundary = -1;
    bool has_any_boundary = false;
    
    for (int i = 0; i < path_length; i++) {
        if (path[i].on_vertex || path[i].on_edge) {
            has_any_boundary = true;
            if (first_boundary == -1) first_boundary = i;
            last_boundary = i;
        }
    }
    
    for (int i = 0; i < path_length; i++) {
        PathPoint& pp = path[i];
        
        // Skip internal points that are before first boundary or after last boundary
        if (pp.internal && has_any_boundary) {
            if (i < first_boundary || i > last_boundary) {
                pp.vertex_index = -1; // Mark as skipped
                continue;
            }
        }
        
        if (pp.on_vertex) {
            pp.vertex_index = pp.original_vertex;
        } else {
            pp.vertex_index = AddKnifeVertex(m, pp.position);
        }
    }

    // PHASE 3: Process cuts - find runs between boundary points (vertices or edges)
    int i = 0;
    // Find first boundary point
    while (i < path_length && !path[i].on_vertex && !path[i].on_edge) i++;

    while (i < path_length) {
        int start_idx = i;

        // Find next boundary point
        int end_idx = i + 1;
        while (end_idx < path_length && !path[end_idx].on_vertex && !path[end_idx].on_edge) {
            end_idx++;
        }

        if (end_idx >= path_length) break;

        // We have a run from start_idx to end_idx
        int v0 = path[start_idx].vertex_index;
        int v1 = path[end_idx].vertex_index;

        // Skip if start and end are the same vertex
        if (v0 == v1) {
            i = end_idx;
            continue;
        }

        // Identify the common face
        int target_face = -1;

        // Collect candidate faces from start point
        int start_faces[8];
        int start_face_count = 0;
        if (path[start_idx].on_vertex) {
            // Find all faces containing this vertex
            for (int fi = 0; fi < m->face_count; fi++) {
                if (FindVertexInFace(m->faces[fi], v0) != -1) {
                    start_faces[start_face_count++] = fi;
                    if (start_face_count >= 8) break;
                }
            }
        } else { // on_edge
            int ev0 = path[start_idx].edge_v0;
            int ev1 = path[start_idx].edge_v1;
            start_face_count = GetFacesWithEdge(m, ev0, ev1, start_faces);
        }

        // Check which of these faces also contains the end point
        for (int k = 0; k < start_face_count; k++) {
            int fi = start_faces[k];
            FaceData& f = m->faces[fi];
            bool end_in_face = false;

            if (path[end_idx].on_vertex) {
                end_in_face = (FindVertexInFace(f, v1) != -1);
            } else { // on_edge
                int ev0 = path[end_idx].edge_v0;
                int ev1 = path[end_idx].edge_v1;
                // Check if this edge is part of the face
                for (int vi = 0; vi < f.vertex_count; vi++) {
                    int fv0 = f.vertices[vi];
                    int fv1 = f.vertices[(vi + 1) % f.vertex_count];
                    if ((fv0 == ev0 && fv1 == ev1) || (fv0 == ev1 && fv1 == ev0)) {
                        end_in_face = true;
                        break;
                    }
                }
            }

            if (end_in_face) {
                // Verify internal points are inside this face (if any)
                if (end_idx > start_idx + 1) {
                    int mid_idx = (start_idx + end_idx) / 2;
                    if (FindFaceContainingPoint(m, path[mid_idx].position) != fi) {
                        continue; // Not this face
                    }
                }
                target_face = fi;
                break;
            }
        }

        if (target_face != -1) {
            FaceData& f = m->faces[target_face];

            // Prepare insertions
            struct Insertion {
                int vertex_index;
                int edge_index;
                float proj;
            };
            Insertion insertions[2];
            int insertion_count = 0;

            // Check start point
            if (path[start_idx].on_edge && FindVertexInFace(f, v0) == -1) {
                int ev0 = path[start_idx].edge_v0;
                int ev1 = path[start_idx].edge_v1;
                // Find edge index
                for (int vi = 0; vi < f.vertex_count; vi++) {
                    int fv0 = f.vertices[vi];
                    int fv1 = f.vertices[(vi + 1) % f.vertex_count];
                    if ((fv0 == ev0 && fv1 == ev1) || (fv0 == ev1 && fv1 == ev0)) {
                        Vec2 p0 = m->vertices[fv0].position;
                        Vec2 p1 = m->vertices[fv1].position;
                        Vec2 edge_dir = p1 - p0;
                        float len = Length(edge_dir);
                        float proj = (len > F32_EPSILON) ? Dot(path[start_idx].position - p0, edge_dir / len) : 0.0f;
                        
                        insertions[insertion_count++] = { v0, vi, proj };
                        break;
                    }
                }
            }

            // Check end point
            if (path[end_idx].on_edge && FindVertexInFace(f, v1) == -1) {
                int ev0 = path[end_idx].edge_v0;
                int ev1 = path[end_idx].edge_v1;
                // Find edge index
                for (int vi = 0; vi < f.vertex_count; vi++) {
                    int fv0 = f.vertices[vi];
                    int fv1 = f.vertices[(vi + 1) % f.vertex_count];
                    if ((fv0 == ev0 && fv1 == ev1) || (fv0 == ev1 && fv1 == ev0)) {
                         Vec2 p0 = m->vertices[fv0].position;
                        Vec2 p1 = m->vertices[fv1].position;
                        Vec2 edge_dir = p1 - p0;
                        float len = Length(edge_dir);
                        float proj = (len > F32_EPSILON) ? Dot(path[end_idx].position - p0, edge_dir / len) : 0.0f;

                        insertions[insertion_count++] = { v1, vi, proj };
                        break;
                    }
                }
            }

            // Sort insertions: descending edge_index, then descending proj
            if (insertion_count > 1) {
                if (insertions[0].edge_index < insertions[1].edge_index ||
                   (insertions[0].edge_index == insertions[1].edge_index && insertions[0].proj < insertions[1].proj)) {
                    Insertion temp = insertions[0];
                    insertions[0] = insertions[1];
                    insertions[1] = temp;
                }
            }

            // Perform insertions
            for (int k = 0; k < insertion_count; k++) {
                InsertVertexInFace(m, target_face, insertions[k].edge_index + 1, insertions[k].vertex_index);
            }

            // Collect internal vertices
            int internal_vertices[128];
            int internal_count = 0;
            for (int k = start_idx + 1; k < end_idx; k++) {
                internal_vertices[internal_count++] = path[k].vertex_index;
            }

            // Split the face
            SplitFaceAlongVertices(m, target_face, v0, v1, internal_vertices, internal_count);
        }

        i = end_idx;
    }

    // Handle internal cuts (no boundary points)
    bool has_boundary = false;
    for (int j = 0; j < path_length; j++) {
        if (path[j].on_vertex || path[j].on_edge) {
            has_boundary = true;
            break;
        }
    }

    if (!has_boundary && path_length > 1) {
        // Find face containing the cut
        int face_index = FindFaceContainingPoint(m, path[0].position);
        if (face_index != -1) {
            FaceData& f = m->faces[face_index];

            // Find closest boundary vertex to connect to
            int closest_v = -1;
            float min_dist = 1e30f;
            for (int j = 0; j < f.vertex_count; j++) {
                float d = Length(m->vertices[f.vertices[j]].position - path[0].position);
                if (d < min_dist) {
                    min_dist = d;
                    closest_v = f.vertices[j];
                }
            }

            if (closest_v != -1) {
                // Check if it's a closed loop
                bool is_loop = (path[0].vertex_index == path[path_length - 1].vertex_index);

                if (is_loop) {
                    // Closed loop (hole)
                    // Collect loop vertices (including the duplicate end point to close the loop)
                    int loop_vertices[128];
                    int loop_count = 0;
                    for (int j = 0; j < path_length; j++) {
                        loop_vertices[loop_count++] = path[j].vertex_index;
                    }
                    SplitFaceAlongVertices(m, face_index, closest_v, closest_v, loop_vertices, loop_count);
                } else {
                    // Open chain (slit)
                    // Stitch the cut into the face: ... -> closest_v -> path[0] -> ... -> path[last] -> ... -> path[0] -> closest_v -> ...
                    int insert_pos = FindVertexInFace(f, closest_v) + 1;
                    
                    // Insert forward path
                    for (int j = 0; j < path_length; j++) {
                        InsertVertexInFace(m, face_index, insert_pos + j, path[j].vertex_index, true);
                    }
                    
                    // Insert backward path (excluding last point)
                    for (int j = path_length - 2; j >= 0; j--) {
                        InsertVertexInFace(m, face_index, insert_pos + path_length + (path_length - 2 - j), path[j].vertex_index, true);
                    }
                }
            }
        }
    }

    // Handle single edge crossing (just add vertex to edge)
    int edge_count = 0;
    for (int ii = 0; ii < path_length; ii++) {
        if (path[ii].on_edge) edge_count++;
    }

    if (edge_count == 1) {
        for (int ii = 0; ii < path_length; ii++) {
            if (path[ii].on_edge) {
                int ev0, ev1;
                if (FindEdgeContainingPoint(m, path[ii].position, 0.01f, &ev0, &ev1)) {
                    int edge_faces[2];
                    int edge_face_count = GetFacesWithEdge(m, ev0, ev1, edge_faces);
                    for (int fi = 0; fi < edge_face_count; fi++) {
                        FaceData& f = m->faces[edge_faces[fi]];
                        for (int vi = 0; vi < f.vertex_count; vi++) {
                            int fv0 = f.vertices[vi];
                            int fv1 = f.vertices[(vi + 1) % f.vertex_count];
                            if ((fv0 == ev0 && fv1 == ev1) || (fv0 == ev1 && fv1 == ev0)) {
                                InsertVertexInFace(m, edge_faces[fi], vi + 1, path[ii].vertex_index);
                                break;
                            }
                        }
                    }
                }
                break;
            }
        }
    }

    // Rebuild edges
    UpdateEdges(m);
    MarkDirty(m);
}
#else

enum KnifePointType {
    KNIFE_POINT_NONE,      // Outside mesh / invalid
    KNIFE_POINT_VERTEX,    // On existing vertex
    KNIFE_POINT_EDGE,      // On edge (clicked or intersection)
    KNIFE_POINT_FACE       // Inside face
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

static const char* GetPointTypeName(KnifePointType type) {
    switch (type) {
        case KNIFE_POINT_NONE:   return "NONE";
        case KNIFE_POINT_VERTEX: return "VERTEX";
        case KNIFE_POINT_EDGE:   return "EDGE";
        case KNIFE_POINT_FACE:   return "FACE";
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

// Build complete path from knife cuts, including edge intersection points
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

        if (cut.vertex_index >= 0) {
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

// Find all boundary point indices (VERTEX or EDGE) in the path
static int FindBoundaryPoints(KnifePathPoint* path, int path_count, int* boundary_indices) {
    int boundary_count = 0;
    for (int i = 0; i < path_count; i++) {
        if (path[i].type == KNIFE_POINT_VERTEX || path[i].type == KNIFE_POINT_EDGE) {
            boundary_indices[boundary_count++] = i;
        }
    }
    return boundary_count;
}

// Find the common face between two boundary points
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

// Determine action type for a segment between two boundary points
static KnifeActionType DetermineActionType(KnifePathPoint* path, int start_bi, int end_bi) {
    KnifePathPoint& start_pt = path[start_bi];
    KnifePathPoint& end_pt = path[end_bi];

    // Check if start and end are on the same edge
    bool same_edge = false;
    if (start_pt.type == KNIFE_POINT_EDGE && end_pt.type == KNIFE_POINT_EDGE) {
        if ((start_pt.edge_v0 == end_pt.edge_v0 && start_pt.edge_v1 == end_pt.edge_v1) ||
            (start_pt.edge_v0 == end_pt.edge_v1 && start_pt.edge_v1 == end_pt.edge_v0)) {
            same_edge = true;
        }
    }

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

// Handle case with no boundary points (all face/none points)
static int BuildActionsNoBoundary(KnifePathPoint* path, int path_count, KnifeAction* actions) {
    if (path_count == 0)
        return 0;

    // Find which face contains this path
    int face = -1;
    for (int i = 0; i < path_count && face == -1; i++) {
        if (path[i].type == KNIFE_POINT_FACE) {
            face = path[i].face_index;
        }
    }

    if (face < 0)
        return 0;

    // Check if this is a closed loop
    bool is_closed_loop = path_count >= 3 &&
        Length(path[0].position - path[path_count-1].position) < 0.01f;

    actions[0] = {
        .type = is_closed_loop ? KNIFE_ACTION_INNER_FACE : KNIFE_ACTION_INNER_SLIT,
        .start_index = 0,
        .end_index = path_count - 1,
        .face_index = face
    };
    return 1;
}

// Handle case with single boundary point
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

// Handle case with multiple boundary points
static int BuildActionsMultipleBoundary(MeshData* m, KnifePathPoint* path, int* boundary_indices, int boundary_count, KnifeAction* actions) {
    int action_count = 0;

    for (int i = 0; i < boundary_count - 1; i++) {
        int start_bi = boundary_indices[i];
        int end_bi = boundary_indices[i + 1];

        int face = FindCommonFace(m, path, start_bi, end_bi);
        KnifeActionType action_type = DetermineActionType(path, start_bi, end_bi);

        if (action_type == KNIFE_ACTION_EDGE_SPLIT) {
            // Same edge, no face points between = the points are just edge crossings
            // Only create edge split actions for points that aren't used by adjacent non-edge-split actions

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

            if (!end_used) {
                actions[action_count++] = {
                    .type = KNIFE_ACTION_EDGE_SPLIT,
                    .start_index = end_bi,
                    .end_index = end_bi,
                    .face_index = face
                };
            }
        } else {
            actions[action_count++] = {
                .type = action_type,
                .start_index = start_bi,
                .end_index = end_bi,
                .face_index = face
            };
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

    return BuildActionsMultipleBoundary(m, path, boundary_indices, boundary_count, actions);
}

// Log the path for debugging
static void LogKnifePath(KnifePathPoint* path, int path_count) {
    LogInfo("=== Knife Path (%d points) ===", path_count);
    for (int i = 0; i < path_count; i++) {
        KnifePathPoint& pp = path[i];
        if (pp.type == KNIFE_POINT_EDGE) {
            LogInfo("  [%d] %s at (%.3f, %.3f) edge %d-%d",
                i, GetPointTypeName(pp.type), pp.position.x, pp.position.y,
                pp.edge_v0, pp.edge_v1);
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

// Log the actions for debugging
static void LogKnifeActions(KnifeAction* actions, int action_count) {
    LogInfo("=== Knife Actions (%d) ===", action_count);
    for (int i = 0; i < action_count; i++) {
        KnifeAction& a = actions[i];
        LogInfo("  [%d] %s: path[%d..%d] face=%d",
            i, GetActionTypeName(a.type), a.start_index, a.end_index, a.face_index);
    }
}

// Execute the knife actions to modify the mesh
void ExecuteKnifeActions(MeshData* m, KnifePathPoint* path, int path_count, KnifeAction* actions, int action_count) {
    // TODO: implement each action type
    (void)m;
    (void)path;
    (void)path_count;
    (void)actions;
    (void)action_count;
}

static void CommitKnifeCuts() {
    MeshData* m = g_knife_tool.mesh;
    if (g_knife_tool.cut_count < 1)
        return;

    // Phase 1: Build complete path with edge intersections
    KnifePathPoint path[512];
    int path_count = BuildKnifePath(m, path);
    LogKnifePath(path, path_count);

    // Phase 2: Segment path into actions
    KnifeAction actions[128];
    int action_count = BuildKnifeActions(m, path, path_count, actions);
    LogKnifeActions(actions, action_count);

    // Phase 3: Execute actions
    ExecuteKnifeActions(m, path, path_count, actions, action_count);

    UpdateEdges(m);
    MarkDirty(m);
}

#endif

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
    // if (g_select_tool.options.update)
    //     g_select_tool.options.update(g_view.mouse_world_position);

    if (WasButtonPressed(KEY_ESCAPE)) {
        EndKnifeTool(false);
        return;
    }

    if (WasButtonPressed(KEY_ENTER)) {
        EndKnifeTool(true);
        return;
    }

    if (WasButtonPressed(MOUSE_LEFT)) {
        int vertex_index = HitTestVertex(g_knife_tool.mesh, g_view.mouse_world_position);
        float edge_hit = 0.0f;
        int edge_index = vertex_index == -1
            ? HitTestEdge(g_knife_tool.mesh, g_view.mouse_world_position, &edge_hit)
            : -1;
        int face_index = (vertex_index == -1 && edge_index == -1)
            ? HitTestFace(g_knife_tool.mesh, Translate(g_knife_tool.mesh->position), g_view.mouse_world_position)
            : -1;

        // Check if clicking on the start point to close the loop
        if (g_knife_tool.cut_count >= 2 &&
            HitTestVertex(g_knife_tool.cuts[0].position + g_knife_tool.mesh->position, g_view.mouse_world_position, 1.0f)) {
            // Add the closing cut point
            g_knife_tool.cuts[g_knife_tool.cut_count++] = {
                .position = g_knife_tool.cuts[0].position,
                .vertex_index = g_knife_tool.cuts[0].vertex_index,
                .face_index = g_knife_tool.cuts[0].face_index,
                .edge_index = g_knife_tool.cuts[0].edge_index,
            };
            // Auto-commit
            EndKnifeTool(true);
            return;
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

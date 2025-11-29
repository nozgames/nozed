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
#if 0
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

// Find which face an edge belongs to (first one found)
static int FindFaceWithEdge(MeshData* m, int v0, int v1) {
    for (int face_index = 0; face_index < m->face_count; face_index++) {
        FaceData& f = m->faces[face_index];
        for (int i = 0; i < f.vertex_count; i++) {
            int fv0 = f.vertices[i];
            int fv1 = f.vertices[(i + 1) % f.vertex_count];
            if ((fv0 == v0 && fv1 == v1) || (fv0 == v1 && fv1 == v0))
                return face_index;
        }
    }
    return -1;
}
#endif

// Find vertex index in face's vertex list, or -1 if not found
static int FindVertexInFace(const FaceData& f, int vertex_index) {
    for (int i = 0; i < f.vertex_count; i++) {
        if (f.vertices[i] == vertex_index)
            return i;
    }
    return -1;
}

// Find an existing vertex at position, or return -1
static int FindVertexAtPosition(MeshData* m, const Vec2& position, float tolerance = 0.001f) {
    for (int i = 0; i < m->vertex_count; i++) {
        if (Length(m->vertices[i].position - position) < tolerance)
            return i;
    }
    return -1;
}

// Add a new vertex to the mesh and return its index, or return existing vertex if one exists at position
static int AddKnifeVertex(MeshData* m, const Vec2& position) {
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
static bool InsertVertexInFace(MeshData* m, int face_index, int insert_pos, int vertex_index) {
    FaceData& f = m->faces[face_index];

    // Check if vertex is already in face
    if (FindVertexInFace(f, vertex_index) != -1)
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

// Split a face along a cut line defined by two vertex indices already in the face
static void SplitFaceAlongVertices(MeshData* m, int face_index, int v0, int v1) {
    if (m->face_count >= MAX_FACES)
        return;

    FaceData& old_face = m->faces[face_index];

    int pos0 = FindVertexInFace(old_face, v0);
    int pos1 = FindVertexInFace(old_face, v1);

    if (pos0 == -1 || pos1 == -1)
        return;

    // Make sure pos0 < pos1
    if (pos0 > pos1) {
        int temp = pos0; pos0 = pos1; pos1 = temp;
        temp = v0; v0 = v1; v1 = temp;
    }

    // Check if they're adjacent - if so, no split needed
    if (pos1 == pos0 + 1 || (pos0 == 0 && pos1 == old_face.vertex_count - 1))
        return;

    // Copy the old face vertices since we'll be modifying the face
    int old_vertices[MAX_FACE_VERTICES];
    int old_count = old_face.vertex_count;
    for (int i = 0; i < old_count; i++)
        old_vertices[i] = old_face.vertices[i];

    // Create new face from vertices pos0 to pos1 (inclusive)
    // This is one "half" of the split
    FaceData& new_face = m->faces[m->face_count++];
    new_face.color = old_face.color;
    new_face.gradient_color = old_face.gradient_color;
    new_face.gradient_dir = old_face.gradient_dir;
    new_face.gradient_offset = old_face.gradient_offset;
    new_face.normal = old_face.normal;
    new_face.selected = false;

    new_face.vertex_count = 0;
    for (int i = pos0; i <= pos1; i++)
        new_face.vertices[new_face.vertex_count++] = old_vertices[i];

    // Update old face: keep vertices from pos1 to end, then 0 to pos0 (inclusive)
    // This is the other "half" of the split
    old_face.vertex_count = 0;
    for (int i = pos1; i < old_count; i++)
        old_face.vertices[old_face.vertex_count++] = old_vertices[i];
    for (int i = 0; i <= pos0; i++)
        old_face.vertices[old_face.vertex_count++] = old_vertices[i];
}

// A cut point is either a vertex, a point on an edge, or an edge intersection
struct CutPoint {
    Vec2 position;
    int vertex_index;     // >= 0 if this is an existing vertex
    int edge_v0, edge_v1; // edge vertices if on an edge (both -1 if existing vertex)
    float t;              // parameter along cut segment (0 = start, 1 = end)
};

// Find all faces that contain a vertex
static int GetFacesWithVertex(MeshData* m, int vertex_index, int faces[MAX_FACES]) {
    int count = 0;
    for (int fi = 0; fi < m->face_count; fi++) {
        if (FindVertexInFace(m->faces[fi], vertex_index) != -1)
            faces[count++] = fi;
    }
    return count;
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

// Find the edge that contains a point (by position), returns edge vertices in out_v0/out_v1
// Returns true if found
static bool FindEdgeContainingPoint(MeshData* m, const Vec2& point, float tolerance, int* out_v0, int* out_v1) {
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

static void CommitKnifeCuts(MeshData* m) {
    if (g_knife_tool.cut_count < 2)
        return;

    // Process each segment of the knife cut
    for (int seg = 0; seg < g_knife_tool.cut_count - 1; seg++) {
        KnifeCut& cut0 = g_knife_tool.cuts[seg];
        KnifeCut& cut1 = g_knife_tool.cuts[seg + 1];

        // Rebuild edges before processing each segment so we have current edge info
        UpdateEdges(m);

        // Collect all cut points along this segment
        CutPoint cut_points[64];
        int cut_point_count = 0;

        // Add start point if it's on an edge or vertex
        int start_vertex = FindVertexAtPosition(m, cut0.position);
        if (start_vertex != -1) {
            cut_points[cut_point_count++] = {
                .position = cut0.position,
                .vertex_index = start_vertex,
                .edge_v0 = -1,
                .edge_v1 = -1,
                .t = 0.0f
            };
        } else {
            int ev0, ev1;
            if (FindEdgeContainingPoint(m, cut0.position, 0.01f, &ev0, &ev1)) {
                cut_points[cut_point_count++] = {
                    .position = cut0.position,
                    .vertex_index = -1,
                    .edge_v0 = ev0,
                    .edge_v1 = ev1,
                    .t = 0.0f
                };
            }
        }

        // Find all edge intersections along the cut line
        for (int edge_i = 0; edge_i < m->edge_count; edge_i++) {
            EdgeData& e = m->edges[edge_i];
            Vec2 ev0 = m->vertices[e.v0].position;
            Vec2 ev1 = m->vertices[e.v1].position;

            Vec2 intersection_point;
            if (!OverlapLine(cut0.position, cut1.position, ev0, ev1, &intersection_point))
                continue;

            // Calculate t along cut segment
            Vec2 cut_dir = cut1.position - cut0.position;
            float cut_len = Length(cut_dir);
            float t = (cut_len > F32_EPSILON) ? Length(intersection_point - cut0.position) / cut_len : 0.0f;

            // Skip if too close to endpoints (those are handled by cut0/cut1)
            if (t < 0.01f || t > 0.99f)
                continue;

            // Calculate t along edge to check if at edge endpoints
            Vec2 edge_dir = ev1 - ev0;
            float edge_len = Length(edge_dir);
            float edge_t = (edge_len > F32_EPSILON) ? Length(intersection_point - ev0) / edge_len : 0.0f;

            // If at edge endpoint, treat as vertex
            if (edge_t < 0.01f) {
                cut_points[cut_point_count++] = {
                    .position = ev0,
                    .vertex_index = e.v0,
                    .edge_v0 = -1,
                    .edge_v1 = -1,
                    .t = t
                };
            } else if (edge_t > 0.99f) {
                cut_points[cut_point_count++] = {
                    .position = ev1,
                    .vertex_index = e.v1,
                    .edge_v0 = -1,
                    .edge_v1 = -1,
                    .t = t
                };
            } else {
                cut_points[cut_point_count++] = {
                    .position = intersection_point,
                    .vertex_index = -1,
                    .edge_v0 = e.v0,
                    .edge_v1 = e.v1,
                    .t = t
                };
            }
        }

        // Add end point if it's on an edge or vertex
        int end_vertex = FindVertexAtPosition(m, cut1.position);
        if (end_vertex != -1) {
            cut_points[cut_point_count++] = {
                .position = cut1.position,
                .vertex_index = end_vertex,
                .edge_v0 = -1,
                .edge_v1 = -1,
                .t = 1.0f
            };
        } else {
            int ev0, ev1;
            if (FindEdgeContainingPoint(m, cut1.position, 0.01f, &ev0, &ev1)) {
                cut_points[cut_point_count++] = {
                    .position = cut1.position,
                    .vertex_index = -1,
                    .edge_v0 = ev0,
                    .edge_v1 = ev1,
                    .t = 1.0f
                };
            }
        }

        // If only 1 cut point (edge intersection), just add the vertex to the edge
        if (cut_point_count == 1 && cut_points[0].edge_v0 != -1) {
            CutPoint& cp = cut_points[0];
            int new_v = AddKnifeVertex(m, cp.position);
            if (new_v != -1) {
                int edge_faces[2];
                int edge_face_count = GetFacesWithEdge(m, cp.edge_v0, cp.edge_v1, edge_faces);
                for (int fi = 0; fi < edge_face_count; fi++) {
                    FaceData& f = m->faces[edge_faces[fi]];
                    for (int vi = 0; vi < f.vertex_count; vi++) {
                        int fv0 = f.vertices[vi];
                        int fv1 = f.vertices[(vi + 1) % f.vertex_count];
                        if ((fv0 == cp.edge_v0 && fv1 == cp.edge_v1) || (fv0 == cp.edge_v1 && fv1 == cp.edge_v0)) {
                            InsertVertexInFace(m, edge_faces[fi], vi + 1, new_v);
                            break;
                        }
                    }
                }
            }
            continue;
        }

        // Need at least 2 cut points to split a face
        if (cut_point_count < 2)
            continue;

        // Sort cut points by t
        for (int i = 0; i < cut_point_count - 1; i++) {
            for (int j = i + 1; j < cut_point_count; j++) {
                if (cut_points[j].t < cut_points[i].t) {
                    CutPoint temp = cut_points[i];
                    cut_points[i] = cut_points[j];
                    cut_points[j] = temp;
                }
            }
        }

        // Process consecutive pairs of cut points
        for (int pair = 0; pair < cut_point_count - 1; pair++) {
            CutPoint& cp0 = cut_points[pair];
            CutPoint& cp1 = cut_points[pair + 1];

            // Get or create vertices for both cut points
            int v0 = cp0.vertex_index;
            if (v0 == -1)
                v0 = AddKnifeVertex(m, cp0.position);

            int v1 = cp1.vertex_index;
            if (v1 == -1)
                v1 = AddKnifeVertex(m, cp1.position);

            if (v0 == -1 || v1 == -1 || v0 == v1)
                continue;

            // Find faces that contain both points
            // For edge points, insert vertex into face first
            int faces0[MAX_FACES], faces1[MAX_FACES];
            int face_count0, face_count1;

            // For edge points, find current edge containing the point and insert vertex
            if (cp0.edge_v0 != -1) {
                int ev0, ev1;
                if (FindEdgeContainingPoint(m, cp0.position, 0.01f, &ev0, &ev1)) {
                    int edge_faces[2];
                    int edge_face_count = GetFacesWithEdge(m, ev0, ev1, edge_faces);
                    for (int fi = 0; fi < edge_face_count; fi++) {
                        FaceData& f = m->faces[edge_faces[fi]];
                        for (int vi = 0; vi < f.vertex_count; vi++) {
                            int fv0 = f.vertices[vi];
                            int fv1 = f.vertices[(vi + 1) % f.vertex_count];
                            if ((fv0 == ev0 && fv1 == ev1) || (fv0 == ev1 && fv1 == ev0)) {
                                InsertVertexInFace(m, edge_faces[fi], vi + 1, v0);
                                break;
                            }
                        }
                    }
                }
            }

            if (cp1.edge_v0 != -1) {
                int ev0, ev1;
                if (FindEdgeContainingPoint(m, cp1.position, 0.01f, &ev0, &ev1)) {
                    int edge_faces[2];
                    int edge_face_count = GetFacesWithEdge(m, ev0, ev1, edge_faces);
                    for (int fi = 0; fi < edge_face_count; fi++) {
                        FaceData& f = m->faces[edge_faces[fi]];
                        for (int vi = 0; vi < f.vertex_count; vi++) {
                            int fv0 = f.vertices[vi];
                            int fv1 = f.vertices[(vi + 1) % f.vertex_count];
                            if ((fv0 == ev0 && fv1 == ev1) || (fv0 == ev1 && fv1 == ev0)) {
                                InsertVertexInFace(m, edge_faces[fi], vi + 1, v1);
                                break;
                            }
                        }
                    }
                }
            }

            // Now find faces that contain both vertices
            face_count0 = GetFacesWithVertex(m, v0, faces0);
            face_count1 = GetFacesWithVertex(m, v1, faces1);

            // Find shared face
            for (int fi0 = 0; fi0 < face_count0; fi0++) {
                for (int fi1 = 0; fi1 < face_count1; fi1++) {
                    if (faces0[fi0] == faces1[fi1]) {
                        SplitFaceAlongVertices(m, faces0[fi0], v0, v1);
                        // Face was split, indices may have changed, break out
                        goto next_pair;
                    }
                }
            }
            next_pair:;
        }
    }

    // Rebuild edges
    UpdateEdges(m);
    MarkDirty(m);
}

static void EndKnifeTool(bool commit) {
    if (commit) {
        RecordUndo(g_knife_tool.mesh);
        CommitKnifeCuts(g_knife_tool.mesh);
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

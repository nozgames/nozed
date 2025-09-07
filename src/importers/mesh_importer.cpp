//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//
// @STL

#include "../gltf.h"
#include "../../../src/internal.h"

namespace fs = std::filesystem;
using namespace noz;

constexpr Vec2 OUTLINE_COLOR = ColorUV(0, 10);

struct OutlineConfig
{
    float width;
    float offset;
    float boundary_taper;
};

struct OutlineEdge
{
    int count;
    u16 i0;
    u16 i1;
    u64 vh0;
    u64 vh1;
};

void GenerateMeshOutline(GLTFMesh* mesh, const OutlineConfig& config = {});

static void FlattenMesh(GLTFMesh* mesh)
{
    // Create a vector of triangle indices with their max z values
    struct TriangleInfo
    {
        float maxZ;
        uint16_t i0, i1, i2;
    };
    std::vector<TriangleInfo> triangles;

    // Process each triangle (3 consecutive indices)
    for (size_t i = 0; i < mesh->indices.size(); i += 3)
    {
        uint16_t idx0 = mesh->indices[i];
        uint16_t idx1 = mesh->indices[i + 1];
        uint16_t idx2 = mesh->indices[i + 2];

        // Find the maximum y value in this triangle
        float maxZ = std::max({
            mesh->positions[idx0].z,
            mesh->positions[idx1].z,
            mesh->positions[idx2].z
        });

        triangles.push_back({maxZ, idx0, idx1, idx2});
    }

    // Sort triangles by max z value (back to front - highest z first)
    std::sort(triangles.begin(), triangles.end(),
        [](const TriangleInfo& a, const TriangleInfo& b)
        {
            return a.maxZ < b.maxZ;
        });

    // Rebuild the indices array with sorted triangles
    for (size_t t = 0; t < triangles.size(); t++)
    {
        const auto& tri = triangles[t];
        auto ii = t;
        
        mesh->indices[t * 3 + 0] = tri.i0;
        mesh->indices[t * 3 + 1] = tri.i1;
        mesh->indices[t * 3 + 2] = tri.i2;
    }
}

static void WriteMeshData(
    Stream* stream,
    const GLTFMesh* mesh,
    Props* meta)
{
    // Write asset header
    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_MESH;
    header.version = 1;
    header.flags = 0;
    WriteAssetHeader(stream, &header);

    std::vector<Vec2> positions;
    positions.reserve(mesh->positions.size());
    for (const Vec3& pos : mesh->positions)
        positions.push_back({pos.x, pos.y});

    // header
    Bounds2 bounds = ToBounds(positions.data(), positions.size());
    WriteStruct(stream, bounds);
    WriteU16(stream, static_cast<u16>(mesh->positions.size()));
    WriteU16(stream, static_cast<u16>(mesh->indices.size()));

    // verts
    for (size_t i = 0; i < mesh->positions.size(); ++i)
    {
        MeshVertex vertex = {};
        vertex.position = positions[i];
        vertex.uv0 = {0, 0};
        vertex.bone = 0;
        vertex.normal = {0, 1};
        
        if (mesh->normals.size() == mesh->positions.size() && i < mesh->normals.size())
            vertex.normal = mesh->normals[i];
            
        if (mesh->uvs.size() == mesh->positions.size() && i < mesh->uvs.size())
            vertex.uv0 = mesh->uvs[i];
            
        if (mesh->bone_indices.size() == mesh->positions.size() && i < mesh->bone_indices.size())
            vertex.bone = static_cast<float>(mesh->bone_indices[i]);
            
        WriteBytes(stream, &vertex, sizeof(MeshVertex));
    }
    
    // indices
    WriteBytes(stream, const_cast<uint16_t*>(mesh->indices.data()), mesh->indices.size() * sizeof(uint16_t));
}

void ImportMesh(const fs::path& source_path, Stream* output_stream, Props* config, Props* meta)
{
    const fs::path& src_path = source_path;

    // Load GLTF/GLB file
    GLTFLoader gltf;
    if (!gltf.open(src_path))
        throw std::runtime_error("Failed to open GLTF/GLB file");

    // Create bone filter from meta file
    fs::path meta_path = fs::path(src_path.string() + ".meta");

    // Read bones and mesh
    std::vector<GLTFBone> bones = gltf.ReadBones();
    GLTFMesh mesh = gltf.ReadMesh(bones);
    
    if (mesh.positions.empty())
    {
        throw std::runtime_error("No mesh data found");
    }

    if (meta->GetBool("mesh", "outline", config->GetBool("mesh.defaults", "outline", false)))
        GenerateMeshOutline(&mesh, {
            .width = meta->GetFloat("outline", "width", config->GetFloat("mesh.defaults.outline", "width", 0.02f)),
            .offset = meta->GetFloat("outline", "offset", config->GetFloat("mesh.defaults.outline", "offset", 0.5f)),
            .boundary_taper = meta->GetFloat("outline", "taper", config->GetFloat("mesh.defaults.outline", "taper", 0.01f))
        });

    // Apply flatten if requested
    if (meta->GetBool("mesh", "flatten", config->GetBool("mesh.defaults", "flatten", false)))
        FlattenMesh(&mesh);

    // Write mesh data to stream
    WriteMeshData(output_stream, &mesh, meta);
}

static u64 GetVertHash(const Vec3& pos)
{
    i32 px = (i32)(pos.x * 10000.0f);
    i32 py = (i32)(pos.y * 10000.0f);
    return Hash(Hash(&px, sizeof(px)), Hash(&py, sizeof(py)));
}

static u64 GetEdgeHash(const Vec3& p0, const Vec3& p1)
{
    u64 h0 = GetVertHash(p0);
    u64 h1 = GetVertHash(p1);
    return Hash(Min(h0,h1), Max(h0,h1));
}

static Vec2 CalculateEdgeDirection(const Vec3& p0, const Vec3& p1)
{
    return Normalize(Vec2{p1.x - p0.x, p1.y - p0.y});
}

static Vec2 CalculateEdgeNormal(const Vec2& edge_dir)
{
    return {-edge_dir.y, edge_dir.x};
}

static Vec2 AverageNormals(const Vec2& normal1, const Vec2& normal2)
{
    return Normalize(normal1 + normal2);
}

static Vec2 CalculateNeighborNormal(const OutlineEdge& neighbor, u64 shared_vertex, const std::map<u64, Vec3>& vert_map)
{
    Vec3 np0 = vert_map.at(neighbor.vh0);
    Vec3 np1 = vert_map.at(neighbor.vh1);
    Vec2 dir = CalculateEdgeDirection(np0, np1);
    return CalculateEdgeNormal(dir);
}

static void AddOutlineVertices(
    GLTFMesh* mesh,
    const Vec3& p0,
    const Vec3& p1,
    const Vec2& normal_p0,
    const Vec2& normal_p1,
    float width_p0,
    float width_p1,
    float offset)
{
    float inner_offset_p0 = -width_p0 * (0.5f + offset * 0.5f);
    float outer_offset_p0 = width_p0 * (0.5f - offset * 0.5f);
    float inner_offset_p1 = -width_p1 * (0.5f + offset * 0.5f);
    float outer_offset_p1 = width_p1 * (0.5f - offset * 0.5f);
    
    // Add vertices for outline quad
    mesh->positions.push_back({
        p0.x + normal_p0.x * inner_offset_p0,
        p0.y + normal_p0.y * inner_offset_p0,
        p0.z + 0.001f
    });
    mesh->positions.push_back({
        p0.x + normal_p0.x * outer_offset_p0,
        p0.y + normal_p0.y * outer_offset_p0,
        p0.z + 0.001f
    });
    mesh->positions.push_back({
        p1.x + normal_p1.x * inner_offset_p1,
        p1.y + normal_p1.y * inner_offset_p1,
        p1.z + 0.001f
    });
    mesh->positions.push_back({
        p1.x + normal_p1.x * outer_offset_p1,
        p1.y + normal_p1.y * outer_offset_p1,
        p1.z + 0.001f
    });
}

static void AddOutlineAttributes(GLTFMesh* mesh, const Vec2& normal_p0, const Vec2& normal_p1)
{
    // Copy normals if they exist
    if (!mesh->normals.empty())
    {
        Vec3 outline_normal_p0 = {normal_p0.x, normal_p0.y, 0.0f};
        Vec3 outline_normal_p1 = {normal_p1.x, normal_p1.y, 0.0f};
        mesh->normals.push_back(outline_normal_p0);
        mesh->normals.push_back(outline_normal_p0);
        mesh->normals.push_back(outline_normal_p1);
        mesh->normals.push_back(outline_normal_p1);
    }
    
    // Add UVs for outline vertices
    if (!mesh->uvs.empty())
    {
        mesh->uvs.push_back(OUTLINE_COLOR);
        mesh->uvs.push_back(OUTLINE_COLOR);
        mesh->uvs.push_back(OUTLINE_COLOR);
        mesh->uvs.push_back(OUTLINE_COLOR);
    }
}

static void AddOutlineTriangles(GLTFMesh* mesh, uint16_t base_vertex_index)
{
    uint16_t v0_inner = base_vertex_index;
    uint16_t v0_outer = base_vertex_index + 1;
    uint16_t v1_inner = base_vertex_index + 2;
    uint16_t v1_outer = base_vertex_index + 3;
    
    // Add triangles for outline quad (2 triangles per edge)
    mesh->indices.push_back(v0_inner);
    mesh->indices.push_back(v0_outer);
    mesh->indices.push_back(v1_inner);
    
    mesh->indices.push_back(v0_outer);
    mesh->indices.push_back(v1_outer);
    mesh->indices.push_back(v1_inner);
}

void GenerateMeshOutline(GLTFMesh* mesh, const OutlineConfig& config)
{
    std::map<u64, Vec3> vert_map;
    std::map<u64, OutlineEdge> edge_map;

    for (size_t i = 0; i < mesh->indices.size(); i += 3)
    {
        u16 i0 = mesh->indices[i];
        u16 i1 = mesh->indices[i + 1];
        u16 i2 = mesh->indices[i + 2];

        const Vec3& p0 = mesh->positions[i0];
        const Vec3& p1 = mesh->positions[i1];
        const Vec3& p2 = mesh->positions[i2];

        u64 ph0 = GetVertHash(p0);
        u64 ph1 = GetVertHash(p1);
        u64 ph2 = GetVertHash(p2);

        vert_map[ph0] = {p0};
        vert_map[ph1] = {p1};
        vert_map[ph2] = {p2};

        u64 eh0 = GetEdgeHash(p0, p1);
        u64 eh1 = GetEdgeHash(p1, p2);
        u64 eh2 = GetEdgeHash(p2, p0);

        OutlineEdge& e0 = edge_map[eh0];
        e0.i0 = i0;
        e0.i1 = i1;
        e0.vh0 = ph0;
        e0.vh1 = ph1;
        e0.count++;

        OutlineEdge& e1 = edge_map[eh1];
        e1.i0 = i1;
        e1.i1 = i2;
        e1.vh0 = ph1;
        e1.vh1 = ph2;
        e1.count++;

        OutlineEdge& e2 = edge_map[eh2];
        e2.i0 = i2;
        e2.i1 = i0;
        e2.vh0 = ph2;
        e2.vh1 = ph0;
        e2.count++;
    }

    for (auto& edge : edge_map)
    {
        if (edge.second.count > 1)
            continue;

        // Skip edges where either vertex has no outline
        if (mesh->outlines[edge.second.i0] <= 0.00001f ||
            mesh->outlines[edge.second.i1] <= 0.00001f)
            continue;

        OutlineEdge n0 = {};
        OutlineEdge n1 = {};

        // Find neighbors that share vertices with this edge
        for (auto& neighbor : edge_map)
        {
            if (neighbor.first == edge.first)
                continue;
            if (neighbor.second.count > 1)
                continue;

            // Check if edge2 shares the second vertex of the current edge (vh1)
            if (neighbor.second.vh0 == edge.second.vh1 ||
                neighbor.second.vh1 == edge.second.vh1)
            {
                if (n0.count == 0)
                    n0 = neighbor.second;
            }
            // Check if edge2 shares the first vertex of the current edge (vh0)
            else if (neighbor.second.vh0 == edge.second.vh0 ||
                     neighbor.second.vh1 == edge.second.vh0)
            {
                if (n1.count == 0) // Only take first neighbor found
                    n1 = neighbor.second;
            }
        }

        // All boundary edges should get outlines, but we adjust width based on neighbors

        Vec3 e0p0 = vert_map[edge.second.vh0];
        Vec3 e0p1 = vert_map[edge.second.vh1];

        // Calculate main edge direction and normal
        Vec2 edge_dir = CalculateEdgeDirection(e0p0, e0p1);
        if (edge_dir.x == 0.0f && edge_dir.y == 0.0f)
            continue; // Skip degenerate edges
            
        Vec2 edge_normal = CalculateEdgeNormal(edge_dir);
        
        Vec2 normal_p0 = edge_normal;
        Vec2 normal_p1 = edge_normal;
        float width_p0 = config.width * mesh->outlines[edge.second.i0];
        float width_p1 = config.width * mesh->outlines[edge.second.i1];

        // Both edge points have neighbors
        if (n0.count > 0 && n1.count > 0)
        {
            Vec2 n0_normal = CalculateNeighborNormal(n0, edge.second.vh1, vert_map);
            Vec2 n1_normal = CalculateNeighborNormal(n1, edge.second.vh0, vert_map);
            
            normal_p1 = AverageNormals(edge_normal, n0_normal);
            normal_p0 = AverageNormals(edge_normal, n1_normal);
        }
        // Only one edge point has a neighbor
        else if (n0.count > 0)
        {
            Vec2 n0_normal = CalculateNeighborNormal(n0, edge.second.vh1, vert_map);
            normal_p1 = AverageNormals(edge_normal, n0_normal);
            
            // Apply taper only if boundary_taper is significantly different from 1.0
            if (config.boundary_taper < 0.9f)
                width_p0 *= config.boundary_taper;
        }
        // Only one edge point has a neighbor  
        else if (n1.count > 0)
        {
            Vec2 n1_normal = CalculateNeighborNormal(n1, edge.second.vh0, vert_map);
            normal_p0 = AverageNormals(edge_normal, n1_normal);
            
            // Apply taper only if boundary_taper is significantly different from 1.0
            if (config.boundary_taper < 0.9f)
                width_p1 *= config.boundary_taper;
        }
        else
        {
            // No neighbors - apply taper only if boundary_taper is significantly different from 1.0
            if (config.boundary_taper < 0.9f)
            {
                width_p0 *= config.boundary_taper;
                width_p1 *= config.boundary_taper;
            }
        }
        
        // Generate outline geometry
        uint16_t base_vertex = static_cast<uint16_t>(mesh->positions.size());
        
        AddOutlineVertices(mesh, e0p0, e0p1, normal_p0, normal_p1, width_p0, width_p1, config.offset);
        AddOutlineAttributes(mesh, normal_p0, normal_p1);
        AddOutlineTriangles(mesh, base_vertex);
    }
}

bool CanImportMesh(const fs::path& source_path)
{
    Stream* stream = LoadStream(ALLOCATOR_DEFAULT, fs::path(source_path.string() + ".meta"));
    if (!stream)
        return true;
    Props* props = Props::Load(stream);
    if (!props)
    {
        Free(stream);
        return true;
    }

    bool can_import = !props->GetBool("mesh", "skip_mesh", false);
    Free(stream);
    delete props;
    return can_import;
}

static const char* g_mesh_extensions[] = { ".glb", nullptr };

static AssetImporterTraits g_mesh_importer_traits = {
    .type_name = "Mesh",
    .signature = ASSET_SIGNATURE_MESH,
    .file_extensions = g_mesh_extensions,
    .import_func = ImportMesh,
    .can_import = CanImportMesh
};

AssetImporterTraits* GetMeshImporterTraits()
{
    return &g_mesh_importer_traits;
}


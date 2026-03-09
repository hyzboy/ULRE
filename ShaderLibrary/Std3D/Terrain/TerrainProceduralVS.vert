// TerrainProceduralVS.vert
// Terrain vertex shader — no VBO/IBO input.
// Grid position X/Y computed from gl_VertexIndex; Z sampled from heightmap.
//
// ECS side: TerrainTileBuffer supplies per-instance data via instance-rate VAB (binding 0).
// Draw call:  vkCmdDrawIndirect — one VkDrawIndirectCommand per tile, all tiles in one call.
//   vertexCount   = grid_w * grid_h * 6
//   instanceCount = 1
//   firstInstance = tile_index  →  gl_InstanceIndex in this shader

#version 450

// ── Per-instance tile parameters (instance-rate VAB, binding 0) ──────────────
// Matches TerrainTileParams { int32 tile_x, tile_y, grid_w, grid_h }.
// Pipeline vertex input state:
//   VkVertexInputBindingDescription   { binding=0, stride=16, inputRate=INSTANCE }
//   VkVertexInputAttributeDescription { location=0, binding=0, format=R32G32B32A32_SINT, offset=0 }
layout(location = 0) in ivec4 i_tile;   // (tile_x, tile_y, grid_w, grid_h)

// ── Camera UBO (bound by RenderDescriptorBindingSystem) ──
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    vec3 camera_pos;
} camera;

// ── Heightmap (bound via material descriptor set) ──
layout(set = 1, binding = 0) uniform sampler2D heightmap;

// ── Terrain parameters UBO ──
layout(set = 1, binding = 1) uniform TerrainParams {
    float tile_world_size;         // world-space size of one tile (e.g. 64.0)
    float height_scale;            // multiplier applied to heightmap sample
    vec2  heightmap_atlas_offset;  // UV offset for this tile in a texture atlas (optional)
} terrain;

// ── Output to fragment shader ──
layout(location = 0) out vec3 v_world_pos;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out vec3 v_normal;  // finite-difference normal from heightmap

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Per-vertex index → (quad_col, quad_row, corner_within_quad)
//
// Layout for one quad (two CCW triangles):
//
//   3 ─── 2
//   │ ╲   │
//   │   ╲ │
//   0 ─── 1
//
//   tri0: 0,1,2   tri1: 0,2,3
//   index within quad: 0 1 2  0 2 3
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
void main()
{
    // Unpack per-instance tile parameters
    int  tile_x   = i_tile.x;
    int  tile_y   = i_tile.y;
    uint grid_w   = uint(i_tile.z);
    uint grid_h   = uint(i_tile.w);

    // Decode vertex index within this tile
    uint vi       = uint(gl_VertexIndex);
    uint quad_idx = vi / 6u;
    uint corner   = vi % 6u;

    // Map corner [0..5] → local (u_off, v_off) on the quad
    // tri0: 0(0,0) 1(1,0) 2(1,1)   tri1: 0(0,0) 2(1,1) 3(0,1)
    const uint u_table[6] = uint[](0u, 1u, 1u, 0u, 1u, 0u);
    const uint v_table[6] = uint[](0u, 0u, 1u, 0u, 1u, 1u);

    uint u_off = u_table[corner];
    uint v_off = v_table[corner];

    // Grid position of this quad
    uint quad_col = quad_idx % grid_w;
    uint quad_row = quad_idx / grid_w;

    // Tile-local grid vertex position (0 .. grid_w, 0 .. grid_h)
    float lx = float(quad_col + u_off);
    float ly = float(quad_row + v_off);

    // World position X/Y (terrain lies in XY plane, Z is up)
    float grid_step = terrain.tile_world_size / float(grid_w);
    float wx = float(tile_x) * terrain.tile_world_size + lx * grid_step;
    float wy = float(tile_y) * terrain.tile_world_size + ly * grid_step;

    // Sample height (tile-local UV, with optional atlas offset)
    v_uv = vec2(lx / float(grid_w), ly / float(grid_h));
    float wz = texture(heightmap, v_uv + terrain.heightmap_atlas_offset).r * terrain.height_scale;

    v_world_pos = vec3(wx, wy, wz);

    // Finite-difference normal (cheap, one-sample-per-vertex approximation)
    float eps = grid_step;
    float h_r = texture(heightmap, v_uv + vec2(eps / terrain.tile_world_size, 0.0)).r * terrain.height_scale;
    float h_u = texture(heightmap, v_uv + vec2(0.0, eps / terrain.tile_world_size)).r * terrain.height_scale;
    vec3 tangent_r = normalize(vec3(eps, 0.0, h_r - wz));
    vec3 tangent_u = normalize(vec3(0.0, eps, h_u - wz));
    v_normal = normalize(cross(tangent_r, tangent_u));

    gl_Position = camera.view_proj * vec4(v_world_pos, 1.0);
}


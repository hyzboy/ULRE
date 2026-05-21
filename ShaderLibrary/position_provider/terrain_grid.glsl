#ifndef ULRE_POS_TERRAIN_GRID_GLSL
#define ULRE_POS_TERRAIN_GRID_GLSL

// position_provider/terrain_grid.glsl
//
// Position source: procedural terrain grid.
// Derives (x, y) from the vertex index interpreted as (col, row) in a grid, then
// samples a heightmap to produce z.  The result is in object (local) space.
//
// MANIFEST: {
//   "vab_count": 0,
//   "position_space": "local",
//   "ssbo": [],
//   "ubo": [{
//     "set":     "TERRAIN_SET",
//     "binding": 0,
//     "name":    "TerrainParams",
//     "members": [
//       "vec2  tile_origin",
//       "vec2  cell_size",
//       "float height_scale",
//       "int   cols"
//     ]
//   }],
//   "samplers": [{
//     "set":     "TERRAIN_SET",
//     "binding": 1,
//     "name":    "u_Heightmap",
//     "type":    "sampler2D"
//   }]
// }
//
// Prerequisites injected by emitter:
//   TERRAIN_SET – descriptor set index shared by the UBO and heightmap

layout(set=TERRAIN_SET, binding=0) uniform TerrainParams
{
    vec2  tile_origin;
    vec2  cell_size;
    float height_scale;
    int   cols;
} u_Terrain;

layout(set=TERRAIN_SET, binding=1) uniform sampler2D u_Heightmap;

vec4 GetPosition()
{
    int   ix = gl_VertexIndex % u_Terrain.cols;
    int   iy = gl_VertexIndex / u_Terrain.cols;
    vec2  xy = u_Terrain.tile_origin + vec2(ix, iy) * u_Terrain.cell_size;
    float z  = texture(u_Heightmap, vec2(ix, iy) / vec2(u_Terrain.cols)).r
               * u_Terrain.height_scale;
    return vec4(xy, z, 1.0);
}

#endif // ULRE_POS_TERRAIN_GRID_GLSL

# Terrain Heightmap Material Generator

A material generator for rendering heightmap-based terrain with multi-layer texture blending using Blinn-Phong + Half Lambert lighting.

## Features

- **Heightmap-based terrain generation**: Uses R16UI heightmap texture to generate Z coordinates from uvec2 grid positions
- **Multi-layer texture blending**: Supports 4-layer texture blending using RGBA8UNORM blend weights
- **Texture arrays**: Uses 2 Texture2DArray for colors and normals (4 layers each)
- **Blinn-Phong + Half Lambert lighting**: Implements professional lighting model with fixed directional sunlight
- **Surface normal calculation**: Generates surface normals using finite differences from heightmap

## Usage

### 1. Create Material Configuration

```cpp
#include <hgl/graph/mtl/TerrainHeightmapMaterial.h>

mtl::TerrainHeightmapMaterialCreateConfig cfg;
cfg.height_scale = 10.0f;    // Scale factor for height values
cfg.terrain_scale = 1.0f / 256.0f;  // UV scale for terrain coordinates
```

### 2. Required Textures

You need to provide the following textures:

- **Heightmap**: R16UI format texture containing height values
- **Blend weights**: RGBA8UNORM texture where each channel represents blend strength for 4 layers
- **Color array**: Texture2DArray with 4 color texture layers
- **Normal array**: Texture2DArray with 4 normal map layers

### 3. Create Material and Bind Textures

```cpp
Material *terrain_material = LoadMaterial("TerrainHeightmapMaterial", &cfg);
Pipeline *terrain_pipeline = CreatePipeline(terrain_material, InlinePipeline::Solid3D);

// Bind textures
terrain_material->BindImageSampler(DescriptorSetType::PerMaterial,
                                  mtl::SamplerName::Heightmap,
                                  heightmap_texture, heightmap_sampler);
                                  
terrain_material->BindImageSampler(DescriptorSetType::PerMaterial,
                                  mtl::SamplerName::BlendWeights,
                                  blend_weights_texture, blend_weights_sampler);
                                  
terrain_material->BindImageSampler(DescriptorSetType::PerMaterial,
                                  mtl::SamplerName::ColorArray,
                                  color_texture_array, texture_array_sampler);
                                  
terrain_material->BindImageSampler(DescriptorSetType::PerMaterial,
                                  mtl::SamplerName::NormalArray,
                                  normal_texture_array, texture_array_sampler);
```

### 4. Generate Terrain Grid

Create a 2D grid of uvec2 positions:

```cpp
// Create vertex data (uvec2 grid positions)
uint32_t *positions = new uint32_t[TERRAIN_SIZE * TERRAIN_SIZE * 2];
uint32_t pos_idx = 0;

for (uint32_t y = 0; y < TERRAIN_SIZE; ++y) {
    for (uint32_t x = 0; x < TERRAIN_SIZE; ++x) {
        positions[pos_idx++] = x;
        positions[pos_idx++] = y;
    }
}

// Generate triangle indices
uint32_t *indices = new uint32_t[(TERRAIN_SIZE-1) * (TERRAIN_SIZE-1) * 6];
// ... fill indices for triangles
```

## Shader Details

### Vertex Shader
- Takes uvec2 grid positions as input
- Converts to normalized UV coordinates using terrain_scale
- Samples heightmap texture to get Z height value
- Calculates surface normals using finite differences
- Outputs world position, UV coordinates, and normal to fragment shader

### Fragment Shader
- Samples blend weights texture (RGBA8UNORM) for 4-layer blending
- Samples color and normal texture arrays based on blend weights
- Implements Blinn-Phong + Half Lambert lighting with fixed directional sunlight
- Combines ambient, diffuse, and specular lighting components

## Configuration Parameters

- **height_scale**: Multiplier for height values from heightmap (default: 1.0f)
- **terrain_scale**: UV coordinate scale factor (default: 1.0f / terrain_size)

## Lighting

The material uses a fixed directional sunlight with:
- **Direction**: normalize(vec3(0.3, 0.8, 0.5))
- **Color**: vec3(1.0, 0.95, 0.8) (warm sunlight)
- **Half Lambert**: Softer diffuse lighting (NdotL * 0.5 + 0.5)
- **Blinn-Phong specular**: Shininess of 32.0

## Example

See `example/Terrain/terrain_heightmap.cpp` for a complete implementation example.
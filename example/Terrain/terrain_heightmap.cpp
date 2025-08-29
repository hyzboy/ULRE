// Terrain heightmap rendering example
// Demonstrates heightmap-based terrain with multi-layer texture blending

#include<hgl/WorkManager.h>
#include<hgl/graph/mtl/TerrainHeightmapMaterial.h>
#include<hgl/graph/mtl/BlinnPhong.h>
#include<hgl/component/MeshComponent.h>
#include<hgl/math/Math.h>

using namespace hgl;
using namespace hgl::graph;

class TerrainApp : public WorkObject
{
private:
    // Textures
    Texture2D *heightmap_texture = nullptr;
    Texture2D *blend_weights_texture = nullptr;
    Texture2DArray *color_texture_array = nullptr;
    Texture2DArray *normal_texture_array = nullptr;
    
    // Samplers
    Sampler *heightmap_sampler = nullptr;
    Sampler *blend_weights_sampler = nullptr;
    Sampler *texture_array_sampler = nullptr;
    
    // Material and rendering
    Material *terrain_material = nullptr;
    MaterialInstance *terrain_material_instance = nullptr;
    Pipeline *terrain_pipeline = nullptr;
    MeshComponent *terrain_mesh_component = nullptr;
    
    // Terrain parameters
    static constexpr uint32_t TERRAIN_SIZE = 256; // 256x256 grid
    static constexpr float HEIGHT_SCALE = 10.0f;
    static constexpr float TERRAIN_SCALE = 1.0f / float(TERRAIN_SIZE);

private:
    bool CreateTerrainGrid()
    {
        // Create a 2D grid of uvec2 positions
        const uint32_t vertex_count = TERRAIN_SIZE * TERRAIN_SIZE;
        const uint32_t index_count = (TERRAIN_SIZE - 1) * (TERRAIN_SIZE - 1) * 6;
        
        // Generate grid positions
        auto *positions = new uint32_t[vertex_count * 2];
        uint32_t pos_idx = 0;
        
        for (uint32_t y = 0; y < TERRAIN_SIZE; ++y)
        {
            for (uint32_t x = 0; x < TERRAIN_SIZE; ++x)
            {
                positions[pos_idx++] = x;
                positions[pos_idx++] = y;
            }
        }
        
        // Generate indices for triangles
        auto *indices = new uint32_t[index_count];
        uint32_t idx = 0;
        
        for (uint32_t y = 0; y < TERRAIN_SIZE - 1; ++y)
        {
            for (uint32_t x = 0; x < TERRAIN_SIZE - 1; ++x)
            {
                uint32_t i0 = y * TERRAIN_SIZE + x;
                uint32_t i1 = y * TERRAIN_SIZE + (x + 1);
                uint32_t i2 = (y + 1) * TERRAIN_SIZE + x;
                uint32_t i3 = (y + 1) * TERRAIN_SIZE + (x + 1);
                
                // First triangle
                indices[idx++] = i0;
                indices[idx++] = i2;
                indices[idx++] = i1;
                
                // Second triangle
                indices[idx++] = i1;
                indices[idx++] = i2;
                indices[idx++] = i3;
            }
        }
        
        // Create mesh (simplified - in real implementation, would use proper mesh creation)
        // This is a placeholder for the mesh creation logic
        // mesh_terrain = CreateMesh("TerrainMesh", vertex_count, terrain_material_instance, terrain_pipeline,
        //                          { {VAN::Position, VAT_UVEC2, positions} }, indices, index_count);
        
        delete[] positions;
        delete[] indices;
        
        // For now, return true as placeholder
        return true;
    }

    bool CreateDummyTextures()
    {
        // Create dummy heightmap texture (R16UI format)
        // In real application, this would be loaded from file
        heightmap_texture = CreateTexture2D("HeightmapTerrain", 
                                          TERRAIN_SIZE, TERRAIN_SIZE, 
                                          UPF_R16U, false);
        if (!heightmap_texture) return false;
        
        // Create dummy blend weights texture (RGBA8UNORM format)
        blend_weights_texture = CreateTexture2D("BlendWeights",
                                               TERRAIN_SIZE, TERRAIN_SIZE,
                                               PF_RGBA8UNORM, false);
        if (!blend_weights_texture) return false;
        
        // Create dummy color texture array (4 layers)
        color_texture_array = CreateTexture2DArray("TerrainColors",
                                                  256, 256, 4,  // 256x256 textures, 4 layers
                                                  PF_RGBA8UNORM, false);
        if (!color_texture_array) return false;
        
        // Create dummy normal texture array (4 layers)
        normal_texture_array = CreateTexture2DArray("TerrainNormals",
                                                   256, 256, 4,  // 256x256 textures, 4 layers
                                                   PF_RGBA8UNORM, false);
        if (!normal_texture_array) return false;
        
        return true;
    }

    bool InitSamplers()
    {
        heightmap_sampler = CreateSampler();
        if (!heightmap_sampler) return false;
        
        blend_weights_sampler = CreateSampler();
        if (!blend_weights_sampler) return false;
        
        texture_array_sampler = CreateSampler();
        if (!texture_array_sampler) return false;
        
        return true;
    }

    bool InitMaterial()
    {
        mtl::TerrainHeightmapMaterialCreateConfig cfg;
        cfg.height_scale = HEIGHT_SCALE;
        cfg.terrain_scale = TERRAIN_SCALE;
        
        terrain_material = LoadMaterial("TerrainHeightmap", &cfg);
        if (!terrain_material) return false;
        
        terrain_pipeline = CreatePipeline(terrain_material, InlinePipeline::Solid3D);
        if (!terrain_pipeline) return false;
        
        // Bind textures to material
        if (!terrain_material->BindImageSampler(DescriptorSetType::PerMaterial,
                                              mtl::SamplerName::Heightmap,
                                              heightmap_texture,
                                              heightmap_sampler))
            return false;
            
        if (!terrain_material->BindImageSampler(DescriptorSetType::PerMaterial,
                                              mtl::SamplerName::BlendWeights,
                                              blend_weights_texture,
                                              blend_weights_sampler))
            return false;
            
        if (!terrain_material->BindImageSampler(DescriptorSetType::PerMaterial,
                                              mtl::SamplerName::ColorArray,
                                              color_texture_array,
                                              texture_array_sampler))
            return false;
            
        if (!terrain_material->BindImageSampler(DescriptorSetType::PerMaterial,
                                              mtl::SamplerName::NormalArray,
                                              normal_texture_array,
                                              texture_array_sampler))
            return false;
        
        // Create material instance with terrain parameters
        struct TerrainMaterialData
        {
            float height_scale;
            float terrain_scale;
            float padding[2];
        } terrain_data = { HEIGHT_SCALE, TERRAIN_SCALE, {0.0f, 0.0f} };
        
        terrain_material_instance = CreateMaterialInstance(terrain_material, &terrain_data, sizeof(terrain_data));
        if (!terrain_material_instance) return false;
        
        return true;
    }

public:
    bool Init() override
    {
        if (!CreateDummyTextures())
        {
            LOG_ERROR(OS_TEXT("Failed to create terrain textures"));
            return false;
        }
        
        if (!InitSamplers())
        {
            LOG_ERROR(OS_TEXT("Failed to create samplers"));
            return false;
        }
        
        if (!InitMaterial())
        {
            LOG_ERROR(OS_TEXT("Failed to create terrain material"));
            return false;
        }
        
        if (!CreateTerrainGrid())
        {
            LOG_ERROR(OS_TEXT("Failed to create terrain mesh"));
            return false;
        }
        
        return true;
    }
};

int os_main(int, os_char **)
{
    ConsoleAppFramework caf;
    
    if (!caf.Init())
        return -1;
        
    WorkManager wm(&caf);
    
    TerrainApp app;
    
    if (!wm.Add(&app))
        return -2;
        
    return wm.Run();
}
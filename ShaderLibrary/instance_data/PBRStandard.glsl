// instance_data/PBRStandard.glsl
// InstanceDataLayout::PBRStandard — stride = 16 bytes
// Used by: PBR standard shaders (standard3d)
struct MaterialInstance
{
    uint  base_color;       // 4 bytes (packed RGBA)
    float metallic;         // 4 bytes
    float roughness;        // 4 bytes
    float normal_scale;     // 4 bytes
};

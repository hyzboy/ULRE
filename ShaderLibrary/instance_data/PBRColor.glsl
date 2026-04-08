// instance_data/PBRColor.glsl
// InstanceDataLayout::PBRColor — stride = 12 bytes
// Used by: PBR packed-color shaders (pbrcolor3d)
struct MaterialInstance
{
    uint  base_color;   // 4 bytes (packed RGBA)
    float metallic;     // 4 bytes
    float roughness;    // 4 bytes
};

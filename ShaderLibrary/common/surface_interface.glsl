// @ulre begin
// @ulre name surface_interface
// @ulre kind Shared
// @ulre priority 0
// @ulre end
// Surface Interface — SurfaceInput / SurfaceOutput / SurfaceOutputExt
// 所有 Surface Function 和 Compositor 模板共享此接口定义

#ifndef SURFACE_INTERFACE_GLSL
#define SURFACE_INTERFACE_GLSL

#include "common/descriptor_macros.glsl"

struct SurfaceInput
{
    vec3 worldPos;       // camera-relative world position（非绝对世界坐标！）
    vec3 worldNormal;
    vec2 uv0;
    vec2 uv1;
    vec4 vertexColor;
    vec3 viewDir;        // normalize(-worldPos)，因为 cameraPos 恒为 0
    vec2 screenPos;
    float luminance;     // 顶点亮度（VertexLuminance 材质使用）
    uint styleID;        // 文本样式索引（Text CharQuad：flat varying fragStyleID → sbo_char_style）
};

// Material attributes returned by a surface module. Compositors must not
// overwrite baseColor with the final lit color.
struct SurfaceOutput
{
    vec3  baseColor;
    vec3  normal;
    float metallic;
    float roughness;
    float fresnel;
    float ao;
    vec3  emissive;
    float alpha;
};

// SurfaceOutputExt — reserved for future specialization (Skin/Hair/ClearCoat/Cloth).
// No active surface currently uses these fields; the struct is kept for the 4-bit
// SurfaceType bit-packing in the permutation key.
struct SurfaceOutputExt
{
    vec3  subsurfaceColor;
    float subsurfacePower;
    float thickness;
    vec3  sheenColor;
    float sheenRoughness;
    float clearCoat;
    float clearCoatRoughness;
    vec3  clearCoatNormal;
    float anisotropy;
    vec3  anisotropyDirection;
};

#endif // SURFACE_INTERFACE_GLSL

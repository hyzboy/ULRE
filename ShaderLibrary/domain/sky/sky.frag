#version 450
// Sky 域 · ForwardColor pass 的**物理固定**主着色器（天空盒变体）。
//
// 没有光照：直接把方向向量当"表面位置"送入材质源，输出 baseColor×alpha。
// 没有 OutputPolicy 模块——`WriteMaterialOutput` 是 pass 级的一行定义，
// 不是可替换策略。
//
// `fragDirection` 落在 **location 6**，不是 0：Sky 的 varying 槽位沿用
// `MaterialVertexVaryingConfig` 的位序（DataIndexID=0，Direction=6）。
// 改这个数字必须同步改顶点阶段的输出布局。
//
// 等价性：SPIR-V 尺寸与 reflection 均与旧 ShaderGen 产物全等。
// 无 cube 采样的 SkyMinimal 使用同域独立主着色器 `sky_minimal.frag`。

#include "domain/root_bindings.glsl"
#include "domain/sky_bindings.glsl"
#include "generated/frag_options.glsl"
#include "common/descriptor_macros.glsl"
#include "ubo/scene_ubo.glsl"
#include "sky/sky_atmosphere.glsl"
#include "common/surface_interface.glsl"
#include "material/sky_cube_surface.glsl"
#include "common/alpha_compositor.glsl"
layout(location=HGL_LOCATION_DATA_INDEX_ID) flat perprimitiveEXT in  uint fragDataIndexID;
layout(location=HGL_LOCATION_FRAG_DIRECTION) in vec3 fragDirection;
layout(location=HGL_LOCATION_VERTEX_COLOR) in vec4 fragVertexColor;
layout(location=0) out vec4 outColor;
void WriteMaterialOutput(vec4 value) { outColor = value * fragVertexColor; }

void main()
{
    SurfaceInput si;
    si.worldPos = fragDirection;
    si.worldNormal = normalize(fragDirection);
    si.uv0 = vec2(0.0);
    si.uv1 = vec2(0.0);
    si.vertexColor = fragVertexColor;
    si.viewDir = fragDirection;
    si.screenPos = vec2(0.0);
    si.luminance = 0.0;
    SurfaceOutput so = EvalSurface(si, 0u);
    WriteMaterialOutput(HGLComposeColor(vec4(so.baseColor, so.alpha)));
}

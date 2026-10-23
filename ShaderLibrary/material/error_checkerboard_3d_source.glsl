// @ulre begin
// @ulre name error_checkerboard_3d_source
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre require ProducedSemantic WorldPosition
// @ulre require Resource MaterialData
// @ulre uses material_source_interface
// @ulre end

// 错误兜底棋盘格（3D）：与 builtin/checkerboard_3d 的唯一差别是**读材质数据行的
// 标记色**（EmissiveSurface.color），棋盘格两色都由它派生 ⇒ 不同错误种类一望即辨。
//
// 标记色种类（`MaterialRecipe.h::kFallbackMaterialRules`，唯一权威表）：
//   洋红 = 无材质来源 / 橙 = 材质定义无法解析 / 青 = 声明 required 却没给纹理
//   黄   = 几何缺顶点法线 / 蓝 = 几何缺纹理 UV
// 本模块是**维度无关**的单一实现，颜色由数据决定 —— 新增错误种类不需要动这里。
//
// 为什么单独一份模块而不是改 checkerboard_3d_source：
//   · checkerboard_3d_source 必须保持**零资源依赖**（任何几何、任何绑定条件下都画得
//     出来），它是回退链的最后一环；
//   · 本模块声明 `require Resource MaterialData` ⇒ 生成器会向 recipe 索要
//     material_ssbo_binding，回退路径由 AttachFallbackMarkerColor 提供；
//     万一那一行拿不到，回退侧会降级回 builtin/checkerboard_3d（见
//     RenderPrimitiveCollectSystem::AttachFallbackMarkerColor）。
//
// 不依赖 UV / 法线 / 颜色：三平面投影 + OptionalFaceFallback 面法线兜底。

#ifndef ERROR_CHECKERBOARD_3D_SOURCE_GLSL
#define ERROR_CHECKERBOARD_3D_SOURCE_GLSL

#include "common/material_source_interface.glsl"

vec3 ResolveErrorCheckerboardNormal(const SurfaceInput surfaceInput)
{
#ifdef HGL_MATERIAL_HAS_VERTEX_NORMAL
    return normalize(surfaceInput.worldNormal);
#else
    vec3 faceNormal = cross(
        dFdx(surfaceInput.worldPos),
        dFdy(surfaceInput.worldPos));
    const float length_squared = dot(faceNormal, faceNormal);
    if (length_squared <= 1e-12)
        return vec3(0.0, 0.0, 1.0);
    faceNormal *= inversesqrt(length_squared);
    return gl_FrontFacing ? faceNormal : -faceNormal;
#endif
}

vec2 ResolveErrorCheckerboardCoordinates(
    const SurfaceInput surfaceInput,
    const vec3 normal)
{
    const vec3 absolute_normal = abs(normal);
    if (absolute_normal.z >= absolute_normal.x
     && absolute_normal.z >= absolute_normal.y)
        return surfaceInput.worldPos.xy;
    if (absolute_normal.x >= absolute_normal.y)
        return surfaceInput.worldPos.zy;
    return surfaceInput.worldPos.xz;
}

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput sourceInput)
{
    // 标记色 = 错误种类的根颜色（洋红 = 无材质来源、橙 = 材质定义无法解析）。
    // 它经 EmissiveSurface 行下发，与 builtin/pure_color 同一条路径。
    EmissiveSurfaceRow marker = MTL_ROW(sourceInput.dataIndex);
    const vec3 marker_color = clamp(marker.color.rgb, 0.0, 1.0);

    const vec3 normal = ResolveErrorCheckerboardNormal(sourceInput.surface);
    const vec2 checker_position =
        ResolveErrorCheckerboardCoordinates(sourceInput.surface, normal);
    const ivec2 cell = ivec2(floor(checker_position / 0.5));
    const int parity = (cell.x + cell.y) & 1;

    // 两色都取自标记色：暗格压到 12%、亮格到 90% ⇒ 色相保留、格子可辨。
    const vec3 checker_color = marker_color * (parity == 0 ? 0.12 : 0.90);

    const vec3 light_direction = normalize(vec3(0.45, 0.55, 0.70));
    const float diffuse = 0.45
        + 0.55 * max(dot(normal, light_direction), 0.0);

    MaterialSourceOutput materialResult;
    materialResult.baseColor = checker_color * diffuse;
    materialResult.metallic = 0.0;
    materialResult.roughness = 1.0;
    materialResult.fresnel = 0.0;
    materialResult.normalScale = 1.0;
    materialResult.ao = 1.0;
    materialResult.emissive = vec3(0.0);
    materialResult.alpha = 1.0;
    return materialResult;
}

float EvalMaterialAlpha(MaterialSourceInput sourceInput)
{
    return 1.0;
}

#endif // ERROR_CHECKERBOARD_3D_SOURCE_GLSL
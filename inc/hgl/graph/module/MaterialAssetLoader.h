#pragma once

/// MaterialAssetLoader.h — 从 MaterialRecipe 驱动材质创建的内联辅助函数
///
/// 将 MaterialRecipe 中的平铺字段还原为对应的
/// Material2DCreateConfig / Material3DCreateConfig / BillboardMaterialCreateConfig，
/// 调用 ShaderMaterialProgramManager::ResolveOrCreateProgram，并可选地加载纹理并绑定到材质。
///
/// 用法示例（示例程序顶部的静态配置表）：
///   static const mtl::MaterialRecipe kMeshMtl {
///       .id      = "my_mesh",
///       .preset  = mtl::MaterialPreset::Standard,
///       .intent_features = mtl::GetDefaultIntentFeatureMask(mtl::MaterialPreset::Standard),
///       .tex_base_color = "res/image/Brick/Albedo.Tex2D",
///       .tex_normal     = "res/image/Brick/Normal.Tex2D",
///   };
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/Material2DCreateConfig.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/graph/module/ShaderMaterialProgramManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/type/String.h>
#include <hgl/type/StdString.h>

namespace hgl::graph
{

inline mtl::MaterialFeatureMask ResolveRecipeIntentFeatureMask(const mtl::MaterialRecipe &rec)
{
    return mtl::ResolveIntentFeatureMask(rec.preset, rec.intent_features);
}

} // namespace hgl::graph
// Note: CreateMaterialFromRecord is now a file-static in ShaderMaterialProgramManager.cpp

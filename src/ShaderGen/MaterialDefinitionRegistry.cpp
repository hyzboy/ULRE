#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include "common/GenericMaterialBuilder.h"
#include<hgl/mtl/MaterialDefinitionFile.h>
#include<hgl/graph/ShaderBufferSource.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/mtl/contract/ShaderGenContract.h>
#include <hgl/mtl/MaterialCoverageContract.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/mtl/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/log/Log.h>
#include <cstring>
#include <algorithm>
#include <vector>
#include <string>

namespace hgl::graph::mtl{

namespace
{

    bool TryGetMaterialDefinitionByIDInternal(
        const char *mtl_def_id,
        MaterialDefinition &out_definition)
    {
        if (!mtl_def_id || !mtl_def_id[0])
            return false;

        // 全部材质定义（含内置 bootstrap：pure_color/text_2d）均为 TOML
        // 文件承载——C++ 硬编码材质已移除，统一走文件注册表查询。
        const MaterialDefinitionFileRegistry &file_registry =
            GetMaterialDefinitionFileRegistry();
        const MaterialDefinition *file_definition =
            file_registry.FindByID(mtl_def_id);
        if (file_definition)
        {
            out_definition = *file_definition;
            return true;
        }

        return false;
    }
}

VertexShaderNodeConfig ResolveMaterialVertexNodeConfig(
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request) noexcept
{
    // 1. request 显式覆盖
    if (request.has_vertex_node_config_override)
        return request.vertex_node_config_override;

    // 2. recipe 显式设置
    if (!IsDefault3DNodeConfig(request.recipe.vertex_node_config))
        return request.recipe.vertex_node_config;

    // 3. definition 非默认
    if (!IsDefault3DNodeConfig(definition.vertex_node_config))
        return definition.vertex_node_config;

    // 4. fallback
    return request.recipe.vertex_node_config;
}

uint64 HashMaterialProgramBuildContext(
    const PrimitiveType primitive_type,
    const GeometryVertexFormat *geometry_vertex_format,
    const contract::PhysicalDeviceProfileLite *profile,
    const mtl::ShaderProgramPurpose purpose) noexcept
{
    hgl::hash::FNV1aHasher64 h;

    h << primitive_type
      << (geometry_vertex_format
            ? geometry_vertex_format->GetVertexInputHash() : 0)
      // 统一用编译目标超集哈希（设备能力 + 解析后的目标版本）——
      // 此前与 compiler_hash 双轨并存（L4 N5），两处口径不一致
      << contract::GetShaderCompilerProfileHash(profile)
      << static_cast<uint32>(purpose);
    return h;
}

MaterialVertexVaryingConfig ResolveMaterialVertexVaryingConfig(
    const MaterialDefinition &definition,
    const mtl::ShaderProgramPurpose purpose,
    const mtl::MaterialCoverageContract &coverage) noexcept
{
    MaterialVertexVaryingConfig varying =
        definition.vertex_varying;
    const bool depth_purpose =
        purpose == ShaderProgramPurpose::DepthOnly
     || purpose == ShaderProgramPurpose::ShadowDepth;
    if (!depth_purpose)
        return varying;

    varying.emit_world_pos = false;
    varying.emit_world_normal = false;
    varying.emit_frag_direction = false;
    varying.emit_data_index_id = false;
    varying.emit_vertex_color = false;
    varying.emit_uv0 = false;
    varying.emit_luminance = false;
    varying.emit_vertex_color_from_palette = false;
    varying.emit_style_id = false;

    if (!coverage.requires_alpha_evaluation)
        return varying;

    const auto needs_semantic =
        [&coverage](const InterStageSemantic semantic)
    {
        return (coverage.required_semantics
            & GetInterStageSemanticMask(semantic)) != 0;
    };
    varying.emit_data_index_id =
        needs_semantic(InterStageSemantic::DataIndexID);
    varying.emit_vertex_color =
        needs_semantic(InterStageSemantic::Color)
     && definition.vertex_varying.emit_vertex_color;
    varying.emit_vertex_color_from_palette =
        needs_semantic(InterStageSemantic::Color)
     && definition.vertex_varying.
            emit_vertex_color_from_palette;
    varying.emit_uv0 =
        needs_semantic(InterStageSemantic::UV0);
    varying.emit_luminance =
        needs_semantic(InterStageSemantic::Luminance);
    return varying;
}

bool TryGetMaterialDefinitionByID(const std::string &mtl_def_id, MaterialDefinition &out_definition)
{
    return TryGetMaterialDefinitionByIDInternal(mtl_def_id.c_str(), out_definition);
}

MaterialDefinitionFileRegistry &GetMaterialDefinitionFileRegistry()
{
    static MaterialDefinitionFileRegistry registry;
    static bool loaded = false;
    if (!loaded)
    {
        int file_count = 0;
        int error_count = 0;
        const hgl::filesystem::Path material_path =
            hgl::filesystem::Path(ToOSString(mtl::GetShaderLibraryPath()))
            / OSString(OS_TEXT("material"));
        if (!registry.LoadDirectory(
                material_path.ToOSString(), &file_count, &error_count))
        {
            GLogWarning("[ShaderGen] Material TOML directory unavailable; using built-in definitions");
        }
        else
        {
            GLogInfo("[ShaderGen] Loaded %d material TOML definitions (%d errors)",
                     file_count, error_count);
        }
        loaded = true;
    }
    return registry;
}

mtl::ShaderBuildContext *CreateMaterialFromDefinition(
    const mtl::contract::PhysicalDeviceProfileLite *profile,
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request)
{
    // BuildGenericMaterial 不修改 definition（const&）——直接透传，
    // 薄包装只承担 API 语义（名字即文档）
    return BuildGenericMaterial(profile, request, definition);
}

void NormalizeRecipe(MaterialRecipe &recipe)
{
    if (recipe.mtl_def_id.empty())
        return;

    MaterialDefinition definition{};
    bool has_definition = TryGetMaterialDefinitionByID(recipe.mtl_def_id, definition);
    if (has_definition)
    {
        // Aliases are accepted only at the compatibility boundary. Once a
        // recipe is normalized, the canonical definition ID is the sole
        // runtime identity used by hashing and caches.
        recipe.mtl_def_id = definition.definition_id;
        ApplyBaseMaterialInfoDefaults(recipe, definition, false);

        const ResolvedMaterialRenderState resolved =
            ResolveMaterialRenderState(definition, recipe);

        // Write resolved values back to render_state_overrides as authoritative.
        recipe.render_state_overrides.has_double_sided = true;
        recipe.render_state_overrides.double_sided = resolved.double_sided;
        recipe.render_state_overrides.has_alpha_test = true;
        recipe.render_state_overrides.alpha_test = resolved.alpha_test;
        recipe.render_state_overrides.has_alpha_cutoff = true;
        recipe.render_state_overrides.alpha_cutoff = resolved.alpha_cutoff;
        recipe.render_state_overrides.has_dither = true;
        recipe.render_state_overrides.dither = resolved.dither;
        recipe.render_state_overrides.has_pipeline_config = true;
        recipe.render_state_overrides.pipeline_config = resolved.pipeline_config;
    }

}

}//namespace hgl::graph::mtl

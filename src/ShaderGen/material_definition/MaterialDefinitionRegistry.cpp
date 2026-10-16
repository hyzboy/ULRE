#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include "builder/GenericMaterialBuilder.h"
#include<hgl/mtl/MaterialDefinitionFile.h>
#include<hgl/graph/ShaderBufferSource.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/mtl/contract/ShaderGenContract.h>
#include <hgl/mtl/MaterialCoverageContract.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/mtl/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/log/Log.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>
#include <string>

namespace hgl::graph::mtl{

namespace
{
    bool AddMaterialProviderRoot(
        RenderTemplateRequest &request,
        const ShaderModuleSlotRole role,
        const char *module_name) noexcept
    {
        if (!module_name || !module_name[0])
            return true;
        return request.AddModuleRoot(role, AnsiString(module_name));
    }

    bool TryGetMaterialDefinitionByIDInternal(
        const char *mtl_def_id,
        MaterialDefinition &out_definition)
    {
        if (!mtl_def_id || !mtl_def_id[0])
            return false;

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

bool AppendMaterialRenderTemplateRoots(
    const MaterialDefinition &definition,
    RenderTemplateRequest &request) noexcept
{
    bool appended = true;
    switch (request.template_id)
    {
    case RenderTemplateID::ForwardLitShadowedAO:
    case RenderTemplateID::ForwardLitShadowedIdentityAO:
    case RenderTemplateID::ForwardLitUnshadowedAO:
        appended = AddMaterialProviderRoot(
                       request, ShaderModuleSlotRole::MaterialSourceProvider,
                      definition.material_source_module_name)
            && AddMaterialProviderRoot(
                   request, ShaderModuleSlotRole::NTBProvider,
                   definition.ntb_module_name);
        break;
    case RenderTemplateID::ForwardUnlit:
    case RenderTemplateID::ShadowCasterOpaque:
    case RenderTemplateID::ShadowCasterMasked:
        appended = AddMaterialProviderRoot(
            request, ShaderModuleSlotRole::MaterialSourceProvider,
            definition.material_source_module_name);
        break;
    case RenderTemplateID::Sky:
        if (definition.surface_module_name && definition.surface_module_name[0])
        {
            for (uint32 index = 0; index < request.module_root_count; ++index)
            {
                RenderTemplateModuleRoot &root = request.module_roots[index];
                if (root.role == ShaderModuleSlotRole::SurfaceProvider)
                {
                    root.module_name = definition.surface_module_name;
                    root.include_path.Clear();
                    break;
                }
            }
        }
        break;
    default:
        break;
    }

    RenderTemplateValidationDiagnostic diagnostic{};
    return appended && ValidateRenderTemplateRequest(request, diagnostic);
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

        if (!material_path.IsDirectory())
        {
            GLogFatal("[ShaderGen] Missing required ShaderLibrary/material directory: %s. File-backed schema-3 material definitions are mandatory.",
                      material_path.ToOSString().c_str());
            std::abort();
        }

        if (!registry.LoadDirectory(
                material_path.ToOSString(), &file_count, &error_count))
        {
            GLogFatal("[ShaderGen] Material TOML directory unavailable; no material definitions are available from %s. This is a hard failure.",
                      material_path.ToOSString().c_str());
            std::abort();
        }

        // 口径（2026-10-11 用户）：**单份材质写错（例如 sampler 配错）= LOG 报错**，不是进程级致命；
        // 出错的那份不注册 ⇒ 引用它的配方落到错误材质路径
        // （MaterialErrorKind::UnknownMaterialDefinition ⇒ 保底材质 + 按颜色标错误种类）。
        // 只有「整库不可用」（目录缺失 / 读不出 / 零份可解析）才是 fatal。
        if (file_count == 0 || registry.GetCount() == 0)
        {
            GLogFatal("[ShaderGen] ShaderLibrary/material is empty or invalid (%d files, %d errors, %d parsed definitions). File-backed schema-3 materials are required.",
                      file_count, error_count, registry.GetCount());
            std::abort();
        }

        if (error_count > 0)
            GLogError("[ShaderGen] %d 份材质定义加载失败（逐份原因见上）——这些定义不注册，"
                      "引用它们的配方会走错误材质路径。", error_count);

        GLogInfo("[ShaderGen] Loaded %d material TOML definitions (%d errors)",
                 file_count, error_count);
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
    return BuildGenericMaterial(profile, request, definition, nullptr);
}

mtl::ShaderBuildContext *CreateMaterialFromDefinition(
    const mtl::contract::PhysicalDeviceProfileLite *profile,
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request,
    mtl::MaterialShaderDocumentCapture *document_capture)
{
    return BuildGenericMaterial(profile, request, definition, document_capture);
}

bool BuildFallbackMaterialRecipe(MaterialRecipe &out_recipe,
                                 const MaterialErrorKind kind)
{
    if (kind == MaterialErrorKind::None)
        return false;

    const FallbackMaterialRule &rule = GetFallbackMaterialRule(kind);

    // 回退配方**从零合成**：不继承调用方残留字段 —— 调用点常是“构建失败后改走回退”，
    // 残留会让回退材质带上半份旧状态（旧纹理绑定 / 旧 SSBO 行）。
    out_recipe = MaterialRecipe{};
    out_recipe.recipe_name = rule.marker_name;
    out_recipe.mtl_def_id  = rule.definition_id;
    out_recipe.fallback_error_kind = kind;
    for (uint32_t i = 0; i < 4; ++i)
        out_recipe.fallback_marker_color[i] = rule.marker_color[i];

    // 与普通配方**同一条**规范化路径（定义默认值 + 解析后的渲染状态）。
    NormalizeRecipe(out_recipe);
    return true;
}

void NormalizeRecipe(MaterialRecipe &recipe)
{
    // Canonicalization is intentionally strict: empty or unknown material IDs are
    // not tolerated as a legacy compatibility branch. They resolve to the single
    // file-backed fallback material so every runtime program key remains stable and
    // traceable to a concrete material definition.
    //
    // A7b：**材质错误**分类（数据驱动：ClassifyMaterialErrorKind + kFallbackMaterialRules）：
    //   · 空 ID   = 无材质来源          ⇒ MissingMaterialSource
    //   · 未知 ID = 定义 ID 不可解析     ⇒ UnknownMaterialDefinition
    // 两者都落到保底材质，并把**错误种类与根颜色**记在配方上（根颜色用于标注错误种类）。
    MaterialErrorKind error_kind = MaterialErrorKind::None;

    if (recipe.mtl_def_id.empty())
        error_kind = MaterialErrorKind::MissingMaterialSource;

    MaterialDefinition definition{};
    if (!TryGetMaterialDefinitionByID(recipe.mtl_def_id, definition))
    {
        if (error_kind == MaterialErrorKind::None)
            error_kind = MaterialErrorKind::UnknownMaterialDefinition;

        // 报错口径（2026-10-14 用户）：材质未知/写错 **直接报错**，不静默。单份定义写错
        // （采样器 / 顶点输入名等配错）在**加载期**已 GLogError（那份定义不注册）；这里覆盖
        // **另一条**到达路径：配方引用的材质定义 ID 无法解析（作者写了不存在的 ID / 数据层
        // 覆盖指向不存在的材质）。本行是这条路径的**唯一出声点** —— ECS 侧把它当"构建成功"
        // 处理（只计数，不再报一次），否则同一事件会出两条日志。
        {
            const FallbackMaterialRule &fallback_rule = GetFallbackMaterialRule(error_kind);
            const char *reason = (error_kind == MaterialErrorKind::MissingMaterialSource)
                                     ? "无材质来源（空 ID）"
                                     : "材质定义 ID 无法解析";

            GLogError(u8"[MaterialFallback] %s: \"%s\" => 回退错误材质 %s"
                      u8"（棋盘格；错误种类 %s，颜色=(%.2f,%.2f,%.2f,%.2f)）",
                      reason, recipe.mtl_def_id.c_str(), fallback_rule.definition_id,
                      fallback_rule.marker_name,
                      fallback_rule.marker_color[0], fallback_rule.marker_color[1],
                      fallback_rule.marker_color[2], fallback_rule.marker_color[3]);
        }

        if (!BuildFallbackMaterialRecipe(recipe, error_kind))
            return;

        if (!TryGetMaterialDefinitionByID(recipe.mtl_def_id, definition))
            return;
    }

    recipe.mtl_def_id = definition.definition_id;

    const ResolvedMaterialRenderState resolved =
        ResolveMaterialRenderState(definition, recipe);

    // Write resolved values back to render_state_overrides as authoritative.
    recipe.render_state_overrides.has_alpha_test = true;
    recipe.render_state_overrides.alpha_test = resolved.alpha_test;
    recipe.render_state_overrides.has_alpha_cutoff = true;
    recipe.render_state_overrides.alpha_cutoff = resolved.alpha_cutoff;
    recipe.render_state_overrides.has_dither = true;
    recipe.render_state_overrides.dither = resolved.dither;
    recipe.render_state_overrides.has_pipeline_config = true;
    recipe.render_state_overrides.pipeline_config = resolved.pipeline_config;
}

}//namespace hgl::graph::mtl

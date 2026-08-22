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
    const char *GetVertexInputName(const VertexSemantic semantic)
    {
        switch (semantic)
        {
        case VertexSemantic::Position:    return "Position";
        case VertexSemantic::Normal:      return "Normal";
        case VertexSemantic::Tangent:     return "Tangent";
        case VertexSemantic::Bitangent:   return "Binormal";
        case VertexSemantic::Color:       return "Color";
        case VertexSemantic::Luminance:   return "Luminance";
        case VertexSemantic::TransformID: return "TransformID";
        case VertexSemantic::TexCoord:    return "TexCoord";
        default:                          return nullptr;
        }
    }

    const char *GetGLSLVertexInputType(const VkFormat format,
                                       const uint8 component_count)
    {
        const uint32 numeric_class =
            GLSLCodeModuleCapabilityResolver::GetNumericClassFromVkFormat(format);
        if (numeric_class == 0 || component_count == 0 || component_count > 4)
            return nullptr;

        const bool is_signed_integer = numeric_class
            & uint32(GLSLCodeModuleNumericClass::SignedInteger);
        const bool is_unsigned_integer = numeric_class
            & uint32(GLSLCodeModuleNumericClass::UnsignedInteger);
        if (component_count == 1)
            return is_signed_integer ? "int" : is_unsigned_integer ? "uint" : "float";

        if (is_signed_integer)
        {
            static const char *const types[] = {nullptr, nullptr, "ivec2", "ivec3", "ivec4"};
            return types[component_count];
        }

        if (is_unsigned_integer)
        {
            static const char *const types[] = {nullptr, nullptr, "uvec2", "uvec3", "uvec4"};
            return types[component_count];
        }

        static const char *const types[] = {nullptr, nullptr, "vec2", "vec3", "vec4"};
        return types[component_count];
    }

    bool BuildResolvedVertexABI(
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
        std::vector<SerializedVertexEntry> &out_vertices,
        VkFormat &out_position_format,
        std::string &out_vertex_input_glsl,
        GLSLCodeModuleResolutionResult &out_resolution)
    {
        switch (definition.vertex_provider_policy)
        {
        case MaterialVertexProviderPolicy::Auto:
        case MaterialVertexProviderPolicy::GeometryOnly:
        case MaterialVertexProviderPolicy::AllowDerived:
            break;
        default:
            return false;
        }
        if (!request.geometry_vertex_format)
            return false;

        if (request.vertex_code_module_registry)
        {
            if (!PreviewMaterialVertexSemanticResolution(
                    *request.vertex_code_module_registry, definition, request, out_resolution)
             || !out_resolution.resolved)
                return false;
        }
        else
        {
            out_resolution = GLSLCodeModuleResolutionResult{};
            out_resolution.resolved = true;
        }

        const GeometryVertexFormat &geometry = *request.geometry_vertex_format;
        out_vertices.clear();
        out_vertex_input_glsl.clear();
        out_position_format = VK_FORMAT_UNDEFINED;

        if (definition.vertex_node_config.transport == VertexTransportMode::SSBO)
        {
            // SSBO 顶点输入：C++ 只做"选择"——按需求语义选 s1_* 模块，
            // 读取代码在模块内（gl_VertexIndex），无 VBO attribute 布局。
            const GeometryVertexAttributeFormat *position_attribute =
                geometry.Find(VertexSemantic::Position);
            if (!position_attribute)
                return false;
            out_position_format = position_attribute->format;

            bool need_uv = false, need_ntb = false, need_joint = false, need_color = false, need_luminance = false, need_transform_id = false;
            for (int i = 0; i < definition.vertex_semantic_requirements.GetCount(); ++i)
            {
                const auto &requirement = definition.vertex_semantic_requirements[i];
                const VertexSemantic semantic =
                    GetVertexSemanticFromGLSLCodeModuleSemantic(requirement.semantic);
                switch (semantic)
                {
                case VertexSemantic::TexCoord: need_uv = true; break;
                case VertexSemantic::Normal:
                case VertexSemantic::Tangent:
                case VertexSemantic::Bitangent: need_ntb = true; break;
                case VertexSemantic::JointID:
                case VertexSemantic::JointWeight: need_joint = true; break;
                case VertexSemantic::Color: need_color = true; break;
                case VertexSemantic::Luminance: need_luminance = true; break;
                case VertexSemantic::TransformID: need_transform_id = true; break;
                default: break;  // Position 由 input mode 决定
                }
            }

            // 各数据模块先 include（定义 HGL_*_LOADER 宏），
            // Position 模块最后（LoadVertexData 以 #ifdef 展开全部 loader）
            // 索引模块最先（HGL_INDEX_LOADER——顶点数据间接读取 + push constant 段偏移）
            out_vertex_input_glsl += "#include \"vertex/s1_index.glsl\"\n";
            if (need_uv)
            {
                // 按 UV 属性格式选解码模块（格式=模块——RG16F 发行版一模块）
                const auto *uv_attr = geometry.Find(VertexSemantic::TexCoord);
                if (uv_attr && uv_attr->format == VK_FORMAT_R16G16_SFLOAT)
                    out_vertex_input_glsl += "#include \"vertex/s1_uv_rg16f.glsl\"\n";
                else
                    out_vertex_input_glsl += "#include \"vertex/s1_uv.glsl\"\n";
            }
            if (need_ntb)
            {
                // 按 Normal 属性格式选解码模块（格式=模块——发行版压缩格式各一模块）
                const auto *normal_attr = geometry.Find(VertexSemantic::Normal);
                if (normal_attr && normal_attr->format == VK_FORMAT_R8G8_UNORM)
                    out_vertex_input_glsl += "#include \"vertex/s1_ntb_rg8.glsl\"\n";
                else if (normal_attr && normal_attr->format == VK_FORMAT_R16G16_SFLOAT)
                    out_vertex_input_glsl += "#include \"vertex/s1_ntb_rg16f.glsl\"\n";
                else if (normal_attr && normal_attr->format == PF_A2BGR10UN)
                    out_vertex_input_glsl += "#include \"vertex/s1_ntb_a2bgr10.glsl\"\n";
                else
                    out_vertex_input_glsl += "#include \"vertex/s1_ntb.glsl\"\n";
            }
            if (need_joint)
                out_vertex_input_glsl += "#include \"vertex/s1_joint.glsl\"\n";
            if (need_color)
            {
                if (definition.vertex_varying.emit_vertex_color_from_palette)
                    out_vertex_input_glsl += "#include \"vertex/s1_palette_index.glsl\"\n";   // palette 材质：ColorIndex（R8_UINT 索引）
                else
                    out_vertex_input_glsl += "#include \"vertex/s1_color.glsl\"\n";           // 标准顶点色：vec4 直读
            }
            if (need_luminance)
                out_vertex_input_glsl += "#include \"vertex/s1_luminance.glsl\"\n";
            if (need_transform_id)
                out_vertex_input_glsl += "#include \"vertex/s1_transform_id.glsl\"\n";

            // 位置模块按 effective input 选择（position_format 判定——与
            // VertexShaderAssembler 一致：geometry 格式说了算，recipe/TOML input 仅兜底）
            VertexInputMode effective_input = definition.vertex_node_config.input;
            if (out_position_format == VK_FORMAT_R32G32_SINT || out_position_format == VK_FORMAT_R32G32_UINT ||
                out_position_format == VK_FORMAT_R16G16_SINT || out_position_format == VK_FORMAT_R16G16_UINT)
                effective_input = VertexInputMode::Vec2IntPosition;
            else if (out_position_format == VK_FORMAT_R32G32_SFLOAT ||
                     out_position_format == VK_FORMAT_R16G16_SFLOAT)
                effective_input = VertexInputMode::Vec2Position;
            else if (out_position_format == VK_FORMAT_R32G32B32_SFLOAT ||
                     out_position_format == VK_FORMAT_R32G32B32A32_SFLOAT)
                effective_input = VertexInputMode::Vec3Position;

            switch (effective_input)
            {
            case VertexInputMode::Vec3Position:
                out_vertex_input_glsl += "#include \"vertex/s1_position_vec3.glsl\"\n";
                break;
            case VertexInputMode::Vec2IntPosition:
                out_vertex_input_glsl += "#include \"vertex/s1_position_vec2i.glsl\"\n";
                break;
            case VertexInputMode::Vec2Position:
                out_vertex_input_glsl += "#include \"vertex/s1_position_vec2.glsl\"\n";
                break;
            case VertexInputMode::Procedural:
                out_vertex_input_glsl += "#include \"vertex/s1_input_procedural.glsl\"\n";
                break;
            default:
                return false;
            }
        }
        else
        {
        for (int i = 0; i < definition.vertex_semantic_requirements.GetCount(); ++i)
        {
            const auto &requirement = definition.vertex_semantic_requirements[i];
            const VertexSemantic semantic =
                GetVertexSemanticFromGLSLCodeModuleSemantic(requirement.semantic);
            const char *name = GetVertexInputName(semantic);
            if (semantic == VertexSemantic::Color
             && definition.vertex_varying.emit_vertex_color_from_palette)
                name = "ColorIndex";
            const GeometryVertexAttributeFormat *attribute = geometry.Find(semantic);
            if (!name || !attribute)
                return false;

            int location = -1;
            for (uint32 index = 0; index < geometry.GetCount(); ++index)
            {
                if (geometry.Get(index) == attribute)
                {
                    location = static_cast<int>(index);
                    break;
                }
            }
            const char *const type = GetGLSLVertexInputType(
                attribute->format, attribute->vec_size);
            if (location < 0 || !type)
                return false;

            out_vertices.push_back({attribute->format, semantic});
            out_vertex_input_glsl += "layout(location=" + std::to_string(location)
                + ") in " + type + " " + name + ";\n";
            if (semantic == VertexSemantic::Position)
                out_position_format = attribute->format;
        }
        }

        return out_position_format != VK_FORMAT_UNDEFINED;
    }

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

VkFormat ResolveMaterialVertexSemanticFormat(const GeometryVertexFormat *gvf, VertexSemantic semantic, VkFormat fallback_format)
{
    if(!gvf)
        return fallback_format;

    const GeometryVertexAttributeFormat *attribute=gvf->Find(semantic);
    if(!attribute||attribute->format==VK_FORMAT_UNDEFINED)
        return fallback_format;

    return attribute->format;
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

bool PreviewMaterialVertexSemanticResolution(
    const GLSLCodeModuleRegistry &registry,
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request,
    GLSLCodeModuleResolutionResult &out_result)
{
    out_result.resolved = false;
    out_result.selections.Clear();
    out_result.diagnostics.Clear();

    if (definition.vertex_semantic_requirements.IsEmpty()
     || !request.geometry_vertex_format)
        return false;

    ValueArray<GLSLCodeModuleGeometryCapability> geometry_capabilities;
    if (!GLSLCodeModuleCapabilityResolver::BuildGeometryCapabilities(
            *request.geometry_vertex_format, geometry_capabilities))
        return false;

    const GLSLCodeModuleResolutionRequest resolution_request{
        definition.vertex_semantic_requirements.GetData(),
        static_cast<uint32>(definition.vertex_semantic_requirements.GetCount()),
        geometry_capabilities.GetData(),
        static_cast<uint32>(geometry_capabilities.GetCount()),
        nullptr,
        0
    };
    GLSLCodeModuleCapabilityResolver resolver;
    resolver.Resolve(registry, resolution_request, out_result);
    return true;
}

bool BuildResolvedMaterialVertexABI(
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request,
    MaterialResolvedVertexABI &out_abi)
{
    std::vector<SerializedVertexEntry> vertices;
    std::string vertex_input_glsl;
    VkFormat position_format = VK_FORMAT_UNDEFINED;
    GLSLCodeModuleResolutionResult resolution;
    if (!BuildResolvedVertexABI(definition, request, vertices, position_format,
                                vertex_input_glsl, resolution))
        return false;

    out_abi.position_format = position_format;
    out_abi.provider_graph_hash =
        GetGLSLCodeModuleProviderGraphHash(resolution);
    out_abi.vertex_entries.Clear();
    for (const SerializedVertexEntry &entry : vertices)
        out_abi.vertex_entries.Add(entry);
    out_abi.vertex_input_glsl = vertex_input_glsl.c_str();
    if (!ComposeGLSLCodeModuleProviderGraph(resolution, out_abi.provider_glsl))
        return false;
    return true;
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

GLSLCodeModuleRegistry &GetGLSLCodeModuleRegistry()
{
    static GLSLCodeModuleRegistry registry;
    static bool loaded = false;
    if (!loaded)
    {
        registry.LoadDirectory(ToOSString(mtl::GetShaderLibraryPath()));
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

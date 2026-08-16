#pragma once

#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/PipelineConfig.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <hgl/mtl/DescriptorSemantic.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/vk/VK.h>
#include <hgl/graph/glsl/GLSLCodeModule.h>
#include <hgl/mtl/VertexNodeConfigResolver.h>
#include <hgl/type/ValueArray.h>
#include <hgl/shadergen/ShaderStageBuildContext.h>
#include <hgl/shadergen/ShaderLinkSpec.h>
#include <hgl/mtl/SurfaceType.h>
#include <hgl/mtl/BlendMode.h>
#include <hgl/mtl/PassType.h>
#include <hgl/util/hash/FNV1a.h>
#include <hgl/type/String.h>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl
{
    constexpr const char DefaultMaterialDataSlotName[] = "mtl";

    // 逻辑纹理槽位（与具体 descriptor set/binding 解耦）。
    // Resolve 阶段会把这些语义槽映射到 bindless handle + 运行时索引。
    // SSBO/slot/binding 基础类型已迁移到 <hgl/graph/ssbo/SSBOTypes.h>
    // Recipe 中的纹理绑定声明（纯输入，不包含任何运行时句柄）。
    struct RecipeTextureBinding
    {
        std::string slot_name;                    // 目标语义槽名（snake_case，如 "base_color"）
        std::string resource_id;                   // 资源标识（路径/资产ID/逻辑名）
        uint32_t direct_value = 0;                // 直接写入 TextureLayerRow 的原始值（例如 array layer）
        bool use_direct_value = false;            // true 时忽略 resource_id，直接使用 direct_value
        bool required = false;                     // true 时缺失应触发显式错误
    };

    struct RecipeSSBOAssetBinding
    {
        std::string data_slot_name;
        uint32_t data_slot = DefaultMaterialDataSlot;
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32_t ssbo_id = 0;
        uint32_t data_index = 0;
        bool use_data_index = false;
        bool shared_across_instances = false;
    };

    // 每个材质的 SSBO slot 声明（由 MaterialDefinition 显式列出）。
    // index 即 data_slot；name 同时作为 GLSL 变量名与 C++ 绑定 key。
    struct DataSlotDeclaration
    {
        std::string name;           // GLSL 变量名 / C++ 绑定 key，如 "pbr_surface" / "pbr_surface_a"
        SSBOType    ssbo_type = SSBOType::UserDefined; // 数据结构语义
    };

    inline bool IsValidMaterialDataSlotName(const std::string &name) noexcept
    {
        if (name.empty())
            return false;

        const auto is_letter = [](const char c)
        {
            return (c >= 'a' && c <= 'z')
                || (c >= 'A' && c <= 'Z')
                || c == '_';
        };
        if (!is_letter(name[0]))
            return false;

        for (size_t i = 1; i < name.size(); ++i)
        {
            const char c = name[i];
            if (!is_letter(c) && !(c >= '0' && c <= '9'))
                return false;
        }
        return true;
    }

    // 纹理槽位能力声明（由 MaterialDefinition 显式列出）。
    // 供 Step C 的 Definition→SerializedDescriptorEntry 推导使用。
    // 无纹理材质此列表为空。

    // GLSL 采样器类型枚举，避免传裸字符串指针。
    // ToGLSLSamplerTypeName() 将此值转换为 GLSL 类型名称字符串。
    enum class GLSLSamplerType : uint8_t
    {
        Sampler2D = 0,        // "sampler2D"
        Sampler2DArray,       // "sampler2DArray"
        Sampler2DShadow,      // "sampler2DShadow"
        SamplerCube,          // "samplerCube"
        SamplerCubeArray,     // "samplerCubeArray"
        Sampler3D,            // "sampler3D"
    };

    inline const char *ToGLSLSamplerTypeName(const GLSLSamplerType t) noexcept
    {
        switch (t)
        {
        case GLSLSamplerType::Sampler2D:        return "sampler2D";
        case GLSLSamplerType::Sampler2DArray:   return "sampler2DArray";
        case GLSLSamplerType::Sampler2DShadow:  return "sampler2DShadow";
        case GLSLSamplerType::SamplerCube:      return "samplerCube";
        case GLSLSamplerType::SamplerCubeArray: return "samplerCubeArray";
        case GLSLSamplerType::Sampler3D:        return "sampler3D";
        }
        return "sampler2D";
    }

    struct TextureSlotDeclaration
    {
        std::string      name;                                        // 纹理槽名（snake_case，主键，如 "base_color"）
        TextureSlot      slot         = TextureSlot::BaseColor;       // 由 name 派生的内部枚举（契约层序列化用）
        GLSLSamplerType  sampler_type = GLSLSamplerType::Sampler2D;   // GLSL 采样器类型
        bool             required     = false;                         // true: 缺失应触发错误；false: 可选
    };

    // Policy for resolving a material vertex semantic. GeometryOnly and
    // AllowDerived share the same ABI builder; the resolver is activated when
    // a vertex code-module registry is supplied.
    enum class MaterialVertexProviderPolicy : uint8
    {
        Auto = 0,
        GeometryOnly,
        AllowDerived
    };

    inline GLSLCodeModuleSemantic GetGLSLCodeModuleSemanticFromVertexSemantic(
        const VertexSemantic semantic) noexcept
    {
        switch (semantic)
        {
        case VertexSemantic::Position:  return GLSLCodeModuleSemantic::Position;
        case VertexSemantic::Normal:    return GLSLCodeModuleSemantic::Normal;
        case VertexSemantic::Tangent:   return GLSLCodeModuleSemantic::Tangent;
        case VertexSemantic::Bitangent: return GLSLCodeModuleSemantic::Binormal;
        case VertexSemantic::Color:     return GLSLCodeModuleSemantic::Color;
        case VertexSemantic::Luminance: return GLSLCodeModuleSemantic::Luminance;
        case VertexSemantic::TexCoord:  return GLSLCodeModuleSemantic::UV0;
        case VertexSemantic::TransformID: return GLSLCodeModuleSemantic::TransformID;
        default:                        return GLSLCodeModuleSemantic::Unknown;
        }
    }

    inline GLSLCodeModuleSemanticRequirement MakeMaterialVertexSemanticRequirement(
        const VertexSemantic semantic) noexcept
    {
        GLSLCodeModuleSemanticRequirement requirement;
        requirement.source = GLSLCodeModuleCapabilitySource::ProducedSemantic;
        requirement.semantic = GetGLSLCodeModuleSemanticFromVertexSemantic(semantic);
        return requirement;
    }

    inline VertexSemantic GetVertexSemanticFromGLSLCodeModuleSemantic(
        const GLSLCodeModuleSemantic semantic) noexcept
    {
        switch (semantic)
        {
        case GLSLCodeModuleSemantic::Position:  return VertexSemantic::Position;
        case GLSLCodeModuleSemantic::Normal:    return VertexSemantic::Normal;
        case GLSLCodeModuleSemantic::Tangent:   return VertexSemantic::Tangent;
        case GLSLCodeModuleSemantic::Binormal:  return VertexSemantic::Bitangent;
        case GLSLCodeModuleSemantic::Color:     return VertexSemantic::Color;
        case GLSLCodeModuleSemantic::Luminance: return VertexSemantic::Luminance;
        case GLSLCodeModuleSemantic::UV0:       return VertexSemantic::TexCoord;
        case GLSLCodeModuleSemantic::TransformID: return VertexSemantic::TransformID;
        default:                                return VertexSemantic::Unknown;
        }
    }

    struct MaterialVertexVaryingConfig
    {
        bool emit_data_index_id = false;
        bool emit_vertex_color = false;
        bool emit_uv0 = false;
        bool emit_world_pos = false;
        bool emit_world_normal = false;
        bool emit_luminance = false;
        bool emit_frag_direction = false;
        bool use_transform_id_attr = false;
        bool emit_vertex_color_from_palette = false;
    };

    // MaterialDefinition 来源标记：区分 built-in 硬编码实现与未来的文件化实现。
    enum class MaterialDefinitionSourceKind : uint8_t
    {
        BuiltIn = 0,  // M_* 硬编码 creator（用于 fallback 与少量保底材质）
        File,         // 外部 MaterialDefinition 文件（未来主路径）
    };

    // MaterialDefinition 用途标签：辅助调试 / 统计 / fallback 降级判断。
    enum class MaterialDefinitionUsageTag : uint8_t
    {
        General = 0,   // 普通材质
        Fallback,      // 错误/缺失材质保底
        Debug,         // 调试/编辑器专用
        Text,          // 文字渲染专用
        Sky,           // 天空专用
    };

    enum class MaterialDefinitionBootstrapKind : uint8_t
    {
        None = 0,
        PureColor,
        TextAlphaBlend
    };

    struct MaterialDefinition
    {
        // ── Layer 1: MaterialDefinition = Capability Superset ─────────────────────────
        // 描述一个材质"能做什么"，由材质文件（未来）或 M_* 内置工厂注册。
        // 包含静态资源能力声明和渲染选项包络。
        // 不含任何运行时句柄或 Vulkan 对象。
        // ─────────────────────────────────────────────────────────────────────────────

        // Part-A: 基础语义/元信息
        std::string definition_id;                                   // 正式主键（字符串 ID / 未来文件名）
        std::string definition_name;                                 // 人类可读名称
        MaterialDefinitionSourceKind source_kind = MaterialDefinitionSourceKind::BuiltIn;         // 来源类型
        MaterialDefinitionUsageTag   usage_tag  = MaterialDefinitionUsageTag::General;            // 用途标签
        MaterialDefinitionBootstrapKind bootstrap_kind = MaterialDefinitionBootstrapKind::None;

        // Lod / 质量包络
        uint16_t default_lod   = 0;
        uint16_t lod_count     = 1;

        // Part-B: 材质 SSBO slot 显式声明（index == data_slot）
        // name 用于 GLSL 变量名 与 C++ SetMaterialDataSlotResource(name,...) 绑定。
        // 无 SSBO 的材质此列表为空。
        std::vector<DataSlotDeclaration> data_slot_decls;

        // Part-B3: UBO 资源能力声明。
        // 显式列出此材质可使用的标准 UBO（ViewportInfo/CameraInfo/SkyInfo/MaterialColorPalette）。
        // 2D/3D 都走这条声明链路。
        std::vector<UBODescriptorSemantic> ubo_requirements;

        // Part-B4: 纹理槽位能力声明。
        // 无纹理材质（PureColor、VertexColor 等）此列表为空。
        // sampler_type 区分 "sampler2D" vs "sampler2DArray" 等 GLSL 采样器变体。
        std::vector<TextureSlotDeclaration> texture_slot_decls;

        // Part-B5: Sampler 预设能力声明（统一注册机制）。
        // 列出此材质在 GLSL 中实际用到的 sampler 预设名（对应 ShaderLibrary/sampler.toml）。
        // ShaderGen 据此生成 "#define <name>Sampler <idx>u" 宏；名字缺失时保底索引 0。
        std::vector<std::string> sampler_names;

        // 材质根 GLSL 代码模块名（注册表唯一键；无数字 ID 轨道）。
        std::vector<AnsiString> code_module_requirements;

        // PCG 顶点节点配置（单一真源）
        VertexShaderNodeConfig vertex_node_config;

        // Unified shader ABI/program contract shared by built-in and
        // file-backed definitions. The generator must not infer this from
        // the material name.
        //
        ValueArray<GLSLCodeModuleSemanticRequirement> vertex_semantic_requirements;
        MaterialVertexProviderPolicy vertex_provider_policy = MaterialVertexProviderPolicy::Auto;
        // Canonical fragment assembly input.
        const char *fragment_source = nullptr;
        // Optional surface function include replacement used by compositor
        // templates (and harmless for raw sources without the marker).
        const char *fragment_surface_module = nullptr;
        // Optional material-source provider include used by lit compositor
        // templates. The provider owns material data and texture extraction.
        const char *fragment_material_source_module = nullptr;
        // Optional NTB provider include used by lit compositor templates.
        const char *fragment_ntb_module = nullptr;
        MaterialVertexVaryingConfig vertex_varying;
        SurfaceType compositor_surface = SurfaceType::Unlit;
        BlendMode compositor_blend = BlendMode::Opaque;
        PassType compositor_pass = PassType::ForwardOpaque;
        ResolvedMaterialRenderState default_render_state;
    };

    inline void SetMaterialFragmentSource(
        MaterialDefinition &definition,
        const char *source)
    {
        definition.fragment_source = source;
    }

    inline void ConfigureMaterialVertexSemanticContract(
        MaterialDefinition &definition,
        const GLSLCodeModuleSemanticRequirement *requirements,
        const uint32 requirement_count,
        const MaterialVertexProviderPolicy provider_policy =
            MaterialVertexProviderPolicy::Auto)
    {
        definition.vertex_semantic_requirements.Clear();
        definition.vertex_provider_policy = provider_policy;

        for (uint32 i = 0; i < requirement_count; ++i)
            definition.vertex_semantic_requirements.Add(requirements[i]);
    }

    // ── Layer 2: MaterialRecipe = Instance Input ──────────────────────────────────
    // 描述"这次渲染想要什么"。由上层作者按需填写，不含 Vulkan 句柄。
    // mtl_def_id 是唯一与 MaterialDefinition 对接的字段。
    // 调用 mtl::NormalizeRecipe() 后，definition 的默认资源与渲染状态会被合入此结构。
    // ─────────────────────────────────────────────────────────────────────────────
    // 纯声明式材质输入（不含 Vulkan/运行时句柄），是 ResolvedBindingTable 的上游输入。
    struct MaterialRecipe
    {
        std::string recipe_name;               // 配方名称（人类可读）
        std::string mtl_def_id;                // MaterialDefinition字符串主键（材质标识 / 未来文件名）
        std::string domain;                    // 资源/缓存域（用于隔离不同管线空间）
        VertexShaderNodeConfig vertex_node_config = MakeDefault3DNodeConfig();
        uint16_t material_lod = 0;            // 作者层选择的材质 LOD

        MaterialRenderStateOverrides render_state_overrides;

        std::vector<RecipeTextureBinding> textures; // 所有纹理语义绑定
        std::vector<RecipeSSBOAssetBinding> ssbo_assets; // 所有 SSBO 运行时绑定（name/slot/type/id/row）
    };

    inline ResolvedMaterialRenderState ResolveMaterialRenderState(
        const MaterialDefinition &definition,
        const MaterialRecipe &recipe) noexcept
    {
        ResolvedMaterialRenderState state = definition.default_render_state;

        const bool definition_masked =
            definition.compositor_blend == BlendMode::Masked
         || definition.compositor_pass == PassType::ForwardMasked;
        const bool definition_dithered =
            definition.compositor_blend == BlendMode::Dither
         || definition.compositor_pass == PassType::ForwardDither;
        const bool definition_transparent =
            definition.compositor_blend == BlendMode::Transparent;
        const bool definition_a2c =
            definition.compositor_blend == BlendMode::AlphaToCoverage
         || definition.compositor_pass == PassType::ForwardA2C;

        if (definition_masked)
            state.alpha_test = true;
        if (definition_dithered)
        {
            state.alpha_test = true;
            state.dither = true;
        }
        if (definition_transparent)
            state.pipeline_config.alpha_blend = true;
        if (definition_a2c)
            state.pipeline_config.alpha_to_coverage = true;

        const MaterialRenderStateOverrides &overrides = recipe.render_state_overrides;
        if (overrides.has_double_sided)
            state.double_sided = overrides.double_sided;
        if (overrides.has_alpha_test)
            state.alpha_test = overrides.alpha_test;
        if (overrides.has_alpha_cutoff)
            state.alpha_cutoff = overrides.alpha_cutoff;
        if (overrides.has_dither)
            state.dither = overrides.dither;
        if (overrides.has_pipeline_config
         || overrides.pipeline_config != MaterialPipelineConfig{})
            state.pipeline_config = overrides.pipeline_config;

        return state;
    }

    inline bool HasUBORequirement(const MaterialDefinition &def, UBODescriptorSemantic s) noexcept
    {
        for (const auto &r : def.ubo_requirements)
            if (r == s) return true;
        return false;
    }

    inline const RecipeSSBOAssetBinding *FindRecipeSSBOAssetBindingByKey(
        const MaterialRecipe &recipe,
        const char *data_slot_name,
        const uint32_t data_slot) noexcept
    {
        if (!data_slot_name || !*data_slot_name)
            return nullptr;

        for (const auto &asset : recipe.ssbo_assets)
        {
            if (asset.data_slot_name != data_slot_name || asset.data_slot != data_slot)
                continue;

            return &asset;
        }

        return nullptr;
    }

    inline SSBOType ResolveRecipeSSBOType(
        const MaterialRecipe &recipe,
        const char *data_slot_name,
        const uint32_t data_slot,
        const SSBOType authored_type) noexcept
    {
        if (authored_type != SSBOType::UserDefined)
            return authored_type;

        if (const auto *asset = FindRecipeSSBOAssetBindingByKey(
                recipe, data_slot_name, data_slot))
            return asset->ssbo_type;

        return authored_type;
    }

    inline const RecipeSSBOAssetBinding *FindRecipeSSBOAssetBinding(
        const MaterialRecipe &recipe,
        const char *data_slot_name,
        const uint32_t data_slot,
        const SSBOType ssbo_type) noexcept
    {
        const auto *asset = FindRecipeSSBOAssetBindingByKey(
            recipe, data_slot_name, data_slot);
        return asset && asset->ssbo_type == ssbo_type ? asset : nullptr;
    }

    inline bool UpsertRecipeSSBOAssetBinding(MaterialRecipe &recipe,
                                             const std::string &data_slot_name,
                                             const SSBOType ssbo_type,
                                             const uint32_t ssbo_id,
                                             const uint32_t data_slot = DefaultMaterialDataSlot,
                                             const uint32_t data_index = 0,
                                             const bool use_data_index = false,
                                             const bool shared_across_instances = false)
    {
        if (!IsValidMaterialDataSlotName(data_slot_name))
            return false;

        for (auto &asset : recipe.ssbo_assets)
        {
            if (asset.data_slot_name == data_slot_name
             && asset.data_slot == data_slot)
            {
                asset.data_slot_name = data_slot_name;
                asset.ssbo_type = ssbo_type;
                asset.ssbo_id = ssbo_id;
                asset.data_slot = data_slot;
                asset.data_index = data_index;
                asset.use_data_index = use_data_index;
                asset.shared_across_instances = shared_across_instances;
                return true;
            }
        }

        RecipeSSBOAssetBinding asset{};
        asset.data_slot_name = data_slot_name;
        asset.data_slot = data_slot;
        asset.ssbo_type = ssbo_type;
        asset.ssbo_id = ssbo_id;
        asset.data_index = data_index;
        asset.use_data_index = use_data_index;
        asset.shared_across_instances = shared_across_instances;
        recipe.ssbo_assets.emplace_back(std::move(asset));
        return true;
    }

    /**
     * CN: UpsertRecipeSSBOAssetBinding 的统一重载 —— 接受 SSBOBinding，
     *     无需将 type/id 分开传。配合 SSBOArrayAccessor::GetSSBOBinding() 使用：
     *       UpsertRecipeSSBOAssetBinding(recipe, name, accessor->GetSSBOBinding());
     * EN: Unified overload accepting SSBOBinding so type/id need not be passed separately.
     */
    inline bool UpsertRecipeSSBOAssetBinding(MaterialRecipe &recipe,
                                             const std::string &data_slot_name,
                                             const SSBOBinding &binding,
                                             const uint32_t data_slot = DefaultMaterialDataSlot)
    {
        return UpsertRecipeSSBOAssetBinding(
            recipe, data_slot_name, binding.ssbo_type, binding.ssbo_id, data_slot);
    }

    inline void ApplyBaseMaterialInfoDefaults(MaterialRecipe &recipe,
                                              const MaterialDefinition &definition,
                                              const bool overwrite_existing = false)
    {
        if (recipe.mtl_def_id.empty())
            recipe.mtl_def_id = definition.definition_id;

        if (definition.lod_count > 0 && recipe.material_lod >= definition.lod_count)
            recipe.material_lod = definition.default_lod;

    }

    inline uint64_t HashMaterialRecipe(const MaterialRecipe &recipe) noexcept
    {
        hgl::hash::FNV1aHasher64 h;

        h << recipe.recipe_name
          << recipe.mtl_def_id
          << recipe.domain;

        h << recipe.vertex_node_config
          << recipe.material_lod
          << recipe.render_state_overrides.has_double_sided
          << recipe.render_state_overrides.double_sided
          << recipe.render_state_overrides.has_alpha_test
          << recipe.render_state_overrides.alpha_test
          << recipe.render_state_overrides.has_alpha_cutoff
          << recipe.render_state_overrides.alpha_cutoff
          << recipe.render_state_overrides.has_dither
          << recipe.render_state_overrides.dither
          << recipe.render_state_overrides.has_pipeline_config
          << HashMaterialPipelineConfig(recipe.render_state_overrides.pipeline_config);

        const uint32_t texture_count = static_cast<uint32_t>(recipe.textures.size());
        h << texture_count;
        for (const auto &texture : recipe.textures)
        {
            h << texture.slot_name
              << texture.resource_id;
            h << texture.direct_value
              << texture.use_direct_value
              << texture.required;
        }

        const uint32_t ssbo_asset_count = static_cast<uint32_t>(recipe.ssbo_assets.size());
        h << ssbo_asset_count;
        for (const auto &asset : recipe.ssbo_assets)
        {
            h << asset.data_slot_name
              << asset.data_slot
              << asset.ssbo_type
              << asset.ssbo_id
              << asset.use_data_index
              << asset.shared_across_instances;
        }

        return h;
    }

}

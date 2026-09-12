#pragma once

#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/PipelineConfig.h>
#include <hgl/mtl/MeshShaderMode.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <hgl/mtl/DescriptorSemantic.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/vk/VK.h>
#include <hgl/mtl/ShaderCodeModule.h>
#include <hgl/mtl/VertexNodeConfigResolver.h>
#include <hgl/type/ValueArray.h>
#include <hgl/mtl/ShaderLinkSpec.h>
#include <hgl/mtl/MaterialVertexVaryingConfig.h>
#include <hgl/util/hash/FNV1a.h>
#include <hgl/type/String.h>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl
{
    constexpr const char DefaultMaterialPrivateDataName[] = "mtl_private_data";

    // Recipe 中按 material.toml 名称声明的纹理绑定（纯输入，不包含任何运行时句柄）。
    struct RecipeTextureBinding
    {
        std::string texture_name;                 // TOML/GLSL 纹理名（snake_case，如 "base_color"）
        std::string resource_id;                   // 资源标识（路径/资产ID/逻辑名）
        bool required = false;                     // true 时缺失应触发显式错误
        uint32_t array_layer = 0;                 // Texture2DArray layer; non-array must be zero.
    };

    // 一个 recipe 至多包含一个材质数据绑定。材质数据通过
    // MaterialSSBOBinding 的类型、物理 SSBO 和行 ID 定位。

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

    constexpr uint32_t DefaultMaterialTextureConfigurationCapacity = 1024u;

    enum class MaterialTextureFilterMode : uint8_t
    {
        Nearest = 0,
        Linear
    };

    enum class MaterialTextureWrapMode : uint8_t
    {
        Repeat = 0,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder,
        MirrorClampToEdge
    };

    enum class MaterialTextureSwizzle : uint8_t
    {
        Zero = 0,
        One,
        R,
        G,
        B,
        A
    };

    enum class MaterialTextureCompareOp : uint8_t
    {
        Never = 0,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    struct MaterialTextureSamplingOptions
    {
        MaterialTextureFilterMode mag_filter =
            MaterialTextureFilterMode::Linear;
        MaterialTextureFilterMode min_filter =
            MaterialTextureFilterMode::Linear;
        MaterialTextureFilterMode mipmap_mode =
            MaterialTextureFilterMode::Linear;
        MaterialTextureWrapMode wrap_u = MaterialTextureWrapMode::Repeat;
        MaterialTextureWrapMode wrap_v = MaterialTextureWrapMode::Repeat;
        MaterialTextureWrapMode wrap_w = MaterialTextureWrapMode::Repeat;
        MaterialTextureSwizzle swizzle_r = MaterialTextureSwizzle::R;
        MaterialTextureSwizzle swizzle_g = MaterialTextureSwizzle::G;
        MaterialTextureSwizzle swizzle_b = MaterialTextureSwizzle::B;
        MaterialTextureSwizzle swizzle_a = MaterialTextureSwizzle::A;
        MaterialTextureCompareOp compare_op =
            MaterialTextureCompareOp::Never;
        float mip_lod_bias = 0.0f;
        float min_lod = 0.0f;
        float max_lod = 15.0f;
        float max_anisotropy = 16.0f;
        bool anisotropy = false;
        bool has_sampler_override = false;
        bool has_swizzle_override = false;
    };

    inline bool operator==(
        const MaterialTextureSamplingOptions &lhs,
        const MaterialTextureSamplingOptions &rhs) noexcept
    {
        return lhs.mag_filter == rhs.mag_filter
            && lhs.min_filter == rhs.min_filter
            && lhs.mipmap_mode == rhs.mipmap_mode
            && lhs.wrap_u == rhs.wrap_u
            && lhs.wrap_v == rhs.wrap_v
            && lhs.wrap_w == rhs.wrap_w
            && lhs.swizzle_r == rhs.swizzle_r
            && lhs.swizzle_g == rhs.swizzle_g
            && lhs.swizzle_b == rhs.swizzle_b
            && lhs.swizzle_a == rhs.swizzle_a
            && lhs.compare_op == rhs.compare_op
            && lhs.mip_lod_bias == rhs.mip_lod_bias
            && lhs.min_lod == rhs.min_lod
            && lhs.max_lod == rhs.max_lod
            && lhs.max_anisotropy == rhs.max_anisotropy
            && lhs.anisotropy == rhs.anisotropy
            && lhs.has_sampler_override == rhs.has_sampler_override
            && lhs.has_swizzle_override == rhs.has_swizzle_override;
    }

    inline uint64_t HashMaterialTextureSamplingOptions(
        const MaterialTextureSamplingOptions &options) noexcept
    {
        hgl::hash::FNV1aHasher64 hasher;
        hasher << options.mag_filter
               << options.min_filter
               << options.mipmap_mode
               << options.wrap_u
               << options.wrap_v
               << options.wrap_w
               << options.swizzle_r
               << options.swizzle_g
               << options.swizzle_b
               << options.swizzle_a
               << options.compare_op
               << options.mip_lod_bias
               << options.min_lod
               << options.max_lod
               << options.max_anisotropy
               << options.anisotropy
               << options.has_sampler_override
               << options.has_swizzle_override;
        return hasher;
    }

    struct MaterialTextureReference
    {
        uint32_t descriptor_index = 0;    // Bindless descriptor index; 0 = unbound.
        uint32_t array_layer = 0;         // Non-array textures must keep this at zero.
    };

    static_assert(sizeof(MaterialTextureReference) == 8);

    inline bool operator==(
        const MaterialTextureReference &lhs,
        const MaterialTextureReference &rhs) noexcept
    {
        return lhs.descriptor_index == rhs.descriptor_index
            && lhs.array_layer == rhs.array_layer;
    }

    struct MaterialTextureDeclaration
    {
        std::string      name;                                        // TOML/GLSL texture key, such as "base_color".
        GLSLSamplerType  sampler_type = GLSLSamplerType::Sampler2D;   // GLSL sampler kind.
        bool             required     = false;                         // Missing binding is an explicit error.
        MaterialTextureSamplingOptions sampling;                       // Sampler/view overrides; not connected at stage 1.
    };

    struct MaterialTextureReferenceLayout
    {
        uint64_t layout_hash = 0;                                     // Texture keys + declaration order + sampler/policy.
        uint32_t reference_count = 0;                                 // Number of uvec2 fields in one configuration row.
        uint32_t row_stride = 0;                                      // 16-byte aligned GPU row stride.
        uint32_t max_configuration_count =
            DefaultMaterialTextureConfigurationCapacity;              // Live rows; pool row zero is reserved.

        bool HasReferences() const noexcept
        {
            return reference_count > 0;
        }
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

    inline ShaderCodeModuleSemantic GetShaderCodeModuleSemanticFromVertexSemantic(
        const VertexSemantic semantic) noexcept
    {
        switch (semantic)
        {
        case VertexSemantic::Position:  return ShaderCodeModuleSemantic::Position;
        case VertexSemantic::Normal:    return ShaderCodeModuleSemantic::Normal;
        case VertexSemantic::Tangent:   return ShaderCodeModuleSemantic::Tangent;
        case VertexSemantic::Bitangent: return ShaderCodeModuleSemantic::Binormal;
        case VertexSemantic::Color:     return ShaderCodeModuleSemantic::Color;
        case VertexSemantic::Luminance: return ShaderCodeModuleSemantic::Luminance;
        case VertexSemantic::TexCoord:  return ShaderCodeModuleSemantic::UV0;
        case VertexSemantic::TransformID: return ShaderCodeModuleSemantic::TransformID;
        default:                        return ShaderCodeModuleSemantic::Unknown;
        }
    }

    inline ShaderCodeModuleSemanticRequirement MakeMaterialVertexSemanticRequirement(
        const VertexSemantic semantic) noexcept
    {
        ShaderCodeModuleSemanticRequirement requirement;
        requirement.source = ShaderCodeModuleCapabilitySource::ProducedSemantic;
        requirement.semantic = GetShaderCodeModuleSemanticFromVertexSemantic(semantic);
        return requirement;
    }

    inline VertexSemantic GetVertexSemanticFromShaderCodeModuleSemantic(
        const ShaderCodeModuleSemantic semantic) noexcept
    {
        switch (semantic)
        {
        case ShaderCodeModuleSemantic::Position:  return VertexSemantic::Position;
        case ShaderCodeModuleSemantic::Normal:    return VertexSemantic::Normal;
        case ShaderCodeModuleSemantic::Tangent:   return VertexSemantic::Tangent;
        case ShaderCodeModuleSemantic::Binormal:  return VertexSemantic::Bitangent;
        case ShaderCodeModuleSemantic::Color:     return VertexSemantic::Color;
        case ShaderCodeModuleSemantic::Luminance: return VertexSemantic::Luminance;
        case ShaderCodeModuleSemantic::UV0:       return VertexSemantic::TexCoord;
        case ShaderCodeModuleSemantic::TransformID: return VertexSemantic::TransformID;
        case ShaderCodeModuleSemantic::Size:        return VertexSemantic::Size;
        default:                                return VertexSemantic::Unknown;
        }
    }

    // MaterialVertexVaryingConfig — see <hgl/mtl/MaterialVertexVaryingConfig.h>

    // MaterialDefinition 来源标记：区分 built-in 硬编码实现与未来的文件化实现。
    enum class MaterialDefinitionSourceKind : uint8_t
    {
        BuiltIn = 0,  // M_* 硬编码 creator（用于 fallback 与少量保底材质）
        File,         // 外部 MaterialDefinition 文件（未来主路径）
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
        MaterialDefinitionBootstrapKind bootstrap_kind = MaterialDefinitionBootstrapKind::None;

        // Part-B: 材质私有数据 SSBO（单一声明，名字为 DefaultMaterialPrivateDataName）。
        // MaterialSSBOType 是材质域专用枚举；不再混入通用 SSBOType。
        MaterialSSBOType material_private_data = MaterialSSBOType::PBRSurface;

        // Part-B3: UBO 资源能力声明。
        // 显式列出此材质可使用的标准 UBO（ViewportInfo/CameraInfo/SkyInfo/MaterialColorPalette）。
        // 2D/3D 都走这条声明链路。
        std::vector<DescriptorSemantic> ubo_requirements;

        // Part-B4: TOML-defined texture-reference declarations.
        // 无纹理材质（PureColor、VertexColor 等）此列表为空。
        // sampler_type 区分 "sampler2D" vs "sampler2DArray" 等 GLSL 采样器变体。
        std::vector<MaterialTextureDeclaration> texture_declarations;
        uint32_t texture_configuration_max_count =
            DefaultMaterialTextureConfigurationCapacity;

        // Part-B5: Sampler 预设能力声明（统一注册机制）。
        // 列出此材质在 GLSL 中实际用到的 sampler 预设名（对应 ShaderLibrary/sampler.toml）。
        // ShaderGen 据此生成 "#define <name>Sampler <idx>u" 宏；名字缺失时保底索引 0。
        std::vector<std::string> sampler_names;

        // Part-B6: 编译期 GLSL 宏定义。
        // 列出此材质需要注入到 GLSL 源码中的 #define 宏名（如 "TEXT_SDF_ENABLED"）。
        // ShaderGen 据此生成 "#define <name> 1" 注入到 fragment shader。
        std::vector<std::string> compile_defines;

        // 材质根 GLSL 代码模块名（注册表唯一键；无数字 ID 轨道）。
        std::vector<AnsiString> code_module_requirements;

        // PCG 顶点节点配置（单一真源）
        VertexShaderNodeConfig vertex_node_config;

        // Unified shader ABI/program contract shared by built-in and
        // file-backed definitions. The generator must not infer this from
        // the material name.
        //
        ValueArray<ShaderCodeModuleSemanticRequirement> vertex_semantic_requirements;
        MaterialVertexProviderPolicy vertex_provider_policy = MaterialVertexProviderPolicy::Auto;
        // Material-source provider capability. Render preparation maps it into
        // the caller-selected MaterialSourceProvider template root.
        const char *material_source_module = nullptr;
        // NTB provider capability. Render preparation maps it into the
        // caller-selected NTBProvider template root.
        const char *ntb_module = nullptr;
        MaterialVertexVaryingConfig vertex_varying;
        ResolvedMaterialRenderState default_render_state;

        // Mesh shader 模式（来自 TOML [mesh_shader] 段；缺省 = VertexPassthrough）
        MeshShaderMode mesh_shader_mode = MeshShaderMode::VertexPassthrough;
        uint32_t mesh_shader_max_invocations = 0;  // 0 = 使用 EmitMeshTemplateDocument 默认值

    };

    inline bool IsMaterialTextureArraySampler(
        const GLSLSamplerType sampler_type) noexcept
    {
        return sampler_type == GLSLSamplerType::Sampler2DArray
            || sampler_type == GLSLSamplerType::SamplerCubeArray;
    }

    inline bool IsValidMaterialTextureName(
        const std::string &name) noexcept
    {
        if (name.empty() || name.size() >= 64)
            return false;

        const auto is_first = [](const char c)
        {
            return (c >= 'a' && c <= 'z')
                || (c >= 'A' && c <= 'Z')
                || c == '_';
        };
        const auto is_rest = [&is_first](const char c)
        {
            return is_first(c) || (c >= '0' && c <= '9');
        };

        if (!is_first(name[0]))
            return false;

        for (size_t i = 1; i < name.size(); ++i)
        {
            if (!is_rest(name[i]))
                return false;
        }

        return true;
    }

    inline bool IsValidMaterialTextureName(
        const char *name) noexcept
    {
        return name && IsValidMaterialTextureName(std::string(name));
    }

    inline int FindMaterialTextureDeclaration(
        const MaterialDefinition &definition,
        const std::string &name) noexcept
    {
        for (size_t i = 0; i < definition.texture_declarations.size(); ++i)
        {
            if (definition.texture_declarations[i].name == name)
                return static_cast<int>(i);
        }
        return -1;
    }

    inline bool BuildMaterialTextureReferenceLayout(
        const MaterialDefinition &definition,
        MaterialTextureReferenceLayout &out_layout) noexcept
    {
        out_layout = {};
        out_layout.max_configuration_count =
            definition.texture_configuration_max_count;
        if (out_layout.max_configuration_count == 0)
            return false;

        const size_t declaration_count =
            definition.texture_declarations.size();
        const uint64_t raw_row_bytes =
            static_cast<uint64_t>(declaration_count)
            * sizeof(MaterialTextureReference);
        if (raw_row_bytes > static_cast<uint64_t>(hgl::HGL_U32_MAX) - 15u)
            return false;

        hgl::hash::FNV1aHasher64 hasher;
        hasher << static_cast<uint32_t>(declaration_count);
        for (size_t i = 0; i < declaration_count; ++i)
        {
            const MaterialTextureDeclaration &declaration =
                definition.texture_declarations[i];
            if (!IsValidMaterialTextureName(declaration.name))
                return false;

            for (size_t j = 0; j < i; ++j)
            {
                if (definition.texture_declarations[j].name
                    == declaration.name)
                    return false;
            }

            hasher << declaration.name
                   << declaration.sampler_type
                   << declaration.required;
        }

        out_layout.layout_hash = hasher;
        out_layout.reference_count =
            static_cast<uint32_t>(declaration_count);
        out_layout.row_stride = raw_row_bytes == 0
            ? 0
            : static_cast<uint32_t>((raw_row_bytes + 15u) & ~uint64_t(15u));
        return out_layout.layout_hash != 0;
    }

    inline void ConfigureMaterialVertexSemanticContract(
        MaterialDefinition &definition,
        const ShaderCodeModuleSemanticRequirement *requirements,
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
    // 纯声明式材质输入（不含 Vulkan/运行时句柄），是材质 runtime 的上游输入。
    struct MaterialRecipe
    {
        std::string recipe_name;               // 配方名称（人类可读）
        std::string mtl_def_id;                // MaterialDefinition字符串主键（材质标识 / 未来文件名）
        VertexShaderNodeConfig vertex_node_config = MakeDefault3DNodeConfig();

        MaterialRenderStateOverrides render_state_overrides;

        std::vector<RecipeTextureBinding> textures; // 所有纹理语义绑定
        std::vector<MaterialSSBOBinding> ssbo_assets; // 唯一材质数据运行时绑定（type/id/row）
    };

    inline ResolvedMaterialRenderState ResolveMaterialRenderState(
        const MaterialDefinition &definition,
        const MaterialRecipe &recipe) noexcept
    {
        ResolvedMaterialRenderState state = definition.default_render_state;

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

    inline bool HasUBORequirement(const MaterialDefinition &def, DescriptorSemantic s) noexcept
    {
        for (const auto &r : def.ubo_requirements)
            if (r == s) return true;
        return false;
    }

    /**
     * 从已 Normalize 的 recipe 重建解析后的渲染状态。
     *
     * NormalizeRecipe 会把 ResolveMaterialRenderState 的结果写回
     * render_state_overrides（has_* 全 true），此后 definition 不再参与——
     * 本函数与 ResolveMaterialRenderState(definition, normalized_recipe) 等价，
     * 供管线创建侧使用，避免为取渲染状态反查材质定义。
     */
    inline ResolvedMaterialRenderState GetNormalizedRecipeRenderState(
        const MaterialRecipe &recipe) noexcept
    {
        const MaterialRenderStateOverrides &overrides = recipe.render_state_overrides;

        ResolvedMaterialRenderState state{};
        state.double_sided = overrides.double_sided;
        state.alpha_test = overrides.alpha_test;
        state.alpha_cutoff = overrides.alpha_cutoff;
        state.dither = overrides.dither;
        state.pipeline_config = overrides.pipeline_config;
        return state;
    }

    /** recipe 是否已经 Normalize（render_state_overrides 被写回为权威值）*/
    inline bool IsRecipeNormalized(const MaterialRecipe &recipe) noexcept
    {
        const auto &overrides = recipe.render_state_overrides;
        return overrides.has_double_sided
            && overrides.has_alpha_test
            && overrides.has_alpha_cutoff
            && overrides.has_dither
            && overrides.has_pipeline_config;
    }

    inline const MaterialSSBOBinding *FindRecipeSSBOAssetBinding(
        const MaterialRecipe &recipe) noexcept
    {
        if (recipe.ssbo_assets.size() != 1)
            return nullptr;

        return &recipe.ssbo_assets.front();
    }

    inline const MaterialSSBOBinding *FindRecipeSSBOAssetBinding(
        const MaterialRecipe &recipe,
        const MaterialSSBOType ssbo_type) noexcept
    {
        const auto *asset = FindRecipeSSBOAssetBinding(recipe);
        return asset && asset->ssbo_type == ssbo_type ? asset : nullptr;
    }

    inline MaterialSSBOType ResolveRecipeSSBOType(
        const MaterialRecipe &recipe,
        const MaterialSSBOType authored_type) noexcept
    {
        if (authored_type != MaterialSSBOType::PBRSurface)
            return authored_type;

        if (const auto *asset = FindRecipeSSBOAssetBinding(recipe))
            return asset->ssbo_type;

        return authored_type;
    }

    inline bool UpsertRecipeSSBOAssetBinding(
        MaterialRecipe &recipe,
        const MaterialSSBOBinding &material_ssbo_binding)
    {
        if (!material_ssbo_binding.IsValid())
            return false;

        if (recipe.ssbo_assets.empty())
        {
            recipe.ssbo_assets.emplace_back(material_ssbo_binding);
            return true;
        }

        if (recipe.ssbo_assets.size() != 1)
            return false;

        MaterialSSBOBinding &asset = recipe.ssbo_assets.front();
        asset = material_ssbo_binding;
        return true;
    }

    // 纹理绑定 upsert（与 UpsertRecipeSSBOAssetBinding 对称）：
    // texture_name 已存在则原位更新，否则追加。
    inline bool UpsertRecipeTextureBinding(MaterialRecipe &recipe,
                                           const std::string &texture_name,
                                           const std::string &resource_id,
                                           const bool required,
                                           const uint32_t array_layer = 0)
    {
        if (texture_name.empty())
            return false;

        for (auto &binding : recipe.textures)
        {
            if (binding.texture_name != texture_name)
                continue;

            binding.resource_id = resource_id;
            binding.array_layer = array_layer;
            binding.required = required;
            return true;
        }

        RecipeTextureBinding binding{};
        binding.texture_name = texture_name;
        binding.resource_id = resource_id;
        binding.array_layer = array_layer;
        binding.required = required;
        recipe.textures.emplace_back(std::move(binding));
        return true;
    }

    inline void ApplyBaseMaterialInfoDefaults(MaterialRecipe &recipe,
                                              const MaterialDefinition &definition,
                                              const bool overwrite_existing = false)
    {
        if (recipe.mtl_def_id.empty())
            recipe.mtl_def_id = definition.definition_id;
    }

    inline uint64_t HashMaterialRecipe(const MaterialRecipe &recipe) noexcept
    {
        hgl::hash::FNV1aHasher64 h;

        h << recipe.recipe_name
          << recipe.mtl_def_id;

        h << recipe.vertex_node_config
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
            h << texture.texture_name
              << texture.resource_id;
            h << texture.array_layer
              << texture.required;
        }

        const uint32_t ssbo_asset_count = static_cast<uint32_t>(recipe.ssbo_assets.size());
        h << ssbo_asset_count;
        for (const auto &asset : recipe.ssbo_assets)
        {
            h << asset.ssbo_type
              << asset.ssbo_id;
        }

        return h;
    }

}

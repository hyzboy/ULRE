#pragma once

#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/graph/PipelinePreset.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <hgl/mtl/DescriptorSemantic.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/vk/VK.h>
#include <hgl/graph/glsl/GLSLCodeModule.h>
#include <hgl/type/ValueArray.h>
#include <hgl/shadergen/ShaderStageBuildSpec.h>
#include <hgl/shadergen/ShaderProgramLinkSpec.h>
#include <hgl/mtl/new/SurfaceType.h>
#include <hgl/mtl/new/BlendMode.h>
#include <hgl/mtl/new/PassType.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl
{
    constexpr uint32_t InvalidBuiltinMaterialCreatorIDHint = 0xffffffffu;
    constexpr const char DefaultMaterialSSBOName[] = "mtl";

    // 逻辑纹理槽位（与具体 descriptor set/binding 解耦）。
    // Resolve 阶段会把这些语义槽映射到 bindless handle + 运行时索引。
    // SSBO/slot/binding 基础类型已迁移到 <hgl/graph/ssbo/SSBOTypes.h>
    // Recipe 中的纹理绑定声明（纯输入，不包含任何运行时句柄）。
    struct RecipeTextureBinding
    {
        TextureSlot slot = TextureSlot::BaseColor; // 目标语义槽
        std::string resource_id;                   // 资源标识（路径/资产ID/逻辑名）
        uint32_t direct_value = 0;                // 直接写入 TextureLayerRow 的原始值（例如 array layer）
        bool use_direct_value = false;            // true 时忽略 resource_id，直接使用 direct_value
        bool required = false;                     // true 时缺失应触发显式错误
    };

    struct RecipeSSBOAssetBinding
    {
        std::string ssbo_name;
        uint32_t ssbo_slot = DefaultMaterialSSBOSlot;
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32_t ssbo_id = 0;
        uint32_t ssbo_element_index = 0;
        bool use_ssbo_element_index = false;
        bool shared_across_instances = false;
    };

    // 每个材质的 SSBO slot 声明（由 MaterialDefinition 显式列出）。
    // index 即 ssbo_slot；name 同时作为 GLSL 变量名与 C++ 绑定 key。
    struct MaterialSSBOSlotDecl
    {
        std::string name;           // GLSL 变量名 / C++ 绑定 key，如 "pbr_surface" / "pbr_surface_a"
        SSBOType    ssbo_type = SSBOType::UserDefined; // 数据结构语义
    };

    // 纹理槽位能力声明（由 MaterialDefinition 显式列出）。
    // 供 Step C 的 Definition→FixedDescriptorEntry 推导使用。
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

    struct MaterialTextureSlotDecl
    {
        TextureSlot      slot         = TextureSlot::BaseColor;       // 纹理语义槽
        GLSLSamplerType  sampler_type = GLSLSamplerType::Sampler2D;   // GLSL 采样器类型
        bool             required     = false;                         // true: 缺失应触发错误；false: 可选
        // GLSL binding name override. nullptr = derive from slot via GetTextureNameBySlot().
        const char *     name         = nullptr;
    };

    // Static vertex-input contract. This describes the shader ABI rather than
    // a runtime Vulkan binding.
    struct MaterialVertexAttributeDefinition
    {
        VertexSemantic semantic = VertexSemantic::Position;
        uint32 location = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
        const char *glsl_declaration = nullptr;

        bool operator==(const MaterialVertexAttributeDefinition &rhs) const noexcept
        {
            return semantic == rhs.semantic && location == rhs.location && format == rhs.format
                && glsl_declaration == rhs.glsl_declaration;
        }
    };

    // Policy for resolving a material vertex semantic. This is declarative only:
    // Phase 4.2 keeps the legacy ABI as the active production path.
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

    struct MaterialVertexVaryingConfig
    {
        bool emit_data_index_id = false;
        bool emit_texture_layer_id = false;
        bool texture_layer_id_uses_data_index = false;
        bool emit_vertex_color = false;
        bool emit_uv0 = false;
        bool emit_world_pos = false;
        bool emit_world_normal = false;
        bool emit_luminance = false;
        bool emit_frag_direction = false;
        bool use_transform_id_attr = false;
        bool emit_vertex_color_from_pattle = false;
    };

    enum class MaterialShaderDomain : uint8
    {
        Generic = 0,
        Screen2D,
        World3D
    };

    enum class MaterialFragmentProgramMode : uint8
    {
        DirectInclude = 0,
        Compositor
    };

    // BMI 来源标记：区分 built-in 硬编码实现与未来的文件化实现。
    enum class MaterialDefinitionSourceKind : uint8_t
    {
        BuiltIn = 0,  // M_* 硬编码 creator（用于 fallback 与少量保底材质）
        File,         // 外部 BMI 文件（未来主路径）
    };

    // BMI 用途标签：辅助调试 / 统计 / fallback 降级判断。
    enum class MaterialDefinitionUsageTag : uint8_t
    {
        General = 0,   // 普通材质
        Fallback,      // 错误/缺失材质保底
        Debug,         // 调试/编辑器专用
        Text,          // 文字渲染专用
        Sky,           // 天空专用
    };

    struct MaterialDefinition
    {
        // ── Layer 1: MaterialDefinition = Capability Superset ─────────────────────────
        // 描述一个材质"能做什么"，由材质文件（未来）或 M_* 内置工厂注册。
        // 包含资源需求声明（required_ssbo_assets）和渲染选项包络。
        // 不含任何运行时句柄或 Vulkan 对象。
        // ─────────────────────────────────────────────────────────────────────────────

        // Part-A: 基础语义/元信息
        std::string definition_id;                                   // 正式主键（字符串 ID / 未来文件名）
        std::string definition_name;                                 // 人类可读名称
        uint32_t    builtin_creator_id = InvalidBuiltinMaterialCreatorIDHint;         // 当前内置 creator 路由字段（enum 序号）
        MaterialDefinitionSourceKind source_kind = MaterialDefinitionSourceKind::BuiltIn;         // 来源类型
        MaterialDefinitionUsageTag   usage_tag  = MaterialDefinitionUsageTag::General;            // 用途标签

        // Lod / 质量包络
        uint16_t default_lod   = 0;
        uint16_t lod_count     = 1;

        // Part-B: 资源契约（Definition 侧的静态资产需求；运行时绑定由 recipe 提供）
        std::vector<RecipeSSBOAssetBinding> required_ssbo_assets;

        // Part-B2: 材质 SSBO slot 显式声明（index == ssbo_slot）
        // name 用于 GLSL 变量名 与 C++ SetMaterialSSBOResource(name,...) 绑定。
        // 无 SSBO 的材质此列表为空。
        std::vector<MaterialSSBOSlotDecl> ssbo_slot_decls;

        // Part-B3: UBO 资源能力声明。
        // 显式列出此材质可使用的标准 UBO（ViewportInfo/CameraInfo/SkyInfo/MaterialColorPalette）。
        // 2D/3D 都走这条声明链路。
        std::vector<UBODescriptorSemantic> ubo_requirements;

        // Part-B4: 纹理槽位能力声明。
        // 无纹理材质（PureColor3D、VertexColor3D 等）此列表为空。
        // sampler_type 区分 "sampler2D" vs "sampler2DArray" 等 GLSL 采样器变体。
        std::vector<MaterialTextureSlotDecl> texture_slot_decls;

        std::vector<GLSLCodeModuleID> code_module_requirements;

        // PCG 顶点节点配置（单一真源）
        VertexShaderNodeConfig vertex_node_config;

        // Unified shader ABI/program contract shared by built-in and
        // file-backed definitions. The generator must not infer this from
        // the material name.
        //
        // Phase 4 transition state: vertex_semantic_requirements is the
        // format-free, file-serializable contract. vertex_attributes remains
        // the active legacy ABI until resolver-backed generation is enabled.
        ValueArray<GLSLCodeModuleSemanticRequirement> vertex_semantic_requirements;
        MaterialVertexProviderPolicy vertex_provider_policy = MaterialVertexProviderPolicy::Auto;
        ValueArray<MaterialVertexAttributeDefinition> vertex_attributes;
        ShaderStageBuildSpec vertex_stage;
        ShaderStageBuildSpec fragment_stage;
        ShaderProgramLinkSpec program_link;
        const char *fragment_program_module = nullptr;
        const char *fragment_surface_module = nullptr;
        MaterialVertexVaryingConfig vertex_varying;
        MaterialShaderDomain shader_domain = MaterialShaderDomain::Generic;
        MaterialFragmentProgramMode fragment_program_mode = MaterialFragmentProgramMode::Compositor;
        SurfaceType compositor_surface = SurfaceType::Unlit;
        BlendMode compositor_blend = BlendMode::Opaque;
        PassType compositor_pass = PassType::ForwardOpaque;
    };

    inline void ConfigureMaterialShaderContract(
        MaterialDefinition &definition,
        const char *fragment_module,
        const MaterialShaderDomain shader_domain,
        const MaterialFragmentProgramMode fragment_program_mode,
        const MaterialVertexAttributeDefinition *attributes,
        const uint32 attribute_count,
        const ShaderStageInterfaceVariable *vertex_inputs,
        const uint32 vertex_input_count,
        const ShaderStageInterfaceVariable *stage_outputs,
        const uint32 stage_output_count,
        const MaterialVertexVaryingConfig &varying)
    {
        definition.fragment_program_module = fragment_module;
        definition.shader_domain = shader_domain;
        definition.fragment_program_mode = fragment_program_mode;
        definition.vertex_stage.stage = ShaderStage::Vertex;
        definition.fragment_stage.stage = ShaderStage::Fragment;
        definition.vertex_varying = varying;

        for (uint32 i = 0; i < attribute_count; ++i)
            definition.vertex_attributes.Add(attributes[i]);

        for (uint32 i = 0; i < vertex_input_count; ++i)
            definition.vertex_stage.inputs.Add(vertex_inputs[i]);

        for (uint32 i = 0; i < stage_output_count; ++i)
        {
            definition.vertex_stage.outputs.Add(stage_outputs[i]);
            definition.fragment_stage.inputs.Add(stage_outputs[i]);
        }

        definition.program_link.vertex_stage = definition.vertex_stage.BuildKey();
        definition.program_link.fragment_stage = definition.fragment_stage.BuildKey();
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

    inline void ConfigureMaterialDefinitionContract(
        MaterialDefinition &definition,
        const char *fragment_module,
        const char *surface_module,
        const VkFormat position_format,
        const MaterialVertexVaryingConfig &varying,
        const MaterialVertexAttributeDefinition *attributes,
        const uint32 attribute_count)
    {
        definition.fragment_program_module = fragment_module;
        definition.fragment_surface_module = surface_module;
        definition.fragment_program_mode = MaterialFragmentProgramMode::Compositor;
        definition.vertex_varying = varying;
        definition.vertex_stage.stage = ShaderStage::Vertex;
        definition.fragment_stage.stage = ShaderStage::Fragment;

        MaterialVertexAttributeDefinition position{};
        position.semantic = VertexSemantic::Position;
        position.location = 0;
        position.format = position_format;
        definition.vertex_attributes.Add(position);
        definition.vertex_stage.inputs.Add({
            0,
            (position_format == VK_FORMAT_R32G32_SFLOAT || position_format == VK_FORMAT_R32G32_SINT)
                ? ShaderStageValueType::Vec2 : ShaderStageValueType::Vec3,
            0, 0
        });

        for (uint32 i = 0; i < attribute_count; ++i)
        {
            definition.vertex_attributes.Add(attributes[i]);
            definition.vertex_stage.inputs.Add({0, ShaderStageValueType::Unknown,
                                                  attributes[i].location, 0});
        }

        uint32 location = 0;
        auto add_output = [&](const ShaderStageValueType type, const uint32 flags)
        {
            const ShaderStageInterfaceVariable value{0, type, location++, flags};
            definition.vertex_stage.outputs.Add(value);
            definition.fragment_stage.inputs.Add(value);
        };
        if (varying.emit_data_index_id)
            add_output(ShaderStageValueType::UInt, uint32(ShaderStageInterfaceFlags::Flat));
        if (varying.emit_texture_layer_id)
            add_output(ShaderStageValueType::UInt, uint32(ShaderStageInterfaceFlags::Flat));
        if (varying.emit_world_pos)
            add_output(ShaderStageValueType::Vec3, 0);
        if (varying.emit_world_normal)
            add_output(ShaderStageValueType::Vec3, 0);
        if (varying.emit_uv0)
            add_output(ShaderStageValueType::Vec2, 0);
        if (varying.emit_vertex_color || varying.emit_vertex_color_from_pattle)
            add_output(ShaderStageValueType::Vec4, 0);
        if (varying.emit_frag_direction)
            add_output(ShaderStageValueType::Vec3, 0);
        if (varying.emit_luminance)
            add_output(ShaderStageValueType::Float, 0);

        definition.program_link.vertex_stage = definition.vertex_stage.BuildKey();
        definition.program_link.fragment_stage = definition.fragment_stage.BuildKey();
    }

    // ── Layer 2: MaterialRecipe = Instance Input ──────────────────────────────────
    // 描述"这次渲染想要什么"。由上层作者按需填写，不含 Vulkan 句柄。
    // mtl_def_id 是唯一与 MaterialDefinition 对接的字段。
    // 调用 mtl::NormalizeRecipe() 后，definition 的默认值会被合入此结构。
    // ─────────────────────────────────────────────────────────────────────────────
    // 纯声明式材质输入（不含 Vulkan/运行时句柄），是 MaterializationSpec 的上游输入。
    struct MaterialRecipe
    {
        std::string recipe_name;               // 配方名称（人类可读）
        std::string mtl_def_id;                // MaterialDefinition字符串主键（材质标识 / 未来文件名）
        std::string domain;                    // 资源/缓存域（用于隔离不同管线空间）
        VertexShaderNodeConfig vertex_node_config = MakeDefault3DNodeConfig();
        uint16_t material_lod = 0;            // 作者层选择的材质 LOD

        bool double_sided = false; // 双面渲染开关
        bool alpha_test = false;   // 是否启用 alpha test
        float alpha_cutoff = 0.5f; // alpha test 阈值（alpha < cutoff 丢弃）
        hgl::graph::PipelinePreset pipeline_preset = hgl::graph::PipelinePreset::Auto; // Auto: 按 MaterialDefinition 推导

        std::vector<RecipeTextureBinding> textures; // 所有纹理语义绑定
        std::vector<RecipeSSBOAssetBinding> ssbo_assets; // 所有 SSBO 运行时绑定（name/slot/type/id/row）
    };

    inline bool HasUBORequirement(const MaterialDefinition &def, UBODescriptorSemantic s) noexcept
    {
        for (const auto &r : def.ubo_requirements)
            if (r == s) return true;
        return false;
    }

    inline const RecipeSSBOAssetBinding *FindRecipeSSBOAssetBinding(const MaterialRecipe &recipe,
                                                                    const char *ssbo_name,
                                                                    const SSBOType ssbo_type) noexcept
    {
        if (!ssbo_name || !*ssbo_name)
            return nullptr;

        for (const auto &asset : recipe.ssbo_assets)
        {
            if (asset.ssbo_name != ssbo_name)
                continue;

            if (asset.ssbo_type != SSBOType::UserDefined
             && ssbo_type != SSBOType::UserDefined
             && asset.ssbo_type != ssbo_type)
                continue;

            return &asset;
        }

        return nullptr;
    }

    inline const RecipeSSBOAssetBinding *FindRecipeSSBOAssetBindingBySlot(const MaterialRecipe &recipe,
                                                                          const uint32_t ssbo_slot,
                                                                          const SSBOType ssbo_type) noexcept
    {
        for (const auto &asset : recipe.ssbo_assets)
        {
            if (asset.ssbo_slot != ssbo_slot)
                continue;

            if (asset.ssbo_type != SSBOType::UserDefined
             && ssbo_type != SSBOType::UserDefined
             && asset.ssbo_type != ssbo_type)
                continue;

            return &asset;
        }

        return nullptr;
    }

    inline void UpsertRecipeSSBOAssetBinding(MaterialRecipe &recipe,
                                             const std::string &ssbo_name,
                                             const SSBOType ssbo_type,
                                             const uint32_t ssbo_id,
                                             const uint32_t ssbo_slot = DefaultMaterialSSBOSlot,
                                             const uint32_t ssbo_element_index = 0,
                                             const bool use_ssbo_element_index = false,
                                             const bool shared_across_instances = false)
    {
        for (auto &asset : recipe.ssbo_assets)
        {
            if (!ssbo_name.empty() && asset.ssbo_name == ssbo_name)
            {
                asset.ssbo_name = ssbo_name;
                asset.ssbo_type = ssbo_type;
                asset.ssbo_id = ssbo_id;
                asset.ssbo_slot = ssbo_slot;
                asset.ssbo_element_index = ssbo_element_index;
                asset.use_ssbo_element_index = use_ssbo_element_index;
                asset.shared_across_instances = shared_across_instances;
                return;
            }

            if (asset.ssbo_slot == ssbo_slot && asset.ssbo_type == ssbo_type)
            {
                if (!ssbo_name.empty())
                    asset.ssbo_name = ssbo_name;
                asset.ssbo_type = ssbo_type;
                asset.ssbo_id = ssbo_id;
                asset.ssbo_slot = ssbo_slot;
                asset.ssbo_element_index = ssbo_element_index;
                asset.use_ssbo_element_index = use_ssbo_element_index;
                asset.shared_across_instances = shared_across_instances;
                return;
            }
        }

        RecipeSSBOAssetBinding asset{};
        asset.ssbo_name = ssbo_name;
        asset.ssbo_slot = ssbo_slot;
        asset.ssbo_type = ssbo_type;
        asset.ssbo_id = ssbo_id;
        asset.ssbo_element_index = ssbo_element_index;
        asset.use_ssbo_element_index = use_ssbo_element_index;
        asset.shared_across_instances = shared_across_instances;
        recipe.ssbo_assets.emplace_back(std::move(asset));
    }

    /**
     * CN: UpsertRecipeSSBOAssetBinding 的统一重载 —— 接受 SSBOBinding，
     *     无需将 type/id 分开传。配合 SSBOArrayAccessor::GetSSBOBinding() 使用：
     *       UpsertRecipeSSBOAssetBinding(recipe, name, accessor->GetSSBOBinding());
     * EN: Unified overload accepting SSBOBinding so type/id need not be passed separately.
     */
    inline void UpsertRecipeSSBOAssetBinding(MaterialRecipe &recipe,
                                             const std::string &ssbo_name,
                                             const SSBOBinding &binding)
    {
        UpsertRecipeSSBOAssetBinding(recipe, ssbo_name, binding.ssbo_type, binding.ssbo_id);
    }

    inline void ApplyBaseMaterialInfoDefaults(MaterialRecipe &recipe,
                                              const MaterialDefinition &bmi,
                                              const bool overwrite_existing = false)
    {
        if (recipe.mtl_def_id.empty())
            recipe.mtl_def_id = bmi.definition_id;

        if (bmi.lod_count > 0 && recipe.material_lod >= bmi.lod_count)
            recipe.material_lod = bmi.default_lod;

        for (const auto &asset : bmi.required_ssbo_assets)
            UpsertRecipeSSBOAssetBinding(recipe, asset.ssbo_name, asset.ssbo_type, asset.ssbo_id);
    }

    inline uint64_t HashMaterialRecipe(const MaterialRecipe &recipe) noexcept
    {
        uint64 hash = hgl::hash::FNV1aInit<uint64>();

        if (!recipe.recipe_name.empty())
            hash = hgl::hash::FNV1aAppendBytes(hash, recipe.recipe_name.data(), recipe.recipe_name.size());
        if (!recipe.mtl_def_id.empty())
            hash = hgl::hash::FNV1aAppendBytes(hash, recipe.mtl_def_id.data(), recipe.mtl_def_id.size());
        if (!recipe.domain.empty())
            hash = hgl::hash::FNV1aAppendBytes(hash, recipe.domain.data(), recipe.domain.size());
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.vertex_node_config);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.material_lod);

        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.double_sided);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.alpha_test);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.alpha_cutoff);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, recipe.pipeline_preset);

        const uint32_t texture_count = static_cast<uint32_t>(recipe.textures.size());
        hash = hgl::hash::FNV1aAppendValueBytes(hash, texture_count);
        for (const auto &texture : recipe.textures)
        {
            hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.slot);
            if (!texture.resource_id.empty())
                hash = hgl::hash::FNV1aAppendBytes(hash, texture.resource_id.data(), texture.resource_id.size());
            hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.direct_value);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.use_direct_value);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.required);
        }

        const uint32_t ssbo_asset_count = static_cast<uint32_t>(recipe.ssbo_assets.size());
        hash = hgl::hash::FNV1aAppendValueBytes(hash, ssbo_asset_count);
        for (const auto &asset : recipe.ssbo_assets)
        {
            if (!asset.ssbo_name.empty())
                hash = hgl::hash::FNV1aAppendBytes(hash, asset.ssbo_name.data(), asset.ssbo_name.size());
            hash = hgl::hash::FNV1aAppendValueBytes(hash, asset.ssbo_slot);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, asset.ssbo_type);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, asset.use_ssbo_element_index);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, asset.shared_across_instances);
        }

        return static_cast<uint64_t>(hash);
    }
}

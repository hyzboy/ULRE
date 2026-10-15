#pragma once

#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/PipelineConfig.h>
#include <hgl/mtl/MeshShaderMode.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <hgl/graph/ssbo/GlobalSSBOTypes.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/common/TextureSamplerTypeDef.h>
#include <hgl/vk/VK.h>
#include <hgl/mtl/ShaderCodeModule.h>
#include <hgl/mtl/VertexNodeConfigResolver.h>
#include <hgl/type/ValueArray.h>
#include <hgl/mtl/ShaderLinkSpec.h>
#include <hgl/mtl/MaterialVertexVaryingConfig.h>
#include <hgl/util/hash/FNV1a.h>
#include <hgl/type/String.h>
#include <hgl/type/StrChar.h>
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
    // GlobalSSBOBinding 的类型、物理 SSBO 和行 ID 定位。

    // 纹理槽位能力声明（由 MaterialDefinition 显式列出）。
    // 无纹理材质此列表为空。

    // 采样器类型直接复用 hgl::graph::SamplerType —— 唯一真源与名字表在
    // inc/hgl/common/TextureSamplerTypeDef.h（GetSamplerTypeName / ParseSamplerType，
    // 匹配大小写不敏感 ⇒ 材质 TOML 可继续写 "Sampler2DArray" 这种作者写法）。
    // 不要再在材质侧另建一份采样器类型枚举：两份枚举的数值不同，会让 layout 哈希与
    // 名字↔值映射各说一套（2026-10-11 S3b 收敛）。

    constexpr uint32_t DefaultMaterialTextureConfigurationCapacity = 1024u;


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
        SamplerType      sampler_type = SamplerType::Sampler2D;       // GLSL sampler kind（真源见 TextureSamplerTypeDef.h）。
        bool             required     = false;                         // Missing binding is an explicit error.
        uint32_t         channels     = 0;                             // 0 = 由纹素格式决定；2 = 双通道法线(XY)，Z 由 shader 还原
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

    // ── 名字注册表（单一真源；T1 口径：名字↔值只此一处）──────────────
    //   GetMaterialVertexProviderPolicyName：枚举 → 名字（switch）
    //   ParseMaterialVertexProviderPolicy：名字 → 枚举（线性查表）
    // 新增策略只需改枚举 + 本 switch 一处（名字表随之自动覆盖）。
    inline constexpr uint32_t MaterialVertexProviderPolicyCount =
        static_cast<uint32_t>(MaterialVertexProviderPolicy::AllowDerived) + 1;

    inline const char *GetMaterialVertexProviderPolicyName(
        const MaterialVertexProviderPolicy policy) noexcept
    {
        switch (policy)
        {
        case MaterialVertexProviderPolicy::Auto:         return "Auto";
        case MaterialVertexProviderPolicy::GeometryOnly: return "GeometryOnly";
        case MaterialVertexProviderPolicy::AllowDerived: return "AllowDerived";
        }
        return "Auto";
    }

    inline bool ParseMaterialVertexProviderPolicy(
        const char *name, MaterialVertexProviderPolicy &out_policy) noexcept
    {
        if (!name || !name[0])
            return false;

        for (uint32_t i = 0; i < MaterialVertexProviderPolicyCount; ++i)
        {
            const MaterialVertexProviderPolicy policy =
                static_cast<MaterialVertexProviderPolicy>(i);
            if (hgl::strcmp(name, GetMaterialVertexProviderPolicyName(policy)) == 0)
            {
                out_policy = policy;
                return true;
            }
        }
        return false;
    }

    enum class MaterialVertexNormalMode : uint8
    {
        None = 0,
        // Emit a vertex normal only when geometry supplies one; providers
        // must implement their own face-normal fallback otherwise.
        OptionalFaceFallback
    };

    // ── 名字注册表（单一真源；T1 口径：名字↔值只此一处）──────────────
    // 2026-10-14（S3b-3）：原为 `MaterialDefinitionFile.cpp::ParseVertexNormalMode` 的本地表。
    inline constexpr uint32_t MaterialVertexNormalModeCount =
        static_cast<uint32_t>(MaterialVertexNormalMode::OptionalFaceFallback) + 1;

    inline const char *GetMaterialVertexNormalModeName(
        const MaterialVertexNormalMode mode) noexcept
    {
        switch (mode)
        {
        case MaterialVertexNormalMode::None:                return "None";
        case MaterialVertexNormalMode::OptionalFaceFallback: return "OptionalFaceFallback";
        }
        return "None";
    }

    inline bool ParseMaterialVertexNormalMode(
        const char *name, MaterialVertexNormalMode &out) noexcept
    {
        if (!name || !name[0])
            return false;

        for (uint32_t i = 0; i < MaterialVertexNormalModeCount; ++i)
        {
            const MaterialVertexNormalMode mode =
                static_cast<MaterialVertexNormalMode>(i);
            if (hgl::strcmp(name, GetMaterialVertexNormalModeName(mode)) == 0)
            {
                out = mode;
                return true;
            }
        }
        return false;
    }

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

    struct MaterialDefinition
    {
        // ── Layer 1: MaterialDefinition = Capability Superset ─────────────────────────
        // 描述一个材质"能做什么"，由 schema-3 MaterialDefinition 文件提供。
        // 包含静态资源能力声明和渲染选项包络。
        // 不含任何运行时句柄或 Vulkan 对象。
        // ─────────────────────────────────────────────────────────────────────────────

        // Part-A: 基础语义/元信息
        std::string definition_id;                                   // 正式主键（字符串 ID / 未来文件名）
        std::string definition_name;                                 // 人类可读名称

        // Part-B: 材质私有数据 SSBO（单一声明，名字为 DefaultMaterialPrivateDataName）。
        // GlobalSSBOType：统一的全局 SSBO 池类型（材质表面字段与 MeshDrawParams 共用）。
        GlobalSSBOType material_private_data = GlobalSSBOType::PBRSurface;

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

        // Unified shader ABI/program contract shared by all file-backed
        // definitions. The generator must not infer this from the material
        // name.
        //
        ValueArray<ShaderCodeModuleSemanticRequirement> vertex_semantic_requirements;
        MaterialVertexProviderPolicy vertex_provider_policy = MaterialVertexProviderPolicy::Auto;
        MaterialVertexNormalMode vertex_normal_mode = MaterialVertexNormalMode::None;
        // Material-source provider capability. Render preparation maps it into
        // the caller-selected MaterialSourceProvider template root.
        const char *material_source_module = nullptr;
        // Surface provider capability(Sky 渲染模板专用):声明后覆盖 Sky profile
        // 默认的 sky_minimal_surface,让纯 Surface 材质提供自己的天空采样。
        // name 须匹配 ShaderCodeModuleRegistry 注册名;include 为库内路径。
        const char *surface_module_name = nullptr;
        const char *surface_module_include = nullptr;
        // NTB provider capability. Render preparation maps it into the
        // caller-selected NTBProvider template root.
        const char *ntb_module = nullptr;
        // Ambient light provider capability(ForwardLit 渲染模板专用):声明后覆盖
        // Lit profile 默认的 indirect_sky_ambient —— 如 IBL 环境光照模块。
        // name 须匹配 ShaderCodeModuleRegistry 注册名;include 为库内路径。
        const char *ambient_module_name = nullptr;
        const char *ambient_module_include = nullptr;
        MaterialVertexVaryingConfig vertex_varying;
        ResolvedMaterialRenderState default_render_state;

        // Mesh shader 模式（来自 TOML [mesh_shader] 段；缺省 = VertexPassthrough）
        MeshShaderMode mesh_shader_mode = MeshShaderMode::VertexPassthrough;
        uint32_t mesh_shader_max_invocations = 0;  // 0 = 使用 EmitMeshTemplateDocument 默认值

    };

    // 数组型采样器（声明需 array_layer / 走 texture2DArray 一族）。收敛到 SamplerType
    // 的 18 项域后，所有 array 形式一并覆盖（含 shadow / MS 数组变体）。
    inline bool IsMaterialTextureArraySampler(
        const SamplerType sampler_type) noexcept
    {
        switch (sampler_type)
        {
        case SamplerType::Sampler1DArray:
        case SamplerType::Sampler2DArray:
        case SamplerType::SamplerCubeArray:
        case SamplerType::Sampler1DArrayShadow:
        case SamplerType::Sampler2DArrayShadow:
        case SamplerType::SamplerCubeArrayShadow:
        case SamplerType::Sampler2DMSArray:
            return true;

        default:
            return false;
        }
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

        const size_t declaration_count =
            definition.texture_declarations.size();

        // Textures are optional by definition: many built-in materials (e.g.
        // pure-color or debug-color surfaces) have no material texture refs at all.
        // In that case there is no reference row to allocate, and the layout must be
        // treated as a valid empty layout rather than a hard failure.
        if (declaration_count == 0)
        {
            out_layout.max_configuration_count =
                definition.texture_configuration_max_count > 0
                    ? definition.texture_configuration_max_count
                    : DefaultMaterialTextureConfigurationCapacity;
            return true;
        }

        out_layout.max_configuration_count =
            definition.texture_configuration_max_count;
        if (out_layout.max_configuration_count == 0)
            return false;

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
                   << declaration.required
                   << declaration.channels;
        }

        out_layout.layout_hash = hasher;
        out_layout.reference_count =
            static_cast<uint32_t>(declaration_count);
        out_layout.row_stride = raw_row_bytes == 0
            ? 0
            : static_cast<uint32_t>((raw_row_bytes + 15u) & ~uint64_t(15u));
        return out_layout.layout_hash != 0;
    }

    // ── Layer 2: MaterialRecipe = Instance Input ──────────────────────────────────
    // 描述"这次渲染想要什么"。由上层作者按需填写，不含 Vulkan 句柄。
    // mtl_def_id 是唯一与 MaterialDefinition 对接的字段。
    // 调用 mtl::NormalizeRecipe() 后，definition 的默认资源与渲染状态会被合入此结构。
    // ─────────────────────────────────────────────────────────────────────────────
    // 纯声明式材质输入（不含 Vulkan/运行时句柄），是材质 runtime 的上游输入。
    // ─────────────────────────────────────────────────────────────
    // 回退（错误）材质规则表（A7b）
    //
    // 语义（用户定稿）：「有几何但无材质来源」**不是剔除条件**，而是**材质错误** ——
    // 必须用保底材质渲染出来，并由**根颜色**标注错误种类。
    //
    // 本表是回退材质的唯一判据（数据驱动、有界枚举）：
    //   kind → 保底材质定义 ID（注册目录里的文件材质）+ 根颜色（RGBA）+ 稳定名。
    // 根颜色经 `MaterialRecipe::fallback_marker_color` 落到材质数据行
    // （`EmissiveSurface.color`）——与 `builtin/pure_color` 完全同一条下发路径，
    // 不新建并行机制。新增错误种类 = 加一行；下面的 static_assert 自动纳入检查。
    //
    // 保底外观（2026-10-12 用户拍板）：**材质错误**（`MissingMaterialSource` /
    //   `UnknownMaterialDefinition`）走**棋盘格** `builtin/checkerboard_3d` —— 它只
    //   依赖 Position 且 `provider_policy = GeometryOnly`（`normal_mode =
    //   OptionalFaceFallback`）⇒ 任意几何都画得出来；**非错误默认**（`None`）仍是
    //   纯色 `builtin/pure_color`（门 F.fallback-dimension-neutral 钉住）。
    //   口径来源（用户原话）：发售用全预编译 SPV、不再有 shader 合成器 ⇒ 材质配置
    //   写错只 LOG 报错 + 材质侧走错误材质处理路径（即棋盘格渲染）。
    //
    // TODO(未来·标记色 + 2D/3D 分派)：**棋盘格现在看不到错误色** ——
    //   `checkerboard_{2d,3d}_source.glsl` 没有 `@ulre require Resource MaterialData`，
    //   棋盘格两色写死灰度（`vec3(0.0)` / `vec3(0.35)`），所以本表的 `marker_color`
    //   虽然照旧下发到材质数据行（`EmissiveSurface.color`）却无人读。未来要做的：
    //     ① 让两套 checkerboard 源同样读 EmissiveSurface 标记色作为棋盘格两色基色
    //        （那时 `marker_color` / `fallback_marker_color` 才真正可见）；
    //     ② **2D/3D 必须区分**：2D 走 NDC/UV、3D 走面空间/三平面（沿用两者现有采样
    //        差异）⇒ 标记色要分别落到两套着色器，不能共用一套；届时回退 ID 还需按
    //        绘制维度分派（现在两条错误种类共用 3D 版 ⇒ 仍是维度无关的单一 ID）；
    //     ③ 门/契约：门 F 仍断言无参回退 ID == `builtin/pure_color`（本次未动）；
    //        门 case `S.material-fallback-definitions-exist` 自动遍历本表校验
    //        "定义已注册 + 只依赖 Position + GeometryOnly"（换 ID 写错它会兜底报红）。
    //   触发前提（用户 2026-10-01 实测）：**跑遍全部示例均正常触发 0 次**，现有示例
    //   不存在"有几何、无材质来源"的实体 ⇒ 该错误路径目前只有契约测试覆盖，
    //   无肉眼可验场景。
    // ─────────────────────────────────────────────────────────────
    enum class MaterialErrorKind : uint32_t
    {
        None = 0,                   ///< 无错误：正常配方（根颜色不参与着色）
        MissingMaterialSource,      ///< 有几何，但**无任何材质来源**（数据层配方与 asset 默认配方皆无）
        UnknownMaterialDefinition,  ///< 有材质来源，但其材质定义 ID 无法解析（落到同一保底材质）
        Count,
    };

    inline constexpr uint32_t MATERIAL_ERROR_KIND_COUNT =
        static_cast<uint32_t>(MaterialErrorKind::Count);

    struct FallbackMaterialRule
    {
        MaterialErrorKind kind;
        const char       *definition_id;    ///< 保底材质定义 ID（文件材质；必须是 ShaderLibrary/material 里存在的）
        float             marker_color[4];  ///< 根颜色：标注错误种类
        const char       *marker_name;      ///< 稳定名（日志 / 契约用）
    };

    inline constexpr FallbackMaterialRule kFallbackMaterialRules[MATERIAL_ERROR_KIND_COUNT] =
    {
        // 非错误回退：既有保底材质。`GetFallbackMaterialDefinitionID()` 的返回值
        // 就是这一条（门 F.fallback-dimension-neutral 钉住 builtin/pure_color）。
        { MaterialErrorKind::None,
          "builtin/pure_color",
          { 1.0f, 1.0f, 1.0f, 1.0f },
          "none" },

        // 有几何、无材质来源：棋盘格保底材质（标记色洋红；**暂未下发到棋盘格**，见上面 TODO）
        { MaterialErrorKind::MissingMaterialSource,
          "builtin/checkerboard_3d",
          { 1.0f, 0.0f, 1.0f, 1.0f },
          "missing_material_source" },

        // 材质定义 ID 不可解析（含配置写错）：棋盘格保底材质（标记色橙；**暂未下发**，见上面 TODO）
        { MaterialErrorKind::UnknownMaterialDefinition,
          "builtin/checkerboard_3d",
          { 1.0f, 0.5f, 0.0f, 1.0f },
          "unknown_material_definition" },
    };

    namespace detail
    {
        /// 规则表自检：① 每个 kind 恰好一条规则且与枚举**同序** ② 定义 ID / 名字非空
        /// ③ 根颜色四通道都在 [0,1]（越界值会写进 SSBO 行，属坏数据）
        constexpr bool CheckFallbackMaterialRules()
        {
            for (uint32_t i = 0; i < MATERIAL_ERROR_KIND_COUNT; ++i)
            {
                const FallbackMaterialRule &rule = kFallbackMaterialRules[i];

                if (static_cast<uint32_t>(rule.kind) != i)
                    return false;

                if (!rule.definition_id || rule.definition_id[0] == '\0')
                    return false;

                if (!rule.marker_name || rule.marker_name[0] == '\0')
                    return false;

                for (uint32_t c = 0; c < 4; ++c)
                    if (!(rule.marker_color[c] >= 0.0f
                       && rule.marker_color[c] <= 1.0f))
                        return false;
            }

            return true;
        }
    }

    static_assert(detail::CheckFallbackMaterialRules(),
                  "回退（错误）材质规则表不自洽：kind 与枚举不同序/缺规则、定义 ID 或名字为空、根颜色越界");

    inline const FallbackMaterialRule &GetFallbackMaterialRule(
        const MaterialErrorKind kind) noexcept
    {
        const uint32_t index = static_cast<uint32_t>(kind);

        return index < MATERIAL_ERROR_KIND_COUNT
            ? kFallbackMaterialRules[index]
            : kFallbackMaterialRules[0];
    }

    // ─────────────────────────────────────────────────────────────
    // 错误种类分类表（**事实 ⇒ 种类**）：同样数据驱动、无特例 if。
    // 未命中任何一行 = None（非材质错误 ⇒ 调用方保持既有失败语义）。
    // ─────────────────────────────────────────────────────────────
    struct MaterialErrorClassifyRule
    {
        bool              has_material_source;
        MaterialErrorKind kind;
    };

    inline constexpr MaterialErrorClassifyRule kMaterialErrorClassifyRules[] =
    {
        { false, MaterialErrorKind::MissingMaterialSource },
    };

    inline constexpr MaterialErrorKind ClassifyMaterialErrorKind(
        const bool has_material_source) noexcept
    {
        for (const MaterialErrorClassifyRule &rule : kMaterialErrorClassifyRules)
            if (rule.has_material_source == has_material_source)
                return rule.kind;

        return MaterialErrorKind::None;
    }

    struct MaterialRecipe
    {
        std::string recipe_name;               // 配方名称（人类可读）
        std::string mtl_def_id;                // MaterialDefinition字符串主键（材质标识 / 未来文件名）
        VertexShaderNodeConfig vertex_node_config = MakeDefault3DNodeConfig();

        MaterialRenderStateOverrides render_state_overrides;

        std::vector<RecipeTextureBinding> textures; // 所有纹理语义绑定
        GlobalSSBOBinding material_ssbo_binding; // 可选的唯一材质数据运行时绑定

        // A7b：本配方是否是**回退（错误）材质** —— 种类与根颜色同属配方身份。
        // `None` = 正常配方；非 None 由 `kFallbackMaterialRules`（本文件）唯一决定
        // 定义 ID 与根颜色。根颜色经材质数据行（EmissiveSurface.color）下发到
        // 着色器，与 builtin/pure_color 完全同一条路径。
        MaterialErrorKind fallback_error_kind = MaterialErrorKind::None;
        float             fallback_marker_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    };

    inline ResolvedMaterialRenderState ResolveMaterialRenderState(
        const MaterialDefinition &definition,
        const MaterialRecipe &recipe) noexcept
    {
        ResolvedMaterialRenderState state = definition.default_render_state;

        const MaterialRenderStateOverrides &overrides = recipe.render_state_overrides;
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

    /** recipe 是否已经 Normalize（render_state_overrides 被写回为权威值）*/
    inline bool IsRecipeNormalized(const MaterialRecipe &recipe) noexcept
    {
        const auto &overrides = recipe.render_state_overrides;
        return overrides.has_alpha_test
            && overrides.has_alpha_cutoff
            && overrides.has_dither
            && overrides.has_pipeline_config;
    }

    /**
     * 从已 Normalize 的 recipe 重建解析后的渲染状态；**未归一化 ⇒ false**（S4b/D4）。
     *
     * 不静默给值：未归一化时拷出来的是结构体默认值（如 alpha_cutoff 0.5 / cull BACK），
     * 与作者定义的默认值可能不同 ⇒ 那是**错值**，比显式失败难查得多。
     * 调用方负责报错（管线创建侧沿用既有文案 + 返回 nullptr）；本口只回答「可不可用」。
     *
     * NormalizeRecipe 会把 ResolveMaterialRenderState 的结果写回
     * render_state_overrides（has_* 全 true），此后 definition 不再参与 ——
     * 本函数与 ResolveMaterialRenderState(definition, normalized_recipe) 等价，
     * 供管线创建侧使用，避免为取渲染状态反查材质定义。
     */
    inline bool TryGetNormalizedRecipeRenderState(
        const MaterialRecipe &recipe,
        ResolvedMaterialRenderState &out_state) noexcept
    {
        if (!IsRecipeNormalized(recipe))
            return false;

        const MaterialRenderStateOverrides &overrides = recipe.render_state_overrides;

        ResolvedMaterialRenderState state{};
        state.alpha_test = overrides.alpha_test;
        state.alpha_cutoff = overrides.alpha_cutoff;
        state.dither = overrides.dither;
        state.pipeline_config = overrides.pipeline_config;
        out_state = state;
        return true;
    }

    inline GlobalSSBOType ResolveRecipeSSBOType(
        const MaterialRecipe &recipe,
        const GlobalSSBOType authored_type) noexcept
    {
        if (authored_type != GlobalSSBOType::PBRSurface)
            return authored_type;

        if (recipe.material_ssbo_binding.IsValid())
            return recipe.material_ssbo_binding.ssbo_type;

        return authored_type;
    }

    // 纹理绑定 upsert：texture_name 已存在则原位更新，否则追加。
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

    inline uint64_t HashMaterialRecipe(const MaterialRecipe &recipe) noexcept
    {
        hgl::hash::FNV1aHasher64 h;

        h << recipe.recipe_name
          << recipe.mtl_def_id;

        h << recipe.vertex_node_config
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

        const uint32_t ssbo_asset_count =
            recipe.material_ssbo_binding.IsValid() ? 1u : 0u;
        h << ssbo_asset_count;
        if (ssbo_asset_count != 0)
        {
            h << recipe.material_ssbo_binding.ssbo_type
              << recipe.material_ssbo_binding.ssbo_id;
        }

        // A7b：回退（错误）材质的种类与根颜色属于配方身份 —— 多个错误种类可能落到
        // **同一个**保底定义（builtin/pure_color），只按定义 ID 去重会让两种错误的
        // 运行时行互认（颜色串味）。data_index 不在本哈希内（见上），故种类必须显式入哈希。
        h << static_cast<uint32_t>(recipe.fallback_error_kind);
        for (uint32_t i = 0; i < 4; ++i)
            h << recipe.fallback_marker_color[i];

        return h;
    }

}

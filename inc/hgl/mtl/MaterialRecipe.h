#pragma once

#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/MaterialDomain.h>
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
        case VertexSemantic::Size:      return ShaderCodeModuleSemantic::Size;
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

        // Part-B0: 表面领域（Fixed-Code Domain Shaders）。
        // `domain = "StandardOpaque"` → 解析期按领域契约校验顶点语义与纹理槽
        // （见 <hgl/mtl/MaterialDomain.h>）。缺省 Custom = 不做契约校验，
        // 保持 schema-4 历史材质的完全向后兼容。
        MaterialDomain domain = MaterialDomain::Custom;

        // Part-B4: TOML-defined texture-reference declarations.
        // 无纹理材质（PureColor、VertexColor 等）此列表为空。
        // sampler_type 区分 "sampler2D" vs "sampler2DArray" 等 GLSL 采样器变体。
        std::vector<MaterialTextureDeclaration> texture_declarations;
        uint32_t texture_configuration_max_count =
            DefaultMaterialTextureConfigurationCapacity;

        // Part-B5: Sampler 预设能力声明（统一注册机制）。
        // 列出此材质在 GLSL 中实际用到的 sampler 预设名（对应 ShaderLibrary/sampler.toml）。
        // 程序构建侧据此生成 "#define <name>Sampler <idx>u" 宏；名字缺失时保底索引 0。
        std::vector<std::string> sampler_names;

        // Part-B6: 编译期 GLSL 宏定义。
        // 列出此材质需要注入到 GLSL 源码中的 #define 宏名（如 "TEXT_SDF_ENABLED"）。
        // 程序构建侧据此生成 "#define <name> 1" 注入到 fragment shader。
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
        const char *material_source_module_name = nullptr;
        // Surface provider capability(Sky 渲染模板专用):声明后覆盖 Sky profile
        // 默认的 sky_minimal_surface,让纯 Surface 材质提供自己的天空采样。
        // 名字必须匹配 @ulre name；include 路径由注册表来源解析。
        const char *surface_module_name = nullptr;
        // NTB provider capability. Render preparation maps it into the
        // caller-selected NTBProvider template root.
        const char *ntb_module_name = nullptr;
        // 注意：ambient light provider（`ambient_light_provider` 槽用哪个 GLSL 模块）
        // **不在材质里声明** —— 它是环境光的**算法轴**，由引擎/环境层决定
        // （`EnvironmentSystem::SetAmbientProvider` → `EnvironmentManager::Profile::ambient_module_name`），
        // 与同属环境层的 `ambient_env` 参数块配对。材质只声明表面能力。
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
    // 保底外观（2026-10-12 用户拍板）：**材质错误**（除 `None` 外的全部种类）走**棋盘格**
    //   `builtin/error_checkerboard_3d` —— 它只
    //   依赖 Position 且 `provider_policy = GeometryOnly`（`normal_mode =
    //   OptionalFaceFallback`）⇒ 任意几何都画得出来；**非错误默认**（`None`）仍是
    //   纯色 `builtin/pure_color`（门 F.fallback-dimension-neutral 钉住）。
    //   口径来源（用户原话）：发售用全预编译 SPV、不再有 shader 合成器 ⇒ 材质配置
    //   写错只 LOG 报错 + 材质侧走错误材质处理路径（即棋盘格渲染）。
    //
    // 标记色（2026-10-24 完成，原 TODO ①）：本表的 `marker_color` 经材质数据行
    //   （`EmissiveSurface.color`）下发，由 `error_checkerboard_3d_source.glsl` 读回，
    //   作为棋盘格**两色的基色**（暗格 12% / 亮格 90%）⇒ 洋红 = 无材质来源、
    //   橙 = 材质定义 ID 无法解析、青 = 声明 required 却没给纹理、黄 = 几何缺顶点法线、
    //   蓝 = 几何缺纹理 UV，肉眼可直接分辨错误种类。
    //   为什么另立一个材质而不是改 `builtin/checkerboard_3d`：那个定义必须保持
    //   **零资源依赖**（不读 payload 行 ⇒ 不需要 `material_ssbo_binding`），它是回退链
    //   的最后一环。带标记色的版本声明 `require Resource MaterialData`，只有在回退路径
    //   拿到 marker 行时才成立；拿不到时 `AttachFallbackMarkerColor` 会把配方降级回
    //   `builtin/checkerboard_3d`（灰度、零依赖）⇒ 兜底永不失效。
    //
    // TODO(未来·2D/3D 分派)：② **2D/3D 必须区分**：2D 走 NDC/UV、3D 走面空间/三平面
    //   （沿用两者现有采样差异）⇒ 标记色要分别落到两套着色器，不能共用一套；届时回退
    //   规则表还需按绘制维度分派（现在全部错误种类共用 3D 版 ⇒ 仍是维度无关的单一 ID）。
    //     ③ 门/契约：门 F 仍断言无参回退 ID == `builtin/pure_color`（未动）；
    //        门 case `S.material-fallback-definitions-exist` 自动遍历本表校验
    //        "定义已注册 + 只依赖 Position + GeometryOnly"（换 ID 写错它会兜底报红）。
    //   可肉眼验证的触发场景：`example/Material/MaterialErrorFallback.cpp`
    //   （2026-10-24 新增；此前跑遍全部示例触发 0 次，该路径只有契约测试覆盖）。
    // ─────────────────────────────────────────────────────────────
    enum class MaterialErrorKind : uint32_t
    {
        None = 0,                   ///< 无错误：正常配方（根颜色不参与着色）
        MissingMaterialSource,      ///< 有几何，但**无任何材质来源**（数据层配方与 asset 默认配方皆无）
        UnknownMaterialDefinition,  ///< 有材质来源，但其材质定义 ID 无法解析（落到同一保底材质）
        MissingTextureResource,     ///< 材质定义声明了 `required` 纹理，但配方里没有实际纹理资源
        MissingVertexNormal,        ///< 材质要顶点法线光照（顶点法线 / NTB 模块），但几何顶点格式没有 Normal
        MissingTextureUV,           ///< 材质要纹理坐标（UV0），但几何顶点格式没有 TexCoord
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

        // 有几何、无材质来源：错误棋盘格（标记色洋红 → 棋盘格两色基色）
        { MaterialErrorKind::MissingMaterialSource,
          "builtin/error_checkerboard_3d",
          { 1.0f, 0.0f, 1.0f, 1.0f },
          "missing_material_source" },

        // 材质定义 ID 不可解析（含配置写错）：错误棋盘格（标记色橙 → 棋盘格两色基色）
        { MaterialErrorKind::UnknownMaterialDefinition,
          "builtin/error_checkerboard_3d",
          { 1.0f, 0.5f, 0.0f, 1.0f },
          "unknown_material_definition" },

        // 声明为 `required` 的纹理在配方里没有实际资源（写漏 / 资源 ID 为空）：
        // 错误棋盘格（标记色青 → 棋盘格两色基色）。
        //
        // 与另一类纹理错误的分工：**纹理名非法 / 重复 / 未声明** 仍是硬错误（图元剔除），
        // 那属于作者写坏 key（无法猜测意图）；而「声明了 required 却没给纹理」是
        // **缺内容**，读者需要看见它 ⇒ 用错误棋盘格标注出来。
        // 判据唯一来源见 `ClassifyMaterialResourceError`（本文件，紧随分类表）。
        { MaterialErrorKind::MissingTextureResource,
          "builtin/error_checkerboard_3d",
          { 0.0f, 1.0f, 1.0f, 1.0f },
          "missing_texture_resource" },

        // 几何级（顶点格式）错误 —— 与上面三种「配方/定义写错」不同，这两类的**定义与配方
        // 都合法**，错在**几何顶点格式与材质需求不匹配**（网格做错了 / 顶点格式选错了）。
        // 判据唯一来源见 `ClassifyMaterialGeometryError`（本文件，紧随资源级判据）。
        //
        // 语义（2026-10-24 用户定稿）：顶点法线缺失**也必须看得见** —— 它渲染出来会变成
        // 面法线平面着色，看起来「像正常的低模」，问题就被这层伪装掩掉了。所以走错误
        // 棋盘格（黄），而不是静默降级。
        { MaterialErrorKind::MissingVertexNormal,
          "builtin/error_checkerboard_3d",
          { 1.0f, 1.0f, 0.0f, 1.0f },
          "missing_vertex_normal" },

        // 「材质配了纹理 / 要采样纹理，但顶点没有 UV」：贴图会按垃圾 UV 采样（画面表现为
        // 随机花纹，比没贴图更难查）⇒ 错误棋盘格（蓝）。
        { MaterialErrorKind::MissingTextureUV,
          "builtin/error_checkerboard_3d",
          { 0.2f, 0.2f, 1.0f, 1.0f },
          "missing_texture_uv" },
    };

    /// 标记色行**不可得**时的降级保底材质：零资源依赖的灰度棋盘格。
    /// 与规则表的 `definition_id` 只差 fragment 模块（那份读标记色行，从而声明了
    /// `require Resource MaterialData`）⇒ 回退路径一旦拿不到 marker 行就必须退回这里，
    /// 否则"声明要 payload 行却没有 binding"会让图元被运行期校验剔除。
    /// 场景：无图形设备（单元测试）、SSBO 池未就绪或已满。
    inline constexpr const char *kMarkerlessFallbackMaterialDefinitionID =
        "builtin/checkerboard_3d";

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

    /**
     * 资源级材质错误判据（A7c）：材质定义声明了 `required` 纹理，但配方里没有实际资源。
     *
     * 判据**双向**（任一命中即错误）：
     *   1. 定义侧声明 `required` 的每个纹理名，配方里必须有绑定且 resource_id 非空；
     *   2. 配方侧自身标了 `required` 的绑定，其 resource_id 必须非空。
     *
     * **不在本函数判定**的纹理错误（仍属硬错误 ⇒ 图元剔除，不走回退）：
     * 纹理名非法 / 重复绑定 / 未声明的绑定 / 数组层错用 —— 那是作者写坏了 key，
     * 无法猜测意图；而「声明 required 却没给纹理」是**缺内容**，读者需要看见。
     *
     * 用量：数据层（`MaterialData::BuildResolvedRecipe`）在 NormalizeRecipe 之后调用，
     * 命中即 `BuildFallbackMaterialRecipe` 换成错误棋盘格（标记色青）。
     */
    inline MaterialErrorKind ClassifyMaterialResourceError(
        const MaterialDefinition &definition,
        const MaterialRecipe &recipe) noexcept
    {
        for (const auto &declaration : definition.texture_declarations)
        {
            if (!declaration.required)
                continue;

            bool bound = false;

            for (const auto &binding : recipe.textures)
            {
                if (binding.texture_name != declaration.name)
                    continue;

                bound = !binding.resource_id.empty();
                break;
            }

            if (!bound)
                return MaterialErrorKind::MissingTextureResource;
        }

        for (const auto &binding : recipe.textures)
        {
            if (binding.required && binding.resource_id.empty())
                return MaterialErrorKind::MissingTextureResource;
        }

        return MaterialErrorKind::None;
    }

    // ─────────────────────────────────────────────────────────────
    // 顶点语义需求判据（材质侧，单一口径）
    //
    // 这些谓词被三处共用，口径必须只有一份：
    //   · 程序构建侧（`MaterialProgramAssembler::PrepareResolvedDefinition`）—— 决定发射
    //     顶点法线还是面法线降级；
    //   · ECS 渲染层（`RenderPrimitiveCollectSystem`）—— 判定几何顶点格式错配（回退棋盘格）；
    //   · 契约测试（`TestRenderStrategyTable`）—— 钉住语义边界。
    // ─────────────────────────────────────────────────────────────

    /// 定义是否声明了某条顶点语义需求（`requirements = [...]` 里的项）。
    inline bool HasMaterialVertexSemanticRequirement(
        const MaterialDefinition &definition,
        const VertexSemantic semantic) noexcept
    {
        for (int i = 0; i < definition.vertex_semantic_requirements.GetCount(); ++i)
        {
            if (GetVertexSemanticFromShaderCodeModuleSemantic(
                    definition.vertex_semantic_requirements[i].semantic)
                == semantic)
                return true;
        }
        return false;
    }

    /**
     * 某条顶点语义需求是否被**当前 varying 配置**真正需要。
     *
     * 用途：把「领域契约要求什么」与「这份材质实际发什么」取交集。领域契约的
     * 语义位是**超集**（同一领域里不同材质可以只用到其中一部分），所以推导出来的
     * 需求必须先过这一层，否则一个不发 `emit_world_normal` 的 `StandardOpaque`
     * 材质会被强行要求顶点法线属性 —— 领域契约的过度收紧。
     *
     * 语义真源只此一份：程序构建侧的深度 pass 重过滤与材质解析期的推导都调它。
     * `default: true` 是刻意的：语义表里没有对应 varying 开关的项（`Size` / `AO` /
     * `Rotation`）既然被领域契约要求了，就不该被静默丢掉。
     */
    inline bool IsMaterialVertexSemanticRequiredForVarying(
        const VertexSemantic semantic,
        const MaterialVertexVaryingConfig &varying) noexcept
    {
        switch (semantic)
        {
        case VertexSemantic::Position:   return true;
        case VertexSemantic::Normal:     return varying.emit_world_normal;
        case VertexSemantic::Tangent:
        case VertexSemantic::Bitangent:  return false;
        case VertexSemantic::TexCoord:   return varying.emit_uv0;
        case VertexSemantic::Color:      return varying.emit_vertex_color
                                             || varying.emit_vertex_color_from_palette;
        case VertexSemantic::Luminance:  return varying.emit_luminance;
        case VertexSemantic::TransformID:return varying.use_transform_id_attr;
        default:                         return true;
        }
    }

    /**
     * 定义是否**容忍**面法线兜底（`normal_mode = "OptionalFaceFallback"`）。
     *
     * 容忍者（`builtin/checkerboard_3d` / `builtin/error_checkerboard_3d`）自带
     * `cross(dFdx,dFdy)` 分支 ⇒ **缺顶点法线不是错误**，它们必须能在任何几何上画出来。
     */
    inline bool MaterialToleratesFaceNormalFallback(
        const MaterialDefinition &definition) noexcept
    {
        return definition.vertex_normal_mode
            == MaterialVertexNormalMode::OptionalFaceFallback;
    }

    /**
     * 定义是否**必须要顶点法线**（缺了就是几何错配）。
     *
     * 判据：不容忍面法线兜底，**且**要法线光照 —— 声明了 Normal/Tangent/Bitangent
     * 任一语义，或挂了法线贴图模块（`ntb_module_name`）。
     *
     * 为什么不看 `normal_mode`：`lit.mi.toml` 的 `normal_mode` 是默认 `None`，
     * 却声明了 `Normal` + `ntb_tangent_vbo_normalmap` ⇒ 它缺顶点法线时 `s1_ntb*.glsl`
     * 会从没上传的 SSBO 槽读垃圾值（画面随机明暗，比没光更坏）。
     */
    inline bool MaterialRequiresVertexNormal(
        const MaterialDefinition &definition) noexcept
    {
        if (MaterialToleratesFaceNormalFallback(definition))
            return false;

        if (definition.ntb_module_name && *definition.ntb_module_name)
            return true;

        return HasMaterialVertexSemanticRequirement(definition, VertexSemantic::Normal)
            || HasMaterialVertexSemanticRequirement(definition, VertexSemantic::Tangent)
            || HasMaterialVertexSemanticRequirement(definition, VertexSemantic::Bitangent);
    }

    /// 定义是否**要法线光照**（含容忍面法线兜底者）：决定要不要输出世界法线 varying。
    /// `= 必须要顶点法线 ∨ 容忍面法线兜底`（后者也要 `si.worldNormal` 有个确定来源）。
    inline bool MaterialWantsNormalLighting(
        const MaterialDefinition &definition) noexcept
    {
        return MaterialRequiresVertexNormal(definition)
            || MaterialToleratesFaceNormalFallback(definition);
    }

    /// 定义是否**要纹理坐标**（声明了 `UV0` 需求）。缺 UV 时贴图会按垃圾 UV 采样。
    inline bool MaterialRequiresTextureCoordinate(
        const MaterialDefinition &definition) noexcept
    {
        return HasMaterialVertexSemanticRequirement(definition, VertexSemantic::TexCoord);
    }

    // ─────────────────────────────────────────────────────────────
    // 几何级材质错误判据（A7d）：**合法**定义 + **合法**配方，但几何顶点格式对不上需求。
    //
    // 事实由调用方从 `GeometryVertexFormat` 采集（本文件不认识 Vulkan 顶点格式）：
    // ECS 渲染层在 `RenderPrimitiveCollectSystem::BuildResolvedRecipe` 采集。
    // ─────────────────────────────────────────────────────────────
    struct MaterialGeometryFacts
    {
        bool has_geometry_format = false;   ///< false = 不知道顶点格式 ⇒ 不判定（宁可不报）
        bool has_vertex_normal   = false;   ///< 顶点格式含 Normal 语义
        bool has_texture_coord   = false;   ///< 顶点格式含 TexCoord 语义
    };

    /**
     * 判据顺序 = 严重度顺序：法线错配优先于 UV 错配（缺法线会连光照都不可信）。
     * 两者都命中只报第一个（一个图元一条标记色，颜色不可叠加）。
     *
     * 不在本函数判定：宏/着色器级别的缺失（纹理名非法、绑定重复等）—— 那些是硬错误，
     * 图元剔除，见 `ValidateMaterialRecipeForRuntime`。
     */
    inline MaterialErrorKind ClassifyMaterialGeometryError(
        const MaterialDefinition &definition,
        const MaterialGeometryFacts &facts) noexcept
    {
        if (!facts.has_geometry_format)
            return MaterialErrorKind::None;

        if (!facts.has_vertex_normal && MaterialRequiresVertexNormal(definition))
            return MaterialErrorKind::MissingVertexNormal;

        if (!facts.has_texture_coord && MaterialRequiresTextureCoordinate(definition))
            return MaterialErrorKind::MissingTextureUV;

        return MaterialErrorKind::None;
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

    /**
     * 着色器**变体**键 —— 与实例键 HashMaterialRecipe 分工不同，两者不可互换。
     *
     *  · HashMaterialRecipe            = 材质**实例行**身份（贴图、SSBO 行、回退标记色
     *                                    全都算）。换张贴图必须换行，所以这些字段必须在内。
     *  · HashMaterialRecipeShaderVariant = 材质**着色器变体**身份（只有会改变发射出的
     *                                    GLSL 字节的字段）。换张贴图 / 只改剔除模式
     *                                    **不得**换变体。
     *
     * 判据只有一条：该字段是否改变 GLSL 文本或 stage 接口。`src/ShaderProgramBuild` 里真正
     * 被消费的配方字段只有（见 MaterialProgramAssembler::ResolvePurposeAndCoverage、
     * BuildMaterialCoverageContract、ResolveMaterialVertexNodeConfig）：
     *   · coverage：alpha_test / alpha_cutoff / dither（宏 + varying 需求 + 模板路由）
     *   · alpha_blend（决定片元是否写出 alpha —— plan.alpha_to_output）
     *   · vertex_node_config
     * 定义身份（definition_id）由调用方折叠，这里不重复。
     *
     * 一律**不进**变体键的字段及理由：
     *   · recipe_name / 贴图 resource_id、array_layer / SSBO 行绑定
     *     —— 改的是描述符绑定与运行时材质行，不是 SPV；
     *   · pipeline_config 的纯 PSO 字段（cull/depth/blend factor/line width/
     *     overlay/wireframe）—— 改的是 VkPipeline，不是 SPV；
     *   · fallback_error_kind / fallback_marker_color —— 经材质数据行下发，不是源码常量。
     *
     * 反面代价（这就是 Stage 1 缺陷 #1/#3 的根因）：这些字段若被误判进变体键，
     * 同一条 program_link 会在烘焙清单里分裂成多条 lookup_key —— 烘焙期只触碰到
     * 其中一条，发行期用另一条查表即 miss，表现为「发行版少了某一批材质」。
     *
     * resolved_state 必须是 ResolveMaterialRenderState(definition, recipe) 的结果，
     * 不能由 recipe.render_state_overrides 直接读 —— 未归一化的配方靠 definition
     * 的默认值补足。
     */
    inline uint64_t HashMaterialRecipeShaderVariant(
        const MaterialRecipe &recipe,
        const ResolvedMaterialRenderState &resolved_state) noexcept
    {
        hgl::hash::FNV1aHasher64 h;

        h << recipe.vertex_node_config;

        h << resolved_state.alpha_test
          << resolved_state.alpha_cutoff
          << resolved_state.dither
          << resolved_state.pipeline_config.alpha_blend;

        return h;
    }

}

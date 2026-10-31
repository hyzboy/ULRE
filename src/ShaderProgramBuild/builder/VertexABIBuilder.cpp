// VertexABIBuilder.cpp — 顶点 ABI 求解（s1_* 模块选择 + VkFormat→GLSL 类型映射）
//
// 从 MaterialDefinitionRegistry.cpp 拆分（5 职责之一：vertex_abi）。
// 公开声明已随生成期入口一并收进 StageSPVCompiler.h（数据面头不再挂生成期能力）。

#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/mtl/StageSPVCompiler.h>
#include<hgl/graph/ShaderBufferSource.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/mtl/GlslModuleRegistry.h>
#include<hgl/mtl/ShaderLibraryPath.h>
#include<hgl/log/Log.h>
#include<hgl/type/String.h>
#include<cstring>

namespace hgl::graph::mtl{

namespace
{
    // ── 顶点流解码模块表 ────────────────────────────────────────────────────
    // 一条记录 = 「哪个几何属性格式 → 哪个 s1_* 解码模块」。选择规则统一为
    // 「精确格式命中优先，否则回落该流的 VK_FORMAT_UNDEFINED 默认行」，
    // 因此新增压缩格式只需在表里加一行，不需要在发射逻辑里加分支。
    //
    // 表顺序 = 发射顺序：数据流必须整体排在位置模块之前。位置模块内部 include 的
    // `vertex/s1_stream_defaults.glsl` 用 `#ifndef S1_STREAM_*` 补空实现，数据模块
    // 若排在它后面，自己定义的 `LoadXxx()` 会与已展开的空 stub 撞成重复定义。
    enum class VertexStreamSlot
    {
        UV,
        NTB,
        Color,
        Luminance,
        TransformID,
        Size,
    };

    struct VertexStreamModuleEntry
    {
        VertexStreamSlot slot;
        VkFormat format;          ///< VK_FORMAT_UNDEFINED = 该流的默认（无格式偏好）实现
        const char *module_name;  ///< ShaderLibrary/vertex/<name>.glsl
    };

    constexpr VertexStreamModuleEntry VERTEX_STREAM_MODULES[] =
    {
        // UV：按属性格式选解码模块（格式=模块，RG16F 发行版独立一模块）
        { VertexStreamSlot::UV, VK_FORMAT_R16G16_SFLOAT, "s1_uv_rg16f" },
        { VertexStreamSlot::UV, VK_FORMAT_UNDEFINED,     "s1_uv" },

        // NTB：按 Normal 属性格式选（发行版压缩格式各一模块）
        { VertexStreamSlot::NTB, VK_FORMAT_R8G8_UNORM,    "s1_ntb_rg8" },
        { VertexStreamSlot::NTB, VK_FORMAT_R16G16_SFLOAT, "s1_ntb_rg16f" },
        { VertexStreamSlot::NTB, PF_A2BGR10UN,            "s1_ntb_a2bgr10" },
        { VertexStreamSlot::NTB, VK_FORMAT_UNDEFINED,     "s1_ntb" },

        // 顶点色：调色板索引（VertexPalette 域）/ 直读 / 无属性时补白
        { VertexStreamSlot::Color, VK_FORMAT_R8_UINT,   "s1_palette_index" },
        { VertexStreamSlot::Color, VK_FORMAT_UNDEFINED, "s1_color" },

        { VertexStreamSlot::Luminance,   VK_FORMAT_UNDEFINED, "s1_luminance" },
        { VertexStreamSlot::TransformID, VK_FORMAT_UNDEFINED, "s1_transform_id" },
        { VertexStreamSlot::Size,        VK_FORMAT_UNDEFINED, "s1_size" },
    };

    // 无顶点色属性时的补白模块。它不装载任何流（不定义 S1_STREAM_*），只提供
    // 常量白的 `GetVertexColor()`，因此不进上表、也不受 LoadXxx 调用序列约束。
    constexpr const char *COLOR_DEFAULT_MODULE = "s1_color_default";

    // ── 位置流解码模块表 ────────────────────────────────────────────────────
    // 位置流的「几何属性格式 ↔ 解码模块 ↔ 分量数」是一体的：stride 与符号由
    // s1_position_* 模块内部硬编码（vec3 = 12B compact、vec2 = 8B、
    // vec2i = 4B packed int16x2），C++ 侧换个别名就等于换了 ABI，而生成器毫不知情。
    //
    // 这里曾经接受过一串"看起来同义"的格式别名：
    //   R32G32_SINT / R32G32_UINT   → Vec2IntPosition（读 4B 打包，实际 8B 输入）
    //   R16G16_UINT                 → Vec2IntPosition（无符号，模块按有符号扩展）
    //   R16G16_SFLOAT               → Vec2Position  （读 8B vec2，实际 4B 输入）
    //   R32G32B32A32_SFLOAT         → Vec3Position  （读 12B scalar vec3，实际 16B 输入）
    // 每一种都会产出「能编译、能跑、顶点全体错位」的着色器。
    // 因此只登记真正闭环的三对：精确格式匹配、无别名、无回退，
    // 新增格式必须同时新增解码模块并在此登记。
    struct PositionModuleEntry
    {
        VkFormat format;            ///< 几何 Position 属性格式（精确匹配）
        uint32_t component_count;   ///< 该模块隐含的分量数（Stage 2 position_mapping 对账用）
        const char *module_name;    ///< ShaderLibrary/vertex/<name>.glsl
    };

    constexpr PositionModuleEntry POSITION_MODULE_TABLE[] =
    {
        { VK_FORMAT_R32G32B32_SFLOAT, 3u, "s1_position_vec3"  },
        { VK_FORMAT_R32G32_SFLOAT,    2u, "s1_position_vec2"  },
        { VK_FORMAT_R16G16_SINT,      2u, "s1_position_vec2i" },
    };

    const PositionModuleEntry *FindPositionModule(VkFormat format)
    {
        for (const PositionModuleEntry &entry : POSITION_MODULE_TABLE)
            if (entry.format == format)
                return &entry;

        return nullptr;
    }

    bool AppendVertexModuleInclude(const char *module_name, AnsiString &out_glsl)
    {
        out_glsl += "#include \"vertex/";
        out_glsl += module_name;
        out_glsl += ".glsl\"\n";
        return true;
    }

    /// 按 (流, 属性格式) 查表发射 include；表里既无精确行也无默认行时返回 false
    bool AppendVertexStreamInclude(VertexStreamSlot slot, VkFormat format, AnsiString &out_glsl)
    {
        const VertexStreamModuleEntry *fallback = nullptr;

        for (const VertexStreamModuleEntry &entry : VERTEX_STREAM_MODULES)
        {
            if (entry.slot != slot)
                continue;

            if (entry.format == format)
                return AppendVertexModuleInclude(entry.module_name, out_glsl);

            if (entry.format == VK_FORMAT_UNDEFINED)
                fallback = &entry;
        }

        if (!fallback)
            return false;

        return AppendVertexModuleInclude(fallback->module_name, out_glsl);
    }

    bool BuildResolvedVertexABI(
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
        VkFormat &out_position_format,
        AnsiString &out_vertex_input_glsl)
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

        // SSBO 顶点输入：顶点输入由下方 s1_* 模块选择完成，无 provider 图。
        const GeometryVertexFormat &geometry = *request.geometry_vertex_format;
        out_vertex_input_glsl.Clear();
        out_position_format = VK_FORMAT_UNDEFINED;

        // SSBO 顶点输入：C++ 只做"选择"——按需求语义选 s1_* 模块，
        // 读取代码在模块内（gl_VertexIndex），无 VBO attribute 布局。
        {
            const GeometryVertexAttributeFormat *position_attribute =
                geometry.Find(VertexSemantic::Position);
            if (!position_attribute)
                return false;
            out_position_format = position_attribute->format;

            bool need_uv = false, need_ntb = false, need_luminance = false, need_transform_id = false, need_size = false;
            for (int i = 0; i < definition.vertex_semantic_requirements.GetCount(); ++i)
            {
                const auto &requirement = definition.vertex_semantic_requirements[i];
                const VertexSemantic semantic =
                    GetVertexSemanticFromGlslModuleSemantic(requirement.semantic);
                switch (semantic)
                {
                case VertexSemantic::TexCoord: need_uv = true; break;
                case VertexSemantic::Normal:
                case VertexSemantic::Tangent:
                case VertexSemantic::Bitangent: need_ntb = true; break;
                case VertexSemantic::Luminance: need_luminance = true; break;
                case VertexSemantic::TransformID: need_transform_id = true; break;
                case VertexSemantic::Size: need_size = true; break;
                default: break;  // Position 由几何 Position 格式查表决定
                }
            }

            // 顶点流装载模块按表选择（表顺序 = 发射顺序，数据流在前、位置模块在后）。
            const GeometryVertexAttributeFormat *uv_attribute =
                geometry.Find(VertexSemantic::TexCoord);
            const GeometryVertexAttributeFormat *normal_attribute =
                geometry.Find(VertexSemantic::Normal);
            const GeometryVertexAttributeFormat *color_attribute =
                geometry.Find(VertexSemantic::Color);

            const VkFormat uv_format     = uv_attribute     ? uv_attribute->format     : VK_FORMAT_UNDEFINED;
            const VkFormat normal_format = normal_attribute ? normal_attribute->format : VK_FORMAT_UNDEFINED;

            if (need_uv
             && !AppendVertexStreamInclude(VertexStreamSlot::UV, uv_format, out_vertex_input_glsl))
                return false;

            if (need_ntb
             && !AppendVertexStreamInclude(VertexStreamSlot::NTB, normal_format, out_vertex_input_glsl))
                return false;

            // Vertex color is a universal mesh input contract. Its provider is
            // selected from authored domain/geometry facts, never inferred from
            // the flattened Color semantic mask.
            if (definition.domain == MaterialDomain::VertexPalette)
            {
                if (!color_attribute || color_attribute->format != VK_FORMAT_R8_UINT)
                {
                    GLogError(u8"VertexPalette requires an R8_UINT palette-index color stream: definition=%s",
                              definition.definition_id.c_str());
                    return false;
                }

                if (!AppendVertexStreamInclude(VertexStreamSlot::Color, VK_FORMAT_R8_UINT, out_vertex_input_glsl))
                    return false;
            }
            else if (color_attribute)
            {
                if (!AppendVertexStreamInclude(VertexStreamSlot::Color, VK_FORMAT_UNDEFINED, out_vertex_input_glsl))
                    return false;
            }
            else
                AppendVertexModuleInclude(COLOR_DEFAULT_MODULE, out_vertex_input_glsl);

            if (need_luminance
             && !AppendVertexStreamInclude(VertexStreamSlot::Luminance, VK_FORMAT_UNDEFINED, out_vertex_input_glsl))
                return false;

            if (need_transform_id
             && !AppendVertexStreamInclude(VertexStreamSlot::TransformID, VK_FORMAT_UNDEFINED, out_vertex_input_glsl))
                return false;

            if (need_size
             && !AppendVertexStreamInclude(VertexStreamSlot::Size, VK_FORMAT_UNDEFINED, out_vertex_input_glsl))
                return false;

            // 位置模块只由几何 Position 格式决定（geometry 说了算，recipe/TOML 的
            // vertex input 仅兜底），查 POSITION_MODULE_TABLE 一次拿齐
            // 「解码模块 + 分量数」，不再维护三份平行 switch。
            const PositionModuleEntry *position_module = FindPositionModule(out_position_format);

            if (!position_module)
            {
                // input=None 表示不读外部顶点位置（CharQuad 一类自持 SSBO），格式对本路径无意义
                if (definition.vertex_node_config.input != VertexInputMode::None)
                {
                    GLogError(u8"顶点位置格式没有对应的 s1_position_* 解码模块：VkFormat=%d（definition=%s）",
                              static_cast<int>(out_position_format), definition.definition_id.c_str());
                    return false;
                }
            }
            else
            {
                // Stage 2 的映射是材质声明（position_mapping），Stage 1 的输入由几何格式决定
                // ——两个来源互不可见，分量数不一致会产出编不过的 GLSL。
                // 在发射期硬门，别让错误漂到 glslang 里。
                if (!VertexNodeConfigResolver::CheckPositionComponentCount(
                        definition.vertex_node_config, position_module->component_count,
                        definition.definition_id.c_str()))
                    return false;

                AppendVertexModuleInclude(position_module->module_name, out_vertex_input_glsl);
            }
        }

        return out_position_format != VK_FORMAT_UNDEFINED;
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

bool BuildResolvedMaterialVertexABI(
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request,
    MaterialResolvedVertexABI &out_abi)
{
    return BuildResolvedVertexABI(definition, request, out_abi.position_format,
                                  out_abi.vertex_input_glsl);
}

}//namespace hgl::graph::mtl

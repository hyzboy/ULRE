#include <hgl/mtl/BuiltinMaterialDefinitions.h>

#include <hgl/mtl/ShaderLibraryPath.h>

#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        // 内嵌源码刻意保持「与资产文件同形」的 schema-4 文本：注册时走同一条解析 /
        // 领域校验管线，领域契约因此仍是唯一真源（见头文件说明）。
        //
        // 与资产文件的唯一差别是注释口径：这些定义不属于资产，注释里不再出现
        // 「作者可填 / 资产只填」之类的话，避免读者误以为它们可被 .mi.toml 覆盖。

        constexpr const char kPureColorSource[] = R"TOML(schema = 4
id = "builtin/pure_color"

# 领域完整决定顶点语义要求、表面来源模块、顶点提供策略、纹理槽白名单。
domain = "UnlitSolid"

[vertex]
varyings = ["emit_data_index_id"]

[resources]
)TOML";

        constexpr const char kCheckerboard3DSource[] = R"TOML(schema = 4
id = "builtin/checkerboard_3d"

# 领域完整决定顶点语义要求、表面来源模块、顶点提供策略、纹理槽白名单。
domain = "Checker3D"

[vertex]
normal_mode = "OptionalFaceFallback"
varyings = ["emit_world_pos"]

[resources]
)TOML";

        constexpr const char kErrorCheckerboard3DSource[] = R"TOML(schema = 4
id = "builtin/error_checkerboard_3d"

# 领域完整决定顶点语义要求、表面来源模块、顶点提供策略、纹理槽白名单。
domain = "ErrorChecker3D"

[vertex]
normal_mode = "OptionalFaceFallback"
varyings = ["emit_data_index_id", "emit_world_pos"]

[resources]
)TOML";

        constexpr const char kTextGpuSource[] = R"TOML(schema = 4
id = "builtin/text_gpu"
name = "Text2D_GPU"

# 领域完整决定顶点语义要求、表面来源模块、顶点提供策略、纹理槽白名单。
domain = "Text2D"

[render_state]
pipeline = { alpha_blend = true }

[vertex]
varyings = ["emit_data_index_id", "emit_uv0", "emit_vertex_color", "emit_style_id"]

[mesh_shader]
mode = "CharQuad"

[resources]
# 槽形态由领域 Text2D 给出（Sampler2DArray / 必绑）。
# 采样器**过滤**不属领域：SDF 用 Linear、位图用 Nearest，两份定义本就要各写一行。
textures = [{ name = "base_color" }]
samplers = ["Linear"]
defines = ["TEXT_SDF_ENABLED"]
)TOML";

        constexpr const char kTextGpuBitmapSource[] = R"TOML(schema = 4
id = "builtin/text_gpu_bitmap"
name = "Text2D_GPU_Bitmap"

# 领域完整决定顶点语义要求、表面来源模块、顶点提供策略、纹理槽白名单。
domain = "Text2D"

[render_state]
pipeline = { alpha_blend = true }

[vertex]
varyings = ["emit_data_index_id", "emit_uv0", "emit_vertex_color"]

[mesh_shader]
mode = "CharQuad"

[resources]
# 槽形态由领域 Text2D 给出（Sampler2DArray / 必绑）。
textures = [{ name = "base_color" }]
samplers = ["Nearest"]
)TOML";

        // 顺序即注册顺序。**ID 绝不能改**：保底链与字体管线按字面量引用它们
        // （kFallbackMaterialRules / TextRenderPipeline）。
        const BuiltinMaterialDefinitionSource kBuiltinSources[] = {
            { OS_TEXT("builtin:pure_color"),
              "builtin/pure_color", kPureColorSource },
            { OS_TEXT("builtin:checkerboard_3d"),
              "builtin/checkerboard_3d", kCheckerboard3DSource },
            { OS_TEXT("builtin:error_checkerboard_3d"),
              "builtin/error_checkerboard_3d", kErrorCheckerboard3DSource },
            { OS_TEXT("builtin:text_gpu"),
              "builtin/text_gpu", kTextGpuSource },
            { OS_TEXT("builtin:text_gpu_bitmap"),
              "builtin/text_gpu_bitmap", kTextGpuBitmapSource },
        };
    }//namespace

    int GetBuiltinMaterialDefinitionCount() noexcept
    {
        return int(sizeof(kBuiltinSources) / sizeof(kBuiltinSources[0]));
    }

    const BuiltinMaterialDefinitionSource *GetBuiltinMaterialDefinitionSource(
        const int index) noexcept
    {
        if (index < 0 || index >= GetBuiltinMaterialDefinitionCount())
            return nullptr;
        return &kBuiltinSources[index];
    }

    const BuiltinMaterialDefinitionSource *FindBuiltinMaterialDefinitionSource(
        const char *definition_id) noexcept
    {
        if (!definition_id || !definition_id[0])
            return nullptr;
        for (int i = 0; i < GetBuiltinMaterialDefinitionCount(); ++i)
        {
            if (strcmp(kBuiltinSources[i].definition_id, definition_id) == 0)
                return &kBuiltinSources[i];
        }
        return nullptr;
    }
}//namespace hgl::graph::mtl

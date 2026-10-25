#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/CanonicalShaderContract.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    // 契约错误 X 列表（单一真源——枚举与 GetXxxErrorName 同源，新增错误只改此处）
#define HGL_MATERIAL_STAGE_INTERFACE_ERROR_LIST \
    HGL_ERROR(None) \
    HGL_ERROR(InvalidVaryingConfiguration) \
    HGL_ERROR(MissingSemanticMetadata) \
    HGL_ERROR(InvalidContract)

    enum class MaterialStageInterfaceError : uint8
    {
#define HGL_ERROR(name) name,
        HGL_MATERIAL_STAGE_INTERFACE_ERROR_LIST
#undef HGL_ERROR
    };

    struct MaterialStageInterfaceDiagnostic
    {
        MaterialStageInterfaceError error =
            MaterialStageInterfaceError::None;
        InterStageSemantic semantic = InterStageSemantic::Unknown;
    };

    const char *GetMaterialStageInterfaceErrorName(
        MaterialStageInterfaceError error) noexcept;

    InterStageSemanticMask GetMaterialInterStageSemanticMask(
        const mtl::MaterialVertexVaryingConfig &varying) noexcept;

    // ── varying ↔ interstage 语义：唯一定义表 ──────────────────────────
    //
    // 正反两个方向都从这一张表派生：
    //   varying → mask（登记自己声明了什么）
    //   mask + varying → varying（按「本次用途真正需要什么」裁剪）
    //
    // 此前裁剪是按语义逐项手写白名单，枚举一加新语义就静默漏项
    // （StyleID / WorldPosition / WorldNormal / FragDirection 长期拿不到），
    // 而 coverage.required_semantics 仍在要求它们 —— 生成出来的
    // depth/shadow program 不再满足自己的 coverage 契约，且不报错。
    //
    // 注意：`use_transform_id_attr` 不在表里。它是顶点属性选择（VS 从哪里取
    // 变换索引），不是跨阶段语义，裁剪时必须原样保留。
    //
    // Color 有两行：顶点色与调色板顶点色是两个输出字段、同一个语义。
    struct MaterialVaryingSemanticBinding
    {
        bool mtl::MaterialVertexVaryingConfig::*field;
        InterStageSemantic semantic;
    };

    inline constexpr MaterialVaryingSemanticBinding
        kMaterialVaryingSemanticBindings[] =
    {
        { &mtl::MaterialVertexVaryingConfig::emit_data_index_id,
          InterStageSemantic::DataIndexID },
        { &mtl::MaterialVertexVaryingConfig::emit_world_pos,
          InterStageSemantic::WorldPosition },
        { &mtl::MaterialVertexVaryingConfig::emit_world_normal,
          InterStageSemantic::WorldNormal },
        { &mtl::MaterialVertexVaryingConfig::emit_uv0,
          InterStageSemantic::UV0 },
        { &mtl::MaterialVertexVaryingConfig::emit_vertex_color,
          InterStageSemantic::Color },
        { &mtl::MaterialVertexVaryingConfig::emit_vertex_color_from_palette,
          InterStageSemantic::Color },
        { &mtl::MaterialVertexVaryingConfig::emit_frag_direction,
          InterStageSemantic::FragDirection },
        { &mtl::MaterialVertexVaryingConfig::emit_luminance,
          InterStageSemantic::Luminance },
        { &mtl::MaterialVertexVaryingConfig::emit_style_id,
          InterStageSemantic::StyleID },
    };

    inline InterStageSemanticMask BuildMaterialInterStageSemanticMaskFromVarying(
        const mtl::MaterialVertexVaryingConfig &varying) noexcept
    {
        InterStageSemanticMask mask = 0;
        for (const MaterialVaryingSemanticBinding &binding :
                kMaterialVaryingSemanticBindings)
        {
            if (varying.*(binding.field))
                mask |= GetInterStageSemanticMask(binding.semantic);
        }
        return mask;
    }

    /// 该语义在表里对应多个输出字段（Color：顶点色 / 调色板顶点色）。
    /// 掩码只能表达「要这个语义」，表达不了「要哪一个字段」，因此裁剪时
    /// 必须保留定义自己的选择，不能替它挑。
    inline bool IsAmbiguousMaterialVaryingSemantic(
        const InterStageSemantic semantic) noexcept
    {
        int count = 0;
        for (const MaterialVaryingSemanticBinding &binding :
                kMaterialVaryingSemanticBindings)
        {
            if (binding.semantic == semantic && ++count > 1)
                return true;
        }
        return false;
    }

    /// 就地裁剪：mask 是唯一判据 —— 要求了就置位（对应 material source 模块
    /// 求 alpha 时真的会读该 varying，与定义是否声明无关），没要求就清零。
    /// 表外语义（use_transform_id_attr）与多字段语义（Color）保持原值。
    inline void RestrictMaterialVertexVaryingToSemanticMask(
        mtl::MaterialVertexVaryingConfig &varying,
        const InterStageSemanticMask mask) noexcept
    {
        for (const MaterialVaryingSemanticBinding &binding :
                kMaterialVaryingSemanticBindings)
        {
            const bool required =
                (mask & GetInterStageSemanticMask(binding.semantic)) != 0;
            if (!required)
                varying.*(binding.field) = false;
            else if (!IsAmbiguousMaterialVaryingSemantic(binding.semantic))
                varying.*(binding.field) = true;
        }
    }

    bool BuildMaterialStageInterface(
        const mtl::MaterialVertexVaryingConfig &varying,
        ValueArray<InterStageSemanticContractEntry> &out_entries,
        MaterialStageInterfaceDiagnostic &out_diagnostic) noexcept;

    const InterStageSemanticContractEntry *FindMaterialStageInterfaceEntry(
        const ValueArray<InterStageSemanticContractEntry> &entries,
        InterStageSemantic semantic) noexcept;

    bool BuildGLSLInterStageDeclaration(
        const InterStageSemanticContractEntry &entry,
        const char *direction,
        AnsiString &out_declaration);

    bool BuildGLSLMaterialSurfaceInput(
        const ValueArray<InterStageSemanticContractEntry> &entries,
        bool camera_ubo_available,
        AnsiString &out_code);

    // 该语义在 mesh shader 里是 per-primitive（perprimitiveEXT）而非 per-vertex：
    // 每图元恒定一份（DataIndexID/StyleID——flat 且按图元共享）。
    // mesh 侧 out 数组尺寸 = max_primitives，FS 侧 in 必须同加 perprimitiveEXT。
    inline bool IsPerPrimitiveInterStageSemantic(
        const InterStageSemantic semantic) noexcept
    {
        return semantic == InterStageSemantic::DataIndexID
            || semantic == InterStageSemantic::StyleID;
    }
}

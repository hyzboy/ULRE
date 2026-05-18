#ifndef HGL_MTL_MATERIAL_VARIANT_DESC_H
#define HGL_MTL_MATERIAL_VARIANT_DESC_H

#include<hgl/mtl/MaterialPreset.h>
#include<hgl/common/CoordinateSystem.h>
#include<optional>
#include<string>

namespace hgl::graph::mtl
{
    struct MaterialVariantRow;

    /// MaterialVariantDesc - 材质变体的完整描述
    /// 包含着色器组装所需路径与调试标识等信息
    struct MaterialVariantDesc
    {
        // 标识
        std::string variant_name;           // 变体名称，用于日志和调试
        std::optional<MaterialPreset> factory_type;

        // 着色器模板路径
        std::string vs_template_path;       // 顶点着色器模板路径（相对 ShaderLibrary）
        std::string fs_template_path;       // 片段着色器模板路径（相对 ShaderLibrary）
        std::string surface_function_path;  // Surface include 路径（相对 ShaderLibrary）

        // Optional explicit row binding.
        // For builtin variants, row lookup should normally come from the registered builtin row table.
        // For custom descriptors, callers can bind a row directly to avoid legacy key-derived fallback.
        const MaterialVariantRow *bound_row = nullptr;

        // Phase 2: FS error indicator support.
        // When non-zero, the assembler will replace the normal surface function with
        // error_indicator_surface.glsl and bake this code into the FS as a compile-time
        // constant (R=reason, G=surface_model_id, B=tex_bits_lo).
        // Encode via ErrorCodeRegistry::EncodeFSError(); decode via FormatFSError().
        uint32_t fs_error_code = 0;

        // P7-3: 2D coordinate system for materials with VAB_Vec2 position inputs.
        // Controls COORD_ORTHO / COORD_ZERO_TO_ONE macro emission in BuildForwardVertexEntry.
        // Default NDC means no extra coordinate transform (pass-through).
        graph::CoordinateSystem2D coord_2d = graph::CoordinateSystem2D::NDC;

        MaterialVariantDesc()
        {
            factory_type.reset();
        }

        static MaterialVariantDesc CreateRowBound(const std::string &name,
                                                 const MaterialVariantRow *row,
                                                 const std::optional<MaterialPreset> &type = std::nullopt,
                                                 const std::string &vs_path = {},
                                                 const std::string &fs_path = {},
                                                 const std::string &surface_path = {})
        {
            MaterialVariantDesc desc;
            desc.variant_name = name;
            desc.factory_type = type;
            desc.vs_template_path = vs_path;
            desc.fs_template_path = fs_path;
            desc.surface_function_path = surface_path;
            desc.bound_row = row;
            return desc;
        }

        MaterialVariantDesc &BindRow(const MaterialVariantRow *row) noexcept
        {
            bound_row = row;
            return *this;
        }

        MaterialVariantDesc(
            const std::string& name,
            MaterialPreset type,
            const std::string& vs_path,
            const std::string& fs_path,
            const std::string& surface_path)
            : variant_name(name),
              factory_type(type),
              vs_template_path(vs_path),
              fs_template_path(fs_path),
              surface_function_path(surface_path),
              bound_row(nullptr)
        {
        }
    };

}

#endif // HGL_MTL_MATERIAL_VARIANT_DESC_H

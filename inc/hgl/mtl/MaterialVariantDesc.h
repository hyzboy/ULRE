#ifndef HGL_MTL_MATERIAL_VARIANT_DESC_H
#define HGL_MTL_MATERIAL_VARIANT_DESC_H

#include<hgl/mtl/MaterialPreset.h>
#include<optional>
#include<string>

namespace hgl::graph::mtl
{
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

        MaterialVariantDesc()
        {
            factory_type.reset();
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
              surface_function_path(surface_path)
        {
        }
    };

}

#endif // HGL_MTL_MATERIAL_VARIANT_DESC_H

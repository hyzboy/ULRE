#ifndef HGL_MTL_MATERIAL_VARIANT_DESC_H
#define HGL_MTL_MATERIAL_VARIANT_DESC_H

#include<hgl/mtl/MaterialPreset.h>
#include<string>

namespace hgl::graph::mtl
{
    /// MaterialVariantDesc - 材质变体的完整描述
    /// 包含着色器模板、MI 布局、描述符类型等实现细节
    struct MaterialVariantDesc
    {
        // 标识
        std::string variant_name;           // 变体名称，用于日志和调试
        MaterialPreset factory_type = MaterialPreset::VertexColor2D;
        bool has_factory_type = false;

        // 材质实例布局
        std::string mi_struct_name;         // MI 结构体名（如 StandardMaterialInstance）
        uint32 mi_struct_size;              // MI 结构体大小（字节）

        // 着色器模板路径
        std::string vs_template_path;       // 顶点着色器模板路径（相对 ShaderLibrary）
        std::string fs_template_path;       // 片段着色器模板路径（相对 ShaderLibrary）
        std::string surface_function_path;  // Surface 函数路径（相对 ShaderLibrary）

        // 描述符配置
        uint32 descriptor_binding_count;    // 总描述符 binding 数量

        // 特性标记
        bool requires_normal_map;           // 是否需要法线贴图
        bool requires_mr_map;               // 是否需要 Metallic+Roughness 贴图
        bool requires_vertex_color;         // 是否需要顶点色
        bool supports_2d_array;             // 是否支持 2D 数组纹理采样

        MaterialVariantDesc()
            : mi_struct_size(0),
              descriptor_binding_count(0),
              requires_normal_map(false),
              requires_mr_map(false),
              requires_vertex_color(false),
              supports_2d_array(false)
        {
        }

        MaterialVariantDesc(
            const std::string& name,
                        MaterialPreset type,
            uint32 mi_size,
            const std::string& vs_path,
            const std::string& fs_path,
            const std::string& surface_path,
            uint32 desc_count = 3)
            : variant_name(name),
                            factory_type(type),
              has_factory_type(true),
              mi_struct_size(mi_size),
              vs_template_path(vs_path),
              fs_template_path(fs_path),
              surface_function_path(surface_path),
              descriptor_binding_count(desc_count),
              requires_normal_map(false),
              requires_mr_map(false),
              requires_vertex_color(false),
              supports_2d_array(false)
        {
        }
    };

}

#endif // HGL_MTL_MATERIAL_VARIANT_DESC_H

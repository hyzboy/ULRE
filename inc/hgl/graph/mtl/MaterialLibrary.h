#pragma once

#include<hgl/vk/VK.h>
#include<hgl/graph/mtl/StdMaterial.h>

namespace hgl::graph::mtl{

enum class InlineMaterial:uint8
{
    VertexColor2D,
    PureColor2D,
    PureTexture2D,
    RectTexture2D,
    RectTexture2DArray,
    Text2D,

    PureColor3D,
    VertexColor3D,
    VertexLuminance3D,
    VertexPattleColor3D,
    Gizmo3D,
    TextureBlinnPhong,
    TerrainGrid,
    SkyMinimal,
    Billboard2D,
    BasicLit,

    ENUM_CLASS_RANGE(VertexColor2D,BasicLit)
};

/// 仅声明材质创建函数与 inline_material 名称常量，不产生工厂注册代码。
/// 用于配置头文件，使配置头文件无需引入工厂注册的副作用。
#define DECLARE_MATERIAL_CREATOR(name,cfg_type) \
namespace inline_material   \
{   \
    constexpr const char name[]=#name; \
}   \
\
MaterialCreateInfo *Create##name(const VulkanDevAttr *dev_attr,cfg_type *); \
\
inline MaterialCreateInfo *Create##name(const VulkanDevAttr *dev_attr)  \
{   \
    cfg_type cfg;   \
    return Create##name(dev_attr,&cfg);  \
}

MaterialCreateInfo *CreateMaterialCreateInfo(const VulkanDevAttr *dev_attr,const InlineMaterial mtl_id,MaterialCreateConfig *cfg);
const char *GetInlineMaterialName(const InlineMaterial mtl_id);

}//namespace hgl::graph::mtl


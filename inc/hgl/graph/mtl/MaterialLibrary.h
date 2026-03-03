#pragma once

#include<hgl/vk/VK.h>
#include<hgl/graph/mtl/StdMaterial.h>

namespace hgl::graph::mtl{

enum class MaterialPreset:uint8
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

/// 仅声明材质创建函数，不产生任何注册或全局常量副作用。
#define DECLARE_MATERIAL_CREATOR(name,cfg_type) \
MaterialCreateInfo *Create##name(const VulkanDevAttr *dev_attr,cfg_type *); \
\
inline MaterialCreateInfo *Create##name(const VulkanDevAttr *dev_attr)  \
{   \
    cfg_type cfg;   \
    return Create##name(dev_attr,&cfg);  \
}

MaterialCreateInfo *CreateMaterialCreateInfo(const VulkanDevAttr *dev_attr,const MaterialPreset mtl_id,MaterialCreateConfig *cfg);
const char *GetInlineMaterialName(const MaterialPreset mtl_id);

}//namespace hgl::graph::mtl


#pragma once

#include<hgl/vk/VK.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include<hgl/mtl/StdMaterial.h>
#include<hgl/mtl/new/MaterialVariantKey.h>
#include<vector>
#include<string>

namespace hgl::graph::mtl{

struct MaterialCreateConfig;
class MaterialCreateInfo;

enum class MaterialPreset:uint8
{
    VertexColor2D,
    PureColor2D,
    PureTexture2D,
    RectTexture2D,
    Text2D,

    PureColor3D,
    VertexColor3D,
    VertexLuminance3D,
    VertexPattleColor3D,
    Gizmo3D,
    TerrainGrid,
    SkyMinimal,
    Billboard2D,
    Standard,
    PBRColor3D,
    VertexLuminance2D,

    ENUM_CLASS_RANGE(VertexColor2D,VertexLuminance2D)
};

/// 仅声明材质创建函数，不产生任何注册或全局常量副作用。
#define DECLARE_MATERIAL_CREATOR(name,cfg_type) \
MaterialCreateInfo *Create##name(const contract::PhysicalDeviceProfileLite *profile,cfg_type *); \
\
inline MaterialCreateInfo *Create##name(const contract::PhysicalDeviceProfileLite *profile)  \
{   \
    cfg_type cfg;   \
    return Create##name(profile,&cfg);  \
}

MaterialCreateInfo *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialPreset mtl_id,
                                             MaterialCreateConfig *cfg);

MaterialCreateInfo *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialVariantKey &key,
                                             MaterialCreateConfig *cfg);

const char *GetMaterialPresetName(const MaterialPreset mtl_id);

// 启动期自检：校验内置变体模板是否可组装，失败诊断写入 diagnostics。
bool ValidateBuiltinMaterialVariants(const std::string &shader_library_path,
                                     std::vector<std::string> &diagnostics);

// Phase-A migration helpers: preset <-> variant mapping.
MaterialVariantKey MapPresetToVariantKey(const MaterialPreset mtl_id);

}//namespace hgl::graph::mtl


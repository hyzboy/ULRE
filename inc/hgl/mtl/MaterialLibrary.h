#pragma once

#include<hgl/vk/VK.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include<hgl/mtl/StdMaterial.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterializationSpec.h>
#include<hgl/mtl/MaterializationResolver.h>
#include<hgl/mtl/MaterializationPools.h>
#include<hgl/mtl/new/MaterialVariantKey.h>
#include<hgl/common/VertexAttribDef.h>

namespace hgl::graph::mtl{

struct MaterialCreateConfig;
class MaterialCreateInfo;

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

    SkyMinimal,
    Standard,
    StandardTextureArray,
    PBRColor3D,

    ENUM_CLASS_RANGE(VertexColor2D,PBRColor3D)
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

VkFormat ResolveMaterialVertexSemanticFormat(const MaterialCreateConfig *cfg, VertexSemantic semantic, VkFormat fallback_format);

VkFormat ResolveMaterialPositionFormat(const MaterialCreateConfig *cfg, VkFormat fallback_format);

const char *GetMaterialPresetName(const MaterialPreset mtl_id);
const char *GetMaterialPresetBMIId(const MaterialPreset mtl_id);

// Phase-A migration helpers: preset <-> variant mapping.
MaterialVariantKey MapPresetToVariantKey(const MaterialPreset mtl_id);
bool TryMapVariantKeyToPreset2D(const MaterialVariantKey &key, MaterialPreset &out_preset);
bool TryMapVariantKeyToPreset3D(const MaterialVariantKey &key, MaterialPreset &out_preset);
bool TryMapVariantKeyToPreset(const MaterialVariantKey &key, MaterialPreset &out_preset);

// BaseMaterialInfo registry:
// Material implementation .cpp files register their BMI defaults here.
void RegisterBaseMaterialInfo(const BaseMaterialInfo &bmi);
void RegisterBaseMaterialInfo(const MaterialPreset preset, const BaseMaterialInfo &bmi);
bool TryGetBaseMaterialInfoByBMIId(const std::string &bmi_id, BaseMaterialInfo &out_bmi);
bool TryGetBaseMaterialInfoByName(const std::string &name, BaseMaterialInfo &out_bmi);
bool TryGetBaseMaterialInfoByPreset(const MaterialPreset preset, BaseMaterialInfo &out_bmi);
bool TryResolveMaterialPresetByBMIId(const std::string &bmi_id, MaterialPreset &out_preset);

}//namespace hgl::graph::mtl

#pragma once

#include<hgl/vk/VK.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include<hgl/mtl/StdMaterial.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterializationSpec.h>
#include<hgl/mtl/MaterializationResolver.h>
#include<hgl/mtl/MaterializationPools.h>
#include<hgl/common/VertexAttribDef.h>

namespace hgl::graph::mtl{

struct MaterialCreateConfig;
class MaterialCreateInfo;

// MaterialPreset：内部创建派发键，与 M_* 创建函数一一对应。
// 作者层不直接使用此 enum；通过 bmi_id 字符串主键识别材质。
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

// BMI registry
void RegisterBaseMaterialInfo(const BaseMaterialInfo &bmi);
void RegisterBaseMaterialInfo(const MaterialPreset preset, const BaseMaterialInfo &bmi);
bool TryGetBaseMaterialInfoByBMIId(const std::string &bmi_id, BaseMaterialInfo &out_bmi);
bool TryGetBaseMaterialInfoByPreset(const MaterialPreset preset, BaseMaterialInfo &out_bmi);

// ── built-in fallback BMI ID 常量 ─────────────────────────────────────────────
constexpr const char *BUILTIN_BMI_FALLBACK_2D       = "builtin/fallback_2d";
constexpr const char *BUILTIN_BMI_FALLBACK_3D       = "builtin/fallback_3d";
constexpr const char *BUILTIN_BMI_MISSING_MATERIAL  = "builtin/missing_material";
constexpr const char *BUILTIN_BMI_ERROR_CHECKER     = "builtin/error_checker";
constexpr const char *BUILTIN_BMI_TEXT              = "builtin/text";
constexpr const char *BUILTIN_BMI_SKY               = "builtin/sky";

inline const char *GetFallbackBMIId(const bool is_2d = false)
{
    return is_2d ? BUILTIN_BMI_FALLBACK_2D : BUILTIN_BMI_FALLBACK_3D;
}

}//namespace hgl::graph::mtl

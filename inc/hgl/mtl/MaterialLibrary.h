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

/// 便利入口：同时设置 recipe.bmi_id（正式主键）与 recipe.preset_hint（当前阶段兼容字段）。
/// 所有作者层代码应统一用此函数代替直接赋 preset_hint。
inline void SetRecipePreset(MaterialRecipe &recipe, const MaterialPreset preset)
{
    const char *bmi_id = GetMaterialPresetBMIId(preset);
    if (bmi_id && *bmi_id)
        recipe.bmi_id = bmi_id;
    recipe.preset_hint = static_cast<uint32_t>(preset);
}

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

// ── built-in fallback BMI ID 常量 ─────────────────────────────────────────────
// 所有 fallback 材质统一走 BMI 标识接口，差别只在来源（built-in）而不是调用面。
// 命名规则：builtin/<category>
constexpr const char *BUILTIN_BMI_FALLBACK_2D       = "builtin/fallback_2d";      // 2D 无材质保底（PureColor2D）
constexpr const char *BUILTIN_BMI_FALLBACK_3D       = "builtin/fallback_3d";      // 3D 无材质保底（PureColor3D）
constexpr const char *BUILTIN_BMI_MISSING_MATERIAL  = "builtin/missing_material"; // 缺失材质（PureColor3D + 红色提示）
constexpr const char *BUILTIN_BMI_ERROR_CHECKER     = "builtin/error_checker";    // 错误棋盘格（未来独立实现）
constexpr const char *BUILTIN_BMI_TEXT              = "builtin/text";             // 文字专用（Text2D）
constexpr const char *BUILTIN_BMI_SKY               = "builtin/sky";              // 天空保底（SkyMinimal）

/// 返回指定场景下的 fallback bmi_id。
/// 当正常材质加载失败时，调用此函数获取保底材质标识，再走统一 BMI 查询接口。
inline const char *GetFallbackBMIId(const bool is_2d = false)
{
    return is_2d ? BUILTIN_BMI_FALLBACK_2D : BUILTIN_BMI_FALLBACK_3D;
}

}//namespace hgl::graph::mtl

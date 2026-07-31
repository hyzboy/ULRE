#pragma once

#include<hgl/vk/VK.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include<hgl/mtl/StdMaterial.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterializationSpec.h>
#include<hgl/mtl/MaterializationResolver.h>
#include<hgl/mtl/MaterializationPools.h>
#include<hgl/common/VertexAttribDef.h>
#include<hgl/common/RenderTargetOutputConfig.h>
#include<hgl/mtl/SkyLight.h>

namespace hgl::graph
{
    class GeometryVertexFormat;
    struct ShaderBufferSource;
}


namespace hgl::graph::mtl{

class ShaderProgramBuildSpec;

// BuiltinMaterialCreatorID：内部创建派发键，与 M_* 创建函数一一对应。
// 作者层不直接使用此 enum；通过 mtl_def_id 字符串主键识别材质。
enum class BuiltinMaterialCreatorID:uint8
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

struct MaterialDefinitionBuildRequest
{
    std::string mtl_def_id;
    MaterialRecipe recipe;
    PrimitiveType primitive_type = PrimitiveType::Triangles;
    const GeometryVertexFormat *geometry_vertex_format = nullptr;

    bool override_shader_stage_bits = false;
    uint32 shader_stage_flag_bit = 0;

    bool override_rt_output = false;
    RenderTargetOutputConfig rt_output{};

    bool override_sky_ambient_model = false;
    SkyLightAmbientModel sky_ambient_model = SkyLightAmbientModel::Simple;

    const ShaderBufferSource *const *private_shader_buffer_sources = nullptr;
    uint32 private_shader_buffer_source_count = 0;

    void SetPrivateShaderBufferSources(const ShaderBufferSource *const *list,const uint32 count)
    {
        private_shader_buffer_sources=list;
        private_shader_buffer_source_count=count;
    }
};

/// 仅声明 request 主路径材质创建函数，不产生任何注册或全局常量副作用。
#define DECLARE_MATERIAL_CREATOR(name) \
ShaderProgramBuildSpec *Create##name(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition);

DECLARE_MATERIAL_CREATOR(VertexColor2D)
DECLARE_MATERIAL_CREATOR(PureColor2D)
DECLARE_MATERIAL_CREATOR(PureTexture2D)
DECLARE_MATERIAL_CREATOR(RectTexture2D)
DECLARE_MATERIAL_CREATOR(RectTexture2DArray)
DECLARE_MATERIAL_CREATOR(Text2D)
DECLARE_MATERIAL_CREATOR(PureColor3D)
DECLARE_MATERIAL_CREATOR(VertexColor3D)
DECLARE_MATERIAL_CREATOR(VertexLuminance3D)
DECLARE_MATERIAL_CREATOR(VertexPattleColor3D)
DECLARE_MATERIAL_CREATOR(Gizmo3D)
DECLARE_MATERIAL_CREATOR(SkyMinimal)
DECLARE_MATERIAL_CREATOR(Standard)
DECLARE_MATERIAL_CREATOR(StandardTextureArray)
DECLARE_MATERIAL_CREATOR(PBRColor3D)

ShaderProgramBuildSpec *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                             const BuiltinMaterialCreatorID mtl_id,
                                             const MaterialDefinition &definition,
                                             const MaterialDefinitionBuildRequest &request);

std::string BuildBuiltinMaterialCreatorRequestHash(const BuiltinMaterialCreatorID mtl_id,
                                                 const MaterialDefinition &definition,
                                                 const MaterialDefinitionBuildRequest &request);

VkFormat ResolveMaterialVertexSemanticFormat(const GeometryVertexFormat *gvf, VertexSemantic semantic, VkFormat fallback_format);
inline VkFormat ResolveMaterialPositionFormat(const GeometryVertexFormat *gvf, VkFormat fallback_format)
{
    return ResolveMaterialVertexSemanticFormat(gvf, VertexSemantic::Position, fallback_format);
}

const char *GetBuiltinMaterialCreatorIDName(const BuiltinMaterialCreatorID mtl_id);

// BMI registry
void RegisterMaterialDefinition(const MaterialDefinition &bmi);
void RegisterMaterialDefinition(const BuiltinMaterialCreatorID preset, const MaterialDefinition &bmi);
bool TryGetMaterialDefinitionByID(const std::string &mtl_def_id, MaterialDefinition &out_bmi);
bool TryGetMaterialDefinitionByBuiltinMaterialCreatorID(const BuiltinMaterialCreatorID preset, MaterialDefinition &out_bmi);

// ── built-in fallback BMI ID 常量 ─────────────────────────────────────────────
constexpr const char *BUILTIN_MTL_DEF_FALLBACK_2D       = "builtin/fallback_2d";
constexpr const char *BUILTIN_MTL_DEF_FALLBACK_3D       = "builtin/fallback_3d";
constexpr const char *BUILTIN_MTL_DEF_MISSING_MATERIAL  = "builtin/missing_material";
constexpr const char *BUILTIN_MTL_DEF_ERROR_CHECKER     = "builtin/error_checker";
constexpr const char *BUILTIN_MTL_DEF_TEXT              = "builtin/text";
constexpr const char *BUILTIN_MTL_DEF_SKY               = "builtin/sky";

inline const char *GetFallbackMaterialDefinitionID(const bool is_2d = false)
{
    return is_2d ? BUILTIN_MTL_DEF_FALLBACK_2D : BUILTIN_MTL_DEF_FALLBACK_3D;
}

}//namespace hgl::graph::mtl

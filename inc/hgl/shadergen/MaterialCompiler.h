#pragma once

/// MaterialCompiler.h — FixedMaterialDef → ShaderProgramBuildSpec 编译器接口
///
/// 使用 CompileCompositorMaterial 编译 Compositor 模板产出的完整 GLSL。
/// 内部流程：
///   1. 按 def.descriptor_entries[] 构建 MaterialDescriptorInfo（描述符布局）
///   2. 使用 SetFinalGLSL + CreateShaderDirect 直接编译
///   3. 填充并返回 ShaderProgramBuildSpec*

#include<hgl/mtl/FixedMaterialDef.h>
#include<hgl/mtl/SkyLight.h>
#include<hgl/common/ShaderStageDef.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include<hgl/mtl/MaterialRecipe.h>
#include <string>
#include <vector>

namespace hgl::graph
{
    struct ShaderBufferSource;
    class GeometryVertexFormat;
}

namespace hgl::graph::mtl{

struct CompositorMaterialBuildConfig
{
    PrimitiveType primitive_type = PrimitiveType::Triangles;
    uint32_t shader_stage_flag_bits = uint32_t(ShaderStage::VertexFragment);
    SkyLightAmbientModel sky_ambient_model = SkyLightAmbientModel::Simple;
    const ::hgl::graph::ShaderBufferSource *const *private_shader_buffer_sources = nullptr;
    uint32_t private_shader_buffer_source_count = 0;
    const ::hgl::graph::GeometryVertexFormat *geometry_vertex_format = nullptr;
    // Per-material SSBO slot declarations (index == ssbo_slot).
    // When non-null and non-empty, MaterialCompiler generates MaterialInstance
    // FixedDescriptorEntry items and injects the material SSBO struct/buffer
    // declarations into the fragment GLSL.
    const std::vector<MaterialSSBOSlotDecl> *ssbo_slot_decls = nullptr;
    // Optional: capability declaration source for development-time subset validation.
    // When non-null, CompileCompositorMaterial checks Layout requirements ⊆ Definition capabilities.
    const MaterialDefinition *material_definition = nullptr;
};

class ShaderProgramBuildSpec;

/**
 * 编译 Compositor 模板产出的完整 GLSL → ShaderProgramBuildSpec*。
 *
 * 使用 SetFinalGLSL() + CreateShaderDirect() 直接编译。
 *
 * @param profile   设备能力 profile
 * @param def       材质定义（descriptor/vertex/MI 元数据）
 * @param vs_glsl   完整的 vertex shader GLSL（含 #version, layout, main）
 * @param fs_glsl   完整的 fragment shader GLSL（含 #version, layout, main）
 * @param config    编译期配置视图
 * @return          编译好的 ShaderProgramBuildSpec*; 失败返回 nullptr
 */
ShaderProgramBuildSpec *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const FixedMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const CompositorMaterialBuildConfig &config);

}//namespace hgl::graph::mtl

// ── MaterialLibrary.h must be included for MaterialDefinitionBuildRequest and MaterialDefinition ──
#include<hgl/mtl/MaterialLibrary.h>

namespace hgl::graph::mtl{

/// Build a CompositorMaterialBuildConfig for a 3D/sky material from a request + definition.
inline CompositorMaterialBuildConfig ToCompositorBuildConfig3D(
    const MaterialDefinitionBuildRequest &request,
    const MaterialDefinition &definition)
{
    CompositorMaterialBuildConfig bc;
    bc.primitive_type       = request.primitive_type;
    bc.sky_ambient_model    = request.override_sky_ambient_model
        ? request.sky_ambient_model
        : SkyLightAmbientModel::Simple;
    if(request.override_shader_stage_bits)
        bc.shader_stage_flag_bits = request.shader_stage_flag_bit;
    bc.geometry_vertex_format            = request.geometry_vertex_format;
    bc.private_shader_buffer_sources     = request.private_shader_buffer_sources;
    bc.private_shader_buffer_source_count = request.private_shader_buffer_source_count;
    bc.ssbo_slot_decls                   = definition.ssbo_slot_decls.empty() ? nullptr : &definition.ssbo_slot_decls;
    bc.material_definition               = &definition;
    return bc;
}

}//namespace hgl::graph::mtl

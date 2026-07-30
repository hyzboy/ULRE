#pragma once

/// MaterialCompiler.h — FixedMaterialDef → ShaderProgramBuildSpec 编译器接口
///
/// 使用 CompileCompositorMaterial 编译 Compositor 模板产出的完整 GLSL。
/// 内部流程：
///   1. 按 def.descriptor_entries[] 构建 MaterialDescriptorInfo（顺序固定，无动态排序）
///   2. 使用 SetFinalGLSL + CreateShaderDirect 直接编译
///   3. 填充并返回 ShaderProgramBuildSpec*

#include<hgl/mtl/FixedMaterialDef.h>
#include<hgl/mtl/SkyLight.h>
#include<hgl/common/ShaderStageDef.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include <string>

namespace hgl::graph
{
    struct ShaderBufferSource;
}

namespace hgl::graph::mtl{

struct CompositorMaterialBuildConfig
{
    PrimitiveType primitive_type = PrimitiveType::Triangles;
    uint32_t shader_stage_flag_bits = uint32_t(ShaderStage::VertexFragment);
    bool material_instance = false;
    bool with_local_to_world = false;
    bool with_camera = false;
    bool with_sky = false;
    SkyLightAmbientModel sky_ambient_model = SkyLightAmbientModel::Simple;
    const ::hgl::graph::ShaderBufferSource *const *private_shader_buffer_sources = nullptr;
    uint32_t private_shader_buffer_source_count = 0;
};


struct Material3DCreateConfig;
struct Material2DCreateConfig;
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

ShaderProgramBuildSpec *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const FixedMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material3DCreateConfig *config = nullptr);

/**
 * CompileCompositorMaterial — 2D 材质重载
 *
 * 将 Material2DCreateConfig 映射到内部 Material3DCreateConfig 后调用主版本。
 * camera/sky 默认关闭，其余字段从 2D 配置继承。
 */
ShaderProgramBuildSpec *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const FixedMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material2DCreateConfig *config);

}//namespace hgl::graph::mtl

#pragma once

/// MaterialCompiler.h — FixedMaterialDef → MaterialCreateInfo 编译器接口
///
/// 使用 CompileCompositorMaterial 编译 Compositor 模板产出的完整 GLSL。
/// 内部流程：
///   1. 按 def.descriptor_entries[] 构建 MaterialDescriptorInfo（顺序固定，无动态排序）
///   2. 使用 SetFinalGLSL + CreateShaderDirect 直接编译
///   3. 填充并返回 MaterialCreateInfo*

#include<hgl/mtl/FixedMaterialDef.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include <string>

namespace hgl::graph::mtl{

struct Material3DCreateConfig;
struct Material2DCreateConfig;
class MaterialCreateInfo;

/**
 * 编译 Compositor 模板产出的完整 GLSL → MaterialCreateInfo*。
 *
 * 使用 SetFinalGLSL() + CreateShaderDirect() 直接编译。
 *
 * @param profile   设备能力 profile
 * @param def       材质定义（descriptor/vertex/MI 元数据）
 * @param vs_glsl   完整的 vertex shader GLSL（含 #version, layout, main）
 * @param fs_glsl   完整的 fragment shader GLSL（含 #version, layout, main）
 * @param config    运行时配置（可选）
 * @return          编译好的 MaterialCreateInfo*; 失败返回 nullptr
 */
MaterialCreateInfo *CompileCompositorMaterial(
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
MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const FixedMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material2DCreateConfig *config);

}//namespace hgl::graph::mtl

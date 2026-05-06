#pragma once

/// CompositorCompiler.h — StaticMaterialDef → MaterialCreateInfo 编译器接口
///
/// 使用 CompileCompositorMaterialOwned 编译 Compositor 模板产出的完整 GLSL。
/// 内部流程：
///   1. 按 def 的 ubo/ssbo/texture_samplers 三组定义构建 MaterialDescriptorDB
///   2. 使用 SetFinalGLSL + CreateShaderDirect 直接编译
///   3. 填充并返回 MaterialCreateInfo 的 unique_ptr

#include<hgl/mtl/StaticMaterialDef.h>
#include<hgl/shadergen/device/DeviceProfile.h>
#include <memory>
#include <string>

namespace hgl::graph::mtl{

struct Material3DCreateConfig;
struct Material2DCreateConfig;
class MaterialCreateInfo;

std::unique_ptr<MaterialCreateInfo> CompileCompositorMaterialOwned(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material3DCreateConfig *config = nullptr);

std::unique_ptr<MaterialCreateInfo> CompileCompositorMaterialOwned(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material2DCreateConfig *config);

/**
 * Resort descriptors, build layout contract, emit layout/sampler defines,
 * and inject them into the VS/FS GLSL stored in @p mci.
 *
 * Call this after all descriptors, vertex inputs, and MI have been set up
 * but before CreateShaderDirect().
 */
bool InjectLayoutDefines(MaterialCreateInfo &mci);

bool PrepareCompositorGLSLForReflection(
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    std::string &out_vs_glsl,
    std::string &out_fs_glsl,
    std::string *diagnostics = nullptr);

// diagnostics may also contain non-fatal inference mismatch warnings on success.

}//namespace hgl::graph::mtl

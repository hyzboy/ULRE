/// CompositorCompiler.cpp — StaticMaterialDef → MaterialCreateInfo 编译器实现
///
/// 流程：
///   1. 从 StaticMaterialDef 的 UBO/SSBO/TextureSampler 组构建 MaterialDescriptorDB
///   2. 从 FixedVertexEntry[] 设置顶点输入
///   3. 使用 SetFinalGLSL + CreateShaderDirect 直接编译

#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/internal/CompositorLayoutDefines.h>
#include <hgl/shadergen/internal/CompositorMaterialPreparation.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/Material2DCreateConfig.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <cstdio>
#include <memory>
#include <string>

namespace hgl::graph::mtl {

std::unique_ptr<MaterialCreateInfo> CompileCompositorMaterialOwned(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material3DCreateConfig *config)
{
    std::string diagnostics;
    std::unique_ptr<MaterialCreateInfo> mci = internal::PrepareCompositorMaterialSnapshotOwned(profile,
                                                                                                 def,
                                                                                                 vs_glsl,
                                                                                                 fs_glsl,
                                                                                                 config,
                                                                                                 &diagnostics);
    if (!mci)
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s failed: %s\n",
            def.name ? def.name : "<unnamed>",
            diagnostics.empty() ? "<unknown>" : diagnostics.c_str());
        return nullptr;
    }

    if (!mci->CompileSPV())
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s failed: CompileSPV() failed (check GLSLCompiler log) (%s)\n",
            def.name ? def.name : "<unnamed>",
            internal::BuildShaderDataSchemaDebugText(def).c_str());
        return nullptr;
    }

    if (!diagnostics.empty())
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s diagnostics: %s\n",
            def.name ? def.name : "<unnamed>",
            diagnostics.c_str());
    }

    return mci;
}

bool PrepareCompositorGLSLForReflection(
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    std::string &out_vs_glsl,
    std::string &out_fs_glsl,
    std::string *diagnostics)
{
    std::unique_ptr<MaterialCreateInfo> mci = internal::PrepareCompositorMaterialSnapshotOwned(nullptr,
                                                                                                 def,
                                                                                                 vs_glsl,
                                                                                                 fs_glsl,
                                                                                                 nullptr,
                                                                                                 diagnostics);
    if (!mci)
        return false;

    ShaderCreateInfoVertex *vert = mci->GetVertexShader();
    ShaderCreateInfo *frag = mci->GetStageShader(ShaderStage::Fragment);

    out_vs_glsl = vert ? vert->GetFinalGLSL() : std::string();
    out_fs_glsl = frag ? frag->GetFinalGLSL() : std::string();

    return true;
}

std::unique_ptr<MaterialCreateInfo> CompileCompositorMaterialOwned(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material2DCreateConfig *config)
{
    Material3DCreateConfig cfg3d(
        config ? config->prim : def.primitive_type,
        IncludeCamera::Without,
        config && config->local_to_world ? IncludeL2W::With : IncludeL2W::Without,
        IncludeSky::Without);

    if (config)
    {
        cfg3d.rt_output                         = config->rt_output;
        cfg3d.material_instance                 = config->material_instance;
        cfg3d.shader_stage_flag_bit             = config->shader_stage_flag_bit;
    }

    return CompileCompositorMaterialOwned(profile, def, vs_glsl, fs_glsl, &cfg3d);
}

bool InjectLayoutDefines(MaterialCreateInfo &mci)
{
    return internal::ApplyCompositorLayoutDefines(mci);
}

}  // namespace hgl::graph::mtl

// 运行时 → 生成器接缝的**生成器侧**实现。
//
// 这里既放背身实现，也放从 MaterialDefinitionRegistry.cpp 迁出的
// `CreateMaterialFromDefinition`（它要调 BuildGenericMaterial ⇒ 属生成期）。
//
// 运行时（SceneGraph）只见 ShaderProgramBuildBackend 抽象；本 TU 通过
// InstallShaderGenProgramBuildBackend() 把它装进契约库的槽位。

#include <hgl/mtl/ShaderProgramBuildBackend.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/MaterialShaderCompiler.h>

#include "builder/GenericMaterialBuilder.h"
#include "../compile/GLSLCompiler.h"

namespace hgl::graph::mtl
{
// ── 生成期入口（原 MaterialDefinitionRegistry.cpp 内的同名函数） ──────────────

mtl::ShaderBuildContext *CreateMaterialFromDefinition(
    const mtl::contract::PhysicalDeviceProfileLite *profile,
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request)
{
    // BuildGenericMaterial 不修改 definition（const&）——直接透传，
    // 薄包装只承担 API 语义（名字即文档）
    return BuildGenericMaterial(profile, request, definition, nullptr);
}

mtl::ShaderBuildContext *CreateMaterialFromDefinition(
    const mtl::contract::PhysicalDeviceProfileLite *profile,
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request,
    mtl::MaterialShaderDocumentCapture *document_capture)
{
    return BuildGenericMaterial(profile, request, definition, document_capture);
}

namespace
{
    /** 把上面两个生成期入口原样转发成运行时认的后端。行为零改动是其全部价值。 */
    class ShaderGenProgramBuildBackend final : public ShaderProgramBuildBackend
    {
    public:

        ShaderBuildContext *CreateBuildContext(
            const contract::PhysicalDeviceProfileLite *profile,
            const MaterialDefinition &definition,
            const MaterialDefinitionBuildRequest &request) override
        {
            return CreateMaterialFromDefinition(profile, definition, request);
        }

        bool FinalizeBuildContext(ShaderBuildContext *ctx) override
        {
            return FinalizeShaderBuildContext(ctx);
        }
    };//class ShaderGenProgramBuildBackend

    ShaderGenProgramBuildBackend g_shadergen_program_build_backend;
}//namespace

    void InstallShaderGenProgramBuildBackend()
    {
        SetShaderProgramBuildBackend(&g_shadergen_program_build_backend);
        InstallShaderCreateInfoSPVReleaser();
    }

    void UninstallShaderGenProgramBuildBackend()
    {
        UninstallShaderCreateInfoSPVReleaser();
        SetShaderProgramBuildBackend(nullptr);
    }
}//namespace hgl::graph::mtl

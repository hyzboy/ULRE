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
#include "ShaderGenBackendInstall.h"
#include <hgl/ShaderCompilerAPI.h>

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

    namespace
    {
        /**
         * 把上面两个安装器注册成编译器插件的生命周期钩子。
         *
         * 编译器桥（ULRE.ShaderCompiler）不认识任何后端；钩子是唯一的连接方式。
         * 注册对象是 TU 级静态量 ⇒ 在 InitShaderCompiler() 之前必然已构造完毕
         * （包括 AppFramework 在 main 之前的初始化），发行形态整个 TU 不存在，
         * 钩子保持为空，编译器照常工作而不装任何后端。
         */
        struct ShaderGenBackendLifecycleRegistration
        {
            ShaderGenBackendLifecycleRegistration()
            {
                hgl::graph::SetShaderCompilerLifecycleHooks(
                    &InstallShaderGenProgramBuildBackend,
                    &UninstallShaderGenProgramBuildBackend);
            }
        };//struct ShaderGenBackendLifecycleRegistration

        ShaderGenBackendLifecycleRegistration g_shadergen_backend_lifecycle_registration;
    }//namespace
}//namespace hgl::graph::mtl

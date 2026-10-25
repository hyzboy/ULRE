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
        /// 生命周期钩子两端的**选择器**：安装时读一次环境变量并记住结论，
        /// 卸载必须装/卸对称（`SetShaderProgramBuildBackend(nullptr)` 对错的
        /// 后端调用会把槽位清空但漏掉释放器，或反之）。
        bool g_use_domain_build_backend = false;

        void InstallSelectedShaderProgramBuildBackend()
        {
            g_use_domain_build_backend = IsDomainShaderBuildBackendRequested();

            if (g_use_domain_build_backend)
                InstallDomainShaderBuildBackend();
            else
                InstallShaderGenProgramBuildBackend();
        }

        void UninstallSelectedShaderProgramBuildBackend()
        {
            if (g_use_domain_build_backend)
                UninstallDomainShaderBuildBackend();
            else
                UninstallShaderGenProgramBuildBackend();

            g_use_domain_build_backend = false;
        }

        /**
         * 把上面的安装器注册成编译器插件的生命周期钩子。
         *
         * 编译器桥（ULRE.ShaderCompiler）不认识任何后端；钩子是唯一的连接方式。
         * 注册对象是 TU 级静态量，构造发生在 main 之前，因此 InitShaderCompiler() 被调用时
         * 钩子必然已经就位——**前提是这个 TU 真的被链接进可执行文件**。它没有任何对外符号，
         * 静态库链接器会把整个 TU 丢掉，所以聚合入口 ULRE::Engine 对 ULRE.ShaderGen 使用
         * $<LINK_LIBRARY:WHOLE_ARCHIVE,...>（见根 CMakeLists.txt 的 ULRE_SHADERGEN）。
         * 发行形态整个 TU 不存在，钩子保持为空，编译器照常工作而不装任何后端。
         */
        struct ShaderGenBackendLifecycleRegistration
        {
            ShaderGenBackendLifecycleRegistration()
            {
                hgl::graph::SetShaderCompilerLifecycleHooks(
                    &InstallSelectedShaderProgramBuildBackend,
                    &UninstallSelectedShaderProgramBuildBackend);
            }
        };//struct ShaderGenBackendLifecycleRegistration

        ShaderGenBackendLifecycleRegistration g_shadergen_backend_lifecycle_registration;
    }//namespace
}//namespace hgl::graph::mtl

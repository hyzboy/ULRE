// ShaderGenBackendInstall.h — 生成器侧的「接缝安装」入口声明
//
// 两个安装器负责把生成器提供的东西挂到运行时：
//   · 构建后端  → inc/hgl/mtl/ShaderProgramBuildBackend.h 的进程级槽位
//   · SPV 释放器 → ShaderCreateInfo::SetSPVReleaser（SPV 由插件分配，必须由插件释放）
//
// 它们原先声明在 compile/GLSLCompiler.h 里。编译器桥搬到 ULRE.ShaderCompiler 之后，
// 该头只保留「纯编译/反射」的公共 API（inc/hgl/ShaderCompilerAPI.h），
// 生成器专属的安装器声明就归到这里——编译器模块不再知道后端的名字。

#pragma once

#include <hgl/mtl/MaterialDomain.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    void InstallShaderGenProgramBuildBackend();
    void UninstallShaderGenProgramBuildBackend();

    /// 「不生成着色器」的后端（阶段 3.1）。它同样需要构建上下文与 SPV 释放器，
    /// 与前者的唯一区别是**片段阶段用领域固定主着色器 + options**，不做拼装。
    /// 两者互斥（`SetShaderProgramBuildBackend` 先装先得）。
    ///
    /// 阶段 5.1 起默认即此后端；生成器后端只在显式设置
    /// `ULRE_SHADER_BUILD_BACKEND=shadergen` 时才被选中，供阶段 5.2 删除前的对照。
    bool IsShaderGenBuildBackendRequested();
    void InstallDomainShaderBuildBackend();
    void UninstallDomainShaderBuildBackend();

    /// 最近一次领域后端失败的原因（空串表示没有失败记录）。
    /// `GLogError` 在门禁/工具里不可见，诊断需要这个出口。
    const char *GetLastDomainShaderBuildBackendError();

    /// 工具/门禁用：按与真实构建路径**完全相同的规则**算出某定义的
    /// `generated/options.glsl` 文本。
    ///
    /// 为什么必须走这里而不是让脚本手抄宏表：等价性验收要分别拿「旧生成器产物」
    /// 与「固定主着色器」编译同一个形状，两者必须吃**同一份** options。手抄的表
    /// 会和 `EmitDomainOptions` 的输入推导悄悄漂移（profile / ambient include /
    /// shadow pcf / face normal fallback / 渲染状态各自都有独立分支），漂移之后
    /// 的等价性结论就没有意义了。
    bool EmitDomainOptionsForDefinition(
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
        MaterialDomain domain,
        ShaderProgramPurpose purpose,
        AnsiString &out_text);

    /// 「本材质该用哪份固定片段源」的持有者。
    ///
    /// `MaterialDefinitionBuildRequest::fixed_fragment_source` 是裸指针（沿用
    /// 构建请求「只借用、不持有」的约定），所以文本必须活在调用者栈上。用它而
    /// 不是让 `ResolveFixedFragmentSource` 直接往请求里写指针——那样指针会指向
    /// 一个已经析构的临时串。
    struct FixedFragmentSource
    {
        AnsiString source;
        uint64 interface_hash = 0;

        /// `generated/options.glsl` 所在目录的**上一级**（编译器的第二 include 根）。
        /// 固定源里带着 `#include "generated/options.glsl"`，所以任何要亲手编译它
        /// 的地方都离不开这个根；把它和源放在一起，免得调用方各自拼路径。
        OSString options_root;

        /// 仅供诊断文本使用（失败信息里要指出是哪个领域/用途编不过）。
        MaterialDomain domain = MaterialDomain::Custom;
        ShaderProgramPurpose purpose = ShaderProgramPurpose::ForwardColor;
    };

    /// 工具/门禁用：按与真实构建路径**完全相同**的规则取出固定片段源。
    ///
    /// 内部就是后端自己用的 `ResolveFixedFragmentStage`：options 落盘、接口形状
    /// 哈希、`// domain-options-key` 溯源注释三者都由它负责。取到之后把
    /// `source` / `interface_hash` 填进构建请求，片段阶段便不再进入任何拼装
    /// 路径，`BuildGenericMaterial` 只做网格阶段与链接规格。
    ///
    /// 失败原因见 `GetLastDomainShaderBuildBackendError()`。
    bool ResolveFixedFragmentSource(
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
        FixedFragmentSource &out_source);

    /// 工具/门禁用：用**生产同一条**编译路径把固定片段编成 SPV。
    ///
    /// 只在「门自己要按 stage 逐个编译以便定位失败」时调用：include 根是插件里
    /// 的进程级配置，手写 `CompileFinalGLSLToSPVWithDiagnostic` 会因为没有第二
    /// 根而在 `#include "generated/options.glsl"` 上失败，于是门只能去复制后端
    /// 的根管理逻辑——那份复制迟早会与后端漂移。这里直接复用后端的实现。
    bool CompileFixedFragmentSource(
        ShaderBuildContext &ctx,
        const FixedFragmentSource &fixed,
        AnsiString &out_diagnostic);

    void InstallShaderCreateInfoSPVReleaser();
    void UninstallShaderCreateInfoSPVReleaser();
}//namespace hgl::graph::mtl

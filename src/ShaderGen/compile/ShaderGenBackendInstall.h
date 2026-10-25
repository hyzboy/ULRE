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

namespace hgl::graph::mtl
{
    void InstallShaderGenProgramBuildBackend();
    void UninstallShaderGenProgramBuildBackend();

    void InstallShaderCreateInfoSPVReleaser();
    void UninstallShaderCreateInfoSPVReleaser();
}//namespace hgl::graph::mtl

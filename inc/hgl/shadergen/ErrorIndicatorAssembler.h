#pragma once

#include <hgl/shadergen/registry/ErrorCodeRegistry.h>
#include <string>
#include <cstdint>

namespace hgl::graph::mtl
{
    struct MaterialVariantKey;
    struct MaterialVariantDesc;
    struct MaterialVariantRow;
}

namespace hgl::graph
{
    /// AssembleErrorIndicatorFS — 在 VS 路由成功但 FS 路由失败时，
    /// 生成带 error_code 颜色编码的 ErrorIndicator 片段着色器 GLSL。
    ///
    /// 实现策略：
    ///   1. 复制 desc，将 surface_function_path 覆盖为 "surface/error_indicator_surface.glsl"
    ///   2. 调用 CompositorAssembler::AssembleFragmentShader 正常走 compositor 流程
    ///   3. 将生成结果中的 specialization constant 声明替换为内联常量，
    ///      使 error_code 在 SPIR-V 编译期固化（无需运行时 spec constant 基础设施）
    ///
    /// @param key         原始 MaterialVariantKey（保留 VS 需要的 geometry/blend/pass 信息）
    /// @param vs_desc     已成功装配 VS 所用的 MaterialVariantDesc（含 factory_type / bound_row）
    /// @param error_code  由 ErrorCodeRegistry::EncodeFSError() 生成的 24 位错误码
    /// @param out_fs_glsl 输出：可直接传入 CompileCompositorMaterial 的 FS GLSL 源码
    /// @param out_error   失败时输出人读错误信息
    /// @return true 表示成功，false 表示 ErrorIndicator FS 本身也装配失败（内部降级为纯红色 FS）
    bool AssembleErrorIndicatorFS(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &vs_desc,
        uint32_t                        error_code,
        std::string                    &out_fs_glsl,
        std::string                    &out_error);

} // namespace hgl::graph

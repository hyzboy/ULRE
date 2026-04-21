#pragma once

#include <hgl/mtl/SurfaceType.h>
#include <hgl/mtl/RenderAlphaMode.h>
#include <hgl/mtl/PassType.h>
#include <hgl/mtl/SkyLight.h>
#include <hgl/mtl/LightingModel.h>
#include <string>
#include <vector>

// Forward declarations for VariantDesc-based overload
namespace hgl::graph::mtl
{
    struct MaterialVariantKey;
    struct MaterialVariantDesc;
}

namespace hgl::graph
{
    /**
     * CompositorAssembler — 组合 Compositor Template + Surface Function 生成完整 GLSL
     *
     * 当前实现（生成路径优先）：
     *   1. 输入：MaterialVariantKey + MaterialVariantDesc
     *   2. 基于模板路由与 Key 派生特性构建 VS/FS 源码
     *   3. 通过 ShaderWriter.EmitInclude 直接拼接 compositor/surface/common include
     *   4. 返回完整 GLSL 字符串
     */
    class CompositorAssembler
    {
    public:

        struct AssembleResult
        {
            std::string vertex_glsl;
            std::string fragment_glsl;
            bool        success;
            std::string error_message;
        };

        /// Uses global ShaderLibrary path from ShaderGenPathConfig.
        CompositorAssembler();

        /// shader_library_path: ShaderLibrary 根目录的绝对路径（不带尾部斜杠）
        explicit CompositorAssembler(const std::string &shader_library_path);

        /// 根据 RenderAlphaMode 返回该模式需要生成 SPV 的所有 PassType 列表
        /// Opaque→[ForwardOpaque,ShadowOpaque,EarlyZSolid]
        /// Masked→[ForwardMasked,ShadowMasked,EarlyZMasked]
        /// Transparent→[ForwardTransparent]
        /// Dither→[ForwardDither,ShadowOpaque]
        /// AlphaToCoverage→[ForwardA2C,ShadowMasked]
        static std::vector<PassType> GetPassTypesForBlendMode(RenderAlphaMode blend);

        /// VariantDesc overload — derives SurfaceType/RenderAlphaMode/PassType/QualityTier from key,
        /// uses desc's shader template paths (empty path → auto-routing fallback).
        AssembleResult Assemble(
            const mtl::MaterialVariantKey  &key,
            const mtl::MaterialVariantDesc &desc
        ) const;

    private:

        bool        TryBuildGeneratedVSTemplatePath(const std::string &template_path, const mtl::MaterialVariantKey &key, std::string &out_source) const;
        bool        TryBuildGeneratedFSTemplatePath(const std::string &template_path, const mtl::MaterialVariantKey &key, RenderAlphaMode blend, const std::string &surface_path, std::string &out_source) const;
        std::string GetCompositorVSPath(SurfaceType surface, PassType pass) const;
        std::string GetCompositorFSPath(SurfaceType surface, RenderAlphaMode blend, PassType pass) const;
        std::string GetSurfaceFunctionPath(SurfaceType surface) const;
        std::string InjectDefines(const std::string &source, const mtl::MaterialVariantKey &key) const;
        bool        ReadFile(const std::string &path, std::string &out_content, std::string &out_error) const;

        std::string shader_lib_path_;
    };
}

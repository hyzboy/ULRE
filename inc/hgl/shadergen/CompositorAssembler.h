#pragma once

#include <hgl/mtl/SurfaceType.h>
#include <hgl/mtl/RenderAlphaMode.h>
#include <hgl/mtl/PassType.h>
#include <hgl/mtl/SkyLight.h>
#include <hgl/mtl/LightingModel.h>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>

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
            bool        success = false;
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
        static std::span<const PassType> GetPassTypesForBlendMode(RenderAlphaMode blend);

        /// VariantDesc overload — derives SurfaceType/RenderAlphaMode/PassType/QualityTier from key,
        /// uses desc's shader template paths (empty path → auto-routing fallback).
        AssembleResult Assemble(
            const mtl::MaterialVariantKey  &key,
            const mtl::MaterialVariantDesc &desc
        ) const;

    private:

        bool AssembleVertexShaderSource(const mtl::MaterialVariantKey &key,
                                        const mtl::MaterialVariantDesc &desc,
                                        std::string &out_source,
                                        std::string &out_error) const;

        bool AssembleFragmentShaderSource(const mtl::MaterialVariantKey &key,
                                          const mtl::MaterialVariantDesc &desc,
                                          const std::string &surface_rel,
                                          std::string &out_source,
                                          std::string &out_error) const;

        std::string InjectDefines(const std::string &source, const mtl::MaterialVariantKey &key) const;

        /// Read a file from shader_lib_path_, with per-instance caching. Thread-safe.
        bool ReadFileCached(const std::string &rel_path, std::string &out_source, std::string &out_error) const;

        std::string shader_lib_path_;

        mutable std::mutex                             file_cache_mutex_;
        mutable std::unordered_map<std::string, std::string> file_cache_;
    };
}

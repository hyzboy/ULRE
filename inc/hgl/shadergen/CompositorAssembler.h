#pragma once

#include <hgl/mtl/new/SurfaceType.h>
#include <hgl/mtl/new/BlendMode.h>
#include <hgl/mtl/new/PassType.h>
#include <hgl/mtl/new/GeometryFetchMode.h>
#include <hgl/mtl/new/NewShaderPermutationKey.h>
#include <string>

namespace hgl::graph
{
    /**
     * CompositorAssembler — 组合 Compositor Template + Surface Function 生成完整 GLSL
     *
     * 第一版最小实现：
     *   1. 输入：SurfaceType, BlendMode, PassType
     *   2. 查表选择 FS Compositor Template 文件路径
     *   3. 读取模板文件内容
     *   4. 注入 #define 宏（NewShaderPermutationKey::AppendGLSLDefines()）
     *   5. 替换 #include SURFACE_FUNCTION_FILE 为实际路径
     *   6. 返回完整 FS GLSL 字符串
     */
    class CompositorAssembler
    {
    public:

        struct AssembleResult
        {
            std::string fragment_glsl;
            bool        success;
            std::string error_message;
        };

        /// shader_library_path: ShaderLibrary 根目录的绝对路径（不带尾部斜杠）
        explicit CompositorAssembler(const std::string &shader_library_path);

        /// fs_template_override: 非空时覆盖默认 FS 模板路径（相对于 ShaderLibrary 根目录）
        /// surface_function_override: 非空时覆盖默认 Surface Function 路径
        AssembleResult Assemble(
            SurfaceType     surface,
            BlendMode       blend,
            PassType        pass,
            const char     *fs_template_override      = nullptr,
            const char     *surface_function_override  = nullptr
        ) const;

    private:

        std::string GetCompositorFSPath(SurfaceType surface, BlendMode blend, PassType pass) const;
        std::string GetSurfaceFunctionPath(SurfaceType surface) const;
        std::string InjectDefines(const std::string &source, const NewShaderPermutationKey &key) const;
        std::string ReplaceSurfaceInclude(const std::string &source, const std::string &surface_path) const;
        bool        ReadFile(const std::string &path, std::string &out_content, std::string &out_error) const;

        std::string shader_lib_path_;
    };
}

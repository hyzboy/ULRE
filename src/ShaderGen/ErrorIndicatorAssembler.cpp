#include <hgl/shadergen/ErrorIndicatorAssembler.h>
#include <hgl/log/Log.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/mtl/MaterialVariantDesc.h>
#include <cstdio>
#include <string>

namespace hgl::graph
{
    namespace
    {
        /// ErrorIndicator surface GLSL 路径（相对于 ShaderLibrary 根目录）
        static constexpr const char *kErrorIndicatorSurfacePath =
            "surface/error_indicator_surface.glsl";

        /// specialization constant 声明模板（在 GLSL 中写死于 surface 文件顶部）
        /// 我们把它替换成内联的 const，避免依赖运行时 spec constant 覆盖基础设施。
        static constexpr const char *kSpecConstPattern =
            "layout(constant_id = 0) const uint ULRE_ERROR_CODE = 0u;";

        /// 将 GLSL 源码中的 spec constant 声明替换为内联字面量
        static void PatchErrorCodeInGLSL(std::string &glsl, uint32_t error_code)
        {
            const auto pos = glsl.find(kSpecConstPattern);
            if (pos == std::string::npos)
                return; // 可能 compositor 模板没有包含 surface 文件内容，不做处理

            char replacement[80];
            std::snprintf(replacement, sizeof(replacement),
                "const uint ULRE_ERROR_CODE = %uu;", error_code);
            glsl.replace(pos, std::strlen(kSpecConstPattern), replacement);
        }

        /// 当 ErrorIndicator FS 装配本身也失败时，生成一个极简纯红色 fallback FS
        static std::string MakePureRedFallbackFS()
        {
            return
                "#version 450\n"
                "layout(location=0) out vec4 outColor;\n"
                "void main(){ outColor = vec4(1.0,0.0,0.0,1.0); }\n";
        }
    } // anonymous namespace

    bool AssembleErrorIndicatorFS(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &vs_desc,
        uint32_t                        error_code,
        std::string                    &out_fs_glsl,
        std::string                    &out_error)
    {
        // 构造 ErrorIndicator 专用的 desc：保留 VS 路由信息，仅覆盖 surface path
        mtl::MaterialVariantDesc ei_desc = vs_desc;
        ei_desc.surface_function_path    = kErrorIndicatorSurfacePath;
        // 清空 fs_template_path 让 assembler 走标准 compositor FS 模板流程
        ei_desc.fs_template_path.clear();

        CompositorAssembler assembler;
        auto fs_result = assembler.AssembleFragmentShader(key, ei_desc, vs_desc.bound_row);

        if (!fs_result.success)
        {
            GLogError(
                "[ErrorIndicatorAssembler] AssembleFragmentShader failed for ErrorIndicator "
                "(error_code=0x%08X): %s — falling back to pure-red FS\n",
                error_code,
                fs_result.error_message.c_str());
            out_error   = fs_result.error_message;
            out_fs_glsl = MakePureRedFallbackFS();
            // 返回 false 表示 ErrorIndicator 装配本身有问题，但 out_fs_glsl 仍有值
            return false;
        }

        // 将 spec constant 声明替换为内联常量，将 error_code 固化进 GLSL
        PatchErrorCodeInGLSL(fs_result.glsl, error_code);

        out_fs_glsl = std::move(fs_result.glsl);
        out_error.clear();
        return true;
    }

} // namespace hgl::graph

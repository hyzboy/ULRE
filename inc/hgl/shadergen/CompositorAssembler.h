#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/SurfaceType.h>
#include <hgl/mtl/BlendMode.h>
#include <hgl/mtl/PassType.h>
#include <hgl/shadergen/ShaderPermutationKey.h>
#include <hgl/shadergen/MaterialStageInterface.h>
#include <hgl/shadergen/MaterialOutputContract.h>
#include <hgl/shadergen/MaterialCoverageContract.h>
#include <string>

namespace hgl::graph::shadergen
{
    using namespace hgl::graph::mtl;
    /**
     * CompositorAssembler — 统一组合 raw fragment source/template 与
     * Surface Function，生成完整 GLSL
     *
     * 第一版最小实现：
     *   1. 输入：SurfaceType, BlendMode, PassType
     *   2. 查表选择 FS Compositor Template 文件路径
     *   3. 读取模板文件内容
     *   4. 注入 #define 宏（ShaderPermutationKey::AppendGLSLDefines()）
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

        struct CompositorModuleOptions
        {
            const char *sky_module = nullptr;              // e.g. "sky/sky_atmosphere.glsl"
            const char *direct_lighting_module = nullptr;  // e.g. "lighting/direct_cook_torrance_pbr.glsl"
            const char *indirect_lighting_module = nullptr;// e.g. "lighting/indirect_simple_ambient.glsl"
            const char *lighting_algorithm_module = nullptr;// e.g. "lighting/forward_pbr.glsl"
            const char *material_source_module = nullptr;  // e.g. "material/pbr_surface_source.glsl"
            const char *ntb_module = nullptr;              // e.g. "ntb/ntb_tangent_vbo_normalmap.glsl"
            const char *forward_lighting_module = nullptr;// e.g. "compositor/forward_lighting.glsl"
            bool enable_material_source_provider = false;
            bool enable_ntb_provider = false;
            bool enable_scene_lighting = false;
            bool alpha_test = false;
            float alpha_cutoff = 0.5f;
            bool dither = false;
            bool use_resolved_render_state = false;
            const hgl::ValueArray<
                InterStageSemanticContractEntry>
                *fragment_inputs = nullptr;
            const OutputContract *output_contract = nullptr;
            const MaterialCoverageContract
                *coverage_contract = nullptr;
        };

        /// shader_library_path: ShaderLibrary 根目录的绝对路径（不带尾部斜杠）
        explicit CompositorAssembler(const std::string &shader_library_path);

        /// fragment_source_override: compositor template path, relative to the
        /// ShaderLibrary root.
        /// surface_function_override: optional Surface Function path override.
        AssembleResult Assemble(
            SurfaceType                  surface,
            BlendMode                    blend,
            PassType                     pass,
            const char                  *fragment_source_override,
            const char                  *surface_function_override,
            const CompositorModuleOptions &module_options
        ) const;

        AssembleResult Assemble(
            SurfaceType                  surface,
            BlendMode                    blend,
            PassType                     pass,
            const char                  *fs_template_override      = nullptr,
            const char                  *surface_function_override = nullptr
        ) const
        {
            return Assemble(surface, blend, pass, fs_template_override, surface_function_override, CompositorModuleOptions{});
        }

    private:

        std::string GetCompositorFSPath(SurfaceType surface, PassType pass) const;
        std::string GetSurfaceFunctionPath(SurfaceType surface) const;
        std::string InjectDefines(const std::string &source, const ShaderPermutationKey &key, const CompositorModuleOptions &module_options) const;
        std::string ReplaceLightingModuleIncludes(const std::string &source, const CompositorModuleOptions &module_options) const;
        std::string ReplaceSurfaceInclude(const std::string &source, const std::string &surface_path) const;
        bool ApplyFragmentInputContract(
            const std::string &source,
            const hgl::ValueArray<InterStageSemanticContractEntry> &inputs,
            std::string &out_source) const;
        bool ApplySurfaceInputContract(
            const std::string &source,
            const hgl::ValueArray<InterStageSemanticContractEntry> &inputs,
            std::string &out_source) const;
        bool        ReadFile(const std::string &path, std::string &out_content, std::string &out_error) const;

        std::string shader_lib_path_;
    };
}

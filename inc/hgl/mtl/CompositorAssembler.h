#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/SurfaceType.h>
#include <hgl/mtl/PassType.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/mtl/MaterialOutputContract.h>
#include <hgl/mtl/MaterialCoverageContract.h>
#include <string>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    /**
     * CompositorAssembler — Fragment 单趟发射器
     *
     * 2026-09 组装模型统一：原"读模板 + 8 遍文本手术"（InjectDefines /
     * ReplaceLightingModuleIncludes / ReplaceSurfaceInclude /
     * ApplyDepthCoverageContract / ApplyFragmentInputContract /
     * ApplySurfaceInputContract / ApplyMaterialOutputContract /
     * 编译期 BeforeSurfaceFunction 注入）收敛为单趟组装：
     *   #version → defines → includes（数据驱动的最终路径）→ 契约声明
     *   （fragment 输入 / 输出附件）→ main 骨架（本文件常量）内嵌契约驱动的
     *   si 装配 → 代码模块（调用方传入，置于 surface 函数 include 之前）。
     *
     * 骨架常量承载于实现文件；算法代码仍在 ShaderLibrary/*.glsl，
     * 经 glslang 原生 #include 组合。
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
            const char *indirect_lighting_module = nullptr;// e.g. "lighting/indirect_sky_ambient.glsl"
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
            const hgl::ValueArray<
                InterStageSemanticContractEntry>
                *fragment_inputs = nullptr;
            const OutputContract *output_contract = nullptr;
            const MaterialCoverageContract
                *coverage_contract = nullptr;
        };

        /// fragment_source_override: 骨架选择键（原 compositor 模板路径），
        /// 空则按 surface/pass 分派。
        /// surface_function_override: surface 函数路径覆盖。
        /// code_module_glsl: ShaderCodeModule 代码模块拼接文本（调用方依据
        /// manifest 生成；置于 surface 函数 include 之前，保证先于其使用声明）。
        AssembleResult Assemble(
            SurfaceType                  surface,
            PassType                     pass,
            const char                  *fragment_source_override,
            const char                  *surface_function_override,
            const CompositorModuleOptions &module_options,
            const std::string           &code_module_glsl = {}) const;

        bool AssembleDocument(
            SurfaceType                  surface,
            PassType                     pass,
            const char                  *fragment_source_override,
            const char                  *surface_function_override,
            const CompositorModuleOptions &module_options,
            const std::string           &code_module_glsl,
            ShaderDocument              &out_document,
            ShaderDocumentDiagnostics    &out_diagnostics) const;

        AssembleResult Assemble(
            SurfaceType                  surface,
            PassType                     pass,
            const char                  *fs_template_override      = nullptr,
            const char                  *surface_function_override = nullptr
        ) const
        {
            return Assemble(surface, pass, fs_template_override, surface_function_override, CompositorModuleOptions{});
        }

    private:

        std::string GetSurfaceFunctionPath(SurfaceType surface) const;
    };
}

#pragma once

#include <hgl/mtl/MaterialCoverageContract.h>
#include <hgl/mtl/MaterialOutputContract.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/mtl/RenderTemplate.h>
#include <hgl/mtl/ResolvedRenderTemplate.h>
#include <hgl/mtl/ShaderDocument.h>

#include <string>

namespace hgl::graph::mtl
{
    // Template-first fragment emission entry point.
    // Normal callers provide a resolved template. Validation and module-graph
    // resolution happen once before native document emission.
    class FragmentTemplateComposer
    {
    public:
        struct ComposeInput
        {
            const ResolvedRenderTemplate *resolved_template = nullptr;
            bool alpha_test = false;
            float alpha_cutoff = 0.5f;
            bool dither = false;
            const hgl::ValueArray<InterStageSemanticContractEntry>
                *fragment_inputs = nullptr;
            const OutputContract *output_contract = nullptr;
            const MaterialCoverageContract *coverage_contract = nullptr;
            const ShaderDocument *code_module_document = nullptr;

            // 纹理声明（含 channels）：法线贴图声明 channels = 2 时，
            // 注入 MTL_TEX_<NAME>_CHANNELS=2，ntb 模块据此走 XY + 还原 Z 分支。
            const std::vector<MaterialTextureDeclaration>
                *texture_declarations = nullptr;

            // 阴影 PCF 采样方式（生成期宏，发射到模板的 {{defines}} 块）：
            //   0   → pcf_shadow 走旧版 3x3 网格盒式采样
            //   > 0 → Poisson 磁盘采样，值 = 采样数（上限 32）
            // 仅当模板真的带 shadow_provider 槽时才会发射该宏。
            hgl::uint32 shadow_pcf_poisson_taps = 16;

            // Normal-offset shadow mapping 总开关（生成期宏）：
            //   0 → 不编译法线偏移代码，只靠深度 bias
            //   1 → 编译；运行时强度仍由 ShadowCascadeInfo::shadow_params.w 决定
            //       （0 = 不偏移），即"有没有这段代码"与"用多大劲"分开控制。
            // 仅当模板真的带 shadow_provider 槽时才会发射该宏。
            hgl::uint32 shadow_normal_offset = 1;
        };

        bool Compose(
            const ComposeInput &input,
            ShaderDocument &out_document,
            ShaderDocumentDiagnostics &out_diagnostics) const;
    };
}

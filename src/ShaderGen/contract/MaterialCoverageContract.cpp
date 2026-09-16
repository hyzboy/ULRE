#include <hgl/mtl/MaterialCoverageContract.h>

#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;

    namespace
    {
        // 材质源 → 评估 coverage(alpha) 所需的 interstage 语义。
        //
        // key 是**模块注册名**（@ulre name；AddMaterialProviderRoot 从 TOML 的
        // include 路径提取文件名主干），不是库内路径——移动或重命名 shader
        // 文件不应牵动 C++。
        //
        // 注意：这组需求与模块自身的 `require` 注解是两回事。后者是模块运行
        // 所需，这里是「求 alpha 需要什么」。例如 debug_normal_source 运行需要
        // WorldNormal/MaterialData，但它的 alpha 是常量，一条都不需要——所以它
        // 必须显式登记，以区别于「未登记」时的兜底推导。
        struct MaterialSourceCoverageRule
        {
            const char *module_name;
            bool data_index_id;         // 需要材质实例索引（取 payload/纹理引用行）
            bool uv0;                   // 需要采样 UV
            bool color;                 // 需要顶点色
            bool requires_material_data;
            bool requires_texture;
        };

        const MaterialSourceCoverageRule kMaterialSourceCoverageRules[] =
        {
            // 模块注册名              DataIndexID  UV0    Color  材质数据行  纹理
            { "pbr_surface_source",     true,       true,  false, false,      true  },
            { "texture_source",         true,       true,  false, false,      true  },
            { "vertex_color_source",    false,      false, true,  false,      false },
            { "unlit_source",           true,       false, false, true,       false },
            { "luminance_source",       true,       false, false, true,       false },
            // alpha 为常量，不需要任何 interstage 语义
            { "debug_normal_source",    false,      false, false, false,      false },
        };

        /// 命中已登记规则则填充 out_contract 并返回 true；
        /// 返回 false 表示未登记，交由调用方走 varying 兜底推导。
        bool ApplyMaterialSourceCoverageRule(
            const RenderTemplateModuleRoot *material_source_root,
            MaterialCoverageContract &out_contract) noexcept
        {
            if (!material_source_root
             || material_source_root->module_name.IsEmpty())
                return false;

            const char *source = material_source_root->module_name.c_str();

            for (const MaterialSourceCoverageRule &rule :
                    kMaterialSourceCoverageRules)
            {
                if (std::strcmp(source, rule.module_name) != 0)
                    continue;

                if (rule.data_index_id)
                    out_contract.required_semantics |=
                        GetInterStageSemanticMask(
                            InterStageSemantic::DataIndexID);
                if (rule.uv0)
                    out_contract.required_semantics |=
                        GetInterStageSemanticMask(InterStageSemantic::UV0);
                if (rule.color)
                    out_contract.required_semantics |=
                        GetInterStageSemanticMask(InterStageSemantic::Color);

                out_contract.requires_material_data =
                    rule.requires_material_data;
                out_contract.requires_texture = rule.requires_texture;
                return true;
            }

            return false;
        }
    }

    bool BuildMaterialCoverageContract(
        const MaterialDefinition &definition,
        const MaterialRecipe &recipe,
        const RenderTemplateRequest &render_template_request,
        const ShaderProgramPurpose purpose,
        MaterialCoverageContract &out_contract) noexcept
    {
        out_contract = {};
        const ResolvedMaterialRenderState state =
            ResolveMaterialRenderState(definition, recipe);
        const bool alpha_test = state.alpha_test;
        const bool dither = state.dither;
        const bool alpha_to_coverage =
            state.pipeline_config.alpha_to_coverage;

        if (alpha_to_coverage
         && purpose == ShaderProgramPurpose::ForwardColor)
        {
            out_contract.mode = MaterialCoverageMode::AlphaToCoverage;
        }
        else if (alpha_to_coverage)
        {
            out_contract.mode = MaterialCoverageMode::Dither;
        }
        else if (alpha_test && dither)
        {
            out_contract.mode =
                MaterialCoverageMode::AlphaTestDither;
        }
        else if (alpha_test)
        {
            out_contract.mode = MaterialCoverageMode::AlphaTest;
        }
        else if (dither)
        {
            out_contract.mode = MaterialCoverageMode::Dither;
        }

        out_contract.alpha_cutoff = state.alpha_cutoff;
        out_contract.requires_alpha_evaluation =
            out_contract.mode != MaterialCoverageMode::None;

        if (out_contract.requires_alpha_evaluation)
        {
            const RenderTemplateModuleRoot *material_source_root =
                render_template_request.FindModuleRoot(
                    ShaderModuleSlotRole::MaterialSourceProvider);

            if (!ApplyMaterialSourceCoverageRule(
                    material_source_root, out_contract))
            {
                // 未登记的材质源：退回按材质自身 varying 声明推导。
                out_contract.required_semantics =
                    GetMaterialInterStageSemanticMask(
                        definition.vertex_varying);
                out_contract.requires_material_data =
                    definition.vertex_varying.emit_data_index_id;
                out_contract.requires_texture =
                    definition.vertex_varying.emit_uv0;
            }
        }
        return true;
    }
}

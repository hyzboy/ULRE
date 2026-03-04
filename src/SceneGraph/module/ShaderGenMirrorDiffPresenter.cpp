#include <hgl/graph/module/ShaderGenMirrorDiffPresenter.h>
#include <hgl/graph/module/ShaderGenValidationReportUtils.h>
#include <hgl/graph/module/ShaderGenValidationStorageService.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/contract/ShaderGenMirrorDiff.h>
#include <cstdio>

namespace hgl::graph
{
    static int BoolToInt(const bool v)
    {
        return v ? 1 : 0;
    }

    bool PresentShaderGenMirrorDiff(const mtl::MaterialCreateInfo &mci,
                                    const mtl::contract::ShaderGenResult &result,
                                    const char *material_name,
                                    ShaderGenDiffLogDetail detail,
                                    ShaderGenValidationReport *out_report)
    {
        const char *mat_name = (material_name && material_name[0]) ? material_name : "<unnamed-material>";

        mtl::contract::ShaderGenMirrorDiffSummary diff_summary;
        mtl::contract::BuildShaderGenMirrorDiffSummary(mci, result, diff_summary);

        RecordShaderGenProfilerSample(diff_summary.all_match,
                                      diff_summary.layout_match,
                                      diff_summary.vertex_match,
                                      diff_summary.spv_match,
                                      diff_summary.legacy_stage_combo,
                                      diff_summary.mirror_stage_combo,
                                      diff_summary.legacy_layout_count,
                                      diff_summary.mirror_layout_count,
                                      diff_summary.legacy_vertex_count,
                                      diff_summary.mirror_vertex_count,
                                      diff_summary.legacy_spv_count,
                                      diff_summary.mirror_spv_count);

        if (detail == ShaderGenDiffLogDetail::Full)
        {
            std::fprintf(stderr,
                "[RendererShaderGenAdapter][DiffKV] material=%s event=layout legacy_count=%u mirror_count=%u legacy_hash=0x%llx mirror_hash=0x%llx match=%d\n",
                mat_name,
                static_cast<unsigned>(diff_summary.legacy_layout_count),
                static_cast<unsigned>(diff_summary.mirror_layout_count),
                static_cast<unsigned long long>(diff_summary.layout_hash_legacy),
                static_cast<unsigned long long>(diff_summary.layout_hash_mirror),
                BoolToInt(diff_summary.layout_match));

            std::fprintf(stderr,
                "[RendererShaderGenAdapter][DiffKV] material=%s event=vertex legacy_count=%u mirror_count=%u legacy_hash=0x%llx mirror_hash=0x%llx match=%d\n",
                mat_name,
                static_cast<unsigned>(diff_summary.legacy_vertex_count),
                static_cast<unsigned>(diff_summary.mirror_vertex_count),
                static_cast<unsigned long long>(diff_summary.vertex_hash_legacy),
                static_cast<unsigned long long>(diff_summary.vertex_hash_mirror),
                BoolToInt(diff_summary.vertex_match));

            std::fprintf(stderr,
                "[RendererShaderGenAdapter][DiffKV] material=%s event=spv legacy_count=%u mirror_count=%u legacy_hash=0x%llx mirror_hash=0x%llx legacy_stages=%s mirror_stages=%s match=%d\n",
                mat_name,
                static_cast<unsigned>(diff_summary.legacy_spv_count),
                static_cast<unsigned>(diff_summary.mirror_spv_count),
                static_cast<unsigned long long>(diff_summary.spv_hash_legacy),
                static_cast<unsigned long long>(diff_summary.spv_hash_mirror),
                diff_summary.legacy_stage_summary.c_str(),
                diff_summary.mirror_stage_summary.c_str(),
                BoolToInt(diff_summary.spv_match));
        }

        if (!diff_summary.all_match && out_report)
        {
            char msg[512] = {};
            std::snprintf(msg,
                sizeof(msg),
                "material=%s legacy/mirror diff mismatch: layout=%d vertex=%d spv=%d legacy_stages=%s mirror_stages=%s",
                mat_name,
                BoolToInt(diff_summary.layout_match),
                BoolToInt(diff_summary.vertex_match),
                BoolToInt(diff_summary.spv_match),
                diff_summary.legacy_stage_summary.c_str(),
                diff_summary.mirror_stage_summary.c_str());
            AddShaderGenValidationError(*out_report, msg);
            out_report->diff_valid = false;
        }

        return diff_summary.all_match;
    }
}

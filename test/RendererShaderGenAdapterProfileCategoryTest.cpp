#include <hgl/graph/module/RendererShaderGenAdapter.h>
#include <hgl/graph/module/ShaderGenContractGateReporter.h>
#include <hgl/graph/module/ShaderGenDiffLogDetail.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/contract/ShaderGenContract.h>

#include <cstdio>

using namespace hgl::graph;

int main()
{
    int failed = 0;

    RendererShaderGenAdapter::ResetValidationReports();

    mtl::MaterialCreateConfig cfg(PrimitiveType::Triangles, false);
    mtl::MaterialCreateInfo mci(&cfg);

    mtl::contract::ShaderGenRequest request;
    request.contract_version = mtl::contract::kShaderGenContractVersion;
    request.has_physical_device_profile = true;
    request.physical_device_profile.name = "ProfileGateDevice";
    request.physical_device_profile.limits.max_vertex_input_attributes = 1;

    mtl::contract::ShaderGenResult result;
    result.contract_version = mtl::contract::kShaderGenContractVersion;
    result.vertex_layout.attributes.push_back({0, "POSITION", "vec3", 0});
    result.vertex_layout.attributes.push_back({1, "NORMAL", "vec3", 0});

    RendererShaderGenAdapter adapter;
    const auto report = adapter.ValidateMaterialContractReadOnly(
        mci,
        &request,
        &result,
        "ProfileMat",
        ShaderGenDiffLogDetail::SummaryOnly);

    if (report.request_result_valid)
    {
        std::fprintf(stderr, "[FAIL] request_result_valid should be false for profile limit violation\n");
        ++failed;
    }

    if (report.category != kShaderGenStrictGateProfileCategory)
    {
        std::fprintf(stderr,
                     "[FAIL] report category mismatch (got=%s, expected=%s)\n",
                     report.category.c_str(),
                     kShaderGenStrictGateProfileCategory);
        ++failed;
    }

    {
        const auto histogram = RendererShaderGenAdapter::GetRecentValidationReportCategoryHistogram(16);
        auto it = histogram.find(kShaderGenStrictGateProfileCategory);

        if (it == histogram.end() || it->second == 0)
        {
            std::fprintf(stderr, "[FAIL] StrictGate.Profile histogram bucket missing\n");
            ++failed;
        }
    }

    {
        const auto matrix = RendererShaderGenAdapter::GetRecentValidationMaterialCategoryMatrix(16);
        auto it_m = matrix.find("ProfileMat");

        if (it_m == matrix.end())
        {
            std::fprintf(stderr, "[FAIL] material not found in category matrix\n");
            ++failed;
        }
        else
        {
            auto it_c = it_m->second.find(kShaderGenStrictGateProfileCategory);
            if (it_c == it_m->second.end() || it_c->second == 0)
            {
                std::fprintf(stderr, "[FAIL] StrictGate.Profile category missing in material matrix\n");
                ++failed;
            }
        }
    }

    if (failed != 0)
    {
        std::fprintf(stderr, "RendererShaderGenAdapterProfileCategoryTest FAILED (%d)\n", failed);
        return 1;
    }

    std::fprintf(stdout, "RendererShaderGenAdapterProfileCategoryTest PASSED\n");
    return 0;
}

#include <hgl/graph/module/RendererShaderGenAdapter.h>
#include <hgl/graph/module/ShaderGenValidationTypes.h>
#include <cstdio>
#include <cstdint>
#include <string>

using namespace hgl::graph;

int main()
{
    int failed = 0;

    RendererShaderGenAdapter::ResetValidationReports();

    RendererShaderGenAdapter::RecordExternalValidationError(
        "StrictMatA",
        "mirror-preferred build aborted: vertex attribute count mismatch",
        "StrictGate.Vertex");

    RendererShaderGenAdapter::RecordExternalValidationError(
        "StrictMatA",
        "mirror-preferred build aborted: descriptor count mismatch",
        "StrictGate.Descriptor");

    RendererShaderGenAdapter::RecordExternalValidationError(
        "StrictMatB",
        "creation aborted: mirror-preferred requires valid mirror result",
        "StrictGate.Prebuild");

    {
        ShaderGenValidationReport last;
        std::string material_name;

        if (!RendererShaderGenAdapter::GetLastValidationReport(last, &material_name))
        {
            std::fprintf(stderr, "[FAIL] GetLastValidationReport returned false after strict records\n");
            ++failed;
        }
        else
        {
            if (material_name != "StrictMatB")
            {
                std::fprintf(stderr, "[FAIL] Last material mismatch (got=%s, expected=StrictMatB)\n", material_name.c_str());
                ++failed;
            }

            if (last.overall_valid)
            {
                std::fprintf(stderr, "[FAIL] Last report should be invalid for strict gate abort\n");
                ++failed;
            }

            if (last.error_count != 1 || last.category != "StrictGate.Prebuild")
            {
                std::fprintf(stderr,
                             "[FAIL] Last report fields mismatch (error_count=%u, category=%s)\n",
                             last.error_count,
                             last.category.c_str());
                ++failed;
            }
        }
    }

    {
        const auto recent = RendererShaderGenAdapter::GetRecentValidationReports(3);
        if (recent.size() != 3)
        {
            std::fprintf(stderr, "[FAIL] Recent report size mismatch (got=%zu, expected=3)\n", recent.size());
            ++failed;
        }
        else
        {
            if (recent[0].material_name != "StrictMatB" || recent[0].report.category != "StrictGate.Prebuild")
            {
                std::fprintf(stderr, "[FAIL] Recent[0] mismatch\n");
                ++failed;
            }

            if (recent[1].material_name != "StrictMatA" || recent[1].report.category != "StrictGate.Descriptor")
            {
                std::fprintf(stderr, "[FAIL] Recent[1] mismatch\n");
                ++failed;
            }

            if (recent[2].material_name != "StrictMatA" || recent[2].report.category != "StrictGate.Vertex")
            {
                std::fprintf(stderr, "[FAIL] Recent[2] mismatch\n");
                ++failed;
            }
        }
    }

    {
        const auto grouped = RendererShaderGenAdapter::GetRecentValidationReportsByMaterial(2, 16);

        auto it_a = grouped.find("StrictMatA");
        auto it_b = grouped.find("StrictMatB");

        if (it_a == grouped.end() || it_b == grouped.end())
        {
            std::fprintf(stderr, "[FAIL] Grouped reports missing material keys\n");
            ++failed;
        }
        else
        {
            if (it_a->second.size() != 2)
            {
                std::fprintf(stderr, "[FAIL] StrictMatA grouped size mismatch (got=%zu, expected=2)\n", it_a->second.size());
                ++failed;
            }

            if (it_b->second.size() != 1)
            {
                std::fprintf(stderr, "[FAIL] StrictMatB grouped size mismatch (got=%zu, expected=1)\n", it_b->second.size());
                ++failed;
            }
        }
    }

    {
        const auto histogram = RendererShaderGenAdapter::GetRecentValidationReportCategoryHistogram(16);

        auto count_of = [&histogram](const char *key) -> uint32_t
        {
            auto it = histogram.find(key);
            if (it == histogram.end())
                return 0;
            return it->second;
        };

        if (count_of("StrictGate.Vertex") != 1
         || count_of("StrictGate.Descriptor") != 1
         || count_of("StrictGate.Prebuild") != 1)
        {
            std::fprintf(stderr,
                         "[FAIL] StrictGate histogram mismatch (vertex=%u descriptor=%u prebuild=%u)\n",
                         count_of("StrictGate.Vertex"),
                         count_of("StrictGate.Descriptor"),
                         count_of("StrictGate.Prebuild"));
            ++failed;
        }
    }

    {
        const auto matrix = RendererShaderGenAdapter::GetRecentValidationMaterialCategoryMatrix(16);

        auto count_of = [&matrix](const char *material, const char *category) -> uint32_t
        {
            auto it_m = matrix.find(material);
            if (it_m == matrix.end())
                return 0;

            auto it_c = it_m->second.find(category);
            if (it_c == it_m->second.end())
                return 0;

            return it_c->second;
        };

        if (count_of("StrictMatA", "StrictGate.Vertex") != 1
         || count_of("StrictMatA", "StrictGate.Descriptor") != 1
         || count_of("StrictMatB", "StrictGate.Prebuild") != 1)
        {
            std::fprintf(stderr,
                         "[FAIL] Material-category matrix mismatch (A/Vertex=%u A/Descriptor=%u B/Prebuild=%u)\n",
                         count_of("StrictMatA", "StrictGate.Vertex"),
                         count_of("StrictMatA", "StrictGate.Descriptor"),
                         count_of("StrictMatB", "StrictGate.Prebuild"));
            ++failed;
        }
    }

    if (failed != 0)
    {
        std::fprintf(stderr, "RendererShaderGenAdapterStrictGateTest FAILED (%d)\n", failed);
        return 1;
    }

    std::fprintf(stdout, "RendererShaderGenAdapterStrictGateTest PASSED\n");
    return 0;
}

#include <hgl/graph/module/RendererShaderGenAdapter.h>
#include <hgl/graph/module/ShaderGenContractGateReporter.h>
#include <hgl/graph/module/ShaderGenDescriptorPolicyAdapter.h>
#include <hgl/graph/module/ShaderGenVertexPolicyAdapter.h>
#include <hgl/vk/VKShaderDescriptor.h>
#include <hgl/vk/VKRenderAssign.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace hgl::graph;

int main()
{
    int failed = 0;

    RendererShaderGenAdapter::ResetValidationReports();

    ReportMirrorPreferredStrictAbort(
        "EntryPrebuild",
        kShaderGenStrictGatePrebuildCategory,
        "creation aborted: mirror-preferred requires valid mirror result");

    ReportMirrorPreferredStrictAbort(
        "EntrySpv",
        kShaderGenStrictGateSpvCategory,
        "mirror SPV module build failed");

    {
        mtl::contract::ShaderGenResult mirror_result;
        mirror_result.vertex_layout.attributes.push_back({
            0,
            VAN::Position,
            "vec3",
            uint32_t(VK_VERTEX_INPUT_RATE_VERTEX)
        });

        VIAArray mirror_input;
        std::string reason;

        const ContractVertexInputDecision decision = BuildVertexInputByContractPolicy(
            nullptr,
            &mirror_result,
            true,
            false,
            mirror_input,
            reason);

        if (decision != ContractVertexInputDecision::StrictAbort)
        {
            std::fprintf(stderr, "[FAIL] Vertex strict decision mismatch\n");
            ++failed;
        }

        ReportMirrorPreferredStrictAbort(
            "EntryVertex",
            kShaderGenStrictGateVertexCategory,
            reason.c_str());
    }

    {
        std::vector<ShaderDescriptor> legacy_descriptors;
        legacy_descriptors.emplace_back();
        std::snprintf(legacy_descriptors[0].name, sizeof(legacy_descriptors[0].name), "%s", "uboPerMaterial");
        legacy_descriptors[0].desc_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        legacy_descriptors[0].set_type = DescriptorSetType::PerMaterial;
        legacy_descriptors[0].set = 1;
        legacy_descriptors[0].binding = 0;
        legacy_descriptors[0].stage_flag = uint32_t(ShaderStage::Vertex);

        mtl::contract::ShaderGenResult mirror_result;

        std::vector<ShaderDescriptor> out_descriptors;
        ContractDescriptorFallbackPhase phase = ContractDescriptorFallbackPhase::None;
        std::string reason;

        const ContractDescriptorDecision decision = BuildDescriptorsByContractPolicy(
            legacy_descriptors,
            &mirror_result,
            true,
            false,
            out_descriptors,
            phase,
            reason);

        if (decision != ContractDescriptorDecision::StrictAbort)
        {
            std::fprintf(stderr, "[FAIL] Descriptor strict decision mismatch\n");
            ++failed;
        }

        ReportMirrorPreferredStrictAbort(
            "EntryDescriptor",
            kShaderGenStrictGateDescriptorCategory,
            reason.c_str());
    }

    {
        const auto histogram = RendererShaderGenAdapter::GetRecentValidationReportCategoryHistogram(32);

        auto count_of = [&histogram](const char *key) -> uint32_t
        {
            auto it = histogram.find(key);
            return (it == histogram.end()) ? 0u : it->second;
        };

        if (count_of(kShaderGenStrictGatePrebuildCategory) != 1
         || count_of(kShaderGenStrictGateSpvCategory) != 1
         || count_of(kShaderGenStrictGateVertexCategory) != 1
         || count_of(kShaderGenStrictGateDescriptorCategory) != 1)
        {
            std::fprintf(stderr,
                         "[FAIL] Strict category histogram mismatch (prebuild=%u spv=%u vertex=%u descriptor=%u)\n",
                         count_of(kShaderGenStrictGatePrebuildCategory),
                         count_of(kShaderGenStrictGateSpvCategory),
                         count_of(kShaderGenStrictGateVertexCategory),
                         count_of(kShaderGenStrictGateDescriptorCategory));
            ++failed;
        }
    }

    {
        const auto matrix = RendererShaderGenAdapter::GetRecentValidationMaterialCategoryMatrix(32);

        auto count_of = [&matrix](const char *material, const char *category) -> uint32_t
        {
            auto it_m = matrix.find(material);
            if (it_m == matrix.end())
                return 0;

            auto it_c = it_m->second.find(category);
            return (it_c == it_m->second.end()) ? 0u : it_c->second;
        };

        if (count_of("EntryPrebuild", kShaderGenStrictGatePrebuildCategory) != 1
         || count_of("EntrySpv", kShaderGenStrictGateSpvCategory) != 1
         || count_of("EntryVertex", kShaderGenStrictGateVertexCategory) != 1
         || count_of("EntryDescriptor", kShaderGenStrictGateDescriptorCategory) != 1)
        {
            std::fprintf(stderr,
                         "[FAIL] Strict material-category matrix mismatch\n");
            ++failed;
        }
    }

    if (failed != 0)
    {
        std::fprintf(stderr, "MaterialBuildStrictGatePolicyMatrixTest FAILED (%d)\n", failed);
        return 1;
    }

    std::fprintf(stdout, "MaterialBuildStrictGatePolicyMatrixTest PASSED\n");
    return 0;
}

#pragma once

#include <hgl/graph/module/ShaderGenDescriptorLayoutAdapter.h>

namespace hgl::graph
{
    enum class ContractDescriptorDecision
    {
        UseLegacy,
        UseMirror,
        StrictAbort
    };

    enum class ContractDescriptorFallbackPhase
    {
        None,
        LayoutMismatch,
        BuildFailed
    };

    ContractDescriptorDecision BuildDescriptorsByContractPolicy(const std::vector<ShaderDescriptor> &legacy_descriptors,
                                                                const mtl::contract::ShaderGenResult *contract_result,
                                                                bool mirror_spv_build_used,
                                                                bool require_mirror_valid,
                                                                std::vector<ShaderDescriptor> &out_descriptors,
                                                                ContractDescriptorFallbackPhase &out_phase,
                                                                std::string &reason);
}

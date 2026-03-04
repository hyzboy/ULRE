#pragma once

#include <hgl/shadergen/contract/ShaderGenContract.h>
#include <hgl/vk/VKShaderDescriptor.h>
#include <string>
#include <vector>

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

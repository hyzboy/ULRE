#pragma once

#include <hgl/shadergen/contract/ShaderGenContract.h>
#include <hgl/vk/VKShaderDescriptor.h>
#include <string>
#include <vector>

namespace hgl::graph
{
    bool ValidateContractDescriptorLayoutAgainstLegacy(const std::vector<ShaderDescriptor> &legacy_descriptors,
                                                       const mtl::contract::ShaderGenResult &contract_result,
                                                       std::string &reason);

    bool BuildShaderDescriptorsFromContractLayout(const mtl::contract::ShaderGenResult &contract_result,
                                                  const std::vector<ShaderDescriptor> &legacy_descriptors,
                                                  std::vector<ShaderDescriptor> &descriptors,
                                                  std::string &reason);
}

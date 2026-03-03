#pragma once

#include <hgl/shadergen/contract/ShaderGenContract.h>
#include <hgl/vk/VKVertexInputAttribute.h>
#include <string>
#include <string_view>

namespace hgl::graph
{
    class ShaderCreateInfoVertex;

    VertexInputGroup ResolveVertexInputGroupBySemantic(std::string_view semantic);

    bool ValidateContractVertexLayoutAgainstLegacy(ShaderCreateInfoVertex *legacy_vertex,
                                                   const mtl::contract::VertexInputLayout &contract_layout,
                                                   std::string &reason);

    bool BuildVertexInputFromContractLayout(const mtl::contract::VertexInputLayout &layout,
                                            VIAArray &out_input,
                                            std::string &reason);
}

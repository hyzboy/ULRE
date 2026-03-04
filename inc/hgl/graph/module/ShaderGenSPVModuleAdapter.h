#pragma once

#include <hgl/shadergen/contract/ShaderGenContract.h>
#include <hgl/vk/VK.h>
#include <functional>
#include <string>
#include <vector>

namespace hgl::graph
{
    class ShaderModule;
    class ShaderCreateInfo;
    class ShaderCreateInfoMap;

    using CreateShaderModuleFromContractSPVCallback = std::function<const ShaderModule *(VkShaderStageFlagBits stage,
                                                                                           const uint32_t *spv_data,
                                                                                           size_t spv_size)>;

    using CreateShaderModuleFromLegacySCIMapCallback = std::function<const ShaderModule *(ShaderCreateInfo *sci)>;

    bool BuildShaderModulesFromContractSPV(const mtl::contract::ShaderGenResult &contract_result,
                                           const CreateShaderModuleFromContractSPVCallback &create_shader_module,
                                           std::vector<const ShaderModule *> &out_modules,
                                           std::string &reason);

    bool BuildShaderModulesFromLegacySCIMap(const ShaderCreateInfoMap &legacy_map,
                                            const CreateShaderModuleFromLegacySCIMapCallback &create_shader_module,
                                            std::vector<const ShaderModule *> &out_modules,
                                            std::string &reason);
}

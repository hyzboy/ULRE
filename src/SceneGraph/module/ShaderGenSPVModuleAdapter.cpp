#include <hgl/graph/module/ShaderGenSPVModuleAdapter.h>
#include <hgl/shadergen/ShaderCreateInfoMap.h>

namespace hgl::graph
{
    bool BuildShaderModulesFromContractSPV(const mtl::contract::ShaderGenResult &contract_result,
                                           const CreateShaderModuleFromContractSPVCallback &create_shader_module,
                                           std::vector<const ShaderModule *> &out_modules,
                                           std::string &reason)
    {
        out_modules.clear();

        if (!create_shader_module)
        {
            reason = "invalid shader module creation callback";
            return false;
        }

        if (contract_result.spv_per_stage.empty())
        {
            reason = "mirror result has no spv_per_stage";
            return false;
        }

        out_modules.reserve(contract_result.spv_per_stage.size());

        for (const auto &blob : contract_result.spv_per_stage)
        {
            if (blob.words.empty())
            {
                reason = "empty spv blob";
                return false;
            }

            const ShaderModule *module = create_shader_module(static_cast<VkShaderStageFlagBits>(blob.stage_mask),
                                                              blob.words.data(),
                                                              blob.words.size() * sizeof(uint32_t));
            if (!module)
            {
                reason = "failed create shader module from mirror spv";
                return false;
            }

            out_modules.push_back(module);
        }

        if (out_modules.size() < 2)
        {
            reason = "insufficient shader stages from mirror result";
            return false;
        }

        return true;
    }

    bool BuildShaderModulesFromLegacySCIMap(const ShaderCreateInfoMap &legacy_map,
                                            const CreateShaderModuleFromLegacySCIMapCallback &create_shader_module,
                                            std::vector<const ShaderModule *> &out_modules,
                                            std::string &reason)
    {
        out_modules.clear();

        if (!create_shader_module)
        {
            reason = "invalid shader module creation callback";
            return false;
        }

        if (legacy_map.GetCount() < 2)
        {
            reason = "insufficient shader stages from legacy map";
            return false;
        }

        out_modules.reserve(static_cast<size_t>(legacy_map.GetCount()));

        for (auto [stage, sci_ptr] : legacy_map)
        {
            (void)stage;

            if (!sci_ptr)
            {
                reason = "null shader create info in legacy map";
                return false;
            }

            const ShaderModule *module = create_shader_module(sci_ptr);
            if (!module)
            {
                reason = "failed create shader module from legacy shader map";
                return false;
            }

            out_modules.push_back(module);
        }

        return true;
    }
}

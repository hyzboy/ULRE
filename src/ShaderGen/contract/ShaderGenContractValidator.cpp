#include <hgl/shadergen/contract/ShaderGenContractValidator.h>
#include <unordered_set>

namespace hgl::graph::mtl::contract
{
    static void AddWarning(ShaderGenContractValidationResult &out, const std::string &msg)
    {
        out.warnings.emplace_back(msg);
        ++out.warning_count;
    }

    static void AddError(ShaderGenContractValidationResult &out, const std::string &msg)
    {
        out.errors.emplace_back(msg);
        ++out.error_count;
        out.valid = false;
    }

    ShaderGenContractValidationResult ValidateShaderGenRequestResult(const ShaderGenRequest &request,
                                                                     const ShaderGenResult &result,
                                                                     const char *material_name)
    {
        ShaderGenContractValidationResult out;

        const char *mat_name = (material_name && material_name[0]) ? material_name : "<unnamed-material>";

        if (request.contract_version != kShaderGenContractVersion)
        {
            AddError(out,
                     std::string("material=") + mat_name +
                     " request contract_version mismatch (request=" + std::to_string(request.contract_version) +
                     ", expected=" + std::to_string(kShaderGenContractVersion) + ")");
        }

        if (result.contract_version != request.contract_version)
        {
            AddError(out,
                     std::string("material=") + mat_name +
                     " request/result contract_version mismatch (request=" + std::to_string(request.contract_version) +
                     ", result=" + std::to_string(result.contract_version) + ")");
        }

        for (const auto &req : request.required_resources)
        {
            if (!req.required)
                continue;

            bool found = false;
            for (const auto &binding : result.layout.bindings)
            {
                if (binding.name != req.name)
                    continue;

                if (req.resource_class != ResourceClass::Unknown && binding.resource_class != req.resource_class)
                    continue;

                found = true;
                break;
            }

            if (!found)
            {
                AddError(out,
                         std::string("material=") + mat_name +
                         " request/result mismatch: missing required resource name=" + req.name +
                         " class=" + std::to_string(static_cast<uint32_t>(req.resource_class)));
            }
        }

        for (const auto &vre : request.vertex_requirements)
        {
            bool found = false;
            for (const auto &attr : result.vertex_layout.attributes)
            {
                if (attr.location != vre.location)
                    continue;

                if (attr.semantic != vre.semantic)
                    continue;

                found = true;
                break;
            }

            if (!found)
            {
                AddError(out,
                         std::string("material=") + mat_name +
                         " request/result mismatch: missing vertex requirement semantic=" + vre.semantic +
                         " location=" + std::to_string(vre.location));
            }
        }

        if (request.vertex_requirements.empty() && result.vertex_layout.attributes.empty())
        {
            AddWarning(out,
                       std::string("material=") + mat_name +
                       " request/result note: no vertex requirements and no vertex attributes in mirror result");
        }

        return out;
    }

    ShaderGenContractValidationResult ValidateShaderGenResult(const ShaderGenResult &result,
                                                              const char *material_name)
    {
        ShaderGenContractValidationResult out;

        const char *mat_name = (material_name && material_name[0]) ? material_name : "<unnamed-material>";

        if (result.contract_version != kShaderGenContractVersion)
        {
            AddError(out,
                     std::string("material=") + mat_name +
                     " contract_version mismatch (result=" + std::to_string(result.contract_version) +
                     ", expected=" + std::to_string(kShaderGenContractVersion) + ")");
        }

        if (result.spv_per_stage.empty())
        {
            AddWarning(out,
                       std::string("material=") + mat_name +
                       " mirror has no stage SPV blobs");
        }

        for (const auto &blob : result.spv_per_stage)
        {
            if (blob.words.empty())
            {
                AddError(out,
                         std::string("material=") + mat_name +
                         " empty SPV blob for stage_mask=" + std::to_string(blob.stage_mask));
            }
        }

        std::unordered_set<uint64_t> seen;
        for (const auto &binding : result.layout.bindings)
        {
            const uint64_t key = (static_cast<uint64_t>(binding.set) << 32) | static_cast<uint64_t>(binding.binding);
            if (!seen.insert(key).second)
            {
                AddError(out,
                         std::string("material=") + mat_name +
                         " duplicate binding in mirror layout (set=" + std::to_string(binding.set) +
                         ", binding=" + std::to_string(binding.binding) + ")");
            }
        }

        for (const auto &warn : result.diagnostics.warnings)
        {
            AddWarning(out,
                       std::string("material=") + mat_name +
                       " shader diagnostics warning: " + warn);
        }

        for (const auto &err : result.diagnostics.errors)
        {
            AddError(out,
                     std::string("material=") + mat_name +
                     " shader diagnostics error: " + err);
        }

        return out;
    }
}

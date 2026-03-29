#include <hgl/mtl/MaterialProfileAsset.h>

#include <algorithm>

namespace hgl::graph::mtl
{
namespace
{

bool ContainsMode(const std::vector<TextureSourceMode> &modes, const TextureSourceMode mode)
{
    return std::find(modes.begin(), modes.end(), mode) != modes.end();
}

bool IsValidTextureModeForSlotPolicy(const TextureSourceMode mode)
{
    return mode == TextureSourceMode::Simple
        || mode == TextureSourceMode::Array
        || mode == TextureSourceMode::Atlas;
}

void PushDiag(std::vector<std::string> &diagnostics, const std::string &message)
{
    diagnostics.push_back("[MaterialProfileAsset] " + message);
}

}

bool ValidateMaterialProfileAsset(const MaterialProfileAsset &asset,
                                  std::vector<std::string> &diagnostics)
{
    diagnostics.clear();

    if (asset.profile_id.empty())
        PushDiag(diagnostics, "profile_id is empty");

    if (asset.slots.empty())
        PushDiag(diagnostics, "slots is empty");

    for (size_t i = 0; i < asset.slots.size(); ++i)
    {
        const MaterialSlotPolicy &slot_policy = asset.slots[i];

        if (slot_policy.allowed_modes.empty())
        {
            PushDiag(diagnostics,
                     "slot[" + std::to_string(i) + "] allowed_modes is empty");
            continue;
        }

        for (const TextureSourceMode mode : slot_policy.allowed_modes)
        {
            if (!IsValidTextureModeForSlotPolicy(mode))
            {
                PushDiag(diagnostics,
                         "slot[" + std::to_string(i) + "] allowed_modes contains unsupported mode");
            }
        }

        if (!ContainsMode(slot_policy.allowed_modes, slot_policy.default_mode))
        {
            PushDiag(diagnostics,
                     "slot[" + std::to_string(i) + "] default_mode is not in allowed_modes");
        }

        for (const TextureSourceMode fallback_mode : slot_policy.fallback_chain)
        {
            if (!ContainsMode(slot_policy.allowed_modes, fallback_mode))
            {
                PushDiag(diagnostics,
                         "slot[" + std::to_string(i) + "] fallback_chain contains mode not in allowed_modes");
            }
        }
    }

    for (size_t i = 0; i < asset.instance_params.size(); ++i)
    {
        const MaterialParamSpec &param = asset.instance_params[i];

        if (param.name.empty())
        {
            PushDiag(diagnostics,
                     "instance_params[" + std::to_string(i) + "] name is empty");
        }

        if (param.has_min && param.has_max && param.min_value > param.max_value)
        {
            PushDiag(diagnostics,
                     "instance_params[" + std::to_string(i) + "] min_value is greater than max_value");
        }
    }

    return diagnostics.empty();
}

} // namespace hgl::graph::mtl

#include <hgl/shadergen/ColorSourceValidator.h>
#include <hgl/mtl/SamplerSlot.h>
#include <bitset>

namespace hgl::graph
{

ColorSourceValidationResult ValidateColorSources(const std::vector<ColorSource> &sources)
{
    ColorSourceValidationResult result;

    std::bitset<size_t(mtl::SamplerSlot::RANGE_SIZE)> seen_slots{};

    for (size_t i = 0; i < sources.size(); ++i)
    {
        const auto &cs = sources[i];

        // 检查 kind
        if (cs.kind == ColorSourceKind::None)
        {
            result.ok = false;
            result.errors.push_back({i, "ColorSourceKind::None is not allowed in a finalized source list"});
            continue;
        }

        // 检查槽位范围
        const auto slot_idx = size_t(cs.slot);
        if (slot_idx >= size_t(mtl::SamplerSlot::RANGE_SIZE))
        {
            result.ok = false;
            result.errors.push_back({i, "SamplerSlot value out of range"});
            continue;
        }

        // 检查槽位重复
        if (seen_slots.test(slot_idx))
        {
            result.ok = false;
            result.errors.push_back({i,
                std::string("Duplicate SamplerSlot: ") + mtl::SamplerSlotNameList[slot_idx]});
        }
        seen_slots.set(slot_idx);

        // 内置 sampler 恰好需要 1 个 binding
        if (cs.kind == ColorSourceKind::BuiltinSampler2D ||
            cs.kind == ColorSourceKind::BuiltinSampler2DArray)
        {
            if (cs.bindings.size() != 1)
            {
                result.ok = false;
                result.errors.push_back({i,
                    "BuiltinSampler* must declare exactly 1 DescriptorRequirement"});
            }
        }

        // UserPCG 允许 0 个 binding（纯函数 PCG），但不是 None
        // 其他 kind 暂不加约束，等具体实现时补充
    }

    return result;
}

} // namespace hgl::graph

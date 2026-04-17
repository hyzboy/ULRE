#pragma once

#include <array>

#include <hgl/vk/VK.h>
#include <hgl/vk/VKVertexInputLayout.h>

namespace hgl::graph
{
class GeometryVertexFormat
{
public:
    struct Slot
    {
        bool enabled = false;
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint32_t stride = 0;
        int32_t binding = -1;
    };

private:
    static constexpr size_t kAttribCount = static_cast<size_t>(VertexAttrib::RANGE_SIZE);

    std::array<Slot, kAttribCount> slots{};
    uint32_t active_count = 0;

public:
    GeometryVertexFormat() = default;

    static GeometryVertexFormat FromVIL(const VIL *vil)
    {
        GeometryVertexFormat gvf;

        if (!vil)
            return gvf;

        const uint32_t count = vil->GetVertexAttribCount();
        for (uint32_t i = 0; i < count; ++i)
        {
            const auto *cfg = vil->GetConfig(i);
            if (!cfg)
                continue;

            gvf.Set(cfg->attrib, cfg->format, cfg->stride, cfg->binding);
        }

        return gvf;
    }

    void Clear()
    {
        slots.fill({});
        active_count = 0;
    }

    bool Set(const VertexAttrib attrib, const VkFormat format, const uint32_t stride = 0, const int32_t binding = -1)
    {
        const size_t index = static_cast<size_t>(attrib);
        if (index >= kAttribCount)
            return false;

        Slot &slot = slots[index];
        if (!slot.enabled)
            ++active_count;

        slot.enabled = true;
        slot.format = format;
        slot.stride = stride;
        slot.binding = binding;
        return true;
    }

    bool Has(const VertexAttrib attrib) const
    {
        const size_t index = static_cast<size_t>(attrib);
        return index < kAttribCount ? slots[index].enabled : false;
    }

    VkFormat GetFormat(const VertexAttrib attrib) const
    {
        const size_t index = static_cast<size_t>(attrib);
        if (index >= kAttribCount)
            return VK_FORMAT_UNDEFINED;

        return slots[index].enabled ? slots[index].format : VK_FORMAT_UNDEFINED;
    }

    uint32_t GetStride(const VertexAttrib attrib) const
    {
        const size_t index = static_cast<size_t>(attrib);
        if (index >= kAttribCount)
            return 0;

        return slots[index].enabled ? slots[index].stride : 0;
    }

    int32_t GetBinding(const VertexAttrib attrib) const
    {
        const size_t index = static_cast<size_t>(attrib);
        if (index >= kAttribCount)
            return -1;

        return slots[index].enabled ? slots[index].binding : -1;
    }

    uint32_t GetActiveCount() const
    {
        return active_count;
    }

    const Slot *GetSlot(const VertexAttrib attrib) const
    {
        const size_t index = static_cast<size_t>(attrib);
        if (index >= kAttribCount || !slots[index].enabled)
            return nullptr;

        return &slots[index];
    }
};
}//

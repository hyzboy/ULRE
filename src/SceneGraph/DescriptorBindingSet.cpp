#include <hgl/graph/DescriptorBindingSet.h>
#include <hgl/vk/VKMaterial.h>

namespace hgl::graph
{
    namespace
    {
        constexpr size_t ToIndex(const mtl::SSBOType type) noexcept
        {
            return size_t(type);
        }

        constexpr size_t ToIndex(const mtl::TextureSlot slot) noexcept
        {
            return size_t(slot);
        }
    }

    DescriptorBindingSet::DescriptorBindingSet(Material *mtl, const VIL *binding_vil)
    {
        material = mtl;
        vil = binding_vil ? binding_vil : (material ? material->GetDefaultVIL() : nullptr);
    }

    void DescriptorBindingSet::SetMaterial(Material *mtl)
    {
        material = mtl;
        if (!vil && material)
            vil = material->GetDefaultVIL();
    }

    const VIL *DescriptorBindingSet::GetVIL() const
    {
        if (vil)
            return vil;

        return material ? material->GetDefaultVIL() : nullptr;
    }

    bool DescriptorBindingSet::SetSSBOBinding(const mtl::SSBOType ssbo_type, const uint32_t ssbo_id, const uint32_t slot_index)
    {
        const size_t index = ToIndex(ssbo_type);
        if (index >= size_t(mtl::SSBOType::RANGE_SIZE))
            return false;

        ssbo_bindings[index].valid = true;
        ssbo_bindings[index].ssbo_type = ssbo_type;
        ssbo_bindings[index].ssbo_id = ssbo_id;
        ssbo_bindings[index].slot_index = slot_index;
        return true;
    }

    bool DescriptorBindingSet::HasSSBOBinding(const mtl::SSBOType ssbo_type) const
    {
        const size_t index = ToIndex(ssbo_type);
        if (index >= size_t(mtl::SSBOType::RANGE_SIZE))
            return false;

        return ssbo_bindings[index].valid;
    }

    bool DescriptorBindingSet::GetSSBOBinding(const mtl::SSBOType ssbo_type, SSBOBinding &out_binding) const
    {
        const size_t index = ToIndex(ssbo_type);
        if (index >= size_t(mtl::SSBOType::RANGE_SIZE))
            return false;

        if (!ssbo_bindings[index].valid)
            return false;

        out_binding = ssbo_bindings[index];
        return true;
    }

    uint32_t DescriptorBindingSet::GetSSBOID(const mtl::SSBOType ssbo_type) const
    {
        SSBOBinding binding;
        if (!GetSSBOBinding(ssbo_type, binding))
            return 0;

        return binding.ssbo_id;
    }

    uint32_t DescriptorBindingSet::GetSlotIndex(const mtl::SSBOType ssbo_type) const
    {
        SSBOBinding binding;
        if (!GetSSBOBinding(ssbo_type, binding))
            return 0;

        return binding.slot_index;
    }

    void DescriptorBindingSet::ClearSSBOBinding(const mtl::SSBOType ssbo_type)
    {
        const size_t index = ToIndex(ssbo_type);
        if (index >= size_t(mtl::SSBOType::RANGE_SIZE))
            return;

        ssbo_bindings[index] = {};
    }

    bool DescriptorBindingSet::SetTextureBinding(const mtl::TextureSlot slot, Texture *texture, Sampler *sampler)
    {
        const size_t index = ToIndex(slot);
        if (index >= size_t(mtl::TextureSlot::RANGE_SIZE))
            return false;

        texture_bindings[index].valid = (texture != nullptr || sampler != nullptr);
        texture_bindings[index].texture = texture;
        texture_bindings[index].sampler = sampler;
        return true;
    }

    bool DescriptorBindingSet::GetTextureBinding(const mtl::TextureSlot slot, TextureBinding &out_binding) const
    {
        const size_t index = ToIndex(slot);
        if (index >= size_t(mtl::TextureSlot::RANGE_SIZE))
            return false;

        if (!texture_bindings[index].valid)
            return false;

        out_binding = texture_bindings[index];
        return true;
    }

    void DescriptorBindingSet::ClearTextureBinding(const mtl::TextureSlot slot)
    {
        const size_t index = ToIndex(slot);
        if (index >= size_t(mtl::TextureSlot::RANGE_SIZE))
            return;

        texture_bindings[index] = {};
    }
}

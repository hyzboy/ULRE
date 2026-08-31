#include <hgl/graph/DescriptorBindingSet.h>
#include <hgl/vk/VKShaderProgram.h>
#include <hgl/log/Log.h>

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

    DescriptorBindingSet::DescriptorBindingSet(ShaderProgram *mtl)
    {
        material = mtl;
    }

    void DescriptorBindingSet::SetMaterial(ShaderProgram *mtl)
    {
        material = mtl;
    }

    bool DescriptorBindingSet::SetSSBOBinding(const mtl::SSBOType ssbo_type, const uint32_t ssbo_id, const uint32_t material_private_data_slot)
    {
        const size_t index = ToIndex(ssbo_type);
        if (index >= size_t(mtl::SSBOType::RANGE_SIZE))
            return false;

        ssbo_bindings[index].valid = true;
        ssbo_bindings[index].ssbo_type = ssbo_type;
        ssbo_bindings[index].ssbo_id = ssbo_id;
        ssbo_bindings[index].material_private_data_slot = material_private_data_slot;
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

    uint32_t DescriptorBindingSet::GetMaterialPrivateDataSlot(const mtl::SSBOType ssbo_type) const
    {
        SSBOBinding binding;
        if (!GetSSBOBinding(ssbo_type, binding))
            return 0;

        return binding.material_private_data_slot;
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

    bool DescriptorBindingSet::SatisfiesResourceLayout(const mtl::ShaderResourceSchema &resource_layout, const char *resource_layout_owner_name) const
    {
        const char *owner_name = (resource_layout_owner_name && *resource_layout_owner_name)
                               ? resource_layout_owner_name
                               : (material ? material->GetName().c_str() : "<unknown>");

        for (const auto &req : resource_layout.resources)
        {
            if (!req.required)
                continue;

            switch (req.semantic)
            {
            case mtl::DescriptorSemantic::MaterialPrivateData:
            case mtl::DescriptorSemantic::MaterialTextureLayerTable:
            case mtl::DescriptorSemantic::MaterialPrivateDataIndex:
            {
                SSBOBinding binding;
                if (!GetSSBOBinding(req.ssbo_type, binding))
                {
                    GLogError("[DBS] Missing required SSBO binding, semantic=%s ssbo_type=%u material=%s",
                              mtl::GetDescriptorSemanticName(req.semantic),
                              uint32_t(req.ssbo_type),
                              owner_name);
                    return false;
                }

                if (binding.ssbo_id != req.ssbo_id)
                {
                    GLogError("[DBS] SSBO id mismatch, semantic=%s expected=%u actual=%u material=%s",
                              mtl::GetDescriptorSemanticName(req.semantic),
                              req.ssbo_id,
                              binding.ssbo_id,
                              owner_name);
                    return false;
                }
                break;
            }
            case mtl::DescriptorSemantic::MaterialTexture:
            case mtl::DescriptorSemantic::MaterialSampler:
            {
                TextureBinding binding;
                if (!GetTextureBinding(req.texture_slot, binding))
                {
                    GLogError("[DBS] Missing required texture binding, semantic=%s texture_slot=%u material=%s",
                              mtl::GetDescriptorSemanticName(req.semantic),
                              uint32_t(req.texture_slot),
                              owner_name);
                    return false;
                }

                if (req.semantic == mtl::DescriptorSemantic::MaterialTexture && !binding.texture)
                {
                    GLogError("[DBS] Required texture is null, texture_slot=%u material=%s",
                              uint32_t(req.texture_slot),
                              owner_name);
                    return false;
                }
                if (req.semantic == mtl::DescriptorSemantic::MaterialSampler && !binding.sampler)
                {
                    GLogError("[DBS] Required sampler is null, texture_slot=%u material=%s",
                              uint32_t(req.texture_slot),
                              owner_name);
                    return false;
                }
                break;
            }
            default:
                break;
            }
        }

        return true;
    }

    bool DescriptorBindingSet::HasRequiredResourceBindings() const
    {
        if (!material)
            return false;

        return SatisfiesResourceLayout(material->GetShaderResourceSchema(), material->GetName().c_str());
    }

    bool DescriptorBindingSet::HasRequiredResourceBindings(const mtl::ShaderResourceSchema &resource_layout, const char *resource_layout_owner_name) const
    {
        return SatisfiesResourceLayout(resource_layout, resource_layout_owner_name);
    }
}


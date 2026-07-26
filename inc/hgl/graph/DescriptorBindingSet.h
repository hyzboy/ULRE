#pragma once

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/vk/VK.h>
#include <cstddef>
#include <cstdint>

namespace hgl
{
    namespace graph
    {
        namespace mtl
        {
            struct MaterialResourceLayout;
        }

        class MaterialProgram;
        class Texture;
        class Sampler;

        class DescriptorBindingSet
        {
        public:
            struct SSBOBinding
            {
                bool valid = false;
                mtl::SSBOType ssbo_type = mtl::SSBOType::UserDefined;
                uint32_t ssbo_id = 0;
                uint32_t slot_index = 0;
            };

            struct TextureBinding
            {
                bool valid = false;
                Texture *texture = nullptr;
                Sampler *sampler = nullptr;
            };

        private:
            MaterialProgram *material = nullptr;
            const VIL *vil = nullptr;
            SSBOBinding ssbo_bindings[size_t(mtl::SSBOType::RANGE_SIZE)]{};
            TextureBinding texture_bindings[size_t(mtl::TextureSlot::RANGE_SIZE)]{};

        public:
            DescriptorBindingSet(MaterialProgram *mtl = nullptr, const VIL *binding_vil = nullptr);

            void SetMaterial(MaterialProgram *mtl);
            MaterialProgram *GetMaterialProgram() const { return material; }

            void SetVIL(const VIL *binding_vil) { vil = binding_vil; }
            const VIL *GetVIL() const;

            bool SetSSBOBinding(mtl::SSBOType ssbo_type, uint32_t ssbo_id, uint32_t slot_index);
            bool HasSSBOBinding(mtl::SSBOType ssbo_type) const;
            bool GetSSBOBinding(mtl::SSBOType ssbo_type, SSBOBinding &out_binding) const;
            uint32_t GetSSBOID(mtl::SSBOType ssbo_type) const;
            uint32_t GetSlotIndex(mtl::SSBOType ssbo_type) const;
            void ClearSSBOBinding(mtl::SSBOType ssbo_type);

            bool SetTextureBinding(mtl::TextureSlot slot, Texture *texture, Sampler *sampler = nullptr);
            bool GetTextureBinding(mtl::TextureSlot slot, TextureBinding &out_binding) const;
            void ClearTextureBinding(mtl::TextureSlot slot);

            bool SatisfiesResourceLayout(const mtl::MaterialResourceLayout &resource_layout, const char *resource_layout_owner_name = nullptr) const;
            bool HasRequiredResourceBindings(const mtl::MaterialResourceLayout &resource_layout, const char *resource_layout_owner_name = nullptr) const;
            bool HasRequiredResourceBindings() const;
        };
    }//namespace graph
}//namespace hgl

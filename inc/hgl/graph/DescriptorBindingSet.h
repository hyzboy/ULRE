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
            struct ShaderResourceSchema;
        }

        class ShaderProgram;
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
                uint32_t material_private_data_slot = 0;
            };

            struct TextureBinding
            {
                bool valid = false;
                Texture *texture = nullptr;
                Sampler *sampler = nullptr;
            };

        private:
            ShaderProgram *material = nullptr;
            SSBOBinding ssbo_bindings[size_t(mtl::SSBOType::RANGE_SIZE)]{};
            TextureBinding texture_bindings[size_t(mtl::TextureSlot::RANGE_SIZE)]{};

        public:
            DescriptorBindingSet(ShaderProgram *mtl = nullptr);

            void SetMaterial(ShaderProgram *mtl);
            ShaderProgram *GetShaderProgram() const { return material; }

            bool SetSSBOBinding(mtl::SSBOType ssbo_type, uint32_t ssbo_id, uint32_t material_private_data_slot);
            bool HasSSBOBinding(mtl::SSBOType ssbo_type) const;
            bool GetSSBOBinding(mtl::SSBOType ssbo_type, SSBOBinding &out_binding) const;
            uint32_t GetSSBOID(mtl::SSBOType ssbo_type) const;
            uint32_t GetMaterialPrivateDataSlot(mtl::SSBOType ssbo_type) const;
            void ClearSSBOBinding(mtl::SSBOType ssbo_type);

            bool SetTextureBinding(mtl::TextureSlot slot, Texture *texture, Sampler *sampler = nullptr);
            bool GetTextureBinding(mtl::TextureSlot slot, TextureBinding &out_binding) const;
            void ClearTextureBinding(mtl::TextureSlot slot);

            bool SatisfiesResourceLayout(const mtl::ShaderResourceSchema &resource_layout, const char *resource_layout_owner_name = nullptr) const;
            bool HasRequiredResourceBindings(const mtl::ShaderResourceSchema &resource_layout, const char *resource_layout_owner_name = nullptr) const;
            bool HasRequiredResourceBindings() const;
        };
    }//namespace graph
}//namespace hgl

#pragma once

#include<hgl/mtl/MaterialCreateConfig.h>
#include<hgl/mtl/DescriptorSemanticRegistry.h>
#include<hgl/mtl/ShaderDataSchema.h>
#include<hgl/common/TextureSamplerTypeDef.h>
#include<string>
#include<cstdint>
#include<memory>

namespace hgl::graph
{
    class ShaderCreateInfo;
    class ShaderCreateInfoVertex;

    namespace mtl
    {
        class MaterialCreateInfo;

        namespace contract
        {
            struct PhysicalDeviceProfileLite;
        }

        /// Builder for constructing MaterialCreateInfo with write-only interface.
        /// Manages the lifecycle of the MaterialCreateInfo being built.
        /// Once Build() is called, ownership transfers to caller; builder becomes empty.
        class MaterialBuilder
        {
        private:
            std::unique_ptr<MaterialCreateInfo> building_;

        public:
            explicit MaterialBuilder(const MaterialCreateConfig *config);
            ~MaterialBuilder();

            // Disable copy, allow move (simplified: just declare as deleted)
            MaterialBuilder(const MaterialBuilder &) = delete;
            MaterialBuilder &operator=(const MaterialBuilder &) = delete;

            // Build phase write interface
            void SetDevice(const contract::PhysicalDeviceProfileLite *profile);

            bool AddUBOStruct(const uint32_t flag_bits,const UBODescriptorSemantic semantic);
            bool AddSSBOStruct(const uint32_t flag_bits,const SSBODescriptorSemantic semantic);

            bool AddTexture(const ShaderStage flag_bits,const TextureType &tt,const SamplerSlot slot);
            bool AddTextureSampler(const ShaderStage flag_bits,const SamplerType &st,const SamplerSlot slot,const TextureChannelHint channel_hint=TextureChannelHint::RGBA);
            bool AddTextureSampler(const uint32_t flag_bits,const SamplerType &st,const SamplerSlot slot,const TextureChannelHint channel_hint=TextureChannelHint::RGBA);

            bool SetMaterialInstance(const uint32_t data_bytes,const uint32_t shader_stage_flag_bits);
            bool SetMaterialInstance(const ShaderDataSchema schema,const ShaderDataSchemaInfo &schema_info,const uint32_t shader_stage_flag_bits);

            bool SetLocalToWorld(const uint32_t shader_stage_flag_bits);

            // Accessor for shader editing (during build phase)
            ShaderCreateInfoVertex *GetVertexShader();
            ShaderCreateInfo *GetStageShader(ShaderStage ss);

            /// Temporarily expose Resort for InjectLayoutDefines compatibility
            /// (will be removed in Phase C.3)
            void Resort();

            /// Complete the build, compile SPV, and return owning MaterialCreateInfo*.
            /// Returns nullptr on failure. After this call, builder owns nothing.
            MaterialCreateInfo *Build();

            /// Build without compiling SPV (snapshot only, for reflection).
            /// Returns nullptr on failure. After this call, builder owns nothing.
            MaterialCreateInfo *BuildSnapshotOnly();
        };
    }//namespace mtl
}//namespace hgl::graph

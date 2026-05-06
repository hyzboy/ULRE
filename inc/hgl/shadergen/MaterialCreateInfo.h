#pragma once

#include<hgl/shadergen/MaterialDescriptorDB.h>
#include<hgl/mtl/DescriptorSemanticRegistry.h>
#include<hgl/mtl/ShaderDataSchema.h>
#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/shadergen/ShaderStageMap.h>
#include<hgl/mtl/MaterialCreateConfig.h>
#include <hgl/common/TextureSamplerTypeDef.h>
#include<hgl/shadergen/MaterialBuilderBlocks.h>
#include<string>
#include<cassert>

namespace hgl::graph
{
    struct UBODescriptor;
    struct SSBODescriptor;

    class ShaderCreateInfo;

    namespace mtl
    {
        class MaterialBuilder;

        namespace contract
        {
            struct PhysicalDeviceProfileLite;
        }

        class MaterialCreateInfo
        {
            friend class MaterialBuilder;
            // Forward declare InjectLayoutDefines for friend access
            friend bool InjectLayoutDefines(MaterialCreateInfo &mci);

        protected:

            MaterialCreateConfig config;
            uint32_t ubo_range;
            uint32_t ssbo_range;

            MaterialDescriptorDB descriptor_db;                     ///<材质描述符管理器
            DescriptorBindingSlots binding_contract;                ///<descriptor semantic contract (phase 2)

            MaterialInstanceBlock material_instance;
            LocalToWorldBlock     local_to_world;

            ShaderStageMap shader_map;                         ///<着色器列表

        private:

            bool AddResolvedUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const UBODescriptorSemantic semantic,const std::string &struct_name,const std::string &name);

            bool AddResolvedSSBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const SSBODescriptorSemantic semantic,const std::string &struct_name,const std::string &name);

            /// Populate binding_contract from the actual descriptors in descriptor_db.
            /// Call after all AddUBOStruct / AddSSBOStruct / AddTextureSampler calls.
            void BuildBindingContract();

            /// Delegates to descriptor_db.Resort() — finalises set and binding numbers.
            /// Call this before BuildShaderLayoutContract() if you need layout numbers
            /// outside of CompilePreparedShaderSources() (which calls Resort internally).
            void Resort(){descriptor_db.Resort();}

            void SetDevice(const contract::PhysicalDeviceProfileLite *profile);

            bool SetMaterialInstance(const uint32_t mi_struct_bytes,const uint32_t shader_stage_flag_bits);
            bool SetMaterialInstance(const ShaderDataSchema schema,const ShaderDataSchemaInfo &schema_info,const uint32_t shader_stage_flag_bits);

            bool SetLocalToWorld(const uint32_t shader_stage_flag_bits);

            bool AddUBOStruct(const uint32_t flag_bits,const UBODescriptorSemantic semantic);

            bool AddSSBOStruct(const uint32_t flag_bits,const SSBODescriptorSemantic semantic);

            bool AddTexture(const ShaderStage flag_bits,const TextureType &tt,const SamplerSlot slot);
            bool AddTextureSampler(const ShaderStage flag_bits,const SamplerType &st,const SamplerSlot slot,const TextureChannelHint channel_hint=TextureChannelHint::RGBA);
            bool AddTextureSampler(const uint32_t flag_bits,const SamplerType &st,const SamplerSlot slot,const TextureChannelHint channel_hint=TextureChannelHint::RGBA);

            bool CompilePreparedShaderSources();     ///< 直接编译各阶段的 FinalGLSL 到 SPV

        public:

            const PrimitiveType GetPrimitiveType()const{return config.prim;}

            const   uint32      GetShaderStage  ()const{return config.shader_stage_flag_bit;}

                    bool        HasLocalToWorld ()const{return config.local_to_world;}

                    bool        HasShader       (const ShaderStage ss)const{return config.shader_stage_flag_bit&(uint32)ss;}

                    bool        HasVertex       ()const{return HasShader(ShaderStage::Vertex);}
                    bool        HasFragment     ()const{return HasShader(ShaderStage::Fragment);}
        //          bool        HasCompute      ()const{return HasShader(ShaderStage::Compute);}

            ShaderCreateInfo *GetStageShader(const ShaderStage ss)
            {
                return shader_map.Find(ss);
            }
            const ShaderCreateInfo *GetStageShader(const ShaderStage ss)const
            {
                return shader_map.Find(ss);
            }

            ShaderCreateInfoVertex *           GetVertexShader(){auto *s=GetStageShader(ShaderStage::Vertex);assert(!s||HasVertex());return static_cast<ShaderCreateInfoVertex *>(s);}
            const ShaderCreateInfoVertex *     GetVertexShader()const{const auto *s=GetStageShader(ShaderStage::Vertex);assert(!s||HasVertex());return static_cast<const ShaderCreateInfoVertex *>(s);}

            const ShaderStageMap &GetShaderMap()const{return shader_map;}

        public:

            const MaterialDescriptorDB &GetDescriptorInfo()const{return descriptor_db;}
            const DescriptorBindingSlots &GetBindingContract()const{return binding_contract;}

            const MaterialInstanceBlock &GetMaterialInstance()const{return material_instance;}
            const LocalToWorldBlock &GetLocalToWorld()const{return local_to_world;}

        public:

            MaterialCreateInfo(const MaterialCreateConfig *);
            ~MaterialCreateInfo();  // Need explicit destructor to properly clean up shader_map

            bool CompileShaderStagesToSPV();
        };//class MaterialCreateInfo
    }//namespace mtl
}//namespace hgl::graph

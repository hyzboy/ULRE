#pragma once

#include<hgl/shadergen/MaterialDescriptorDB.h>
#include<hgl/mtl/DescriptorSemanticRegistry.h>
#include<hgl/mtl/ShaderDataSchema.h>
#include<hgl/shadergen/ShaderStageMap.h>
#include<hgl/mtl/MaterialCreateConfig.h>
#include <hgl/common/TextureSamplerTypeDef.h>
#include<string>

namespace hgl::graph
{
    struct UBODescriptor;
    struct SSBODescriptor;

    class ShaderCreateInfoVertex;
    class ShaderCreateInfo;

    namespace mtl
    {
        namespace contract
        {
            struct PhysicalDeviceProfileLite;
        }

        class MaterialCreateInfo
        {
        protected:

            MaterialCreateConfig config;
            uint32_t ubo_range;
            uint32_t ssbo_range;

            MaterialDescriptorDB descriptor_db;                    ///<材质描述符管理器
            DescriptorBindingSlots binding_contract;                       ///<descriptor semantic contract (phase 2)

            uint32_t material_instance_stride;                      ///<MaterialInstance数据长度
            uint32_t material_instance_stage_bits;                  ///<MaterialInstance着色器阶段
            uint32_t material_instance_max_count;
            ShaderDataSchema material_instance_schema;              ///<MaterialBindingInstance schema枚举
            std::string material_instance_schema_file;              ///<schema GLSL文件名
            SSBODescriptor *material_instance_ssbo;

            uint32_t local_to_world_max_count;
            uint32_t local_to_world_stage_bits;
            SSBODescriptor *local_to_world_ssbo;

            ShaderStageMap shader_map;                         ///<着色器列表

            bool has_local_to_world;

        private:

            bool AddResolvedUBO(const ShaderStage flag_bits,const DescriptorSetType set_type,const UBODescriptorSemantic semantic,const std::string &struct_name,const std::string &name);
            bool AddResolvedUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const UBODescriptorSemantic semantic,const std::string &struct_name,const std::string &name);

            bool AddResolvedSSBO(const ShaderStage flag_bits,const DescriptorSetType set_type,const SSBODescriptorSemantic semantic,const std::string &struct_name,const std::string &name);
            bool AddResolvedSSBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const SSBODescriptorSemantic semantic,const std::string &struct_name,const std::string &name);

        public:

            const PrimitiveType GetPrimitiveType()const{return config.prim;}

            const   uint32      GetShaderStage  ()const{return config.shader_stage_flag_bit;}

                    bool        hasShader       (const ShaderStage ss)const{return config.shader_stage_flag_bit&(uint32)ss;}

                    bool        hasVertex       ()const{return hasShader(ShaderStage::Vertex);}
        //          bool        hasTessCtrl     ()const{return hasShader(ShaderStage::TessControl);}
        //          bool        hasTessEval     ()const{return hasShader(ShaderStage::TessEval);}
                    bool        hasFragment     ()const{return hasShader(ShaderStage::Fragment);}
        //          bool        hasCompute      ()const{return hasShader(ShaderStage::Compute);}

            ShaderCreateInfo *GetStageShader(const ShaderStage ss)
            {
                if(!shader_map.ContainsKey(ss))
                    return nullptr;

                return shader_map[ss];
            }
            const ShaderCreateInfo *GetStageShader(const ShaderStage ss)const
            {
                if(!shader_map.ContainsKey(ss))
                    return nullptr;

                return shader_map[ss];
            }

            ShaderCreateInfoVertex *           GetVertexShader(){return reinterpret_cast<ShaderCreateInfoVertex *>(GetStageShader(ShaderStage::Vertex));}
            const ShaderCreateInfoVertex *     GetVertexShader()const{return reinterpret_cast<const ShaderCreateInfoVertex *>(GetStageShader(ShaderStage::Vertex));}

            const ShaderStageMap &GetShaderMap()const{return shader_map;}

        public:

            const MaterialDescriptorDB &GetDescriptorInfo()const{return descriptor_db;}
            const DescriptorBindingSlots &GetBindingContract()const{return binding_contract;}

            /// Populate binding_contract from the actual descriptors in descriptor_db.
            /// Call after all AddUBOStruct / AddSSBOStruct / AddTextureSampler calls.
            void BuildBindingContract();

            /// Delegates to descriptor_db.Resort() — finalises set and binding numbers.
            /// Call this before BuildShaderLayoutContract() if you need layout numbers
            /// outside of CreateShaderDirect() (which calls Resort internally).
            void Resort(){descriptor_db.Resort();}

            const uint32_t GetMaterialInstanceStride    ()const{return material_instance_stride;}
            const uint32_t GetMaterialInstanceMaxCount  ()const{return material_instance_max_count;}
            const ShaderDataSchema GetMaterialInstanceSchema()const{return material_instance_schema;}
            const std::string &GetMaterialInstanceSchemaFile()const{return material_instance_schema_file;}

            const bool HasLocalToWorld                  ()const{return has_local_to_world;}

        public:

            MaterialCreateInfo(const MaterialCreateConfig *);
            ~MaterialCreateInfo();  // Need explicit destructor to properly clean up shader_map

            void SetDevice(const contract::PhysicalDeviceProfileLite *profile);

            bool SetMaterialInstance(const uint32_t mi_struct_bytes,const uint32_t shader_stage_flag_bits);
            bool SetMaterialInstance(const ShaderDataSchema schema,const ShaderDataSchemaInfo &schema_info,const uint32_t shader_stage_flag_bits);

            bool SetLocalToWorld(const uint32_t shader_stage_flag_bits);
            //bool SetWorldPosition(const uint32_t shader_stage_flag_bits);

            bool AddUBOStruct(const uint32_t flag_bits,const UBODescriptorSemantic semantic);

            bool AddSSBOStruct(const uint32_t flag_bits,const SSBODescriptorSemantic semantic);

            bool AddTexture(const ShaderStage flag_bits,const TextureType &tt,const SamplerSlot slot);
            bool AddTextureSampler(const ShaderStage flag_bits,const SamplerType &st,const SamplerSlot slot,const TextureChannelHint channel_hint=TextureChannelHint::RGBA);
            bool AddTextureSampler(const uint32_t flag_bits,const SamplerType &st,const SamplerSlot slot,const TextureChannelHint channel_hint=TextureChannelHint::RGBA);

            bool CreateShaderDirect();               ///< 直接编译各阶段的 FinalGLSL 到 SPV
        };//class MaterialCreateInfo
    }//namespace mtl
}//namespace hgl::graph

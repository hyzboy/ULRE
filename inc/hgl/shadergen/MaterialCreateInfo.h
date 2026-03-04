#pragma once

#include<hgl/shadergen/MaterialDescriptorInfo.h>
#include<hgl/graph/mtl/DescriptorBindingContract.h>
#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/shadergen/ShaderCreateInfoGeometry.h>
#include<hgl/shadergen/ShaderCreateInfoFragment.h>
#include<hgl/shadergen/ShaderCreateInfoMap.h>
#include<hgl/graph/render/RenderOptions.h>
#include<hgl/graph/render/RenderTargetOutputConfig.h>
#include<hgl/graph/mtl/MaterialCreateConfig.h>
#include<hgl/graph/mtl/ShaderBufferSource.h>
#include<hgl/vk/VKTextureType.h>
#include<hgl/vk/VKSamplerType.h>
#include<string>

namespace hgl::graph
{
    struct VulkanDevAttr;
    struct UBODescriptor;
    struct SSBODescriptor;

    namespace mtl
    {
        class MaterialCreateInfo
        {
        protected:

            MaterialCreateConfig config;
            uint32_t ubo_range;
            uint32_t ssbo_range;

            MaterialDescriptorInfo mdi;                             ///<材质描述符管理器
            BindingContract binding_contract;                       ///<descriptor semantic contract (phase 2)

            std::string mi_codes;                                   ///<MaterialInstance代码
            uint32_t mi_data_bytes;                                 ///<MaterialInstance数据长度
            uint32_t mi_shader_stage;                               ///<MaterialInstance着色器阶段
            uint32_t mi_max_count;
        #if defined(HGL_MI_USE_SSBO) && HGL_MI_USE_SSBO
            SSBODescriptor *mi_ssbo;
        #else
            UBODescriptor *mi_ubo;
        #endif

            uint32_t l2w_max_count;
            uint32_t l2w_shader_stage;
        #if defined(HGL_L2W_USE_SSBO)
            SSBODescriptor *l2w_ssbo;
        #else
            UBODescriptor *l2w_ubo;
        #endif

            ShaderCreateInfoMap shader_map;                         ///<着色器列表

            ShaderCreateInfoVertex *vert;
            ShaderCreateInfoGeometry *geom;
            ShaderCreateInfoFragment *frag;

            bool has_l2w_matrix;

        public:

            const PrimitiveType GetPrimitiveType()const{return config.prim;}

            const   uint32      GetShaderStage  ()const{return config.shader_stage_flag_bit;}

                    bool        hasShader       (const ShaderStage ss)const{return config.shader_stage_flag_bit&(uint32)ss;}

                    bool        hasVertex       ()const{return hasShader(ShaderStage::Vertex);}
        //          bool        hasTessCtrl     ()const{return hasShader(ShaderStage::TessControl);}
        //          bool        hasTessEval     ()const{return hasShader(ShaderStage::TessEval);}
                    bool        hasGeometry     ()const{return hasShader(ShaderStage::Geometry);}
                    bool        hasFragment     ()const{return hasShader(ShaderStage::Fragment);}
        //          bool        hasCompute      ()const{return hasShader(ShaderStage::Compute);}

            ShaderCreateInfoVertex *   GetVS()const{return vert;}
            ShaderCreateInfoGeometry * GetGS()const{return geom;}
            ShaderCreateInfoFragment * GetFS()const{return frag;}

            const ShaderCreateInfoMap &GetShaderMap()const{return shader_map;}

        public:

            const MaterialDescriptorInfo &GetMDI()const{return mdi;}
            const BindingContract &GetBindingContract()const{return binding_contract;}

            void SetBindingContract(const BindingContract &contract){binding_contract=contract;}

            const uint32_t GetMIDataBytes   ()const{return mi_data_bytes;}
            const uint32_t GetMIMaxCount    ()const{return mi_max_count;}

            const bool hasLocalToWorld      ()const{return has_l2w_matrix;}

        public:

            MaterialCreateInfo(const MaterialCreateConfig *);
            ~MaterialCreateInfo();  // Need explicit destructor to properly clean up shader_map

            void SetDevice(const VulkanDevAttr *dev_attr);

            bool SetMaterialInstance(const std::string &mi_glsl_codes,const uint32_t mi_struct_bytes,const uint32_t shader_stage_flag_bits);
            bool SetMaterialInstance(const char *mi_glsl_codes,const uint32_t mi_struct_bytes,const uint32_t shader_stage_flag_bits)
            {
                return SetMaterialInstance(std::string(mi_glsl_codes?mi_glsl_codes:""),mi_struct_bytes,shader_stage_flag_bits);
            }

            bool SetLocalToWorld(const uint32_t shader_stage_flag_bits);
            //bool SetWorldPosition(const uint32_t shader_stage_flag_bits);

            bool AddStruct(const std::string &ubo_typename,const std::string &codes);
            bool AddStruct(const char *ubo_typename,const char *codes)
            {
                return AddStruct(std::string(ubo_typename?ubo_typename:""),std::string(codes?codes:""));
            }

            bool AddUBO(const ShaderStage flag_bits,const DescriptorSetType set_type,const std::string &struct_name,const std::string &name);
            bool AddUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const std::string &struct_name,const std::string &name);
            bool AddUBO(const ShaderStage flag_bits,const DescriptorSetType set_type,const char *struct_name,const char *name)
            {
                return AddUBO(flag_bits,set_type,std::string(struct_name?struct_name:""),std::string(name?name:""));
            }
            bool AddUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const char *struct_name,const char *name)
            {
                return AddUBO(flag_bits,set_type,std::string(struct_name?struct_name:""),std::string(name?name:""));
            }

            bool AddUBOStruct(const uint32_t flag_bits,const ShaderBufferSource &ss);

            bool AddSSBO(const ShaderStage flag_bits,const DescriptorSetType set_type,const std::string &struct_name,const std::string &name);
            bool AddSSBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const std::string &struct_name,const std::string &name);
            bool AddSSBO(const ShaderStage flag_bits,const DescriptorSetType set_type,const char *struct_name,const char *name)
            {
                return AddSSBO(flag_bits,set_type,std::string(struct_name?struct_name:""),std::string(name?name:""));
            }
            bool AddSSBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const char *struct_name,const char *name)
            {
                return AddSSBO(flag_bits,set_type,std::string(struct_name?struct_name:""),std::string(name?name:""));
            }

            bool AddSSBOStruct(const uint32_t flag_bits,const ShaderBufferSource &ss);

            bool AddTexture(const ShaderStage flag_bits,const DescriptorSetType set_type,const TextureType &tt,const std::string &name);
            bool AddTextureSampler(const ShaderStage flag_bits,const DescriptorSetType set_type,const SamplerType &st,const std::string &name);
            bool AddTexture(const ShaderStage flag_bits,const DescriptorSetType set_type,const TextureType &tt,const char *name)
            {
                return AddTexture(flag_bits,set_type,tt,std::string(name?name:""));
            }
            bool AddTextureSampler(const ShaderStage flag_bits,const DescriptorSetType set_type,const SamplerType &st,const char *name)
            {
                return AddTextureSampler(flag_bits,set_type,st,std::string(name?name:""));
            }

            bool CreateShader();
        };//class MaterialCreateInfo
    }//namespace mtl
}//namespace hgl::graph

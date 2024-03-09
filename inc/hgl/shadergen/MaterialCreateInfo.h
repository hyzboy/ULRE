#pragma once

#include<hgl/shadergen/MaterialDescriptorInfo.h>
#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/shadergen/ShaderCreateInfoGeometry.h>
#include<hgl/shadergen/ShaderCreateInfoFragment.h>
#include<hgl/shadergen/ShaderCreateInfoMap.h>
#include<hgl/graph/RenderTargetOutputConfig.h>
#include<hgl/graph/mtl/MaterialConfig.h>
#include<hgl/graph/mtl/ShaderBuffer.h>
#include<hgl/graph/VKSamplerType.h>

namespace hgl{namespace graph
{
    struct GPUDeviceAttribute;
    struct UBODescriptor;

    namespace mtl
    {
        class MaterialCreateInfo
        {
        protected:

            const MaterialCreateConfig *config;
            uint32_t ubo_range;
            uint32_t ssbo_range;

            MaterialDescriptorInfo mdi;                             ///<材质描述符管理器

            AnsiString mi_codes;                                    ///<MaterialInstance代码
            uint32_t mi_data_bytes;                                 ///<MaterialInstance数据长度
            uint32_t mi_shader_stage;                               ///<MaterialInstance着色器阶段
            uint32_t mi_max_count;
            UBODescriptor *mi_ubo;

            uint32_t l2w_shader_stage;
            uint32_t l2w_max_count;
            UBODescriptor *l2w_ubo;

            ShaderCreateInfoMap shader_map;                         ///<着色器列表

            ShaderCreateInfoVertex *vert;
            ShaderCreateInfoGeometry *geom;
            ShaderCreateInfoFragment *frag;

        public:

            const   AnsiString &GetName         ()const{return config->mtl_name;}

            const   uint32      GetShaderStage  ()const{return config->shader_stage_flag_bit;}

                    bool        hasShader       (const VkShaderStageFlagBits ss)const{return config->shader_stage_flag_bit&ss;}

                    bool        hasVertex       ()const{return hasShader(VK_SHADER_STAGE_VERTEX_BIT);}
        //          bool        hasTessCtrl     ()const{return hasShader(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);}
        //          bool        hasTessEval     ()const{return hasShader(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);}
                    bool        hasGeometry     ()const{return hasShader(VK_SHADER_STAGE_GEOMETRY_BIT);}
                    bool        hasFragment     ()const{return hasShader(VK_SHADER_STAGE_FRAGMENT_BIT);}
        //          bool        hasCompute      ()const{return hasShader(VK_SHADER_STAGE_COMPUTE_BIT);}

            ShaderCreateInfoVertex *   GetVS()const{return vert;}
            ShaderCreateInfoGeometry * GetGS()const{return geom;}
            ShaderCreateInfoFragment * GetFS()const{return frag;}

            const ShaderCreateInfoMap &GetShaderMap()const{return shader_map;}

        public:

            const MaterialDescriptorInfo &GetMDI()const{return mdi;}

            const uint32_t GetMIDataBytes   ()const{return mi_data_bytes;}
            const uint32_t GetMIMaxCount    ()const{return mi_max_count;}

        public:

            MaterialCreateInfo(const MaterialCreateConfig *);
            ~MaterialCreateInfo()=default;

            bool SetMaterialInstance(const AnsiString &mi_glsl_codes,const uint32_t mi_struct_bytes,const uint32_t shader_stage_flag_bits);

            bool SetLocalToWorld(const uint32_t shader_stage_flag_bits);

            bool AddStruct(const AnsiString &ubo_typename,const AnsiString &codes);
            bool AddStruct(const ShaderBufferSource &ss){return AddStruct(ss.struct_name,ss.codes);}

            bool AddUBO(const VkShaderStageFlagBits flag_bits,const DescriptorSetType set_type,const AnsiString &struct_name,const AnsiString &name);
            bool AddUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const AnsiString &struct_name,const AnsiString &name);
            bool AddUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const ShaderBufferSource &ss){return AddUBO(flag_bits,set_type,ss.struct_name,ss.name);}

            bool AddSampler(const VkShaderStageFlagBits flag_bits,const DescriptorSetType set_type,const SamplerType &st,const AnsiString &name);

            bool CreateShader();
        };//class MaterialCreateInfo
    }//namespace mtl
}//namespace graph
}//namespace hgl

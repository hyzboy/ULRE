#pragma once

#include<hgl/shadergen/MaterialDescriptorInfo.h>
#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/shadergen/ShaderCreateInfoGeometry.h>
#include<hgl/shadergen/ShaderCreateInfoFragment.h>
#include<hgl/shadergen/ShaderCreateInfoMap.h>
#include<hgl/graph/RenderTargetOutputConfig.h>
#include<hgl/graph/mtl/MaterialCreateConfig.h>
#include<hgl/graph/mtl/ShaderBufferSource.h>
#include<hgl/graph/VKSamplerType.h>

namespace hgl::graph
{
    struct VulkanDevAttr;
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

            uint32_t l2w_max_count;
            uint32_t l2w_shader_stage;
            UBODescriptor *l2w_ubo;

            ShaderCreateInfoMap shader_map;                         ///<着色器列表

            ShaderCreateInfoVertex *vert;
            ShaderCreateInfoGeometry *geom;
            ShaderCreateInfoFragment *frag;

            bool has_l2w_matrix;

        public:

            const PrimitiveType GetPrimitiveType()const{return config->prim;}

            const   uint32      GetShaderStage  ()const{return config->shader_stage_flag_bit;}

                    bool        hasShader       (const ShaderStage ss)const{return config->shader_stage_flag_bit&(uint32)ss;}

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

            const uint32_t GetMIDataBytes   ()const{return mi_data_bytes;}
            const uint32_t GetMIMaxCount    ()const{return mi_max_count;}

            const bool hasLocalToWorld      ()const{return has_l2w_matrix;}

        public:

            MaterialCreateInfo(const MaterialCreateConfig *);
            ~MaterialCreateInfo()=default;

            void SetDevice(const VulkanDevAttr *dev_attr);

            bool SetMaterialInstance(const AnsiString &mi_glsl_codes,const uint32_t mi_struct_bytes,const uint32_t shader_stage_flag_bits);

            bool SetLocalToWorld(const uint32_t shader_stage_flag_bits);
            //bool SetWorldPosition(const uint32_t shader_stage_flag_bits);

            bool AddStruct(const AnsiString &ubo_typename,const AnsiString &codes);

            bool AddUBO(const ShaderStage flag_bits,const DescriptorSetType set_type,const AnsiString &struct_name,const AnsiString &name);
            bool AddUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const AnsiString &struct_name,const AnsiString &name);

            bool AddUBOStruct(const uint32_t flag_bits,const ShaderBufferSource &ss);

            bool AddTextureSampler(const ShaderStage flag_bits,const DescriptorSetType set_type,const SamplerType &st,const AnsiString &name);

            bool CreateShader();
        };//class MaterialCreateInfo
    }//namespace mtl
}//namespace hgl::graph

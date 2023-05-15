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

STD_MTL_NAMESPACE_BEGIN
class MaterialCreateInfo
{
protected:

    const MaterialConfig *config;

    MaterialDescriptorInfo mdi;                             ///<材质描述符管理器

    AnsiString mi_codes;                                    ///<MaterialInstance代码
    uint32_t mi_length;                                     ///<MaterialInstance数据长度

    ShaderCreateInfoMap shader_map;                         ///<着色器列表

    ShaderCreateInfoVertex *vert;
    ShaderCreateInfoGeometry *geom;
    ShaderCreateInfoFragment *frag;

public:

    const AnsiString &GetName()const{return config->mtl_name;}

    const uint32 GetShaderStage()const{return config->shader_stage;}

    bool hasShader(const VkShaderStageFlagBits ss)const{return config->shader_stage&ss;}

    bool hasVertex  ()const{return hasShader(VK_SHADER_STAGE_VERTEX_BIT);}
//    bool hasTessCtrl()const{return hasShader(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);}
//    bool hasTessEval()const{return hasShader(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);}
    bool hasGeometry()const{return hasShader(VK_SHADER_STAGE_GEOMETRY_BIT);}
    bool hasFragment()const{return hasShader(VK_SHADER_STAGE_FRAGMENT_BIT);}
//    bool hasCompute ()const{return hasShader(VK_SHADER_STAGE_COMPUTE_BIT);}

    ShaderCreateInfoVertex *   GetVS()const{return vert;}
    ShaderCreateInfoGeometry * GetGS()const{return geom;}
    ShaderCreateInfoFragment * GetFS()const{return frag;}

public:

    MaterialCreateInfo(const MaterialConfig *);
    ~MaterialCreateInfo()=default;

    bool SetMaterialInstance(const AnsiString &codes,const uint32_t length)
    {
        if(length>0&&codes.Length()<4)return(false);

        mi_length=length;

        if(length>0)
            mi_codes=codes;

        return(true);
    }

    bool AddStruct(const AnsiString &ubo_typename,const AnsiString &codes);
    bool AddStruct(const ShaderBufferSource &ss)
    {
        return AddStruct(ss.struct_name,ss.codes);
    }

    bool AddUBO(const VkShaderStageFlagBits flag_bits,const DescriptorSetType set_type,const AnsiString &type_name,const AnsiString &name);
    bool AddSampler(const VkShaderStageFlagBits flag_bits,const DescriptorSetType set_type,const SamplerType &st,const AnsiString &name);

    bool AddUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const ShaderBufferSource &ss);

    bool CreateShader();

    const MaterialDescriptorInfo &GetMDI()const{return mdi;}
};//class MaterialCreateInfo
STD_MTL_NAMESPACE_END

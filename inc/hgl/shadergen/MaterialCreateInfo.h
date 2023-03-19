#pragma once

#include<hgl/shadergen/MaterialDescriptorInfo.h>
#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/shadergen/ShaderCreateInfoGeometry.h>
#include<hgl/shadergen/ShaderCreateInfoFragment.h>
#include<hgl/shadergen/ShaderCreateInfoMap.h>
#include<hgl/graph/VKSamplerType.h>

SHADERGEN_NAMESPACE_BEGIN
class MaterialCreateInfo
{
    AnsiString shader_name;

protected:

    uint rt_color_count;                                    ///<输出的RT数量
    bool rt_depth;                                          ///<是否输出深度

    uint32_t shader_stage;                                  ///<着色器阶段

    MaterialDescriptorInfo mdm;                             ///<材质描述符管理器

    ShaderCreateInfoMap shader_map;                         ///<着色器列表

    ShaderCreateInfoVertex *vert;
    ShaderCreateInfoGeometry *geom;
    ShaderCreateInfoFragment *frag;

public:

    const AnsiString &GetName()const{return shader_name;}

    const uint32 GetShaderStage()const{return shader_stage;}

    bool hasShader(const VkShaderStageFlagBits ss)const{return shader_stage&ss;}

    bool hasVertex  ()const{return hasShader(VK_SHADER_STAGE_VERTEX_BIT);}
//    bool hasTessCtrl()const{return hasShader(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);}
//    bool hasTessEval()const{return hasShader(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);}
    bool hasGeometry()const{return hasShader(VK_SHADER_STAGE_GEOMETRY_BIT);}
    bool hasFragment()const{return hasShader(VK_SHADER_STAGE_FRAGMENT_BIT);}
//    bool hasCompute ()const{return hasShader(VK_SHADER_STAGE_COMPUTE_BIT);}

    const ShaderCreateInfoVertex *   GetVS()const{return vert;}
    const ShaderCreateInfoGeometry * GetGS()const{return geom;}
    const ShaderCreateInfoFragment * GetFS()const{return frag;}

public:

    MaterialCreateInfo(const AnsiString &,const uint rc,const bool rd,const uint32 ss=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
    ~MaterialCreateInfo()=default;

    bool AddStruct(const AnsiString &ubo_typename,const AnsiString &codes);

    bool AddUBO(const VkShaderStageFlagBits flag_bits,const DescriptorSetType set_type,const AnsiString &type_name,const AnsiString &name);
    bool AddSampler(const VkShaderStageFlagBits flag_bits,const DescriptorSetType set_type,const SamplerType &st,const AnsiString &name);

    bool CreateShader();
};//class MaterialCreateInfo
SHADERGEN_NAMESPACE_END

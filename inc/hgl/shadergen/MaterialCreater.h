#pragma once

#include<hgl/shadergen/MaterialDescriptorManager.h>
#include<hgl/shadergen/ShaderCreaterVertex.h>
#include<hgl/shadergen/ShaderCreaterGeometry.h>
#include<hgl/shadergen/ShaderCreaterFragment.h>
#include<hgl/shadergen/ShaderCreaterMap.h>
#include<hgl/graph/VKSamplerType.h>

SHADERGEN_NAMESPACE_BEGIN
class MaterialCreater
{
    uint rt_count;                                          ///<输出的RT数量

    uint32_t shader_stage;                                  ///<着色器阶段

    MaterialDescriptorManager mdm;                          ///<材质描述符管理器

    ShaderCreaterMap shader_map;                            ///<着色器列表

    ShaderCreaterVertex *vert;
    ShaderCreaterGeometry *geom;
    ShaderCreaterFragment *frag;

private:

    void SetContext();

public:

    bool hasShader(const VkShaderStageFlagBits ss)const{return shader_stage&ss;}

    bool hasVertex  ()const{return hasShader(VK_SHADER_STAGE_VERTEX_BIT);}
//    bool hasTessCtrl()const{return hasShader(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);}
//    bool hasTessEval()const{return hasShader(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);}
    bool hasGeometry()const{return hasShader(VK_SHADER_STAGE_GEOMETRY_BIT);}
    bool hasFragment()const{return hasShader(VK_SHADER_STAGE_FRAGMENT_BIT);}
//    bool hasCompute ()const{return hasShader(VK_SHADER_STAGE_COMPUTE_BIT);}

    ShaderCreaterVertex *   GetVS(){return vert;}
    ShaderCreaterGeometry * GetGS(){return geom;}
    ShaderCreaterFragment * GetFS(){return frag;}

public:

    MaterialCreater(const uint rc,const uint32 ss=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
    ~MaterialCreater()=default;

    bool AddUBOStruct(const AnsiString &ubo_typename,const AnsiString &codes);

    bool AddUBO(const VkShaderStageFlagBits flag_bits,const DescriptorSetType set_type,const AnsiString &type_name,const AnsiString &name);
    bool AddSampler(const VkShaderStageFlagBits flag_bits,const DescriptorSetType set_type,const SamplerType &st,const AnsiString &name);

    bool CompileShader();
};//class MaterialCreater
SHADERGEN_NAMESPACE_END
#pragma once

#include<hgl/shadergen/ShaderGenNamespace.h>
#include<hgl/type/Map.h>
#include<hgl/type/StringList.h>
#include<hgl/graph/VKShaderStage.h>
#include<hgl/graph/VKDescriptorSetType.h>
#include<hgl/shadergen/MaterialDescriptorManager.h>

SHADERGEN_NAMESPACE_BEGIN

using UBODescriptorList=List<const UBODescriptor *>;
using SamplerDescriptorList=List<const SamplerDescriptor *>;
using ConstValueDescriptorList=ObjectList<ConstValueDescriptor>;
using SubpassInputDescriptorList=ObjectList<SubpassInputDescriptor>;

/**
* Shader数据管理器,用于生成正式Shader前的资源统计
*/
class ShaderDescriptorManager
{
    VkShaderStageFlagBits shader_stage;

    ShaderStageIO stage_io;

    AnsiStringList                      struct_list;        //用到的结构列表

    //ubo/object在这里以及MaterialDescriptorManager中均有一份，mdm中的用于产生set/binding号，这里的用于产生shader
    UBODescriptorList                   ubo_list;
    SamplerDescriptorList               sampler_list;
    
    ConstValueDescriptorList            const_value_list;
    SubpassInputDescriptorList          subpass_input;
    
    ShaderPushConstant                  push_constant;

public:

    ShaderDescriptorManager(VkShaderStageFlagBits);
    ~ShaderDescriptorManager()=default;

    const VkShaderStageFlagBits GetStageBits()const { return shader_stage; }
    const AnsiString            GetStageName()const { return AnsiString(GetShaderStageName(shader_stage)); }

public:

    const ShaderStageIO &               GetShaderStageIO()const{return stage_io;}

    const AnsiStringList &              GetStructList()const{return struct_list;}

    const UBODescriptorList &           GetUBOList()const{return ubo_list;}
    const SamplerDescriptorList &       GetSamplerList()const{return sampler_list;}

    const ConstValueDescriptorList &    GetConstList()const{return const_value_list;}

    const SubpassInputDescriptorList &  GetSubpassInputList()const{return subpass_input;}

public:

    bool AddInput(ShaderStage *);
    bool AddOutput(ShaderStage *);

    bool AddUBO(DescriptorSetType type,const UBODescriptor *sd);
    bool AddSampler(DescriptorSetType type,const SamplerDescriptor *sd);

    bool AddConstValue(ConstValueDescriptor *sd);    
    bool AddSubpassInput(const AnsiString name,uint8_t index);
    
    void SetPushConstant(const AnsiString name,uint8_t offset,uint8_t size);

#ifdef _DEBUG
    void DebugOutput(int);
#endif//_DEBUG
};//class ShaderDescriptorManager
SHADERGEN_NAMESPACE_END

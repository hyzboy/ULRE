#pragma once

#include<hgl/shadergen/ShaderGenNamespace.h>
#include<hgl/type/Map.h>
#include<hgl/type/StringList.h>
#include<hgl/graph/VKShaderStage.h>
#include<hgl/graph/VKDescriptorSetType.h>
#include<hgl/shadergen/MaterialDescriptorInfo.h>

SHADERGEN_NAMESPACE_BEGIN

using UBODescriptorList=List<const UBODescriptor *>;
using SamplerDescriptorList=List<const SamplerDescriptor *>;
using ConstValueDescriptorList=ObjectList<ConstValueDescriptor>;
using SubpassInputDescriptorList=ObjectList<SubpassInputDescriptor>;

/**
* Shader数据管理器,用于生成正式Shader前的资源统计
*/
class ShaderDescriptorInfo
{
    VkShaderStageFlagBits stage_flag;

    ShaderStageIO stage_io;

    AnsiStringList                      struct_list;        //用到的结构列表

    //ubo/object在这里以及MaterialDescriptorInfo中均有一份，mdi中的用于产生set/binding号，这里的用于产生shader
    UBODescriptorList                   ubo_list;
    SamplerDescriptorList               sampler_list;
    
    ConstValueDescriptorList            const_value_list;
    SubpassInputDescriptorList          subpass_input;
    
    ShaderPushConstant                  push_constant;

public:

    ShaderDescriptorInfo(VkShaderStageFlagBits);
    ~ShaderDescriptorInfo()=default;

    const VkShaderStageFlagBits         GetStageFlag()const { return stage_flag; }
    const AnsiString                    GetStageName()const { return AnsiString(GetShaderStageName(stage_flag)); }

public:

    const ShaderStageIO &               GetShaderStageIO()const{return stage_io;}

    const AnsiStringList &              GetStructList()const{return struct_list;}

    const UBODescriptorList &           GetUBOList()const{return ubo_list;}
    const SamplerDescriptorList &       GetSamplerList()const{return sampler_list;}

    const ConstValueDescriptorList &    GetConstList()const{return const_value_list;}

    const SubpassInputDescriptorList &  GetSubpassInputList()const{return subpass_input;}

public:

    bool AddInput(ShaderAttribute *);
    bool AddOutput(ShaderAttribute *);

    bool AddUBO(DescriptorSetType type,const UBODescriptor *sd);
    bool AddSampler(DescriptorSetType type,const SamplerDescriptor *sd);

    bool AddConstValue(ConstValueDescriptor *sd);    
    bool AddSubpassInput(const AnsiString name,uint8_t index);
    
    void SetPushConstant(const AnsiString name,uint8_t offset,uint8_t size);

#ifdef _DEBUG
    void DebugOutput(int);
#endif//_DEBUG
};//class ShaderDescriptorInfo
SHADERGEN_NAMESPACE_END

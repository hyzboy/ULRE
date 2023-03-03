#pragma once

#include<hgl/shadergen/ShaderCommon.h>
#include<hgl/type/Map.h>
#include<hgl/graph/VKShaderStage.h>
#include<hgl/graph/VKDescriptorSetType.h>
#include<hgl/shadergen/MaterialDescriptorManager.h>

SHADERGEN_NAMESPACE_BEGIN

using ConstUBODescriptorList=List<const UBODescriptor *>;
using ConstSamplerDescriptorList=List<const SamplerDescriptor *>;

/**
* Shader数据管理器,用于生成正式Shader前的资源统计
*/
class ShaderDescriptorManager
{
    ShaderStageIO stage_io;
    
    //ubo/object在这里以及MaterialDescriptorManager中均有一份，mdm中的用于产生set/binding号，这里的用于产生shader
    ConstUBODescriptorList              ubo_list;
    ConstSamplerDescriptorList          sampler_list;
    
    ObjectList<ConstValueDescriptor>    const_value_list;
    ObjectList<SubpassInputDescriptor>  subpass_input;
    
    ShaderPushConstant                  push_constant;

    AnsiString                          source_codes;

public:

    ShaderDescriptorManager(VkShaderStageFlagBits);
    ~ShaderDescriptorManager()=default;

    const VkShaderStageFlagBits GetStageBits()const { return stage_io.cur; }
    const AnsiString            GetStageName()const { return AnsiString(GetShaderStageName(stage_io.cur)); }
   
    void SetPrevShader(VkShaderStageFlagBits prev) { stage_io.prev=prev; }
    void SetNextShader(VkShaderStageFlagBits next) { stage_io.next=next; }

public:

    const ShaderStageIO &                       GetShaderStageIO()const{return stage_io;}

    const ConstUBODescriptorList &              GetUBOList()const{return ubo_list;}
    const ConstSamplerDescriptorList &          GetSamplerList()const{return sampler_list;}

    const ObjectList<ConstValueDescriptor> &    GetConstList()const{return const_value_list;}

    const ObjectList<SubpassInputDescriptor> &  GetSubpassInputList()const { return subpass_input; }

    const AnsiString &GetSources()const{return source_codes;}
    
public:

    bool AddInput(ShaderStage *);
    bool AddOutput(ShaderStage *);

    bool AddUBO(DescriptorSetType type,const UBODescriptor *sd);
    bool AddSampler(DescriptorSetType type,const SamplerDescriptor *sd);

    bool AddConstValue(ConstValueDescriptor *sd);    
    bool AddSubpassInput(const AnsiString name,uint8_t index);
    
    void SetPushConstant(const AnsiString name,uint8_t offset,uint8_t size);

    void AddSource(const AnsiString &str)
    {
        source_codes+=str;
        source_codes+="\n";
    }

#ifdef _DEBUG
    void DebugOutput(int);
#endif//_DEBUG
};//class ShaderDescriptorManager
SHADERGEN_NAMESPACE_END

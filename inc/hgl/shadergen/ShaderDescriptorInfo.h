#pragma once

#include<hgl/type/Map.h>
#include<hgl/type/StringList.h>
#include<hgl/graph/VK.h>
#include<hgl/graph/VKDescriptorSetType.h>
#include<hgl/graph/mtl/ShaderVariableType.h>
#include<hgl/shadergen/MaterialDescriptorInfo.h>

namespace hgl{namespace graph
{
using UBODescriptorList=ArrayList<const UBODescriptor *>;
using SamplerDescriptorList=ArrayList<const SamplerDescriptor *>;
using ConstValueDescriptorList=ObjectList<ConstValueDescriptor>;
using SubpassInputDescriptorList=ObjectList<SubpassInputDescriptor>;

/**
* Shader数据管理器,用于生成正式Shader前的资源统计
*/
class ShaderDescriptorInfo
{
protected:

    ShaderStage                         stage_flag;

    AnsiStringList                      struct_list;        //用到的结构列表

    //ubo/object在这里以及MaterialDescriptorInfo中均有一份，mdi中的用于产生set/binding号，这里的用于产生shader
    UBODescriptorList                   ubo_list;
    SamplerDescriptorList               sampler_list;
    
    ConstValueDescriptorList            const_value_list;
    
    ShaderPushConstant                  push_constant;

public:

    ShaderDescriptorInfo(ShaderStage);
    virtual ~ShaderDescriptorInfo()=default;

    const ShaderStage                   GetShaderStage()const { return stage_flag; }
    const VkShaderStageFlagBits         GetVkShaderStage()const { return (VkShaderStageFlagBits)stage_flag; }
    const AnsiString                    GetStageName()const { return AnsiString(GetShaderStageName((VkShaderStageFlagBits)stage_flag)); }

public:

    const AnsiStringList &              GetStructList()const{return struct_list;}

    const UBODescriptorList &           GetUBOList()const{return ubo_list;}
    const SamplerDescriptorList &       GetSamplerList()const{return sampler_list;}

    const ConstValueDescriptorList &    GetConstList()const{return const_value_list;}

public:

    void AddStruct(const AnsiString &);
    bool AddUBO(DescriptorSetType type,const UBODescriptor *sd);
    bool AddSampler(DescriptorSetType type,const SamplerDescriptor *sd);

    bool AddConstValue(ConstValueDescriptor *sd);
    
    void SetPushConstant(const AnsiString &name,uint8_t offset,uint8_t size);
};//class ShaderDescriptorInfo

template<ShaderStage SS,typename IArray,typename I,typename OArray,typename O> class CustomShaderDescriptorInfo:public ShaderDescriptorInfo
{
    IArray input;
    OArray output;

public:

    CustomShaderDescriptorInfo():ShaderDescriptorInfo(SS){}
    virtual ~CustomShaderDescriptorInfo()override=default;

    bool AddInput(I &item){return input.Add(item);}
    bool AddOutput(O &item){return output.Add(item);}

    bool hasInput(const char *name)const{return input.Contains(name);}     ///<是否有指定输入

public:

    IArray &GetInput(){return input;}
    OArray &GetOutput(){return output;}

    const bool IsEmptyInput()const{return input.IsEmpty();}
    const bool IsEmptyOutput()const{return output.IsEmpty();}
};//class CustomShaderDescriptorInfo

class VertexShaderDescriptorInfo:public CustomShaderDescriptorInfo<ShaderStage::Vertex,VIAArray,VIA,SVArray,ShaderVariable  >
{
    SubpassInputDescriptorList          subpass_input;

public:

    const SubpassInputDescriptorList &  GetSubpassInputList()const{return subpass_input;}

public:

    using CustomShaderDescriptorInfo<ShaderStage::Vertex,VIAArray,VIA,SVArray,ShaderVariable>::CustomShaderDescriptorInfo;
    ~VertexShaderDescriptorInfo()override=default;

    bool AddSubpassInput(const AnsiString &name,uint8_t index);
};//class VertexShaderDescriptorInfo

using TessCtrlShaderDescriptorInfo=CustomShaderDescriptorInfo<ShaderStage::TessellationControl,     SVArray,  ShaderVariable,   SVArray,    ShaderVariable  >;
using TessEvalShaderDescriptorInfo=CustomShaderDescriptorInfo<ShaderStage::TessellationEvaluation,  SVArray,  ShaderVariable,   SVArray,    ShaderVariable  >;
using GeometryShaderDescriptorInfo=CustomShaderDescriptorInfo<ShaderStage::Geometry,                 SVArray,  ShaderVariable,   SVArray,    ShaderVariable  >;
using FragmentShaderDescriptorInfo=CustomShaderDescriptorInfo<ShaderStage::Fragment,                 SVArray,  ShaderVariable,   VIAArray,   VIA             >;

}}//namespace hgl::graph

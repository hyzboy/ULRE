#include <string>
#include <vector>
#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKVertexInputAttribute.h>
#include<hgl/vk/VKDescriptorSetType.h>
#include<hgl/graph/mtl/ShaderVariableType.h>
#include<hgl/shadergen/MaterialDescriptorInfo.h>

namespace hgl{namespace graph
{
using UBODescriptorList=ValueArray<const UBODescriptor *>;
using SSBODescriptorList=ValueArray<const SSBODescriptor *>;
using TextureDescriptorList = ValueArray<const TextureDescriptor *>;
using TextureSamplerDescriptorList=ValueArray<const TextureSamplerDescriptor *>;
using ConstValueDescriptorList=ManagedArray<ConstValueDescriptor>;
using SubpassInputDescriptorList=ManagedArray<SubpassInputDescriptor>;

/**
* Shader数据管理器,用于生成正式Shader前的资源统计
*/
class ShaderDescriptorInfo
{
protected:

    ShaderStage                         stage_flag;

    std::vector<std::string>                      struct_list;        //用到的结构列表

    //ubo/object在这里以及MaterialDescriptorInfo中均有一份，mdi中的用于产生set/binding号，这里的用于产生shader
    UBODescriptorList                   ubo_list;
    SSBODescriptorList                  ssbo_list;
    TextureDescriptorList               texture_list;
    TextureSamplerDescriptorList        texture_sampler_list;

    ConstValueDescriptorList            const_value_list;

    ShaderPushConstant                  push_constant;

public:

    ShaderDescriptorInfo(ShaderStage);
    virtual ~ShaderDescriptorInfo()=default;

    const ShaderStage                   GetShaderStage()const { return stage_flag; }
    const VkShaderStageFlagBits         GetVkShaderStage()const { return (VkShaderStageFlagBits)stage_flag; }
    const std::string                    GetStageName()const { return std::string(GetShaderStageName((VkShaderStageFlagBits)stage_flag)); }

public:

    const std::vector<std::string> &              GetStructList()const{return struct_list;}

    const UBODescriptorList &           GetUBOList()const{return ubo_list;}
    const SSBODescriptorList &          GetSSBOList()const{return ssbo_list;}
    const TextureSamplerDescriptorList &GetTextureSamplerList()const{return texture_sampler_list;}

    const ConstValueDescriptorList &    GetConstList()const{return const_value_list;}

public:

    void AddStruct(const std::string &);
    bool AddUBO(DescriptorSetType type,const UBODescriptor *sd);
    bool AddSSBO(DescriptorSetType type,const SSBODescriptor *sd);
    bool AddTexture(DescriptorSetType type,const TextureDescriptor *sd);
    bool AddTextureSampler(DescriptorSetType type,const TextureSamplerDescriptor *sd);

    bool AddConstValue(ConstValueDescriptor *sd);

    void SetPushConstant(const std::string &name,uint8_t offset,uint8_t size);
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

    const bool IsEmptyInput()const{return input.empty();}
    const bool IsEmptyOutput()const{return output.empty();}
};//class CustomShaderDescriptorInfo

class VertexShaderDescriptorInfo:public CustomShaderDescriptorInfo<ShaderStage::Vertex,VIAArray,VIA,SVArray,ShaderVariable  >
{
    SubpassInputDescriptorList          subpass_input;

public:

    const SubpassInputDescriptorList &  GetSubpassInputList()const{return subpass_input;}

public:

    using CustomShaderDescriptorInfo<ShaderStage::Vertex,VIAArray,VIA,SVArray,ShaderVariable>::CustomShaderDescriptorInfo;
    ~VertexShaderDescriptorInfo()override=default;

    bool AddSubpassInput(const std::string &name,uint8_t index);
};//class VertexShaderDescriptorInfo

using TessCtrlShaderDescriptorInfo=CustomShaderDescriptorInfo<ShaderStage::TessControl, SVArray,  ShaderVariable,   SVArray,    ShaderVariable  >;
using TessEvalShaderDescriptorInfo=CustomShaderDescriptorInfo<ShaderStage::TessEval,    SVArray,  ShaderVariable,   SVArray,    ShaderVariable  >;
using GeometryShaderDescriptorInfo=CustomShaderDescriptorInfo<ShaderStage::Geometry,    SVArray,  ShaderVariable,   SVArray,    ShaderVariable  >;
using FragmentShaderDescriptorInfo=CustomShaderDescriptorInfo<ShaderStage::Fragment,    SVArray,  ShaderVariable,   VIAArray,   VIA             >;
using ComputeShaderDescriptorInfo =CustomShaderDescriptorInfo<ShaderStage::Compute,     SVArray,  ShaderVariable,   SVArray,    ShaderVariable  >;

}}//namespace hgl::graph

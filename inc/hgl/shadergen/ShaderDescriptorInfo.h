#pragma once

#include <hgl/common/ShaderStageDef.h>
#include <hgl/common/ShaderDescriptorDef.h>
#include <hgl/common/VertexInputDef.h>
#include <hgl/common/DescriptorSetTypeDef.h>
#include<hgl/mtl/ShaderVariableType.h>
#include<ankerl/unordered_dense.h>
#include<vector>
#include<string>

namespace hgl{namespace graph
{
using UBODescriptorList=std::vector<const UBODescriptor *>;
using SSBODescriptorList=std::vector<const SSBODescriptor *>;
using TextureDescriptorList = std::vector<const TextureDescriptor *>;
using TextureSamplerDescriptorList=std::vector<const TextureSamplerDescriptor *>;
using ConstValueDescriptorList=std::vector<ConstValueDescriptor *>;

/**
* Shader数据管理器,用于生成正式Shader前的资源统计
*/
class ShaderDescriptorInfo
{
protected:

    ShaderStage                         stage_flag;

    //ubo/object在这里以及MaterialDescriptorInfo中均有一份，mdi中的用于产生set/binding号，这里的用于产生shader
    UBODescriptorList                   ubo_list;
    SSBODescriptorList                  ssbo_list;
    TextureDescriptorList               texture_list;
    TextureSamplerDescriptorList        texture_sampler_list;

    ConstValueDescriptorList            const_value_list;

    ShaderPushConstant                  push_constant;

public:

    ShaderDescriptorInfo(ShaderStage);
    virtual ~ShaderDescriptorInfo();

    const ShaderStage                   GetShaderStage()const { return stage_flag; }
    std::string                         GetStageName()const;

public:

    const UBODescriptorList &           GetUBOList()const{return ubo_list;}
    const SSBODescriptorList &          GetSSBOList()const{return ssbo_list;}
    const TextureDescriptorList &       GetTextureList()const{return texture_list;}
    const TextureSamplerDescriptorList &GetTextureSamplerList()const{return texture_sampler_list;}

    const ConstValueDescriptorList &    GetConstList()const{return const_value_list;}

public:

    bool AddUBO(DescriptorSetType type,const UBODescriptor *sd);
    bool AddSSBO(DescriptorSetType type,const SSBODescriptor *sd);
    bool AddTexture(DescriptorSetType type,const TextureDescriptor *sd);
    bool AddTextureSampler(DescriptorSetType type,const TextureSamplerDescriptor *sd);

    bool AddConstValue(ConstValueDescriptor *sd);

    void SetPushConstant(const std::string &name,uint8_t offset,uint8_t size);
    void SetPushConstant(const char *name,uint8_t offset,uint8_t size)
    {
        SetPushConstant(std::string(name?name:""),offset,size);
    }
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
    const IArray &GetInput()const{return input;}
    OArray &GetOutput(){return output;}

    const bool IsEmptyInput()const{return input.IsEmpty();}
    const bool IsEmptyOutput()const{return output.IsEmpty();}
};//class CustomShaderDescriptorInfo

using VertexShaderDescriptorInfo  =CustomShaderDescriptorInfo<ShaderStage::Vertex,      VIAArray, VIA,              SVArray,    ShaderVariable  >;
using FragmentShaderDescriptorInfo=CustomShaderDescriptorInfo<ShaderStage::Fragment,    SVArray,  ShaderVariable,   VIAArray,   VIA             >;
using ComputeShaderDescriptorInfo =CustomShaderDescriptorInfo<ShaderStage::Compute,     SVArray,  ShaderVariable,   SVArray,    ShaderVariable  >;

}}//namespace hgl::graph

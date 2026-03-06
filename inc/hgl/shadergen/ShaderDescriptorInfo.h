#pragma once

#include <hgl/graph/shared/ShaderStageDef.h>
#include <hgl/graph/shared/ShaderDescriptorDef.h>
#include <hgl/graph/shared/VertexInputDef.h>
#include <hgl/graph/shared/DescriptorSetTypeDef.h>
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
using SubpassInputDescriptorList=std::vector<SubpassInputDescriptor *>;

/**
* Shader数据管理器,用于生成正式Shader前的资源统计
*/
class ShaderDescriptorInfo
{
protected:

    ShaderStage                         stage_flag;

    ankerl::unordered_dense::set<std::string> struct_list;  //用到的结构列表(去重)

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

    const ankerl::unordered_dense::set<std::string> &GetStructList()const{return struct_list;}
    std::vector<std::string>            GetStructNameList()const
    {
        std::vector<std::string> names;
        names.reserve(struct_list.size());

        for(const auto &name:struct_list)
            names.emplace_back(name);

        return names;
    }

    const UBODescriptorList &           GetUBOList()const{return ubo_list;}
    const SSBODescriptorList &          GetSSBOList()const{return ssbo_list;}
    const TextureSamplerDescriptorList &GetTextureSamplerList()const{return texture_sampler_list;}

    const ConstValueDescriptorList &    GetConstList()const{return const_value_list;}

public:

    void AddStruct(const std::string &name){struct_list.emplace(name);}
    void AddStruct(const char *name){struct_list.emplace(name?name:"");}
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
    ~VertexShaderDescriptorInfo()override;

    bool AddSubpassInput(const std::string &name,uint8_t index);
    bool AddSubpassInput(const char *name,uint8_t index)
    {
        return AddSubpassInput(std::string(name?name:""),index);
    }
};//class VertexShaderDescriptorInfo

using TessCtrlShaderDescriptorInfo=CustomShaderDescriptorInfo<ShaderStage::TessControl, SVArray,  ShaderVariable,   SVArray,    ShaderVariable  >;
using TessEvalShaderDescriptorInfo=CustomShaderDescriptorInfo<ShaderStage::TessEval,    SVArray,  ShaderVariable,   SVArray,    ShaderVariable  >;
using GeometryShaderDescriptorInfo=CustomShaderDescriptorInfo<ShaderStage::Geometry,    SVArray,  ShaderVariable,   SVArray,    ShaderVariable  >;
using FragmentShaderDescriptorInfo=CustomShaderDescriptorInfo<ShaderStage::Fragment,    SVArray,  ShaderVariable,   VIAArray,   VIA             >;
using ComputeShaderDescriptorInfo =CustomShaderDescriptorInfo<ShaderStage::Compute,     SVArray,  ShaderVariable,   SVArray,    ShaderVariable  >;

}}//namespace hgl::graph

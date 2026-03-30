#pragma once

#include <hgl/common/ShaderStageDef.h>
#include <hgl/common/ShaderDescriptorDef.h>
#include <hgl/common/VertexInputDef.h>
#include<hgl/mtl/ShaderVariableType.h>
#include<ankerl/unordered_dense.h>
#include<vector>
#include<string>

namespace hgl{namespace graph
{
using ConstValueDescriptorList=std::vector<ConstValueDescriptor *>;

/**
* Shader数据管理器,用于生成正式Shader前的资源统计
*/
class ShaderStageIO
{
protected:

    ShaderStage                         stage_flag;

    ConstValueDescriptorList            const_value_list;

    ShaderPushConstant                  push_constant;

public:

    ShaderStageIO(ShaderStage);
    virtual ~ShaderStageIO();

    const ShaderStage                   GetShaderStage()const { return stage_flag; }
    std::string                         GetStageName()const;

public:

    const ConstValueDescriptorList &    GetConstList()const{return const_value_list;}

public:

    bool AddConstValue(ConstValueDescriptor *sd);

    void SetPushConstant(const std::string &name,uint8_t offset,uint8_t size);
    void SetPushConstant(const char *name,uint8_t offset,uint8_t size)
    {
        SetPushConstant(std::string(name?name:""),offset,size);
    }
};//class ShaderStageIO

template<ShaderStage SS,typename IArray,typename I,typename OArray,typename O> class CustomShaderDescriptorInfo:public ShaderStageIO
{
    IArray input;
    OArray output;

public:

    CustomShaderDescriptorInfo():ShaderStageIO(SS){}
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

using VertexShaderStageIO  =CustomShaderDescriptorInfo<ShaderStage::Vertex,      VIAArray, VIA,              SVArray,    ShaderVariable  >;
using FragmentShaderStageIO=CustomShaderDescriptorInfo<ShaderStage::Fragment,    SVArray,  ShaderVariable,   VIAArray,   VIA             >;

// Compute Shader 还没有启用，这里只是预留一个类型别名
using ComputeShaderDescriptorInfo =CustomShaderDescriptorInfo<ShaderStage::Compute,     SVArray,  ShaderVariable,   SVArray,    ShaderVariable  >;

}}//namespace hgl::graph

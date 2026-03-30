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

class VertexShaderStageIO : public ShaderStageIO
{
    VIAArray input;
    SVArray  output;

public:

    VertexShaderStageIO() : ShaderStageIO(ShaderStage::Vertex) {}
    virtual ~VertexShaderStageIO() override = default;

    bool AddInput(VIA &item)            { return input.Add(item); }
    bool AddOutput(ShaderVariable &item){ return output.Add(item); }

    bool hasInput(const VertexAttrib attrib) const { return input.Contains(attrib); }  ///<是否有指定输入
    bool hasInput(const char *name) const
    {
        const VertexAttrib attrib = GetVertexAttribByName(name);
        return (attrib != VertexAttrib::RANGE_SIZE) ? input.Contains(attrib) : false;
    }

public:

    VIAArray       &GetInput()       { return input; }
    const VIAArray &GetInput() const { return input; }
    SVArray        &GetOutput()      { return output; }

    bool IsEmptyInput()  const { return input.count == 0; }
    bool IsEmptyOutput() const { return output.IsEmpty(); }
};//class VertexShaderStageIO

class FragmentShaderStageIO : public ShaderStageIO
{
    SVArray  input;
    VIAArray output;

public:

    FragmentShaderStageIO() : ShaderStageIO(ShaderStage::Fragment) {}
    virtual ~FragmentShaderStageIO() override = default;

    bool AddInput(ShaderVariable &item){ return input.Add(item); }
    bool AddOutput(VIA &item)          { return output.Add(item); }

    bool hasInput(const char *name) const { return input.Contains(name); }  ///<是否有指定输入

public:

    SVArray        &GetInput()       { return input; }
    const SVArray  &GetInput() const { return input; }
    VIAArray       &GetOutput()      { return output; }

    bool IsEmptyInput()  const { return input.IsEmpty(); }
    bool IsEmptyOutput() const { return output.count == 0; }
};//class FragmentShaderStageIO

// Compute Shader 还没有启用，暂时注释掉
// class ComputeShaderStageIO : public ShaderStageIO
// {
//     SVArray input;
//     SVArray output;
// public:
//     ComputeShaderStageIO() : ShaderStageIO(ShaderStage::Compute) {}
//     virtual ~ComputeShaderStageIO() override = default;
//     bool AddInput(ShaderVariable &item) { return input.Add(item); }
//     bool AddOutput(ShaderVariable &item){ return output.Add(item); }
//     SVArray       &GetInput()       { return input; }
//     const SVArray &GetInput() const { return input; }
//     SVArray       &GetOutput()      { return output; }
//     bool IsEmptyInput()  const { return input.IsEmpty(); }
//     bool IsEmptyOutput() const { return output.IsEmpty(); }
// };//class ComputeShaderStageIO

}}//namespace hgl::graph

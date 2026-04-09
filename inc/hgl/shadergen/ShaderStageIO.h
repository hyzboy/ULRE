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
    std::vector<VIA> input;

public:

    VertexShaderStageIO() : ShaderStageIO(ShaderStage::Vertex) {}
    virtual ~VertexShaderStageIO() override = default;

    bool AddInput(VIA &item)
    {
        for(const auto &existing:input)
        {
            if(existing.attrib==item.attrib)
                return false;
        }

        item.location=static_cast<uint8>(input.size());
        input.push_back(item);
        return true;
    }

public:

    std::vector<VIA>       &GetInput()       { return input; }
    const std::vector<VIA> &GetInput() const { return input; }
};//class VertexShaderStageIO

class FragmentShaderStageIO : public ShaderStageIO
{
public:

    FragmentShaderStageIO() : ShaderStageIO(ShaderStage::Fragment) {}
    virtual ~FragmentShaderStageIO() override = default;
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

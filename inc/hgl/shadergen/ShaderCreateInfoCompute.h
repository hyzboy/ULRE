#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl::graph
{
    /**
     * Compute Shader创建信息类
     * Compute Shader不需要传统的输入输出，主要通过UBO/SSBO/Image进行数据交互
     */
    class ShaderCreateInfoCompute:public ShaderCreateInfo
    {
        ComputeShaderDescriptorInfo csdi;

    protected:

        bool ProcLayout() override;

        bool IsEmptyOutput()const override{return true;}  // Compute shader没有传统输出

        void AddMaterialInstanceOutput() override{};  // Compute shader不需要材质实例输出

    public:

        ShaderDescriptorInfo *GetSDI()override{return &csdi;}

    public:

        ShaderCreateInfoCompute(MaterialDescriptorInfo *m):ShaderCreateInfo(){ShaderCreateInfo::Init(&csdi,m);}
        ~ShaderCreateInfoCompute()override=default;

        /**
         * 设置工作组大小
         * @param x X维度的工作组大小
         * @param y Y维度的工作组大小
         * @param z Z维度的工作组大小
         */
        void SetWorkGroupSize(uint32 x, uint32 y, uint32 z);
    };//class ShaderCreateInfoCompute:public ShaderCreateInfo
}//namespace hgl::graph

#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl::graph
{
    /**
     * Compute Shader creation info class
     * Compute Shader does not require traditional input/output, mainly exchanges data through UBO/SSBO/Image
     */
    class ShaderCreateInfoCompute:public ShaderCreateInfo
    {
        ComputeShaderDescriptorInfo csdi;

    protected:

        bool ProcLayout() override;
        
        bool IsEmptyOutput()const override{return true;}  // Compute shader has no traditional output
        
        void AddMaterialInstanceOutput() override{};  // Compute shader does not need material instance output

    public:

        ShaderDescriptorInfo *GetSDI()override{return &csdi;}

    public:

        ShaderCreateInfoCompute(MaterialDescriptorInfo *m):ShaderCreateInfo(){ShaderCreateInfo::Init(&csdi,m);}
        ~ShaderCreateInfoCompute()override=default;

        /**
         * Set work group size
         * @param x Work group size in X dimension
         * @param y Work group size in Y dimension
         * @param z Work group size in Z dimension
         */
        void SetWorkGroupSize(uint32 x, uint32 y, uint32 z);
    };//class ShaderCreateInfoCompute:public ShaderCreateInfo
}//namespace hgl::graph

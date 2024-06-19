#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl
{
    namespace graph
    {
        class ShaderCreateInfoVertex:public ShaderCreateInfo
        {
            VertexShaderDescriptorInfo vsdi;

            bool ProcSubpassInput();
            bool ProcInput(ShaderCreateInfo *) override;

        public:

            ShaderDescriptorInfo *GetSDI()override{return &vsdi;}

        public:

            ShaderCreateInfoVertex(MaterialDescriptorInfo *m):ShaderCreateInfo(){ShaderCreateInfo::Init(&vsdi,m);}
            ~ShaderCreateInfoVertex()override=default;
            
            int AddInput(const graph::VAType &type,const AnsiString &name,const VkVertexInputRate input_rate=VK_VERTEX_INPUT_RATE_VERTEX,const VertexInputGroup &group=VertexInputGroup::Basic);
            int AddInput(const AnsiString &type,const AnsiString &name,const VkVertexInputRate input_rate=VK_VERTEX_INPUT_RATE_VERTEX,const VertexInputGroup &group=VertexInputGroup::Basic);

            int hasInput(const char *);
    
            int AddOutput(const SVType &type,const AnsiString &name,Interpolation inter=Interpolation::Smooth);
            void AddMaterialInstanceOutput() override;

            void AddAssign();

            void AddJoint();
        };//class ShaderCreateInfoVertex:public ShaderCreateInfo
    }//namespace graph
}//namespace hgl::graph

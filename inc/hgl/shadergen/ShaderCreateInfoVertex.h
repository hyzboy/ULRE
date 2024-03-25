#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/graph/VKShaderStage.h>

namespace hgl
{
    namespace graph
    {
        class ShaderCreateInfoVertex:public ShaderCreateInfo
        {
            bool ProcInput(ShaderCreateInfo *) override;

        public:

            ShaderCreateInfoVertex(MaterialDescriptorInfo *);
            ~ShaderCreateInfoVertex()=default;

            int AddInput(const graph::VAT &type,const AnsiString &name,const VkVertexInputRate input_rate=VK_VERTEX_INPUT_RATE_VERTEX,const VertexInputGroup &group=VertexInputGroup::Basic);
            int AddInput(const AnsiString &type,const AnsiString &name,const VkVertexInputRate input_rate=VK_VERTEX_INPUT_RATE_VERTEX,const VertexInputGroup &group=VertexInputGroup::Basic);

            int hasInput(const char *);

            void AddLocalToWorld();
            void AddMaterialInstanceID();
            void AddJoint();
        };//class ShaderCreateInfoVertex:public ShaderCreateInfo
    }//namespace graph
}//namespace hgl::graph

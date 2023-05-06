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

            ShaderCreateInfoVertex(MaterialDescriptorInfo *m):ShaderCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,m){}
            ~ShaderCreateInfoVertex()=default;

            int AddInput(const graph::VAT &type,const AnsiString &name,const VkVertexInputRate input_rate=VK_VERTEX_INPUT_RATE_VERTEX,const VertexInputGroup &group=VertexInputGroup::Basic);
            int AddInput(const AnsiString &type,const AnsiString &name,const VkVertexInputRate input_rate=VK_VERTEX_INPUT_RATE_VERTEX,const VertexInputGroup &group=VertexInputGroup::Basic);

            void AddBone()
            {
                VAT id      {VertexAttribType::BaseType::UInt,4};
                VAT weight  {VertexAttribType::BaseType::Float,4};

                AddInput(id,    VAN::BoneID,    VK_VERTEX_INPUT_RATE_VERTEX,VertexInputGroup::Bone);
                AddInput(weight,VAN::BoneWeight,VK_VERTEX_INPUT_RATE_VERTEX,VertexInputGroup::Bone);
            }

            void AddLocalToWorld()
            {
                VAT vat{VertexAttribType::BaseType::Float,4};

                char name[]= "LocalToWorld_?";

                for(uint i=0;i<4;i++)
                {
                    name[sizeof(name)-2]='0'+i;

                    AddInput(vat,name,VK_VERTEX_INPUT_RATE_INSTANCE,VertexInputGroup::LocalToWorld);
                }
            }
        };//class ShaderCreateInfoVertex:public ShaderCreateInfo
    }//namespace graph
}//namespace hgl::graph

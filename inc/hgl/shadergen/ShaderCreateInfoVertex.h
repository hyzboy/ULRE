﻿#pragma once

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

            void AddMaterialID()
            {
                constexpr const VAT vat{VertexAttribType::BaseType::UInt,1};        //使用uint8

                AddInput(vat,VAN::MaterialInstanceID,VK_VERTEX_INPUT_RATE_VERTEX,VertexInputGroup::MaterialInstanceID);
            }

            void AddJoint()
            {
                constexpr const VAT id      {VertexAttribType::BaseType::UInt,4};   //使用uint8[4]
                constexpr const VAT weight  {VertexAttribType::BaseType::Float,4};  //使用uint8[4]

                AddInput(id,    VAN::JointID,    VK_VERTEX_INPUT_RATE_VERTEX,VertexInputGroup::JointID);
                AddInput(weight,VAN::JointWeight,VK_VERTEX_INPUT_RATE_VERTEX,VertexInputGroup::JointWeight);
            }

            void AddLocalToWorld()
            {
                constexpr const VAT vat{VertexAttribType::BaseType::Float,4};       //使用float[4]

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

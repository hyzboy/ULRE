#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl::graph
{
    class ShaderCreateInfoVertex:public ShaderCreateInfo
    {
        VertexShaderDescriptorInfo vsdi;

        bool ProcSubpassInput();
        bool ProcInput(ShaderCreateInfo *) override;

        bool IsEmptyOutput()const override{return vsdi.IsEmptyOutput();}
        void GetOutputStrcutString(std::string &str) override;

    public:

        VIAArray &GetInput(){return vsdi.GetInput();}

        ShaderDescriptorInfo *GetSDI()override{return &vsdi;}

    public:

        ShaderCreateInfoVertex(MaterialDescriptorInfo *m):ShaderCreateInfo(){ShaderCreateInfo::Init(&vsdi,m);}
        ~ShaderCreateInfoVertex()override=default;

        int AddInput(VIAList &);
        int AddInput(const VAType &type,const std::string &name,const VkVertexInputRate input_rate=VK_VERTEX_INPUT_RATE_VERTEX,const VertexInputGroup &group=VertexInputGroup::Basic);
        int AddInput(const char *type,const std::string &name,const VkVertexInputRate input_rate=VK_VERTEX_INPUT_RATE_VERTEX,const VertexInputGroup &group=VertexInputGroup::Basic);
        int AddInput(const VAType &type,const char *name,const VkVertexInputRate input_rate=VK_VERTEX_INPUT_RATE_VERTEX,const VertexInputGroup &group=VertexInputGroup::Basic)
        {
            return AddInput(type,std::string(name?name:""),input_rate,group);
        }

        int hasInput(const char *);

        int AddOutput(SVList &);
        int AddOutput(const SVType &type,const std::string &name,Interpolation inter=Interpolation::Smooth);
        int AddOutput(const SVType &type,const char *name,Interpolation inter=Interpolation::Smooth)
        {
            return AddOutput(type,std::string(name?name:""),inter);
        }
        void AddMaterialInstanceOutput() override;

        void AddAssignTransform();
        void AddAssignMaterialInstance();

        void AddJoint();
    };//class ShaderCreateInfoVertex:public ShaderCreateInfo
}//namespace hgl::graph

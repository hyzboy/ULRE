#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<hgl/graph/VertexAttrib.h>
#include<hgl/graph/VKShaderStage.h>
#include<hgl/graph/VKRenderAssign.h>
#include"GLSLCompiler.h"
#include"common/MFCommon.h"
#include"ShaderLibrary.h"

VK_NAMESPACE_BEGIN
ShaderCreateInfoVertex::ShaderCreateInfoVertex(MaterialDescriptorInfo *m):ShaderCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,m)
{
}

int ShaderCreateInfoVertex::AddInput(const VAT &type,const AnsiString &name,const VkVertexInputRate input_rate,const VertexInputGroup &group)
{
    ShaderAttribute *ss=new ShaderAttribute;

    hgl::strcpy(ss->name,sizeof(ss->name),name.c_str());

    ss->basetype=(uint8) type.basetype;
    ss->vec_size=        type.vec_size;

    ss->input_rate      =input_rate;
    ss->group           =group;

    return sdm->AddInput(ss);
}

int ShaderCreateInfoVertex::AddInput(const AnsiString &type,const AnsiString &name,const VkVertexInputRate input_rate,const VertexInputGroup &group)
{
    VAT vat;

    if(!ParseVertexAttribType(&vat,type))
        return(-2);

    return AddInput(vat,name,input_rate,group);
}

void ShaderCreateInfoVertex::AddJoint()
{
    AddInput(VAT_UVEC4, VAN::JointID,    VK_VERTEX_INPUT_RATE_VERTEX,VertexInputGroup::JointID);
    AddInput(VAT_VEC4,  VAN::JointWeight,VK_VERTEX_INPUT_RATE_VERTEX,VertexInputGroup::JointWeight);
}

namespace
{
    constexpr const char MF_GetLocalToWorld[]="\nmat4 GetLocalToWorld(){return l2w.mats[Assign.x];}\n";
}

void ShaderCreateInfoVertex::AddAssign()
{
    AddInput(   ASSIGN_VAT_FMT,
                ASSIGN_VIS_NAME,
                VK_VERTEX_INPUT_RATE_INSTANCE,
                VertexInputGroup::Assign);

    AddFunction(MF_GetLocalToWorld);
}

bool ShaderCreateInfoVertex::ProcInput(ShaderCreateInfo *)
{    
    const auto &input=sdm->GetShaderStageIO().input;

    if(input.count<=0)
    {
        //no input ? this isn't a bug.
        //maybe position info from UBO/SBBO/Texture.
        return(true);
    }

    final_shader+="\n";

    const ShaderAttribute *ss=input.items;

    for(uint i=0;i<input.count;i++)
    {
        final_shader+="layout(location=";
        final_shader+=AnsiString::numberOf(ss->location);
        final_shader+=") in ";
        final_shader+=GetShaderAttributeTypename(ss);
        final_shader+=" "+AnsiString(ss->name);
        final_shader+=";\n";

        ++ss;
    }

    return(true);
}
VK_NAMESPACE_END

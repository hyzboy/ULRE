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

int ShaderCreateInfoVertex::hasInput(const char *name)
{
    return sdm->hasInput(name);
}

void ShaderCreateInfoVertex::AddJoint()
{
    AddInput(VAT_UVEC4, VAN::JointID,    VK_VERTEX_INPUT_RATE_VERTEX,VertexInputGroup::JointID);
    AddInput(VAT_VEC4,  VAN::JointWeight,VK_VERTEX_INPUT_RATE_VERTEX,VertexInputGroup::JointWeight);
}

namespace
{
    constexpr const char MF_GetLocalToWorld_ByID[]="\nmat4 GetLocalToWorld(){return l2w.mats[LocalToWorld_ID];}\n";

    constexpr const char MF_GetLocalToWorld_by4VI[]=R"(
mat4 GetLocalToWorld()
{
    return mat4(LocalToWorld_0,
                LocalToWorld_1,
                LocalToWorld_2,
                LocalToWorld_3);
}
)";
}

void ShaderCreateInfoVertex::AddLocalToWorld()
{
    char name[]= "LocalToWorld_?";

    for(uint i=0;i<4;i++)
    {
        name[sizeof(name)-2]='0'+i;

        AddInput(VAT_VEC4,name,VK_VERTEX_INPUT_RATE_INSTANCE,VertexInputGroup::LocalToWorld);
    }

    AddFunction(MF_GetLocalToWorld_by4VI);
}

void ShaderCreateInfoVertex::AddMaterialInstanceID()
{
    AddInput(   MI_VAT_FMT,
                MI_VIS_NAME,
                VK_VERTEX_INPUT_RATE_INSTANCE,
                VertexInputGroup::MaterialInstanceID);
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

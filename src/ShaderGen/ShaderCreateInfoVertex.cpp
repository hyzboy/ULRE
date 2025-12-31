#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/graph/VertexAttrib.h>
#include<hgl/graph/VKVertexInputAttribute.h>
#include<hgl/graph/VKRenderAssign.h>
#include"GLSLCompiler.h"
#include"common/MFCommon.h"
#include"ShaderLibrary.h"

VK_NAMESPACE_BEGIN

void ShaderCreateInfoVertex::AddMaterialInstanceOutput()
{
    AddOutput(SVT_UINT,mtl::func::MI_ID_OUTPUT,Interpolation::Flat);
    AddFunction(mtl::func::MF_HandoverMI_VS);
}

int ShaderCreateInfoVertex::AddInput(VIAList &via_list)
{
    int count=0;

    for(VIA &via:via_list)
    {
        via.input_rate=VK_VERTEX_INPUT_RATE_VERTEX;
        via.group=VertexInputGroup::Basic;

        if(vsdi.AddInput(via))
            ++count;
    }

    return count;
}

int ShaderCreateInfoVertex::AddInput(const VAType &type,const AnsiString &name,const VkVertexInputRate input_rate,const VertexInputGroup &group)
{
    VIA via;

    hgl::strcpy(via.name,sizeof(via.name),name.c_str());

    via.basetype=(uint8) type.basetype;
    via.vec_size=        type.vec_size;

    via.input_rate      =input_rate;
    via.group           =group;

    via.interpolation   =Interpolation::Smooth;

    return vsdi.AddInput(via);
}

int ShaderCreateInfoVertex::AddInput(const AnsiString &type,const AnsiString &name,const VkVertexInputRate input_rate,const VertexInputGroup &group)
{
    VAType vat;

    if(!ParseVertexAttribType(&vat,type))
        return(-2);

    return AddInput(vat,name,input_rate,group);
}

int ShaderCreateInfoVertex::hasInput(const char *name)
{
    return vsdi.hasInput(name);
}

int ShaderCreateInfoVertex::AddOutput(SVList &sv_list)
{
    int count=0;

    for(ShaderVariable &sv:sv_list)
    {
        sv.interpolation=Interpolation::Smooth;

        if(vsdi.AddOutput(sv))
            ++count;
    }

    return count;
}

int ShaderCreateInfoVertex::AddOutput(const SVType &type,const AnsiString &name,Interpolation inter)
{
    ShaderVariable sv;
    
    hgl::strcpy(sv.name,sizeof(sv.name),name.c_str());

    sv.type=type;
    sv.interpolation=inter;

    return vsdi.AddOutput(sv);
}

void ShaderCreateInfoVertex::AddJoint()
{
    AddInput(VAT_UVEC4, VAN::JointID,    VK_VERTEX_INPUT_RATE_VERTEX,VertexInputGroup::JointID);
    AddInput(VAT_VEC4,  VAN::JointWeight,VK_VERTEX_INPUT_RATE_VERTEX,VertexInputGroup::JointWeight);
}

void ShaderCreateInfoVertex::AddAssignTransform()
{
    // 添加Transform索引输入
    AddInput(   Assign::TransformID::VAT_FMT,
                Assign::TransformID::VIS_NAME,
                VK_VERTEX_INPUT_RATE_INSTANCE,
                VertexInputGroup::TransformID);
    
    AddFunction(STD_MTL_FUNC_NAMESPACE::MF_GetLocalToWorld_ByAssign);
}

void ShaderCreateInfoVertex::AddAssignMaterialInstance()
{
    // 添加MaterialInstance索引输入
    AddInput(   Assign::MaterialInstanceID::VAT_FMT,
                Assign::MaterialInstanceID::VIS_NAME,
                VK_VERTEX_INPUT_RATE_INSTANCE,
                VertexInputGroup::MaterialInstanceID);
}

bool ShaderCreateInfoVertex::ProcSubpassInput()
{
    auto sil=vsdi.GetSubpassInputList();

    if(sil.IsEmpty())
        return(true);

    final_shader+="\n";

    auto si=sil.GetData();
    int si_count=sil.GetCount();

    for(int i=0;i<si_count;i++)
    {
        final_shader+="layout(input_attachment_index=";
        final_shader+=AnsiString::numberOf((*si)->input_attachment_index);
        final_shader+=", binding=";
        final_shader+=AnsiString::numberOf((*si)->binding);
        final_shader+=") uniform subpassInput ";
        final_shader+=(*si)->name;
        final_shader+=";\n";

        ++si;
    }

    return(true);
}

bool ShaderCreateInfoVertex::ProcInput(ShaderCreateInfo *)
{
    if(!ProcSubpassInput())
        return(false);

    const auto &input=vsdi.GetInput();

    if(input.count<=0)
    {
        //no input ? this isn't a bug.
        //maybe position info from UBO/SBBO/Texture.
        return(true);
    }

    final_shader+="\n";

    const VertexInputAttribute *ss=input.items;

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

void ShaderCreateInfoVertex::GetOutputStrcutString(AnsiString &str)
{
    vsdi.GetOutput().ToString(str);
}
VK_NAMESPACE_END

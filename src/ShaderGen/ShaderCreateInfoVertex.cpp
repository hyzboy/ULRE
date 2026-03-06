#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/common/RenderAssignDef.h>
#include"GLSLCompiler.h"
#include"common/MFCommon.h"
#include<string>

namespace hgl::graph{

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
        via.input_rate=uint8_t(VertexInputRate::Vertex);
        via.group=VertexInputGroup::Basic;

        if(vsdi.AddInput(via))
            ++count;
    }

    return count;
}

int ShaderCreateInfoVertex::AddInput(const VAType &type,const std::string &name,const VertexInputRate input_rate,const VertexInputGroup &group)
{
    VIA via;

    hgl::strcpy(via.name,sizeof(via.name),name.c_str());

    via.basetype=(uint8) type.basetype;
    via.vec_size=        type.vec_size;

    via.input_rate      =uint8_t(input_rate);
    via.group           =group;

    via.interpolation   =Interpolation::Smooth;

    return vsdi.AddInput(via);
}

int ShaderCreateInfoVertex::AddInput(const char *type,const std::string &name,const VertexInputRate input_rate,const VertexInputGroup &group)
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

int ShaderCreateInfoVertex::AddOutput(const SVType &type,const std::string &name,Interpolation inter)
{
    ShaderVariable sv;

    hgl::strcpy(sv.name,sizeof(sv.name),name.c_str());

    sv.type=type;
    sv.interpolation=inter;

    return vsdi.AddOutput(sv);
}

void ShaderCreateInfoVertex::AddJoint()
{
    AddInput(VAT_UVEC4, VAN::JointID,    VertexInputRate::Vertex,VertexInputGroup::JointID);
    AddInput(VAT_VEC4,  VAN::JointWeight,VertexInputRate::Vertex,VertexInputGroup::JointWeight);
}

void ShaderCreateInfoVertex::AddAssignTransform()
{
    // 添加Transform索引输入
    AddInput(   Assign::TransformID::VAT_FMT,
                Assign::TransformID::VIS_NAME,
                VertexInputRate::Instance,
                VertexInputGroup::TransformID);

    AddFunction(hgl::graph::mtl::func::MF_GetLocalToWorld_ByAssign);
}

void ShaderCreateInfoVertex::AddAssignMaterialInstance()
{
    // 添加MaterialInstance索引输入
    AddInput(   Assign::MaterialInstanceID::VAT_FMT,
                Assign::MaterialInstanceID::VIS_NAME,
                VertexInputRate::Instance,
                VertexInputGroup::MaterialInstanceID);
}

bool ShaderCreateInfoVertex::ProcSubpassInput()
{
    const auto &sil=vsdi.GetSubpassInputList();

    if(sil.empty())
        return(true);

    std::string block;
    block+="\n";

    for(const auto *si:sil)
    {
        block+="layout(input_attachment_index=";
        const std::string input_attachment_index_str=std::to_string(si->input_attachment_index);
        block+=input_attachment_index_str;
        block+=", binding=";
        const std::string binding_str=std::to_string(si->binding);
        block+=binding_str;
        block+=") uniform subpassInput ";
        block+=si->name;
        block+=";\n";
    }

    final_shader+=block.c_str();

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

    const VertexInputAttribute *ss=input.items;
    std::string block;
    block+="\n";

    for(uint i=0;i<input.count;i++)
    {
        block+="layout(location=";
        const std::string location_str=std::to_string(ss->location);
        block+=location_str;
        block+=") in ";
        block+=GetShaderAttributeTypename(ss);
        block+=" ";
        block+=ss->name;
        block+=";\n";

        ++ss;
    }

    final_shader+=block.c_str();

    return(true);
}

void ShaderCreateInfoVertex::GetOutputStrcutString(std::string &str)
{
    vsdi.GetOutput().ToString(str);
}
}//namespace hgl::graph


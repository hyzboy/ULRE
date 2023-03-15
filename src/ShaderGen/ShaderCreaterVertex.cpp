#include<hgl/shadergen/ShaderCreaterVertex.h>
#include<hgl/shadergen/ShaderDescriptorManager.h>
#include<hgl/graph/VertexAttrib.h>
#include<hgl/graph/VKShaderStage.h>

SHADERGEN_NAMESPACE_BEGIN

using namespace hgl;
using namespace hgl::graph;

int ShaderCreaterVertex::AddInput(const VAT &type,const AnsiString &name)
{
    ShaderStage *ss=new ShaderStage;

    hgl::strcpy(ss->name,sizeof(ss->name),name.c_str());

    ss->basetype=(uint8) type.basetype;
    ss->vec_size=        type.vec_size;

    return sdm->AddInput(ss);
}

int ShaderCreaterVertex::AddInput(const AnsiString &type,const AnsiString &name)
{
    VAT vat;

    if(!ParseVertexAttribType(&vat,type))
        return(-2);

    return AddInput(vat,name);
}

bool ShaderCreaterVertex::ProcInput(ShaderCreater *)
{    
    const auto &input=sdm->GetShaderStageIO().input;

    if(input.IsEmpty())
    {
        //no input ? this isn't a bug.
        //maybe position info from UBO/SBBO/Texture.
        return(true);
    }

    if(!input.IsEmpty())
    {
        final_shader+="\n";

        for(auto *ss:input)
        {
            final_shader+="layout(location=";
            final_shader+=UTF8String::numberOf(ss->location);
            final_shader+=") in ";
            final_shader+=UTF8String(GetShaderStageTypeName(ss));
            final_shader+=" "+UTF8String(ss->name);
            final_shader+=";\n";
        }
    }

    return(true);
}
SHADERGEN_NAMESPACE_END

#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<hgl/graph/VertexAttrib.h>
#include<hgl/graph/VKShaderStage.h>
#include"GLSLCompiler.h"

SHADERGEN_NAMESPACE_BEGIN

using namespace hgl;
using namespace hgl::graph;

int ShaderCreateInfoVertex::AddInput(const VAT &type,const AnsiString &name)
{
    ShaderAttribute *ss=new ShaderAttribute;

    hgl::strcpy(ss->name,sizeof(ss->name),name.c_str());

    ss->basetype=(uint8) type.basetype;
    ss->vec_size=        type.vec_size;

    return sdm->AddInput(ss);
}

int ShaderCreateInfoVertex::AddInput(const AnsiString &type,const AnsiString &name)
{
    VAT vat;

    if(!ParseVertexAttribType(&vat,type))
        return(-2);

    return AddInput(vat,name);
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
SHADERGEN_NAMESPACE_END

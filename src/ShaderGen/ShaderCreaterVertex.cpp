#include<hgl/shadergen/ShaderCreaterVertex.h>

SHADERGEN_NAMESPACE_BEGIN
int ShaderCreaterVertex::AddInput(const VAT &type,const AnsiString &name)
{
    ShaderStage *ss=new ShaderStage;

    hgl::strcpy(ss->name,sizeof(ss->name),name.c_str());

    ss->basetype=(uint8) type.basetype;
    ss->vec_size=        type.vec_size;

    return sdm.AddInput(ss);
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
    const auto &io=sdm.GetShaderStageIO();

    if(io.input.IsEmpty())
    {
        //no input ? this isn't a bug.
        //maybe position info from UBO/SBBO/Texture.
        return(true);
    }

    {
        for(auto *ss:io.input)
        {
            final_shader+="layout(location=";
            final_shader+=UTF8String::numberOf(ss->location);
            final_shader+=") in ";
            final_shader+=UTF8String(GetShaderStageTypeName(ss));
            final_shader+=" "+UTF8String(ss->name);
            final_shader+=";\n";
        }

        final_shader+="\n";
    }

    return(true);
}
SHADERGEN_NAMESPACE_END

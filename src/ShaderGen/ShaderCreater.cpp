#include<hgl/shadergen/ShaderCreater.h>

SHADERGEN_NAMESPACE_BEGIN
int ShaderCreater::AddOutput(const VAT &type,const AnsiString &name)
{
    ShaderStage *ss=new ShaderStage;

    hgl::strcpy(ss->name,sizeof(ss->name),name.c_str());

    ss->basetype=(uint8) type.basetype;
    ss->vec_size=        type.vec_size;

    return sdm.AddOutput(ss);
}

int ShaderCreater::AddOutput(const AnsiString &type,const AnsiString &name)
{
    VAT vat;

    if(!ParseVertexAttribType(&vat,type))
        return(-2);

    return AddOutput(vat,name);
}

bool ShaderCreater::CompileToSPV()
{
    if(shader_codes.IsEmpty())
        return(false);

    
}
SHADERGEN_NAMESPACE_END

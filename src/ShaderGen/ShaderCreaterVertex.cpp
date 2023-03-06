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
SHADERGEN_NAMESPACE_END

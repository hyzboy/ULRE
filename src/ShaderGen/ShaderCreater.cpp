#include<hgl/shadergen/ShaderCreater.h>

SHADERGEN_NAMESPACE_BEGIN
int ShaderCreater::AddInput(const VAT &type,const AnsiString &name)
{
    ShaderStage *ss=new ShaderStage;

    hgl::strcpy(ss->name,sizeof(ss->name),name.c_str());

    ss->basetype=(uint8) type.basetype;
    ss->vec_size=        type.vec_size;

    return sdm.AddInput(ss);
}

int ShaderCreater::AddInput(const AnsiString &type,const AnsiString &name)
{
    VAT vat;

    if(!ParseVertexAttribType(&vat,type))
        return(-2);

    return AddInput(vat,name);
}

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

bool ShaderCreater::AddFunction(const AnsiString &return_type,const AnsiString &func_name,const AnsiString &param_list,const AnsiString &codes)
{
    //return sdm.AddFunction(return_type,func_name,param_list,code);
}
SHADERGEN_NAMESPACE_END
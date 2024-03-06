#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/mtl/ShaderBuffer.h>

STD_MTL_NAMESPACE_BEGIN

/**
* 所有UBO的基类，它即向生成器提供代码，也可以为渲染器提供数据
*/
class UniformBuffer
{
private:

    ShaderBufferSource *sbs;

public:

    virtual const AnsiString &GetStructName         ()const{return sbs->struct_name;}               ///<取得结构名称
    virtual const AnsiString &GetDefaultValueName   ()const{return sbs->name;}                      ///<取得默认变量名称
    virtual const AnsiString &GetShaderCodes        ()const{return sbs->codes;}                     ///<取得Shader代码
};//class UniformBuffer

STD_MTL_NAMESPACE_END

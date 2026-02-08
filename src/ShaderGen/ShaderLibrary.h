#pragma once

#include<hgl/type/String.h>
#include<hgl/shader_schema/StdMaterial.h>

STD_MTL_NAMESPACE_BEGIN

//void SetGlobalDefine(const AnsiString &,const AnsiString &);

const AnsiString *LoadShader(const AnsiString &);

STD_MTL_NAMESPACE_END

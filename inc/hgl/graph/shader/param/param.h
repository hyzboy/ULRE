#ifndef HGL_GRAPH_SHADER_PARAM_INCLUDE
#define HGL_GRAPH_SHADER_PARAM_INCLUDE

#include<hgl/type/BaseString.h>
#include<hgl/graph/shader/common.h>
#include<hgl/graph/shader/param/type.h>

SHADER_PARAM_NAMESPACE_BEGIN
/**
 * 参数定义
 */
class Param
{
    UTF8String name;            //参数名称
    ParamType type;             //类型

public:

    Param(const UTF8String &n,const ParamType &t)
    {
        name=n;
        type=t;
    }

    virtual ~Param()=default;

    const UTF8String &  GetName()const{return name;}
    const ParamType     GetType()const{return type;}
};//class Param
SHADER_PARAM_NAMESPACE_END
#endif//HGL_GRAPH_SHADER_PARAM_INCLUDE

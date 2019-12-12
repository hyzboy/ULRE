#ifndef HGL_GRAPH_SHADER_PARAM_DATA_INCLUDE
#define HGL_GRAPH_SHADER_PARAM_DATA_INCLUDE

#include<hgl/graph/shader/param/type.h>

BEGIN_SHADER_PARAM_NAMESPACE
class ParamData
{
    ParamType type;

public:

    ParamData();
    virtual ~ParamData();

    /**
     * 比较当前类型是否接收外部参数的输入
     * @param in 外部传入的参数
     */
    virtual bool JoinCheck(const ParamData *in)
    {
        if(!in)return(false);

        return(type==in->type);
    }
};//class ParamData
END_SHADER_PARAM_NAMESPACE
#endif//HGL_GRAPH_SHADER_PARAM_DATA_INCLUDE

#include<hgl/graph/shader/node/finished.h>

SHADER_NODE_NAMESPACE_BEGIN
bool VertexFinished::GenCode(UTF8StringList &sl)
{
    if(!Finished::GenCode(sl))
        return(false);

    UTF8String name;

    if(!GetInputParamName(name,ip_Position))
        return(false);

    sl.Add("gl_Position="+name+";");
    return(true);
}
SHADER_NODE_NAMESPACE_END

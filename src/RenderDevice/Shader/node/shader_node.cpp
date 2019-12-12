#include<hgl/graph/shader/node/node.h>

BEGIN_SHADER_NODE_NAMESPACE
bool Node::CheckInputParam()
{
    const int count=input_params.GetCount();
    param::InputParam **ip=input_params.GetData();

    for(int i=0;i<count;i++)
        if(!(*ip)->Check())
            return(false);
        else
            ++ip;

    return(true);
}
END_SHADER_NODE_NAMESPACE

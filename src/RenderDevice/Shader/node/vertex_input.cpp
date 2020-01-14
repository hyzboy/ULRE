#include<hgl/graph/shader/node/vertex_input.h>

SHADER_NODE_NAMESPACE_BEGIN
bool VertexInput::GetOutputParamName(UTF8String &result,const param::OutputParam *op)
{
    if(!op||!IsOutput(op))
        return(false);

    result=op->GetName();
    return(true);
}
SHADER_NODE_NAMESPACE_END
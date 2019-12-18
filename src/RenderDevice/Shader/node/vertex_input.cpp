#include<hgl/graph/shader/node/vertex_input.h>

BEGIN_SHADER_NODE_NAMESPACE
bool VertexInput::GetOutputParamName(UTF8String &result,const param::OutputParam *op)
{
    if(!op||!IsOutput(op))
        return(false);

    result=op->GetName();
    return(true);
}
END_SHADER_NODE_NAMESPACE
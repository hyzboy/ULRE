#include<hgl/graph/shader/param/in.h>

BEGIN_SHADER_PARAM_NAMESPACE
bool InputParam::Join(node::Node *n,OutputParam *op)
{
    if(!n||!op)return(false);

    if(!n->IsOutputParam(op))
        return(false);

    input_node=n;
    output_param=op;

    return(true);
}

void InputParam::Break()
{
}

bool InputParam::Check()
{
}
END_SHADER_PARAM_NAMESPACE

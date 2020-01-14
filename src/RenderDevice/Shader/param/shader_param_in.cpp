#include<hgl/graph/shader/param/in.h>
#include<hgl/graph/shader/node/node.h>

SHADER_PARAM_NAMESPACE_BEGIN
bool InputParam::Join(node::Node *n,param::OutputParam *op)
{
    if(!n||!op)return(false);

    if(!n->IsOutput(op))
        return(false);

    if(!JoinCheck(n,op))
        return(false);

    join_node=n;
    join_param=op;

    return(true);
}
SHADER_PARAM_NAMESPACE_END

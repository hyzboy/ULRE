#include<hgl/graph/shader/param/in.h>
#include<hgl/graph/shader/node/node.h>

BEGIN_SHADER_PARAM_NAMESPACE
bool InputParam::Join(node::Node *n,param::OutputParam *op)
{
    if(!n||!op)return(false);

    if(!n->IsOutputParam(op))
        return(false);

    if(!JoinCheck(n,op))
        return(false);

    join_node=n;
    join_param=op;

    return(true);
}
END_SHADER_PARAM_NAMESPACE

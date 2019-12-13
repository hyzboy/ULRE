#include<hgl/graph/shader/node/node.h>

BEGIN_SHADER_NODE_NAMESPACE
void Node::AddInput(bool mj,const UTF8String &n,const param::ParamType &pt)
{
    param::InputParam *ip=new param::InputParam(mj,n,pt);

    input_params.Add(ip);

    input_params_by_name.Add(n,ip);
}

void Node::AddOutput(const UTF8String &n,const param::ParamType &pt)
{
    param::OutputParam *op=new param::OutputParam(n,pt);

    output_params.Add(op);

    output_params_by_name.Add(n,op);
}

bool Node::JoinInput(const UTF8String &param_name,node::Node *n,const UTF8String &op_name)
{
    if(param_name.IsEmpty()||!n)
        return(false);

    param::OutputParam *op=n->GetOutput(op_name);

    if(!op)
        return(false);

    param::InputParam *ip=GetInput(param_name);

    if(!ip)
        return(false);

    return ip->Join(n,op);
}

bool Node::IsOutput(param::OutputParam *op)
{
    if(!op)return(false);

    return output_params.IsExist(op);
}

bool Node::Check()
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

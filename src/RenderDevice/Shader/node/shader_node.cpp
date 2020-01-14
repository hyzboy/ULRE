#include<hgl/graph/shader/node/node.h>

SHADER_NODE_NAMESPACE_BEGIN
param::InputParam *Node::AddInput(bool mj,const UTF8String &n,const param::ParamType &pt)
{
    param::InputParam *ip=new param::InputParam(mj,n,pt);

    input_params.Add(ip);

    input_params_by_name.Add(n,ip);

    return ip;
}

param::OutputParam *Node::AddOutput(const UTF8String &n,const param::ParamType &pt)
{
    param::OutputParam *op=new param::OutputParam(n,pt);

    output_params.Add(op);

    output_params_by_name.Add(n,op);

    return op;
}

bool Node::JoinInput(const UTF8String &param_name,node::Node *n,param::OutputParam *op)
{
    if(param_name.IsEmpty()||!n||!op)
        return(false);

    if(!n->IsOutput(op))
        return(false);

    param::InputParam *ip=GetInput(param_name);

    if(!ip)
        return(false);

    return ip->Join(n,op);
}

bool Node::JoinInput(const UTF8String &param_name,node::Node *n)
{
    if(param_name.IsEmpty()||!n)
        return(false);

    node::OutputParamList &opl=n->GetOutputParamList();

    if(opl.GetCount()!=1)
        return(false);

    param::OutputParam *op=*(opl.GetBegin());
    
    param::InputParam *ip=GetInput(param_name);

    if(!ip)
        return(false);

    return ip->Join(n,op);    
}

bool Node::IsOutput(const param::OutputParam *op) const
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

bool Node::GetInputParamName(UTF8String &result,const param::InputParam *ip)
{
    if(!ip)return(false);

    result=node_name+U8_TEXT("_")+ip->GetName();

    return(true);
}

bool Node::GetOutputParamName(UTF8String &result,const param::OutputParam *op)
{
    if(!op)return(false);

    result=node_name+U8_TEXT("_")+op->GetName();

    return(true);
}

bool Node::GenInputParamCode(UTF8StringList &sl)
{
    const int count=input_params.GetCount();
    param::InputParam **ip=input_params.GetData();

    UTF8String in_name,out_name;
    Node *jin;
    param::OutputParam *jop;

    for(int i=0;i<count;i++)
    {
        if(!this->GetInputParamName(in_name,*ip))
            return(false);

        jin=(*ip)->GetJoinNode();
        if(jin)
        {
            jop=(*ip)->GetJoinParam();

            if(!jin->GetOutputParamName(out_name,jop))
                return(false);
        }
        else
        {
            out_name=(*ip)->GetDefaultValue();
        }

        sl.Add(in_name+"="+out_name+";");

        ++ip;
    }

    return(true);
}

#define SHADER_NODE_TEMP_VALUE_COMMENT ";\t\t\t// temp value of ["+node_name+"]"

bool Node::GenTempValueDefine(UTF8StringList &sl)
{
    const int count=input_params.GetCount();
    param::InputParam **ip=input_params.GetData();
    param::ParamType pt;
    UTF8String value_name;

    for(int i=0;i<count;i++)
    {
        if(!GetInputParamName(value_name,*ip))
            return(false);

        pt=(*ip)->GetType();

        if((*ip)->GetJoinNode())
            sl.Add("\t"+UTF8String(param::GetTypename(pt))+" "+value_name+SHADER_NODE_TEMP_VALUE_COMMENT);
        else
            sl.Add("\t"+UTF8String(param::GetTypename(pt))+" "+value_name+"="+(*ip)->GetDefaultValue()+SHADER_NODE_TEMP_VALUE_COMMENT);

        ++ip;
    }

    return(true);
}

bool Node::GenCode(UTF8StringList &sl)
{
    if(!GenInputParamCode(sl))
        return(false);

    if(!GenOutputParamCode(sl))
        return(false);

    return(true);
}
SHADER_NODE_NAMESPACE_END

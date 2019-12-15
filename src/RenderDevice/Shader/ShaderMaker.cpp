#include<hgl/graph/shader/ShaderMaker.h>
#include<hgl/graph/shader/param/in.h>
#include<hgl/type/Set.h>
#include<hgl/log/LogInfo.h>

BEGIN_SHADER_NAMESPACE
bool ShaderMaker::Check()
{
    if(!fin_node)
        return(false);

    Set<node::Node *> post;             //完成检测的节点合集
    Set<node::Node *> prev;             //等待检测的节点合集
    node::Node *cur,*nn;

    node::NodeType type;
    node::InputParamList *ipl;
    int i,count;
    param::InputParam **ip;

    prev.Add(fin_node);

    while(prev.GetEnd(cur))
    {
        ipl     =&(cur->GetInputParamList());
        count   =ipl->GetCount();
        ip      =ipl->GetData();

        for(i=0;i<count;i++)
        {
            type=cur->GetNodeType();
            
            if(type<node::NodeType::BEGIN_NODE_TYPE_RANGE
             ||type>node::NodeType::END_NODE_TYPE_RANGE)
            {
                LOG_ERROR(U8_TEXT("node type error!"));
                return(false);
            }

            if(!(*ip)->Check())
                return(false);

            nn=(*ip)->GetJoinNode();

            if(nn)
            {
                if(!post.IsMember(nn)
                 &&!prev.IsMember(nn)
                 &&nn!=cur)
                {
                    prev.Add(nn);
                }
            }

            ip++;
        }

        node_list[int(type)-int(node::NodeType::BEGIN_NODE_TYPE_RANGE)].Add(cur);
        prev.Delete(cur);
        post.Add(cur);
    }

    return(true);
}

void ShaderMaker::MakeHeader()
{
}

void ShaderMaker::MakeVertexInput(const NodeList &nl)
{
}

void ShaderMaker::MakeConstValue(const NodeList &nl)
{
}

void ShaderMaker::MakeTextureInput(const NodeList &nl)
{
}

void ShaderMaker::MakeUBOInput(const NodeList &nl)
{
}

void ShaderMaker::MakeOutput()
{
}

void ShaderMaker::MakeFinished()
{
}

bool ShaderMaker::Make()
{
    if(!Check())
        return(false);

    MakeHeader();
    MakeVertexInput(    node_list[int(node::NodeType::VertexInput   )-int(node::NodeType::BEGIN_NODE_TYPE_RANGE)]);
    MakeConstValue(     node_list[int(node::NodeType::Const         )-int(node::NodeType::BEGIN_NODE_TYPE_RANGE)]);
    MakeTextureInput(   node_list[int(node::NodeType::Texture       )-int(node::NodeType::BEGIN_NODE_TYPE_RANGE)]);
    MakeUBOInput(       node_list[int(node::NodeType::UBO           )-int(node::NodeType::BEGIN_NODE_TYPE_RANGE)]);
    MakeOutput();
    MakeFinished();

    return(true);
}
END_SHADER_NAMESPACE

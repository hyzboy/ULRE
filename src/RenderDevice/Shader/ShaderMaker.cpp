#include<hgl/graph/shader/ShaderMaker.h>
#include<hgl/graph/shader/param/in.h>
#include<hgl/graph/shader/node/texture.h>
#include<hgl/type/Set.h>
#include<hgl/log/LogInfo.h>

#include<hgl/io/FileOutputStream.h>
#include<hgl/io/DataOutputStream.h>
#include<hgl/io/TextOutputStream.h>

BEGIN_SHADER_NAMESPACE
namespace
{
    
}//namespace

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
        type=cur->GetNodeType();
            
        if(type<node::NodeType::BEGIN_NODE_TYPE_RANGE
         ||type>node::NodeType::END_NODE_TYPE_RANGE)
        {
            LOG_ERROR(U8_TEXT("node type error!"));
            return(false);
        }

        ipl     =&(cur->GetInputParamList());
        count   =ipl->GetCount();
        ip      =ipl->GetData();

        for(i=0;i<count;i++)
        {
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
        node_stack.Add(cur);
    }

    return(true);
}

bool ShaderMaker::MakeHeader()
{
    if(api==API::Vulkan||api==API::OpenGLCore)  shader_source.Add("#version "+UTF8String(api_version)+" core"); else
    if(api==API::OpenGLES)                      shader_source.Add("#version "+UTF8String(api_version)+" es"); else
    {
        return(false);
    }

    shader_source.Add("layout(push_constant) uniform PushConstant");
    shader_source.Add("{");
    shader_source.Add("    mat4 local_to_world;");
    shader_source.Add("}pc;");

    shader_source.Add("");
    return(true);
}

bool ShaderMaker::MakeVertexInput()
{
    if(!vi_node)return(true);

    const node::OutputParamList &op_list=vi_node->GetOutputParamList();

    const uint count=op_list.GetCount();

    if(count<=0)return(false);

    param::OutputParam **op=op_list.GetData();
    param::ParamType pt;

    for(uint i=0;i<count;i++)
    {
        pt=(*op)->GetType();

        shader_source.Add("layout(location="+UTF8String(in_location)+") in "+GetTypename(pt)+" "+(*op)->GetName()+";");

        ++op;
        ++in_location;
    }

    shader_source.Add("");
    return(true);
}

void ShaderMaker::MakeConstValue(const NodeList &nl)
{
}

void ShaderMaker::MakeTextureInput(const NodeList &nl)
{
    const uint count=nl.GetCount();

    if(count<=0)return;

    node::texture **tex_node=(node::texture **)nl.GetData();
    param::ParamType tt;

    for(uint i=0;i<count;i++)
    {
        tt=(*tex_node)->GetTextureType();

        shader_source.Add("layout(binding="+UTF8String(binding)+") uniform "+GetTypename(tt)+" "+(*tex_node)->GetNodeName()+";");

        ++tex_node;
        ++binding;
    }

    shader_source.Add("");
}

void ShaderMaker::MakeUBOInput(const NodeList &nl)
{
}

void ShaderMaker::MakeOutput()
{
    node::OutputParamList &opl=fin_node->GetOutputParamList();

    const uint count=opl.GetCount();

    if(count<=0)return;

    param::OutputParam **op=opl.GetData();
    param::ParamType pt;

    for(uint i=0;i<count;i++)
    {
        pt=(*op)->GetType();

        shader_source.Add("layout(location="+UTF8String(out_location)+") out "+GetTypename(pt)+" "+(*op)->GetName()+";");

        ++op;
        ++out_location;        
    }
    
    shader_source.Add("");
}

void ShaderMaker::MakeFinished()
{
    shader_source.Add("void main()");
    shader_source.Add("{");
    {
        const uint count=node_stack.GetCount();
        node::Node **cur=node_stack.GetEnd();

        for(uint i=0;i<count;i++)
        {
            (*cur)->GenTempValueDefine(shader_source);      //临时变量定义

            --cur;
        }
        
        shader_source.Add("");

        cur=node_stack.GetEnd();

        for(uint i=0;i<count;i++)
        {
            #ifdef _DEBUG
            shader_source.Add("//[begin] auto code of "+(*cur)->GetNodeName());
            #endif//_DEBUG

            (*cur)->GenCode(shader_source);                 //产生代码

            #ifdef _DEBUG
            shader_source.Add("//[end] auto code of "+(*cur)->GetNodeName());
            shader_source.Add("");
            #endif//_DEBUG

            --cur;
        }
    }
    shader_source.Add("}");
}

bool ShaderMaker::Make()
{
    shader_source.Clear();
    node_stack.ClearData();
    in_location=0;
    out_location=0;
    binding=0;

    if(!Check())
        return(false);

    if(!MakeHeader())return(false);

    MakeConstValue  (node_list[int(node::NodeType::Const    )-int(node::NodeType::BEGIN_NODE_TYPE_RANGE)]);
    MakeTextureInput(node_list[int(node::NodeType::Texture  )-int(node::NodeType::BEGIN_NODE_TYPE_RANGE)]);
    MakeUBOInput    (node_list[int(node::NodeType::UBO      )-int(node::NodeType::BEGIN_NODE_TYPE_RANGE)]);

    if(!MakeVertexInput())return(false);

    MakeOutput      ();
    MakeFinished    ();

    return(true);
}

bool ShaderMaker::SaveToFile(const OSString &filename)
{
    io::FileOutputStream fos;

    if(!fos.CreateTrunc(filename))
        return(false);

    io::TextOutputStream *tos=io::CreateTextOutputStream<char>(&fos);

    tos->WriteText(shader_source);

    delete tos;
    return(true);
}
END_SHADER_NAMESPACE

#ifndef HGL_GRAPH_SHADER_NODE_INCLUDE
#define HGL_GRAPH_SHADER_NODE_INCLUDE

#include<hgl/type/BaseString.h>
#include<hgl/type/List.h>
#include<hgl/type/Map.h>
#include<hgl/graph/shader/node/type.h>
#include<hgl/graph/shader/param/in.h>
#include<hgl/graph/shader/param/out.h>

BEGIN_SHADER_NODE_NAMESPACE

#define SHADER_INPUT_PARAM(mj,name,type) AddInput(mj,#name,SHADER_PARAM_NAMESPACE::ParamType::type);
#define SHADER_OUTPUT_PARAM(name,type) AddOutput(#name,SHADER_PARAM_NAMESPACE::ParamType::type);

using InputParamList=ObjectList<param::InputParam>;
using OutputParamList=ObjectList<param::OutputParam>;
using InputParamMapByName=Map<UTF8String,param::InputParam *>;
using OutputParamMapByName=Map<UTF8String,param::OutputParam *>;

/**
 * Shader 节点是所有Shader的基础，它可以是一个数据转接(纹理或流)，也可以是一个纯粹的计算公式
 */
class Node
{
    NodeType node_type;

protected:

    UTF8String node_name;                                                       ///<节点用户自定义名称

    InputParamList input_params;
    OutputParamList output_params;

    InputParamMapByName input_params_by_name;
    OutputParamMapByName output_params_by_name;

protected:

            param::InputParam * AddInput            (bool mj,const UTF8String &n,const param::ParamType &pt);
            param::OutputParam *AddOutput           (const UTF8String &n,const param::ParamType &pt);

public:

            param::InputParam * GetInput            (const UTF8String &n)       ///<根据名称获取输入参数
                {return GetListObject(input_params_by_name,n);}

            param::OutputParam *GetOutput           (const UTF8String &n)       ///<根据名称获取输出参数
                {return GetListObject(output_params_by_name,n);}

public:

    Node(const NodeType &nt){node_type=nt;}
    Node(const NodeType &nt,const UTF8String &n){node_type=nt;node_name=n;}
    virtual ~Node()=default;

    const   NodeType            GetNodeType         ()const{return node_type;}

    const   UTF8String &        GetNodeName         ()const{return node_name;}
            void                SetNodeName         (const UTF8String &n){node_name=n;}

public: //参数相关

            InputParamList &    GetInputParamList   (){return input_params;}
            OutputParamList &   GetOutputParamList  (){return output_params;}

    virtual bool                JoinInput           (const UTF8String &,node::Node *,param::OutputParam *);

public: //参数相关

    virtual bool IsOutput(param::OutputParam *);

    virtual bool Check();                                                       ///<检测当前节点是否可用

    virtual bool GenCode(UTF8StringList &){return false;}
};//class Node
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_INCLUDE

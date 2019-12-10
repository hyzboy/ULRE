#ifndef HGL_GRAPH_SHADER_NODE_INCLUDE
#define HGL_GRAPH_SHADER_NODE_INCLUDE

#include<hgl/type/BaseString.h>
#include<hgl/type/List.h>

#define SHADER_NODE_NAMESPACE       hgl::graph::shader::node
#define BEGIN_SHADER_NODE_NAMESPACE namespace hgl{namespace graph{namespace shader{namespace node{
#define END_SHADER_NODE_NAMESPACE   }}}}

BEGIN_SHADER_NODE_NAMESPACE
    /**
    * Shader 节点是所有Shader的基础，它可以是一个简单的计算，也可以是一段复杂的函数
    */
    class Node
    {
        UTF8String type_name;                                                   ///<节点类型本身的名称
        UTF8String name;                                                        ///<节点用户自定义名称

    public:

        Node(const UTF8String &n){type_name=n;}
        virtual ~Node()=default;
    };//class Node
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_INCLUDE

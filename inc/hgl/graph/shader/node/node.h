#ifndef HGL_GRAPH_SHADER_NODE_INCLUDE
#define HGL_GRAPH_SHADER_NODE_INCLUDE

#include<hgl/type/BaseString.h>
#include<hgl/type/List.h>
#include<hgl/graph/shader/common.h>

BEGIN_SHADER_NODE_NAMESPACE
/**
 * Shader 节点是所有Shader的基础，它可以是一个简单的计算，也可以是一段复杂的函数
 */
class Node
{
    UTF8String type_name;                                                   ///<节点类型本身的名称

protected:

    UTF8String name;                                                        ///<节点用户自定义名称

    ObjectList<param::InputParam> input_params;
    ObjectList<param::OutputParam> output_params;

public:

    Node(const UTF8String &n){type_name=n;}
    virtual ~Node()=default;

    const UTF8String &  GetTypename()const{return type_name;}

    const UTF8String &  GetName()const{return name;}
    void                SetName(const UTF8String &n){name=n;}
    
public: //参数相关

    virtual bool IsOutputParam(param::OutputParam *);

    virtual bool CheckInputParam();
};//class Node
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_INCLUDE

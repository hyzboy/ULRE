#ifndef HGL_SHADER_NODE_INCLUDE
#define HGL_SHADER_NODE_INCLUDE

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

    /**
     * 纯输入节点
     */
    class Input:virtual public Node
    {
    public:

        ObjectList<InputParam> input_params;

    public:

        using Node::Node;
        virtual ~InputNode()=default;
    };//class InputNode

    /**
     * 纯输出节点，用于固管行为的向下一级节点输入数据，无输入部分
     */
    class Output:virtual public Node
    {
    public:

        ObjectList<OutputParam> output_params;

    public:

        using Node::Node;
        virtual ~OutputNode()=default;
    };//

    /**
     * 输入输出节点
     */
    class InputOutput:public Node
    {
    public:

        ObjectList<InputParam> input_params;
        ObjectList<OutputParam> output_params;

    public:

        using Node::Node;
        virtual ~InputOutputNode()=default;
    };//

    /**
     * 最终节点，用于最终结果的一类节点，无输出部分
     */
    class Finish:virtual public InputNode
    {
    public:

        using InputNode::InputNode;
        virtual ~FinishNode()=default;
    };//
BEGIN_SHADER_NODE_NAMESPACE
#endif//HGL_SHADER_NODE_INCLUDE

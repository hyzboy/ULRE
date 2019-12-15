#ifndef HGL_GRAPH_SHADER_MAKER_INCLUDE
#define HGL_GRAPH_SHADER_MAKER_INCLUDE

#include<hgl/graph/shader/node/finished.h>
#include<hgl/graph/shader/node/vertex_input.h>
BEGIN_SHADER_NAMESPACE

using NodeList=List<node::Node *>;

class ShaderMaker
{
    node::Finished *fin_node;

protected:

    NodeList node_list[node::NodeType::NODE_TYPE_RANGE_SIZE];

protected:

    virtual void MakeHeader();
    virtual void MakeVertexInput(const NodeList &);
    virtual void MakeConstValue(const NodeList &);
    virtual void MakeTextureInput(const NodeList &);
    virtual void MakeUBOInput(const NodeList &);
    virtual void MakeOutput();
    virtual void MakeFinished();

public:

    ShaderMaker(node::Finished *fn){fin_node=fn;}
    virtual ~ShaderMaker()=default;

    virtual bool Check();
    virtual bool Make();
};//class ShaderMaker
END_SHADER_NAMESPACE
#endif//HGL_GRAPH_SHADER_MAKER_INCLUDE

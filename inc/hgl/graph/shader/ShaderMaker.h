#ifndef HGL_GRAPH_SHADER_MAKER_INCLUDE
#define HGL_GRAPH_SHADER_MAKER_INCLUDE

#include<hgl/graph/shader/node/finished.h>
#include<hgl/graph/shader/node/vertex_input.h>
#include<hgl/type/StringList.h>
SHADER_NAMESPACE_BEGIN

using NodeList=List<node::Node *>;

enum class API
{
    OpenGLCore,
    OpenGLES,
    Vulkan
};

/**
 * 缺省绑定号
 */
enum class ShaderDefaultBinding
{
    WorldMatrix=0,
};

class ShaderMaker
{
protected:

    API api;
    uint api_version;

    UTF8StringList shader_source;

protected:

    node::VertexInput *vi_node;
    node::Finished *fin_node;

    List<node::Node *> node_stack;

protected:

    NodeList node_list[node::NodeType::NODE_TYPE_RANGE_SIZE];

    uint in_location=0;
    uint out_location=0;
    uint binding=0;

protected:

    virtual bool MakeHeader();
    virtual bool MakeVertexInput();
    virtual void MakeConstValue(const NodeList &);
    virtual void MakeScalarValue(const NodeList &);
    virtual void MakeTextureInput(const NodeList &);
    virtual void MakeUBOInput(const NodeList &);
    virtual void MakeOutput();
    virtual void MakeFinished();

public:

    ShaderMaker(const API &a,const uint av,node::Finished *fn,node::VertexInput *vi=nullptr)
    {
        api=a;
        api_version=av;

        fin_node=fn;
        vi_node=vi;
    }
    virtual ~ShaderMaker()=default;

    virtual bool Check();
    virtual bool Make();

    virtual bool SaveToFile(const OSString &);
};//class ShaderMaker
SHADER_NAMESPACE_END
#endif//HGL_GRAPH_SHADER_MAKER_INCLUDE

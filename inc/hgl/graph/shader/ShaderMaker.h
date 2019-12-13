#ifndef HGL_GRAPH_SHADER_MAKER_INCLUDE
#define HGL_GRAPH_SHADER_MAKER_INCLUDE

#include<hgl/graph/shader/node/finished.h>
BEGIN_SHADER_NAMESPACE
class ShaderMaker
{
    node::Finished *fin_node;

public:

    ShaderMaker(node::Finished *fn){fin_node=fn;}
    virtual ~ShaderMaker()=default;

    virtual bool Make();
};//class ShaderMaker
END_SHADER_NAMESPACE
#endif//HGL_GRAPH_SHADER_MAKER_INCLUDE

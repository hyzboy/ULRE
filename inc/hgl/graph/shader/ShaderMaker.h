#ifndef HGL_GRAPH_SHADER_MAKER_INCLUDE
#define HGL_GRAPH_SHADER_MAKER_INCLUDE

#include<hgl/graph/shader/node/finished.h>
#include<hgl/graph/shader/node/vertex_input.h>
BEGIN_SHADER_NAMESPACE
class ShaderMaker
{
    node::Finished *fin_node;

protected:

    List<node::VertexInput *> vi_list;              ///<顶点输入节点列表
//    List<node::Texture *> tex_list;                  ///<纹理输入节点列表
//    List<node::UBO *> ubo_list;                      ///<UBO输入节点列表
//    List<node::Function *> func_list;               ///<材质函数节点列表

public:

    ShaderMaker(node::Finished *fn){fin_node=fn;}
    virtual ~ShaderMaker()=default;

    virtual bool Check();
    virtual bool Make();
};//class ShaderMaker
END_SHADER_NAMESPACE
#endif//HGL_GRAPH_SHADER_MAKER_INCLUDE

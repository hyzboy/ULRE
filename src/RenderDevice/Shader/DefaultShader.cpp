#include<hgl/graph/shader/ShaderMaker.h>
#include<hgl/graph/shader/param/in.h>
#include<hgl/graph/shader/param/out.h>
#include<hgl/graph/shader/node/finished.h>
#include<hgl/graph/shader/node/vertex_input.h>

BEGIN_SHADER_NAMESPACE
namespace
{
    #define SHADER_VERTEX_INPUT_POSITION    "Position"

    bool CreateDefaultVertexShader()
    {
        node::VertexFinished vs_fin_node;                       //创建一个vs最终节点

                                                                //创建一个顶点输入节点
        node::VertexInput ni(   SHADER_VERTEX_INPUT_POSITION,   //该节点shader名称
                                param::ParamType::Float3);      //该节点数据类型

                                                                //对接顶点输入节点到VS最终节点
        vs_fin_node.JoinInput(  SHADER_VERTEX_INPUT_POSITION,   //最终节点名称
                                &ni,                            //要接入的节点
                                SHADER_VERTEX_INPUT_POSITION);  //要入节点的输出参数名称       (注：这里只是碰巧名字一样，所以共用一个宏)

        ShaderMaker vs_maker(&vs_fin_node);

        if(!vs_maker.Make())
            return(false);

        return(true);
    }

    bool CreateDefaultFragmentVertex()
    {
        node::FragmentFinished fs_fin_node;

        ShaderMaker fs_maker(&fs_fin_node);

        if(!fs_maker.Make())
            return(false);

        return(true);
    }
}//namespace

bool CreateDefaultMaterial()
{
    if(!CreateDefaultVertexShader())return(false);
    if(!CreateDefaultFragmentVertex())return(false);

    return(true);
}
END_SHADER_NAMESPACE

#include<hgl/graph/shader/ShaderMaker.h>
#include<hgl/graph/shader/param/in.h>
#include<hgl/graph/shader/param/out.h>
#include<hgl/graph/shader/node/finished.h>
#include<hgl/graph/shader/node/vertex_input.h>

BEGIN_SHADER_NAMESPACE
namespace
{
    #define SHADER_VERTEX_INPUT_STREAM_PARAM_NAME    "Position"      //<顶点数据输入流，输出参数名称
    #define SHADER_VERTEX_POSITION_INPUT_PARAM_NAME  "Position"      //<顶点shader最终节点，输入参数名称
    
    //**********************************************************************注：它们两个名字一样只是碰巧****************

    bool CreateDefaultVertexShader()
    {
        node::VertexFinished vs_fin_node;                                           //创建一个vs最终节点

                                                                                    //创建一个顶点输入节点
        node::VertexInput ni(   SHADER_VERTEX_INPUT_STREAM_PARAM_NAME,              //该节点shader名称
                                param::ParamType::Float3);                          //该节点数据类型

        param::OutputParam *op=ni.GetOutput(SHADER_VERTEX_INPUT_STREAM_PARAM_NAME); //获取节点中的输出参数

                                                                                    //对接顶点输入节点到VS最终节点
        vs_fin_node.JoinInput(  SHADER_VERTEX_POSITION_INPUT_PARAM_NAME,            //最终节点中这个输入参数的名称
                                &ni,op);                                            //要接入的节点以及它的输出参数

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

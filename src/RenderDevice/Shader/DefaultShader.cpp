#include<hgl/graph/shader/ShaderMaker.h>
#include<hgl/graph/shader/param/in.h>
#include<hgl/graph/shader/param/out.h>
#include<hgl/graph/shader/node/finished.h>
#include<hgl/graph/shader/node/vertex_input.h>
#include<hgl/graph/shader/node/vector.h>
#include<hgl/graph/shader/node/combo_vector.h>
#include<hgl/assets/AssetsSource.h>

BEGIN_SHADER_NAMESPACE
namespace
{
    namespace InlineShader
    {
        UTF8String Header;
        UTF8String UBOWorldMatrix;
        UTF8String PushConstant;

        UTF8String VSOutputLayout;

        bool Load()
        {
            assets::AssetsSource *as=assets::GetSource("shader");

            if(!as)
                return(false);

            #ifndef USE_MOBILE_SHADER
            Header          =UTF8String(as->Open("header_desktop.glsl"));
            #else
            Header          =UTF8String(as->Open("header_mobile.glsl"));
            #endif//

            UBOWorldMatrix  =UTF8String(as->Open("UBO_WorldMatrix.glsl"));
            PushConstant    =UTF8String(as->Open("push_constant_3d.glsl"));
            VSOutputLayout  =UTF8String(as->Open("vertex_shader_output_layout.glsl"));

            return(true);
        }
    }//namespace InlineShader

    constexpr enum class API    DEFAULT_RENDER_API          =API::Vulkan;
    constexpr uint              DEFAULT_RENDER_API_VERSION  =450;

    #define SHADER_VERTEX_INPUT_STREAM_POSITION_NAME "Position"      //<顶点数据输入流，输出参数名称
    #define SHADER_VERTEX_POSITION_INPUT_PARAM_NAME  "Position"      //<顶点shader最终节点，输入参数名称
    
    //**********************************************************************注：它们两个名字一样只是碰巧****************

    bool CreateDefaultVertexShader()
    {
        node::VertexFinished vs_fin_node;                                           //创建一个vs最终节点

        node::VertexInput vi;                                                       //创建一个顶点输入节点

        param::OutputParam *op=vi.Add(  SHADER_VERTEX_INPUT_STREAM_POSITION_NAME,   //该节点输出参数名称
                                        param::ParamType::Float3);                  //该节点数据类型

        node::Float1 x,y,z;

        node::ComboVector1to3 cv1to3;

        cv1to3.JoinInput("X",&x);
        cv1to3.JoinInput("Y",&y);
        cv1to3.JoinInput("Z",&z);
                                                                                    //对接顶点输入节点到VS最终节点
        vs_fin_node.JoinInput(  SHADER_VERTEX_POSITION_INPUT_PARAM_NAME,            //最终节点中这个输入参数的名称
                                &cv1to3);
//                                &vi,op);                                            //要接入的节点以及它的输出参数

        ShaderMaker vs_maker(   DEFAULT_RENDER_API,
                                DEFAULT_RENDER_API_VERSION,
                                &vs_fin_node,
                                &vi);

        if(!vs_maker.Make())
            return(false);

        vs_maker.SaveToFile(OS_TEXT("default.vert"));

        return(true);
    }

    bool CreateDefaultFragmentVertex()
    {
        node::FragmentFinished fs_fin_node;

        ShaderMaker fs_maker(   DEFAULT_RENDER_API,
                                DEFAULT_RENDER_API_VERSION,
                                &fs_fin_node);

        if(!fs_maker.Make())
            return(false);

        return(true);
    }
}//namespace

bool CreateDefaultMaterial()
{
    if(!InlineShader::Load())
        return(false);

    if(!CreateDefaultVertexShader())return(false);
    if(!CreateDefaultFragmentVertex())return(false);

    return(true);
}
END_SHADER_NAMESPACE

#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/module/RenderModule.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/log/Logger.h>

using namespace hgl;
using namespace hgl::graph;

class TestRenderModule:public RenderModule
{
public:

    RENDER_MODULE_CONSTRUCT(TestRenderModule)

    void OnResize(const VkExtent2D &size) override
    {
        LOG_INFO(OS_TEXT("Resize: ")+OSString::numberOf(size.width)+OS_TEXT("x")+OSString::numberOf(size.height));
    }

    void OnFrameRender(const double,RenderCmdBuffer *cmd) override
    {
        LOG_INFO(OS_TEXT("Execute"));

        //cmd->Begin
    }
};//class TestGraphModule:public RenderModule

int os_main(int,os_char **)
{
    RenderFramework rf;

    if(!rf.Init(1280,720,OS_TEXT("FirstApp")))
        return 1;

    rf.AddModule<TestRenderModule>();

    rf.Run();

    return 0;
}
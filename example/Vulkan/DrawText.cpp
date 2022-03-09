#include<hgl/type/StringList.h>
#include<hgl/graph/font/TextRender.h>
#include"VulkanAppFramework.h"

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH =1280;
constexpr uint32_t SCREEN_HEIGHT=SCREEN_WIDTH/16*9;

class TestApp:public VulkanApplicationFramework
{
private:

    TextRender *        text_render         =nullptr;

    TextRenderable *    text_render_obj     =nullptr;
    RenderableInstance *render_instance     =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(text_render)
    }

private:

    bool InitTextRenderable()
    {
        UTF16String str;

        FontSource *fs=AcquireFontSource(OS_TEXT("微软雅黑"),12);

        text_render=CreateTextRender(device,fs,device_render_pass,ubo_camera_info);
        if(!text_render)
            return(false);

        LoadStringFromTextFile(str,OS_TEXT("res/text/DaoDeBible.txt"));

        text_render_obj=text_render->CreateRenderable(str);
        if(!text_render_obj)
            return(false);

        render_instance=text_render->CreateRenderableInstance(text_render_obj);
        if(!render_instance)
            return(false);

        return(true);
    }

public:

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        if(!InitTextRenderable())
            return(false);

        BuildCommandBuffer(render_instance);

        return(true);
    }

    void Resize(int w,int h)override
    {
        VulkanApplicationFramework::Resize(w,h);
        
        BuildCommandBuffer(render_instance);
    }
};//class TestApp:public VulkanApplicationFramework

int main(int,char **)
{
    TestApp app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}

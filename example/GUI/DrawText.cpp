#include<hgl/io/LoadString.h>
#include<hgl/graph/font/TextRender.h>
#include"VulkanAppFramework.h"

using namespace hgl;
using namespace hgl::graph;

constexpr uint32_t SCREEN_WIDTH =1280;
constexpr uint32_t SCREEN_HEIGHT=SCREEN_WIDTH/16*9;

class TestApp:public CameraAppFramework
{
private:

    TextRender *        text_render         =nullptr;

    TextPrimitive *     text_primitive      =nullptr;
    Renderable *        render_obj          =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(text_render)
    }

private:

    bool InitTextRenderable()
    {
        U16String str;
        
        LoadStringFromTextFile(str,OS_TEXT("res/text/DaoDeBible.txt"));

        if(str.IsEmpty())return(false);

        FontSource *fs=AcquireFontSource(OS_TEXT("微软雅黑"),12);

        text_render=CreateTextRender(device,fs,device_render_pass,ubo_camera_info,str.Length());
        if(!text_render)
            return(false);

        text_primitive=text_render->CreatePrimitive(str);
        if(!text_primitive)
            return(false);

        render_obj=text_render->CreateRenderable(text_primitive);
        if(!render_obj)
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

        VulkanApplicationFramework::BuildCommandBuffer(render_obj);

        return(true);
    }

    void Resize(uint w,uint h)override
    {
        VulkanApplicationFramework::Resize(w,h);
        
        VulkanApplicationFramework::BuildCommandBuffer(render_obj);
    }

    void BuildCommandBuffer(uint32_t index)
    {
        VulkanApplicationFramework::BuildCommandBuffer(index,render_obj);
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

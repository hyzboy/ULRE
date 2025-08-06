#include<hgl/io/LoadString.h>
#include<hgl/graph/font/TextRender.h>
#include<hgl/WorkManager.h>

using namespace hgl;
using namespace hgl::graph;

class TestApp:public WorkObject
{
private:

    TextRender *        text_render         =nullptr;

    TextPrimitive *     text_primitive      =nullptr;
    Mesh *              render_obj          =nullptr;

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

        render_obj=text_render->CreateMesh(text_primitive);
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


int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("DrawText"),1280,720);
}

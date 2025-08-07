#include<hgl/io/LoadString.h>
#include<hgl/graph/font/TextRender.h>
#include<hgl/WorkManager.h>
#include<hgl/component/MeshComponent.h>

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

        text_render=CreateTextRender(GetRenderFramework(),fs,nullptr,str.Length());

        if(!text_render)
            return(false);

        text_primitive=text_render->CreatePrimitive(str);
        if(!text_primitive)
            return(false);

        render_obj=text_render->CreateMesh(text_primitive);
        if(!render_obj)
            return(false);

        CreateComponentInfo cci(GetSceneRoot());

        return CreateComponent<MeshComponent>(&cci,render_obj); //创建一个静态网格组件
    }

public:

    using WorkObject::WorkObject;

    bool Init() override
    {
        if(!InitTextRenderable())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("DrawText"),1280,720);
}

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
        
        LoadStringFromTextFile(str,OS_TEXT("res/text/桃花庵歌.txt"));

        if(str.IsEmpty())return(false);

        const int unique_char_count=str.UniqueCharCount();

        FontSource *fs=CreateCJKFontSource(OS_TEXT("Consolas"),OS_TEXT("楷体"),48);

        text_render=CreateTextRender(fs,unique_char_count);

        if(!text_render)
            return(false);

        text_primitive=text_render->CreatePrimitive();

        text_render->Layout(text_primitive,str);

        if(!text_primitive||!text_primitive->IsValid())
            return(false);

        render_obj=text_render->CreateMesh(text_primitive);

        layout::CharStyle cs;

        cs.CharColor=Color4ub(64,173,235,255);
        text_render->SetFixedStyle(cs);

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
    return RunFramework<TestApp>(OS_TEXT("DrawText"),2560,1440);
}

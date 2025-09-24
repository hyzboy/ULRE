#include<hgl/type/StringViewList.h>
#include<hgl/graph/font/TextRender.h>
#include<hgl/WorkManager.h>
#include<hgl/component/MeshComponent.h>
#include<hgl/graph/font/TextGeometry.h>
#include<random>

using namespace hgl;
using namespace hgl::graph;

constexpr const uint32_t WINDOW_WIDTH=2560;
constexpr const uint32_t WINDOW_HEIGHT=1440;

constexpr const uint32_t FONT_SIZE=48;
constexpr const uint32_t MAX_CHAR_COUNT=1024;

class TestApp:public WorkObject
{
private:

    TextRender *        text_render         =nullptr;

    TextGeometry *     text_primitive      =nullptr;
    Mesh *              render_obj          =nullptr;

public:

    ~TestApp()
    {
        SAFE_CLEAR(text_render)
    }

private:

    bool InitTextRenderable()
    {
        U16StringViewList str_list;
        
        LoadStringViewListFromTextFile(str_list,OS_TEXT("res/text/百家姓.txt"));

        if(str_list.IsEmpty())return(false);

        FontSource *fs=CreateCJKFontSource(OS_TEXT("Consolas"),OS_TEXT("楷体"),FONT_SIZE);

        text_render=CreateTextRender(fs,MAX_CHAR_COUNT);

        if(!text_render)
            return(false);

        text_primitive=text_render->Begin();

        Vector2i start_pos(0,0);
        std::default_random_engine dre;
        std::uniform_int_distribution<int> rand_x(0,WINDOW_WIDTH -FONT_SIZE);
        std::uniform_int_distribution<int> rand_y(0,WINDOW_HEIGHT-FONT_SIZE);
        
        for (auto str:str_list)
        {
            start_pos.x=rand_x(dre);
            start_pos.y=rand_y(dre);

            text_render->Layout(start_pos,str);
        }

        text_render->End();

        if(!text_primitive||!text_primitive->IsValid())
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
    return RunFramework<TestApp>(OS_TEXT("DrawText"),WINDOW_WIDTH,WINDOW_HEIGHT);
}

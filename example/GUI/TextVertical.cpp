#include<hgl/io/LoadString.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/TextComponent.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/graph/font/TextCharSSBO.h>
#include<hgl/color/Color4ub.h>
#include<hgl/framework/WorkManager.h>

using namespace hgl;
using namespace hgl::ecs;
using namespace hgl::graph;

constexpr uint SCREEN_WIDTH=2560;
constexpr uint SCREEN_HEIGHT=1440;

class TestApp:public WorkObject
{
private:

    ECSContext *        ecs_world           = nullptr;

protected:

    bool InitTextRenderable()
    {
        U16String str;

        LoadStringFromTextFile(str, OS_TEXT("res/text/道德经.txt"));

        if(str.IsEmpty())
            return(false);

        ecs_world = GetECSContext();
        if(!ecs_world)
            return(false);

        // Create font source
        FontSource *fs = CreateFontSource(OS_TEXT("微软雅黑"), 24);
        if(!fs)
            return(false);

        // Create entity with TextComponent
        auto* entity = ecs_world->CreateEntity();
        if(!entity)
            return(false);

        auto text_comp = entity->AddComponent<TextComponent>();
        if(!text_comp)
            return(false);

        // 竖排：从上到下、从右到左
        auto para = text_comp->GetParagraphStyle();
        para.text_direction = layout::TextDirection::Vertical;
        text_comp->SetParagraphStyle(para);

        hgl::graph::layout::CharStyle cs{};

        cs.text_color = Color4ub(255, 255, 255, 255).packUnorm4x8();   // 白（packUnorm4x8 序）

        // Just set text and font - TextRenderPipeline handles the rest!
        text_comp->SetText(str);
        text_comp->SetFontSource(fs);

        // 竖排：首坐标为第一个字符的右上角（列从右向左排）
        text_comp->SetStartPosition({SCREEN_WIDTH-24, 0});
        text_comp->SetCharStyle(cs);

        return(true);
    }

public:
    bool Init() override
    {
        if(!InitTextRenderable())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int argc, os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("DrawText_Vertical"), argc, argv, SCREEN_WIDTH, SCREEN_HEIGHT);
}

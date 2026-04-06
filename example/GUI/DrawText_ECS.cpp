#include<hgl/io/LoadString.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/TextComponent.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/framework/WorkManager.h>

using namespace hgl;
using namespace hgl::ecs;
using namespace hgl::graph;

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

        hgl::graph::layout::CharStyle cs;

        cs.CharColor.setOne();

        // Just set text and font - TextRenderPipeline handles the rest!
        text_comp->SetText(str);
        text_comp->SetFontSource(fs);
        text_comp->SetStartPosition({0, 0});
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
    return RunFramework<TestApp>(OS_TEXT("DrawText_ECS"), argc, argv, 2560, 1440);
}


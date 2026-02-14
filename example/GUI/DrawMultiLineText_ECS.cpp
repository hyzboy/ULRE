#include<hgl/type/StringViewList.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/TextComponent.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/WorkManager.h>
#include<random>

using namespace hgl;
using namespace hgl::ecs;
using namespace hgl::graph;

constexpr const uint32_t WINDOW_WIDTH = 2560;
constexpr const uint32_t WINDOW_HEIGHT = 1440;

constexpr const uint32_t FONT_SIZE = 48;

class TestApp:public WorkObject
{
private:

    ECSContext *        ecs_world           = nullptr;

protected:

    bool InitTextRenderable()
    {
        U16StringViewList str_list;

        LoadStringViewListFromTextFile(str_list, OS_TEXT("res/text/百家姓.txt"));

        if(str_list.IsEmpty())
            return(false);

        ecs_world = GetECSContext();
        if(!ecs_world)
            return(false);

        // Create CJK font source
        FontSource *fs = CreateCJKFontSource(OS_TEXT("Consolas"), OS_TEXT("楷体"), FONT_SIZE);
        if(!fs)
            return(false);

        // For random positioning
        std::default_random_engine dre;
        std::uniform_int_distribution<int> rand_x(0, WINDOW_WIDTH - FONT_SIZE);
        std::uniform_int_distribution<int> rand_y(0, WINDOW_HEIGHT - FONT_SIZE);

        // Create one entity per text string
        for (auto str : str_list)
        {
            auto* entity = ecs_world->CreateEntity();
            if(!entity)
                continue;

            auto text_comp = entity->AddComponent<TextComponent>();
            if(!text_comp)
                continue;

            // Set text, font, and random position
            text_comp->SetText(U16String(str.c_str(), str.Length()));
            text_comp->SetFontSource(fs);
            text_comp->SetStartPosition({rand_x(dre), rand_y(dre)});

            // TextRenderSystem will batch by FontSource automatically!
        }

        return(true);
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

int os_main(int, os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("DrawMultiLineText_ECS"), WINDOW_WIDTH, WINDOW_HEIGHT);
}


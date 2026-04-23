// 画一个带纹理的四边形 (ECS)
#include<hgl/framework/WorkManager.h>
#include<hgl/graph/module/GeometryManager.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

constexpr uint32_t VERTEX_COUNT=6;

constexpr float position_data[VERTEX_COUNT][2]=
{
    {-1, -1},
    {-1,  1},
    { 1, -1},

    { 1, -1},
    {-1,  1},
    { 1,  1},
};

constexpr float tex_coord_data[VERTEX_COUNT][2]=
{
    {0,0},
    {0,1},
    {1,0},

    {1,0},
    {0,1},
    {1,1}
};

class TestApp:public WorkObject
{
private:

    ECSContext *        ecs_world           = nullptr;
    Entity *            quad_entity         = nullptr;

    Geometry *          quad_geometry       = nullptr;

    inline static const mtl::MaterialRecipe kTexQuadCfg {
        .id             = "texture_quad",
        .preset         = mtl::MaterialPreset::PureTexture2D,
        .dim            = mtl::MaterialRecipe::Dim::D2,
        .l2w            = false,
        .pipeline  = GraphicsPipelinePreset::Solid2D,
        .textures  = {
            {mtl::SamplerSlot::BaseColor, mtl::TextureSourceMode::None, "res/image/lena.Tex2D"},
        },
    };

private:

    bool InitVBO()
    {

        quad_geometry = WorkObject::CreateGeometry("TextureQuad",
                                                   VERTEX_COUNT,
                                                   {{VAN::Position, VF_V2F, position_data},
                                                    {VAN::TexCoord, VF_V2F, tex_coord_data}});
        if (!quad_geometry)
            return false;

        return(true);
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        quad_entity = ecs_world->CreateEntity<Entity>("TextureQuad");
        auto quad_transform = quad_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto quad_primitive = quad_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        quad_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        quad_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        quad_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        quad_transform->SetMovable(false);

        quad_primitive->SetUnresolvedGeometry(quad_geometry);
        quad_primitive->SetMaterialRecipe(RegisterMaterialRecipe(kTexQuadCfg));
        quad_primitive->SetVisible(true);

        return true;
    }

public:
    bool Init() override
    {
        if(!InitVBO())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Draw a quad with texture"),argc,argv,256,256);
}



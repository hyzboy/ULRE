// 画一个带纹理的矩形，2D模式专用 (ECS)

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/mtl/MaterialLibrary.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateRectTexture2DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V2F},
            {VertexSemantic::TexCoord, VF_V2F},
        };
        return gvf;
    }
}

constexpr float position_data[12]=
{
    0,0,
    0,1,
    1,0,
    1,0,
    0,1,
    1,1
};

constexpr float tex_coord_data[12]=
{
    0,0,
    0,1,
    1,0,
    1,0,
    0,1,
    1,1
};

class TestApp:public WorkObject
{
private:

    ECSContext *        ecs_world           = nullptr;
    Entity *            rect_entity         = nullptr;

    Texture2D *         texture             = nullptr;
    Sampler *           sampler             = nullptr;
    graph::mtl::MaterialRecipe rect_recipe{};
    PrimitiveAsset      rect_asset{};
    std::unique_ptr<BindlessTextureManager> bindless_texture_manager;

private:

    bool InitMaterial()
    {
        auto* sampler_manager = GetManager<SamplerManager>();
        auto* tex_manager = GetManager<TextureManager>();
        if (!sampler_manager || !tex_manager )
            return false;

        texture=tex_manager->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"),true);

        if(!texture)return(false);

        sampler=sampler_manager->CreateSampler();

        return(true);
    }

    bool InitVBO()
    {
        auto* device = GetDevice();
        auto* buffer_manager = GetManager<BufferManager>();
        auto* geometry_manager = GetManager<GeometryManager>();
        if (!device || !buffer_manager || !geometry_manager)
            return false;

        GeometryCreater pc(device, CreateRectTexture2DGeometryVertexFormat(), buffer_manager);
        pc.Init("TextureRect", 6);
        if (!pc.WriteVAB(VAN::Position, VF_V2F, position_data) ||
            !pc.WriteVAB(VAN::TexCoord, VF_V2F, tex_coord_data))
            return false;

        auto* geometry = pc.Create();
        if (!geometry)
            return false;
        geometry_manager->Add(geometry);
        rect_recipe.recipe_name = "TextureRect.UnlitTexture";
        rect_recipe.mtl_def_id = "UnlitTexture";
        rect_recipe.pipeline_preset = PipelinePreset::Solid2D;
        rect_recipe.vertex_node_config = graph::mtl::Make2DNodeConfigZeroToOne(true);
        rect_recipe.domain = "TextureRect";
        rect_asset = PrimitiveAsset(geometry, &rect_recipe, PrimitiveType::Triangles);

        return(true);
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        rect_entity = ecs_world->CreateEntity<Entity>("TextureRect");
        auto rect_transform = rect_entity->AddComponent<TransformComponent>(Mobility::Static);
        auto rect_primitive = rect_entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        rect_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        rect_transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        rect_transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        rect_transform->SetMovable(false);

        rect_primitive->SetPrimitiveAsset(&rect_asset);
        rect_primitive->SetMaterialTextureResource(graph::mtl::TextureSlot::BaseColor, texture, sampler);
        rect_primitive->SetVisible(true);

        return true;
    }

public:
    bool Init() override
    {
        if(!InitMaterial())
            return(false);

        if(!InitVBO())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Draw a rectangle with texture"),argc,argv,256,256);
}

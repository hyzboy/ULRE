// 画一个带纹理的四边形 (ECS)
#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/SSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>

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
    GeometryVertexFormat CreatePureTexture2DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V2F},
            {VertexSemantic::TexCoord, VF_V2F},
        };
        return gvf;
    }
}

constexpr uint32_t VERTEX_COUNT=4;
constexpr uint32_t INDEX_COUNT=6;

constexpr float position_data[VERTEX_COUNT][2]=
{
    {-1, -1},
    { 1, -1},
    { 1,  1},
    {-1,  1},
};

constexpr float tex_coord_data[VERTEX_COUNT][2]=
{
    {0,0},
    {1,0},
    {1,1},
    {0,1}
};

// 两个三角形（绕序与原 Fan 装配一致——(0,1,2),(0,2,3)，顶点复用）
constexpr uint16_t index_data[INDEX_COUNT]=
{
    0, 1, 2,
    0, 2, 3,
};

class TestApp:public WorkObject
{
private:

    ECSContext *        ecs_world           = nullptr;
    Entity *            quad_entity         = nullptr;

    Texture2D *         texture             = nullptr;
    Sampler *           sampler             = nullptr;
    graph::SSBOArrayAccessor<ssbo::TextureLayerRow>* tex_row_accessor = nullptr;
    graph::mtl::MaterialRecipe quad_recipe{};
    PrimitiveAsset      quad_asset{};

private:

    bool InitMaterial()
    {
        auto* sampler_manager = GetManager<SamplerManager>();
        auto* tex_manager = GetManager<TextureManager>();
        if (!sampler_manager || !tex_manager)
            return false;

        texture=tex_manager->LoadTexture2D(OS_TEXT("res/image/lena.Tex2D"),true);

        if(!texture)return(false);

        // Arena+BDA：句柄行（TextureLayerRow）——基址由 Collect 镜像写 tex_tail
        if (auto *domain_manager = GetManager<SSBOBufferRegistry>())
        {
            tex_row_accessor = domain_manager->AllocateArrayAccessor<ssbo::TextureLayerRow>(
                "Example:TextureQuad:MaterialData", 1);
            if (!tex_row_accessor)
                return(false);
            (*tex_row_accessor)[0].tex_tail[graph::mtl::TextureSlot::BaseColor] = 0; //句柄由 Collect 镜像写入
            tex_row_accessor->Commit();
        }

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

        GeometryCreater pc(device, CreatePureTexture2DGeometryVertexFormat(), buffer_manager);
        pc.Init("TextureQuad", VERTEX_COUNT, INDEX_COUNT);
        if (!pc.WriteVAB(VAN::Position, VF_V2F, position_data) ||
            !pc.WriteVAB(VAN::TexCoord, VF_V2F, tex_coord_data) ||
            !pc.WriteIBO(index_data))
            return false;

        auto* geometry = pc.Create();
        if (!geometry)
            return false;
        geometry_manager->Add(geometry);
        quad_recipe.recipe_name = "TextureQuad.UnlitTexture";
        quad_recipe.mtl_def_id = "UnlitTexture";
        quad_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid2DConfig();
        quad_asset = PrimitiveAsset(geometry, &quad_recipe, PrimitiveType::Triangles);

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

        quad_primitive->SetPrimitiveAsset(&quad_asset);
        quad_primitive->SetMaterialTextureResource(graph::mtl::TextureSlot::BaseColor, texture, sampler);

        // Arena+BDA：纹理句柄经 TextureLayerRow 行尾下发（每材质实例一行）
        hgl::ecs::PrimitiveComponent::MaterialPrivateDataSlotAuthoringResource tex_struct{};
        tex_struct.material_private_data_slot_name = graph::mtl::DefaultMaterialPrivateDataSlotName;
        tex_struct.ssbo_id = tex_row_accessor->GetSSBOId();
        tex_struct.data_index = 0;
        tex_struct.use_data_index = true;
        tex_struct.shared_across_instances = false;
        quad_primitive->SetMaterialPrivateDataSlotResource(tex_struct);
        quad_primitive->SetVisible(true);

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
    return RunFramework<TestApp>(OS_TEXT("Draw a quad with texture"),argc,argv,256,256);
}

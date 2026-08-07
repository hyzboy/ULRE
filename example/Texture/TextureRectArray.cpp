// 画一个带纹理的矩形，2D模式专用 (ECS)

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/math/Vector.h>

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
    GeometryVertexFormat CreateRectTexture2DArrayGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V2F},
            {VertexSemantic::TexCoord, VF_V2F},
        };
        return gvf;
    }
}

constexpr const os_char *tex_filename[]=
{
    OS_TEXT("001-online resume.Tex2D"),
    OS_TEXT("002-salary.Tex2D"),
    OS_TEXT("003-application.Tex2D"),
    OS_TEXT("004-job interview.Tex2D")
};

constexpr const size_t TexCount=sizeof(tex_filename)/sizeof(os_char *);

constexpr const float rect_right=1.0f/float(TexCount);

constexpr const float position_data[12]=
{
    0,0,
    0,1,
    rect_right,0,
    rect_right,0,
    0,1,
    rect_right,1
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

    Texture2DArray *    texture             = nullptr;
    Sampler *           sampler             = nullptr;
    graph::mtl::MaterialRecipe rect_recipe{};
    PrimitiveAsset      rect_asset{};
    graph::SSBOArrayAccessor<hgl::math::Vector4u> * mtl_data_ssbo_accessor = nullptr;
    std::unique_ptr<BindlessTextureManager> bindless_texture_manager;

    struct
    {
        Entity *entity;
    }render_obj[TexCount]{};

private:

    bool InitTexture()
    {
        auto* tex_manager = GetManager<TextureManager>();
        if (!tex_manager)
            return false;

        texture = tex_manager->CreateTexture2DArray("freepik icons",
                                512,512,            ///<纹理尺寸
                                TexCount,           ///<纹理层数
                                PF_BC7UN,           ///<纹理格式
                                false);             ///<是否自动产生mipmaps

        if(!texture)return(false);

        OSString filename;

        for(uint i=0;i<TexCount;i++)
        {
            filename=filesystem::JoinPathWithFilename(OS_TEXT("res/image/icon/freepik"),tex_filename[i]);

            if(!tex_manager->LoadTexture2DArray(texture,i,filename))
                return(false);
        }

        return(true);
    }

    bool InitMaterial()
    {
        auto* sampler_manager = GetManager<SamplerManager>();
        if (!sampler_manager)
            return false;

        auto *domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return false;

        sampler=sampler_manager->CreateSampler();

        mtl_data_ssbo_accessor = domain_manager->AllocateArrayAccessor<hgl::math::Vector4u>(
            graph::mtl::SSBOType::TextureRectArraySurface,
            "TextureRectArray:MaterialData",
            TexCount);
        if (!mtl_data_ssbo_accessor)
            return false;

        for (uint32_t i = 0; i < TexCount; ++i)
            (*mtl_data_ssbo_accessor)[i] = hgl::math::Vector4u{i, 0u, 0u, 0u};
        mtl_data_ssbo_accessor->Commit();

        rect_recipe.recipe_name = "TextureRectArray.Texture2DArray";
        rect_recipe.mtl_def_id = "Texture2DArray";
        rect_recipe.pipeline_config = mtl::MakeSolid2DConfig();
        rect_recipe.vertex_node_config = graph::mtl::Make2DNodeConfigZeroToOne(true);
        rect_recipe.domain = "TextureRectArray";
        graph::mtl::UpsertRecipeSSBOAssetBinding(rect_recipe,
                                                 graph::mtl::DefaultMaterialDataSlotName,
                                                 mtl_data_ssbo_accessor->GetSSBOBinding());

        return(true);
    }

    bool InitVBOAndRenderList()
    {
        auto* device = GetDevice();
        auto* buffer_manager = GetManager<BufferManager>();
        auto* geometry_manager = GetManager<GeometryManager>();
        if (!device || !buffer_manager || !geometry_manager)
            return false;

        GeometryCreater pc(device, CreateRectTexture2DArrayGeometryVertexFormat(), buffer_manager);
        pc.Init("TextureRect", 6);
        if (!pc.WriteVAB(VAN::Position, VF_V2F, position_data) ||
            !pc.WriteVAB(VAN::TexCoord, VF_V2F, tex_coord_data))
            return false;

        auto* geometry = pc.Create();
        if (!geometry)
            return false;
        geometry_manager->Add(geometry);
        rect_asset = PrimitiveAsset(geometry, &rect_recipe, PrimitiveType::Triangles);

        return(true);
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        math::Vector3f offset(1.0f/float(TexCount),0,0);

        for(uint32_t i=0;i<TexCount;i++)
        {
            offset.x=rect_right*2*float(i);

            render_obj[i].entity = ecs_world->CreateEntity<Entity>("TextureRect");
            auto transform = render_obj[i].entity->AddComponent<TransformComponent>(Mobility::Static);
            auto primitive = render_obj[i].entity->AddComponent<hgl::ecs::PrimitiveComponent>();

            transform->SetLocalPosition(glm::vec3(offset.x, offset.y, offset.z));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);

            primitive->SetPrimitiveAsset(&rect_asset);
            primitive->SetMaterialTextureResource(graph::mtl::TextureSlot::BaseColor,
                                                  texture,
                                                  sampler,
                                                  PrimitiveComponent::MaterialTextureResourceKind::Texture2DArray);
            hgl::ecs::PrimitiveComponent::MaterialDataSlotAuthoringResource rect_struct{};
            rect_struct.data_slot_name = graph::mtl::DefaultMaterialDataSlotName;
            rect_struct.ssbo_id = mtl_data_ssbo_accessor->GetSSBOId();
            rect_struct.data_index = i;
            rect_struct.use_data_index = true;
            rect_struct.shared_across_instances = false;
            primitive->SetMaterialDataSlotResource(rect_struct);
            primitive->SetVisible(true);
        }

        return true;
    }

public:
    TestApp() = default;
    explicit TestApp(std::shared_ptr<ecs::ECSContext> ctx) : WorkObject(std::move(ctx)) {}
    ~TestApp()
    {
        SAFE_CLEAR(mtl_data_ssbo_accessor)
    }
    bool Init() override
    {
        if(!InitTexture())
            return(false);

        if(!InitMaterial())
            return(false);

        if(!InitVBOAndRenderList())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Draw many rectangle with texture"),argc,argv,256*TexCount,256);
}

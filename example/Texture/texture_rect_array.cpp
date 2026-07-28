// 画一个带纹理的矩形，2D模式专用 (ECS)

#include<hgl/framework/WorkManager.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>

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
    constexpr uint32_t kTextureRectArraySsboId = hgl::graph::mtl::MakeRecipeSSBOId(8201);

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
    DeviceBuffer *      mi_ssbo             = nullptr;
    std::unique_ptr<BindlessTextureManager> bindless_texture_manager;
    graph::mtl::SSBOType material_ssbo_type = graph::mtl::SSBOType::PBRSurface;
    uint32_t material_ssbo_id = kTextureRectArraySsboId;
    uint32_t material_ssbo_count = 0;
    uint32_t material_ssbo_stride = sizeof(uint32_t);

    struct
    {
        Entity *entity;
    }render_obj[TexCount]{};

private:

    bool InitTexture()
    {
        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

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
        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* sampler_manager = GetManager<SamplerManager>();
        if (!sampler_manager)
            return false;

        auto *domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return false;

        sampler=sampler_manager->CreateSampler();

        material_ssbo_count = TexCount;
        mi_ssbo = domain_manager->EnsureBuffer(graph::mtl::SSBOAddress{material_ssbo_type, material_ssbo_id, 0},
                                               "TextureRectArray:MIData",
                                               VkDeviceSize(TexCount) * material_ssbo_stride,
                                               material_ssbo_count,
                                               SharingMode::Exclusive);
        if (!mi_ssbo)
            return false;

        auto *gpu_buf = mi_ssbo->GetGPUBuffer();
        if (!gpu_buf)
            return false;

        auto *dst = static_cast<uint8_t *>(gpu_buf->Map(0, VkDeviceSize(TexCount) * material_ssbo_stride));
        if (!dst)
            return false;

        memset(dst, 0, size_t(TexCount) * material_ssbo_stride);
        for (uint32_t i = 0; i < TexCount; ++i)
        {
            memcpy(dst + VkDeviceSize(i) * material_ssbo_stride, &i, sizeof(uint32_t));
        }
        gpu_buf->Unmap();

        rect_recipe.recipe_name = "TextureRectArray.RectTexture2DArray";
        rect_recipe.shading_model = graph::mtl::ShadingModel::Unlit;
        rect_recipe.preset_hint = static_cast<uint32_t>(graph::mtl::MaterialPreset::RectTexture2DArray);
        rect_recipe.coordinate_system_2d = graph::CoordinateSystem2D::ZeroToOne;
        rect_recipe.local_to_world_2d = true;
        rect_recipe.domain = "TextureRectArray";
        graph::mtl::UpsertRecipeSSBOAssetBinding(rect_recipe,
                                                 graph::mtl::SBS_MaterialInstance.name,
                                                 material_ssbo_type,
                                                 material_ssbo_id);

        return(true);
    }

    bool InitVBOAndRenderList()
    {
        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* device = graphics_context->GetDevice();
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
            hgl::ecs::PrimitiveComponent::MaterialStructNamedAuthoringResource rect_struct{};
            rect_struct.ssbo_name = graph::mtl::SBS_MaterialInstance.name;
            rect_struct.ssbo_id = material_ssbo_id;
            rect_struct.struct_index = i;
            rect_struct.use_struct_index = true;
            rect_struct.shared_across_instances = false;
            primitive->SetMaterialStructResource(rect_struct);
            primitive->RequestPipeline(InlinePipeline::Solid2D);
            primitive->SetVisible(true);
        }

        return true;
    }

public:
    TestApp() = default;
    explicit TestApp(std::shared_ptr<ecs::ECSContext> ctx) : WorkObject(std::move(ctx)) {}
    ~TestApp()
    {
        SAFE_CLEAR(mi_ssbo)
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

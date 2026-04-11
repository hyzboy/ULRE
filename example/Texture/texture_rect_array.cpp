// 画一个带纹理的矩形，2D模式专用 (ECS)

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/SamplerManager.h>

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
    MaterialTemplate *          material            = nullptr;

    Primitive *         mesh_rect           = nullptr;  // kept for geometry lifecycle

    struct
    {
        Entity *            entity    = nullptr;
        PrimitiveMaterialSlot slot;
        Primitive *         primitive = nullptr;  // one per entity for distinct MIT layer
    }render_obj[TexCount]{};

private:

    bool InitTexture()
    {

        auto* tex_manager = GetTextureManager();
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

        auto* sampler_manager = GetSamplerManager();
        if (!sampler_manager)
            return false;

        static const mtl::MaterialAssetRecord kTexArrayCfg {
            .id              = "texture_rect_array",
            .preset          = mtl::MaterialPreset::PureTexture2D,
            .dim             = mtl::MaterialAssetRecord::Dim::D2,
            .coord_2d  = CoordinateSystem2D::ZeroToOne,
            .pipeline  = GraphicsPipelinePreset::Solid2D,
            .textures  = {
                {mtl::SamplerSlot::BaseColor, mtl::TextureSourceMode::Array, ""},
            },
        };
        auto* registry = GetMaterialAssetRegistry();
        auto* material_manager = GetMaterialManager();
        if(!registry || !material_manager)
            return false;

        auto handle = registry->Acquire(kTexArrayCfg);
        if(!handle.IsValid())
            return false;

        const VIL *resolved_vil = registry->ResolveVIL(handle.material, kTexArrayCfg);
        if(!resolved_vil)
            return false;

        material = handle.material;

        sampler=sampler_manager->CreateSampler();

        if(!material->BindTextureSampler( mtl::SamplerSlot::BaseColor,
                          texture,
                          sampler))
            return(false);

        for(uint32_t i=0;i<TexCount;i++)
        {
            render_obj[i].slot = material_manager->AllocMaterialInstanceSlot(
                handle.domain,
                handle.material,
                resolved_vil,
                kTexArrayCfg.pipeline);

            if(!render_obj[i].slot.IsValid())
                return false;
        }

        return(true);
    }

    bool InitVBOAndRenderList()
    {

        auto* device = GetDevice();
        auto* buffer_manager = GetBufferManager();
        auto* geometry_manager = GetGeometryManager();
        auto* primitive_manager = GetPrimitiveManager();
        if (!device || !buffer_manager || !geometry_manager || !primitive_manager)
            return false;

        GeometryCreater pc(device, MakeGeometryVertexFormatMap(render_obj[0].slot.vil), buffer_manager);
        pc.Init("TextureRect", 6);
        if (!pc.WriteVAB(VAN::Position, VF_V2F, position_data) ||
            !pc.WriteVAB(VAN::TexCoord, VF_V2F, tex_coord_data))
            return false;

        auto* geometry = pc.Create();
        if (!geometry)
            return false;
        geometry_manager->Add(geometry);

        mesh_rect = primitive_manager->CreatePrimitive(geometry, render_obj[0].slot);

        if(!mesh_rect)
            return(false);

        // Array 模式：通过 Primitive 的 MIT 写入纹理层索引
        mesh_rect->SetTextureArrayLayer(mtl::SamplerSlot::BaseColor, 0);

        for(uint32_t i = 1; i < TexCount; ++i)
        {
            render_obj[i].primitive = primitive_manager->CreatePrimitive(geometry, render_obj[i].slot);
            if(!render_obj[i].primitive)
                return false;

            render_obj[i].primitive->SetTextureArrayLayer(mtl::SamplerSlot::BaseColor, i);
        }
        render_obj[0].primitive = mesh_rect;

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

            primitive->SetPrimitive(render_obj[i].primitive);
            primitive->SetVisible(true);
        }

        return true;
    }

public:
    TestApp() = default;
    explicit TestApp(std::shared_ptr<ecs::ECSContext> ctx) : WorkObject(std::move(ctx)) {}
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



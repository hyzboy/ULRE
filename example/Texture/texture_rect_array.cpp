// 画一个带纹理的矩形，2D模式专用 (ECS)

#include<hgl/framework/WorkManager.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/DescriptorBindingSet.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>

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
    MaterialProgram *          material            = nullptr;
    Primitive *         mesh_rect           = nullptr;
    DeviceBuffer *      mi_ssbo             = nullptr;
    std::unique_ptr<BindlessTextureManager> bindless_texture_manager;

    struct
    {
        Entity *              entity;
        DescriptorBindingSet *dbs;
    }render_obj[TexCount]{};

private:

    bool InitTexture()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* tex_manager = graphics_context->GetTextureManager();
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
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        auto* sampler_manager = graphics_context->GetSamplerManager();
        auto* device = graphics_context->GetDevice();
        if (!material_manager || !sampler_manager || !device)
            return false;

        mtl::Material2DCreateConfig cfg(PrimitiveType::Triangles,
                                        CoordinateSystem2D::ZeroToOne,
                                        mtl::WithLocalToWorld::With);

        material=material_manager->AcquireMaterialProgram(mtl::MaterialPreset::RectTexture2DArray,&cfg);

        if(!material)
            return(false);

        if (!bindless_texture_manager)
        {
            bindless_texture_manager = std::make_unique<BindlessTextureManager>();
            if (!bindless_texture_manager->Init(VkDevice(*device)))
                return false;

            render_context->SetBindlessTextureManager(bindless_texture_manager.get());
            material_manager->SetBindlessLayout(bindless_texture_manager->GetLayout());
        }

        auto *buffer_manager = graphics_context->GetBufferManager();
        auto *domain_manager = graphics_context->GetResourceDomainManager();
        if (!buffer_manager || !domain_manager)
            return false;

        sampler=sampler_manager->CreateSampler();

        // Build SSBO: each slot holds the per-instance data (layer index or similar)
        const uint32_t stride = material->GetMIDataBytes();
        if (stride > 0)
        {
            mi_ssbo = buffer_manager->CreateSSBO("TextureRectArray:MIData",
                                                  VkDeviceSize(TexCount) * stride,
                                                  nullptr, SharingMode::Exclusive);
            if (!mi_ssbo)
                return false;

            auto *gpu_buf = mi_ssbo->GetGPUBuffer();
            if (!gpu_buf)
                return false;

            auto *dst = static_cast<uint8_t *>(gpu_buf->Map(0, VkDeviceSize(TexCount) * stride));
            if (!dst)
                return false;

            memset(dst, 0, size_t(TexCount) * stride);
            // Write layer index i at slot i (matches original WriteMIData(i) intent).
            const uint32_t copy_bytes = hgl_min(stride, static_cast<uint32_t>(sizeof(uint32_t)));
            for (uint32_t i = 0; i < TexCount; ++i)
            {
                memcpy(dst + VkDeviceSize(i) * stride, &i, copy_bytes);
            }
            gpu_buf->Unmap();

            for (const auto &req : material->GetMaterialResourceLayout().requirements)
            {
                if (req.semantic != mtl::DescriptorSemantic::MaterialInstance)
                    continue;

                const mtl::SSBOAddress addr{req.ssbo_type, req.ssbo_id, 0};
                if (!domain_manager->RegisterBuffer(addr, mi_ssbo, TexCount))
                    return false;

                for (uint32_t i = 0; i < TexCount; ++i)
                {
                    render_obj[i].dbs = new DescriptorBindingSet(material);
                    if (!render_obj[i].dbs)
                        return false;
                    render_obj[i].dbs->SetSSBOBinding(req.ssbo_type, req.ssbo_id, i);
                }
            }
        }
        else
        {
            // No per-instance data — create plain DBS per slot
            for (uint32_t i = 0; i < TexCount; ++i)
            {
                render_obj[i].dbs = new DescriptorBindingSet(material);
                if (!render_obj[i].dbs)
                    return false;
            }
        }

        return(true);
    }

    bool InitVBOAndRenderList()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* device = graphics_context->GetDevice();
        auto* buffer_manager = graphics_context->GetBufferManager();
        auto* geometry_manager = graphics_context->GetGeometryManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!device || !buffer_manager || !geometry_manager || !primitive_manager)
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

        mesh_rect = primitive_manager->CreatePrimitive(geometry,
                                                       material,
                                                       render_obj[0].dbs,
                                                       nullptr);

        if(!mesh_rect)
            return(false);

        return(true);
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        auto rdbs = ecs_world->GetSystem<RenderDescriptorBindingSystem>();
        if (!rdbs)
            return false;

        auto* render_context = GetRenderContext();
        auto* bindless_mgr = render_context ? render_context->GetBindlessTextureManager() : nullptr;
        if (!bindless_mgr)
            return false;

        if (rdbs->RegisterTexture2DArrayResource("", texture, sampler, bindless_mgr) == 0)
            return false;
        if (!rdbs->RegisterMaterialTextureSampler(material, mtl::SamplerName::BaseColor, texture, sampler))
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

            primitive->SetPrimitive(mesh_rect);
            primitive->SetDescriptorBindingSet(render_obj[i].dbs);
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
        for (auto &obj : render_obj)
        {
            delete obj.dbs;
            obj.dbs = nullptr;
        }
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

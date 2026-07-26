#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/vk/VKVertexInputLayout.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/DescriptorBindingSet.h>
#include<hgl/color/Color.h>
#include<cstring>
#include<hgl/math/geometry/AABB.h>
#include<hgl/type/StdString.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>

#include<memory>
#include<string>
#include<vector>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    GeometryVertexFormat CreateGeometryVertexFormatFromVIL(const VIL *vil)
    {
        GeometryVertexFormat gvf;
        if(!vil)
            return gvf;

        const uint32_t count = vil->GetVertexAttribCount();
        const VertexInputFormat *vif_list = vil->GetVIFList();

        for(uint32_t i=0;i<count;i++)
        {
            const VertexInputFormat &vif = vif_list[i];
            gvf.Add(vif.semantic, vif.format, uint8_t(vif.vec_size), uint32_t(vif.stride));
        }

        return gvf;
    }

    GeometryVertexFormat CreatePureColor3DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf;
        gvf.Add(VertexSemantic::Position, VF_V3F, 3, sizeof(float) * 3);
        return gvf;
    }
}

namespace hgl::graph{
Geometry *LoadGeometry(VulkanDevice *device,const GeometryVertexFormat &geometry_vertex_format,const OSString &filename);
}//namespace hgl::graph

constexpr const COLOR TestColor[] =
{
    COLOR::MozillaCharcoal,
    COLOR::MozillaSand,

    COLOR::BlenderAxisRed,
    COLOR::BlenderAxisGreen,
    COLOR::BlenderAxisBlue,

    COLOR::BananaYellow,
    COLOR::CherryBlossomPink,

    COLOR::SkyBlue,
};

constexpr const size_t COLOR_COUNT = sizeof(TestColor) / sizeof(COLOR);

class TestApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    struct MaterialData
    {
        MaterialProgram *material = nullptr;
        const VIL *vil = nullptr;
        GeometryVertexFormat geometry_vertex_format;

        DescriptorBindingSet *dbs[COLOR_COUNT]{};
        graph::DeviceBuffer *mi_ssbo = nullptr;

        void FreeDBS()
        {
            for (auto *&b : dbs)
            {
                delete b;
                b = nullptr;
            }
        }
    };

    MaterialData solid;
    MaterialData wire;

    struct RenderMesh
    {
        Geometry *geometry;
        Primitive *primitive;

        hgl::ecs::Entity *entity = nullptr;
        std::shared_ptr<hgl::ecs::TransformComponent> transform;
        std::shared_ptr<hgl::ecs::PrimitiveComponent> primitive_comp;

    public:

        ~RenderMesh()
        {
            delete primitive;
            delete geometry;
        }
    };

    struct BoundingBoxMesh
    {
        hgl::ecs::Entity *entity = nullptr;
        std::shared_ptr<hgl::ecs::TransformComponent> transform;
        std::shared_ptr<hgl::ecs::PrimitiveComponent> primitive_comp;
    };

    std::vector<std::unique_ptr<RenderMesh>> render_mesh;
    std::vector<std::unique_ptr<BoundingBoxMesh>> bounding_boxes;

    Geometry *bbox_geometry = nullptr;
    Primitive *bbox_primitive = nullptr;

private:

    bool InitMaterialForDBS(MaterialData *md, const char *tag)
    {
        if (!md || !md->material)
            return false;

        auto *render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto *graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto *buffer_manager = graphics_context->GetBufferManager();
        auto *domain_manager = graphics_context->GetResourceDomainManager();
        if (!buffer_manager || !domain_manager)
            return false;

        md->vil = md->material->GetDefaultVIL();
        if (!md->vil)
            return false;

        md->geometry_vertex_format = CreateGeometryVertexFormatFromVIL(md->vil);
        if (md->geometry_vertex_format.GetCount() == 0)
            return false;

        const uint32_t stride      = md->material->GetMIDataBytes();
        const uint32_t color_count = static_cast<uint32_t>(COLOR_COUNT);

        if (stride > 0)
        {
            const VkDeviceSize ssbo_size = VkDeviceSize(color_count) * stride;
            md->mi_ssbo = buffer_manager->CreateSSBO(tag, ssbo_size, nullptr, SharingMode::Exclusive);
            if (!md->mi_ssbo)
                return false;

            auto *gpu_buf = md->mi_ssbo->GetGPUBuffer();
            if (!gpu_buf)
                return false;

            auto *dst = static_cast<uint8_t *>(gpu_buf->Map(0, ssbo_size));
            if (!dst)
                return false;

            memset(dst, 0, static_cast<size_t>(ssbo_size));
            const uint32_t copy_bytes = hgl_min(stride, static_cast<uint32_t>(sizeof(Color4f)));
            for (uint32_t i = 0; i < color_count; ++i)
            {
                const Color4f color = GetColor4f(TestColor[i], 1.0f);
                memcpy(dst + VkDeviceSize(i) * stride, &color, copy_bytes);
            }
            gpu_buf->Unmap();

            for (const auto &req : md->material->GetMaterialResourceLayout().requirements)
            {
                if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                    continue;

                const graph::mtl::SSBOAddress addr{req.ssbo_type, req.ssbo_id, 0};
                if (!domain_manager->RegisterBuffer(addr, md->mi_ssbo, color_count))
                    return false;

                for (uint32_t c = 0; c < color_count; ++c)
                {
                    md->dbs[c] = new DescriptorBindingSet(md->material);
                    if (!md->dbs[c])
                        return false;
                    md->dbs[c]->SetSSBOBinding(req.ssbo_type, req.ssbo_id, c);
                }
            }
        }

        // Fallback: DBS with no SSBO binding (vertex-color-only materials)
        for (uint32_t c = 0; c < color_count; ++c)
        {
            if (!md->dbs[c])
                md->dbs[c] = new DescriptorBindingSet(md->material);
        }

        return true;
    }

    bool InitSolidMDP()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return false;

        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);
        solid.material = material_manager->AcquireMaterialProgram(mtl::MaterialPreset::Gizmo3D,&cfg);

        return InitMaterialForDBS(&solid, "LoadGeometry:SolidMIData");
    }

    bool InitWireMDP()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return false;

        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);
        wire.material=material_manager->AcquireMaterialProgram(mtl::MaterialPreset::PureColor3D,&cfg);

        return InitMaterialForDBS(&wire, "LoadGeometry:WireMIData");
    }

    bool CreateBoundingBoxMesh()
    {
        using namespace inline_geometry;

        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* device = graphics_context->GetDevice();
        auto* geometry_manager = graphics_context->GetGeometryManager();
        if (!device || !geometry_manager)
            return false;

        auto pc = std::make_unique<GeometryCreater>(device, CreatePureColor3DGeometryVertexFormat());

        inline_geometry::BoundingBoxCreateInfo bbci;

        bbox_geometry = CreateBoundingBox(pc.get(),&bbci);
        if(!bbox_geometry)
            return false;

        geometry_manager->Add(bbox_geometry);
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!primitive_manager)
            return false;

        bbox_primitive = primitive_manager->CreatePrimitive(bbox_geometry,
                                                            wire.material,
                                                            wire.dbs[5],
                                                            nullptr);
        return bbox_primitive != nullptr;
    }

    RenderMesh *CreateRenderMesh(Geometry *geometry,MaterialData *md,const int color)
    {
        if(!geometry)
            return(nullptr);

        auto* render_context = GetRenderContext();
        if (!render_context)
            return nullptr;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return nullptr;

        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!primitive_manager)
            return nullptr;

        Primitive *primitive = primitive_manager->CreatePrimitive(geometry,
                                                                  md->material,
                                                                  md->dbs[color],
                                                                  nullptr);

        if(!primitive)
            return nullptr;

        auto rm = std::make_unique<RenderMesh>();
        rm->geometry = geometry;
        rm->primitive = primitive;

        RenderMesh *result = rm.get();
        render_mesh.push_back(std::move(rm));
        return result;
    }

    bool CreateGeometryMesh()
    {
        int count=0;
        const GeometryVertexFormat &geometry_vertex_format = solid.geometry_vertex_format;

        for(int i=0;i< COLOR_COUNT;i++)
        {
            OSString fn = OSString(OS_TEXT("res/model/Chess/ABeautifulGame.")) + OSString::numberOf(i) + OS_TEXT(".geometry");

            Geometry *geo = LoadGeometry(GetDevice(),geometry_vertex_format,fn);

            if(!geo)
                continue;

            RenderMesh *rm=CreateRenderMesh(geo,&solid,i);

            if(!rm)
            {
                delete geo;
                continue;
            }

            ++count;
        }

        return(count>0);
    }

    bool InitBoundingBoxScene()
    {
        if(!bbox_primitive)
            return false;

        for(size_t i = 0; i < render_mesh.size(); ++i)
        {
            auto *rm = render_mesh[i].get();
            if(!rm || !rm->entity || !rm->primitive_comp)
                continue;

            hgl::math::AABB local_aabb;
            if(!rm->primitive_comp->GetLocalAABB(local_aabb))
                continue;

            auto bbox = std::make_unique<BoundingBoxMesh>();
            bbox->entity = ecs_context->CreateEntity<hgl::ecs::Entity>("BBox_" + std::to_string(i));
            bbox->transform = bbox->entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
            bbox->primitive_comp = bbox->entity->AddComponent<hgl::ecs::PrimitiveComponent>();

            bbox->transform->SetParent(rm->entity->GetID());

            const auto &center = local_aabb.GetCenter();
            const auto &size = local_aabb.GetLength();

            bbox->transform->SetLocalPosition(glm::vec3(center.x, center.y, center.z));
            bbox->transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            bbox->transform->SetLocalScale(glm::vec3(size.x, size.y, size.z));
            bbox->transform->SetMovable(false);

            bbox->primitive_comp->SetPrimitive(bbox_primitive);
            bbox->primitive_comp->SetDescriptorBindingSet(wire.dbs[i % COLOR_COUNT]);
            bbox->primitive_comp->RequestPipeline(InlinePipeline::Solid3D);
            bbox->primitive_comp->SetVisible(true);

            bounding_boxes.push_back(std::move(bbox));
        }

        return true;
    }

    bool InitScene()
    {
        if(!ecs_context)
            return false;

        const size_t mesh_count = render_mesh.empty() ? 1 : render_mesh.size();

        for(size_t i = 0; i < render_mesh.size(); ++i)
        {
            auto *rm = render_mesh[i].get();
            if(!rm || !rm->primitive)
                continue;

            rm->entity = ecs_context->CreateEntity<hgl::ecs::Entity>("Mesh_" + std::to_string(i));
            rm->transform = rm->entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
            rm->primitive_comp = rm->entity->AddComponent<hgl::ecs::PrimitiveComponent>();

            const float angle = glm::radians(360.0f * static_cast<float>(i) / static_cast<float>(mesh_count));
            const glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            const glm::vec3 pos = glm::rotate(rotation, glm::vec3(0.25f, 0.0f, 0.0f));

            rm->transform->SetLocalPosition(pos);
            rm->transform->SetLocalRotation(rotation);
            rm->transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            rm->transform->SetMovable(false);

            rm->primitive_comp->SetPrimitive(rm->primitive);
            rm->primitive_comp->SetDescriptorBindingSet(solid.dbs[i % COLOR_COUNT]);
            rm->primitive_comp->RequestPipeline(InlinePipeline::Solid3D);
            rm->primitive_comp->SetVisible(true);
        }

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<hgl::ecs::CameraComponent>();

        camera->control_mode = hgl::ecs::CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 8.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<hgl::graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();

        if(!ecs_context)
            return false;

        if(!InitScene())
            return false;

        if(!InitBoundingBoxScene())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

public:
    ~TestApp()
    {
        delete bbox_primitive;
        delete bbox_geometry;
        solid.FreeDBS();
        wire.FreeDBS();
        SAFE_CLEAR(wire.mi_ssbo)
        SAFE_CLEAR(solid.mi_ssbo)
    }

    bool Init() override
    {
        if(!InitSolidMDP())
            return(false);

        if(!InitWireMDP())
            return(false);

        if(!CreateGeometryMesh())
            return(false);

        if(!CreateBoundingBoxMesh())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class TestApp

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Load Geometry"),argc,argv,1280,720);
}


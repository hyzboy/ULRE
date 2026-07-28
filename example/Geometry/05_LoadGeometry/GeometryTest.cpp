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
#include<hgl/mtl/MaterialRecipe.h>
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
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
        };
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
        graph::SSBOArrayAccessor<Color4f>* mi_ssbo_accessor = nullptr;
        uint32_t ssbo_count = 0;

        ~MaterialData()
        {
            delete mi_ssbo_accessor;
            mi_ssbo_accessor = nullptr;
        }
    };

    MaterialData solid;
    MaterialData wire;

    struct RenderMesh
    {
        Geometry *geometry;
        Primitive *primitive;
        uint32_t color_index = 0;

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

    bool InitMaterialRuntimeData(MaterialData *md, const char *tag)
    {
        if (!md || !md->material)
            return false;

        auto *domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return false;

        md->vil = md->material->GetDefaultVIL();
        if (!md->vil)
            return false;

        md->geometry_vertex_format = CreateGeometryVertexFormatFromVIL(md->vil);
        if (md->geometry_vertex_format.GetCount() == 0)
            return false;

        if (md->material->GetMIDataBytes() > 0)
        {
            const uint32_t color_count = static_cast<uint32_t>(COLOR_COUNT);
            bool has_struct_binding = false;
            graph::mtl::SSBOType ssbo_type = graph::mtl::SSBOType::UserDefined;
            for (const auto &req : md->material->GetMaterialResourceLayout().requirements)
            {
                if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                    continue;

                has_struct_binding = true;
                ssbo_type = req.ssbo_type;
                break;
            }
            if (!has_struct_binding)
                return false;

            md->ssbo_count = color_count;
            md->mi_ssbo_accessor = domain_manager->AllocateArrayAccessor<Color4f>(
                ssbo_type,
                tag,
                color_count);
            if (!md->mi_ssbo_accessor)
                return false;

            for (uint32_t i = 0; i < color_count; ++i)
                (*md->mi_ssbo_accessor)[i] = GetColor4f(TestColor[i], 1.0f);
            md->mi_ssbo_accessor->Commit();
        }

        return true;
    }

    bool InitSolidMDP()
    {
        auto* material_manager = GetManager<MaterialManager>();
        if (!material_manager)
            return false;

        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);
        const auto acquire_mtl3d = static_cast<MaterialProgram *(MaterialManager::*)(const mtl::MaterialPreset, mtl::Material3DCreateConfig *)>(&MaterialManager::AcquireMaterialProgram);
        solid.material = (material_manager->*acquire_mtl3d)(mtl::MaterialPreset::Gizmo3D, &cfg);

        return InitMaterialRuntimeData(&solid, "LoadGeometry:SolidMIData");
    }

    bool InitWireMDP()
    {
        auto* material_manager = GetManager<MaterialManager>();
        if (!material_manager)
            return false;

        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);
        const auto acquire_mtl3d = static_cast<MaterialProgram *(MaterialManager::*)(const mtl::MaterialPreset, mtl::Material3DCreateConfig *)>(&MaterialManager::AcquireMaterialProgram);
        wire.material = (material_manager->*acquire_mtl3d)(mtl::MaterialPreset::PureColor3D, &cfg);

        return InitMaterialRuntimeData(&wire, "LoadGeometry:WireMIData");
    }

    bool CreateBoundingBoxMesh()
    {
        using namespace inline_geometry;

        auto* device = GetDevice();
        auto* geometry_manager = GetManager<GeometryManager>();
        if (!device || !geometry_manager)
            return false;

        auto pc = std::make_unique<GeometryCreater>(device, CreatePureColor3DGeometryVertexFormat());

        inline_geometry::BoundingBoxCreateInfo bbci;

        bbox_geometry = CreateBoundingBox(pc.get(),&bbci);
        if(!bbox_geometry)
            return false;

        geometry_manager->Add(bbox_geometry);
        auto* primitive_manager = GetManager<PrimitiveManager>();
        if (!primitive_manager)
            return false;

        bbox_primitive = primitive_manager->CreatePrimitive(bbox_geometry,
                                                            wire.material,
                                                            nullptr,
                                                            nullptr);
        return bbox_primitive != nullptr;
    }

    RenderMesh *CreateRenderMesh(Geometry *geometry,MaterialData *md,const int color)
    {
        if(!geometry)
            return(nullptr);

        auto* primitive_manager = GetManager<PrimitiveManager>();
        if (!primitive_manager)
            return nullptr;

        Primitive *primitive = primitive_manager->CreatePrimitive(geometry,
                                                                  md->material,
                                                                  nullptr,
                                                                  nullptr);

        if(!primitive)
            return nullptr;

        auto rm = std::make_unique<RenderMesh>();
        rm->geometry = geometry;
        rm->primitive = primitive;
        rm->color_index = static_cast<uint32_t>(color);

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
            graph::mtl::MaterialRecipe recipe{};
            recipe.recipe_name = "LoadGeometry.Wire";
            recipe.shading_model = graph::mtl::ShadingModel::Unlit;
            recipe.preset_hint = static_cast<uint32_t>(graph::mtl::MaterialPreset::PureColor3D);
            recipe.domain = "LoadGeometry";
            bbox->primitive_comp->SetMaterialRecipe(recipe);
            hgl::ecs::PrimitiveComponent::MaterialStructNamedAuthoringResource bbox_struct{};
            bbox_struct.ssbo_name = graph::mtl::SBS_MaterialInstance.name;
            bbox_struct.ssbo_id = wire.mi_ssbo_accessor->GetSSBOId();
            bbox_struct.struct_index = static_cast<uint32_t>(i % COLOR_COUNT);
            bbox_struct.use_struct_index = true;
            bbox_struct.shared_across_instances = true;
            bbox->primitive_comp->SetMaterialStructResource(bbox_struct);
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
            graph::mtl::MaterialRecipe recipe{};
            recipe.recipe_name = "LoadGeometry.Gizmo3D";
            recipe.shading_model = graph::mtl::ShadingModel::Unlit;
            recipe.preset_hint = static_cast<uint32_t>(graph::mtl::MaterialPreset::Gizmo3D);
            recipe.domain = "LoadGeometry";
            rm->primitive_comp->SetMaterialRecipe(recipe);
            hgl::ecs::PrimitiveComponent::MaterialStructNamedAuthoringResource mesh_struct{};
            mesh_struct.ssbo_name = graph::mtl::SBS_MaterialInstance.name;
            mesh_struct.ssbo_id = solid.mi_ssbo_accessor->GetSSBOId();
            mesh_struct.struct_index = rm->color_index;
            mesh_struct.use_struct_index = true;
            mesh_struct.shared_across_instances = true;
            rm->primitive_comp->SetMaterialStructResource(mesh_struct);
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

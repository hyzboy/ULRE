// NOTE (test-only):
// This sample validates the MaterialRecipe authoring entry on PrimitiveComponent.
// It is NOT the final production authoring/runtime pipeline.
//
// Planned production path:
// 1) MaterialRecipe is authored at top-level material definitions.
// 2) ECS/GPUSCENE resolves + materializes runtime data automatically.
// 3) Sample/app code should not manually own final runtime material binding.
//
// 该范例主要演示使用ECS架构绘制一个立方体，并验证 PrimitiveComponent 的 MaterialRecipe 入口。
// This example demonstrates rendering a cube with ECS and validating PrimitiveComponent MaterialRecipe ingress.
//
// 本范例展示了：
// 1. 使用ECS架构创建立方体实体
// 2. 使用TransformComponent管理空间变换
// 3. 使用PrimitiveComponent管理渲染图元
// 4. CameraSystem配置为ViewModel控制模式

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/DescriptorBindingSet.h>
#include<hgl/graph/SSBOSlotAllocator.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/mtl/MaterialRecipe.h>

#include<hgl/color/Color.h>

// 引入ECS相关头文件
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/MaterialComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>
#include<cstring>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateGizmo3DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf;
        gvf.Add(VertexSemantic::Position, VF_V3F, 3, sizeof(float) * 3);
        gvf.Add(VertexSemantic::Normal, VF_V3F, 3, sizeof(float) * 3);
        return gvf;
    }
}

class TestApp:public WorkObject
{
private:

    ECSContext *  ecs_context      =nullptr;
    Entity *      cube_entity    =nullptr;
    Entity *      camera_entity  =nullptr;

    MaterialProgram *          material        = nullptr;
    DescriptorBindingSet *binding_set   = nullptr;

    Geometry *          geometry        = nullptr;
    Primitive *         primitive       = nullptr;
    graph::DeviceBuffer *mi_ssbo        = nullptr;
    SSBOSlotAllocator    slot_allocator;

private:

    bool InitMaterial()
    {
        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);

        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        if (!material_manager)
            return false;

        material = material_manager->AcquireMaterialProgram(mtl::MaterialPreset::Gizmo3D, &cfg);

        if(!material)
            return false;

        return true;
    }

    bool CreateCubeGeometry()
    {
        using namespace inline_geometry;

        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* geometry_manager = graphics_context->GetGeometryManager();
        if (!geometry_manager)
            return false;

        auto* device = graphics_context->GetDevice();
        if (!device)
            return false;

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateGizmo3DGeometryVertexFormat());

        CubeCreateInfo cci;
        cci.segments_x = 2;
        cci.segments_y = 3;
        cci.segments_z = 4;

        geometry = CreateCube(pc.get(), &cci);

        if(!geometry)
            return false;

        geometry_manager->Add(geometry);
        return true;
    }

    bool InitPrimitive()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        if (!primitive_manager)
            return false;

        primitive = primitive_manager->CreatePrimitive(geometry, material, binding_set, nullptr);
        return primitive != nullptr;
    }

    bool InitMISSBO()
    {
        if (!material)
            return false;

        if (!ecs_context)
            ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        const uint32_t mi_data_bytes = material->GetMIDataBytes();
        if (mi_data_bytes == 0)
            return true;
        if (mi_data_bytes != sizeof(Color4f))
            return false;

        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* buffer_manager = graphics_context->GetBufferManager();
        auto* domain_manager = graphics_context->GetResourceDomainManager();
        if (!buffer_manager || !domain_manager)
            return false;

        auto descriptor_system = ecs_context->GetSystem<RenderDescriptorBindingSystem>();
        if (!descriptor_system)
            return false;

        if (!slot_allocator.Init(1))
            return false;

        uint32_t slot_index = 0;
        if (!slot_allocator.Allocate(slot_index))
            return false;

        const uint32_t mi_count = 1;
        const VkDeviceSize ssbo_size = static_cast<VkDeviceSize>(mi_count) * mi_data_bytes;

        mi_ssbo = buffer_manager->CreateSSBO("SimpleCube:PBRSurface:MIData", ssbo_size, nullptr, SharingMode::Exclusive);
        if (!mi_ssbo)
            return false;

        auto *gpu_buf = mi_ssbo->GetGPUBuffer();
        if (!gpu_buf)
            return false;

        uint8_t *dst = static_cast<uint8_t *>(gpu_buf->Map(0, ssbo_size));
        if (!dst)
            return false;

        memset(dst, 0, static_cast<size_t>(ssbo_size));

        const Color4f color = GetColor4f(COLOR::BlenderAxisBlue, 1.0f);
        memcpy(dst + static_cast<VkDeviceSize>(slot_index) * mi_data_bytes, &color, mi_data_bytes);

        gpu_buf->Unmap();

        binding_set = new DescriptorBindingSet(material);
        if (!binding_set)
            return false;

        bool has_struct_binding = false;
        for (const auto &req : material->GetBindingContract().requirements)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                continue;

            has_struct_binding = true;
            if (!descriptor_system->RegisterMaterialStructLayout(req.ssbo_type, req.ssbo_id, mi_data_bytes))
                return false;

            const graph::mtl::SSBOAddress addr{req.ssbo_type, req.ssbo_id, 0};
            if (!domain_manager->RegisterBuffer(addr, mi_ssbo, mi_count))
                return false;

            if (!binding_set->SetSSBOBinding(req.ssbo_type, req.ssbo_id, slot_index))
                return false;
        }

        return has_struct_binding;
    }
    bool InitECS()
    {
        ecs_context = GetECSContext();
        if(!ecs_context)
            return false;

        cube_entity = ecs_context->CreateEntity<Entity>("CubeEntity");

        auto transform = cube_entity->AddComponent<TransformComponent>(Mobility::Static);
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        auto primitive_comp = cube_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        primitive_comp->SetPrimitive(primitive);
        primitive_comp->SetDescriptorBindingSet(binding_set);
        primitive_comp->RequestPipeline(InlinePipeline::Solid3D);
        graph::mtl::MaterialRecipe recipe{};
        recipe.recipe_name = "Phase2.MaterialRecipeEntry.Cube";
        recipe.shading_model = graph::mtl::ShadingModel::Unlit;
        recipe.preset_hint = static_cast<uint32_t>(graph::mtl::MaterialPreset::Gizmo3D);
        recipe.domain = "Phase2AuthoringTest";
        primitive_comp->SetMaterialRecipe(recipe);
        primitive_comp->SetVisible(true);

        auto material_comp = cube_entity->AddComponent<hgl::ecs::MaterialComponent>();
        if (!material_comp)
            return false;

        material_comp->program = material;
        material_comp->dbs_compat = binding_set;

        if (!cube_entity->GetComponent<hgl::ecs::MaterialComponent>())
            return false;

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 6.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    ~TestApp()
    {
        delete binding_set;
        SAFE_CLEAR(mi_ssbo)
        SAFE_CLEAR(primitive)
        SAFE_CLEAR(geometry)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if(!InitMaterial())
            return false;

        if(!CreateCubeGeometry())
            return false;

        if(!InitMISSBO())
            return false;

        if(!InitPrimitive())
            return false;

        if(!InitECS())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("MaterialRecipe Entry Cube (Test)"), argc, argv, 1280, 720);
}

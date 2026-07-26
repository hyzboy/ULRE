// RayPicking (ECS Version)
// 该范例主要演示使用ECS架构实现射线拾取功能
// This example demonstrates ray picking using ECS architecture
//
// 本范例展示了：
// 1. 使用ECS架构创建场景对象（平面网格和射线）
// 2. 使用TransformComponent管理空间变换
// 3. 使用PrimitiveComponent管理渲染图元
// 4. 动态更新顶点数据以显示实时射线
// 5. 使用新的 ECS Camera 系统替代旧的 CameraControl

#include<hgl/framework/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/camera/Camera.h>
#include<hgl/math/geometry/Ray.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/log/Log.h>
#include<memory>
#include<cstring>

// 引入ECS相关头文件
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    constexpr uint32_t kRayPickingSsboId = hgl::graph::mtl::MakeRecipeSSBOId(8001);

    GeometryVertexFormat CreateVertexLuminance2DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position,  VF_V2F},
            {VertexSemantic::Luminance, VF_V1UN8},
        };
        return gvf;
    }

    GeometryVertexFormat CreateVertexLuminance3DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position,  VF_V3F},
            {VertexSemantic::Luminance, VF_V1UN8},
        };
        return gvf;
    }
}

static float position_data[2][3]=
{
    {100,100,100},
    {0,0,0}
};

static uint8 lumiance_data[2]={255,255};

static Color4f white_color(1,1,1,1);
static Color4f yellow_color(1,1,0,1);

class TestApp:public WorkObject
{
private:

    // ECS组件
    ECSContext* ecs_world = nullptr;   // 由默认 ECSContext 统一维护
    Entity* plane_grid_entity = nullptr;
    Entity* ray_line_entity = nullptr;

    // 传统渲染资源
    MaterialProgram *          mtl_plane_grid      =nullptr;
    Geometry *          geom_plane_grid     =nullptr;
    graph::DeviceBuffer *mi_shared_ssbo      =nullptr;
    graph::mtl::SSBOType material_ssbo_type = graph::mtl::SSBOType::UserDefined;
    uint32_t             material_ssbo_count = 0;
    uint32_t             material_ssbo_stride = 0;

    MaterialProgram *          mtl_line            =nullptr;
    Geometry *          geom_line           =nullptr;
    Primitive *         prim_line           =nullptr;
    VAB *               prim_line_vab       =nullptr;

    math::Ray           ray;

private:

    bool InitMaterialAndPipeline()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = GetManager<MaterialManager>();
        auto* device = graphics_context->GetDevice();
        if (!material_manager)
            return false;
        if (!device)
            return false;

        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines);

        cfg.local_to_world=true;

        {
            const GeometryVertexFormat plane_grid_gvf = CreateVertexLuminance2DGeometryVertexFormat();
            mtl_plane_grid = material_manager->AcquireMaterialProgram(mtl::MaterialPreset::VertexLuminance3D, &cfg, plane_grid_gvf);
            if(!mtl_plane_grid)return(false);
        }

        {
            const GeometryVertexFormat line_gvf = CreateVertexLuminance3DGeometryVertexFormat();
            mtl_line = material_manager->AcquireMaterialProgram(mtl::MaterialPreset::VertexLuminance3D, &cfg, line_gvf);
            if(!mtl_line)return(false);
        }

        return(true);
    }

    bool CreateGeometry()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* device = graphics_context->GetDevice();
        auto* geometry_manager = GetManager<GeometryManager>();
        if (!device || !geometry_manager)
            return false;

        using namespace inline_geometry;

        // === 创建平面网格几何体 ===
        {
            auto pc = std::make_unique<GeometryCreater>(
                device,
                CreateVertexLuminance2DGeometryVertexFormat());

            struct PlaneGridCreateInfo pgci;

            pgci.grid_size.Set(32,32);
            pgci.sub_count.Set(8,8);

            pgci.lum=128;
            pgci.sub_lum=196;

            geom_plane_grid=CreatePlaneGrid2D(pc.get(),&pgci);

            if(!geom_plane_grid)
                return(false);

            geometry_manager->Add(geom_plane_grid);
        }

        // === 创建射线线段几何体 ===
        {
            auto* device = graphics_context->GetDevice();
            auto* buffer_manager = GetManager<BufferManager>();
            auto* geometry_manager = GetManager<GeometryManager>();
            if (!device || !buffer_manager || !geometry_manager)
                return false;

            GeometryCreater pc(device,
                               CreateVertexLuminance3DGeometryVertexFormat(),
                               buffer_manager);
            pc.Init("RayLine", 2);
            if (!pc.WriteVAB(VAN::Position, VF_V3F, position_data) ||
                !pc.WriteVAB(VAN::Luminance, VF_V1UN8, lumiance_data))
                return false;

            geom_line = pc.Create();
            if (!geom_line)
                return false;
            geometry_manager->Add(geom_line);
        }

        return(true);
    }

    bool InitECS()
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        // === 步骤1: 获取ECS世界 ===
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        auto *buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager)
            return false;

        if (!mtl_plane_grid || !mtl_line)
            return false;

        const uint32_t plane_mi_bytes = mtl_plane_grid->GetMIDataBytes();
        const uint32_t line_mi_bytes = mtl_line->GetMIDataBytes();
        if (plane_mi_bytes == 0 || line_mi_bytes == 0 || plane_mi_bytes != line_mi_bytes)
            return false;
        if (plane_mi_bytes != sizeof(Color4f))
            return false;

        const uint32_t plane_slot = 0;
        const uint32_t line_slot = 1;

        const uint32_t mi_count = (std::max)(plane_slot, line_slot) + 1;
        const VkDeviceSize ssbo_size = static_cast<VkDeviceSize>(mi_count) * plane_mi_bytes;
        material_ssbo_count = mi_count;
        material_ssbo_stride = plane_mi_bytes;
        GLogInfo("[RayPicking] MI setup: plane_slot=%u line_slot=%u stride=%u count=%u bytes=%llu",
                 plane_slot, line_slot, plane_mi_bytes, mi_count,
                 static_cast<unsigned long long>(ssbo_size));

        mi_shared_ssbo = buffer_manager->CreateSSBO("RayPicking:SharedMIData", ssbo_size, nullptr, SharingMode::Exclusive);
        if (!mi_shared_ssbo)
            return false;

        auto *gpu_buf = mi_shared_ssbo->GetGPUBuffer();
        if (!gpu_buf)
            return false;

        auto *dst = static_cast<uint8_t *>(gpu_buf->Map(0, ssbo_size));
        if (!dst)
            return false;

        memset(dst, 0, static_cast<size_t>(ssbo_size));
        memcpy(dst + static_cast<VkDeviceSize>(plane_slot) * plane_mi_bytes, &white_color, plane_mi_bytes);
        memcpy(dst + static_cast<VkDeviceSize>(line_slot) * plane_mi_bytes, &yellow_color, plane_mi_bytes);

        gpu_buf->Unmap();

        for (const auto &req : mtl_plane_grid->GetMaterialResourceLayout().requirements)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                continue;

            material_ssbo_type = req.ssbo_type;
            break;
        }
        if (material_ssbo_type == graph::mtl::SSBOType::UserDefined)
            return false;

        // === 步骤2: 创建平面网格实体 ===
        {
            plane_grid_entity = ecs_world->CreateEntity<Entity>("PlaneGrid");

            // 创建Primitive
            auto* primitive_manager = GetManager<PrimitiveManager>();
            if (!primitive_manager)
                return false;

            Primitive* prim_plane = primitive_manager->CreatePrimitive(geom_plane_grid,
                                                                       mtl_plane_grid,
                                                                       nullptr,
                                                                       nullptr);
            if(!prim_plane)
                return false;

            // 添加TransformComponent
            auto transform = plane_grid_entity->AddComponent<TransformComponent>(Mobility::Static);
            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));

            // 添加PrimitiveComponent
            auto primitive_comp = plane_grid_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
            primitive_comp->SetPrimitive(prim_plane);
            graph::mtl::MaterialRecipe recipe{};
            recipe.recipe_name = "RayPicking.PlaneGrid";
            recipe.shading_model = graph::mtl::ShadingModel::Unlit;
            recipe.preset_hint = static_cast<uint32_t>(graph::mtl::MaterialPreset::VertexLuminance3D);
            recipe.domain = "RayPicking";
            primitive_comp->SetMaterialRecipe(recipe);
            primitive_comp->SetMaterialStructResource(graph::mtl::DataSlot::PBRSurface,
                                                      material_ssbo_type,
                                                      kRayPickingSsboId,
                                                      mi_shared_ssbo,
                                                      material_ssbo_count,
                                                      material_ssbo_stride,
                                                      plane_slot,
                                                      true,
                                                      true);
            primitive_comp->RequestPipeline(InlinePipeline::Solid3D);
            primitive_comp->SetVisible(true);
        }

        // === 步骤3: 创建射线线段实体 ===
        {
            ray_line_entity = ecs_world->CreateEntity<Entity>("RayLine");

            // 创建Primitive
            auto* primitive_manager = GetManager<PrimitiveManager>();
            if (!primitive_manager)
                return false;

            prim_line = primitive_manager->CreatePrimitive(geom_line,
                                                           mtl_line,
                                                           nullptr,
                                                           nullptr);
            if(!prim_line)
                return false;

            // 获取VAB用于后续动态更新顶点数据
            prim_line_vab = prim_line->GetVAB(VAN::Position);

            // 添加TransformComponent
            auto transform = ray_line_entity->AddComponent<TransformComponent>(Mobility::Static);//线段虽然会动，但我们改的是VAB不是Transform
            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));

            // 添加PrimitiveComponent
            auto primitive_comp = ray_line_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
            primitive_comp->SetPrimitive(prim_line);
            graph::mtl::MaterialRecipe recipe{};
            recipe.recipe_name = "RayPicking.Line";
            recipe.shading_model = graph::mtl::ShadingModel::Unlit;
            recipe.preset_hint = static_cast<uint32_t>(graph::mtl::MaterialPreset::VertexLuminance3D);
            recipe.domain = "RayPicking";
            primitive_comp->SetMaterialRecipe(recipe);
            primitive_comp->SetMaterialStructResource(graph::mtl::DataSlot::PBRSurface,
                                                      material_ssbo_type,
                                                      kRayPickingSsboId,
                                                      mi_shared_ssbo,
                                                      material_ssbo_count,
                                                      material_ssbo_stride,
                                                      line_slot,
                                                      true,
                                                      true);
            primitive_comp->RequestPipeline(InlinePipeline::Solid3D);
            primitive_comp->SetVisible(true);
        }

        return true;
    }

    bool InitScene()
    {
        // === 使用新的 ECS Camera 系统 ===
        // Create camera entity and component
        auto camera_entity = ecs_world->CreateEntity<Entity>("MainCamera");
        auto camera_component = camera_entity->AddComponent<CameraComponent>();

        // 设置纯数据 / Set pure data
        camera_component->position = math::Vector3f(32, 32, 32);
        camera_component->target = math::Vector3f(0, 0, 0);
        camera_component->world_up = math::Vector3f(0, 0, 1);
        camera_component->control_mode = CameraComponent::ControlMode::ViewModel;
        camera_component->fov = 45.0f;
        camera_component->near_plane = 0.1f;
        camera_component->far_plane = 1000.0f;
        camera_component->distance = 55.4f;
        camera_component->yaw = -135.0f;
        camera_component->pitch = -35.0f;
        camera_component->rotation_sensitivity = 0.2f;
        camera_component->zoom_sensitivity = 1.0f;
        camera_component->move_speed = 10.0f;

        // 连接到渲染系统 / Connect to rendering system
        camera_component->camera_data = GetCamera();
        camera_component->camera_info = const_cast<graph::CameraInfo*>(GetCameraInfo());
        camera_component->viewport_info = GetViewportInfo();
        // camera_component->camera_ubo = nullptr; // UBO is managed internally
        camera_component->is_main_camera = true;
        camera_component->matrix_dirty = true;

        return(true);
    }

public:
    ~TestApp()
    {
        SAFE_CLEAR(geom_plane_grid);
        SAFE_CLEAR(geom_line);
        SAFE_CLEAR(mi_shared_ssbo);
    }

    bool Init() override
    {
        if(!InitMaterialAndPipeline())
            return(false);

        if(!CreateGeometry())
            return(false);

        if(!InitECS())
            return(false);

        if(!InitScene())
            return(false);

        return(true);
    }

    void Tick(double delta) override
    {
        WorkObject::Tick(delta);

        // === 射线拾取逻辑 ===
        // 根据鼠标位置计算射线，并找到与原点最近的点
        const math::Vector2i *mouse_position_ptr=GetMouseCoord();
        if(!mouse_position_ptr)
            return;

        const math::Vector2i &mouse_position=*mouse_position_ptr;

        // 从 ECS 获取摄像机信息（替代旧的 CameraControl）
        const CameraInfo *ci = GetCameraInfo();
        const ViewportInfo *vi = GetViewportInfo();

        if(!ci || !vi)
            return;

        // 设置射线查询的屏幕坐标点
        ray.SetFromViewportPoint(mouse_position,ci,vi->GetViewport());

        // 更新VAB上射线的起点和方向（画出完整的射线线段）
        // 注意: sizeof(Vector3f)==16 (GLM_FORCE_DEFAULT_ALIGNED_GENTYPES 对齐填充)，
        //       但 VAB stride==12 (VK_FORMAT_R32G32B32_SFLOAT)，
        //       所以不能直接用 Vector3f[] 数组传给 Write，必须用紧凑 float 数组。
        if(prim_line_vab)
        {
            const math::Vector3f endpoint = ray.origin + ray.direction * 100.0f;
            float ray_pts[2][3];
            ray_pts[0][0] = ray.origin.x;
            ray_pts[0][1] = ray.origin.y;
            ray_pts[0][2] = ray.origin.z;
            ray_pts[1][0] = 0;
            ray_pts[1][1] = 0;
            ray_pts[1][2] = 0;
            prim_line_vab->Write(ray_pts, 2);  // 更新两个顶点
        }
    }
};//class TestApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("RayPicking (ECS Version)"),argc,argv,1280,720);
}

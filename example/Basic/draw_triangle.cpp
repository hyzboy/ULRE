// 该范例主要演示使用新的ECS架构管理和绘制一个渐变色的三角形，参考draw_triangle_use_UBO.cpp
// This example demonstrates managing and drawing a gradient colored triangle using the new ECS architecture
//
// 本范例展示了：
// 1. 创建ECS World和Entity
// 2. 使用TransformComponent管理空间变换
// 3. 使用PrimitiveComponent管理渲染图元
// 4. ECS与传统渲染系统的集成

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/graph/module/GeometryManager.h>
#include<cstdio>

 // 引入ECS相关头文件
 #include<hgl/ecs/core/Context.h>
 #include<hgl/ecs/core/Entity.h>
 #include<hgl/ecs/components/TransformComponent.h>
 #include<hgl/ecs/components/PrimitiveComponent.h>
 #include<hgl/object/ObjectTracker.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

constexpr uint32_t VERTEX_COUNT=3;

static float position_data_float[VERTEX_COUNT][2]=
{
    {0.5,   0.25},
    {0.75,  0.75},
    {0.25,  0.75}
};

static float position_data[VERTEX_COUNT][3]={};

constexpr uint8 color_data[VERTEX_COUNT*4]=
{
    255,0,0,255,
    0,255,0,255,
    0,0,255,255
};

constexpr VAType   POSITION_SHADER_FORMAT   =VAT_VEC3;
constexpr VkFormat POSITION_DATA_FORMAT     =VF_V3F;

constexpr VkFormat COLOR_DATA_FORMAT        =VF_V4UN8;

class TestApp:public WorkObject
{
private:

    // ECS组件
    ECSContext *  ecs_world      =nullptr;   // 由默认 ECSContext 统一维护
    Entity* triangle_entity     =nullptr;
    uint64_t entity_id          =0;          // 对象追踪ID

    // 渲染资源
    Geometry *          geometry            =nullptr;

    inline static const mtl::MaterialRecipe kTriangleCfg {
        .id         = "draw_triangle_vertex_color",
        .preset     = mtl::MaterialPreset::VertexColor2D,
        .dim        = mtl::MaterialRecipe::Dim::D2,
        .l2w        = false,
        .pos_format = POSITION_SHADER_FORMAT,
        .coord_2d   = CoordinateSystem2D::Ortho,
        .pipeline   = GraphicsPipelinePreset::Solid2D,
        .attribute_providers = []
        {
            std::array<AttributeProviderId, size_t(VertexAttrib::RANGE_SIZE)> providers{};
            providers[size_t(VertexAttrib::Color)] = AttributeProviderId::SSBO_PackedRGBA8;
            return providers;
        }(),
        .position_provider = PositionProviderId::SSBO_PackedVec3,
    };

private:

    bool UpdateTrianglePositionData(const VkExtent2D &extent)
    {
        if(extent.width == 0 || extent.height == 0)
        {
            std::printf("[draw_triangle][UpdateTrianglePositionData] invalid extent: %ux%u\n",
                        extent.width,
                        extent.height);
            return false;
        }

        for(uint i=0;i<VERTEX_COUNT;i++)
        {
            position_data[i][0] = position_data_float[i][0] * extent.width;
            position_data[i][1] = position_data_float[i][1] * extent.height;
            position_data[i][2] = 0.0f;
        }

        return true;
    }

    bool RefreshGeometryPositionBuffer()
    {
        if(!geometry)
        {
            std::printf("[draw_triangle][RefreshGeometryPositionBuffer] geometry is null\n");
            return false;
        }

        VAB *position_vab = geometry->GetVAB(VAN::Position);
        if(!position_vab)
        {
            std::printf("[draw_triangle][RefreshGeometryPositionBuffer] position VAB is null\n");
            return false;
        }

        const bool write_ok = position_vab->Write(position_data, VERTEX_COUNT);
        std::printf("[draw_triangle][RefreshGeometryPositionBuffer] write=%s vkBuf=%p stride=%u count=%u\n",
                    write_ok ? "OK" : "FAIL",
                    (void *)position_vab->GetVkBuffer(),
                    position_vab->GetStride(),
                    position_vab->GetCount());

        return write_ok;
    }

    bool CreateGeometry()
    {
        VkExtent2D init_extent{};
        if(const auto *ext = GetExtent(); ext && ext->width > 0 && ext->height > 0)
        {
            init_extent = *ext;
            std::printf("[draw_triangle][CreateGeometry] using current extent: %ux%u\n", init_extent.width, init_extent.height);
        }
        else
        {
            // Keep sample robust even if swapchain extent is not ready at Init().
            init_extent.width = 1280;
            init_extent.height = 720;
            std::printf("[draw_triangle][CreateGeometry] extent not ready, fallback to: %ux%u\n", init_extent.width, init_extent.height);
        }

        if(!UpdateTrianglePositionData(init_extent))
            return false;

        geometry = WorkObject::CreateGeometry("Triangle",
                                              VERTEX_COUNT,
                                              {{VAN::Position, POSITION_DATA_FORMAT, position_data},
                                               {VAN::Color, COLOR_DATA_FORMAT, color_data}});

        if(!geometry)
            std::printf("[draw_triangle][CreateGeometry] geometry creation failed\n");

        return geometry != nullptr;
    }

    bool InitECS()
    {
        HGL_CAPTURE_SCOPE();  // 记录此函数调用的栈信息

        // === 步骤1: 创建ECS世界 ===
        // World是ECS架构的顶层容器，管理所有Entity和System
        ecs_world = GetECSContext();
        if(!ecs_world)
            return false;

        // === 步骤2: 创建Entity ===
        // Entity是游戏对象的容器，本身不包含数据，只是Component的集合
        triangle_entity = ecs_world->CreateEntity<Entity>("TriangleEntity");
        entity_id = HGL_TRACK_ALLOCATION("TriangleEntity", hgl::core::ObjectTypeTag::RenderSystem);

        // === 步骤3: 添加TransformComponent ===
        // TransformComponent管理空间变换（位置、旋转、缩放）
        // 内部使用SOA（Structure of Arrays）存储以提高缓存性能
        HGL_TRACK_ALLOCATION("TriangleTransform", hgl::core::ObjectTypeTag::FrameResource);
        auto transform = triangle_entity->AddComponent<TransformComponent>(Mobility::Static);
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));

        // 设置为静态对象 - 系统会缓存世界矩阵，提高性能
        transform->SetMovable(false);

        // === 步骤4: 添加ECS PrimitiveComponent ===
        // 新的ECS PrimitiveComponent用于管理渲染图元
        // 注意：需要明确使用hgl::ecs命名空间，因为有两个PrimitiveComponent
        HGL_TRACK_ALLOCATION("TrianglePrimitive", hgl::core::ObjectTypeTag::FrameResource);
        auto ecs_primitive = triangle_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        ecs_primitive->SetUnresolvedGeometry(geometry);
        ecs_primitive->SetMaterialRecipe(RegisterMaterialRecipe(kTriangleCfg));
        ecs_primitive->SetVisible(true);

        return true;
    }

public:

    bool Init() override
    {
        HGL_CAPTURE_SCOPE();  // 记录应用初始化的调用栈

        if(!CreateGeometry())
            return(false);

        if(!InitECS())
            return(false);

        // 初始化时已设置默认 ECSContext

         return(true);
     }

     void OnResize(const VkExtent2D &new_extent) override
     {
         std::printf("[draw_triangle][OnResize] new extent=%ux%u\n", new_extent.width, new_extent.height);

         if(!UpdateTrianglePositionData(new_extent))
             return;

         const bool refresh_ok = RefreshGeometryPositionBuffer();
         std::printf("[draw_triangle][OnResize] refresh position VAB: %s\n", refresh_ok ? "OK" : "FAIL");
     }
 };//class TestApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Draw triangle use ECS"),argc,argv);
}

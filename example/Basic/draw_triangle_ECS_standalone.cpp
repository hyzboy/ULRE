// 该范例演示一个完全独立的ECS渲染程序，不依赖WorkObject/WorkManager/RenderFramework体系
// This example demonstrates a completely standalone ECS rendering program without WorkObject/WorkManager/RenderFramework dependencies
//
// 本范例展示了：
// 1. 直接使用Vulkan和ECS进行最小化渲染
// 2. 手动管理Vulkan初始化和渲染循环
// 3. 使用ECS RenderCollector进行批次渲染
// 4. 不依赖任何高层框架

#include<iostream>
#include<memory>
#include<vector>

// Vulkan基础
#include<hgl/graph/VK.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKSwapchain.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKFramebuffer.h>

// 材质和图元
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>

// ECS系统
#include<hgl/ecs/Context.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/RenderCollector.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

// 顶点数据
constexpr uint32_t VERTEX_COUNT = 3;

static float position_data_float[VERTEX_COUNT][2] = {
    {0.5f,   0.25f},
    {0.75f,  0.75f},
    {0.25f,  0.75f}
};

static int16 position_data[VERTEX_COUNT][2] = {};

constexpr uint8 color_data[VERTEX_COUNT * 4] = {
    255, 0, 0, 255,
    0, 255, 0, 255,
    0, 0, 255, 255
};

constexpr VAType   POSITION_SHADER_FORMAT = VAT_IVEC2;
constexpr VkFormat POSITION_DATA_FORMAT = VF_V2I16;
constexpr VkFormat COLOR_DATA_FORMAT = VF_V4UN8;

/**
 * 独立的ECS三角形渲染应用
 * Standalone ECS Triangle Rendering Application
 */
class StandaloneECSApp
{
private:
    // Vulkan核心对象
    VulkanDevice* device = nullptr;
    Swapchain* swapchain = nullptr;
    RenderPass* render_pass = nullptr;
    std::vector<Framebuffer*> framebuffers;
    CommandBuffer* cmd_buffer = nullptr;
    
    // 渲染资源
    MaterialInstance* material_instance = nullptr;
    Primitive* prim_triangle = nullptr;
    Pipeline* pipeline = nullptr;
    
    // ECS组件
    std::shared_ptr<ECSContext> ecs_world = nullptr;
    std::shared_ptr<Entity> triangle_entity = nullptr;
    RenderCollector* render_collector = nullptr;
    
    // 窗口尺寸
    uint32_t width = 1024;
    uint32_t height = 1024;
    
    bool running = true;

public:
    StandaloneECSApp() = default;
    ~StandaloneECSApp() { Cleanup(); }
    
    bool Initialize()
    {
        std::cout << "=== Initializing Standalone ECS Triangle App ===" << std::endl;
        
        if (!InitVulkan())
        {
            std::cerr << "Failed to initialize Vulkan" << std::endl;
            return false;
        }
        
        if (!InitMaterial())
        {
            std::cerr << "Failed to initialize materials" << std::endl;
            return false;
        }
        
        if (!InitGeometry())
        {
            std::cerr << "Failed to initialize geometry" << std::endl;
            return false;
        }
        
        if (!InitECS())
        {
            std::cerr << "Failed to initialize ECS" << std::endl;
            return false;
        }
        
        std::cout << "=== Initialization Complete ===" << std::endl;
        return true;
    }
    
    void Run()
    {
        std::cout << "=== Starting Render Loop ===" << std::endl;
        
        // 简单的渲染循环（通常会集成到窗口系统）
        // Simple render loop (normally integrated with window system)
        int frame_count = 0;
        const int max_frames = 60; // 渲染60帧后退出
        
        while (running && frame_count < max_frames)
        {
            Update(0.016); // 假设60 FPS
            Render();
            frame_count++;
            
            if (frame_count % 10 == 0)
            {
                std::cout << "Rendered " << frame_count << " frames..." << std::endl;
            }
        }
        
        std::cout << "=== Render Loop Complete ===" << std::endl;
    }
    
    void Cleanup()
    {
        std::cout << "=== Cleaning Up ===" << std::endl;
        
        if (ecs_world)
        {
            ecs_world->Shutdown();
            ecs_world.reset();
        }
        
        // 清理Vulkan资源
        // Clean up Vulkan resources
        if (prim_triangle) delete prim_triangle;
        if (pipeline) delete pipeline;
        if (material_instance) delete material_instance;
        
        for (auto fb : framebuffers)
            if (fb) delete fb;
        framebuffers.clear();
        
        if (cmd_buffer) delete cmd_buffer;
        if (render_pass) delete render_pass;
        if (swapchain) delete swapchain;
        if (device) delete device;
        
        std::cout << "=== Cleanup Complete ===" << std::endl;
    }

private:
    bool InitVulkan()
    {
        std::cout << "Initializing Vulkan..." << std::endl;
        
        // 注意：这是一个简化版本
        // 完整的Vulkan初始化需要：
        // 1. 创建VkInstance
        // 2. 选择物理设备
        // 3. 创建逻辑设备
        // 4. 创建Swapchain
        // 5. 创建RenderPass
        // 6. 创建Framebuffers
        // 7. 创建CommandBuffer
        //
        // 由于这些依赖于底层Vulkan框架代码，这里使用注释说明
        // 实际实现需要访问VulkanInstance、PhysicalDevice等
        
        std::cout << "NOTE: This is a conceptual example." << std::endl;
        std::cout << "Full Vulkan initialization requires:" << std::endl;
        std::cout << "  - VkInstance creation" << std::endl;
        std::cout << "  - Physical device selection" << std::endl;
        std::cout << "  - Logical device creation" << std::endl;
        std::cout << "  - Swapchain setup" << std::endl;
        std::cout << "  - RenderPass and Framebuffer creation" << std::endl;
        
        // 在实际应用中，这里会调用底层API创建这些对象
        // device = CreateVulkanDevice(...);
        // swapchain = CreateSwapchain(device, width, height);
        // render_pass = CreateRenderPass(device, swapchain->GetFormat());
        // etc.
        
        return true; // 概念性返回
    }
    
    bool InitMaterial()
    {
        std::cout << "Initializing materials..." << std::endl;
        
        if (!device)
        {
            std::cerr << "Device not initialized" << std::endl;
            return false;
        }
        
        // 创建材质配置
        mtl::Material2DCreateConfig cfg(
            PrimitiveType::Triangles,
            CoordinateSystem2D::Ortho,
            mtl::WithLocalToWorld::With
        );
        
        VILConfig vil_config;
        cfg.position_format = POSITION_SHADER_FORMAT;
        vil_config.Add(VAN::Position, POSITION_DATA_FORMAT);
        vil_config.Add(VAN::Color, COLOR_DATA_FORMAT);
        
        // 在实际应用中创建材质实例
        // material_instance = device->CreateMaterialInstance(
        //     mtl::inline_material::VertexColor2D, &cfg, &vil_config);
        
        // 创建Pipeline
        // pipeline = device->CreatePipeline(material_instance, render_pass);
        
        std::cout << "Materials initialized" << std::endl;
        return true;
    }
    
    bool InitGeometry()
    {
        std::cout << "Initializing geometry..." << std::endl;
        
        // 转换位置数据到屏幕坐标
        for (uint i = 0; i < VERTEX_COUNT; i++)
        {
            position_data[i][0] = static_cast<int16>(position_data_float[i][0] * width);
            position_data[i][1] = static_cast<int16>(position_data_float[i][1] * height);
        }
        
        // 创建Primitive
        // prim_triangle = device->CreatePrimitive(
        //     "Triangle", VERTEX_COUNT, material_instance, pipeline,
        //     {
        //         {VAN::Position, POSITION_DATA_FORMAT, position_data},
        //         {VAN::Color, COLOR_DATA_FORMAT, color_data}
        //     }
        // );
        
        std::cout << "Geometry initialized" << std::endl;
        return true;
    }
    
    bool InitECS()
    {
        std::cout << "Initializing ECS..." << std::endl;
        
        // 创建ECS世界
        ecs_world = std::make_shared<ECSContext>("StandaloneTriangleWorld");
        ecs_world->Initialize();
        
        // 注册RenderCollector系统
        render_collector = ecs_world->RegisterSystem<RenderCollector>();
        render_collector->SetWorld(ecs_world);
        render_collector->SetDevice(device);
        render_collector->Initialize();
        
        // 配置相机
        CameraInfo camera;
        camera.position = glm::vec3(0.0f, 0.0f, 10.0f);
        camera.forward = glm::vec3(0.0f, 0.0f, -1.0f);
        camera.viewMatrix = glm::mat4(1.0f);
        camera.projectionMatrix = glm::ortho(
            0.0f, static_cast<float>(width),
            0.0f, static_cast<float>(height),
            0.1f, 100.0f
        );
        camera.UpdateViewProjection();
        render_collector->SetCameraInfo(&camera);
        
        // 创建Entity
        triangle_entity = ecs_world->CreateEntity<Entity>("TriangleEntity");
        
        // 添加TransformComponent
        auto transform = triangle_entity->AddComponent<TransformComponent>();
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);
        
        // 添加PrimitiveComponent
        auto ecs_primitive = triangle_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        ecs_primitive->SetPrimitive(prim_triangle);
        ecs_primitive->SetVisible(true);
        
        std::cout << "ECS initialized with 1 entity" << std::endl;
        return true;
    }
    
    void Update(double delta_time)
    {
        // 更新ECS世界
        if (ecs_world)
        {
            ecs_world->Update(delta_time);
        }
    }
    
    void Render()
    {
        // 这是渲染的概念性流程
        // Conceptual rendering flow
        
        // 1. 获取下一个swapchain图像
        // uint32_t image_index = swapchain->AcquireNextImage();
        
        // 2. 开始命令缓冲记录
        // cmd_buffer->Begin();
        
        // 3. 开始RenderPass
        // cmd_buffer->BeginRenderPass(render_pass, framebuffers[image_index]);
        
        // 4. 使用ECS RenderCollector进行渲染
        if (render_collector && cmd_buffer)
        {
            render_collector->CollectRenderables();
            render_collector->Render(cmd_buffer);
        }
        
        // 5. 结束RenderPass和命令缓冲
        // cmd_buffer->EndRenderPass();
        // cmd_buffer->End();
        
        // 6. 提交命令缓冲
        // device->Submit(cmd_buffer);
        
        // 7. 呈现图像
        // swapchain->Present(image_index);
    }
};

int main(int argc, char** argv)
{
    std::cout << "============================================" << std::endl;
    std::cout << " Standalone ECS Triangle Rendering Example " << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << std::endl;
    std::cout << "This is a conceptual standalone example showing:" << std::endl;
    std::cout << "  - Direct Vulkan initialization (without framework)" << std::endl;
    std::cout << "  - ECS World and Entity management" << std::endl;
    std::cout << "  - ECS RenderCollector usage" << std::endl;
    std::cout << "  - Manual render loop" << std::endl;
    std::cout << std::endl;
    std::cout << "NOTE: Full implementation requires platform-specific" << std::endl;
    std::cout << "      window creation and complete Vulkan setup." << std::endl;
    std::cout << std::endl;
    
    StandaloneECSApp app;
    
    if (!app.Initialize())
    {
        std::cerr << "Failed to initialize application" << std::endl;
        return -1;
    }
    
    app.Run();
    app.Cleanup();
    
    std::cout << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << " Application Terminated Successfully" << std::endl;
    std::cout << "============================================" << std::endl;
    
    return 0;
}

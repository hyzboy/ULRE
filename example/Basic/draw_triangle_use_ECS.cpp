// 该范例主要演示使用新的ECS架构管理和绘制一个渐变色的三角形，参考draw_triangle_use_UBO.cpp
// This example demonstrates managing and drawing a gradient colored triangle using the new ECS architecture
// 
// 本范例展示了：
// 1. 创建ECS World和Entity
// 2. 使用TransformComponent管理空间变换
// 3. 使用PrimitiveComponent管理渲染图元
// 4. 使用ECS RenderCollector进行实际渲染（不再依赖传统渲染系统）

#include<hgl/WorkManager.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>

// 引入ECS相关头文件
#include<hgl/ecs/Context.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/RenderCollector.h>

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

static int16 position_data[VERTEX_COUNT][2]={};

constexpr uint8 color_data[VERTEX_COUNT*4]=
{   
    255,0,0,255,
    0,255,0,255,
    0,0,255,255
};

constexpr VAType   POSITION_SHADER_FORMAT   =VAT_IVEC2;
constexpr VkFormat POSITION_DATA_FORMAT     =VF_V2I16;

constexpr VkFormat COLOR_DATA_FORMAT        =VF_V4UN8;

class TestApp:public WorkObject
{
private:

    // ECS组件
    std::shared_ptr<ECSContext>  ecs_world      =nullptr;
    std::shared_ptr<Entity> triangle_entity     =nullptr;
    RenderCollector* render_collector           =nullptr;

    // 渲染资源
    MaterialInstance *  material_instance   =nullptr;
    Primitive *         prim_triangle       =nullptr;
    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        mtl::Material2DCreateConfig cfg(PrimitiveType::Triangles,
                                        CoordinateSystem2D::Ortho,
                                        mtl::WithLocalToWorld::With);

        VILConfig vil_config;

        cfg.position_format     =       POSITION_SHADER_FORMAT;     //这里指定shader中使用ivec2当做顶点输入格式
                                //      ^
                                //      +  这上下两种格式要配套，否则会出错
                                //      v
        vil_config.Add(VAN::Position,   POSITION_DATA_FORMAT);     //这里指定VAB中使用RG16I当做顶点数据格式

        vil_config.Add(VAN::Color,      COLOR_DATA_FORMAT);        //这里指定VAB中使用RGBA8UNorm当做颜色数据格式

        material_instance=CreateMaterialInstance(mtl::inline_material::VertexColor2D,&cfg,&vil_config);

        if(!material_instance)
            return(false);
           
        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D);     //使用swapchain的render target

        return pipeline;
    }

    bool InitVBO()
    {
        const auto ext=GetExtent();

        for(uint i=0;i<VERTEX_COUNT;i++)
        {
            position_data[i][0]=position_data_float[i][0]*ext->width;
            position_data[i][1]=position_data_float[i][1]*ext->height;
        }

        prim_triangle=CreatePrimitive("Triangle",VERTEX_COUNT,material_instance,pipeline,
                                    {
                                        {VAN::Position,POSITION_DATA_FORMAT,position_data},
                                        {VAN::Color,   COLOR_DATA_FORMAT,   color_data}
                                    });

        if(!prim_triangle)
            return(false);

        return true;
    }

    bool InitECS()
    {
        // === 步骤1: 创建ECS世界 ===
        // World是ECS架构的顶层容器，管理所有Entity和System
        ecs_world = std::make_shared<ECSContext>("TriangleWorld");
        
        // 初始化世界 - 这会初始化所有注册的System
        ecs_world->Initialize();

        // === 步骤2: 注册并初始化RenderCollector系统 ===
        // RenderCollector负责收集所有可渲染的Entity并进行批次渲染
        render_collector = ecs_world->RegisterSystem<RenderCollector>();
        render_collector->SetWorld(ecs_world);
        render_collector->SetDevice(GetDevice());
        render_collector->Initialize();

        // 配置相机信息（使用正交投影的2D相机）
        CameraInfo camera;
        const auto ext = GetExtent();
        camera.position = glm::vec3(0.0f, 0.0f, 10.0f);
        camera.forward = glm::vec3(0.0f, 0.0f, -1.0f);
        
        // 创建正交投影矩阵以匹配屏幕空间
        camera.viewMatrix = glm::mat4(1.0f);  // 单位矩阵（无变换）
        camera.projectionMatrix = glm::ortho(
            0.0f, static_cast<float>(ext->width),   // left, right
            0.0f, static_cast<float>(ext->height),  // bottom, top
            0.1f, 100.0f                            // near, far
        );
        camera.UpdateViewProjection();
        
        render_collector->SetCameraInfo(&camera);

        // === 步骤3: 创建Entity ===
        // Entity是游戏对象的容器，本身不包含数据，只是Component的集合
        triangle_entity = ecs_world->CreateEntity<Entity>("TriangleEntity");

        // === 步骤4: 添加TransformComponent ===
        // TransformComponent管理空间变换（位置、旋转、缩放）
        // 内部使用SOA（Structure of Arrays）存储以提高缓存性能
        auto transform = triangle_entity->AddComponent<TransformComponent>();
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        
        // 设置为静态对象 - 系统会缓存世界矩阵，提高性能
        transform->SetMovable(false);

        // === 步骤5: 添加ECS PrimitiveComponent ===
        // 新的ECS PrimitiveComponent用于管理渲染图元
        // 注意：需要明确使用hgl::ecs命名空间，因为有两个PrimitiveComponent
        auto ecs_primitive = triangle_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        ecs_primitive->SetPrimitive(prim_triangle);
        ecs_primitive->SetVisible(true);

        // === 现在完全使用ECS渲染系统 ===
        // 不再需要传统的Component系统，RenderCollector会自动收集并渲染ECS Entity

        return true;
    }

public:

    using WorkObject::WorkObject;

    bool Init() override
    {
        if(!InitMaterial())
            return(false);

        if(!InitVBO())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }

    void Tick(double delta_time) override
    {
        // 更新ECS世界 - 这会更新所有Entity和Component
        if(ecs_world)
        {
            ecs_world->Update(delta_time);
        }

        WorkObject::Tick(delta_time);
    }

    void Draw(RenderCmdBuffer* cmd_buf) override
    {
        // === 使用ECS RenderCollector进行渲染 ===
        if(render_collector)
        {
            // 收集所有可渲染的Entity
            render_collector->CollectRenderables();
            
            // 渲染收集到的Entity（使用间接渲染和批次优化）
            render_collector->Render(cmd_buf);
        }

        // 调用基类Draw以处理其他渲染任务
        WorkObject::Draw(cmd_buf);
    }

    ~TestApp()
    {
        // 关闭ECS世界
        if(ecs_world)
        {
            ecs_world->Shutdown();
        }
    }
};//class TestApp:public WorkObject

int os_main(int,os_char **)
{
    return RunFramework<TestApp>(OS_TEXT("Draw triangle use ECS"));
}

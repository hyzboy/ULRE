// 该范例主要演示使用ECS架构和RenderCollector系统绘制多个三角形，并利用RenderCollector进行排序以及自动合并进行Instance渲染
// This example demonstrates using ECS architecture with RenderCollector to draw multiple triangles with automatic instance rendering

#include<hgl/WorkManager.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>

// 引入ECS相关头文件
#include<hgl/ecs/Context.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/PrimitiveComponent.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

constexpr uint32_t VERTEX_COUNT=3;
constexpr uint32_t TRIANGLE_NUMBER=12;

constexpr float position_data[VERTEX_COUNT*2]=
{
     0.0,  0.0,
    -0.1,  0.9,
     0.1,  0.9
};

constexpr uint8 color_data[VERTEX_COUNT][4]=
{
    {255,0,0,255},
    {0,255,0,255},
    {0,0,255,255}
};

class TestApp:public WorkObject
{
private:

    // ECS组件
    std::shared_ptr<ECSContext>  ecs_world      =nullptr;
    
    // 传统渲染资源
    MaterialInstance *  material_instance   =nullptr;
    Primitive *         render_obj          =nullptr;
    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        {
            mtl::Material2DCreateConfig cfg(PrimitiveType::Triangles,
                                            CoordinateSystem2D::NDC,
                                            mtl::WithLocalToWorld::With);

            VILConfig vil_config;

            vil_config.Add(VAN::Color,VF_V4UN8);

            material_instance=CreateMaterialInstance(mtl::inline_material::VertexColor2D,&cfg,&vil_config);
        }

        if(!material_instance)
            return(false);

        pipeline=CreatePipeline(material_instance,InlinePipeline::Solid2D);

        return pipeline;
    }

    bool InitVBO()
    {
        render_obj=CreatePrimitive("Triangle",VERTEX_COUNT,material_instance,pipeline,
                                    {
                                        {VAN::Position,   VF_V2F,     position_data},
                                        {VAN::Color,      VF_V4UN8,   color_data   }
                                    });

        if(!render_obj)
            return(false);

        return(true);
    }

    bool InitECS()
    {
        // === 步骤1: 创建ECS世界 ===
        ecs_world = std::make_shared<ECSContext>("AutoInstanceWorld");
        ecs_world->Initialize();

        // === 步骤2: 创建多个三角形Entity，每个旋转不同角度 ===
        double rad;
        
        for(uint i=0;i<TRIANGLE_NUMBER;i++)
        {
            // 创建Entity
            auto entity = ecs_world->CreateEntity<Entity>("Triangle_" + std::to_string(i));

            // 添加TransformComponent并设置旋转
            auto transform = entity->AddComponent<TransformComponent>();
            
            rad = deg2rad((360.0f/double(TRIANGLE_NUMBER))*i);
            glm::quat rotation = glm::angleAxis((float)rad, glm::vec3(0.0f, 0.0f, 1.0f));
            
            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(rotation);
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);  // 静态对象，优化性能

            // 添加ECS PrimitiveComponent
            auto ecs_primitive = entity->AddComponent<hgl::ecs::PrimitiveComponent>();
            ecs_primitive->SetPrimitive(render_obj);
            ecs_primitive->SetVisible(true);

            // === 同时使用传统渲染系统 ===
            // 由于ECS RenderCollector尚未完全集成，仍需要传统Component来实际渲染
            CreateComponentInfo cci(GetWorldRootNode());
            cci.mat = math::AxisRotate(rad, math::Vector3f(0,0,1));
            CreateComponent<component::PrimitiveComponent>(&cci, render_obj);
        }

        return(true);
    }

public:

    using WorkObject::WorkObject;

    bool Init() override
    {
        GetSceneRenderer()->SetClearColor(Color4f(0.2f,0.2f,0.2f,1.0f));

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
        // 更新ECS世界
        if(ecs_world)
        {
            ecs_world->Update(delta_time);
        }

        WorkObject::Tick(delta_time);
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
    return RunFramework<TestApp>(OS_TEXT("AutoInstance_ECS"),1024,1024);
}

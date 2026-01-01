// 该范例主要演示使用SimpleECSFramework和ECS架构，在一个材质下的不同材质实例传递颜色参数绘制三角形
// 并依赖RenderCollector中的自动合并功能，让同一材质下所有不同材质实例的对象一次渲染完成
// This example demonstrates using SimpleECSFramework and ECS architecture with different material instances under one material
// and auto-merging functionality in RenderCollector for efficient rendering

#include"SimpleECSFramework.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/color/Color.h>

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

constexpr float position_data[VERTEX_COUNT*2]=
{
     0.0,  0.0,
    -0.1,  0.9,
     0.1,  0.9
};

constexpr uint DRAW_OBJECT_COUNT=12;
constexpr double TRI_ROTATE_ANGLE=360.0f/DRAW_OBJECT_COUNT;

//#define USE_MATERIAL_FILE   true        //是否使用材质文件

class TestApp:public SimpleECSWorkObject
{
    // ECS组件
    std::shared_ptr<ECSContext>  ecs_world      =nullptr;
    RenderCollector* render_collector           =nullptr;

    // 渲染资源
    Material *          material            =nullptr;

    struct
    {
        MaterialInstance *  mi;
        Primitive *         primitive;
    }render_obj[DRAW_OBJECT_COUNT]{};

    Pipeline *          pipeline            =nullptr;

private:

    bool InitMaterial()
    {
        {
            mtl::Material2DCreateConfig cfg(PrimitiveType::Triangles,CoordinateSystem2D::NDC,mtl::WithLocalToWorld::With);

        #ifndef USE_MATERIAL_FILE
            mtl::MaterialCreateInfo *mci=mtl::CreatePureColor2D(GetDevAttr(),&cfg);

            material=CreateMaterial("PureColor2D",mci);
        #else
            material=LoadMaterial("Std2D/PureColor2D",&cfg);
        #endif//USE_MATERIAL_FILE

            if(!material)
                return(false);

            // 为每个对象创建不同颜色的材质实例
            for(uint i=0;i<DRAW_OBJECT_COUNT;i++)
            {
                render_obj[i].mi=CreateMaterialInstance(material);

                if(!render_obj[i].mi)
                    return(false);

                Color4f color=GetColor4f((COLOR)(i+int(COLOR::Blue)),1.0f);

                render_obj[i].mi->WriteMIData(color);       //设置MaterialInstance的数据
            }
        }

        pipeline=CreatePipeline(material,InlinePipeline::Solid2D);
        
        return pipeline;
    }

    bool InitVBOAndECS()
    {
        // 创建共享的几何体
        Geometry *geometry=CreateGeometry("Triangle",VERTEX_COUNT,material->GetDefaultVIL(),
                                        {{VAN::Position,   VF_V2F, position_data}});

        if(!geometry)
            return(false);

        // === 创建ECS世界 ===
        ecs_world = std::make_shared<ECSContext>("AutoMergeMaterialInstanceWorld");
        ecs_world->Initialize();

        // === 注册并初始化RenderCollector系统 ===
        render_collector = ecs_world->RegisterSystem<RenderCollector>();
        render_collector->SetWorld(ecs_world);
        render_collector->SetDevice(GetDevice());
        render_collector->Initialize();

        // 配置相机信息（使用NDC坐标系）
        CameraInfo camera;
        camera.position = glm::vec3(0.0f, 0.0f, 10.0f);
        camera.forward = glm::vec3(0.0f, 0.0f, -1.0f);
        camera.viewMatrix = glm::mat4(1.0f);
        camera.projectionMatrix = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f); // NDC坐标
        camera.UpdateViewProjection();
        render_collector->SetCameraInfo(&camera);

        // === 为每个材质实例创建Entity ===
        for(uint i=0;i<DRAW_OBJECT_COUNT;i++)
        {
            // 创建Primitive（每个使用不同的材质实例）
            render_obj[i].primitive=CreatePrimitive(geometry,render_obj[i].mi,pipeline);

            if(!render_obj[i].primitive)
                return(false);

            // 创建Entity
            auto entity = ecs_world->CreateEntity<Entity>("Triangle_MI_" + std::to_string(i));

            // 添加TransformComponent并设置旋转
            auto transform = entity->AddComponent<TransformComponent>();
            
            double rad = deg2rad(TRI_ROTATE_ANGLE*i);
            glm::quat rotation = glm::angleAxis((float)rad, glm::vec3(0.0f, 0.0f, 1.0f));
            
            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(rotation);
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);  // 静态对象

            // 添加ECS PrimitiveComponent
            auto ecs_primitive = entity->AddComponent<hgl::ecs::PrimitiveComponent>();
            ecs_primitive->SetPrimitive(render_obj[i].primitive);
            ecs_primitive->SetVisible(true);
        }

        return(true);
    }

public:

    using SimpleECSWorkObject::SimpleECSWorkObject;

    bool Init() override
    {
        if(!InitMaterial())
            return(false);

        if(!InitVBOAndECS())
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
    }

    void Draw(RenderCmdBuffer* cmd_buf) override
    {
        // 使用ECS RenderCollector进行渲染
        if(render_collector && cmd_buf)
        {
            // 收集所有可渲染的Entity
            render_collector->CollectRenderables();
            
            // 渲染收集到的Entity（自动合并材质实例）
            render_collector->Render(cmd_buf);
        }
    }

    ~TestApp()
    {
        // 关闭ECS世界
        if(ecs_world)
        {
            ecs_world->Shutdown();
        }
    }
};//class TestApp:public SimpleECSWorkObject

int os_main(int,os_char **)
{
    return RunSimpleECSFramework<TestApp>(OS_TEXT("AutoMergeMaterialInstance_ECS"),1024,1024);
}

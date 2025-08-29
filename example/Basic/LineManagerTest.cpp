// 该范例演示使用LineManager进行3D线条绘制管理

#include<hgl/WorkManager.h>
#include<hgl/math/Math.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/LineManager.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/FirstPersonCameraControl.h>
#include<hgl/color/Color.h>
#include<hgl/component/MeshComponent.h>

using namespace hgl;
using namespace hgl::graph;

class TestApp:public WorkObject
{
private:

    LineManager* line_manager = nullptr;
    Pipeline* line_pipeline = nullptr;
    MeshComponent* line_component = nullptr;

public:

    using WorkObject::WorkObject;

    ~TestApp()
    {
        SAFE_CLEAR(line_manager);
    }

    bool Init() override
    {
        // 创建线条管理器
        line_manager = new LineManager(this);  // 传入RenderFramework (this)
        
        if (!line_manager->Init())
        {
            LOG_ERROR("Failed to initialize LineManager");
            return false;
        }

        // 添加一些测试线条
        CreateTestLines();

        // 创建渲染管线
        line_pipeline = CreatePipeline(line_manager->GetMaterialInstance());
        if (!line_pipeline)
        {
            LOG_ERROR("Failed to create pipeline");
            return false;
        }

        // 更新网格数据
        if (!line_manager->Update(line_pipeline))
        {
            LOG_ERROR("Failed to update LineManager");
            return false;
        }

        // 创建网格组件
        if (line_manager->HasLines())
        {
            CreateComponentInfo cci(GetSceneRoot());
            
            MeshComponentData* data = new MeshComponentData(line_manager->GetMesh());
            ComponentDataPtr cdp = data;
            
            line_component = CreateComponent<MeshComponent>(&cci, cdp);
            
            if (!line_component)
            {
                LOG_ERROR("Failed to create line component");
                return false;
            }
        }

        SetupCamera();
        return true;
    }

private:

    void CreateTestLines()
    {
        // 创建坐标轴
        line_manager->AddLine(Vector3f(0, 0, 0), Vector3f(5, 0, 0), Color4f(1, 0, 0, 1)); // X轴 - 红色
        line_manager->AddLine(Vector3f(0, 0, 0), Vector3f(0, 5, 0), Color4f(0, 1, 0, 1)); // Y轴 - 绿色
        line_manager->AddLine(Vector3f(0, 0, 0), Vector3f(0, 0, 5), Color4f(0, 0, 1, 1)); // Z轴 - 蓝色

        // 创建一个立方体框架
        const float size = 2.0f;
        const Vector3f corners[8] = {
            Vector3f(-size, -size, -size), Vector3f( size, -size, -size),
            Vector3f( size,  size, -size), Vector3f(-size,  size, -size),
            Vector3f(-size, -size,  size), Vector3f( size, -size,  size),
            Vector3f( size,  size,  size), Vector3f(-size,  size,  size)
        };

        const Color4f cube_color(1, 1, 0, 1); // 黄色立方体

        // 底面
        line_manager->AddLine(corners[0], corners[1], cube_color);
        line_manager->AddLine(corners[1], corners[2], cube_color);
        line_manager->AddLine(corners[2], corners[3], cube_color);
        line_manager->AddLine(corners[3], corners[0], cube_color);

        // 顶面
        line_manager->AddLine(corners[4], corners[5], cube_color);
        line_manager->AddLine(corners[5], corners[6], cube_color);
        line_manager->AddLine(corners[6], corners[7], cube_color);
        line_manager->AddLine(corners[7], corners[4], cube_color);

        // 竖直边
        line_manager->AddLine(corners[0], corners[4], cube_color);
        line_manager->AddLine(corners[1], corners[5], cube_color);
        line_manager->AddLine(corners[2], corners[6], cube_color);
        line_manager->AddLine(corners[3], corners[7], cube_color);

        // 添加一些装饰性的线条
        const Color4f decoration_color(0, 1, 1, 1); // 青色
        const int circle_segments = 16;
        const float radius = 3.0f;
        
        // 在XY平面创建一个圆形
        for (int i = 0; i < circle_segments; ++i)
        {
            float angle1 = (float(i) / circle_segments) * 2.0f * PI;
            float angle2 = (float(i + 1) / circle_segments) * 2.0f * PI;
            
            Vector3f p1(radius * cos(angle1), radius * sin(angle1), 0);
            Vector3f p2(radius * cos(angle2), radius * sin(angle2), 0);
            
            line_manager->AddLine(p1, p2, decoration_color);
        }
    }

    void SetupCamera()
    {
        CameraControl* camera_control = GetCameraControl();
        camera_control->SetPosition(Vector3f(8, 8, 8));
        camera_control->SetTarget(Vector3f(0, 0, 0));
    }
};

int os_main(int, os_char**)
{
    return RunFramework<TestApp>(OS_TEXT("LineManager Test"), 1280, 720);
}
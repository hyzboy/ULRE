// 该范例演示使用光源线段生成器进行3D光源和摄像机范围可视化

#include<hgl/WorkManager.h>
#include<hgl/math/Math.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/LineManager.h>
#include<hgl/graph/LightVizGenerators.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/FirstPersonCameraControl.h>
#include<hgl/color/Color.h>
#include<hgl/component/MeshComponent.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::graph::light_viz_generators;

class LightVizTestApp:public WorkObject
{
private:

    LineManager* line_manager = nullptr;
    MeshComponent* line_component = nullptr;

public:

    using WorkObject::WorkObject;

    ~LightVizTestApp()
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

        // 创建各种光源和摄像机可视化
        CreateLightVisualizations();

        // 更新网格数据
        if (!line_manager->Update())
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

    void CreateLightVisualizations()
    {
        // 创建坐标轴作为参考
        line_manager->AddLine(Vector3f(0, 0, 0), Vector3f(5, 0, 0), Color4f(1, 0, 0, 1)); // X轴 - 红色
        line_manager->AddLine(Vector3f(0, 0, 0), Vector3f(0, 5, 0), Color4f(0, 1, 0, 1)); // Y轴 - 绿色  
        line_manager->AddLine(Vector3f(0, 0, 0), Vector3f(0, 0, 5), Color4f(0, 0, 1, 1)); // Z轴 - 蓝色

        // 1. 创建点光源可视化
        PointLight point_light;
        point_light.position = Vector3f(3, 2, 0);
        
        GeneratePointLightLines(line_manager, point_light, 1.5f, Color4f(1, 1, 0, 1)); // 黄色

        // 2. 创建聚光灯可视化
        SpotLight spot_light;
        spot_light.position = Vector3f(-3, 3, 2);
        spot_light.direction = Vector3f(0.5f, -0.7f, -0.5f);  // 朝向原点附近
        spot_light.coscutoff = 0.8f;  // 大约36度的锥角
        
        GenerateSpotLightLines(line_manager, spot_light, 4.0f, Color4f(0, 1, 1, 1)); // 青色

        // 3. 创建矩形光源可视化
        Vector3f rect_pos(0, 4, -3);
        Vector3f rect_dir(0, -1, 0.2f);  // 朝下稍微倾斜
        rect_dir = Normalize(rect_dir);
        Vector3f rect_up(0, 0.2f, 1);   // 向上
        rect_up = Normalize(rect_up);
        
        GenerateRectLightLines(line_manager, rect_pos, rect_dir, rect_up, 2.0f, 1.5f, Color4f(1, 0, 1, 1)); // 洋红色

        // 4. 创建摄像机视锥体可视化
        Vector3f cam_pos(5, 5, 5);
        Vector3f cam_dir(-0.5f, -0.5f, -0.7f);  // 朝向原点
        cam_dir = Normalize(cam_dir);
        Vector3f cam_up(0, 1, 0);
        
        GenerateCameraFrustumLines(line_manager, cam_pos, cam_dir, cam_up, 
                                 PI * 0.3f,  // 54度 FOV
                                 16.0f / 9.0f,  // 16:9 宽高比
                                 0.5f,  // 近裁剪面
                                 8.0f,  // 远裁剪面
                                 Color4f(1, 0.5f, 0, 1)); // 橙色

        // 5. 添加一个第二个点光源在不同位置
        PointLight point_light2;
        point_light2.position = Vector3f(-2, -2, 3);
        
        GeneratePointLightLines(line_manager, point_light2, 1.0f, Color4f(0.5f, 1, 0.5f, 1)); // 浅绿色

        // 6. 添加一个第二个聚光灯
        SpotLight spot_light2;
        spot_light2.position = Vector3f(2, -3, -1);
        spot_light2.direction = Vector3f(-0.3f, 0.8f, 0.5f);
        spot_light2.coscutoff = 0.9f;  // 更窄的锥角
        
        GenerateSpotLightLines(line_manager, spot_light2, 3.5f, Color4f(1, 0.5f, 1, 1)); // 浅洋红色
    }

    void SetupCamera()
    {
        CameraControl* camera_control = GetCameraControl();
        camera_control->SetPosition(Vector3f(10, 10, 10));
        camera_control->SetTarget(Vector3f(0, 0, 0));
    }
};

int os_main(int, os_char**)
{
    return RunFramework<LightVizTestApp>(OS_TEXT("Light Visualization Test"), 1280, 720);
}
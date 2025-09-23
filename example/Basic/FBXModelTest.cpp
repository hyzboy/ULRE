#include<hgl/WorkManager.h>
#include<hgl/graph/VKVertexInputConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/component/MeshComponent.h>
#include<hgl/graph/PrimitiveCreater.h>

// 注意：这个例子假设我们有一个简单的FBX文件可以测试
// 由于我们暂时没有实际的FBX SDK集成，这主要是展示架构

using namespace hgl;
using namespace hgl::graph;

class FBXModelTestApp : public WorkObject
{
private:
    MaterialInstance*   material_instance = nullptr;
    Mesh*              test_mesh = nullptr;
    Pipeline*          pipeline = nullptr;

    // 用于测试的简单立方体数据（模拟从FBX加载的数据）
    static constexpr uint32_t VERTEX_COUNT = 8;
    static constexpr uint32_t INDEX_COUNT = 36; // 12个三角形，每个三角形3个顶点
    
    // 立方体顶点位置
    static constexpr float positions[VERTEX_COUNT * 3] = {
        -0.5f, -0.5f, -0.5f,  // 0
         0.5f, -0.5f, -0.5f,  // 1
         0.5f,  0.5f, -0.5f,  // 2
        -0.5f,  0.5f, -0.5f,  // 3
        -0.5f, -0.5f,  0.5f,  // 4
         0.5f, -0.5f,  0.5f,  // 5
         0.5f,  0.5f,  0.5f,  // 6
        -0.5f,  0.5f,  0.5f   // 7
    };
    
    // 立方体法线（每个顶点的法线）
    static constexpr float normals[VERTEX_COUNT * 3] = {
        -0.577f, -0.577f, -0.577f,  // 0
         0.577f, -0.577f, -0.577f,  // 1
         0.577f,  0.577f, -0.577f,  // 2
        -0.577f,  0.577f, -0.577f,  // 3
        -0.577f, -0.577f,  0.577f,  // 4
         0.577f, -0.577f,  0.577f,  // 5
         0.577f,  0.577f,  0.577f,  // 6
        -0.577f,  0.577f,  0.577f   // 7
    };
    
    // 立方体索引
    static constexpr uint32_t indices[INDEX_COUNT] = {
        // 前面
        0, 1, 2,  2, 3, 0,
        // 后面
        4, 7, 6,  6, 5, 4,
        // 左面
        0, 3, 7,  7, 4, 0,
        // 右面
        1, 5, 6,  6, 2, 1,
        // 上面
        3, 2, 6,  6, 7, 3,
        // 下面
        0, 4, 5,  5, 1, 0
    };

private:
    bool InitMaterial()
    {
        // 创建3D材质配置
        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);
        
        VILConfig vil_config;
        vil_config.Add(VAN::Position, VF_V3F);  // 顶点位置
        vil_config.Add(VAN::Normal,   VF_V3F);  // 顶点法线
        
        // 创建基础3D材质实例
        material_instance = CreateMaterialInstance(mtl::inline_material::Solid3D, &cfg, &vil_config);
        
        if(!material_instance)
        {
            LOG_ERROR(OS_TEXT("Failed to create material instance"));
            return false;
        }
        
        // 创建渲染管线
        pipeline = CreatePipeline(material_instance, InlinePipeline::Solid3D);
        
        if(!pipeline)
        {
            LOG_ERROR(OS_TEXT("Failed to create pipeline"));
            return false;
        }
        
        return true;
    }
    
    bool InitMesh()
    {
        // 创建网格（模拟从FBX加载的数据）
        test_mesh = CreateMesh("FBXTestCube", VERTEX_COUNT, material_instance, pipeline,
                              {
                                  {VAN::Position, VF_V3F, positions},
                                  {VAN::Normal,   VF_V3F, normals}
                              },
                              indices, INDEX_COUNT);
        
        if(!test_mesh)
        {
            LOG_ERROR(OS_TEXT("Failed to create mesh"));
            return false;
        }
        
        // 创建网格组件
        CreateComponentInfo cci(GetSceneRoot());
        bool result = CreateComponent<MeshComponent>(&cci, test_mesh);
        
        if(!result)
        {
            LOG_ERROR(OS_TEXT("Failed to create mesh component"));
            return false;
        }
        
        LOG_INFO(OS_TEXT("Successfully created FBX test mesh with ") + 
                UTF8String(VERTEX_COUNT) + OS_TEXT(" vertices and ") + 
                UTF8String(INDEX_COUNT / 3) + OS_TEXT(" triangles"));
        
        return true;
    }

public:
    using WorkObject::WorkObject;
    
    bool Init() override
    {
        LOG_INFO(OS_TEXT("=== FBX Model Test Application ==="));
        LOG_INFO(OS_TEXT("This example demonstrates the FBX loader architecture"));
        LOG_INFO(OS_TEXT("Currently showing a test cube (FBX loader will replace this)"));
        
        if(!InitMaterial())
        {
            LOG_ERROR(OS_TEXT("Failed to initialize material"));
            return false;
        }
        
        if(!InitMesh())
        {
            LOG_ERROR(OS_TEXT("Failed to initialize mesh"));
            return false;
        }
        
        // 设置相机位置以便观察立方体
        auto camera = GetCamera();
        if(camera)
        {
            camera->SetPosition(Vector3f(2.0f, 2.0f, 2.0f));
            camera->LookAt(Vector3f(0.0f, 0.0f, 0.0f));
        }
        
        LOG_INFO(OS_TEXT("FBX Model Test Application initialized successfully"));
        return true;
    }
    
    void Update(float time_step) override
    {
        // 简单的旋转动画
        if(test_mesh)
        {
            static float rotation_angle = 0.0f;
            rotation_angle += time_step * 45.0f; // 每秒45度
            
            Matrix4f rotation_matrix;
            rotation_matrix.identity();
            rotation_matrix.Rotate(rotation_angle, Vector3f(0, 1, 0)); // 绕Y轴旋转
            
            // 这里应该设置网格的变换矩阵，但需要通过SceneNode来实现
            // 现在先留空，专注于架构展示
        }
        
        WorkObject::Update(time_step);
    }
};//class FBXModelTestApp

int os_main(int argc, os_char** argv)
{
    LOG_INFO(OS_TEXT("Starting FBX Model Test Application"));
    
    // 如果提供了FBX文件路径，可以在这里尝试加载
    if(argc > 1)
    {
        OSString fbx_file = argv[1];
        LOG_INFO(OS_TEXT("FBX file specified: ") + fbx_file);
        
        // 这里将来可以集成实际的FBX加载代码
        /*
        FBXLoader loader;
        if(loader.LoadFile(fbx_file))
        {
            LOG_INFO(OS_TEXT("Successfully loaded FBX file"));
            loader.PrintSceneInfo();
        }
        else
        {
            LOG_ERROR(OS_TEXT("Failed to load FBX file"));
        }
        */
    }
    else
    {
        LOG_INFO(OS_TEXT("No FBX file specified, showing test cube"));
        LOG_INFO(OS_TEXT("Usage: FBXModelTest <path_to_fbx_file>"));
    }
    
    return RunFramework<FBXModelTestApp>(OS_TEXT("FBX Model Test"));
}
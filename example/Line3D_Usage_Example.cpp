// Line3D Material Example
// 演示如何使用Line3D材质从点数据生成线条

#include <hgl/graph/mtl/Material3DCreateConfig.h>
#include <hgl/graph/VKRenderResource.h>

using namespace hgl;
using namespace hgl::graph;

/*
使用示例:

// 1. 创建Line3D材质配置
mtl::Line3DMaterialCreateConfig line_cfg;
line_cfg.line_width = 2.0f;  // 设置线条宽度（保留用于未来扩展）

// 2. 创建材质实例
MaterialInstance* material_instance = CreateMaterialInstance(mtl::inline_material::Line3D, &line_cfg);

// 3. 创建管线
Pipeline* pipeline = CreatePipeline(material_instance, InlinePipeline::Solid3D);

// 4. 准备顶点数据
// 每个顶点包含：起点位置(Position)、终点位置(Normal)、颜色(Color)
struct LineVertexData {
    Vector3f start_pos;     // 映射到Position属性 - 线段起点
    Vector3f end_pos;       // 映射到Normal属性 - 线段终点  
    Vector4f color;         // 映射到Color属性 - 线段颜色
};

// 示例：创建三条坐标轴线
LineVertexData line_data[] = {
    // X轴 - 红色线条
    { Vector3f(0,0,0), Vector3f(10,0,0), Vector4f(1,0,0,1) },
    
    // Y轴 - 绿色线条  
    { Vector3f(0,0,0), Vector3f(0,10,0), Vector4f(0,1,0,1) },
    
    // Z轴 - 蓝色线条
    { Vector3f(0,0,0), Vector3f(0,0,10), Vector4f(0,0,1,1) }
};

// 5. 创建顶点缓冲区和渲染对象
// DeviceBuffer* vertex_buffer = db->CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(line_data), line_data);
// Primitive* primitive = db->CreatePrimitive(vertex_buffer, PrimitiveType::Points, 3);
// Mesh* mesh = db->CreateMesh(primitive, material_instance, pipeline);

特性说明:
- 使用POINTS作为输入原语类型，每个点包含线段的完整信息
- 几何着色器将每个点扩展为一条线段（2个顶点）
- 输出为LINES原语类型
- 支持相机变换(camera.vp)和本地到世界变换(local-to-world)
- 每条线可以有独立的颜色
- 适用于需要动态生成线条的场景，如调试线条、网格线等

注意事项:
- Position属性存储线段起点
- Normal属性被重用来存储线段终点（语义上不完美但实用）
- 支持3D空间中的任意线段，不仅限于从原点开始的线条
*/
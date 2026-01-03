# 内联几何体库 - 架构设计文档

## 📐 设计目标

### 核心原则
1. **独立性**：核心算法不依赖特定图形API
2. **可扩展**：易于添加新的几何体类型
3. **高性能**：零拷贝设计，最小化内存分配
4. **易用性**：简洁的API，清晰的文档
5. **可测试**：纯数据驱动，易于单元测试

---

## 🏗️ 层级架构

```
┌─────────────────────────────────────────────────────────────┐
│                    Layer 4: 用户接口层                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ InlineGeometry│  │ CreateInfo   │  │ Geometry     │     │
│  │   API        │  │   结构体      │  │   对象       │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
└─────────────────────────────────────────────────────────────┘
                             ↓
┌─────────────────────────────────────────────────────────────┐
│                    Layer 3: 算法实现层                       │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐      │
│  │ Cube     │ │ Sphere   │ │ Cylinder │ │ Revolution│      │
│  │ 算法     │ │ 算法     │ │ 算法     │ │ 算法      │      │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘      │
│         [48 个几何体算法实现]                               │
└─────────────────────────────────────────────────────────────┘
                             ↓
┌─────────────────────────────────────────────────────────────┐
│                    Layer 2: 构建工具层                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ GeometryBuilder│ │IndexGenerator│ │GeometryValidator│   │
│  │ 顶点数据写入  │ │ 索引生成    │ │ 参数验证      │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
└─────────────────────────────────────────────────────────────┘
                             ↓
┌─────────────────────────────────────────────────────────────┐
│                    Layer 1: 抽象接口层                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │IGeometry     │  │ GeometryData │  │ VAB/IB       │     │
│  │Builder       │  │   (纯数据)    │  │   抽象       │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
└─────────────────────────────────────────────────────────────┘
                             ↓
┌─────────────────────────────────────────────────────────────┐
│                    Layer 0: 后端适配层                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ Vulkan       │  │ DirectX 12   │  │ Metal        │     │
│  │ Adapter      │  │ Adapter      │  │ Adapter      │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

---

## 📦 模块划分

### 模块 1: Core（核心模块）- 无外部依赖

**职责：**
- 定义几何数据结构
- 提供几何体生成算法
- 顶点和索引计算

**接口示例：**
```cpp
namespace hgl::inline_geometry::core
{
    // 纯数据结构
    struct VertexData
    {
        std::vector<float> positions;    // xyz, xyz, ...
        std::vector<float> normals;      // xyz, xyz, ...
        std::vector<float> tangents;     // xyz, xyz, ...
        std::vector<float> texcoords;    // uv, uv, ...
    };
    
    struct GeometryData
    {
        VertexData vertices;
        std::vector<uint32_t> indices;
        std::string name;
    };
    
    // 几何体生成函数（纯数据输出）
    bool GenerateCube(GeometryData& output, const CubeConfig& config);
    bool GenerateSphere(GeometryData& output, const SphereConfig& config);
    // ... 其他几何体
}
```

**依赖：**
- C++ 标准库
- 数学库（Vector, Matrix - 可以是 GLM 或自定义）

---

### 模块 2: Builder（构建器模块）

**职责：**
- 提供高级构建接口
- 管理顶点属性映射
- 提供便利方法

**接口示例：**
```cpp
namespace hgl::inline_geometry::builder
{
    // 抽象构建器接口
    class IGeometryBuilder
    {
    public:
        virtual void WriteVertex(float x, float y, float z) = 0;
        virtual void WriteNormal(float x, float y, float z) = 0;
        virtual void WriteTangent(float x, float y, float z) = 0;
        virtual void WriteTexCoord(float u, float v) = 0;
        virtual void WriteIndex(uint32_t index) = 0;
        
        virtual bool HasNormals() const = 0;
        virtual bool HasTangents() const = 0;
        virtual bool HasTexCoords() const = 0;
        
        virtual ~IGeometryBuilder() = default;
    };
    
    // 基于 GeometryData 的实现
    class DataBuilder : public IGeometryBuilder
    {
        GeometryData& data_;
    public:
        explicit DataBuilder(GeometryData& data) : data_(data) {}
        
        void WriteVertex(float x, float y, float z) override
        {
            data_.vertices.positions.push_back(x);
            data_.vertices.positions.push_back(y);
            data_.vertices.positions.push_back(z);
        }
        // ... 其他实现
    };
}
```

---

### 模块 3: Adapters（适配器模块）

**职责：**
- 连接核心模块与具体图形API
- 处理API特定的资源管理
- 提供向后兼容接口

**Vulkan 适配器示例：**
```cpp
namespace hgl::inline_geometry::adapters::vulkan
{
    class VulkanGeometryAdapter
    {
        VulkanDevice* device_;
        GeometryCreater* creater_;
        
    public:
        VulkanGeometryAdapter(VulkanDevice* device);
        
        // 从 GeometryData 创建 Vulkan Geometry
        Geometry* CreateFromData(const core::GeometryData& data);
        
        // 兼容旧接口
        Geometry* CreateCube(const CubeCreateInfo* info)
        {
            core::GeometryData data;
            core::CubeConfig config = ConvertConfig(info);
            
            if(!core::GenerateCube(data, config))
                return nullptr;
            
            return CreateFromData(data);
        }
    };
}
```

---

## 🔄 数据流

### 方式 1: 纯数据流（推荐用于测试和导出）

```
用户代码
    ↓
配置结构体 (CubeConfig)
    ↓
核心算法 (GenerateCube)
    ↓
几何数据 (GeometryData)
    ↓
导出/序列化 (OBJ, FBX, etc.)
```

**示例：**
```cpp
// 生成立方体数据
core::GeometryData cube_data;
core::CubeConfig config;
config.size = 1.0f;
config.generate_normals = true;

core::GenerateCube(cube_data, config);

// 导出为 OBJ
exporter::SaveAsOBJ(cube_data, "cube.obj");
```

---

### 方式 2: 直接渲染流（用于实时渲染）

```
用户代码
    ↓
CreateInfo 结构体
    ↓
适配器 (VulkanAdapter)
    ↓
    ├─→ 核心算法 (生成数据)
    └─→ 资源管理 (创建 GPU 缓冲)
    ↓
Geometry 对象 (可直接渲染)
```

**示例：**
```cpp
// Vulkan 渲染路径
VulkanGeometryAdapter adapter(device);

CubeCreateInfo info;
info.normal = true;
info.tangent = true;

Geometry* cube = adapter.CreateCube(&info);
// cube 可以直接用于渲染
```

---

## 🧩 关键设计决策

### 决策 1: 分离数据和资源

**问题：** 当前实现数据生成和GPU资源创建耦合在一起

**解决方案：**
```cpp
// 分离前（旧）
Geometry* CreateCube(GeometryCreater* pc, ...);  // 直接创建GPU资源

// 分离后（新）
// 步骤1: 生成数据
core::GeometryData data;
core::GenerateCube(data, config);

// 步骤2: 创建GPU资源（可选）
Geometry* gpu_geom = adapter.CreateFromData(data);
```

**优势：**
- 数据可以在CPU端测试
- 数据可以序列化保存
- 同一份数据可用于不同API
- 便于单元测试

---

### 决策 2: 使用接口而非模板

**问题：** GeometryBuilder 当前是具体类，难以替换

**解决方案：**
```cpp
// 定义接口
class IGeometryBuilder { /* ... */ };

// 不同实现
class DataBuilder : public IGeometryBuilder { /* 写入 vector */ };
class VulkanBuilder : public IGeometryBuilder { /* 直接写入映射内存 */ };
class TestBuilder : public IGeometryBuilder { /* 用于测试 */ };

// 算法使用接口
void GenerateCube(IGeometryBuilder* builder, const CubeConfig& config)
{
    builder->WriteVertex(0.5f, 0.5f, 0.5f);
    // ...
}
```

**优势：**
- 易于测试（Mock Builder）
- 支持不同后端
- 运行时多态

---

### 决策 3: 配置与数据分离

**问题：** 当前 CreateInfo 结构体包含过多状态

**解决方案：**
```cpp
// 几何配置（输入）
struct CubeConfig
{
    float size = 1.0f;
    bool generate_normals = true;
    bool generate_tangents = true;
    bool generate_uvs = true;
};

// 几何数据（输出）
struct GeometryData
{
    VertexData vertices;
    std::vector<uint32_t> indices;
};

// 清晰的输入输出
bool GenerateCube(GeometryData& output, const CubeConfig& input);
```

---

## 🔌 扩展点

### 扩展点 1: 添加新几何体

```cpp
// 1. 定义配置
struct MyGeometryConfig
{
    float parameter1;
    uint32_t parameter2;
    // ...
};

// 2. 实现算法
bool GenerateMyGeometry(GeometryData& output, const MyGeometryConfig& config)
{
    // 计算顶点和索引
    // 写入 output
    return true;
}

// 3. 在适配器中添加便利函数（可选）
Geometry* VulkanAdapter::CreateMyGeometry(const MyGeometryConfig& config)
{
    GeometryData data;
    if(!GenerateMyGeometry(data, config))
        return nullptr;
    return CreateFromData(data);
}
```

---

### 扩展点 2: 支持新图形API

```cpp
// 1. 实现 IGeometryBuilder 接口
class DirectX12Builder : public IGeometryBuilder
{
    ID3D12Resource* vertex_buffer_;
    void* mapped_data_;
    
public:
    void WriteVertex(float x, float y, float z) override
    {
        // 写入 D3D12 缓冲
    }
    // ...
};

// 2. 创建适配器
class DirectX12Adapter
{
public:
    Geometry* CreateFromData(const GeometryData& data)
    {
        // 创建 D3D12 资源
        // 上传数据
        // 返回 Geometry 对象
    }
};
```

---

### 扩展点 3: 自定义顶点格式

```cpp
// 1. 定义顶点结构
struct MyVertex
{
    Vector3f position;
    Vector3f normal;
    Vector2f uv;
    Color4f color;
};

// 2. 实现转换
std::vector<MyVertex> ConvertToCustomFormat(const GeometryData& data)
{
    std::vector<MyVertex> result;
    const size_t count = data.vertices.positions.size() / 3;
    
    for(size_t i = 0; i < count; ++i)
    {
        MyVertex v;
        v.position.x = data.vertices.positions[i * 3 + 0];
        v.position.y = data.vertices.positions[i * 3 + 1];
        v.position.z = data.vertices.positions[i * 3 + 2];
        // ... 填充其他字段
        result.push_back(v);
    }
    
    return result;
}
```

---

## 📊 性能考虑

### 1. 零拷贝设计

**当前实现（优化）：**
```cpp
// 直接映射 GPU 内存，无拷贝
VABMapFloat vertex_map(pc->GetVABMap(VAN::Position), VF_V3F);
float* vp = vertex_map;

// 直接写入
*vp++ = x; *vp++ = y; *vp++ = z;
```

**未来优化：**
```cpp
// 预分配容量，减少 reallocation
data.vertices.positions.reserve(expected_vertex_count * 3);

// 使用 emplace_back 避免临时对象
data.vertices.positions.emplace_back(x);
```

---

### 2. 内存布局优化

**选项 A: SoA (Structure of Arrays) - 当前方案**
```cpp
struct VertexData
{
    std::vector<float> positions;  // [x,y,z, x,y,z, ...]
    std::vector<float> normals;    // [x,y,z, x,y,z, ...]
    std::vector<float> texcoords;  // [u,v, u,v, ...]
};
```
- ✅ 灵活（可选属性）
- ✅ GPU友好（分离缓冲）
- ⚠️ 缓存不友好

**选项 B: AoS (Array of Structures)**
```cpp
struct Vertex
{
    float position[3];
    float normal[3];
    float texcoord[2];
};

std::vector<Vertex> vertices;
```
- ✅ 缓存友好
- ✅ 易于理解
- ⚠️ 不灵活（固定格式）

**推荐：** 保持 SoA，提供转换函数

---

### 3. 索引类型优化

```cpp
// 自动选择索引类型
IndexType DetermineIndexType(uint32_t vertex_count)
{
    if(vertex_count <= 65535)
        return IndexType::U16;  // 2字节，节省内存
    else
        return IndexType::U32;  // 4字节
}
```

---

## 🧪 测试策略

### 单元测试（Core 模块）

```cpp
TEST_CASE("Cube generation", "[geometry][cube]")
{
    core::GeometryData data;
    core::CubeConfig config;
    
    REQUIRE(core::GenerateCube(data, config));
    
    // 验证顶点数
    REQUIRE(data.vertices.positions.size() == 24 * 3);
    
    // 验证索引数
    REQUIRE(data.indices.size() == 36);
    
    // 验证顶点范围
    for(size_t i = 0; i < data.vertices.positions.size(); ++i)
    {
        REQUIRE(data.vertices.positions[i] >= -0.5f);
        REQUIRE(data.vertices.positions[i] <= 0.5f);
    }
    
    // 验证法线归一化
    // ...
}
```

---

### 集成测试（Adapter 模块）

```cpp
TEST_CASE("Vulkan adapter", "[adapter][vulkan]")
{
    VulkanTestContext ctx;  // 测试用 Vulkan 上下文
    
    VulkanAdapter adapter(ctx.device);
    CubeCreateInfo info;
    
    Geometry* geom = adapter.CreateCube(&info);
    
    REQUIRE(geom != nullptr);
    REQUIRE(geom->GetVertexCount() == 24);
    REQUIRE(geom->GetIndexCount() == 36);
    
    // 清理
    delete geom;
}
```

---

### 渲染测试

```cpp
// 渲染到纹理，对比像素
TEST_CASE("Cube rendering", "[render][cube]")
{
    // 1. 渲染参考图像
    Image reference = RenderReference("cube_baseline.png");
    
    // 2. 渲染当前实现
    Geometry* cube = CreateCube(...);
    Image result = RenderGeometry(cube);
    
    // 3. 对比图像
    float diff = CompareImages(reference, result);
    REQUIRE(diff < 0.01f);  // 允许小误差
}
```

---

## 📚 使用示例

### 示例 1: 基础使用（纯数据）

```cpp
#include <hgl_inline_geometry/core.h>
#include <hgl_inline_geometry/export.h>

int main()
{
    // 生成立方体数据
    hgl::inline_geometry::core::GeometryData cube;
    hgl::inline_geometry::core::CubeConfig config;
    
    config.size = 2.0f;
    config.generate_normals = true;
    
    if(!hgl::inline_geometry::core::GenerateCube(cube, config))
    {
        std::cerr << "Failed to generate cube" << std::endl;
        return 1;
    }
    
    // 导出为 OBJ
    hgl::inline_geometry::export_obj(cube, "my_cube.obj");
    
    return 0;
}
```

---

### 示例 2: Vulkan 集成

```cpp
#include <hgl/graph/geo/InlineGeometry.h>

void CreateScene(VulkanDevice* device)
{
    GeometryCreater creater(device, vertex_input_layout);
    
    // 创建立方体
    CubeCreateInfo cube_info;
    Geometry* cube = CreateCube(&creater, &cube_info);
    
    // 创建球体
    Geometry* sphere = CreateSphere(&creater, 32);
    
    // 添加到场景...
}
```

---

### 示例 3: 批量生成

```cpp
// 批量生成多个几何体（共享数据）
std::vector<core::GeometryData> geometries;

for(int i = 0; i < 100; ++i)
{
    core::GeometryData data;
    core::SphereConfig config;
    config.radius = 0.5f + i * 0.01f;
    config.slices = 16;
    
    core::GenerateSphere(data, config);
    geometries.push_back(std::move(data));
}

// 一次性上传到 GPU
adapter.BatchUpload(geometries);
```

---

## 🗺️ 迁移路径

### 阶段 1: 内部重构（向后兼容）
```cpp
// 用户代码不变
Geometry* cube = CreateCube(pc, &info);

// 内部实现改为：
Geometry* CreateCube(GeometryCreater* pc, const CubeCreateInfo* info)
{
    // 1. 生成数据
    core::GeometryData data;
    core::CubeConfig config = ConvertConfig(info);
    core::GenerateCube(data, config);
    
    // 2. 上传到 GPU
    return UploadToGPU(pc, data);
}
```

---

### 阶段 2: 提供新API
```cpp
// 新 API（推荐）
core::GeometryData data;
core::GenerateCube(data, config);

// 旧 API（兼容）
Geometry* cube = CreateCube(pc, &info);  // 内部调用新 API
```

---

### 阶段 3: 弃用旧API
```cpp
// 标记为废弃
[[deprecated("Use core::GenerateCube instead")]]
Geometry* CreateCube(GeometryCreater* pc, const CubeCreateInfo* info);
```

---

### 阶段 4: 移除旧API（主版本升级）
```cpp
// 仅保留新 API
namespace hgl::inline_geometry::core
{
    bool GenerateCube(GeometryData& output, const CubeConfig& config);
}
```

---

## 📖 总结

这个架构设计实现了：

✅ **独立性** - 核心算法不依赖图形API  
✅ **灵活性** - 支持多种后端和自定义格式  
✅ **可测试性** - 纯数据驱动，易于单元测试  
✅ **高性能** - 零拷贝设计，内存高效  
✅ **向后兼容** - 保持现有接口，渐进迁移  
✅ **易扩展** - 清晰的扩展点，易于添加新功能  

**下一步：** 参考 [Quick_Start_Guide.md](Quick_Start_Guide.md) 开始实施！

---

*架构文档版本：v1.0*  
*最后更新：2026-01-02*

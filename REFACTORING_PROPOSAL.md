# 几何体代码重构方案 - 纯数据与引擎输出完全分离

## 1. 重构目标

将几何体生成代码分离为两层：
1. **纯数据层（Pure Data Layer）**：只负责计算顶点和索引数据，不依赖任何渲染引擎
2. **引擎适配层（Engine Adapter Layer）**：负责将纯数据写入到具体的渲染引擎（当前是Vulkan）

## 2. 整体架构设计

```
┌─────────────────────────────────────────────────────────────┐
│  应用层 (Application Layer)                                  │
│  - 调用几何体创建函数                                         │
│  - 传入CreateInfo参数                                        │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│  纯数据层 (Pure Data Layer)                                  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ GeometryData (新)                                      │  │
│  │ - std::vector<Vertex> vertices                        │  │
│  │ - std::vector<uint32_t> indices                       │  │
│  │ - BoundingVolumes bounds                              │  │
│  │ - VertexLayout layout                                 │  │
│  └───────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ Geometry Generators (重构)                             │  │
│  │ - GenerateSphere(info) -> GeometryData               │  │
│  │ - GenerateCube(info) -> GeometryData                 │  │
│  │ - GenerateCylinder(info) -> GeometryData             │  │
│  │ - ... (50+ 个生成器)                                  │  │
│  └───────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ Helpers (纯数学函数)                                   │  │
│  │ - QuaternionRotate, MatrixMultiply, etc.             │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│  引擎适配层 (Engine Adapter Layer)                           │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ IGeometryWriter (接口)                                 │  │
│  │ - WriteVertex(pos, normal, texcoord, ...)            │  │
│  │ - WriteIndex(index)                                   │  │
│  │ - Finalize() -> EngineGeometry                       │  │
│  └───────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ VulkanGeometryWriter (实现)                            │  │
│  │ - 使用 GeometryCreater                                │  │
│  │ - 写入 VAB, IBO                                       │  │
│  │ - 创建 Geometry 对象                                  │  │
│  └───────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ GeometryAdapter (工具类)                               │  │
│  │ - ToVulkan(GeometryData, GeometryCreater)            │  │
│  │ - ToOpenGL(...) [未来扩展]                            │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│  渲染引擎层 (Rendering Engine Layer)                         │
│  - Vulkan API                                               │
│  - VAB, IBO, Geometry                                       │
└─────────────────────────────────────────────────────────────┘
```

## 3. 核心数据结构设计

### 3.1 纯数据层数据结构

```cpp
// 位置: inc/hgl/graph/data/GeometryData.h
namespace hgl::graph::data
{
    // 顶点数据（支持多种布局）
    struct Vertex
    {
        Vector3f position;
        Vector3f normal;      // optional
        Vector3f tangent;     // optional
        Vector2f texcoord;    // optional
        Vector4f color;       // optional
    };

    // 顶点布局描述
    struct VertexLayout
    {
        bool has_position = true;   // 必须
        bool has_normal = false;
        bool has_tangent = false;
        bool has_texcoord = false;
        bool has_color = false;

        size_t GetVertexSize() const;
        bool IsCompatible(const VertexLayout &other) const;
    };

    // 几何体数据（纯数据，无渲染依赖）
    class GeometryData
    {
    public:
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        VertexLayout layout;
        BoundingVolumes bounds;
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;

        // 工具方法
        void CalculateBounds();
        void CalculateNormals();
        void CalculateTangents();
        void Optimize();  // 去重、索引优化
        
        // 验证
        bool Validate() const;
        
        // 统计信息
        size_t GetVertexCount() const { return vertices.size(); }
        size_t GetIndexCount() const { return indices.size(); }
        size_t GetTriangleCount() const { return indices.size() / 3; }
    };

    // 几何体生成器的返回类型
    using GeometryDataPtr = std::shared_ptr<GeometryData>;
}
```

### 3.2 生成器函数接口

```cpp
// 位置: inc/hgl/graph/data/GeometryGenerators.h
namespace hgl::graph::data
{
    // 所有生成器函数的统一签名
    // 输入: CreateInfo参数
    // 输出: GeometryData（纯数据，无渲染依赖）

    // 基础形状
    GeometryDataPtr GenerateRectangle(const RectangleCreateInfo &info);
    GeometryDataPtr GenerateCircle(const CircleCreateInfo &info);
    GeometryDataPtr GenerateCube(const CubeCreateInfo &info);
    GeometryDataPtr GeneratePlane(const PlaneCreateInfo &info);

    // 球体类
    GeometryDataPtr GenerateSphere(uint32_t slices);
    GeometryDataPtr GenerateDome(uint32_t slices);
    GeometryDataPtr GenerateHexSphere(const HexSphereCreateInfo &info);

    // 柱体类
    GeometryDataPtr GenerateCylinder(const CylinderCreateInfo &info);
    GeometryDataPtr GenerateHollowCylinder(const HollowCylinderCreateInfo &info);
    GeometryDataPtr GenerateCone(const ConeCreateInfo &info);
    GeometryDataPtr GenerateTorus(const TorusCreateInfo &info);

    // ... 其他50+个生成器
    
    // 高级生成器
    GeometryDataPtr GenerateExtrudedPolygon(const ExtrudedPolygonCreateInfo &info);
    GeometryDataPtr GenerateRevolution(const RevolutionCreateInfo &info);
    GeometryDataPtr GenerateWall(const WallCreateInfo &info);
}
```

### 3.3 引擎适配层接口

```cpp
// 位置: inc/hgl/graph/adapter/GeometryAdapter.h
namespace hgl::graph
{
    // 将 GeometryData 转换为 Vulkan Geometry
    Geometry* ToVulkanGeometry(
        const data::GeometryData &geom_data,
        GeometryCreater *creater,
        const AnsiString &name
    );

    // 或者使用更现代的接口
    Geometry* CreateGeometry(
        VulkanDevice *device,
        const VIL *vil,
        const data::GeometryData &geom_data,
        const AnsiString &name
    );
}
```

## 4. 详细重构步骤

### 第一阶段：创建纯数据层基础设施

#### 步骤 1.1: 创建新的目录结构

```
inc/hgl/graph/data/           # 新增：纯数据层
├── GeometryData.h            # 核心数据结构
├── VertexLayout.h            # 顶点布局
├── GeometryGenerators.h      # 生成器函数声明
└── GeometryDataUtils.h       # 工具函数

src/SceneGraph/data/          # 新增：纯数据层实现
├── GeometryData.cpp
├── VertexLayout.cpp
└── GeometryDataUtils.cpp

inc/hgl/graph/adapter/        # 新增：适配层
└── GeometryAdapter.h         # Vulkan适配器

src/SceneGraph/adapter/       # 新增：适配层实现
└── VulkanGeometryAdapter.cpp
```

#### 步骤 1.2: 实现 GeometryData 类

```cpp
// src/SceneGraph/data/GeometryData.cpp
namespace hgl::graph::data
{
    void GeometryData::CalculateBounds()
    {
        if(vertices.empty())
            return;

        Vector3f min = vertices[0].position;
        Vector3f max = vertices[0].position;

        for(const auto &v : vertices)
        {
            min = Vector3f::Min(min, v.position);
            max = Vector3f::Max(max, v.position);
        }

        bounds.SetFromAABB(min, max);
    }

    void GeometryData::CalculateNormals()
    {
        // 实现法线计算算法
        // 基于三角形面法线的平均
    }

    bool GeometryData::Validate() const
    {
        if(vertices.empty())
            return false;

        if(topology == PrimitiveTopology::TriangleList)
        {
            if(indices.size() % 3 != 0)
                return false;
        }

        // 检查索引范围
        for(auto idx : indices)
        {
            if(idx >= vertices.size())
                return false;
        }

        return true;
    }
}
```

### 第二阶段：重构现有几何体生成器

#### 步骤 2.1: 重构 Sphere 生成器（示例）

**原始代码** (`src/SceneGraph/InlineGeometry/SphereDomeTorus.cpp`):
```cpp
Geometry *CreateSphere(GeometryCreater *pc, const uint numberSlices)
{
    // 计算顶点和索引数量
    uint numberParallels = (numberSlices+1) / 2;
    uint numberVertices = (numberParallels + 1) * (numberSlices + 1);
    uint numberIndices = numberParallels * numberSlices * 6;

    // 初始化 GeometryCreater
    if(!pc->Init("Sphere", numberVertices, numberIndices))
        return nullptr;

    // 使用 GeometryBuilder 写入数据
    GeometryBuilder builder(pc);
    
    for(/* 生成顶点 */)
    {
        builder.WriteVertex(x, y, z);
        builder.WriteNormal(x, y, z);
        builder.WriteTexCoord(tex_x, tex_y);
    }

    // 生成索引
    IndexGenerator::CreateSphereIndices<uint16>(pc, numberParallels, numberSlices);

    return pc->Create();
}
```

**重构后的纯数据生成器** (`src/SceneGraph/data/generators/Sphere.cpp`):
```cpp
namespace hgl::graph::data
{
    GeometryDataPtr GenerateSphere(uint32_t numberSlices)
    {
        // 参数验证
        if(numberSlices < 3)
            return nullptr;

        auto geom = std::make_shared<GeometryData>();
        
        // 计算顶点和索引数量
        uint32_t numberParallels = (numberSlices + 1) / 2;
        uint32_t numberVertices = (numberParallels + 1) * (numberSlices + 1);
        uint32_t numberIndices = numberParallels * numberSlices * 6;

        // 预分配空间
        geom->vertices.reserve(numberVertices);
        geom->indices.reserve(numberIndices);

        // 设置布局
        geom->layout.has_position = true;
        geom->layout.has_normal = true;
        geom->layout.has_texcoord = true;
        geom->topology = PrimitiveTopology::TriangleList;

        const double angleStep = 2.0 * M_PI / double(numberSlices);

        // 生成顶点
        for(uint32_t i = 0; i <= numberParallels; i++)
        {
            for(uint32_t j = 0; j <= numberSlices; j++)
            {
                Vertex v;
                
                // 位置（同时也是法线，因为是单位球）
                v.position.x = sin(angleStep * i) * sin(angleStep * j);
                v.position.y = sin(angleStep * i) * cos(angleStep * j);
                v.position.z = cos(angleStep * i);
                
                // 法线
                v.normal = v.position;
                
                // 纹理坐标
                v.texcoord.x = float(j) / float(numberSlices);
                v.texcoord.y = 1.0f - float(i) / float(numberParallels);

                geom->vertices.push_back(v);
            }
        }

        // 生成索引
        for(uint32_t i = 0; i < numberParallels; i++)
        {
            for(uint32_t j = 0; j < numberSlices; j++)
            {
                uint32_t i0 = i * (numberSlices + 1) + j;
                uint32_t i1 = (i + 1) * (numberSlices + 1) + (j + 1);
                uint32_t i2 = (i + 1) * (numberSlices + 1) + j;
                uint32_t i3 = i * (numberSlices + 1) + (j + 1);

                // 三角形 1
                geom->indices.push_back(i0);
                geom->indices.push_back(i1);
                geom->indices.push_back(i2);

                // 三角形 2
                geom->indices.push_back(i0);
                geom->indices.push_back(i3);
                geom->indices.push_back(i1);
            }
        }

        // 计算包围体
        geom->CalculateBounds();

        return geom;
    }
}
```

**适配层** (`src/SceneGraph/adapter/VulkanGeometryAdapter.cpp`):
```cpp
namespace hgl::graph
{
    Geometry* ToVulkanGeometry(
        const data::GeometryData &geom_data,
        GeometryCreater *pc,
        const AnsiString &name)
    {
        if(!pc || !geom_data.Validate())
            return nullptr;

        const auto &layout = geom_data.layout;
        const size_t vertex_count = geom_data.GetVertexCount();
        const size_t index_count = geom_data.GetIndexCount();

        // 初始化 GeometryCreater
        if(!pc->Init(name, vertex_count, index_count))
            return nullptr;

        // 写入位置数据（必需）
        if(layout.has_position)
        {
            std::vector<float> positions;
            positions.reserve(vertex_count * 3);
            
            for(const auto &v : geom_data.vertices)
            {
                positions.push_back(v.position.x);
                positions.push_back(v.position.y);
                positions.push_back(v.position.z);
            }
            
            if(!pc->WriteVAB(VAN::Position, VF_V3F, positions.data()))
                return nullptr;
        }

        // 写入法线数据（可选）
        if(layout.has_normal)
        {
            std::vector<float> normals;
            normals.reserve(vertex_count * 3);
            
            for(const auto &v : geom_data.vertices)
            {
                normals.push_back(v.normal.x);
                normals.push_back(v.normal.y);
                normals.push_back(v.normal.z);
            }
            
            pc->WriteVAB(VAN::Normal, VF_V3F, normals.data());
        }

        // 写入切线数据（可选）
        if(layout.has_tangent)
        {
            std::vector<float> tangents;
            tangents.reserve(vertex_count * 3);
            
            for(const auto &v : geom_data.vertices)
            {
                tangents.push_back(v.tangent.x);
                tangents.push_back(v.tangent.y);
                tangents.push_back(v.tangent.z);
            }
            
            pc->WriteVAB(VAN::Tangent, VF_V3F, tangents.data());
        }

        // 写入纹理坐标（可选）
        if(layout.has_texcoord)
        {
            std::vector<float> texcoords;
            texcoords.reserve(vertex_count * 2);
            
            for(const auto &v : geom_data.vertices)
            {
                texcoords.push_back(v.texcoord.x);
                texcoords.push_back(v.texcoord.y);
            }
            
            pc->WriteVAB(VAN::TexCoord, VF_V2F, texcoords.data());
        }

        // 写入颜色数据（可选）
        if(layout.has_color)
        {
            std::vector<float> colors;
            colors.reserve(vertex_count * 4);
            
            for(const auto &v : geom_data.vertices)
            {
                colors.push_back(v.color.r);
                colors.push_back(v.color.g);
                colors.push_back(v.color.b);
                colors.push_back(v.color.a);
            }
            
            pc->WriteVAB(VAN::Color, VF_V4F, colors.data());
        }

        // 写入索引数据
        pc->WriteIBO(geom_data.indices.data());

        // 创建 Geometry 对象
        Geometry *geom = pc->Create();
        if(geom)
        {
            geom->SetBoundingVolumes(geom_data.bounds);
        }

        return geom;
    }
}
```

**兼容层（保持原有API）**:
```cpp
// src/SceneGraph/InlineGeometry/SphereDomeTorus.cpp
// 保持原有接口不变，内部调用新的纯数据生成器

namespace hgl::graph::inline_geometry
{
    Geometry *CreateSphere(GeometryCreater *pc, const uint numberSlices)
    {
        if(!pc)
            return nullptr;

        // 使用纯数据生成器
        auto geom_data = data::GenerateSphere(numberSlices);
        if(!geom_data)
            return nullptr;

        // 通过适配器转换为 Vulkan Geometry
        return ToVulkanGeometry(*geom_data, pc, "Sphere");
    }
}
```

#### 步骤 2.2: 依次重构其他几何体生成器

按照相同的模式重构所有 47 个几何体生成器：
1. Cube, Rectangle, Circle, Plane (基础形状)
2. Dome, HexSphere (球体类)
3. Cylinder, HollowCylinder, Cone, Torus (柱体类)
4. Capsule, TaperedCapsule (胶囊体类)
5. Arrow, Frustum, RoundedBox, ... (扩展几何体)
6. ExtrudedPolygon, Revolution, Wall (高级几何体)

### 第三阶段：重构工具类

#### 步骤 3.1: 重构 IndexGenerator

**原始代码** (依赖 GeometryCreater):
```cpp
template<typename T>
void CreateSphereIndices(GeometryCreater *pc, uint numberParallels, uint numberSlices)
{
    IBTypeMap<T> ib_map(pc->GetIBMap());
    T *tp = ib_map;
    
    for(/* 生成索引 */)
    {
        *tp = ...; ++tp;
    }
}
```

**重构后** (纯函数，无渲染依赖):
```cpp
// inc/hgl/graph/data/IndexGenerators.h
namespace hgl::graph::data
{
    // 生成球体索引（纯函数）
    void GenerateSphereIndices(
        std::vector<uint32_t> &indices,
        uint32_t numberParallels,
        uint32_t numberSlices);

    // 生成圆环索引（纯函数）
    void GenerateTorusIndices(
        std::vector<uint32_t> &indices,
        uint32_t numberSlices,
        uint32_t numberStacks);

    // ... 其他索引生成器
}
```

#### 步骤 3.2: 移除 GeometryBuilder

GeometryBuilder 主要是为了简化 VAB 写入，在纯数据层中不再需要。
直接操作 `std::vector<Vertex>` 更简单直接。

#### 步骤 3.3: 简化 GeometryValidator

```cpp
// inc/hgl/graph/data/GeometryValidator.h
namespace hgl::graph::data
{
    // 纯数值验证，无渲染依赖
    constexpr uint32_t MAX_VERTICES = 1048576;
    constexpr uint32_t MAX_INDICES = MAX_VERTICES * 4;

    inline bool ValidateVertexCount(uint32_t count)
    {
        return count > 0 && count <= MAX_VERTICES;
    }

    inline bool ValidateIndexCount(uint32_t count)
    {
        return count > 0 && count <= MAX_INDICES;
    }

    inline bool ValidateSlices(uint32_t slices, uint32_t min_slices = 3)
    {
        return slices >= min_slices;
    }

    inline bool ValidateStacks(uint32_t stacks, uint32_t min_stacks = 1)
    {
        return stacks >= min_stacks;
    }
}
```

### 第四阶段：提供便捷API

#### 步骤 4.1: 保持向后兼容

保留原有的 `CreateXXX()` 函数，内部调用新的纯数据生成器：

```cpp
// inc/hgl/graph/geo/InlineGeometry.h (保持不变)
namespace hgl::graph::inline_geometry
{
    // 原有接口保持不变
    Geometry *CreateSphere(GeometryCreater *pc, const uint numberSlices);
    Geometry *CreateCube(GeometryCreater *pc, const CubeCreateInfo *cci);
    // ... 其他函数
}

// src/SceneGraph/InlineGeometry/*.cpp (内部实现改为调用新API)
namespace hgl::graph::inline_geometry
{
    Geometry *CreateSphere(GeometryCreater *pc, const uint numberSlices)
    {
        auto geom_data = data::GenerateSphere(numberSlices);
        return ToVulkanGeometry(*geom_data, pc, "Sphere");
    }
    
    Geometry *CreateCube(GeometryCreater *pc, const CubeCreateInfo *cci)
    {
        auto geom_data = data::GenerateCube(*cci);
        return ToVulkanGeometry(*geom_data, pc, "Cube");
    }
}
```

#### 步骤 4.2: 提供新的便捷API

```cpp
// inc/hgl/graph/GeometryFactory.h (新增)
namespace hgl::graph
{
    // 直接从 GeometryData 创建 Vulkan Geometry
    class GeometryFactory
    {
        VulkanDevice *device;
        const VIL *vil;

    public:
        GeometryFactory(VulkanDevice *dev, const VIL *v) : device(dev), vil(v) {}

        // 从 GeometryData 创建
        Geometry* Create(const data::GeometryData &geom_data, const AnsiString &name);

        // 便捷方法（直接生成并创建）
        Geometry* CreateSphere(uint32_t slices, const AnsiString &name = "Sphere");
        Geometry* CreateCube(const CubeCreateInfo &info, const AnsiString &name = "Cube");
        // ... 其他便捷方法
    };
}
```

## 5. 使用示例对比

### 5.1 原始用法（保持不变）

```cpp
// 现有代码可以继续使用，无需修改
GeometryCreater creater(device, vil);
Geometry *sphere = CreateSphere(&creater, 32);
```

### 5.2 新的纯数据用法

```cpp
// 方式1: 只生成数据，不涉及渲染
auto geom_data = data::GenerateSphere(32);

// 可以保存到文件
SaveGeometryData("sphere.geom", *geom_data);

// 可以修改数据
geom_data->CalculateNormals();
geom_data->Optimize();

// 可以分析数据
std::cout << "Vertices: " << geom_data->GetVertexCount() << std::endl;
std::cout << "Triangles: " << geom_data->GetTriangleCount() << std::endl;

// 方式2: 生成数据后转换为 Vulkan Geometry
GeometryCreater creater(device, vil);
Geometry *sphere = ToVulkanGeometry(*geom_data, &creater, "Sphere");

// 方式3: 使用 GeometryFactory
GeometryFactory factory(device, vil);
Geometry *cube = factory.CreateCube(cube_info, "Cube");
```

### 5.3 适配其他渲染后端（未来扩展）

```cpp
// OpenGL 适配器（未来）
Geometry* ToOpenGLGeometry(const data::GeometryData &geom_data, ...);

// DirectX 12 适配器（未来）
Geometry* ToDX12Geometry(const data::GeometryData &geom_data, ...);

// Metal 适配器（未来）
Geometry* ToMetalGeometry(const data::GeometryData &geom_data, ...);
```

## 6. 目录结构变化

### 6.1 新增文件

```
inc/hgl/graph/data/                          # 新增：纯数据层
├── GeometryData.h                           # 核心数据结构
├── VertexLayout.h                           # 顶点布局
├── GeometryGenerators.h                     # 生成器声明
├── IndexGenerators.h                        # 索引生成器
├── GeometryValidator.h                      # 参数验证
└── GeometryDataUtils.h                      # 工具函数

src/SceneGraph/data/                         # 新增：纯数据层实现
├── GeometryData.cpp
├── VertexLayout.cpp
├── generators/                              # 生成器实现
│   ├── BasicShapes.cpp                      # Rectangle, Circle, Cube, Plane
│   ├── Sphere.cpp                           # Sphere, Dome, HexSphere
│   ├── Cylinder.cpp                         # Cylinder, Cone, Torus, HollowCylinder
│   ├── Capsule.cpp                          # Capsule, TaperedCapsule
│   ├── ExtendedShapes1.cpp                  # Frustum, Arrow, RoundedBox, ...
│   ├── ExtendedShapes2.cpp                  # Helix, Gears, Heart, Star, ...
│   ├── ExtendedShapes3.cpp                  # Stairs, Arch, Tube, Bolts, ...
│   ├── ExtendedShapes4.cpp                  # Diamond, Crystal, Cross, Ribbon, ...
│   ├── AdvancedShapes.cpp                   # TorusKnot, MobiusStrip, KleinBottle, ...
│   └── Procedural.cpp                       # ExtrudedPolygon, Revolution, Wall
└── IndexGenerators.cpp                      # 索引生成器实现

inc/hgl/graph/adapter/                       # 新增：适配层
├── GeometryAdapter.h                        # 适配器接口
└── VulkanGeometryAdapter.h                  # Vulkan专用适配器

src/SceneGraph/adapter/                      # 新增：适配层实现
└── VulkanGeometryAdapter.cpp

inc/hgl/graph/GeometryFactory.h              # 新增：便捷工厂类
src/SceneGraph/GeometryFactory.cpp
```

### 6.2 修改文件

```
src/SceneGraph/InlineGeometry/*.cpp          # 修改：内部调用新API
inc/hgl/graph/geo/InlineGeometry.h           # 保持不变（向后兼容）
inc/hgl/graph/geo/GeometryBuilder.h          # 标记为废弃
inc/hgl/graph/geo/IndexGenerator.h           # 标记为废弃
```

### 6.3 废弃但保留（向后兼容）

```
inc/hgl/graph/geo/GeometryBuilder.h          # 标记 [[deprecated]]
src/SceneGraph/InlineGeometry/GeometryBuilder.cpp
inc/hgl/graph/geo/IndexGenerator.h           # 标记 [[deprecated]]
src/SceneGraph/InlineGeometry/GeometryValidator.h  # 标记 [[deprecated]]
```

## 7. 重构优先级和时间表

### 第一阶段（基础设施）- 预计 1-2 周
1. ✓ 创建 `GeometryData` 类及相关数据结构
2. ✓ 创建 `VulkanGeometryAdapter` 适配器
3. ✓ 重构 3-5 个基础几何体作为示例（Sphere, Cube, Cylinder）
4. ✓ 验证新架构可行性
5. ✓ 编写单元测试

### 第二阶段（批量重构）- 预计 3-4 周
1. ✓ 重构剩余的基础几何体（10个）
2. ✓ 重构扩展几何体（30个）
3. ✓ 重构高级几何体（3个）
4. ✓ 更新所有测试用例

### 第三阶段（优化和完善）- 预计 1-2 周
1. ✓ 性能优化（减少数据拷贝）
2. ✓ 添加几何体缓存机制
3. ✓ 完善文档和示例
4. ✓ 代码审查和重构收尾

### 第四阶段（向后兼容）- 预计 1 周
1. ✓ 标记旧API为废弃
2. ✓ 提供迁移指南
3. ✓ 更新所有示例代码
4. ✓ 最终测试和发布

**总计：6-9 周**

## 8. 性能考虑

### 8.1 数据拷贝优化

**问题**: 新架构引入了额外的数据拷贝（GeometryData -> Vulkan Buffer）

**解决方案**:
```cpp
// 选项1: 使用移动语义（推荐）
GeometryDataPtr GenerateSphere(uint32_t slices)
{
    auto geom = std::make_shared<GeometryData>();
    // ... 生成数据
    return geom;  // 使用移动语义，避免拷贝
}

// 选项2: 就地生成（零拷贝）
class VulkanGeometryWriter
{
    GeometryCreater *creater;
    VABMap *vab_map;

public:
    void WriteVertex(const Vertex &v)
    {
        // 直接写入 Vulkan Buffer，无中间数据
    }
};

// 选项3: 缓存常用几何体
class GeometryCache
{
    std::unordered_map<std::string, GeometryDataPtr> cache;

public:
    GeometryDataPtr GetSphere(uint32_t slices)
    {
        std::string key = "sphere_" + std::to_string(slices);
        if(cache.find(key) == cache.end())
        {
            cache[key] = data::GenerateSphere(slices);
        }
        return cache[key];
    }
};
```

### 8.2 内存优化

```cpp
// 使用自定义分配器
class GeometryData
{
public:
    std::vector<Vertex, PoolAllocator<Vertex>> vertices;
    std::vector<uint32_t, PoolAllocator<uint32_t>> indices;
};

// 或者使用对象池
class GeometryDataPool
{
    std::vector<std::unique_ptr<GeometryData>> pool;

public:
    GeometryDataPtr Acquire()
    {
        if(pool.empty())
            return std::make_shared<GeometryData>();
        
        auto ptr = std::move(pool.back());
        pool.pop_back();
        return ptr;
    }

    void Release(GeometryDataPtr ptr)
    {
        ptr->vertices.clear();
        ptr->indices.clear();
        pool.push_back(std::move(ptr));
    }
};
```

## 9. 测试策略

### 9.1 单元测试

```cpp
// test/GeometryDataTest.cpp
TEST(GeometryData, SphereGeneration)
{
    auto geom = data::GenerateSphere(32);
    
    ASSERT_NE(geom, nullptr);
    EXPECT_GT(geom->GetVertexCount(), 0);
    EXPECT_GT(geom->GetIndexCount(), 0);
    EXPECT_TRUE(geom->Validate());
    
    // 验证球体属性
    EXPECT_NEAR(geom->bounds.sphere.radius, 1.0f, 0.01f);
}

TEST(GeometryData, CubeGeneration)
{
    CubeCreateInfo info;
    info.normal = true;
    info.tex_coord = true;
    
    auto geom = data::GenerateCube(info);
    
    ASSERT_NE(geom, nullptr);
    EXPECT_EQ(geom->GetVertexCount(), 24);  // 6面 * 4顶点
    EXPECT_EQ(geom->GetIndexCount(), 36);   // 6面 * 2三角形 * 3顶点
    EXPECT_TRUE(geom->layout.has_normal);
    EXPECT_TRUE(geom->layout.has_texcoord);
}
```

### 9.2 集成测试

```cpp
// test/VulkanGeometryAdapterTest.cpp
TEST(VulkanAdapter, SphereCreation)
{
    // 创建 Vulkan 测试环境
    auto device = CreateTestDevice();
    auto vil = CreateTestVIL();
    
    // 生成纯数据
    auto geom_data = data::GenerateSphere(32);
    
    // 转换为 Vulkan Geometry
    GeometryCreater creater(device, vil);
    Geometry *geom = ToVulkanGeometry(*geom_data, &creater, "TestSphere");
    
    ASSERT_NE(geom, nullptr);
    EXPECT_EQ(geom->GetVertexCount(), geom_data->GetVertexCount());
    EXPECT_EQ(geom->GetIndexCount(), geom_data->GetIndexCount());
    
    // 清理
    delete geom;
}
```

### 9.3 性能测试

```cpp
// benchmark/GeometryGenerationBenchmark.cpp
BENCHMARK(BM_SpherePureData)
{
    for(auto _ : state)
    {
        auto geom = data::GenerateSphere(32);
    }
}

BENCHMARK(BM_SphereVulkan)
{
    auto device = GetTestDevice();
    auto vil = GetTestVIL();
    GeometryCreater creater(device, vil);
    
    for(auto _ : state)
    {
        auto geom_data = data::GenerateSphere(32);
        auto geom = ToVulkanGeometry(*geom_data, &creater, "Sphere");
        delete geom;
    }
}
```

## 10. 迁移指南

### 10.1 对于库维护者

**步骤1**: 实现基础设施
- 创建 `GeometryData` 类
- 创建 `VulkanGeometryAdapter`
- 编写单元测试

**步骤2**: 逐个重构几何体生成器
- 每次重构 1-3 个生成器
- 确保原有接口继续工作
- 运行回归测试

**步骤3**: 更新文档
- 添加新API文档
- 提供迁移示例
- 标记旧API为废弃

### 10.2 对于库用户

**现有代码无需修改**，继续使用原有API：
```cpp
// 现有代码继续工作
GeometryCreater creater(device, vil);
Geometry *sphere = CreateSphere(&creater, 32);
```

**可选：迁移到新API**（获得更多灵活性）：
```cpp
// 新API：先生成数据，再转换
auto geom_data = data::GenerateSphere(32);
// 可以保存、修改、分析数据
Geometry *sphere = ToVulkanGeometry(*geom_data, &creater, "Sphere");

// 或使用工厂类
GeometryFactory factory(device, vil);
Geometry *sphere = factory.CreateSphere(32);
```

## 11. 潜在问题和解决方案

### 问题 1: 数据拷贝开销

**解决方案**:
- 使用移动语义
- 提供就地生成选项
- 实现几何体缓存

### 问题 2: 向后兼容性

**解决方案**:
- 保留所有原有接口
- 使用 `[[deprecated]]` 标记
- 提供清晰的迁移路径

### 问题 3: 增加的代码量

**解决方案**:
- 使用代码生成工具
- 共享通用代码
- 清晰的代码组织

### 问题 4: 测试工作量

**解决方案**:
- 自动化测试
- 渐进式重构
- 保持原有测试用例

## 12. 总结

### 12.1 优点

1. **完全分离**：纯数据生成与渲染引擎完全解耦
2. **可重用**：可用于其他渲染后端（OpenGL, DX12, Metal）
3. **易测试**：纯数据生成器易于单元测试
4. **可扩展**：易于添加新的几何体类型
5. **向后兼容**：现有代码无需修改
6. **灵活性**：支持数据保存、修改、分析

### 12.2 缺点

1. **额外工作量**：需要重构所有生成器（6-9周）
2. **数据拷贝**：可能引入额外的内存拷贝（可优化）
3. **代码增加**：新增适配层代码
4. **学习曲线**：用户需要了解新API

### 12.3 建议

**推荐采用此重构方案**，原因：
1. 长期收益大于短期成本
2. 符合软件工程最佳实践（分层、解耦）
3. 为未来扩展打下基础
4. 不影响现有用户

**实施建议**：
1. 分阶段进行，降低风险
2. 保持向后兼容，逐步迁移
3. 重点优化常用几何体
4. 充分测试，确保质量

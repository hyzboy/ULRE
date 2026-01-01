# 几何体代码重构执行计划（步骤式）

## 执行原则

**关键要求**：
- ✅ 每一步都可以完整编译运行
- ✅ 每一步都保持现有功能不受影响
- ✅ 每一步都有明确的验证标准
- ✅ 可随时停止，已完成部分保持可用

## 阶段概览

```
第一阶段：基础设施（7步）        → 建立新架构基础
第二阶段：示例迁移（3步）        → 验证方案可行性
第三阶段：批量迁移（6批次）      → 逐步迁移所有几何体
第四阶段：优化清理（4步）        → 优化和废弃标记
```

---

# 第一阶段：基础设施建设（约 1-2 周）

## Step 1: 创建纯数据层目录结构和基础类型定义

**目标**：建立新的目录结构和最基础的数据类型

**新增文件**：
```
inc/hgl/graph/data/PrimitiveTopology.h    # 基础类型定义
inc/hgl/graph/data/VertexLayout.h         # 顶点布局
```

**文件内容**：

`inc/hgl/graph/data/PrimitiveTopology.h`:
```cpp
#pragma once

namespace hgl::graph::data
{
    // 图元拓扑类型
    enum class PrimitiveTopology
    {
        PointList = 0,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
        TriangleFan
    };
}
```

`inc/hgl/graph/data/VertexLayout.h`:
```cpp
#pragma once
#include<cstddef>

namespace hgl::graph::data
{
    // 顶点布局描述
    struct VertexLayout
    {
        bool has_position = true;   // 必须有位置
        bool has_normal = false;
        bool has_tangent = false;
        bool has_texcoord = false;
        bool has_color = false;

        size_t GetVertexSize() const;
        bool IsCompatible(const VertexLayout &other) const;
    };
}
```

`src/SceneGraph/data/VertexLayout.cpp`:
```cpp
#include<hgl/graph/data/VertexLayout.h>

namespace hgl::graph::data
{
    size_t VertexLayout::GetVertexSize() const
    {
        size_t size = 0;
        if(has_position) size += 12;  // 3 floats
        if(has_normal)   size += 12;  // 3 floats
        if(has_tangent)  size += 12;  // 3 floats
        if(has_texcoord) size += 8;   // 2 floats
        if(has_color)    size += 16;  // 4 floats
        return size;
    }

    bool VertexLayout::IsCompatible(const VertexLayout &other) const
    {
        return has_position == other.has_position
            && has_normal == other.has_normal
            && has_tangent == other.has_tangent
            && has_texcoord == other.has_texcoord
            && has_color == other.has_color;
    }
}
```

**修改 CMakeLists.txt**：
在 `src/SceneGraph/CMakeLists.txt` 中添加新文件到构建系统。

**验证**：
```bash
cd /home/runner/work/ULRE/ULRE
mkdir -p build && cd build
cmake ..
make -j4
# 应该成功编译，无错误
```

**提交**：
```
git add .
git commit -m "Step 1: Add basic data layer types (PrimitiveTopology, VertexLayout)"
```

---

## Step 2: 创建 Vertex 和 GeometryData 核心类

**目标**：创建纯数据层的核心类

**新增文件**：
```
inc/hgl/graph/data/Vertex.h
inc/hgl/graph/data/GeometryData.h
src/SceneGraph/data/GeometryData.cpp
```

**文件内容**：

`inc/hgl/graph/data/Vertex.h`:
```cpp
#pragma once
#include<hgl/math/Vector.h>
#include<hgl/color/Color.h>

namespace hgl::graph::data
{
    using namespace hgl::math;

    // 通用顶点结构
    struct Vertex
    {
        Vector3f position;
        Vector3f normal = Vector3f(0, 0, 0);
        Vector3f tangent = Vector3f(0, 0, 0);
        Vector2f texcoord = Vector2f(0, 0);
        Vector4f color = Vector4f(1, 1, 1, 1);

        Vertex() = default;
        Vertex(const Vector3f &pos) : position(pos) {}
        Vertex(float x, float y, float z) : position(x, y, z) {}
    };
}
```

`inc/hgl/graph/data/GeometryData.h`:
```cpp
#pragma once
#include<hgl/graph/data/Vertex.h>
#include<hgl/graph/data/VertexLayout.h>
#include<hgl/graph/data/PrimitiveTopology.h>
#include<hgl/math/geometry/BoundingVolumes.h>
#include<vector>
#include<memory>

namespace hgl::graph::data
{
    // 几何体数据（纯数据，无渲染依赖）
    class GeometryData
    {
    public:
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        VertexLayout layout;
        BoundingVolumes bounds;
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;

    public:
        GeometryData() = default;
        ~GeometryData() = default;

        // 工具方法
        void CalculateBounds();
        void CalculateNormals();
        bool Validate() const;
        
        // 统计信息
        size_t GetVertexCount() const { return vertices.size(); }
        size_t GetIndexCount() const { return indices.size(); }
        size_t GetTriangleCount() const;
    };

    using GeometryDataPtr = std::shared_ptr<GeometryData>;
}
```

`src/SceneGraph/data/GeometryData.cpp`:
```cpp
#include<hgl/graph/data/GeometryData.h>
#include<algorithm>
#include<cmath>

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
            min.x = std::min(min.x, v.position.x);
            min.y = std::min(min.y, v.position.y);
            min.z = std::min(min.z, v.position.z);
            
            max.x = std::max(max.x, v.position.x);
            max.y = std::max(max.y, v.position.y);
            max.z = std::max(max.z, v.position.z);
        }

        bounds.SetFromAABB(min, max);
    }

    void GeometryData::CalculateNormals()
    {
        if(topology != PrimitiveTopology::TriangleList)
            return;
        
        if(indices.empty() || vertices.empty())
            return;

        // 重置所有法线
        for(auto &v : vertices)
            v.normal = Vector3f(0, 0, 0);

        // 累加每个三角形的面法线到顶点
        for(size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            uint32_t i0 = indices[i];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];

            if(i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
                continue;

            const Vector3f &p0 = vertices[i0].position;
            const Vector3f &p1 = vertices[i1].position;
            const Vector3f &p2 = vertices[i2].position;

            Vector3f edge1 = p1 - p0;
            Vector3f edge2 = p2 - p0;
            Vector3f normal = edge1.Cross(edge2);

            vertices[i0].normal += normal;
            vertices[i1].normal += normal;
            vertices[i2].normal += normal;
        }

        // 归一化
        for(auto &v : vertices)
        {
            float len = std::sqrt(v.normal.x * v.normal.x + 
                                 v.normal.y * v.normal.y + 
                                 v.normal.z * v.normal.z);
            if(len > 0.0001f)
            {
                v.normal.x /= len;
                v.normal.y /= len;
                v.normal.z /= len;
            }
        }
    }

    bool GeometryData::Validate() const
    {
        if(vertices.empty())
            return false;

        // 检查索引有效性
        for(auto idx : indices)
        {
            if(idx >= vertices.size())
                return false;
        }

        // 检查拓扑类型
        if(topology == PrimitiveTopology::TriangleList)
        {
            if(indices.size() % 3 != 0)
                return false;
        }

        return true;
    }

    size_t GeometryData::GetTriangleCount() const
    {
        if(topology == PrimitiveTopology::TriangleList)
            return indices.size() / 3;
        return 0;
    }
}
```

**修改 CMakeLists.txt**：添加新文件。

**验证**：
```bash
cd build
make -j4
# 应该成功编译
```

**提交**：
```
git add .
git commit -m "Step 2: Add Vertex and GeometryData core classes"
```

---

## Step 3: 创建 Vulkan 适配器接口

**目标**：创建将 GeometryData 转换为 Vulkan Geometry 的适配器

**新增文件**：
```
inc/hgl/graph/adapter/GeometryAdapter.h
src/SceneGraph/adapter/VulkanGeometryAdapter.cpp
```

**文件内容**：

`inc/hgl/graph/adapter/GeometryAdapter.h`:
```cpp
#pragma once
#include<hgl/graph/VK.h>
#include<hgl/graph/data/GeometryData.h>

namespace hgl::graph
{
    /**
     * 将 GeometryData 转换为 Vulkan Geometry
     * @param geom_data 纯数据几何体
     * @param pc GeometryCreater
     * @param name 几何体名称
     * @return Vulkan Geometry 对象，失败返回 nullptr
     */
    Geometry* ToVulkanGeometry(
        const data::GeometryData &geom_data,
        GeometryCreater *pc,
        const AnsiString &name);
}
```

`src/SceneGraph/adapter/VulkanGeometryAdapter.cpp`:
```cpp
#include<hgl/graph/adapter/GeometryAdapter.h>
#include<hgl/graph/GeometryCreater.h>
#include<hgl/graph/VKGeometry.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<vector>

namespace hgl::graph
{
    Geometry* ToVulkanGeometry(
        const data::GeometryData &geom_data,
        GeometryCreater *pc,
        const AnsiString &name)
    {
        if(!pc)
            return nullptr;

        if(!geom_data.Validate())
            return nullptr;

        const auto &layout = geom_data.layout;
        const size_t vertex_count = geom_data.GetVertexCount();
        const size_t index_count = geom_data.GetIndexCount();

        if(vertex_count == 0)
            return nullptr;

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
        if(index_count > 0)
        {
            pc->WriteIBO(geom_data.indices.data());
        }

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

**修改 CMakeLists.txt**：添加新文件和 adapter 目录。

**验证**：
```bash
cd build
make -j4
# 应该成功编译，适配器可用
```

**提交**：
```
git add .
git commit -m "Step 3: Add Vulkan geometry adapter for converting GeometryData to Vulkan"
```

---

## Step 4: 创建第一个纯数据生成器（Sphere）

**目标**：创建 GenerateSphere 函数作为示例

**新增文件**：
```
inc/hgl/graph/data/GeometryGenerators.h
src/SceneGraph/data/generators/Sphere.cpp
```

**文件内容**：

`inc/hgl/graph/data/GeometryGenerators.h`:
```cpp
#pragma once
#include<hgl/graph/data/GeometryData.h>
#include<cstdint>

namespace hgl::graph::data
{
    /**
     * 生成球体几何数据
     * @param numberSlices 切片数（经线数量）
     * @return 几何体数据，失败返回 nullptr
     */
    GeometryDataPtr GenerateSphere(uint32_t numberSlices);
}
```

`src/SceneGraph/data/generators/Sphere.cpp`:
```cpp
#include<hgl/graph/data/GeometryGenerators.h>
#include<cmath>

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
                v.position.x = std::sin(angleStep * i) * std::sin(angleStep * j);
                v.position.y = std::sin(angleStep * i) * std::cos(angleStep * j);
                v.position.z = std::cos(angleStep * i);
                
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

**修改 CMakeLists.txt**：添加新文件和 generators 子目录。

**验证**：
```bash
cd build
make -j4
# 应该成功编译
```

**提交**：
```
git add .
git commit -m "Step 4: Add GenerateSphere - first pure data generator"
```

---

## Step 5: 重构 CreateSphere 使用新架构（保持兼容）

**目标**：修改现有的 CreateSphere 函数，内部调用新的纯数据生成器

**修改文件**：
```
src/SceneGraph/InlineGeometry/SphereDomeTorus.cpp
```

**修改内容**：

在文件顶部添加：
```cpp
#include<hgl/graph/data/GeometryGenerators.h>
#include<hgl/graph/adapter/GeometryAdapter.h>
```

修改 CreateSphere 函数（保持签名不变）：
```cpp
namespace hgl::graph::inline_geometry
{
    Geometry *CreateSphere(GeometryCreater *pc, const uint numberSlices)
    {
        if(!pc)
            return nullptr;

        // 使用新的纯数据生成器
        auto geom_data = data::GenerateSphere(numberSlices);
        if(!geom_data)
            return nullptr;

        // 通过适配器转换为 Vulkan Geometry
        return ToVulkanGeometry(*geom_data, pc, "Sphere");
    }
}
```

**保留原有的 CreateDome 和 CreateTorus**（暂不修改）。

**验证**：
```bash
cd build
make -j4
# 编译成功

# 如果有测试程序，运行测试
./test/geometry_test  # 或相应的测试程序
# 确保 CreateSphere 功能正常
```

**提交**：
```
git add .
git commit -m "Step 5: Refactor CreateSphere to use new pure data generator (backward compatible)"
```

---

## Step 6: 添加单元测试框架（可选但推荐）

**目标**：为纯数据层添加单元测试

**新增文件**：
```
test/data/GeometryDataTest.cpp
test/data/SphereGeneratorTest.cpp
```

**文件内容**：

`test/data/GeometryDataTest.cpp`:
```cpp
#include<hgl/graph/data/GeometryData.h>
#include<cassert>
#include<iostream>

namespace test
{
    void TestGeometryDataBasic()
    {
        hgl::graph::data::GeometryData geom;
        
        // 添加简单的三角形
        geom.vertices.push_back(hgl::graph::data::Vertex(0, 0, 0));
        geom.vertices.push_back(hgl::graph::data::Vertex(1, 0, 0));
        geom.vertices.push_back(hgl::graph::data::Vertex(0, 1, 0));
        
        geom.indices = {0, 1, 2};
        geom.topology = hgl::graph::data::PrimitiveTopology::TriangleList;
        
        assert(geom.Validate());
        assert(geom.GetVertexCount() == 3);
        assert(geom.GetIndexCount() == 3);
        assert(geom.GetTriangleCount() == 1);
        
        std::cout << "✓ GeometryData basic test passed" << std::endl;
    }
}

int main()
{
    test::TestGeometryDataBasic();
    return 0;
}
```

`test/data/SphereGeneratorTest.cpp`:
```cpp
#include<hgl/graph/data/GeometryGenerators.h>
#include<cassert>
#include<iostream>

namespace test
{
    void TestSphereGeneration()
    {
        auto geom = hgl::graph::data::GenerateSphere(32);
        
        assert(geom != nullptr);
        assert(geom->GetVertexCount() > 0);
        assert(geom->GetIndexCount() > 0);
        assert(geom->Validate());
        assert(geom->topology == hgl::graph::data::PrimitiveTopology::TriangleList);
        
        std::cout << "✓ Sphere generation test passed" << std::endl;
        std::cout << "  Vertices: " << geom->GetVertexCount() << std::endl;
        std::cout << "  Indices: " << geom->GetIndexCount() << std::endl;
        std::cout << "  Triangles: " << geom->GetTriangleCount() << std::endl;
    }
}

int main()
{
    test::TestSphereGeneration();
    return 0;
}
```

**修改 CMakeLists.txt**：添加测试目标。

**验证**：
```bash
cd build
make -j4
./test/data/GeometryDataTest
./test/data/SphereGeneratorTest
# 所有测试应该通过
```

**提交**：
```
git add .
git commit -m "Step 6: Add unit tests for pure data layer"
```

---

## Step 7: 文档更新 - 第一阶段完成

**目标**：更新文档，记录已完成的基础设施

**新增文件**：
```
docs/GeometryDataLayer.md
```

**内容**：简要说明新架构的使用方法和示例。

**验证**：
- 确保所有代码可以编译
- 确保现有功能不受影响
- 确保新的 GenerateSphere 和适配器工作正常

**提交**：
```
git add .
git commit -m "Step 7: Complete Phase 1 - Infrastructure established"
git tag -a phase1-complete -m "Phase 1: Basic infrastructure complete"
```

---

# 第二阶段：示例迁移验证（约 3-5 天）

## Step 8: 迁移 Cube 生成器

**目标**：迁移第二个几何体，验证方案的通用性

**新增文件**：
```
src/SceneGraph/data/generators/Cube.cpp
```

**在 GeometryGenerators.h 中添加**：
```cpp
GeometryDataPtr GenerateCube(const CubeCreateInfo &info);
```

**实现 GenerateCube**：基于现有的 Cube.cpp 代码，转换为纯数据生成。

**修改 Cube.cpp**：
```cpp
Geometry *CreateCube(GeometryCreater *pc, const CubeCreateInfo *cci)
{
    if(!pc || !cci)
        return nullptr;

    auto geom_data = data::GenerateCube(*cci);
    if(!geom_data)
        return nullptr;

    return ToVulkanGeometry(*geom_data, pc, "Cube");
}
```

**验证**：
```bash
cd build
make -j4
# 运行测试确保 Cube 功能正常
```

**提交**：
```
git add .
git commit -m "Step 8: Migrate Cube generator to pure data layer"
```

---

## Step 9: 迁移 Cylinder 生成器

**目标**：迁移第三个几何体

**新增文件**：
```
src/SceneGraph/data/generators/Cylinder.cpp
```

**类似步骤 8**，迁移 Cylinder。

**验证**：编译和测试。

**提交**：
```
git add .
git commit -m "Step 9: Migrate Cylinder generator to pure data layer"
```

---

## Step 10: 阶段评估和调整

**目标**：评估前 3 个几何体的迁移，确认方案无问题

**任务**：
1. 运行所有相关测试
2. 性能对比（新旧实现）
3. 代码审查
4. 如有问题，调整架构

**验证**：
- 所有功能正常
- 性能可接受（差异 < 10%）
- 代码清晰易维护

**提交**：
```
git add .
git commit -m "Step 10: Complete Phase 2 - Validation successful"
git tag -a phase2-complete -m "Phase 2: Sample migration validated"
```

---

# 第三阶段：批量迁移（约 3-4 周）

将剩余的 44 个几何体分 6 批次迁移，每批次 7-8 个。

## 批次 1: 基础形状（Step 11）

**几何体**：
- Rectangle
- Circle  
- Plane
- Square
- Axis
- BoundingBox
- PlaneGrid

**每个的步骤**：
1. 在 generators 目录添加实现
2. 在 GeometryGenerators.h 添加声明
3. 修改原有的 CreateXXX 函数
4. 编译测试

**验证**：
```bash
cd build
make -j4
# 运行相关测试
```

**提交**：
```
git add .
git commit -m "Step 11: Migrate batch 1 - Basic shapes (7 geometries)"
```

---

## 批次 2: 球体类（Step 12）

**几何体**：
- Dome
- HexSphere
- Torus

**步骤同上**。

**提交**：
```
git add .
git commit -m "Step 12: Migrate batch 2 - Sphere family (3 geometries)"
```

---

## 批次 3: 柱体类（Step 13）

**几何体**：
- HollowCylinder
- Cone
- Capsule
- TaperedCapsule
- Frustum

**提交**：
```
git add .
git commit -m "Step 13: Migrate batch 3 - Cylinder/Cone family (5 geometries)"
```

---

## 批次 4: 工业零件（Step 14）

**几何体**：
- Arrow
- PipeElbow
- Tube
- Bolt
- Nut
- SolidGear
- HollowGear
- ChainLink

**提交**：
```
git add .
git commit -m "Step 14: Migrate batch 4 - Industrial parts (8 geometries)"
```

---

## 批次 5: 建筑装饰（Step 15）

**几何体**：
- RoundedBox
- PhoneShapedBox
- Arch
- StraightStairs
- SpiralStairs
- Heart
- Star
- Diamond
- Crystal

**提交**：
```
git add .
git commit -m "Step 15: Migrate batch 5 - Architectural and decorative (9 geometries)"
```

---

## 批次 6: 高级和数学曲面（Step 16）

**几何体**：
- ExtrudedPolygon
- Revolution
- Wall
- Helix
- TorusKnot
- MobiusStrip
- KleinBottle
- Superellipsoid
- Polygon
- Wedge
- Teardrop
- Egg
- Crescent
- Cross
- Ribbon

**提交**：
```
git add .
git commit -m "Step 16: Migrate batch 6 - Advanced and mathematical surfaces (15 geometries)"
```

**阶段完成标记**：
```
git tag -a phase3-complete -m "Phase 3: All 47 geometries migrated"
```

---

# 第四阶段：优化和清理（约 1-2 周）

## Step 17: 性能优化

**目标**：优化数据传输和内存使用

**优化项**：
1. 添加几何体缓存机制
2. 优化适配器的数据拷贝（使用移动语义）
3. 添加对象池（可选）

**新增文件**：
```
inc/hgl/graph/data/GeometryCache.h
src/SceneGraph/data/GeometryCache.cpp
```

**验证**：
- 性能测试
- 内存使用分析

**提交**：
```
git add .
git commit -m "Step 17: Add performance optimizations (caching, move semantics)"
```

---

## Step 18: 标记旧代码为废弃

**目标**：标记旧的辅助类为废弃，但保持可用

**修改文件**：
```
inc/hgl/graph/geo/GeometryBuilder.h
inc/hgl/graph/geo/IndexGenerator.h
```

**添加废弃标记**：
```cpp
// GeometryBuilder.h
[[deprecated("Use pure data layer generators instead")]]
class GeometryBuilder { ... };

// IndexGenerator.h
namespace IndexGenerator
{
    [[deprecated("Use pure data layer generators instead")]]
    template<typename T>
    void CreateSphereIndices(...) { ... }
}
```

**验证**：
- 编译时会看到警告，但功能正常

**提交**：
```
git add .
git commit -m "Step 18: Mark old helper classes as deprecated"
```

---

## Step 19: 更新文档和示例

**目标**：完善文档和使用示例

**更新文件**：
```
docs/GeometryDataLayer.md         # 完善
docs/MigrationGuide.md            # 新增迁移指南
examples/GeometryDataUsage.cpp    # 新增使用示例
```

**文档内容**：
- API 参考
- 使用示例
- 最佳实践
- 性能建议

**提交**：
```
git add .
git commit -m "Step 19: Update documentation and add usage examples"
```

---

## Step 20: 最终验证和发布

**目标**：全面测试和准备发布

**任务**：
1. 运行完整测试套件
2. 性能基准测试
3. 内存泄漏检查
4. 代码覆盖率分析
5. 更新 CHANGELOG

**验证清单**：
- [ ] 所有单元测试通过
- [ ] 所有集成测试通过
- [ ] 性能测试通过（无显著性能下降）
- [ ] 内存测试通过（无泄漏）
- [ ] 文档完整
- [ ] 示例代码可运行

**提交**：
```
git add .
git commit -m "Step 20: Final validation and release preparation"
git tag -a v1.0.0-refactored -m "Version 1.0.0 - Geometry data layer refactoring complete"
```

---

# 验证标准

## 每步验证检查表

```bash
# 1. 编译检查
cd build
make clean
make -j4
# 必须：无错误，警告可接受

# 2. 单元测试
./test/data/GeometryDataTest
./test/data/[NewTest]
# 必须：所有测试通过

# 3. 集成测试（如果有）
./test/integration/GeometryTest
# 必须：现有功能不受影响

# 4. 代码检查
git diff HEAD~1
# 确认修改符合预期

# 5. 功能验证
# 运行使用几何体的应用程序，确保正常工作
```

## 回滚策略

如果某一步出现严重问题：

```bash
# 回滚到上一个标签
git reset --hard <previous-tag>

# 或者只回滚特定文件
git checkout HEAD~1 -- path/to/file

# 重新思考和调整方案
```

---

# 时间估算

| 阶段 | 步骤 | 预计时间 | 累计时间 |
|------|------|---------|---------|
| 第一阶段 | Step 1-7 | 1-2 周 | 1-2 周 |
| 第二阶段 | Step 8-10 | 3-5 天 | 2-3 周 |
| 第三阶段 | Step 11-16 | 3-4 周 | 5-7 周 |
| 第四阶段 | Step 17-20 | 1-2 周 | 6-9 周 |

**总计：6-9 周**

---

# 风险控制

## 高风险点

1. **数据拷贝性能**
   - 缓解：使用移动语义，添加缓存
   - 监控：每步都做性能测试

2. **API 兼容性**
   - 缓解：保持所有原有接口不变
   - 验证：回归测试

3. **内存管理**
   - 缓解：使用智能指针
   - 监控：内存泄漏检测工具

## 停止条件

如果遇到以下情况，停止并重新评估：
- 性能下降 > 15%
- 出现内存泄漏
- 测试通过率 < 95%
- 代码复杂度显著增加

---

# 总结

本执行计划的特点：

✅ **渐进式**：每一步都是独立的，可以随时停止
✅ **可验证**：每一步都有明确的验证标准
✅ **向后兼容**：保持现有 API 不变
✅ **风险可控**：分批次进行，及时发现问题
✅ **文档完善**：每个阶段都有文档支持

按照此计划执行，可以确保重构过程平稳、可控、高质量。

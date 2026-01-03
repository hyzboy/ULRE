# 内联几何体代码重构与独立化计划

## 📋 项目概况

**当前状态：**
- ✅ 代码可以完整编译
- ✅ 主要简单几何体渲染完全正常
- ⚠️ 代码风格不完全统一
- ⚠️ 层级结构不够清晰
- ⚠️ 依赖关系复杂，难以独立

**目标：**
- 🎯 统一代码风格和编码规范
- 🎯 建立清晰的工作流层级
- 🎯 解耦依赖，为独立化做准备
- 🎯 提高代码可维护性和可测试性

---

## 🏗️ 代码结构现状分析

### 当前目录结构
```
inc/hgl/graph/geo/          # 公共头文件
├── Extruded.h              # 挤压几何体
├── GeometryBuilder.h       # 几何体构建器基类
├── IndexGenerator.h        # 索引生成器
├── InlineGeometry.h        # 主接口（48个几何体）
├── Revolution.h            # 旋转体
└── Wall.h                  # 墙壁

src/SceneGraph/InlineGeometry/  # 实现文件（48个cpp）
├── InlineGeometryCommon.h      # 内部公共头
├── GeometryValidator.h         # 几何体验证器
├── GeometryBuilder.cpp         # 构建器实现
├── Cube.cpp                    # 立方体（旧风格）
├── Capsule.cpp                 # 胶囊体（新风格）
├── Revolution.cpp              # 旋转体（新风格）
├── Wall.cpp                    # 墙壁（新风格）
└── [其他45个几何体实现]
```

### 代码风格分类

#### 🟢 **新风格代码**（已使用 GeometryBuilder/IndexGenerator）
- `Capsule.cpp`
- `TaperedCapsule.cpp`
- `Revolution.cpp`
- `Wall.cpp`
- `ExtrudedPolygon.cpp`
- 等

**特点：**
- 使用 `GeometryBuilder` 统一管理顶点属性
- 使用 `IndexGenerator` 生成索引
- 使用 `GeometryValidator` 验证参数
- 代码结构清晰，易读易维护

#### 🔴 **旧风格代码**（直接操作VAB/IB）
- `Cube.cpp`
- `Sphere.cpp`
- `Cylinder.cpp`
- `Cone.cpp`
- `Torus.cpp`
- 等（约40个）

**特点：**
- 直接使用 `VABMapFloat` 和 `IBTypeMap`
- 手动管理指针和映射
- 重复代码较多
- 维护成本高

---

## 🎯 重构计划

### 阶段一：代码审计与分类（1-2天）

#### 任务 1.1：完整代码审计
- [ ] 统计所有48个几何体实现的代码风格
- [ ] 识别每个文件使用的编码模式（新/旧）
- [ ] 记录特殊情况和复杂依赖
- [ ] 建立几何体分类表格

#### 任务 1.2：依赖关系分析
- [ ] 分析 `GeometryCreater` 的外部依赖
- [ ] 识别 Vulkan 特定依赖（`VKDevice`, `VKBuffer` 等）
- [ ] 识别数学库依赖（`hgl/math/*`）
- [ ] 识别工具类依赖（`hgl/type/*`, `hgl/color/*`）
- [ ] 绘制依赖关系图

#### 产出物
- `GEOMETRY_AUDIT_REPORT.md` - 代码审计报告
- `DEPENDENCY_GRAPH.md` - 依赖关系图
- `REFACTORING_PRIORITY.md` - 重构优先级列表

---

### 阶段二：统一代码风格（3-5天）

#### 任务 2.1：完善基础设施
```cpp
// 1. 扩展 GeometryBuilder（如需要）
class GeometryBuilder {
    // 添加更多便利方法
    void WriteQuad(/* ... */);
    void WriteTriangle(/* ... */);
};

// 2. 扩展 IndexGenerator
namespace IndexGenerator {
    // 添加更多索引生成模式
    template<typename T>
    void WriteQuadIndices(/* ... */);
    
    template<typename T>
    void WriteTriangleStrip(/* ... */);
}

// 3. 扩展 GeometryValidator
namespace GeometryValidator {
    bool ValidateRadius(float radius);
    bool ValidateDimensions(float width, float height, float depth);
    // 更多验证函数...
}
```

#### 任务 2.2：重构旧风格代码
**优先级分组：**

**P0（高优先级）- 核心基础几何体（5个）**
- [ ] `Cube.cpp` - 立方体
- [ ] `Sphere.cpp` - 球体
- [ ] `Cylinder.cpp` - 圆柱
- [ ] `Cone.cpp` - 圆锥
- [ ] `Torus.cpp` - 圆环

**P1（中优先级）- 常用几何体（10个）**
- [ ] `PlaneAndSquare.cpp` - 平面和正方形
- [ ] `Circle.cpp` - 圆形
- [ ] `Rectangle.cpp` - 矩形
- [ ] `HollowCylinder.cpp` - 空心圆柱
- [ ] `HexSphere.cpp` - 六边形球体
- [ ] `Arrow.cpp` - 箭头
- [ ] `Axis.cpp` - 坐标轴
- [ ] `BoundingBox.cpp` - 包围盒
- [ ] `Polygon.cpp` - 多边形
- [ ] `Star.cpp` - 星形

**P2（低优先级）- 特殊几何体（剩余~30个）**
- [ ] 其余复杂几何体（按使用频率排序）

**重构步骤（每个文件）：**
1. 备份原文件（添加 `.backup` 后缀）
2. 将 VAB/IB 直接操作改为 `GeometryBuilder` 调用
3. 添加参数验证（使用 `GeometryValidator`）
4. 使用模板化索引生成（`IndexGenerator`）
5. 统一错误处理
6. 添加代码注释
7. 编译测试
8. 渲染验证

#### 产出物
- 所有几何体实现统一为新风格
- `REFACTORING_LOG.md` - 重构日志

---

### 阶段三：层级划分与抽象（3-4天）

#### 任务 3.1：定义清晰的层级结构

```
┌─────────────────────────────────────────┐
│  Layer 4: 用户接口层 (User Interface)   │
│  - InlineGeometry.h (公共API)          │
│  - 各种 CreateInfo 结构体               │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│  Layer 3: 算法层 (Algorithm Layer)      │
│  - 各个几何体实现文件 (.cpp)            │
│  - 专门的几何生成算法                    │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│  Layer 2: 构建工具层 (Builder Layer)    │
│  - GeometryBuilder (顶点写入)          │
│  - IndexGenerator (索引生成)           │
│  - GeometryValidator (参数验证)        │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│  Layer 1: 底层抽象 (Low-level Abstract) │
│  - GeometryCreater (图形API抽象)       │
│  - VAB/IB 映射接口                      │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│  Layer 0: 渲染后端 (Rendering Backend)  │
│  - Vulkan 具体实现                       │
│  - Device, Buffer, Memory 等            │
└─────────────────────────────────────────┘
```

#### 任务 3.2：接口抽象化

**3.2.1 创建几何数据抽象层**
```cpp
// inc/hgl/graph/geo/GeometryData.h
namespace hgl::graph::inline_geometry
{
    // 抽象的几何数据容器（独立于图形API）
    struct GeometryRawData
    {
        struct VertexAttribute
        {
            std::string name;
            uint32_t component_count;  // 2/3/4
            uint32_t vertex_count;
            std::vector<float> data;
        };
        
        std::vector<VertexAttribute> attributes;
        std::vector<uint32_t> indices;
        
        // 便利方法
        void AddAttribute(const std::string& name, uint32_t components);
        float* GetAttributeData(const std::string& name);
        // ...
    };
    
    // 几何生成器接口（不依赖具体图形API）
    class IGeometryDataBuilder
    {
    public:
        virtual void WriteVertex(float x, float y, float z) = 0;
        virtual void WriteNormal(float x, float y, float z) = 0;
        virtual void WriteTangent(float x, float y, float z) = 0;
        virtual void WriteTexCoord(float u, float v) = 0;
        virtual void WriteIndex(uint32_t index) = 0;
        
        virtual ~IGeometryDataBuilder() = default;
    };
}
```

**3.2.2 适配器模式连接到 GeometryCreater**
```cpp
// src/SceneGraph/InlineGeometry/GeometryCreaterAdapter.h
class GeometryCreaterAdapter : public IGeometryDataBuilder
{
    GeometryBuilder builder_;
    IBMap* index_map_;
    
public:
    GeometryCreaterAdapter(GeometryCreater* pc);
    
    void WriteVertex(float x, float y, float z) override {
        builder_.WriteVertex(x, y, z);
    }
    // ... 实现其他接口
};
```

#### 任务 3.3：数学库独立化
- [ ] 识别使用的数学类型（`Vector2f`, `Vector3f`, `AABB` 等）
- [ ] 评估是否可以使用标准库或 GLM
- [ ] 如需保留自定义数学库，明确接口边界

#### 产出物
- `inc/hgl/graph/geo/GeometryData.h` - 抽象数据结构
- `inc/hgl/graph/geo/IGeometryBuilder.h` - 构建器接口
- `GeometryCreaterAdapter` - 适配器实现
- `ARCHITECTURE_DOC.md` - 架构文档

---

### 阶段四：解耦与模块化（4-5天）

#### 任务 4.1：识别并隔离外部依赖

**4.1.1 Vulkan 依赖隔离**
- [ ] 将 `GeometryCreater` 相关代码移至适配器层
- [ ] 确保核心算法不直接依赖 Vulkan 类型

**4.1.2 创建依赖注入接口**
```cpp
// 配置接口
struct GeometryConfig
{
    bool generate_normals = true;
    bool generate_tangents = true;
    bool generate_texcoords = true;
    // ...
};

// 内存分配接口
class IGeometryAllocator
{
public:
    virtual void* Allocate(size_t size) = 0;
    virtual void Deallocate(void* ptr) = 0;
    virtual ~IGeometryAllocator() = default;
};
```

#### 任务 4.2：创建独立模块结构

**建议的独立模块目录结构：**
```
HGL_InlineGeometry/               # 独立模块根目录
├── include/                      # 公共头文件
│   └── hgl_inline_geometry/
│       ├── geometry.h            # 主要公共接口
│       ├── builder.h             # 构建器接口
│       ├── data.h                # 数据结构
│       └── config.h              # 配置
├── src/                          # 实现文件
│   ├── core/                     # 核心功能
│   │   ├── builder.cpp
│   │   ├── validator.cpp
│   │   └── index_generator.cpp
│   ├── primitives/               # 基础几何体
│   │   ├── cube.cpp
│   │   ├── sphere.cpp
│   │   ├── cylinder.cpp
│   │   └── ...
│   ├── complex/                  # 复杂几何体
│   │   ├── revolution.cpp
│   │   ├── extruded.cpp
│   │   ├── wall.cpp
│   │   └── ...
│   └── adapters/                 # 图形API适配器
│       ├── vulkan_adapter.cpp
│       └── ...
├── tests/                        # 单元测试
├── examples/                     # 示例代码
├── docs/                         # 文档
├── CMakeLists.txt               # 构建系统
└── README.md
```

#### 任务 4.3：建立构建系统
```cmake
# CMakeLists.txt 示例
cmake_minimum_required(VERSION 3.15)
project(HGL_InlineGeometry VERSION 1.0.0)

# 选项
option(HGL_INLINE_GEOMETRY_BUILD_TESTS "Build tests" ON)
option(HGL_INLINE_GEOMETRY_BUILD_EXAMPLES "Build examples" ON)
option(HGL_INLINE_GEOMETRY_ENABLE_VULKAN "Enable Vulkan adapter" ON)

# 核心库（无外部依赖）
add_library(hgl_inline_geometry_core STATIC
    src/core/builder.cpp
    src/core/validator.cpp
    # ... 所有核心文件
)

target_include_directories(hgl_inline_geometry_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Vulkan 适配器（可选）
if(HGL_INLINE_GEOMETRY_ENABLE_VULKAN)
    add_library(hgl_inline_geometry_vulkan STATIC
        src/adapters/vulkan_adapter.cpp
    )
    target_link_libraries(hgl_inline_geometry_vulkan
        PUBLIC hgl_inline_geometry_core
        PRIVATE Vulkan::Vulkan
    )
endif()
```

#### 产出物
- 独立的模块目录结构
- `CMakeLists.txt` - 独立构建系统
- 适配器实现
- `INTEGRATION_GUIDE.md` - 集成指南

---

### 阶段五：测试与文档（3-4天）

#### 任务 5.1：建立测试框架
```cpp
// tests/test_cube.cpp
#include <catch2/catch.hpp>
#include <hgl_inline_geometry/geometry.h>

TEST_CASE("Cube generation", "[cube]") {
    // 使用纯数据接口测试，不依赖 Vulkan
    GeometryRawData data;
    SimpleDataBuilder builder(data);
    
    CubeCreateInfo info;
    bool success = CreateCubeData(&builder, &info);
    
    REQUIRE(success == true);
    REQUIRE(data.GetVertexCount() == 24);
    REQUIRE(data.GetIndexCount() == 36);
    
    // 验证顶点数据...
}
```

#### 任务 5.2：创建示例程序
```cpp
// examples/simple_cube.cpp
#include <hgl_inline_geometry/geometry.h>

int main() {
    // 1. 创建纯数据
    GeometryRawData cube_data;
    SimpleDataBuilder builder(cube_data);
    
    CubeCreateInfo info;
    CreateCubeData(&builder, &info);
    
    // 2. 导出为 OBJ（演示独立性）
    ExportToOBJ(cube_data, "cube.obj");
    
    // 3. 集成到 Vulkan（演示适配器）
    #ifdef USE_VULKAN
    VulkanAdapter vk_adapter(device);
    Geometry* vk_geometry = vk_adapter.CreateGeometry(cube_data);
    #endif
    
    return 0;
}
```

#### 任务 5.3：完善文档

**文档结构：**
- [ ] `README.md` - 项目概述、快速开始
- [ ] `ARCHITECTURE.md` - 架构设计文档
- [ ] `API_REFERENCE.md` - API 参考手册
- [ ] `MIGRATION_GUIDE.md` - 从旧版本迁移指南
- [ ] `CONTRIBUTING.md` - 贡献指南
- [ ] `CHANGELOG.md` - 变更日志

**每个几何体的文档模板：**
```markdown
## CreateCube

### 描述
创建一个单位立方体（中心在原点，边长为1）。

### 参数
- `CubeCreateInfo`: 立方体配置
  - `normal`: 是否生成法线（默认true）
  - `tangent`: 是否生成切线（默认true）
  - `tex_coord`: 是否生成纹理坐标（默认true）
  - `color_type`: 颜色类型（无/统一/面/顶点）

### 返回值
成功返回 `Geometry*`，失败返回 `nullptr`

### 示例
```cpp
CubeCreateInfo info;
info.normal = true;
info.tangent = true;
Geometry* cube = CreateCube(creater, &info);
```

### 顶点数据
- 顶点数：24（每面4个，共6面）
- 索引数：36（每面2个三角形，每个三角形3个索引）

### 注意事项
- 立方体中心在原点(0,0,0)
- 边长为1（从-0.5到+0.5）
```

#### 产出物
- 单元测试套件（覆盖所有几何体）
- 示例程序（至少5个）
- 完整文档集

---

### 阶段六：集成与验证（2-3天）

#### 任务 6.1：向后兼容层
```cpp
// 为现有代码提供兼容接口
namespace hgl::graph::inline_geometry
{
    // 保持原有接口不变
    inline Geometry* CreateCube(GeometryCreater* pc, const CubeCreateInfo* cci)
    {
        // 内部使用新实现，但保持接口兼容
        GeometryCreaterAdapter adapter(pc);
        return CreateCubeImpl(&adapter, cci);
    }
}
```

#### 任务 6.2：渐进式迁移
1. [ ] 第一步：保持旧接口，内部使用新实现
2. [ ] 第二步：提供新接口（数据驱动）
3. [ ] 第三步：标记旧接口为 deprecated
4. [ ] 第四步：完全移除旧接口（大版本更新）

#### 任务 6.3：性能测试
```cpp
// benchmark/bench_cube.cpp
#include <benchmark/benchmark.h>

static void BM_CreateCube(benchmark::State& state) {
    for (auto _ : state) {
        // 测试代码...
    }
}
BENCHMARK(BM_CreateCube);
```

测试指标：
- [ ] 内存分配次数
- [ ] 生成时间
- [ ] 内存占用
- [ ] 与旧实现对比

#### 产出物
- 兼容层实现
- 性能测试报告
- 集成测试结果

---

## 📊 工作量估算

| 阶段 | 任务数 | 估计时间 | 人力 |
|------|--------|----------|------|
| 阶段一：代码审计 | 2 | 1-2天 | 1人 |
| 阶段二：统一代码风格 | 2 | 3-5天 | 1-2人 |
| 阶段三：层级划分 | 3 | 3-4天 | 1人 |
| 阶段四：解耦模块化 | 3 | 4-5天 | 1-2人 |
| 阶段五：测试文档 | 3 | 3-4天 | 1人 |
| 阶段六：集成验证 | 3 | 2-3天 | 1人 |
| **总计** | **16** | **16-23天** | **1-2人** |

---

## 🎯 里程碑与交付物

### M1：代码审计完成（第2天）
- ✅ 完整的代码审计报告
- ✅ 依赖关系图
- ✅ 重构优先级列表

### M2：核心几何体重构完成（第7天）
- ✅ P0 级别5个核心几何体重构完成
- ✅ 新风格基础设施完善
- ✅ 编译和渲染测试通过

### M3：代码风格统一（第12天）
- ✅ 所有48个几何体统一为新风格
- ✅ 重构日志记录完整

### M4：层级划分完成（第16天）
- ✅ 清晰的5层架构
- ✅ 抽象接口定义
- ✅ 架构文档

### M5：独立模块完成（第21天）
- ✅ 独立的目录结构
- ✅ 独立的构建系统
- ✅ Vulkan 适配器实现

### M6：发布就绪（第23天）
- ✅ 完整的测试套件
- ✅ 完整的文档
- ✅ 示例程序
- ✅ 集成验证通过

---

## 🚀 快速开始建议

### 立即可以开始的任务

#### 1. 代码审计脚本（30分钟）
```python
# tools/audit_geometry_files.py
import os
import re

def analyze_cpp_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # 检测是否使用新风格
    uses_geometry_builder = 'GeometryBuilder' in content
    uses_index_generator = 'IndexGenerator' in content
    uses_validator = 'GeometryValidator' in content
    
    # 检测是否使用旧风格
    uses_vab_direct = 'VABMapFloat' in content and not uses_geometry_builder
    uses_ib_direct = 'IBTypeMap' in content and not uses_index_generator
    
    return {
        'file': os.path.basename(filepath),
        'style': 'new' if uses_geometry_builder else 'old',
        'uses_builder': uses_geometry_builder,
        'uses_generator': uses_index_generator,
        'uses_validator': uses_validator,
        'lines': len(content.splitlines())
    }

# 扫描所有文件
src_dir = 'd:/ULRE/src/SceneGraph/InlineGeometry'
results = []
for filename in os.listdir(src_dir):
    if filename.endswith('.cpp'):
        filepath = os.path.join(src_dir, filename)
        results.append(analyze_cpp_file(filepath))

# 生成报告...
```

#### 2. 创建第一个重构示例（2小时）
选择 `Cube.cpp` 作为第一个重构目标：
- 复制为 `Cube.cpp.backup`
- 按新风格重写
- 对比测试

#### 3. 建立测试框架（1天）
```cpp
// tests/test_framework.h
// 简单的测试框架，验证几何体基本属性
struct GeometryTestResult {
    bool vertex_count_correct;
    bool index_count_correct;
    bool has_valid_normals;
    bool has_valid_uvs;
    // ...
};

GeometryTestResult TestGeometry(Geometry* geom, 
                                 uint expected_vertices, 
                                 uint expected_indices);
```

---

## 📝 注意事项

### 风险与挑战
1. **兼容性风险**：重构可能影响现有代码
   - 解决：保持向后兼容层
   
2. **测试覆盖**：48个几何体测试量大
   - 解决：自动化测试生成

3. **性能回退**：抽象层可能影响性能
   - 解决：性能基准测试、内联优化

4. **数学库依赖**：自定义数学类型难以替换
   - 解决：接口适配器模式

### 最佳实践
- ✅ 每次重构后立即测试
- ✅ 保持小步迭代
- ✅ 及时提交代码（Git）
- ✅ 详细记录重构日志
- ✅ 保留旧版本备份

---

## 📚 参考资源

### 相关设计模式
- Builder Pattern（构建器模式）
- Adapter Pattern（适配器模式）
- Template Method Pattern（模板方法模式）
- Strategy Pattern（策略模式）

### 推荐阅读
- 《重构：改善既有代码的设计》- Martin Fowler
- 《代码整洁之道》- Robert C. Martin
- 《架构整洁之道》- Robert C. Martin

---

## 🤝 协作流程

### Git 分支策略
```
master (protected)
  ├── develop
  │   ├── feature/refactor-cube
  │   ├── feature/refactor-sphere
  │   ├── feature/layer-abstraction
  │   └── feature/vulkan-adapter
  └── release/v2.0.0
```

### Code Review 检查点
- [ ] 代码风格统一
- [ ] 使用新风格基础设施
- [ ] 参数验证完整
- [ ] 错误处理正确
- [ ] 注释清晰
- [ ] 测试通过

---

## ✅ 总结

这是一个系统性的重构计划，分为6个阶段：

1. **审计** → 了解现状
2. **统一** → 统一代码风格
3. **抽象** → 建立清晰层级
4. **解耦** → 模块化独立
5. **测试** → 保证质量
6. **集成** → 平滑过渡

**关键原则：**
- 渐进式重构，不求一步到位
- 保持向后兼容
- 持续测试验证
- 详细文档记录

**最终目标：**
- 可独立编译的几何体生成库
- 清晰的层级结构
- 易于维护和扩展
- 可集成到任何渲染后端

---

*文档版本：v1.0*  
*创建日期：2026-01-02*  
*最后更新：2026-01-02*

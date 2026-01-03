# 内联几何体重构 - 快速行动指南

## 🎯 今天就可以开始的任务

### ✅ 任务 1: 运行代码审计（30分钟）

```powershell
# 运行审计脚本
cd d:\ULRE
python tools\audit_geometry_files.py

# 查看报告
notepad doc\GEOMETRY_AUDIT_REPORT.md
```

**预期输出:**
- 完整的文件分类列表
- 新/旧风格统计
- 重构优先级建议

---

### ✅ 任务 2: 建立测试基准（1小时）

创建一个简单的测试来验证当前实现：

```cpp
// tests/baseline_test.cpp
#include<hgl/graph/geo/InlineGeometry.h>

// 测试所有几何体是否能成功创建
struct GeometryBaseline
{
    const char* name;
    uint expected_vertices;
    uint expected_indices;
};

const GeometryBaseline baselines[] = {
    {"Cube", 24, 36},
    {"Sphere(16)", 0, 0},  // TODO: 计算预期值
    // ... 添加所有几何体
};

void TestAllGeometries(GeometryCreater* pc)
{
    for(const auto& baseline : baselines)
    {
        // 创建几何体
        // 验证顶点数和索引数
        // 记录结果
    }
}
```

**目的:** 
- 建立重构前的基准数据
- 确保重构后行为一致

---

### ✅ 任务 3: 第一个重构示例（2-3小时）

选择最简单的 **Cube.cpp** 作为第一个重构目标：

#### 步骤：

**1. 备份原文件**
```powershell
cd d:\ULRE\src\SceneGraph\InlineGeometry
copy Cube.cpp Cube.cpp.backup
```

**2. 分析现有代码**
- 顶点数：24（每面4个顶点）
- 索引数：36（6面 × 2三角形 × 3索引）
- 属性：位置、法线、切线、UV

**3. 使用新风格重写**

```cpp
// Cube.cpp - 新风格版本
#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    Geometry *CreateCube(GeometryCreater *pc, const CubeCreateInfo *cci)
    {
        // 1. 验证参数
        if(!pc || !cci)
            return nullptr;
        
        constexpr uint VERTEX_COUNT = 24;
        constexpr uint INDEX_COUNT = 36;
        
        // 2. 使用 GeometryValidator
        if(!GeometryValidator::ValidateBasicParams(pc, VERTEX_COUNT, INDEX_COUNT))
            return nullptr;
        
        // 3. 初始化
        if(!pc->Init("Cube", VERTEX_COUNT, INDEX_COUNT))
            return nullptr;
        
        // 4. 使用 GeometryBuilder
        GeometryBuilder builder(pc);
        if(!builder.IsValid())
            return nullptr;
        
        // 5. 定义顶点数据（保持原有数据不变）
        constexpr float positions[] = {
            // 底面 (Y = -0.5)
            -0.5f, -0.5f, -0.5f,  // 0
            -0.5f, -0.5f, +0.5f,  // 1
            +0.5f, -0.5f, +0.5f,  // 2
            +0.5f, -0.5f, -0.5f,  // 3
            // ... 其他面
        };
        
        // 6. 写入顶点（使用 Builder）
        const uint face_count = 6;
        const uint verts_per_face = 4;
        
        for(uint i = 0; i < VERTEX_COUNT; ++i)
        {
            builder.WriteVertex(
                positions[i*3 + 0],
                positions[i*3 + 1],
                positions[i*3 + 2]
            );
            
            if(builder.HasNormals() && cci->normal)
            {
                // 写入法线...
            }
            
            // ... 其他属性
        }
        
        // 7. 生成索引（使用 IndexGenerator）
        const IndexType index_type = pc->GetIndexType();
        if(index_type == IndexType::U16)
        {
            IBTypeMap<uint16> ib(pc->GetIBMap());
            uint16* ip = ib;
            
            // 每个面2个三角形
            for(uint face = 0; face < 6; ++face)
            {
                uint base = face * 4;
                // 三角形1
                *ip++ = base + 0;
                *ip++ = base + 2;
                *ip++ = base + 1;
                // 三角形2
                *ip++ = base + 0;
                *ip++ = base + 3;
                *ip++ = base + 2;
            }
        }
        else // U32
        {
            // 类似处理...
        }
        
        return pc->Create();
    }
}
```

**4. 编译测试**
```powershell
# 使用你的构建系统编译
cmake --build build --target YourTarget
```

**5. 渲染验证**
- 运行你的渲染测试程序
- 对比新旧实现的渲染结果
- 确保完全一致

**6. 性能对比**
```cpp
// 简单的性能测试
auto start = std::chrono::high_resolution_clock::now();
for(int i = 0; i < 1000; ++i)
{
    Geometry* cube = CreateCube(pc, &info);
    delete cube;
}
auto end = std::chrono::high_resolution_clock::now();
// 记录时间
```

---

## 📋 本周计划（如果全职投入）

### 第1天：准备与审计
- [x] 运行审计脚本
- [ ] 阅读完整计划文档
- [ ] 建立测试基准
- [ ] 设置 Git 分支策略

### 第2天：第一批重构（P0核心）
- [ ] 重构 Cube.cpp ✨（最简单）
- [ ] 重构 Sphere.cpp
- [ ] 建立重构模板和检查清单

### 第3天：继续P0重构
- [ ] 重构 Cylinder.cpp
- [ ] 重构 Cone.cpp
- [ ] 重构 Torus.cpp

### 第4天：完善基础设施
- [ ] 扩展 GeometryBuilder（添加便利方法）
- [ ] 扩展 IndexGenerator（添加更多模式）
- [ ] 完善 GeometryValidator

### 第5天：P1重构开始
- [ ] 重构 PlaneAndSquare.cpp
- [ ] 重构 Circle.cpp
- [ ] 重构 Rectangle.cpp
- [ ] 编写重构日志

---

## 🎨 代码风格检查清单

每个重构后的文件应该满足：

### ✅ 结构检查
- [ ] 使用 `GeometryBuilder` 进行顶点写入
- [ ] 使用 `GeometryValidator` 进行参数验证
- [ ] 使用 `IndexGenerator` 模板（如果适用）
- [ ] 错误处理：所有可能失败的地方都返回 `nullptr`

### ✅ 代码质量
- [ ] 使用 `constexpr` 定义常量
- [ ] 合理的变量命名（避免 `i`, `j`, `k` 除非是循环变量）
- [ ] 添加必要的注释（特别是复杂算法）
- [ ] 避免魔法数字（使用命名常量）

### ✅ 性能检查
- [ ] 避免不必要的内存分配
- [ ] 使用引用避免拷贝
- [ ] 循环优化（减少重复计算）

### ✅ 测试验证
- [ ] 编译通过
- [ ] 渲染结果正确
- [ ] 性能无明显下降
- [ ] 内存无泄漏

---

## 🔧 常用代码片段

### 1. 标准重构模板

```cpp
#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    Geometry *CreateXXX(GeometryCreater *pc, const XXXCreateInfo *info)
    {
        // 1. 参数验证
        if(!pc || !info)
            return nullptr;
        
        // 2. 计算顶点和索引数量
        const uint numberVertices = /* 计算 */;
        const uint numberIndices = /* 计算 */;
        
        // 3. 基本验证
        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;
        
        // 4. 初始化
        if(!pc->Init("XXX", numberVertices, numberIndices))
            return nullptr;
        
        // 5. 创建 Builder
        GeometryBuilder builder(pc);
        if(!builder.IsValid())
            return nullptr;
        
        // 6. 生成顶点数据
        // ... 使用 builder.WriteVertex/Normal/Tangent/TexCoord
        
        // 7. 生成索引数据
        const IndexType index_type = pc->GetIndexType();
        if(index_type == IndexType::U16)
        {
            IBTypeMap<uint16> ib(pc->GetIBMap());
            uint16* ip = ib;
            // ... 写入索引
        }
        else
        {
            IBTypeMap<uint32> ib(pc->GetIBMap());
            uint32* ip = ib;
            // ... 写入索引
        }
        
        return pc->Create();
    }
}
```

### 2. 旋转体通用模式

```cpp
// 适用于 Cylinder, Cone, Sphere 等
const float angleStep = (2.0f * std::numbers::pi_v<float>) / float(slices);

for(uint stack = 0; stack <= stacks; ++stack)
{
    const float v = float(stack) / float(stacks);
    
    for(uint slice = 0; slice <= slices; ++slice)
    {
        const float u = float(slice) / float(slices);
        const float angle = angleStep * float(slice);
        
        const float x = radius * cos(angle);
        const float y = /* 根据几何体类型 */;
        const float z = radius * sin(angle);
        
        builder.WriteFullVertex(x, y, z, nx, ny, nz, tx, ty, tz, u, v);
    }
}
```

### 3. 索引生成模式（四边形网格）

```cpp
// 生成 slices × stacks 网格的索引
for(uint stack = 0; stack < stacks; ++stack)
{
    for(uint slice = 0; slice < slices; ++slice)
    {
        const IndexT i0 = stack * (slices + 1) + slice;
        const IndexT i1 = i0 + 1;
        const IndexT i2 = i0 + (slices + 1);
        const IndexT i3 = i2 + 1;
        
        // 三角形1
        *ip++ = i0; *ip++ = i2; *ip++ = i1;
        // 三角形2
        *ip++ = i1; *ip++ = i2; *ip++ = i3;
    }
}
```

---

## 📊 进度追踪表

创建一个简单的 Excel 或 Markdown 表格追踪进度：

| 文件名 | 优先级 | 行数 | 状态 | 负责人 | 完成日期 | 备注 |
|--------|--------|------|------|--------|----------|------|
| Cube.cpp | P0 | 100 | ✅ 完成 | - | 2026-01-02 | 第一个示例 |
| Sphere.cpp | P0 | 150 | 🔄 进行中 | - | - | |
| Cylinder.cpp | P0 | 180 | ⏳ 待开始 | - | - | |
| ... | | | | | | |

---

## 💡 重要提示

### Do's ✅
- **小步迭代**：每次只重构一个文件
- **立即测试**：重构后马上编译和测试
- **保留备份**：重构前复制原文件
- **详细记录**：记录遇到的问题和解决方案
- **保持一致**：所有文件使用相同的风格和模式

### Don'ts ❌
- **不要批量重构**：不要一次改太多文件
- **不要改变行为**：重构应该保持功能完全一致
- **不要忽略测试**：没有测试的重构是危险的
- **不要优化过早**：先保证正确性，再考虑优化
- **不要删除旧代码**：保留 `.backup` 文件直到确认无问题

---

## 🚨 遇到问题时

### 编译错误
1. 检查是否包含了 `InlineGeometryCommon.h`
2. 检查命名空间是否正确
3. 检查是否遗漏了某些头文件

### 渲染错误
1. 对比新旧实现的顶点数据（使用调试器）
2. 检查索引顺序（顺时针/逆时针）
3. 检查法线方向
4. 验证 UV 坐标范围

### 性能问题
1. 使用 Profiler 找出热点
2. 检查是否有不必要的拷贝
3. 考虑内联关键函数
4. 优化循环结构

---

## 📞 需要帮助？

如果遇到困难：
1. 查看已完成的示例文件（Capsule.cpp, Wall.cpp）
2. 参考详细计划文档
3. 查看 GeometryBuilder/IndexGenerator 的接口
4. 在重构日志中记录问题

---

## 🎉 里程碑庆祝

- ✨ 完成第一个重构（Cube）
- 🎊 完成 P0 所有5个核心几何体
- 🚀 完成 P1 所有10个常用几何体
- 🏆 完成全部48个几何体统一

---

*快速指南版本：v1.0*  
*最后更新：2026-01-02*

**下一步：运行 `python tools\audit_geometry_files.py` 开始审计！**

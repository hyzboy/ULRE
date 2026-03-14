# ULRE 渲染系统重构 — 工作进度跟踪

> **用途**: 换机开发时快速恢复上下文。包含所有已完成/进行中的工作、文件清单、构建信息和后续计划。  
> **最后更新**: 2025-07 (Stages 1–6 完成后 + 文档更新)

---

## 一、总体进度概览

| 阶段 | 描述 | 状态 |
|------|------|------|
| Stage 1 | GBuffer 清理 & 重命名 | ✅ 完成 |
| Stage 2 | 核心类型头文件 (9个) | ✅ 完成 (2.10/2.11 延后) |
| Stage 3 | Descriptor Set Layout | ⚠️ 部分完成 (3.1–3.2) |
| Stage 4 | Reversed-Z + Camera 相对渲染 | ⚠️ 部分完成 (4.1–4.2, 4.6, 4.7.1, 4.7.3) |
| Stage 5 | Compositor / Shader 组装 | ✅ 完成 (5.1–5.10) |
| Stage 6 | SSBO Vertex Fetch 路径 | ✅ 完成 (6.1–6.6) |
| Stage 7–16 | 后续阶段 | ⬜ 未开始 |

---

## 二、设计文档

三份核心设计文档位于 `doc/` 下：

| 文件 | 内容 |
|------|------|
| `SimplifiedMaterialSystem_Design.md` | SurfaceType×QualityTier 架构设计，含 SPV 部署生命周期 |
| `RenderingPipeline_Design.md` | 16 章渲染管线配套文档，§14.5 SPV 部署策略 |
| `RefactoringPlan_Incremental.md` | 16 阶段、~100 增量步骤的重构计划 |

---

## 三、各阶段详细进度

### Stage 1: GBuffer 清理 (COMPLETED)

**修改的文件:**
- `inc/hgl/shadergen/RenderFlowDef.h` — 移除 GBuffer 类型，`GBufferChannel` → `RenderChannel`（保留 using 别名），创建 `QualityTier` 占位（含 `GBufferQualityPreset` 别名）
- `inc/hgl/shadergen/ShaderComposition.h` — 移除 `GBufferConfigurations` 命名空间、`Deferred_Standard` 流程

**删除内容:**
- `GBufferFormatLevel`, `GBufferFormatSpec`, `GBufferConfiguration`
- `ComputeGBufferVariantHash`, `HashFNV1a32`
- `RenderPipeline` struct, `GetSPVPath()`
- `PipelineRenderPath`、`RenderStage`、`RenderFlowPreset` 中的废弃枚举值
- `NormalCompressionPolicy`、`PipelineMode` 清理

---

### Stage 2: 核心类型 (COMPLETED)

**创建的 9 个头文件** (`inc/hgl/mtl/new/`):

| 文件 | 内容 |
|------|------|
| `SurfaceType.h` | 表面类型枚举 |
| `QualityTier.h` | 质量等级枚举 |
| `BlendMode.h` | 混合模式枚举 |
| `PassType.h` | 渲染 Pass 类型 |
| `PlatformBackend.h` | 平台后端 + `GeometryFetchMode` |
| `MaterialCategory.h` | 材质分类 |
| `NewShaderPermutationKey.h` | Shader 排列组合 Key |
| `MaterialPresetDef.h` | 材质预设定义 |
| `DeviceQualityProfile.h` | 设备质量档案 |

**延后步骤:**
- Step 2.10: `DeviceQualityProfile` 自动检测（需要 Vulkan headers）
- Step 2.11: `AppendGLSLDefines`（需要 `AnsiString` 集成）

---

### Stage 3: Descriptor Set Layout (部分完成)

**已完成:**
- Step 3.1: `inc/hgl/mtl/new/NewDescriptorSetType.h`
- Step 3.2: `inc/hgl/mtl/new/DescriptorSetBindings.h`

**延后:**
- Steps 3.3–3.6: 需要 Vulkan 集成

---

### Stage 4: Reversed-Z + Camera 相对渲染 (部分完成)

**已完成:**
- Step 4.1: `inc/hgl/graph/camera/ReversedZProj.h` — Reversed-Z 投影矩阵
- Step 4.2: Camera.h 添加 `use_reversed_z` 标志，`RefreshCameraInfo` 中条件投影
- Step 4.6: `ShaderLibrary/common/depth_utils.glsl` — 深度工具函数
- Step 4.7.1: Camera.h 添加 `Vector3d world_position_double` + `GetWorldPositionDouble()`
- Step 4.7.3: CameraInfo.h 添加 `cameraPosWorld` 字段
- CMakeLists: 已将 ReversedZProj 注册到 SceneGraph

**延后:**
- Steps 4.3–4.5: 管线集成
- Steps 4.7.2, 4.7.4: Example/管线集成

**注意:** `src/SceneGraph/ReversedZProj.cpp` 可能在另一台机器上创建但未同步，需确认。

---

### Stage 5: Compositor / Shader 组装 (COMPLETED)

**创建的文件:**

| 文件路径 | 用途 |
|----------|------|
| `inc/hgl/shadergen/CompositorAssembler.h` | GLSL 组装器接口 |
| `src/ShaderGen/CompositorAssembler.cpp` | GLSL 组装器实现 |
| `inc/hgl/shadergen/PresetShaderCompiler.h` | 预设 Shader 编译器（离线构建工具） |
| `src/ShaderGen/PresetShaderCompiler.cpp` | 实现 |
| `inc/hgl/shadergen/SPVCache.h` | SPV 缓存（构建时 Store/Save，运行时 Load/Lookup） |
| `src/ShaderGen/SPVCache.cpp` | 实现 |
| `inc/hgl/mtl/new/NewDescriptorSetLayoutFactory.h` | Descriptor Set Layout 工厂 |
| `src/ShaderGen/NewDescriptorSetLayoutFactory.cpp` | 实现 |
| `inc/hgl/mtl/new/NewDescriptorBinding.h` | Descriptor Binding 定义 |
| `src/ShaderGen/NewDescriptorBinding.cpp` | 实现 |
| `src/ShaderGen/NewShaderPermutationKey.cpp` | Permutation Key 实现 |
| `src/ShaderGen/DeviceQualityProfile.cpp` | 设备质量档案实现 |
| `ShaderLibrary/compositor/main_forward_opaque.vert.glsl` | Forward Opaque 顶点模板 |
| `ShaderLibrary/compositor/main_forward_opaque.frag.glsl` | Forward Opaque 片段模板 |
| `ShaderLibrary/common/surface_interface.glsl` | Surface 接口定义 |
| `ShaderLibrary/surface/standard_surface.glsl` | 标准表面函数 |
| `ShaderLibrary/common/lighting.glsl` | 光照函数 |

**Step 5.10: CompositorRenderTest**
- `example/Basic/CompositorRenderTest.cpp` — 6 阶段验证测试
- `example/Basic/CMakeLists.txt` — 添加 `CreateProject(10_CompositorRenderTest ...)`
- 修改 `inc/hgl/vk/VKRenderPass.h` — 添加 public `CreatePipeline` 重载（接受原始 shader stages）
- 修改 `src/Vulkan/VKRenderPass.cpp` — 实现上述重载
- 修改 `src/Vulkan/pipeline/VKPipelineData.cpp` — `InitVertexInputState(nullptr)` 支持空顶点输入（SSBO 模式）

---

### Stage 6: SSBO Vertex Fetch 路径 (COMPLETED)

**创建的文件:**

| 文件路径 | 用途 |
|----------|------|
| `ShaderLibrary/common/vertex_fetch_ssbo.glsl` | SSBO Fetch 函数（已存在） |
| `ShaderLibrary/common/vertex_fetch_vbo.glsl` | VBO 别名函数 |
| `inc/hgl/graph/VertexDataBufferManager.h` | `SSBOVertexData` 结构体 (48 bytes, std430) + 管理器类 |
| `src/SceneGraph/VertexDataBufferManager.cpp` | 实现：StagedBuffer + BlockAllocator |

**修改的文件:**
- `src/SceneGraph/CMakeLists.txt` — 添加 `SSBO_VERTEX_FETCH_FILES` 段
- `inc/hgl/graph/geo/VKGeometry.h` — 添加 `ssbo_vtx_node`/`ssbo_idx_node` 字段 + Set/Get/Offset 方法 + `#include<hgl/type/BlockAllocator.h>`
- `inc/hgl/vk/VKRenderPass.h` — 添加 `GeometryFetchMode` 感知的 `CreatePipeline` 重载 + `#include<hgl/mtl/new/PlatformBackend.h>`
- `src/Vulkan/VKRenderPass.cpp` — 实现 GeometryFetchMode 分支（SSBO → nullptr VIL; VBO → `mi->GetVIL()`）

**CompositorRenderTest Phase 6 扩展:**
- 创建 `VertexDataBufferManager`，上传三角形顶点+索引
- 验证 allocation / dirty / descriptor 信息
- 实际 `vkCmdDraw` 延后到 Stage 7（需要完整 UBO/descriptor chain）

---

## 四、关键技术要点

### GEOMETRY_FETCH_SSBO
- **PC 平台**: 始终为 1（见 `NewShaderPermutationKey.cpp`）
- **Android**: 仅在 High+ 质量时为 1
- SSBO 是 PC 的默认顶点获取路径

### SSBOVertexData 结构
- 48 字节，std430 对齐
- `vec3 position + pad` / `vec3 normal + pad` / `vec2 uv0 + pad`

### Pipeline 架构注意事项
- `RenderPass::CreatePipeline(raw components)` 原为 protected，已添加 public 重载
- `Pipeline` 构造函数为 private，`friend class RenderPass`
- `InitVertexInputState(nullptr)` 创建空顶点输入（0 bindings, 0 attributes）

### SPV 部署模型
三个阶段：
1. **渲染器开发阶段** — 实时 GLSL 生成 + SPV 编译
2. **游戏编辑器阶段** — 仅使用离线 SPV 包
3. **游戏运行时** — 按 `PlatformBackend × QualityTier` 分发 SPV 包

### 常用模式
- `GRAPH_MODULE_CLASS(ClassName)` 宏用于 GraphModule 派生
- `ENUM_CLASS_RANGE(first, last)` 来自 `EnumUtil.h`
- `AnsiString` = `String<char>`，来自 CMCore `<hgl/type/String.h>`
- 数学类型来自 CMMath: `Vector3f` / `Vector3d` / `Matrix4f`（GLM 基础）
- `VkShaderStageFlagBits` 需要显式 cast: `(VkShaderStageFlagBits)VK_SHADER_STAGE_VERTEX_BIT`

---

## 五、构建信息

```
构建目录: build_new
CMake 重新配置: cmake build_new
构建命令: cmake --build build_new --config Debug
编译器: MSVC (VS 18/Community), C++20
```

### 构建状态
- **重构代码**: 零错误（全项目构建验证通过）
- **已知预存错误**: `LoadGeometry.cpp` 在 examples 05/06 中存在 `MiniPackReader` API 不匹配问题 — 与本次重构无关

---

## 六、延后步骤汇总

| 步骤 | 内容 | 延后原因 |
|------|------|----------|
| 2.10 | DeviceQualityProfile 自动检测 | 需要 Vulkan headers |
| 2.11 | AppendGLSLDefines | 需要 AnsiString 集成 |
| 3.3–3.6 | Descriptor Set Vulkan 集成 | 需要 Vulkan 运行时 |
| 4.3–4.5 | Reversed-Z 管线集成 | 需要 example/pipeline 集成 |
| 4.7.2 | Camera 相对渲染 — 着色器端 | 需要管线集成 |
| 4.7.4 | Camera 相对渲染 — Example | 需要管线集成 |

---

## 七、下一步工作

**Stage 7: Forward 材质迁移** (Steps 7.1–7.15)
- 将现有材质逐个迁移到新的 Compositor 系统
- 需要完整的 UBO / descriptor chain
- CompositorRenderTest 中的实际 `vkCmdDraw` 渲染将在此阶段实现

**后续阶段** (8–16):
- Stage 8: 多灯光系统
- Stage 9: Shadow 系统
- Stage 10: Post-processing
- Stage 11–16: 详见 `RefactoringPlan_Incremental.md`

---

## 八、完整文件变更清单

### 新建文件 (共 ~30 个)

```
# Stage 2 — 核心类型
inc/hgl/mtl/new/SurfaceType.h
inc/hgl/mtl/new/QualityTier.h
inc/hgl/mtl/new/BlendMode.h
inc/hgl/mtl/new/PassType.h
inc/hgl/mtl/new/PlatformBackend.h
inc/hgl/mtl/new/MaterialCategory.h
inc/hgl/mtl/new/NewShaderPermutationKey.h
inc/hgl/mtl/new/MaterialPresetDef.h
inc/hgl/mtl/new/DeviceQualityProfile.h

# Stage 3 — Descriptor Set
inc/hgl/mtl/new/NewDescriptorSetType.h
inc/hgl/mtl/new/DescriptorSetBindings.h

# Stage 4 — Reversed-Z
inc/hgl/graph/camera/ReversedZProj.h
ShaderLibrary/common/depth_utils.glsl

# Stage 5 — Compositor / Shader
inc/hgl/shadergen/CompositorAssembler.h
src/ShaderGen/CompositorAssembler.cpp
inc/hgl/shadergen/PresetShaderCompiler.h
src/ShaderGen/PresetShaderCompiler.cpp
inc/hgl/shadergen/SPVCache.h
src/ShaderGen/SPVCache.cpp
inc/hgl/mtl/new/NewDescriptorSetLayoutFactory.h
src/ShaderGen/NewDescriptorSetLayoutFactory.cpp
inc/hgl/mtl/new/NewDescriptorBinding.h
src/ShaderGen/NewDescriptorBinding.cpp
src/ShaderGen/NewShaderPermutationKey.cpp
src/ShaderGen/DeviceQualityProfile.cpp
ShaderLibrary/compositor/main_forward_opaque.vert.glsl
ShaderLibrary/compositor/main_forward_opaque.frag.glsl
ShaderLibrary/common/surface_interface.glsl
ShaderLibrary/surface/standard_surface.glsl
ShaderLibrary/common/lighting.glsl
example/Basic/CompositorRenderTest.cpp

# Stage 6 — SSBO Vertex Fetch
ShaderLibrary/common/vertex_fetch_vbo.glsl
inc/hgl/graph/VertexDataBufferManager.h
src/SceneGraph/VertexDataBufferManager.cpp
```

### 修改的文件

```
# Stage 1
inc/hgl/shadergen/RenderFlowDef.h
inc/hgl/shadergen/ShaderComposition.h

# Stage 4
inc/hgl/graph/camera/Camera.h          (use_reversed_z, world_position_double)
inc/hgl/graph/camera/CameraInfo.h       (cameraPosWorld)
src/SceneGraph/CMakeLists.txt           (ReversedZProj 注册)

# Stage 5 & 6
inc/hgl/vk/VKRenderPass.h              (public CreatePipeline 重载, GeometryFetchMode 重载)
src/Vulkan/VKRenderPass.cpp             (对应实现)
src/Vulkan/pipeline/VKPipelineData.cpp  (nullptr VIL 支持)
inc/hgl/graph/geo/VKGeometry.h          (SSBO allocation 字段)
src/SceneGraph/CMakeLists.txt           (SSBO_VERTEX_FETCH_FILES)
example/Basic/CMakeLists.txt            (CompositorRenderTest 目标)

# 文档
doc/SimplifiedMaterialSystem_Design.md  (SPV 部署生命周期)
doc/RenderingPipeline_Design.md         (§14.5/§14.6 拆分)
doc/RefactoringPlan_Incremental.md      (部署说明 + Step 5.8/5.9 注解)
```

---

## 九、换机开发检查清单

1. **拉取代码**: 确保所有分支/提交已推送并在新机器上拉取
2. **确认环境**: MSVC (VS 18/Community), CMake, C++20 支持
3. **构建验证**:
   ```
   cmake build_new
   cmake --build build_new --config Debug
   ```
4. **确认文件存在**: 核对上述"完整文件变更清单"中所有文件均已同步
5. **阅读设计文档**: `doc/RefactoringPlan_Incremental.md` 中找到当前阶段
6. **继续实现**: 从 Stage 7 (Forward 材质迁移) 开始，或先处理延后步骤

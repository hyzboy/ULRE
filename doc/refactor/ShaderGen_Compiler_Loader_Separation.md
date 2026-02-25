# Shader 生成器 / 编译器 / 加载器彻底分离设计方案

> 状态：设计草稿，2026-02  
> 目标：以 `SPVParseData` 为边界，将 Generator / Compiler / Loader 完全解耦

---

## 1. 问题陈述

### 1.1 当前三层混杂

```
┌──────────────────────────────────────────────────────────┐
│ MaterialFactory (M_PureColor3D.cpp … 等18个文件)         │
│   调用 Std3DMaterial::CompileStdMaterial()               │
│     ↓                                                    │
│ ShaderCreateInfo::CreateShader()                         │
│   ① 生成 GLSL 字符串                    ← 生成器职责     │
│   ② 调用 CompileShader()               ← 编译器职责     │
│   ③ 存储 SPVData *spv_data             ← 存储职责       │
│     ↓                                                    │
│ MaterialManager::CreateShaderModule()                    │
│   ④ spv_data → VkShaderModule          ← 加载器职责     │
│   ⑤ MaterialDescriptorInfo 手工维护    ← 应由反射驱动   │
│      → VkDescriptorSetLayout                             │
│   ⑥ MaterialCreateInfo::vertex_attribs ← 应由反射驱动   │
│      → VkVertexInputState                                │
└──────────────────────────────────────────────────────────┘
```

### 1.2 核心症状

| # | 问题 | 影响 |
|---|------|------|
| P1 | `ShaderCreateInfo` 既生成 GLSL 也调 `CompileShader()` | 生成器与编译器强耦合 |
| P2 | `MaterialDescriptorInfo` 手工维护 set/binding | 与实际 GLSL 可以不一致、两套数据源 |
| P3 | `VkVertexInputState` 来自生成器声明，非 SPV 反射 | 同上，漂移风险 |
| P4 | `SPVParseData` 里 `ParseSPV()` 的结果从未用于 VkLayout | 反射功能形同虚设 |
| P5 | `SPVParseData` 固定 `name[32]`、无 buffer_size、无 member 反射 | 不足以做完整验证 |
| P6 | 没有离线编译缓存机制 | 每次启动都要重编译 GLSL |

---

## 2. 目标三层架构

```
Layer 1: ShaderGen (纯 GLSL 文本生成)
  输入: MaterialSpec + PipelineMode
  输出: std::string GLSL (vert / frag / [geom / task / mesh])
  依赖: STL only + inja (模板引擎)
  不允许: #include vulkan、不允许调编译器、不允许文件系统

      ─────────────── std::string GLSL ───────────────────

Layer 2: GLSLCompiler.dll/.so (编译 + 反射)
  输入: GLSL text + CompileInfo (target vulkan/spv version)
  输出: SPVData (raw SPIR-V bytes + log)
        SPVParseData (descriptor/IO 完整反射结构)
  依赖: glslang + SPIRV-Cross (封装在 dll 内，不外泄 ABI)
  接口: C API via GetInterface()，跨平台 dll/so

      ─────── SPVData* + SPVParseData* (C struct) ─────────

Layer 3: Material Loader (ULRE 引擎)
  输入: SPVData* → VkShaderModule
        SPVParseData* → VkDescriptorSetLayout
        SPVParseData* → VkVertexInputAttributeDescription[]
  依赖: Vulkan SDK + ULRE 引擎
  不允许: 调 glslang、不允许重新生成 GLSL
```

---

## 3. SPVParseData 重新设计

### 3.1 当前设计的不足

```cpp
// 当前 GLSLCompiler/glsl2spv.cpp
struct Descriptor {
    char name[32];   // 溢出风险
    uint8_t set;
    uint8_t binding;
    // ❌ 缺 descriptor kind (UBO/SSBO/Sampler…)
    // ❌ 缺 buffer_size（UBO/SSBO 大小验证）
    // ❌ 缺 array_count（纹理数组）
    // ❌ 缺成员反射（无法验证 MaterialInstance struct）
};

// 按 VkDescriptorType 分成11个数组，种类隐含于数组下标
// 导致枚举所有描述符需要遍历11个数组
ShaderDescriptorResource resource; // resource[VK_DESCRIPTOR_TYPE_COUNT]
```

### 3.2 建议的新 SPVParseData（共享头文件）

**文件：`GLSLCompiler/SPVParseData.h`（GLSLCompiler 仓库导出，ULRE 通过 submodule 使用）**

```cpp
#pragma once
#include <stdint.h>  // 纯 C 兼容，无 STL

//----------------------------------------------------------------------
// 基础枚举（紧凑存储）
//----------------------------------------------------------------------
enum class SPVBaseType : uint8_t {
    Bool = 0, Int, UInt, Float, Double, Struct, Image, Sampler, MAX
};

enum class SPVDescriptorKind : uint8_t {
    UniformBuffer        = 0,   // UBO
    StorageBuffer        = 1,   // SSBO
    CombinedImageSampler = 2,   // sampler2D / sampler2DArray
    SampledImage         = 3,   // texture2D (separable)
    StorageSampler       = 4,   // sampler (separable)
    StorageImage         = 5,   // image2D (read/write)
    InputAttachment      = 6,   // subpassInput
    PushConstant         = 7,
    MAX
};

//----------------------------------------------------------------------
// UBO/SSBO 成员反射（用于验证 MaterialInstance struct 对齐）
//----------------------------------------------------------------------
struct SPVMember {
    char        name[64];
    uint32_t    offset;       // 字节偏移
    uint32_t    size;         // 字节大小
    SPVBaseType basetype;
    uint8_t     vec_size;     // 1-4
    uint8_t     col_count;    // 矩阵列数（非矩阵为0）
    uint8_t     array_size;   // 数组大小（非数组为0）
    uint8_t     _pad;
};

//----------------------------------------------------------------------
// 描述符绑定（所有类型统一表示）
//----------------------------------------------------------------------
struct SPVDescriptorBinding {
    char                name[64];
    uint32_t            set;
    uint32_t            binding;
    SPVDescriptorKind   kind;
    uint8_t             _pad[3];
    uint32_t            array_count;    // 纹理数组大小；非数组=1
    uint32_t            buffer_size;    // UBO/SSBO struct 大小（字节）；其它=0
    uint32_t            member_count;
    SPVMember          *members;        // 仅 UBO/SSBO 时非 nullptr
};

//----------------------------------------------------------------------
// Vertex Input / Stage Output 属性
//----------------------------------------------------------------------
struct SPVStageAttribute {
    char        name[64];
    uint32_t    location;
    uint32_t    component;  // location 内分量偏移（packed 时）
    SPVBaseType basetype;
    uint8_t     vec_size;   // 分量数（1-4）
    uint8_t     _pad[2];
};

//----------------------------------------------------------------------
// Push Constant Range
//----------------------------------------------------------------------
struct SPVPushConstantRange {
    char     name[64];
    uint32_t offset;
    uint32_t size;
};

//----------------------------------------------------------------------
// Subpass Input（Mobile Vulkan subpassLoad）
//----------------------------------------------------------------------
struct SPVSubpassInput {
    char     name[64];
    uint32_t attachment_index;
    uint32_t binding;
};

//----------------------------------------------------------------------
// 通用数组容器（C ABI 安全）
//----------------------------------------------------------------------
template<typename T>
struct SPVArray {
    uint32_t count;
    T       *items;
};

//----------------------------------------------------------------------
// 顶层结构
//----------------------------------------------------------------------
struct SPVParseData {
    SPVArray<SPVStageAttribute>    stage_inputs;    // VS/mesh inputs
    SPVArray<SPVStageAttribute>    stage_outputs;   // VS outputs / FS outputs
    SPVArray<SPVDescriptorBinding> descriptors;     // 所有描述符，统一列表
    SPVArray<SPVPushConstantRange> push_constants;
    SPVArray<SPVSubpassInput>      subpass_inputs;

public:
    SPVParseData()  { memset(this, 0, sizeof(*this)); }
    ~SPVParseData();  // 实现在 dll 内
};
```

**与当前结构对比**：

| 字段 | 当前 | 新 |
|------|------|----|
| 名称缓冲区 | `name[32]` | `name[64]` |
| 描述符分类 | 11 个独立数组（按 VkDescriptorType） | 1 个统一数组（每项带 `kind`） |
| descriptor kind 获取 | 依赖数组下标隐含 | `binding.kind` 显式 |
| buffer_size | ❌ 无 | ✅ `buffer_size` |
| UBO 成员反射 | ❌ 无 | ✅ `members[]` |
| array_count (纹理数组) | ❌ 无 | ✅ `array_count` |
| attribute component | ❌ 无 | ✅ `component` |

---

## 4. 三层 API 设计

### 4.1 Layer 1 — Generator API（无编译，无 Vulkan）

```cpp
// inc/hgl/shadergen/ShaderGenAPI.h
// 依赖: STL only

namespace hgl::shadergen {

// 生成器直接输出文本，不再调 Compile()
struct GeneratedGLSL {
    std::string vert;   // 可能为空（compute/mesh shader）
    std::string frag;   // 可能为空（depth-only）
    std::string geom;   // 可选
    std::string task;   // 可选（mesh shader path）
    std::string mesh;   // 可选（mesh shader path）
    std::string comp;   // 可选（compute）

    // 诊断信息
    std::string error_log;       // 生成阶段错误（逻辑错误，非编译错误）
    bool        success = false;
};

// MaterialSpec：开发者提供的受限输入
struct MaterialSpec {
    std::string                         name;
    ShaderPermutationKey                permutation;
    std::vector<std::string>            required_resources; // 语义名
    std::string                         lighting_fn_glsl;   // EvalLighting() 实现
    std::string                         surface_fn_glsl;    // EvalSurface()  实现
};

// PipelineMode：六轴模式
struct PipelineMode {
    RenderPathType      render_path   = RenderPathType::Forward;
    CoverageType        coverage      = CoverageType::Solid;
    InputSourceType     input_source  = InputSourceType::VertexInput;
    StageTopologyType   topology      = StageTopologyType::VS_FS;
    GBufferFormatLevel  gbuffer_fmt   = GBufferFormatLevel::DesktopStandard;
    ForwardLightingType fwd_lighting  = ForwardLightingType::PerPixel;
};

// 生成接口（Layer 1 唯一对外入口）
GeneratedGLSL GenerateShader(const MaterialSpec &spec, const PipelineMode &mode);

// 内置（硬编码）材质生成 — 编辑器 fallback
GeneratedGLSL GenerateBuiltinShader(BuiltinMaterialType type, const PipelineMode &mode);

} // namespace hgl::shadergen
```

### 4.2 Layer 2 — GLSLCompiler DLL API（保持 C ABI）

在现有 `GLSLCompilerInterface` 基础上，`ParseSPV` 返回新 `SPVParseData *`：

```cpp
// GLSLCompiler/glsl2spv.cpp — 更新 GLSLCompilerInterface
struct GLSLCompilerInterface {
    bool        (*Init)();
    void        (*Close)();

    bool        (*GetLimit)(void *, int);
    bool        (*SetLimit)(void *, int);

    uint32_t    (*GetType)(const char *ext_name);   // 文件扩展名 → VkShaderStageFlagBits

    SPVData *   (*Compile)(uint32_t stage, const char *src, const CompileInfo *ci);
    SPVData *   (*CompileFromPath)(uint32_t stage, const char *path, const CompileInfo *ci);
    void        (*Free)(SPVData *);                 // 释放 SPVData

    SPVParseData *(*ParseSPV)(const SPVData *);     // 反射（const：不修改 SPVData）
    void         (*FreeParseSPVData)(SPVParseData *);

    // 新增：直接保存/加载 SPVParseData（用于离线缓存）
    bool         (*SaveParseData)(const SPVParseData *, const char *path);
    SPVParseData *(*LoadParseData)(const char *path);
};
```

注意：`ParseSPV` 参数改为 `const SPVData *`（不消耗 SPVData），以便 SPVData 可独立缓存至磁盘。

### 4.3 Layer 3 — Loader API（ULRE 引擎）

```cpp
// inc/hgl/graph/mtl/SPVLayoutBuilder.h
namespace hgl::graph {

// 从 SPVParseData 构建 VkDescriptorSetLayout
// 合并多个 shader stage 的反射数据（vert + frag），自动设置 stageFlags
VkDescriptorSetLayout BuildDescriptorSetLayout(
    VulkanDevice        *device,
    const SPVParseData  *vert_parse,
    const SPVParseData  *frag_parse,
    const SPVParseData  *geom_parse = nullptr);

// 从 SPVParseData (vertex stage) 构建 VkVertexInputAttributeDescription[]
// 配合 VertexInputLayout (VIL) 使用：按 name 匹配 VAB 通道
struct VertexAttributeMatch {
    uint32_t    location;
    VkFormat    vk_format;   // 从 basetype+vec_size 推导
    std::string name;        // 语义名（Position, Normal, TexCoord0 …）
};
std::vector<VertexAttributeMatch> BuildVertexInputAttributes(
    const SPVParseData *vert_parse);

// 跨阶段 IO 验证：确保 vert outputs 与 frag inputs 一致
struct IOValidationResult {
    bool        ok;
    std::string error_message;
};
IOValidationResult ValidateStageIO(
    const SPVParseData *vert_parse,
    const SPVParseData *frag_parse);

// 生成器意图 vs 反射结果一致性验证
// 比较 MaterialDescriptorInfo（生成器声明）与 SPVParseData（编译器反射）
struct DescriptorConsistencyResult {
    bool        ok;
    std::string error_message;
};
DescriptorConsistencyResult ValidateDescriptorConsistency(
    const MaterialDescriptorInfo *gen_decl,
    const SPVParseData *vert_parse,
    const SPVParseData *frag_parse);

} // namespace hgl::graph
```

---

## 5. 完整数据流（新）

### 5.1 离线构建流（工具 / 测试）

```
                ┌─────────────────────────────┐
                │  MaterialSpec + PipelineMode │
                └─────────────┬───────────────┘
                              │
                    ┌─────────▼────────┐
                    │  GenerateShader() │  ← Layer 1（纯 STL）
                    │  hgl::shadergen   │
                    └─────────┬────────┘
                              │ vert.glsl, frag.glsl (std::string)
                   ┌──────────▼──────────┐
                   │  GLSLCompiler.dll   │  ← Layer 2（glslang+SPIRV-Cross）
                   │  gsi->Compile()     │
                   │  gsi->ParseSPV()    │
                   └──────────┬──────────┘
                              │ SPVData*, SPVParseData*
                  ┌───────────▼───────────┐
                  │  离线缓存写入         │
                  │  vert.spv, frag.spv   │
                  │  vert.spvparse        │  ← 二进制或 JSON
                  │  frag.spvparse        │
                  └───────────────────────┘
```

### 5.2 运行时加载流（引擎）

```
                  ┌───────────────────────┐
                  │  从磁盘加载缓存       │
                  │  vert.spv, frag.spv   │
                  │  vert.spvparse        │
                  │  frag.spvparse        │
                  └───────────┬───────────┘
                              │
                   ┌──────────▼──────────┐
                   │  SPVLayoutBuilder    │  ← Layer 3（ULRE 引擎）
                   │  BuildDescriptorSetLayout()    → VkDescriptorSetLayout
                   │  BuildVertexInputAttributes()  → VkVertexInput
                   │  ValidateStageIO()             → 诊断
                   └──────────┬──────────┘
                              │
                   ┌──────────▼──────────┐
                   │  VulkanDevice        │
                   │  CreateShaderModule()│  ← SPV bytes → VkShaderModule
                   └─────────────────────┘
```

### 5.3 运行时回退（编辑器 / 文件丢失）

```
                  ┌───────────────────────┐
                  │  GenerateBuiltinShader│  ← Layer 1（硬编码）
                  │  (BuiltinMaterialType)│
                  └───────────┬───────────┘
                              │ GLSL strings（in-process）
                   ┌──────────▼──────────┐
                   │  GLSLCompiler.dll   │  ← Layer 2（运行时编译）
                   │  Compile + ParseSPV │
                   └──────────┬──────────┘
                              │ SPVData + SPVParseData（不写磁盘）
                   ┌──────────▼──────────┐
                   │  SPVLayoutBuilder    │  ← Layer 3
                   └─────────────────────┘
```

---

## 6. 离线编译缓存格式

```
<data_root>/materials/
  PureColor3D/
    forward_solid_vi/               ← PipelineMode hash key
      vert.spv
      frag.spv
      vert.spvparse                 ← 二进制 SPVParseData 序列化
      frag.spvparse
      meta.json                     ← 版本、hash、生成时间
    forward_solid_ssbo/
      …
    gbuffer_solid_vi/
      …
  BasicLit/
    …
```

**`meta.json` 示例**：

```json
{
  "material": "PureColor3D",
  "pipeline_mode": "forward_solid_vi",
  "generator_version": "1",
  "glsl_hash_vert": "a1b2c3d4",
  "glsl_hash_frag": "e5f6a7b8",
  "spv_vulkan_version": "1.3",
  "generated_at": "2026-02-25T10:00:00Z"
}
```

运行时通过 `glsl_hash_*` 判断缓存是否有效（生成器代码变更 → hash 变 → 重编译）。

---

## 7. VkFormat 推导表（basetype + vec_size → VkFormat）

Layer 3 中 `BuildVertexInputAttributes()` 需要将 `SPVBaseType + vec_size` 转为 `VkFormat`：

```cpp
VkFormat SPVAttribToVkFormat(SPVBaseType bt, uint8_t vec_size) {
    if (bt == SPVBaseType::Float) {
        switch (vec_size) {
            case 1: return VK_FORMAT_R32_SFLOAT;
            case 2: return VK_FORMAT_R32G32_SFLOAT;
            case 3: return VK_FORMAT_R32G32B32_SFLOAT;
            case 4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        }
    }
    if (bt == SPVBaseType::Int) {
        switch (vec_size) {
            case 1: return VK_FORMAT_R32_SINT;
            case 2: return VK_FORMAT_R32G32_SINT;
            case 3: return VK_FORMAT_R32G32B32_SINT;
            case 4: return VK_FORMAT_R32G32B32A32_SINT;
        }
    }
    if (bt == SPVBaseType::UInt) {
        switch (vec_size) {
            case 1: return VK_FORMAT_R32_UINT;
            case 2: return VK_FORMAT_R32G32_UINT;
            case 3: return VK_FORMAT_R32G32B32_UINT;
            case 4: return VK_FORMAT_R32G32B32A32_UINT;
        }
    }
    return VK_FORMAT_UNDEFINED;
}
```

注意：半精度（f16）、8bit、16bit 整数需要额外推导规则，暂不列出。

---

## 8. 现有代码的对应变化

### 8.1 删除 / 简化（精简后更简单）

| 文件 | 变化 | 原因 |
|------|------|------|
| `ShaderCreateInfo::CreateShader()` | 不再调 `CompileShader()` | Generator 不再编译 |
| `ShaderCreateInfo::spv_data` | 移除此字段 | SPVData 由 Layer 2 管理 |
| `MaterialDescriptorInfo` 作为 layout 源 | 降级为"生成器声明"，不再是唯一源 | Layout 改由 SPVParseData 驱动 |
| `MaterialCreateInfo::vertex_attributes` | 降级为生成器内部状态 | VertexInput 改由 SPVParseData 驱动 |
| `GLSLCompiler.cpp` `ParseSPV()` 调用 | 移至 Layer 3 `SPVLayoutBuilder` | 职责分离 |

### 8.2 新增

| 文件 | 内容 |
|------|------|
| `GLSLCompiler/SPVParseData.h` | 新 SPVParseData 结构（GLSLCompiler 仓库） |
| `inc/hgl/graph/mtl/SPVLayoutBuilder.h` | Layer 3 builder API |
| `src/SceneGraph/module/SPVLayoutBuilder.cpp` | 实现 BuildDescriptorSetLayout / BuildVertexInputAttributes / ValidateStageIO |
| `inc/hgl/shadergen/ShaderGenAPI.h` | Layer 1 纯生成接口 |
| `src/ShaderGen/ShaderGenAPI.cpp` | 实现 GenerateShader() |
| `tools/shader_compile/` | 离线编译工具（可选独立 exe）：调 Layer 1 + 2，写 spvparse 缓存 |
| `test/test_SPVLayoutBuilder.cpp` | Layer 3 单元测试 |

### 8.3 迁移路径

**阶段 M-A（最高优先，止血）**
1. 在 `GLSLCompiler` 仓库新建 `SPVParseData.h`，内含新结构体（向后兼容旧结构的 `ParseSPV_Legacy()` 备用）
2. ULRE `GLSLCompiler.cpp` wrapper 更新 `SPVParseData` 转发声明

**阶段 M-B（解耦 Generator）**
3. `ShaderCreateInfo` 拆分：`BuildGLSL()` 保留，`CreateShader()` 里的 `CompileShader()` 移出
4. 新增 `MaterialCompileSession`：持有 `GeneratedGLSL` + `SPVData*` + `SPVParseData*`（三件套），由调用方统一管理生命周期

**阶段 M-C（SPVLayoutBuilder）**
5. 实现 `SPVLayoutBuilder.cpp`
6. `MaterialManager` 改为调 `SPVLayoutBuilder::BuildDescriptorSetLayout()` 替代 `MaterialDescriptorInfo` 直接转 layout
7. 添加 `ValidateDescriptorConsistency()` 检查（DEBUG only）

**阶段 M-D（离线缓存）**
8. 新增 `shader_compile` 工具，生成 `.spv` + `.spvparse` 文件
9. `MaterialManager` 优先加载磁盘缓存，缺失时回退到运行时编译

**阶段 M-E（清理）**
10. `MaterialDescriptorInfo` 的 set/binding 字段降级为 ASSERT 验证
11. `MaterialCreateInfo::vertex_attributes` 降级为内部 ASSERT 验证
12. 移除 `ShaderCreateInfo::spv_data`

---

## 9. MaterialDescriptorInfo 保留理由

有人会问：既然 SPVParseData 包含完整反射，为什么还要保留 `MaterialDescriptorInfo`？

答：**保留，但职责变更**。

| 职责 | 当前 | 新 |
|------|------|----|
| 确定 set/binding 编号 | MaterialDescriptorInfo **是唯一来源** | Generator 内部确定，SPVParseData **验证** |
| 构建 VkDescriptorSetLayout | MaterialDescriptorInfo → VkLayout | SPVParseData → VkLayout（主路径） |
| 人类可读文档 | 部分 | 保留 |
| 生成 GLSL layout 声明 | ✅ | ✅ 保留（仍是生成器输出 GLSL 的基础） |
| 验证生成器意图 == 编译器反射 | ❌ 无 | ✅ `ValidateDescriptorConsistency()` |

新角色：`MaterialDescriptorInfo` 是生成器的**内部约定**（生成 GLSL 时用），  
`SPVParseData` 是**外部事实**（编译器反射），二者在 DEBUG 模式下必须一致。

---

## 10. 硬编码内置材质的保留策略

编辑器不能依赖文件系统 → 需要硬编码 fallback：

```cpp
// 保留的内置材质列表（编辑器 fallback）
enum class BuiltinMaterialType {
    PureColor2D,     // 编辑器 UI 底层
    PureColor3D,     // gizmo / 线框
    VertexColor3D,   // debug 顶点着色
    Gizmo3D,         // 世界轴 / 辅助线
    // … 不超过 6 个，够基础渲染即可
};
```

这些材质的 GLSL 全部硬编码为 `constexpr const char *`，  
在 Layer 1 中通过 `GenerateBuiltinShader()` 返回，  
运行时走 Layer 2 编译（GLSLCompiler.dll），  
结果**不写磁盘缓存**（避免文件依赖）。

其余材质（BasicLit / PBR / Terrain / Sky 等）走离线缓存路径，  
运行时只做加载，不做编译。

---

## 11. 本方案对"一 DrawCall 全场景"的适配

本三层架构对 GPU-Driven 场景的一致性约束：

1. **生成器**：`PipelineMode.input_source` 选 `SSBO_VertexInput` 时，生成器自动切换 VS 骨架为 SSBO 读取路径（不改 `MaterialSpec`）
2. **描述符固定分配**：Generator 内部的 `ResourceRegistry` 硬绑定：
   - set=0 binding=0 → TransformSSBO (L2W 矩阵)
   - set=0 binding=1 → CameraUBO
   - set=0 binding=2 → MaterialInstanceSSBO
   - set=0 binding=3 → LightUBO / LightSSBO
   - set=1 → Texture2DArray (固定 slot)
   这一分配不由材质决定，而由引擎架构固定，对所有材质均相同
3. **SPVParseData 验证**：编译后验证 descriptor 反射与 ResourceRegistry 分配一致 → 保证 VkDescriptorSet 绑定正确
4. **VertexInput 验证**：反射 VS stage_inputs，确认 `L2W_ID`（uint） 和 `MI_ID`（uint） 存在且 location 固定

---

## 12. 实施检查项

### GLSLCompiler 仓库

- [ ] 新建 `SPVParseData.h`（如第3节所述）
- [ ] `glsl2spv.cpp` 中 `ParseSPV()` 实现填充新结构
- [ ] `GLSLCompilerInterface` 新增 `SaveParseData` / `LoadParseData`
- [ ] 单元测试：编译一段已知 GLSL → 验证 SPVParseData 字段正确

### ULRE 仓库

- [ ] `src/ShaderGen/GLSLCompiler.h` 更新 `SPVParseData` forward decl 为新结构
- [ ] `ShaderCreateInfo` 移除 `SPVData *spv_data` 字段，新增 `std::string glsl_source`
- [ ] 新增 `MaterialCompileSession` 结构体（生命周期管理三件套）
- [ ] 新增 `inc/hgl/graph/mtl/SPVLayoutBuilder.h`
- [ ] 新增 `src/SceneGraph/module/SPVLayoutBuilder.cpp`
- [ ] `MaterialManager` 改调 `SPVLayoutBuilder::BuildDescriptorSetLayout()`
- [ ] `SPVAttribToVkFormat()` 推导函数
- [ ] `ValidateStageIO()` 实现
- [ ] `ValidateDescriptorConsistency()` 实现（DEBUG only）
- [ ] `test/test_SPVLayoutBuilder.cpp` 测试
- [ ] `tools/shader_compile/` 离线编译工具（CMakeLists + main.cpp）
- [ ] 文件缓存加载路径（`.spv` + `.spvparse`）

---

> 本文档版本：2026-02-25  
> 对应工作计划节：SG-3（独立库化）+ SG-4（产物与验证）  
> 下一步：在 GLSLCompiler 仓库创建 `SPVParseData.h` PR

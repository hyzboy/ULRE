/// ShaderComposition_Architecture.md — 着色器合成框架详细设计

# 着色器合成系统设计文档

## 1. 核心理念

### 问题陈述
当前 `FixedMaterialDef` 要求开发者手写完整的 GLSL：
- **坐标变换重复**：每个材质都手工处理 L2W + VP 投影
- **光照适配困难**：支持不同光照模型（Lambert/PBR/Cel）需要多份代码副本
- **输出合成繁琐**：Alpha Blend、Additive、G-Buffer 需要手工选择 RT 写法
- **可维护性差**：修改一个通用部分要同步修改多个材质

### 解决方案：ComposedMaterialDef 体系
```
开发者业务代码                框架自动生成部分
┌──────────────────┐          ┌────────────────────────┐
│ VS Business      │          │ 前置宏 + 引入           │
│ FS Business      │    +      │ 通用结构体定义          │  →  完整 GLSL  →  SPV
│ MI 数据结构      │          │ 坐标变换函数            │
│ 顶点/描述符定义  │          │ 光照计算（自动选择）     │
└──────────────────┘          │ 输出合成（自动选择）     │
                              │ Main 函数骨架            │
                              └────────────────────────┘
```

### 关键设计原则
1. **分离关注点**：业务逻辑与框架代码完全解耦
2. **参数化驱动**：光照/输出模式通过枚举参数选择，不需代码改动
3. **Permutation 兼容**：ShaderPermutationKey → 光照选择 → 代码生成
4. **零成本抽象**：最终编译到相同 SPV，无额外 if-else 开销

### 新增硬约束（2026-02）
1. **目标优先级：一 Drawcall 全场景**
    - 渲染器主目标是极端批处理（一个 Drawcall 覆盖全场景），该目标已实现。
    - 3D 材质系统必须服务该目标，禁止引入会破坏批处理一致性的自由度。

2. **3D 材质采用“现代固定管线”而非自由材质图**
    - 不追求 Unreal 风格超自由拼装。
    - 开发者输入聚焦于：
      - 光照参数（参数块）
      - 数据需求（required resources）
      - 光照/着色函数（受限接口）
    - 其余流程（输入装配、坐标变换、输出合成、变体分发）由生成器统一完成。

3. **生成器按标准渲染模式分型，而非任意拼装**
    - 支持并固化以下模式组合：
    - 渲染路径：Forward / GBuffer Deferred / VBuffer Deferred / MobileSubpassGBufferDeferred / PostProcess
    - 覆盖模式：Solid / Mask / DepthOnlySolid / DepthOnlyMask
            - 输入模式：VertexInput / SSBO VertexInput（长期目标：全 SSBO GPU-Driven）
            - 阶段拓扑：VS/FS 或 MeshShader/FS
            - 后处理输出：GBuffer 通道组合（MotionVector 默认可关闭，按需开启）
            - 前向光照模式：PerPixel / PerVertex（低配与远景）
            - 法线压缩策略：VertexInput / NormalMap / GBuffer 三处可独立配置，生成器自动编解码
    - 通过受控 permutation 生成变体，不提供任意节点图式扩展点。

5. **GPU-Driven 演进方向（输入与阶段自适应）**
        - 未来主路径为全 GPU-Driven，几何输入从 VBO 渐进迁移到 SSBO 输入。
        - 合成器必须根据平台能力和配置自动选择：
            - 输入路径：`VertexInput`（兼容）或 `SSBO VertexInput`（主路径）
            - 阶段拓扑：`VS/FS`（兼容）或 `MeshShader/FS`（主路径）
        - 同一材质语义在不同路径上保持行为一致，仅底层装配方式不同。

4. **Shader 生成器工程独立化**
    - 目标形态：独立程序或独立动态库（dll/so）。
    - 不要求与引擎基础库共享容器与工具类型。
    - 允许直接采用 STL / Abseil / ankerl 作为核心基础设施。

### 单 Drawcall 运行时数据模型（当前实现，必须保持）

该模型是“一 Drawcall 全场景”的核心基础，生成器与材质系统必须围绕它设计：

1. **材质（Material）= 一套 Shader 语义**
    - 每个材质类型对应固定的一套着色语义，运行时可映射到 VS/FS 或 Mesh/FS 变体。
    - 材质不承载逐对象参数；逐对象差异全部进入 MaterialInstance。

2. **材质实例（MaterialInstance）= UBO/纹理配置实例**
    - 每个实例包含本对象所需的参数数据与纹理引用。
    - 纹理引用不是直接绑定单纹理，而是索引到 Texture Array Layer。

3. **对象关键 ID（当前由 Instanced Vertex Attribute 承载）**
    - `L2W_ID`：对象的 LocalToWorld 矩阵索引。
    - `MI_ID`：对象的 MaterialInstance 索引。
    - 在全 GPU-Driven 路径中，等价 ID 将由 SSBO/Draw 参数间接提供，不依赖固定 VBO instanced 通道。

4. **ID → SSBO 间接寻址**
    - `L2W_ID` 访问 Transform SSBO，取 `LocalToWorld`/法线相关数据。
    - `MI_ID` 访问 MaterialInstance SSBO，取参数与纹理层索引。

5. **纹理二级索引（MI → Texture Array Layer）**
    - MaterialInstance 中的纹理字段保存 Layer ID。
    - Shader 统一采样 `Texture2DArray/CubemapArray`，以 Layer ID 选具体贴图。

6. **结果约束**
    - 单次 Draw 不切材质绑定，不切对象级 descriptor。
    - 通过实例 ID 间接寻址实现全场景对象差异化。

数据流示意：

```
Instanced Attr: [L2W_ID, MI_ID]
        ├─ L2W_ID ──> TransformSSBO[L2W_ID] ──> LocalToWorld / NormalBasis
        └─ MI_ID  ──> MaterialSSBO[MI_ID]  ──> Params + TexLayerIDs
                                                              └─ sample(TextureArray, uv, layer)
```

---

## 2. 执行流程（以 BasicLit 为例）

### 输入
```cpp
ComposedMaterialDef BASIC_LIT_COMPOSED {
    .name = "BasicLit",
    .vertex_business = VertexShaderBusiness{BASIC_LIT_VS_BUSINESS},
    .fragment_business = FragmentShaderBusiness{BASIC_LIT_FS_BUSINESS},
    .output_mode = ShaderOutputMode::SingleRTAlphaBlend,
    .enable_lighting = true,
    // ... 其他字段
};

ShaderPermutationKey key = {
    .light_model = LightModelType::PBR,
    .ambient_model = AmbientModelType::IBL,
    .specular_channel = SpecularChannelType::Roughness,
    .shadow_receive = ShadowReceiveType::ShadowMap,
};
```

### 处理步骤

#### Step 1: 生成前置部分
```glsl
#version 450 core
#extension GL_ARB_gpu_shader_int64 : enable

// 由 key 决定的宏定义
#define LIGHT_MODEL 2      // PBR
#define AMBIENT_MODEL 1    // IBL
#define SPECULAR_CHANNEL 0 // Roughness
#define SHADOW_RECEIVE 1
```

#### Step 2: 生成通用结构体
```glsl
// 顶点输入（来自 VBO）
struct VertexInput {
    vec3 Position;
    vec3 Normal;
    vec2 TexCoord;
};

// VS → FS 插值
struct VS_Output {
    vec4 ClipPos;       // 隐式，用于 gl_Position
    vec3 WorldPos;      // 插值
    vec3 WorldNormal;   // 插值
    vec2 TexCoord;      // 插值
    // ... 其他 VS 生成的字段
};

// 光照计算输出
struct LightingOutput {
    vec3 diffuse;       // 漫反射颜色
    vec3 specular;      // 高光颜色
    vec3 reflection;    // 反射色（IBL 用）
};

// 材质实例数据
struct MaterialInstance {
    float Alpha;
    float Metallic;
    float Roughness;
};
```

#### Step 3: 注入坐标变换函数
```glsl
// 框架自动生成（基于描述符）
layout(set=0, binding=2) uniform TransformBlock {
    mat4 LocalToWorld;
    mat3 NormalMatrix;  // 法线矩阵 (L2WNorm^T)^{-1}
};

layout(set=0, binding=1) uniform CameraBlock {
    mat4 ViewMatrix;
    mat4 ProjectionMatrix;
    vec3 CameraWorldPos;
};

// 坐标变换辅助函数
vec4 TransformPosition(vec3 localPos) {
    vec4 worldPos = LocalToWorld * vec4(localPos, 1.0);
    return ProjectionMatrix * ViewMatrix * worldPos;
}

vec3 TransformNormal(vec3 localNorm) {
    return normalize(NormalMatrix * localNorm);
}
```

#### Step 4: 插入开发者的业务代码
```glsl
// 开发者编写的 VertexShaderBusiness
vec4 VertexShaderBusiness(const VertexInput vi) {
    vec3 albedo = texture(BaseColorMap, vso.TexCoord).rgb;
    vec3 normal = normalize(vso.WorldNormal);
    // ... 业务逻辑 ...
    return vec4(local_pos, 1.0);
}

// 开发者编写的 FragmentShaderBusiness
vec4 FragmentShaderBusiness(const VS_Output vso) {
    // ... 采样贴图、计算颜色 ...
    return vec4(finalColor, alpha);
}
```

#### Step 5: 生成光照计算代码（由 ShaderPermutationKey 决定）
```glsl
// 伪代码：框架根据 {LIGHT_MODEL=2, AMBIENT_MODEL=1, ...} 选择

#if LIGHT_MODEL == 2  // PBR
LightingOutput ComputeLighting(vec3 normal, vec3 albedo, vec3 view_dir) {
    LightingOutput out;
    
    // PBR 计算逻辑
    vec3 light_dir = normalize(LightDir);
    float NdotL = max(dot(normal, light_dir), 0.0);
    
    // Cook-Torrance BRDF
    vec3 F0 = mix(vec3(0.04), albedo, Metallic);
    // ... 详细 PBR 计算（来自 cook_torrance.glsl 模板）
    
    out.diffuse = albedo * (1.0 - Metallic) * NdotL;
    out.specular = F * (D * G) / (4.0 * NdotL * NdotV + 0.001);
    
    #if AMBIENT_MODEL == 1  // IBL
    out.reflection = texture(IrradianceMap, normal).rgb;
    #endif
    
    return out;
}
#endif

// 类似地，还有 LIGHT_MODEL == 0 (Lambert)，LIGHT_MODEL == 1 (Phong) 等分支
```

#### Step 6: 生成输出合成代码（由 output_mode 决定）
```glsl
// 根据 ShaderOutputMode::SingleRTAlphaBlend 生成

void ComposeFinalOutput(vec4 color_with_alpha, out vec4 out_rt0) {
    // Alpha Blend 模式：输出颜色 + Alpha
    out_rt0 = color_with_alpha;
    // RT blend 设置：ONE_MINUS_SRC_ALPHA（由运行时 pipeline state 指定）
}

// 若是 SingleRTAdditive，则：
// out_rt0 = vec4(color.rgb, 0.0);  // 忽略 alpha，RT blend = ONE_ONE
```

#### Step 7: 组装 main() 函数

**顶点着色器 main()**：
```glsl
layout(location=0) in vec3 in_Position;
layout(location=1) in vec3 in_Normal;
layout(location=2) in vec2 in_TexCoord;

layout(location=0) out vec3 out_WorldPos;
layout(location=1) out vec3 out_WorldNormal;
layout(location=2) out vec2 out_TexCoord;

void main() {
    VertexInput vi = VertexInput(
        in_Position,
        in_Normal,
        in_TexCoord
    );
    
    // 调用业务代码，得到 local space 坐标
    vec4 local_pos = VertexShaderBusiness(vi);  // 开发者提供
    
    // 自动应用坐标变换
    gl_Position = TransformPosition(local_pos.xyz) * local_pos.w;
    
    // 法线变换（由框架自动处理）
    out_WorldNormal = TransformNormal(vi.Normal);
    
    // 世界坐标（用于光照计算）
    vec4 world = LocalToWorld * local_pos;
    out_WorldPos = world.xyz;
    out_TexCoord = in_TexCoord;
    
    // 建立 VS_Output（对应 FS 输入）
    VS_Output vso = VS_Output(
        gl_Position,
        out_WorldPos,
        out_WorldNormal,
        out_TexCoord
    );
}
```

**片元着色器 main()**：
```glsl
layout(location=0) in vec3 in_WorldPos;
layout(location=1) in vec3 in_WorldNormal;
layout(location=2) in vec2 in_TexCoord;

layout(location=0) out vec4 out_RT0;  // 或多个 RT 取决于 output_mode

void main() {
    VS_Output vso = VS_Output(
        gl_FragCoord,  // 不用，这里只是占位
        in_WorldPos,
        in_WorldNormal,
        in_TexCoord
    );
    
    // 调用业务代码，得到颜色 + alpha
    vec4 color_alpha = FragmentShaderBusiness(vso);  // 开发者提供
    
    // 计算光照（如果启用，根据 ShaderPermutationKey）
    LightingOutput lighting = ComputeLighting(
        normalize(vso.WorldNormal),
        color_alpha.rgb,  // 假设 FS 返回 diffuse color
        normalize(CameraWorldPos - vso.WorldPos)
    );
    
    // 应用光照到颜色 + 合成输出
    vec3 final_color = color_alpha.rgb * (lighting.diffuse + lighting.specular);
    
    // 根据 output_mode 生成最终输出
    ComposeFinalOutput(vec4(final_color, color_alpha.a), out_RT0);
}
```

### 最终生成的完整 GLSL 大约 250 行
```
前置部分（50 行）
通用结构体（40 行）
坐标变换函数（30 行）
业务 VS 代码（20 行）
业务 FS 代码（40 行）
光照计算代码（60 行）← 根据 ShaderPermutationKey 选择
输出合成代码（10 行）
main() 函数（20-30 行）
```

---

## 3. 框架与多个 Permutation 的交互

### 场景：BasicLit 要同时支持 Lambert + PBR + IBL 的 3×3×2 组合

```cpp
// 方式 1：传统（要生成 3×3×2 = 18 份 GLSL）
for (int light_model = 0; light_model < 3; light_model++) {
    for (int ambient = 0; ambient < 3; ambient++) {
        for (int shadow = 0; shadow < 2; shadow++) {
            key = {light_model, ambient, ...};
            glsl_src = ComposeShaderAsString(BASIC_LIT_COMPOSED, key);
            spv[i] = CompileGLSLToSPV(glsl_src);
        }
    }
}

// 框架处理（ComposedShaderGenerator）
class ComposedShaderGenerator {
    AnsiString ComposeFragmentShader(
        const ComposedMaterialDef &def,
        const ShaderPermutationKey &key
    ) {
        // 1. 复制开发者代码
        result += def.fragment_business->code;
        
        // 2. 根据 key.light_model 注入光照计算
        switch (key.light_model) {
            case LightModelType::Lambertian:
                result += GetLambertianLightingCode(key);
                break;
            case LightModelType::PBR:
                result += GetPBRLightingCode(key);
                break;
            // ...
        }
        
        // 3. 根据 key.ambient_model 注入环境光
        switch (key.ambient_model) {
            case AmbientModelType::FixedColor:
                result += "#define AMBIENT_FIXED\n";
                break;
            case AmbientModelType::IBL:
                result += GetIBLCode();
                break;
            // ...
        }
        
        // 4. 生成输出合成代码
        result += GetOutputCompositionCode(def.output_mode);
        
        return result;
    }
};
```

### 生成 18 个 shader variant 的成本分析

| 操作 | 成本 |
|------|------|
| GLSL 生成（18 份） | 18 × 5ms = 90ms |
| GLSL 到 SPIR-V 编译 | 18 × 50ms = 900ms |
| **总耗时** | **~1 秒**（可缓存） |

---

## 4. 未来架构重设计（固定流程分型）

### 4.1 系统定位

Shader 生成器定位为“受控组合器（Controlled Composer）”：
- 输入是受限语义（参数、资源需求、光照函数）。
- 输出是固定流程下的变体 shader 包。
- 不支持任意自由拼图式 shader graph。

### 4.2 统一模式矩阵

生成目标由六轴 + 一组格式约束定义：

| 轴 | 枚举 |
|---|---|
| Render Path | Forward / GBuffer / VBuffer / MobileSubpassGBuffer / PostProcess |
| Coverage | Solid / Mask / DepthOnlySolid / DepthOnlyMask |
| Input Source | VertexInput / SSBO VertexInput |
| Stage Topology | VS/FS / Mesh/FS |
| GBuffer Format Level | MobileLite / MobileExtended / DesktopStandard / DesktopFull / Custom |
| Forward Lighting | PerPixel / PerVertex / AutoByCapability |

附加约束（非独立轴）：
- `PostProcess Output Channels` 为通道组合掩码（位集），且必须是 `GBuffer Format` 可用通道的子集。

其中 `Input Source` 在运行时落地为两类固定路径：
- **VertexInput + Instanced IDs**：几何属性走 VertexInput，对象差异走 `L2W_ID/MI_ID`。
- **SSBO VertexInput + Instanced IDs**：几何与对象数据均可通过索引化 SSBO 读取。

其中 `Stage Topology` 在运行时落地为两类固定路径：
- **VS/FS**：传统图形管线，作为兼容与回退路径。
- **Mesh/FS**：GPU-Driven 主路径，mesh shader 完成几何装配与索引化读取。

不论哪种输入路径，都必须兼容以下不变式：
- `MI_ID -> MaterialSSBO` 参数读取
- `TexLayerID -> TextureArrayLayer` 采样
- 单 Drawcall 下不做对象级 descriptor 切换

不论哪种阶段拓扑，都必须兼容以下不变式：
- 材质参数语义一致（同一 MaterialSpec）
- 光照与输出语义一致（同一 Render Path/Coverage）
- 变体键可统一追踪（Topology 只作为额外轴，不改材质语义）

当 `RenderPath = PostProcess` 时，额外约束：
- 覆盖模式通常固定为 `Solid`（全屏/区域后处理），DepthOnly 由通道掩码显式控制。
- 输出由 `PostProcess Output Channels`（组合掩码）控制，可任意组合，例如：
    - `{Normal}`
    - `{Emissive}`
    - `{MotionVector}`
    - `{Normal, Depth}`
    - `{Emissive, MotionVector}`
- 该通道集合必须满足：`PostProcessOutput ⊆ GBufferFormatChannels`。
- 后处理模式下可跳过常规光照合成路径。

当 `RenderPath = GBuffer/VBuffer` 时，额外约束：
- 开发者在指定光照算法档位时，必须同时指定 `GBufferFormatLevel`。
- 生成器根据 `GBufferFormat` 自动生成 shader 参数结构体（如 `GBufferParams`）。
- 后处理阶段直接复用该格式定义作为可输出通道列表。
- `MotionVector` 作为可选通道，不强制进入默认格式（尤其移动端默认关闭）。
- 通过 `GBufferFormatSpec.enable_motion_vector` 显式启用后才加入格式通道掩码。

当 `RenderPath = MobileSubpassGBuffer` 时，额外约束：
- 面向移动端 Vulkan 路径，采用 `subpassLoad` 读取前序 subpass 输出。
- GBuffer 数据优先以内存别名/输入附件方式在 render pass 内传递，减少外部带宽。
- 通道可用性仍受 `GBufferFormat` 约束，且需满足 subpass 输入附件布局要求。
- 该路径与常规 `GBufferDeferred` 在材质语义上保持一致，仅数据读取机制不同。

当 `RenderPath = Forward` 时，额外约束：
- `Forward Lighting` 可选择：
    - `PerPixel`：常规片元光照路径（质量优先）
    - `PerVertex`：顶点光照路径（极低配机型或远端物体）
- `PerVertex` 路径要求：
    - 顶点阶段输出已计算光照项（或其压缩表达）
    - 片元阶段以插值结果为主，不重复完整逐像素 BRDF

每个材质只需提供：
- 光照参数结构（Material/Lighting Params）
- 资源依赖（required_resources）
- 光照函数入口（如 `EvalLighting()`）

生成器负责：
- VS/FS 与 Mesh/FS 主体骨架生成
- 输入读取策略切换（VBO vs SSBO）
- 输出目标写入策略（Forward RT vs GBuffer/VBuffer）
- Solid/Mask 分支（含 Alpha Cutoff）
- DepthOnlySolid/DepthOnlyMask 分支（ShadowMap / Early-Z）
- PostProcess 特殊通道组合分支（由 GBuffer 通道掩码驱动）
- Forward 顶点光照分支（PerVertex）
- Mobile Subpass GBuffer 分支（subpassLoad 输入附件读取）
- permutation 宏与变体产物打包

### 4.3 组件边界（建议）

```
ShaderGen Core (独立库)
    ├─ IR Layer
    │   ├─ MaterialSpec（参数 + 资源需求 + 函数签名）
    │   ├─ PipelineMode（Path/Coverage/Input/Topology/GBufferFormat/ForwardLighting）
    │   └─ PostProcessOutputMask（受 GBufferFormat 约束的通道组合）
    ├─ Validator
    │   ├─ 资源可解析性检查
    │   ├─ 模式兼容性检查
    │   └─ 约束检查（禁止超自由扩展）
    ├─ Composer
    │   ├─ Layout Composer
    │   ├─ Helper Composer
    │   ├─ Stage Composer (VS/FS + Mesh/FS)
    │   └─ Output Composer (Forward/GBuffer/VBuffer)
    └─ Packager
            ├─ Variant Key 生成
            ├─ GLSL/SPV 产物缓存
            └─ 诊断报告导出
```

### 4.4 依赖策略（独立程序 / dll / so）

- Core 数据结构以 STL 为主（`std::string`, `std::vector`, `std::unordered_map`）。
- 可选引入：
    - Abseil（状态、字符串、flat hash）
    - ankerl::unordered_dense（高性能 hash map）
- 对外 API 不暴露引擎内部类型，保持 ABI 边界清晰。

---

## 5. 设计好处总结

```
M0-M1：ComposedMaterialDef（硬编码）
  ├─ PureColor3D（已完成）
  ├─ BasicLit（待实现）
  └─ 特效材质（待实现）

M2-M4：ShaderTemplateEngine（模板驱动）
  ├─ 读取 cook_torrance.glsl 模板
  ├─ 读取 hemisphere.glsl 模板
  └─ 使用 Inja 生成 GLSL 片段
      ↓
      合并到 ComposedShaderGenerator 输出
```

关键点：
- **ComposedMaterialDef**：硬编码、编译期常量、零运行时开销
- **ShaderTemplateEngine**：文件驱动、动态加载、支持美术自定义

最终框架架构：
```
开发者定义材质
  ↓
选择方式 A（编译期硬编码）或方式 B（模板驱动）
  ↓
ComposedShaderGenerator.Compose()
  ├─ Case A: 直接使用 FixedMaterialDef 片段
  └─ Case B: 从 ShaderTemplateEngine 加载 GLSL 片段
  ↓
生成完整 GLSL → SPV 编译 → 运行时加载
```

---

## 5. 设计好处总结

| 方面 | 收益 |
|------|------|
| **可维护性** | 修改光照算法一次，18 个 shader 同时更新 |
| **开发效率** | 编写业务代码无需考虑框架细节，大幅降低出错率 |
| **受控灵活性** | 开发者只在参数/函数层表达意图，流程由生成器托管，避免系统失控 |
| **性能** | 编译期代码生成，零运行时 if-else，最终 SPV 和手写等效 |
| **复用性** | 同一份业务代码支持 Forward/Deferred/Alpha/Additive 多种输出 |
| **批处理一致性** | 固定流程分型可与“一 Drawcall 全场景”策略长期兼容 |
| **工程独立性** | 可独立构建为 dll/so/工具程序，降低与引擎主工程耦合 |

---

## 6. 实现优先级（建议）

### 当前状态（2026-02）
- [x] `ComposedShaderGenerator::ComposeVertexShader()` 已完成并通过回归
- [x] `ComposedShaderGenerator::ComposeFragmentShader()` 已完成并通过回归
- [x] `ResourceLayoutGenerator` 已接入合成流程（统一 layout 生成）
- [x] `BuiltinHelpers` 已支持字符串检测 + 显式依赖 + 逻辑依赖注入
- [x] `BuildComposedMaterialDefFromLogic()` 已实现：
    - 从 `MaterialLogicDef.vertex/fragment.required_resources` 过滤描述符
    - 聚合 `required_helpers` 到 `ComposedMaterialDef.logic_required_helpers`
    - 返回缺失资源诊断（`missing_resources`）
- [x] `test_ComposedShaderGenerator` 已覆盖逻辑桥接路径并通过

### Phase 1（M1-M2）：ComposedMaterialDef 框架
- [x] 完成 `ComposedShaderGenerator::ComposeVertexShader()`
- [x] 完成 `ComposedShaderGenerator::ComposeFragmentShader()`
- [x] 完成 BasicLit ComposedMaterialDef

### Phase 2（M3-M4）：固定流程模式化
- [ ] 引入 `PipelineMode`（Forward/GBuffer/VBuffer + Solid/Mask + VertexInput/SSBO + VS/FS/Mesh/FS）
- [ ] 将 `Compose*Shader` 重构为按模式分发的子流水线
- [ ] 建立模式兼容矩阵与静态校验（非法组合在生成期报错）

### Phase 2.5（M4-M5）：GPU-Driven 输入与拓扑自适应
- [ ] 定义输入路径策略：`PreferSSBO` / `ForceVertexInput` / `AutoByCapability`
- [ ] 定义拓扑策略：`PreferMeshShader` / `ForceVSFS` / `AutoByCapability`
- [ ] 建立 VS/FS 与 Mesh/FS 语义一致性回归（同材质同参数输出等价）

### Phase 3（M5-M6）：受控材质接口
- [ ] 定义受限材质接口（参数块 + 资源需求 + 光照函数签名）
- [ ] 移除/收紧自由拼接入口，统一走受控接口
- [ ] 完成 Forward/GBuffer/VBuffer 三路径对齐测试

### Phase 4（M7+）：独立化与产物化
- [ ] 生成器核心拆分为独立库（目标：dll/so + CLI）
- [ ] 数据结构迁移到 STL（按需引入 Abseil / ankerl）
- [ ] 输出变体清单、诊断报告、可复现实验脚本

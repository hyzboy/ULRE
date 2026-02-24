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

## 4. 与 ShaderTemplateEngine 的协作

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
| **灵活性** | Permutation 由参数驱动，支持新的光照模型无需改业务代码 |
| **性能** | 编译期代码生成，零运行时 if-else，最终 SPV 和手写等效 |
| **复用性** | 同一份业务代码支持 Forward/Deferred/Alpha/Additive 多种输出 |
| **可视化支持** | 美术可理解"开发者业务段"作为材质"核心"，框架负责"周边" |

---

## 6. 实现优先级（建议）

### Phase 1（M1-M2）：ComposedMaterialDef 框架
- [ ] 完成 `ComposedShaderGenerator::ComposeVertexShader()`
- [ ] 完成 `ComposedShaderGenerator::ComposeFragmentShader()`
- [ ] 完成 BasicLit ComposedMaterialDef

### Phase 2（M3-M4）：ShaderTemplateEngine 整合
- [ ] 实现 ShaderTemplateEngine 文件读取
- [ ] 实现 Inja 模板渲染
- [ ] 添加 cook_torrance.glsl、pbr_lite.glsl 等模板库

### Phase 3（M5+）：编辑器支持
- [ ] 材质编辑器显示"业务代码"vs"框架代码"分离
- [ ] 支持动态创建新的 ComposedMaterialDef（可视化编辑）

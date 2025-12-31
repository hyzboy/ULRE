# Shader Template Engine 设计文档

## 概述

Shader Template Engine 是一个基于模板引擎的着色器生成系统，旨在取代传统的 C++ 字符串拼接方式。它提供了模块化、声明式的方式来组织和生成 GLSL 着色器代码。

## 系统架构

### 核心组件

```
ShaderTemplateEngine (核心引擎)
    ├── inja::Environment (模板渲染引擎)
    ├── Module Cache (模块缓存)
    └── Interface Cache (接口定义缓存)

ShaderLibrary/ (资源目录)
    ├── templates/ (顶层模板)
    │   ├── forward.vert.tmpl
    │   └── forward.frag.tmpl
    ├── modules/ (GLSL模块库)
    │   ├── lighting/ (光照模型)
    │   ├── specular/ (高光模型)
    │   ├── ambient/ (环境光)
    │   ├── albedo/ (基础色)
    │   ├── normal/ (法线)
    │   └── utils/ (工具函数)
    └── recipes/ (材质配方)
        ├── standard/ (标准材质)
        └── organic/ (有机材质)
```

### 数据结构

#### ShaderModule (着色器模块)
```cpp
struct ShaderModule {
    AnsiString name;                    // 模块名称
    AnsiString file_path;               // 文件路径
    AnsiString code;                    // GLSL代码
    
    AnsiStringList provides;            // 提供的函数
    AnsiStringList requires_funcs;      // 需要的函数
    AnsiStringList requires_inputs;     // 需要的输入变量
    AnsiStringList requires_descriptors;// 需要的描述符
    AnsiStringList dependencies;        // 依赖的模块
};
```

#### ShaderRecipe (着色器配方)
```cpp
struct ShaderRecipe {
    AnsiString name;                              // 配方名称
    AnsiString template_file;                     // 模板文件
    Map<AnsiString, AnsiString> module_map;       // 模块映射
    json template_data;                           // 模板数据
};
```

## 工作流程

### 1. 模块加载流程

```
LoadModule(category, name)
    ↓
检查模块缓存
    ↓
加载接口定义 (category_interface.json)
    ↓
读取 GLSL 文件
    ↓
解析依赖信息
    ↓
缓存模块
    ↓
返回 ShaderModule*
```

### 2. 依赖解析流程

```
ResolveDependencies(recipe)
    ↓
收集所有需要的模块
    ↓
递归加载依赖
    ↓
构建依赖图
    ↓
拓扑排序 (Kahn算法)
    ↓
检测循环依赖
    ↓
返回排序后的模块列表
```

### 3. 代码生成流程

```
Generate(recipe)
    ↓
解析依赖树
    ↓
按依赖顺序收集模块代码
    ↓
构建模板数据
    ↓
加载模板文件
    ↓
inja 渲染模板
    ↓
返回最终 GLSL 代码
```

## 如何添加新的光照模型

### 步骤 1: 创建 GLSL 文件

在 `ShaderLibrary/modules/lighting/` 目录下创建新的 `.glsl` 文件，例如 `toon.glsl`:

```glsl
// Toon/Cel Shading Model
// Stylized cartoon-like lighting
// Interface: GetDiffuseColor()
// Requires: Input.Normal, sky
// Dependencies: GetAlbedo

vec3 GetDiffuseColor()
{
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(sky.sun_direction.xyz);
    
    float NdotL = dot(normal, lightDir);
    
    // Quantize lighting into steps
    float intensity = smoothstep(0.0, 0.1, NdotL) * 0.5 +
                     smoothstep(0.5, 0.6, NdotL) * 0.5;
    
    vec3 baseColor = GetAlbedo();
    vec3 lightColor = sky.sun_color.rgb * sky.sun_intensity;
    
    return baseColor * intensity * lightColor;
}
```

### 步骤 2: 更新接口定义

编辑 `ShaderLibrary/modules/lighting/lighting_interface.json`，添加新模块的定义:

```json
{
  "modules": {
    ...现有模块...,
    "toon": {
      "file": "toon.glsl",
      "provides": ["GetDiffuseColor"],
      "requires": {
        "functions": [],
        "inputs": ["Normal"],
        "descriptors": ["sky"]
      },
      "dependencies": ["GetAlbedo"]
    }
  }
}
```

### 步骤 3: 在配方中使用

在材质配方中引用新的光照模型:

```json
{
  "fragment_shader": {
    "modules": {
      "lighting": "toon",
      ...其他模块...
    }
  }
}
```

## 如何创建自定义材质配方

### 示例：创建玻璃材质

创建 `ShaderLibrary/recipes/standard/glass.json`:

```json
{
  "name": "Glass",
  "material_type": "Standard",
  "rendering_path": "Forward",
  "description": "Transparent glass material with reflections",
  
  "quality_levels": {
    "high": {
      "fragment_shader": {
        "template": "forward.frag.tmpl",
        "inputs": [
          {"type": "vec3", "name": "Normal"},
          {"type": "vec4", "name": "Position"},
          {"type": "vec2", "name": "TexCoord"}
        ],
        "outputs": [
          {"type": "vec4", "name": "FragColor"}
        ],
        "modules": {
          "lighting": "blinn_phong",
          "specular": "ggx",
          "ambient": "ibl",
          "albedo": "constant_color"
        },
        "frag_color_body": "vec3 color = GetSpecularColor() * 0.9 + GetAmbientColor(); return color;",
        "alpha_expression": "0.3"
      },
      "descriptors": {
        "ubos": [
          {
            "set": 0,
            "binding": 0,
            "struct_name": "CameraUBO",
            "name": "camera",
            "members": "    vec3 pos;\n    mat4 view;\n    mat4 proj;"
          },
          {
            "set": 0,
            "binding": 1,
            "struct_name": "SkyUBO",
            "name": "sky",
            "members": "    vec3 sun_direction;\n    vec3 sun_color;\n    float sun_intensity;"
          },
          {
            "set": 0,
            "binding": 2,
            "struct_name": "MaterialInstance",
            "name": "materialInstance",
            "members": "    vec4 baseColor;"
          }
        ],
        "samplers": [
          {"set": 1, "binding": 0, "type": "samplerCube", "name": "irradianceMap"},
          {"set": 1, "binding": 1, "type": "samplerCube", "name": "prefilterMap"},
          {"set": 1, "binding": 2, "type": "sampler2D", "name": "brdfLUT"}
        ]
      }
    }
  }
}
```

### 配方组成要素

1. **基本信息**
   - `name`: 材质名称
   - `material_type`: 材质类型
   - `rendering_path`: 渲染路径（Forward/Deferred）
   - `description`: 描述信息

2. **质量等级** (`quality_levels`)
   - `high`: 高质量（完整特性）
   - `medium`: 中等质量（平衡）
   - `low`: 低质量（性能优先）

3. **着色器配置**
   - `template`: 使用的模板文件
   - `inputs`/`outputs`: 输入输出变量
   - `modules`: 模块选择
   - `frag_color_body`: 颜色组合逻辑
   - `alpha_expression`: 透明度计算

4. **描述符配置**
   - `ubos`: Uniform Buffer Objects
   - `samplers`: 纹理采样器

## API 使用示例

### 基本使用

```cpp
#include<hgl/shadergen/ShaderTemplateEngine.h>

using namespace hgl::graph;

// 初始化引擎
ShaderTemplateEngine engine(
    AnsiString("/path/to/ShaderLibrary/templates"),
    AnsiString("/path/to/ShaderLibrary/modules")
);

// 加载配方
ShaderRecipe recipe = engine.LoadRecipe(
    AnsiString("/path/to/ShaderLibrary/recipes/standard/metal.json"),
    AnsiString("high")
);

// 生成着色器代码
AnsiString fragmentShaderCode = engine.Generate(recipe);

// 使用生成的代码
LOG_INFO("Generated shader:\n" + fragmentShaderCode);
```

### 手动构建配方

```cpp
// 创建自定义配方
ShaderRecipe customRecipe;
customRecipe.name = AnsiString("CustomMaterial");
customRecipe.template_file = AnsiString("forward.frag.tmpl");

// 设置模块映射
customRecipe.module_map.Add(AnsiString("lighting"), AnsiString("lambert"));
customRecipe.module_map.Add(AnsiString("specular"), AnsiString("phong"));
customRecipe.module_map.Add(AnsiString("ambient"), AnsiString("skylight"));
customRecipe.module_map.Add(AnsiString("albedo"), AnsiString("texture_albedo"));

// 设置模板数据
customRecipe.template_data["frag_color_body"] = 
    "return GetDiffuseColor() + GetSpecularColor() + GetAmbientColor();";
customRecipe.template_data["alpha_expression"] = "1.0";

// 生成代码
AnsiString shaderCode = engine.Generate(customRecipe);
```

### 加载和验证模块

```cpp
// 加载单个模块
ShaderModule* lambertModule = engine.LoadModule(
    AnsiString("lighting"),
    AnsiString("lambert")
);

if (lambertModule) {
    LOG_INFO("Module loaded: " + lambertModule->name);
    LOG_INFO("Provides: " + lambertModule->provides[0]);
}

// 解析依赖
AnsiStringList orderedModules;
AnsiStringList missingDeps;

bool success = engine.ResolveDependencies(
    recipe,
    orderedModules,
    missingDeps
);

if (!success) {
    LOG_ERROR("Dependency resolution failed!");
    for (int i = 0; i < missingDeps.GetCount(); ++i) {
        LOG_ERROR("Missing: " + missingDeps[i]);
    }
}
```

## 与现有 MaterialCreateInfo 的集成

### 集成策略

ShaderTemplateEngine 可以与现有的 `MaterialCreateInfo` 系统协同工作：

```cpp
class MaterialCreateInfo
{
    // 现有成员...
    
    // 新增：模板引擎支持
    ShaderTemplateEngine* templateEngine;
    
public:
    // 使用模板引擎生成着色器
    bool CreateShaderFromTemplate(const AnsiString& recipePath, 
                                  const AnsiString& qualityLevel)
    {
        if (!templateEngine) {
            templateEngine = new ShaderTemplateEngine(
                GetTemplateRoot(),
                GetModuleRoot()
            );
        }
        
        ShaderRecipe recipe = templateEngine->LoadRecipe(
            recipePath, 
            qualityLevel
        );
        
        AnsiString fragmentCode = templateEngine->Generate(recipe);
        
        // 将生成的代码设置到现有系统
        if (frag) {
            frag->SetMain(fragmentCode);
            return frag->CreateShader(nullptr);
        }
        
        return false;
    }
    
    // 保持向后兼容
    bool CreateShader()
    {
        // 现有的字符串拼接方式仍然可用
        // ...原有代码...
    }
};
```

### 迁移路径

1. **阶段 1**：并行运行
   - 保留现有的字符串拼接代码
   - 添加模板引擎作为可选功能
   - 通过配置选择使用哪种方式

2. **阶段 2**：逐步迁移
   - 将新材质使用模板引擎实现
   - 逐个迁移现有材质到配方系统
   - 维护两种系统的功能对等

3. **阶段 3**：完全切换
   - 所有材质使用模板引擎
   - 移除旧的字符串拼接代码
   - 优化和简化 MaterialCreateInfo

### 配置示例

```cpp
// 在 MaterialCreateConfig 中添加选项
struct MaterialCreateConfig
{
    // 现有成员...
    
    bool use_template_engine;        // 是否使用模板引擎
    AnsiString recipe_path;          // 配方路径
    AnsiString quality_level;        // 质量等级
    
    MaterialCreateConfig()
    {
        use_template_engine = false;  // 默认使用旧系统
        quality_level = AnsiString("medium");
    }
};

// 使用
MaterialCreateConfig config;
config.use_template_engine = true;
config.recipe_path = AnsiString("recipes/standard/metal.json");
config.quality_level = AnsiString("high");

MaterialCreateInfo* material = new MaterialCreateInfo(&config);
```

## 性能考虑

### 缓存机制

1. **模块缓存**: 加载过的模块保存在内存中，避免重复磁盘 I/O
2. **接口缓存**: 接口定义文件只加载一次
3. **模板缓存**: inja 内部缓存已解析的模板

### 优化建议

1. **预加载**: 在启动时预加载常用模块
2. **异步加载**: 在后台线程加载和编译着色器
3. **增量更新**: 只在模块更改时重新生成

## 调试和错误处理

### 日志输出

系统使用 `LOG_ERROR`, `LOG_WARNING`, `LOG_INFO` 进行日志输出：

```cpp
// 模块加载失败
LOG_ERROR("Failed to load module file: " + file_path);

// 依赖解析警告
LOG_WARNING("Dependency function not found: " + dep_func);

// 循环依赖检测
LOG_ERROR("Circular dependency detected");

// 模板渲染失败
LOG_ERROR("Template rendering failed: " + AnsiString(e.what()));
```

### 常见问题排查

1. **模块加载失败**
   - 检查文件路径是否正确
   - 验证 JSON 格式是否有效
   - 确认文件权限

2. **依赖解析失败**
   - 检查 `provides` 和 `dependencies` 是否匹配
   - 查看是否存在循环依赖
   - 验证所有依赖的模块都已定义

3. **模板渲染失败**
   - 检查模板语法是否正确
   - 验证传递给模板的数据结构
   - 查看是否有拼写错误

## 扩展性

### 添加新的模块类型

1. 在 `ShaderLibrary/modules/` 下创建新目录
2. 创建 GLSL 文件和接口定义
3. 在配方中使用新的模块类型

### 自定义模板

可以创建新的模板文件以支持不同的渲染管线：

```glsl
// deferred.frag.tmpl - 延迟渲染模板
#version 450

layout(location=0) out vec4 GBuffer0; // Albedo + Roughness
layout(location=1) out vec4 GBuffer1; // Normal + Metallic
layout(location=2) out vec4 GBuffer2; // Emissive + AO

// ... 模块函数 ...

void main()
{
    GBuffer0 = vec4(GetAlbedo(), GetRoughness());
    GBuffer1 = vec4(GetNormal() * 0.5 + 0.5, GetMetallic());
    GBuffer2 = vec4(GetEmissive(), GetAO());
}
```

## 总结

Shader Template Engine 提供了一个灵活、可扩展的着色器生成系统。通过模块化设计和声明式配置，它使得着色器开发更加高效和易于维护。同时，它与现有系统保持兼容，允许平滑迁移。

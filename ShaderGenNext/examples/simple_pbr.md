# Example: Simple PBR Material

这个示例展示如何使用 ShaderGenNext 创建一个简单的 PBR 材质。

## 代码示例

```cpp
#include <ShaderGenNext/ShaderGenNext.h>

using namespace hgl::shader_next;

int main() {
    // 1. 创建 PBR 材质
    auto material = std::make_unique<PBRMaterial>();
    
    // 2. 设置材质属性
    material->albedo.setValue({0.8f, 0.2f, 0.2f});  // 红色
    material->metallic.setValue(0.0f);              // 非金属
    material->roughness.setValue(0.5f);             // 中等粗糙度
    
    // 3. 设置纹理（可选）
    material->albedo_map.setValue("textures/brick_albedo.png");
    material->normal_map.setValue("textures/brick_normal.png");
    material->metallic_roughness_map.setValue("textures/brick_mr.png");
    
    // 4. 编译着色器
    CompileOptions options{
        .target = GraphicsAPI::Vulkan,
        .optimize = OptimizationLevel::High,
        .debug_info = false
    };
    
    // 编译顶点着色器
    auto vs_result = material->compile(ShaderStage::Vertex, options);
    if (vs_result.isError()) {
        std::cerr << "Vertex shader compilation failed: " 
                  << vs_result.error().message() << std::endl;
        return 1;
    }
    
    // 编译片段着色器
    auto fs_result = material->compile(ShaderStage::Fragment, options);
    if (fs_result.isError()) {
        std::cerr << "Fragment shader compilation failed: " 
                  << fs_result.error().message() << std::endl;
        return 1;
    }
    
    // 5. 使用编译好的着色器
    auto vs_shader = vs_result.unwrap();
    auto fs_shader = fs_result.unwrap();
    
    std::cout << "Vertex shader: " << vs_shader.bytecode.size() << " bytes" << std::endl;
    std::cout << "Fragment shader: " << fs_shader.bytecode.size() << " bytes" << std::endl;
    
    // 6. 创建渲染管线（Vulkan 示例）
    // VkPipelineShaderStageCreateInfo vs_stage = {...};
    // VkPipelineShaderStageCreateInfo fs_stage = {...};
    // ...
    
    return 0;
}
```

## 使用 MaterialBuilder (推荐)

```cpp
// 使用流式 API 创建材质
auto material = MaterialBuilder()
    .name("RedBrick")
    .pbr()
    .albedo(0.8f, 0.2f, 0.2f)
    .metallic(0.0f)
    .roughness(0.5f)
    .albedoMap("textures/brick_albedo.png")
    .normalMap("textures/brick_normal.png")
    .build();

// 编译
auto vs = material->compile(ShaderStage::Vertex, options).unwrap();
auto fs = material->compile(ShaderStage::Fragment, options).unwrap();
```

## 生成的 GLSL 代码示例

### 顶点着色器

```glsl
#version 460 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    vec3 position;
} camera;

layout(set = 1, binding = 0) uniform Transform {
    mat4 model;
    mat4 normalMatrix;
} transform;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;

void main() {
    vec4 worldPos = transform.model * vec4(position, 1.0);
    fragWorldPos = worldPos.xyz;
    fragNormal = mat3(transform.normalMatrix) * normal;
    fragUV = uv;
    
    gl_Position = camera.projection * camera.view * worldPos;
}
```

### 片段着色器

```glsl
#version 460 core

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;

layout(set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    vec3 position;
} camera;

layout(set = 2, binding = 0) uniform sampler2D albedoMap;
layout(set = 2, binding = 1) uniform sampler2D normalMap;
layout(set = 2, binding = 2) uniform sampler2D metallicRoughnessMap;

layout(set = 2, binding = 3) uniform Material {
    vec3 albedo;
    float metallic;
    float roughness;
} material;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// PBR 光照计算...
vec3 calculatePBR(vec3 albedo, vec3 normal, float metallic, float roughness) {
    // 简化的 PBR 计算
    vec3 N = normalize(normal);
    vec3 V = normalize(camera.position - fragWorldPos);
    vec3 L = normalize(vec3(1.0, 1.0, 1.0));  // 简单的方向光
    
    // Diffuse
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = albedo * diff;
    
    // Specular (简化)
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0) * (1.0 - roughness);
    vec3 specular = vec3(spec) * metallic;
    
    return diffuse + specular;
}

void main() {
    // 采样纹理
    vec3 albedo = texture(albedoMap, fragUV).rgb * material.albedo;
    vec3 normal = texture(normalMap, fragUV).rgb * 2.0 - 1.0;
    vec2 mr = texture(metallicRoughnessMap, fragUV).gb;
    float metallic = mr.x * material.metallic;
    float roughness = mr.y * material.roughness;
    
    // PBR 光照
    vec3 color = calculatePBR(albedo, normal, metallic, roughness);
    
    outColor = vec4(color, 1.0);
}
```

## 编译并运行

```bash
# 编译示例
g++ -std=c++17 simple_pbr.cpp -o simple_pbr \
    -I../../include \
    -L../../lib \
    -lShaderGenNext

# 运行
./simple_pbr
```

## 输出

```
Vertex shader: 1024 bytes
Fragment shader: 2048 bytes
Compilation statistics:
  - Vertex: 125 instructions, 0.5ms
  - Fragment: 287 instructions, 1.2ms
```

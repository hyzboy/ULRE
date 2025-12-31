# ShaderGenNext - 现代化着色器生成框架

## 概述

ShaderGenNext 是 ULRE 引擎的下一代着色器生成和管理系统，采用现代化设计理念，提供高性能、易用性和可扩展性。

## 设计目标

### 核心原则
1. **性能优先** - 内置缓存、批量编译、异步加载
2. **类型安全** - 强类型系统，编译期检查
3. **易于使用** - 声明式 API，链式调用，智能默认值
4. **可扩展** - 插件架构，支持自定义着色器节点
5. **跨平台** - 支持 Vulkan、OpenGL、DirectX、Metal
6. **现代特性** - PBR、IBL、Compute Shader、Ray Tracing

### 与旧系统的主要区别

| 特性 | ShaderGen (旧) | ShaderGenNext (新) |
|------|----------------|-------------------|
| **API 风格** | 命令式，手动管理 | 声明式，自动管理 |
| **类型安全** | 运行时检查 | 编译期 + 运行时 |
| **缓存机制** | 无 | 多层缓存（内存/磁盘/网络） |
| **错误处理** | 布尔返回值 | Result<T, Error> 模式 |
| **材质定义** | 文本解析 | DSL + 二进制 + 可视化 |
| **图形 API** | Vulkan 专用 | 多后端抽象 |
| **并发支持** | 单线程 | 异步编译，并行加载 |
| **热重载** | 不支持 | 内置支持 |
| **调试工具** | 基础日志 | 可视化调试器，性能分析 |

## 架构设计

### 模块划分

```
ShaderGenNext/
├── core/                   # 核心框架
│   ├── ShaderGraph.h      # 着色器图系统
│   ├── ShaderNode.h       # 节点基类
│   ├── ShaderContext.h    # 编译上下文
│   ├── TypeSystem.h       # 类型系统
│   └── Result.h           # 错误处理
│
├── compiler/               # 编译器后端
│   ├── CompilerBackend.h  # 后端抽象接口
│   ├── SPIRVBackend.h     # SPIR-V 编译器
│   ├── GLSLBackend.h      # GLSL 转译器
│   ├── HLSLBackend.h      # HLSL 转译器
│   └── MetalBackend.h     # Metal 转译器
│
├── cache/                  # 缓存系统
│   ├── ShaderCache.h      # 着色器缓存
│   ├── CachePolicy.h      # 缓存策略
│   └── CacheStorage.h     # 存储后端
│
├── materials/              # 材质系统
│   ├── Material.h         # 材质基类
│   ├── PBRMaterial.h      # PBR 材质
│   ├── UnlitMaterial.h    # 无光照材质
│   └── CustomMaterial.h   # 自定义材质
│
├── pipeline/               # 渲染管线
│   ├── RenderPipeline.h   # 管线定义
│   ├── PassGraph.h        # 渲染通道图
│   └── ResourceBinding.h  # 资源绑定
│
└── tools/                  # 开发工具
    ├── ShaderEditor.h     # 可视化编辑器
    ├── Debugger.h         # 调试器
    └── Profiler.h         # 性能分析器
```

## 快速开始

### 示例 1：创建简单材质

```cpp
#include <ShaderGenNext/ShaderGenNext.h>

using namespace hgl::shader_next;

// 声明式 API - 链式调用
auto material = MaterialBuilder()
    .name("SimplePBR")
    .shader(ShaderStage::Vertex)
        .input("vec3", "position")
        .input("vec3", "normal")
        .input("vec2", "uv")
        .output("vec3", "worldPos")
        .output("vec3", "worldNormal")
        .output("vec2", "texCoord")
        .main([](ShaderContext& ctx) {
            ctx.worldPos = ctx.transform(ctx.position);
            ctx.worldNormal = ctx.transformNormal(ctx.normal);
            ctx.texCoord = ctx.uv;
            ctx.glPosition = ctx.project(ctx.worldPos);
        })
    .shader(ShaderStage::Fragment)
        .texture2D("albedoMap")
        .texture2D("normalMap")
        .texture2D("metallicRoughnessMap")
        .uniform("vec3", "cameraPos")
        .output("vec4", "color")
        .main([](ShaderContext& ctx) {
            auto albedo = ctx.sample(ctx.albedoMap, ctx.texCoord);
            auto normal = ctx.sampleNormal(ctx.normalMap, ctx.texCoord);
            auto mr = ctx.sample(ctx.metallicRoughnessMap, ctx.texCoord);
            
            ctx.color = ctx.calculatePBR(
                albedo.rgb(),
                normal,
                mr.g(), // metallic
                mr.b(), // roughness
                ctx.worldPos,
                ctx.cameraPos
            );
        })
    .build();
```

### 示例 2：使用着色器图

```cpp
// 基于节点的图形化编程
auto graph = ShaderGraph::create("PBRGraph");

// 添加节点
auto albedoTex = graph->addNode<TextureSampleNode>("albedoMap");
auto normalTex = graph->addNode<TextureSampleNode>("normalMap");
auto mrTex = graph->addNode<TextureSampleNode>("metallicRoughnessMap");
auto pbrNode = graph->addNode<PBRLightingNode>();
auto output = graph->addNode<OutputNode>();

// 连接节点
graph->connect(albedoTex->output("rgb"), pbrNode->input("albedo"));
graph->connect(normalTex->output("rgb"), pbrNode->input("normal"));
graph->connect(mrTex->output("g"), pbrNode->input("metallic"));
graph->connect(mrTex->output("b"), pbrNode->input("roughness"));
graph->connect(pbrNode->output("color"), output->input("fragColor"));

// 编译
auto result = graph->compile(CompileOptions{
    .target = GraphicsAPI::Vulkan,
    .optimize = OptimizationLevel::High,
    .debug = false
});

if (result.isOk()) {
    auto shader = result.unwrap();
    // 使用编译好的着色器
}
```

### 示例 3：异步编译与缓存

```cpp
// 异步编译多个着色器
auto compiler = ShaderCompiler::create();

// 配置缓存
compiler->setCache(ShaderCache::create(CacheOptions{
    .memory_cache = true,
    .disk_cache = true,
    .cache_dir = ".shader_cache",
    .max_memory_size = 256_MB,
    .compression = CompressionType::LZ4
}));

// 批量异步编译
std::vector<Future<CompiledShader>> futures;

for (auto& material : materials) {
    futures.push_back(compiler->compileAsync(material));
}

// 等待所有编译完成
auto results = Future::waitAll(futures);

for (auto& result : results) {
    if (result.isOk()) {
        auto shader = result.unwrap();
        pipeline->addShader(shader);
    } else {
        LogError("Compilation failed: {}", result.error());
    }
}
```

## 核心特性

### 1. 智能缓存系统

```cpp
// 多层缓存架构
class ShaderCache {
public:
    // L1: 内存缓存 (最快)
    // L2: 磁盘缓存 (快速)
    // L3: 网络缓存 (可选，用于团队协作)
    
    Result<CompiledShader> getOrCompile(
        const ShaderSource& source,
        const CompileOptions& options
    );
    
    // 预热缓存
    void preheat(const std::vector<ShaderID>& shaders);
    
    // 缓存统计
    CacheStats getStats() const;
    
    // 清理策略
    void cleanup(CleanupPolicy policy);
};
```

### 2. 类型安全的着色器变量

```cpp
// 强类型的着色器变量
template<typename T>
class ShaderVariable {
    std::string name;
    ShaderType type;
    T default_value;
    
public:
    // 类型检查在编译期完成
    static_assert(IsShaderType<T>::value, 
                  "T must be a valid shader type");
    
    // 自动类型转换
    operator ShaderValue() const;
};

// 使用示例
ShaderVariable<vec3> lightDirection{"lightDir", vec3(0, -1, 0)};
ShaderVariable<float> roughness{"roughness", 0.5f};
ShaderVariable<Texture2D> albedoMap{"albedo"};
```

### 3. 现代错误处理

```cpp
// Result<T, E> 模式，避免异常
template<typename T, typename E = Error>
class Result {
public:
    bool isOk() const;
    bool isError() const;
    
    T unwrap();  // 获取值，错误时 panic
    T unwrapOr(T default_value);  // 获取值或默认值
    E error() const;  // 获取错误
    
    // 链式操作
    template<typename F>
    auto map(F&& func) -> Result<...>;
    
    template<typename F>
    auto andThen(F&& func) -> Result<...>;
};

// 使用示例
auto result = compiler->compile(source);

result
    .map([](auto shader) {
        return optimizer.optimize(shader);
    })
    .andThen([](auto optimized) {
        return cache.store(optimized);
    })
    .unwrapOr(fallbackShader);
```

### 4. 热重载支持

```cpp
// 文件监视和自动重载
class ShaderHotReload {
public:
    // 监视着色器文件变化
    void watch(const Path& directory);
    
    // 注册重载回调
    void onReload(std::function<void(ShaderID, CompiledShader)> callback);
    
    // 触发重新编译
    void triggerRecompile(ShaderID id);
};

// 使用示例
auto hotReload = ShaderHotReload::create();
hotReload->watch("shaders/");
hotReload->onReload([&](ShaderID id, CompiledShader shader) {
    material->updateShader(id, shader);
    LogInfo("Shader {} reloaded", id);
});
```

### 5. 跨平台抽象

```cpp
// 统一的图形 API 抽象
enum class GraphicsAPI {
    Vulkan,
    OpenGL_4_6,
    DirectX12,
    Metal,
    WebGPU
};

// 编译选项
struct CompileOptions {
    GraphicsAPI target;
    OptimizationLevel optimize;
    bool debug_info;
    FeatureSet features;
};

// 自动选择最佳后端
auto backend = CompilerBackend::select(GraphicsAPI::Vulkan);

// 或手动指定
auto spvBackend = std::make_unique<SPIRVBackend>();
compiler->setBackend(std::move(spvBackend));
```

### 6. 内置 PBR 支持

```cpp
// 完整的 PBR 材质系统
class PBRMaterial : public Material {
public:
    // 物理参数
    Property<Color> albedo;
    Property<float> metallic;
    Property<float> roughness;
    Property<float> ao;
    
    // 纹理
    Property<Texture2D> albedoMap;
    Property<Texture2D> normalMap;
    Property<Texture2D> metallicRoughnessMap;
    Property<Texture2D> aoMap;
    Property<Texture2D> emissiveMap;
    
    // IBL
    Property<TextureCube> irradianceMap;
    Property<TextureCube> prefilterMap;
    Property<Texture2D> brdfLUT;
    
    // 自动生成着色器
    CompiledShader compile() override;
};
```

### 7. 性能分析与调试

```cpp
// 内置性能分析
class ShaderProfiler {
public:
    // 记录编译时间
    void recordCompileTime(ShaderID id, Duration time);
    
    // 记录运行时性能
    void recordRenderTime(ShaderID id, Duration time);
    
    // 生成报告
    ProfileReport generateReport();
    
    // 热点分析
    std::vector<ShaderID> findHotspots();
};

// 调试工具
class ShaderDebugger {
public:
    // 导出中间代码
    void exportIR(ShaderID id, Path output);
    
    // 反汇编
    std::string disassemble(CompiledShader shader);
    
    // 变量追踪
    void traceVariable(const std::string& name);
    
    // 断点支持
    void setBreakpoint(ShaderID id, int line);
};
```

## 材质定义语言 (SDL)

### DSL 语法

```sdl
// Shader Definition Language (.sdl)
material PBRMaterial {
    name: "Standard PBR"
    version: "1.0"
    
    // 属性定义
    properties {
        albedo: color = #FFFFFF
        metallic: float = 0.0 range(0, 1)
        roughness: float = 0.5 range(0, 1)
        
        albedoMap: texture2D
        normalMap: texture2D
        metallicRoughnessMap: texture2D
    }
    
    // 顶点着色器
    vertex {
        input {
            vec3 position
            vec3 normal
            vec2 uv
        }
        
        output {
            vec3 worldPos
            vec3 worldNormal
            vec2 texCoord
        }
        
        code {
            worldPos = (model * vec4(position, 1.0)).xyz;
            worldNormal = normalize(normalMatrix * normal);
            texCoord = uv;
            gl_Position = projection * view * vec4(worldPos, 1.0);
        }
    }
    
    // 片段着色器
    fragment {
        output {
            vec4 fragColor
        }
        
        code {
            vec3 albedo = texture(albedoMap, texCoord).rgb * properties.albedo;
            vec3 normal = texture(normalMap, texCoord).rgb * 2.0 - 1.0;
            float metallic = texture(metallicRoughnessMap, texCoord).b * properties.metallic;
            float roughness = texture(metallicRoughnessMap, texCoord).g * properties.roughness;
            
            fragColor = calculatePBR(albedo, normal, metallic, roughness, worldPos);
        }
    }
}
```

### 二进制格式

```cpp
// 编译后的二进制格式 (.sbc - Shader Binary Cache)
struct ShaderBinaryFormat {
    uint32_t magic;           // 'SGEN'
    uint32_t version;         // 格式版本
    uint64_t hash;            // 源码哈希
    uint32_t api;             // GraphicsAPI
    uint32_t stage;           // ShaderStage
    uint64_t compile_time;    // 编译时间戳
    
    // 元数据
    MetadataBlock metadata;   // 名称、作者等
    
    // 代码段
    CodeSection code;         // SPIR-V/DXBC/MSL
    
    // 反射信息
    ReflectionData reflection; // 输入输出、UBO、采样器等
    
    // 调试信息 (可选)
    DebugInfo debug;          // 行号映射、变量名等
};
```

## 实现优先级

### Phase 1: 核心框架 (2-3 周)
- [ ] 类型系统和 Result<T, E>
- [ ] ShaderGraph 基础框架
- [ ] ShaderNode 基类和常用节点
- [ ] CompilerBackend 接口
- [ ] SPIRVBackend 实现

### Phase 2: 缓存与编译 (2-3 周)
- [ ] ShaderCache 多层缓存
- [ ] 异步编译支持
- [ ] 批量编译优化
- [ ] 缓存策略和清理

### Phase 3: 材质系统 (2-3 周)
- [ ] Material 基类
- [ ] PBRMaterial 实现
- [ ] SDL 解析器
- [ ] 二进制格式读写

### Phase 4: 工具与调试 (2-3 周)
- [ ] 热重载支持
- [ ] ShaderProfiler
- [ ] ShaderDebugger
- [ ] 可视化编辑器原型

### Phase 5: 多后端支持 (3-4 周)
- [ ] GLSLBackend
- [ ] HLSLBackend
- [ ] MetalBackend
- [ ] 后端自动选择

### Phase 6: 高级特性 (持续)
- [ ] Compute Shader 支持
- [ ] Ray Tracing 支持
- [ ] Mesh Shader 支持
- [ ] 自动优化器

## 性能目标

| 指标 | 目标 | 当前 ShaderGen |
|------|------|----------------|
| 首次编译时间 | < 100ms | ~500ms |
| 缓存命中加载 | < 5ms | N/A |
| 内存占用 | < 50MB (1000 个着色器) | ~200MB |
| 热重载延迟 | < 50ms | N/A |
| 并发编译 | 8+ 线程 | 1 线程 |

## API 参考

详细 API 文档请参考：
- [Core API](docs/api/core.md)
- [Compiler API](docs/api/compiler.md)
- [Material API](docs/api/materials.md)
- [Cache API](docs/api/cache.md)

## 示例项目

完整示例代码在 `examples/` 目录下：
- [simple_pbr](examples/simple_pbr/) - 简单 PBR 材质
- [shader_graph](examples/shader_graph/) - 着色器图系统
- [hot_reload](examples/hot_reload/) - 热重载示例
- [compute_shader](examples/compute_shader/) - 计算着色器

## 迁移指南

从 ShaderGen 迁移到 ShaderGenNext 的指南：
- [Migration Guide](docs/migration.md)
- [Breaking Changes](docs/breaking_changes.md)
- [Compatibility Layer](docs/compatibility.md)

## 贡献指南

欢迎贡献代码！请参考：
- [Contributing Guide](CONTRIBUTING.md)
- [Code Style](docs/code_style.md)
- [Testing Guide](docs/testing.md)

## 许可证

与 ULRE 项目保持一致的许可证。

---

**注意**：这是设计文档和框架，尚未完整实现。实现将分阶段进行。

# GLTF Model Loader Example

这个范例演示了如何将GLTF模型加载并映射到ULRE引擎的场景系统中。

## 功能概述

本范例实现了基础的GLTF模型加载功能，包括：

- **场景层次结构映射**：每个GLTF节点对应一个引擎SceneNode
- **网格模型加载**：每个GLTF primitive创建独立的Primitive/Mesh
- **层次结构保持**：完整保持GLTF文件中的节点层次关系

## 架构设计

### 核心映射关系

```
GLTF节点 → 引擎SceneNode
├── GLTF Mesh → 特殊SceneNode (容器节点)
│   ├── GLTF Primitive 1 → 引擎Primitive + Mesh + MeshComponent
│   ├── GLTF Primitive 2 → 引擎Primitive + Mesh + MeshComponent
│   └── ...
└── 子节点递归处理
```

### 关键类设计

1. **GLTFDocument**: 存储完整的GLTF文件结构
2. **GLTFLoader**: 负责加载和转换GLTF数据到引擎对象
3. **SimpleJSONParser**: 简化的JSON解析器（当前使用硬编码测试数据）

## 当前实现状态

### ✅ 已实现功能
- [x] 基础GLTF数据结构定义
- [x] 场景层次结构映射
- [x] 顶点位置数据提取
- [x] Primitive/Mesh创建
- [x] SceneNode层次结构构建
- [x] 基础材质支持

### 🚧 未来扩展计划

#### 材质系统
```cpp
// 预留的材质结构
struct GLTFMaterial {
    struct PBRMetallicRoughness {
        std::vector<float> baseColorFactor;
        float metallicFactor;
        float roughnessFactor;
        uint32_t baseColorTexture;          // 待实现
        uint32_t metallicRoughnessTexture;  // 待实现
    } pbrMetallicRoughness;
    
    uint32_t normalTexture;     // 法线贴图
    uint32_t occlusionTexture;  // 遮挡贴图
    uint32_t emissiveTexture;   // 自发光贴图
    // ...
};
```

#### 纹理系统
```cpp
struct GLTFTexture {
    uint32_t sampler;
    uint32_t source;  // 图像索引
};

struct GLTFImage {
    std::string uri;
    std::string mimeType;
    uint32_t bufferView;
};
```

#### 动画系统
```cpp
struct GLTFAnimation {
    std::vector<GLTFAnimationChannel> channels;
    std::vector<GLTFAnimationSampler> samplers;
};

struct GLTFSkin {
    uint32_t inverseBindMatrices;
    std::vector<uint32_t> joints;
    uint32_t skeleton;
};
```

#### 顶点属性扩展
```cpp
// 当前只支持POSITION，未来可扩展：
primitive.attributes["NORMAL"];      // 法线数据
primitive.attributes["TEXCOORD_0"];  // UV坐标
primitive.attributes["COLOR_0"];     // 顶点颜色
primitive.attributes["JOINTS_0"];    // 骨骼索引
primitive.attributes["WEIGHTS_0"];   // 骨骼权重
```

## 使用方法

### 编译运行
```bash
# 在项目根目录执行
mkdir build && cd build
cmake ..
make GLTFLoaderTest  # 编译GLTF加载器示例
./GLTFLoaderTest     # 运行示例
```

### 代码集成
```cpp
class MyApp : public WorkObject {
    GLTFLoader* loader;
    
    bool Init() override {
        loader = new GLTFLoader(this);
        
        // 初始化渲染资源
        if (!loader->InitializeRenderResources()) {
            return false;
        }
        
        // 加载GLTF文件
        if (!loader->LoadFromFile("my_model.gltf")) {
            return false;
        }
        
        // 构建场景
        if (!loader->BuildSceneHierarchy(GetScene(), GetSceneRoot())) {
            return false;
        }
        
        return true;
    }
};
```

## 测试文件

项目包含两个测试GLTF文件：

1. **triangle.gltf**: 简单三角形，演示基础功能
2. **complex_scene.gltf**: 复杂场景，演示层次结构

## 开发注意事项

### JSON解析
当前使用简化的硬编码测试数据，实际项目中建议：
- 使用jsoncpp或类似JSON库进行完整解析
- 支持Base64数据解码
- 处理外部文件引用（.bin, 纹理文件等）

### 内存管理
- SceneNode由GLTFLoader管理生命周期
- Primitive/Mesh由引擎的资源管理器管理
- 注意避免内存泄漏

### 性能优化
- 大型模型考虑分块加载
- 纹理资源缓存
- LOD支持

## 扩展指南

### 添加新顶点属性
1. 在`CreatePrimitiveFromGLTF`中检测新属性
2. 更新VILConfig配置
3. 添加到VertexAttribData列表

### 实现材质支持
1. 解析GLTFMaterial数据
2. 创建对应的引擎Material/MaterialInstance
3. 为每个primitive设置正确材质

### 添加动画支持
1. 解析GLTFAnimation数据
2. 创建AnimationComponent
3. 实现关键帧插值

## 相关文档

- [GLTF 2.0 规范](https://github.com/KhronosGroup/glTF/tree/master/specification/2.0)
- [ULRE引擎SceneNode文档](../inc/hgl/graph/SceneNode.h)
- [ULRE引擎Mesh系统文档](../inc/hgl/graph/Mesh.h)
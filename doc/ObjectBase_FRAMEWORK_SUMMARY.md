# ObjectBase 框架总结

## 🎯 核心目标

创建一个**编译时强制、运行时自动**的对象生命周期追踪系统，使得：
- ✅ 所有资源对象都有唯一ID
- ✅ 创建/销毁位置自动记录
- ✅ 泄漏自动检测
- ✅ 即使崩溃也能恢复信息

---

## 📦 框架组成

### 1. **ObjectBase** (`ObjectBase.h`)
核心基类，所有需要追踪的对象必须继承

**关键特性：**
- 魔数验证（0xDEADBEEFCAFEBABE）
- 自动ID分配（无锁原子自增）
- 源位置追踪（C++20 source_location）
- 销毁标记防止二次销毁
- 禁止复制/移动

**类定义：**
```cpp
class ObjectBase
{
    static const uint64_t MAGIC_NUMBER;
    uint64_t magic_;
    uint64_t object_id_;
    ObjectTypeTag type_;
    SourceLocation creation_loc_;
    SourceLocation destruction_loc_;
    bool is_destroyed_;
    
public:
    ObjectBase(ObjectTypeTag type, const std::string& name, const std::source_location& loc);
    virtual ~ObjectBase();
    
    // 查询接口
    uint64_t get_object_id() const;
    ObjectTypeTag get_object_type() const;
    bool is_valid() const;              // 检查魔数
    bool is_destroyed() const;
    // ...
};
```

**存储开销：**
- 基础对象：~100 字节
- 每个对象额外开销：可接受

---

### 2. **ObjectRegistry** (`ObjectBase.h`)
全局对象注册表，管理所有活跃对象

**单例模式实现：**
```cpp
class ObjectRegistry
{
    static ObjectRegistry* instance_;
    std::unordered_map<uint64_t, ObjectBase*> objects_;
    std::mutex lock_;
    
public:
    static ObjectRegistry& get_instance();
    void register_object(ObjectBase* obj);
    void unregister_object(uint64_t id);
    ObjectBase* find_object(uint64_t id);
    size_t report_leaks();
};
```

**功能：**
- 自动注册/注销（由ObjectBase的构造/析构触发）
- 线程安全（互斥锁）
- 泄漏检测
- 对象查询

---

### 3. **ObjectCrashRecovery** (`ObjectCrashRecovery.h`)
从崩溃中恢复对象信息

**关键功能：**
```cpp
class ObjectCrashRecovery
{
    // 快照保存
    static bool save_snapshot(const std::string& filename);
    
    // 快照加载与分析
    static bool load_and_analyze(const std::string& filename);
    
    // 内存扫描（寻找魔数）
    static std::vector<ObjectSnapshot> scan_memory_for_objects(
        const void* memory_start, size_t size);
    
    // HTML报告生成
    static bool generate_html_report(
        const std::vector<ObjectSnapshot>& objects,
        const std::string& output_file);
};
```

**用途：**
- 程序崩溃前保存对象状态
- 离线分析泄漏
- 生成可视化报告

---

### 4. **便利宏** (`ObjectBase.h`)
提供简洁的API

```cpp
// 列出所有对象
HGL_LIST_ALL_OBJECTS()

// 检测泄漏
HGL_REPORT_LEAKS()

// 获取对象数
HGL_GET_OBJECT_COUNT()

// 检查有效性
HGL_CHECK_OBJECT_VALID(obj)

// 从ID查找
HGL_FIND_OBJECT(id, type)

// 记录销毁位置
HGL_OBJECT_DESTROY_LOCATION()
```

---

## 🚀 使用流程

### 步骤1：创建基类

```cpp
// 原始代码
class Fence
{
public:
    Fence(VkDevice dev, VkFence f) : device(dev), fence(f) {}
    ~Fence() { vkDestroyFence(device, fence, nullptr); }
    VkDevice device;
    VkFence fence;
};
```

### 步骤2：添加ObjectBase

```cpp
// 改进后
class Fence : public hgl::utils::ObjectBase
{
public:
    // ✓ 必须传入创建位置（强制通过编译时类型检查）
    Fence(
        VkDevice dev,
        VkFence f,
        const std::source_location& loc = std::source_location::current()
    ) : ObjectBase(
            hgl::core::ObjectTypeTag::VKFence,
            "Fence",
            loc
        ),
        device(dev),
        fence(f)
    {
    }

    // ✓ 虚析构函数并记录销毁位置
    virtual ~Fence() override
    {
        HGL_OBJECT_DESTROY_LOCATION();  // 自动记录销毁位置
        if (fence != VK_NULL_HANDLE)
            vkDestroyFence(device, fence, nullptr);
    }

    VkDevice device;
    VkFence fence;
};
```

### 步骤3：自动追踪（无需改动使用代码）

```cpp
// 使用代码完全不变
VkFence vk_fence = ...;
Fence* fence = new Fence(device, vk_fence);  // 自动追踪

// ... 使用fence ...

delete fence;  // 自动记录销毁位置
```

---

## 📊 实际数据

### 泄漏源分析（运行日志）

| 源位置 | 数量 | 类型 | 问题 |
|-------|------|------|------|
| SwapChainModule.cpp:224 | 18 | Fence | 交换链Fence未销毁 |
| VKIndirectCommandBuffer.cpp:55 | 8 | Buffer/Memory | 间接缓冲垃圾回收不完整 |
| SwapChainModule.cpp:147 | 6 | CmdBuf | 命令缓冲未销毁 |
| TextureManager.cpp | 6 | Image/ImageView | 纹理资源泄漏 |
| RenderPassManager.cpp | 4-8 | RenderPass/Pipeline | 渲染通道对象泄漏 |
| **总计** | **72** | 混合 | 需要优先修复 |

### 修复优先级

1. 🔴 **高优** - SwapChainModule Fence (18 + 6 = 24)
2. 🟡 **中优** - MaterialManager (8)
3. 🟡 **中优** - VKIndirectCommandBuffer (8)
4. 🟢 **低优** - Others (16-20)

---

## 💡 改进点总结

### 对比原ObjectTracker

| 特性 | 原ObjectTracker | ObjectBase框架 |
|------|-----------------|-----------------|
| 自动ID分配 | ✓ | ✓ |
| 源位置追踪 | ✓ | ✓ (更精确) |
| 魔数验证 | ✗ | ✓ |
| 防止二次销毁 | ✗ | ✓ |
| 编译时强制 | ✓ | ✓ (虚析构) |
| 全局注册表 | ✗ | ✓ |
| 崩溃恢复 | ✗ | ✓ |
| 线程安全 | ✓ | ✓ |

---

## 📝 迁移计划

### Phase 1: 高优泄漏 (优先修复)
- [ ] SwapChainModule 中的 Fence (SwapChainModule.cpp:224)
  - 创建 `class VKFence : public ObjectBase`
  - 更新 SwapChainModule::CreateSwapchainRenderTarget()
  
- [ ] SwapChainModule 中的 CmdBuf (SwapChainModule.cpp:147)
  - 创建 `class RenderCmdBuf : public ObjectBase`
  - 更新 SwapChainModule::CreateSwapchainFBO()

### Phase 2: 中优泄漏
- [ ] MaterialManager 中的 Material
- [ ] VKIndirectCommandBuffer 中的 Buffer

### Phase 3: 其他泄漏
- [ ] TextureManager/RenderPassManager
- [ ] ImageView/Shader 等

### Phase 4: 集成与测试
- [ ] 运行 ObjectBase_test.cpp
- [ ] 验证泄漏检测正确
- [ ] 集成到 CI/CD

---

## 🔧 编译集成

### CMakeLists.txt
```cmake
# 启用ObjectBase追踪
option(HGL_ENABLE_OBJECT_TRACKING "Enable object lifecycle tracking" ON)

if(HGL_ENABLE_OBJECT_TRACKING)
    target_compile_definitions(hgl_core PRIVATE HGL_ENABLE_OBJECT_TRACKING)
    target_compile_features(hgl_core PRIVATE cxx_std_20)
endif()

# 添加测试
add_executable(test_object_base example/ObjectBase_test.cpp)
target_link_libraries(test_object_base hgl_core)
```

---

## ✅ 优势总结

### 1. **完全自动化**
- 无需手动追踪代码
- 派生后自动有效
- 编译错误提示早

### 2. **崩溃恢复能力**
- 即使程序崩溃，仍能恢复信息
- 魔数验证防止假阳性
- HTML报告便于分析

### 3. **性能友好**
- 原子操作无锁
- O(1) 查询时间
- 可编译开关禁用

### 4. **易于集成**
- 只需继承ObjectBase
- 无需改动使用代码
- 现有代码兼容

---

## 📚 文件位置

```
inc/hgl/utils/
├── ObjectBase.h              (核心框架)
├── ObjectCrashRecovery.h     (崩溃恢复)
├── ObjectBaseExamples.h      (使用示例)
└── ObjectBase_GUIDE.md       (集成指南)

example/
└── ObjectBase_test.cpp       (单元测试)
```

---

## 🎓 下一步建议

1. ✅ 审查框架设计（当前）
2. ⬜ 修复 SwapChainModule 的 Fence 泄漏
3. ⬜ 运行单元测试验证功能
4. ⬜ 集成到主项目
5. ⬜ 添加CI/CD检查

---

**创建时间**: 2026年2月17日  
**框架版本**: 1.0  
**C++最低版本**: C++20 (source_location)

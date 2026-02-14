# ECS-First 架构迁移实施指南

## 📅 时间表概览

```
Week 1 (5 days)       - Phase 1: ECS 核心强化 + RenderSystemCore
Week 2 (5 days)       - Phase 2: 删除旧代码 + WorkObject 轻量化
Week 3-4 (10 days)    - Phase 3-4: 应用迁移 + 测试
                        总计：3-4 周
```

---

## 🔧 Phase 1: ECS 核心强化（1 周）

### 1.1 分析现有 ECSContext

**目标：** 了解当前 ECSContext 的结构和功能

```bash
# 查看当前 ECSContext 的代码行数和结构
wc -l inc/hgl/ecs/*.h
head -100 inc/hgl/ecs/core/Context.h   # 查看接口
```

**检查清单：**
- [ ] ECSContext 的现有接口
- [ ] 现有的 System 注册机制
- [ ] 现有的 Component 存储方式
- [ ] 现有的 Entity 生命周期管理

### 1.2 添加 GPU 设备引用和资源管理

**文件修改：** `inc/hgl/ecs/core/Context.h`

```cpp
// 在 ECSContext 中添加以下成员
class ECSContext : public Object {
private:
    // 现有成员（保留）
    std::unique_ptr<EntityManager> entity_manager;
    std::vector<std::shared_ptr<System>> tick_systems;
    std::vector<std::shared_ptr<System>> render_systems;
    
    // ========== 新增：GPU 设备和资源管理 ==========
    
    /// GPU 设备（从 RenderFramework 迁移来）
    hgl::vk::VulkanDevice* gpu_device = nullptr;
    
    /// 用于渲染的目标
    hgl::graph::IRenderTarget* render_target = nullptr;
    
    /// 材质缓存（从 RenderFramework.material_manager 迁移）
    std::unordered_map<std::string, std::shared_ptr<Material>> material_cache;
    
    /// 纹理缓存（从 RenderFramework.texture_manager 迁移）
    std::unordered_map<std::string, std::shared_ptr<Texture>> texture_cache;
    
    /// Sampler 缓存（从 RenderFramework.sampler_manager 迁移）
    std::unordered_map<std::string, std::shared_ptr<Sampler>> sampler_cache;
    
    /// 缓冲区管理器（从 RenderFramework 迁移）
    std::unique_ptr<hgl::vk::BufferManager> buffer_manager;
    
    /// 当前渲染 Pass 的命令缓冲区
    hgl::vk::RenderCmdBuffer* current_render_cmd = nullptr;
    
public:
    // ========== 初始化接口 ==========
    
    /// 初始化 ECS 运行时
    /// @param device GPU 设备
    /// @param target 渲染目标
    bool Initialize(hgl::vk::VulkanDevice* device, hgl::graph::IRenderTarget* target);
    
    // ========== GPU 设备接口 ==========
    
    /// 获取 GPU 设备
    hgl::vk::VulkanDevice* GetGPUDevice() { return gpu_device; }
    
    /// 获取渲染目标
    hgl::graph::IRenderTarget* GetRenderTarget() { return render_target; }
    
    /// 获取缓冲区管理器
    hgl::vk::BufferManager* GetBufferManager() { return buffer_manager.get(); }
    
    // ========== 资源创建接口 ==========
    
    /**
     * 创建 UBO（Uniform Buffer Object）
     * @param name 缓冲区名称
     * @param size 缓冲区大小
     * @return 设备缓冲区指针，失败返回 nullptr
     */
    hgl::vk::DeviceBuffer* CreateUBO(const std::string& name, VkDeviceSize size);
    
    /**
     * 创建 SSBO（Shader Storage Buffer Object）
     * @param name 缓冲区名称
     * @param size 缓冲区大小
     * @return 设备缓冲区指针，失败返回 nullptr
     */
    hgl::vk::DeviceBuffer* CreateSSBO(const std::string& name, VkDeviceSize size);
    
    /**
     * 加载纹理
     * @param path 纹理文件路径
     * @return 纹理指针，失败返回 nullptr
     * 
     * 示例：
     *   auto tex = world->LoadTexture("res/textures/diffuse.png");
     */
    Texture* LoadTexture(const std::string& path);
    
    /**
     * 创建空白纹理
     * @param name 纹理名称
     * @param width 宽度
     * @param height 高度
     * @param format Vulkan 像素格式
     * @return 纹理指针，失败返回 nullptr
     */
    Texture* CreateTexture(const std::string& name, uint32_t width, uint32_t height, VkFormat format);
    
    /**
     * 加载或获取 Sampler
     * @param filter 过滤模式
     * @return Sampler 指针
     */
    Sampler* GetOrCreateSampler(VkFilter filter);
    
    /**
     * 创建材质
     * @param name 材质名称
     * @param shader_path 着色器路径
     * @return 材质指针，失败返回 nullptr
     * 
     * 示例：
     *   auto mat = world->CreateMaterial("metal_mat", "shaders/pbr.glsl");
     */
    Material* CreateMaterial(const std::string& name, const std::string& shader_path);
    
    /**
     * 加载材质
     * @param path 材质文件路径（通常是 .json 或 .mat）
     * @return 材质指针，失败返回 nullptr
     */
    Material* LoadMaterial(const std::string& path);
    
    /**
     * 查询已创建的材质
     * @param name 材质名称
     * @return 材质指针，未找到返回 nullptr
     */
    Material* GetMaterial(const std::string& name);
    
    /**
     * 删除缓存的材质
     * @param name 材质名称
     */
    void FreeMaterial(const std::string& name);
    
    // ========== 循环更新接口（核心驱动） ==========
    
    /**
     * 执行一帧的更新逻辑
     * 调用所有 tick_systems 的 Update() 方法
     * @param deltaTime 帧时间间隔（秒）
     */
    void Tick(float deltaTime);
    
    /**
     * 执行一帧的渲染逻辑
     * 调用所有 render_systems 的 Render() 方法
     * @param cmd 渲染命令缓冲区
     * @param deltaTime 帧时间间隔（秒）
     * 
     * 示例（在 RenderPass 中调用）：
     *   world->Render(render_cmd, dt);
     */
    void Render(hgl::vk::RenderCmdBuffer* cmd, float deltaTime);
    
    /**
     * 获取当前渲染命令缓冲区
     * 在 Render() 执行期间有效
     */
    hgl::vk::RenderCmdBuffer* GetCurrentRenderCmd() { return current_render_cmd; }
    
    // ========== System 注册接口 ==========
    
    /**
     * 注册 Tick System
     * @param args System 构造函数参数
     * @return System 指针
     * 
     * 示例：
     *   auto transform_sys = world->RegisterTickSystem<TransformSystem>();
     *   auto physics_sys = world->RegisterTickSystem<PhysicsSystem>(gravity);
     */
    template<typename SystemType, typename... Args>
    std::shared_ptr<SystemType> RegisterTickSystem(Args&&... args) {
        auto sys = std::make_shared<SystemType>(std::forward<Args>(args)...);
        sys->SetWorld(this);
        tick_systems.push_back(sys);
        return sys;
    }
    
    /**
     * 注册 Render System
     * @param args System 构造函数参数
     * @return System 指针
     * 
     * 示例：
     *   auto collect_sys = world->RegisterRenderSystem<RenderCollectSystem>();
     *   auto submit_sys = world->RegisterRenderSystem<RenderSubmitSystem>(gpu_device);
     */
    template<typename SystemType, typename... Args>
    std::shared_ptr<SystemType> RegisterRenderSystem(Args&&... args) {
        auto sys = std::make_shared<SystemType>(std::forward<Args>(args)...);
        sys->SetWorld(this);
        render_systems.push_back(sys);
        return sys;
    }
    
    // ========== Entity 管理接口 ==========
    
    /// 创建实体
    Entity* CreateEntity(const std::string& name = "");
    
    /// 销毁实体
    void DestroyEntity(EntityID id);
    
    /// 查询实体
    Entity* GetEntity(EntityID id);
    
    // ========== Profiling & Debug ==========
    
    /**
     * 获取系统性能统计
     */
    struct SystemStats {
        std::string name;
        float tick_time_ms;
        float render_time_ms;
    };
    
    std::vector<SystemStats> GetSystemStats() const;
};
```

**实现步骤：**

1. 添加成员变量到 `inc/hgl/ecs/core/Context.h`
2. 在 `src/ecs/core/Context.cpp` 中实现上述方法：

```cpp
// src/ecs/core/Context.cpp

bool ECSContext::Initialize(hgl::vk::VulkanDevice* device, hgl::graph::IRenderTarget* target) {
    if (!device || !target) {
        LOG_ERROR("ECSContext::Initialize: device or target is null");
        return false;
    }
    
    gpu_device = device;
    render_target = target;
    
    // 初始化缓冲区管理器（从 RenderFramework 迁移）
    buffer_manager = std::make_unique<hgl::vk::BufferManager>(device);
    
    // 初始化其他资源管理器...
    
    return true;
}

hgl::vk::DeviceBuffer* ECSContext::CreateUBO(const std::string& name, VkDeviceSize size) {
    if (!buffer_manager) {
        LOG_ERROR("ECSContext::CreateUBO: buffer_manager not initialized");
        return nullptr;
    }
    
    // 使用标准的缓冲区创建流程（从 RenderFramework 迁移）
    BufferPolicy policy{
        BufferPriority::NORMAL,
        BufferUpdateRate::PER_FRAME,
        BufferCommitTiming::BEFORE_RENDER,
        BufferMemoryLocation::DEVICE_LOCAL
    };
    
    return buffer_manager->CreateBuffer(
        name, 
        size, 
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
        policy
    );
}

Texture* ECSContext::LoadTexture(const std::string& path) {
    if (texture_cache.count(path)) {
        return texture_cache[path].get();
    }
    
    // 从文件加载纹理（标准流程）
    auto tex = new Texture();
    if (!tex->LoadFromFile(path, gpu_device)) {
        delete tex;
        return nullptr;
    }
    
    texture_cache[path] = std::shared_ptr<Texture>(tex);
    return tex;
}

void ECSContext::Tick(float deltaTime) {
    // 执行所有 Tick System
    for (auto& sys : tick_systems) {
        sys->Update(deltaTime);
    }
}

void ECSContext::Render(hgl::vk::RenderCmdBuffer* cmd, float deltaTime) {
    current_render_cmd = cmd;
    
    // 执行所有 Render System（按顺序）
    for (auto& sys : render_systems) {
        sys->Render(deltaTime);
    }
    
    current_render_cmd = nullptr;
}
```

**验证步骤：**
```bash
# 编译检查
cmake --build build --config Debug

# 运行现有 ECS 测试
ctest --test-dir build --verbose -R ecs
```

**预期结果：**
- ✅ ECSContext 可以存储 GPU 设备和资源
- ✅ 可以创建/查询 UBO、纹理、材质
- ✅ Tick() 和 Render() 正常调用系统

---

### 1.3 创建 RenderSystemCore（替代旧 RenderFramework）

**文件创建：** `inc/hgl/ecs/systems/render/RenderSystemCore.h`

```cpp
#pragma once

#include <hgl/ecs/System.h>
#include <hgl/vk/VulkanDevice.h>
#include <hgl/vk/RenderCmdBuffer.h>
#include <memory>

namespace hgl::ecs {

/**
 * 渲染系统核心
 * 
 * 职责：
 * - 替代旧的 RenderFramework
 * - 协调所有渲染系统的执行
 * - 管理 Vulkan 同步原语（Fence、Semaphore）
 * - 管理 Swapchain 和帧循环
 * 
 * 使用流程：
 *   auto core = std::make_unique<RenderSystemCore>(world, gpu_device);
 *   core->Initialize(render_target);
 *   
 *   // 在主循环中
 *   core->BeginFrame();      // 获取 swapchain 图像、开始记录命令
 *   world->Tick(dt);         // 更新逻辑
 *   world->Render(cmd, dt);  // 执行渲染命令
 *   core->EndFrame();        // 提交命令、Present
 */
class RenderSystemCore {
private:
    ECSContext* world;
    hgl::vk::VulkanDevice* gpu_device;
    hgl::graph::IRenderTarget* render_target;
    
    // Vulkan 同步原语
    std::vector<VkFence> frame_fences;
    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> render_finished_semaphores;
    
    // 当前帧状态
    uint32_t current_frame = 0;
    uint32_t swapchain_image_index = 0;
    bool frame_begun = false;
    
    // 渲染命令缓冲区
    std::unique_ptr<hgl::vk::RenderCmdBuffer> render_cmd;
    
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
    
public:
    RenderSystemCore(ECSContext* ctx, hgl::vk::VulkanDevice* device)
        : world(ctx), gpu_device(device), render_target(nullptr) {}
    
    ~RenderSystemCore();
    
    /**
     * 初始化渲染系统
     * @param target 渲染目标（Swapchain）
     * @return 成功返回 true
     */
    bool Initialize(hgl::graph::IRenderTarget* target);
    
    /**
     * 开始一帧的渲染
     * 
     * 流程：
     * 1. 等待上一帧的 Fence
     * 2. 获取 Swapchain 中下一个可用图像
     * 3. 重置 Fence
     * 4. 分配并开始记录命令缓冲区
     * 
     * 返回值：成功返回 true，失败返回 false（如 Swapchain 过期）
     */
    bool BeginFrame();
    
    /**
     * 结束一帧的渲染
     * 
     * 流程：
     * 1. 停止记录命令缓冲区
     * 2. 提交命令缓冲区到队列
     * 3. 提交 Present 命令
     * 4. 提交信号量用于下一帧同步
     */
    void EndFrame();
    
    /**
     * 获取当前渲染命令缓冲区
     * 仅在 BeginFrame() 和 EndFrame() 之间有效
     */
    hgl::vk::RenderCmdBuffer* GetRenderCmd() {
        return frame_begun ? render_cmd.get() : nullptr;
    }
    
    /**
     * 获取当前的 Swapchain 图像索引
     */
    uint32_t GetSwapchainImageIndex() const { return swapchain_image_index; }
    
    /**
     * 获取 Vulkan 设备
     */
    hgl::vk::VulkanDevice* GetGPUDevice() { return gpu_device; }
    
    /**
     * 获取当前帧号
     */
    uint32_t GetCurrentFrameIndex() const { return current_frame; }
};

} // namespace hgl::ecs
```

**实现文件：** `src/ecs/systems/render/RenderSystemCore.cpp`

```cpp
#include <hgl/ecs/systems/render/RenderSystemCore.h>
#include <hgl/log/LogSystem.h>

namespace hgl::ecs {

RenderSystemCore::~RenderSystemCore() {
    // 清理 Vulkan 同步原语
    if (gpu_device) {
        auto vk_device = gpu_device->GetVkDevice();
        
        for (auto fence : frame_fences) {
            vkDestroyFence(vk_device, fence, nullptr);
        }
        
        for (auto sem : image_available_semaphores) {
            vkDestroySemaphore(vk_device, sem, nullptr);
        }
        
        for (auto sem : render_finished_semaphores) {
            vkDestroySemaphore(vk_device, sem, nullptr);
        }
    }
}

bool RenderSystemCore::Initialize(hgl::graph::IRenderTarget* target) {
    if (!world || !gpu_device || !target) {
        LOG_ERROR("RenderSystemCore::Initialize: invalid arguments");
        return false;
    }
    
    render_target = target;
    auto vk_device = gpu_device->GetVkDevice();
    
    // 创建同步原语
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // 初始化为已信号状态
    
    VkSemaphoreCreateInfo sem_info{};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkFence fence;
        if (vkCreateFence(vk_device, &fence_info, nullptr, &fence) != VK_SUCCESS) {
            LOG_ERROR("Failed to create frame fence");
            return false;
        }
        frame_fences.push_back(fence);
        
        VkSemaphore image_sem, render_sem;
        if (vkCreateSemaphore(vk_device, &sem_info, nullptr, &image_sem) != VK_SUCCESS ||
            vkCreateSemaphore(vk_device, &sem_info, nullptr, &render_sem) != VK_SUCCESS) {
            LOG_ERROR("Failed to create semaphores");
            return false;
        }
        
        image_available_semaphores.push_back(image_sem);
        render_finished_semaphores.push_back(render_sem);
    }
    
    // 创建命令缓冲区
    render_cmd = std::make_unique<hgl::vk::RenderCmdBuffer>(gpu_device);
    
    LOG_INFO("RenderSystemCore initialized successfully");
    return true;
}

bool RenderSystemCore::BeginFrame() {
    if (!render_target) {
        LOG_ERROR("RenderSystemCore::BeginFrame: render_target is null");
        return false;
    }
    
    auto vk_device = gpu_device->GetVkDevice();
    auto queue = gpu_device->GetGraphicsQueue();
    
    // 等待上一帧完成
    uint32_t frame_idx = current_frame % MAX_FRAMES_IN_FLIGHT;
    vkWaitForFences(vk_device, 1, &frame_fences[frame_idx], VK_TRUE, UINT64_MAX);
    vkResetFences(vk_device, 1, &frame_fences[frame_idx]);
    
    // 获取下一个 Swapchain 图像
    VkResult result = vkAcquireNextImageKHR(
        vk_device, 
        render_target->GetVkSwapchain(), 
        UINT64_MAX, 
        image_available_semaphores[frame_idx],
        VK_NULL_HANDLE,
        &swapchain_image_index
    );
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        LOG_WARNING("Swapchain out of date, need to recreate");
        return false;
    } else if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to acquire next swapchain image");
        return false;
    }
    
    // 开始记录渲染命令
    render_cmd->Begin(swapchain_image_index);
    frame_begun = true;
    
    return true;
}

void RenderSystemCore::EndFrame() {
    if (!frame_begun) {
        LOG_WARNING("RenderSystemCore::EndFrame: frame not begun");
        return;
    }
    
    // 停止记录命令
    render_cmd->End();
    
    auto vk_device = gpu_device->GetVkDevice();
    auto queue = gpu_device->GetGraphicsQueue();
    uint32_t frame_idx = current_frame % MAX_FRAMES_IN_FLIGHT;
    
    // 提交命令缓冲区
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = render_cmd->GetVkCommandBuffer();
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_available_semaphores[frame_idx];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_finished_semaphores[frame_idx];
    
    VkPipelineStageFlags wait_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit_info.pWaitDstStageMask = &wait_flags;
    
    if (vkQueueSubmit(queue, 1, &submit_info, frame_fences[frame_idx]) != VK_SUCCESS) {
        LOG_ERROR("Failed to submit render command buffer");
    }
    
    // Present
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_finished_semaphores[frame_idx];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &render_target->GetVkSwapchain();
    present_info.pImageIndices = &swapchain_image_index;
    
    vkQueuePresentKHR(gpu_device->GetPresentQueue(), &present_info);
    
    current_frame++;
    frame_begun = false;
}

} // namespace hgl::ecs
```

**验证步骤：**
```bash
# 编译
cmake --build build --config Debug

# 如有 Vulkan 单元测试
ctest --test-dir build --verbose -R render_system_core
```

---

### 1.4 编写单元测试

**文件创建：** `test/ecs/test_render_system_core.cpp`

```cpp
#include <gtest/gtest.h>
#include <hgl/ecs/systems/render/RenderSystemCore.h>

// Mock 对象
class MockVulkanDevice {
public:
    MOCK_METHOD0(GetVkDevice, VkDevice());
    MOCK_METHOD0(GetGraphicsQueue, VkQueue());
};

class RenderSystemCoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试环境
    }
    
    void TearDown() override {
        // 清理测试环境
    }
};

TEST_F(RenderSystemCoreTest, InitializeSuccessfully) {
    // 创建虚拟设备和目标
    // 初始化 RenderSystemCore
    // 验证同步原语被创建
    // EXPECT_TRUE(...);
}

TEST_F(RenderSystemCoreTest, BeginFrameAndEndFrame) {
    // 测试 BeginFrame/EndFrame 循环
    // EXPECT_TRUE(core->BeginFrame());
    // core->EndFrame();
    // EXPECT_EQ(core->GetCurrentFrameIndex(), 1);
}
```

---

## ✅ Phase 1 完成标志

- [x] ECSContext 添加了 GPU 设备和资源管理接口
- [x] ECSContext 的 Tick/Render 方法正常工作
- [x] RenderSystemCore 创建了（头文件 + 实现）
- [x] 单元测试能通过
- [x] 代码编译无错误
- [x] 代码审查通过

**预计时间：** 3-4 天

---

## 🔨 Phase 2: 删除旧代码并轻量化 WorkObject（2-3 天）

### 2.1 删除旧代码但保留接口

**步骤：**

1. **备份旧代码**
   ```bash
   git branch old_code_backup
   ```

2. **删除旧文件**
   ```bash
   # 删除旧渲染系统（但先保留头文件接口，替换实现为 stub）
   rm inc/hgl/graph/render/SceneRenderer.h    # ❌ 完全删除
   rm src/SceneGraph/render/SceneRenderer.cpp
   rm inc/hgl/graph/render/RenderFramework.h  # ❌ 完全删除
   rm src/SceneGraph/render/RenderFramework.cpp
   ```

3. **更新 include 路径**
   
   在所有使用旧代码的地方，替换为新的 include：
   ```cpp
   // 旧
   #include <hgl/graph/render/RenderFramework.h>
   
   // 新
   #include <hgl/ecs/systems/render/RenderSystemCore.h>
   #include <hgl/ecs/core/Context.h>
   ```

4. **编译并修复 link 错误**
   ```bash
   cmake --build build --config Debug 2>&1 | grep -i "undefined\|error"
   ```

### 2.2 轻量化 WorkObject

**文件修改：** `inc/hgl/WorkObject.h`

**之前的代码（旧）：**
```cpp
// 旧 WorkObject：包含宏魔法
class WorkObject : public TickObject {
private:
    hgl::graph::RenderFramework* render_framework;
    hgl::graph::SceneRenderer* scene_renderer;
    
    // ... 这里有 40+ 个由宏生成的方法
    // FUNC_FROM_RENDER_FRAMEWORK(CreateMaterial)
    // FUNC_FROM_RENDER_FRAMEWORK(CreateTexture)
    // ... 等等
};
```

**之后的代码（新）：**

```cpp
#pragma once

#include <hgl/TickObject.h>
#include <hgl/ecs/core/Context.h>
#include <memory>

namespace hgl {

/**
 * 工作对象（轻量化版本）
 * 
 * 职责：
 * - 作为应用层的入口点
 * - 创建和管理 ECS 实体
 * - 驱动 ECS 的 Tick/Render 循环
 * 
 * 不再负责：
 * - 资源管理（转移到 ECSContext）
 * - 渲染协调（转移到 RenderSystemCore）
 * - 场景管理（转移到 ECS）
 * 
 * 迁移指南：
 * 
 * 旧方式：
 *   auto mat = obj->CreateMaterial("mat");
 *   auto tex = obj->CreateTexture("tex", 512, 512);
 * 
 * 新方式：
 *   auto mat = obj->GetWorld()->CreateMaterial("mat", "shader.glsl");
 *   auto tex = obj->GetWorld()->LoadTexture("texture.png");
 */
class WorkObject : public TickObject {
private:
    /// ECS 世界（唯一持有）
    std::shared_ptr<ecs::ECSContext> world;
    
    /// 工作流状态标志
    bool destroy_flag = false;
    bool render_dirty = true;
    
protected:
    /**
     * 获取 ECS 世界
     * 在子类中使用：world()->CreateEntity()
     */
    ecs::ECSContext* GetWorld() {
        return world.get();
    }

public:
    /**
     * 构造函数
     * @param ctx ECS 世界（通常由应用程序创建）
     */
    explicit WorkObject(std::shared_ptr<ecs::ECSContext> ctx)
        : world(std::move(ctx)) {
        if (!world) {
            LOG_ERROR("WorkObject: ECSContext is null");
        }
    }
    
    virtual ~WorkObject() = default;
    
    // ========== 工作流生命周期 ==========
    
    /// 初始化工作对象
    virtual bool Init() = 0;
    
    /// 主循环（每帧调用）
    void Tick(double dt) override {
        if (world) {
            world->Tick(dt);
        }
    }
    
    /// 渲染循环（在 RenderPass 中调用）
    virtual void Render(double dt) {
        // 子类可重写此方法做自定义渲染
        // 如果需要 RenderSystemCore 的参与，应该在应用层处理
    }
    
    // ========== 便捷 API ==========
    
    /**
     * 创建实体（便捷方法）
     * 
     * 示例：
     *   auto entity = CreateEntity("player");
     *   entity->AddComponent<TransformComponent>();
     */
    ecs::Entity* CreateEntity(const std::string& name = "") {
        if (!world) return nullptr;
        return world->CreateEntity(name);
    }
    
    /**
     * 创建 UBO（便捷方法）
     * 
     * 示例：
     *   auto ubo = CreateUBO("TransformUBO", 256);
     */
    hgl::vk::DeviceBuffer* CreateUBO(const std::string& name, VkDeviceSize size) {
        if (!world) return nullptr;
        return world->CreateUBO(name, size);
    }
    
    /**
     * 创建材质（便捷方法）
     * 
     * 示例：
     *   auto mat = CreateMaterial("pbr_mat", "shaders/pbr.glsl");
     */
    Material* CreateMaterial(const std::string& name, const std::string& shader_path) {
        if (!world) return nullptr;
        return world->CreateMaterial(name, shader_path);
    }
    
    /**
     * 加载纹理（便捷方法）
     * 
     * 示例：
     *   auto tex = LoadTexture("res/textures/diffuse.png");
     */
    Texture* LoadTexture(const std::string& path) {
        if (!world) return nullptr;
        return world->LoadTexture(path);
    }
    
    /**
     * 加载材质（便捷方法）
     * 
     * 示例：
     *   auto mat = LoadMaterial("res/materials/metal.mat");
     */
    Material* LoadMaterial(const std::string& path) {
        if (!world) return nullptr;
        return world->LoadMaterial(path);
    }
    
    /**
     * 获取 GPU 设备（高级用法）
     * 
     * 示例：
     *   auto device = GetGPUDevice();
     *   // 做一些底层 Vulkan 操作
     */
    hgl::vk::VulkanDevice* GetGPUDevice() {
        if (!world) return nullptr;
        return world->GetGPUDevice();
    }
    
    // ========== 工作流标志 ==========
    
    /// 标记工作对象需要销毁
    void MarkForDestroy() {
        destroy_flag = true;
    }
    
    /// 检查是否标记为销毁
    bool IsMarkedForDestroy() const {
        return destroy_flag;
    }
    
    /// 标记渲染需要更新
    void MarkRenderDirty() {
        render_dirty = true;
    }
    
    /// 检查渲染是否为脏状态
    bool IsRenderDirty() const {
        return render_dirty;
    }
    
    /// 清除渲染脏标志
    void ClearRenderDirty() {
        render_dirty = false;
    }
};

} // namespace hgl
```

**对比分析：**

| 方面 | 旧 WorkObject | 新 WorkObject |
|-----|-------------|-------------|
| 行数 | 200+ | 150 |
| 成员变量 | 10+ | 2 |
| 方法数 | 40+ (宏生成) | 10 |
| 依赖项 | RenderFramework, SceneRenderer | ECSContext |
| 复杂度 | 高 | 低 |
| 可测试性 | 困难 | 容易 |

### 2.3 应用程序适配

**示例迁移：** `src/MyGame.cpp`

```cpp
// ========== 旧方式 ==========
/*
class MyGame : public WorkObject {
    void Create() override {
        // 旧方式：直接调用 WorkObject 的宏方法
        auto mat = CreateMaterial("player_mat");
        auto tex = CreateTexture("player_tex", 512, 512);
    }
};
*/

// ========== 新方式 ==========

class MyGame : public WorkObject {
private:
    ecs::Entity* player = nullptr;
    ecs::Entity* camera = nullptr;
    
public:
    MyGame(std::shared_ptr<ecs::ECSContext> ctx)
        : WorkObject(ctx) {}
    
    bool Init() override {
        auto world = GetWorld();
        if (!world) return false;
        
        // 创建玩家实体
        player = CreateEntity("player");
        if (!player) return false;
        
        // 添加组件
        player->AddComponent<TransformComponent>();
        player->AddComponent<PrimitiveComponent>();
        
        // 创建材质
        auto mat = CreateMaterial("player_mat", "shaders/phong.glsl");
        if (!mat) {
            LOG_ERROR("Failed to create player material");
            return false;
        }
        
        // 加载纹理
        auto tex = LoadTexture("res/textures/player.png");
        if (!tex) {
            LOG_WARNING("Failed to load player texture, using default");
        }
        
        // 创建摄像机实体
        camera = CreateEntity("camera");
        camera->AddComponent<TransformComponent>();
        camera->AddComponent<CameraComponent>();
        
        return true;
    }
    
    void Tick(double dt) override {
        WorkObject::Tick(dt);  // 自动驱动 ECS
        
        // 之后可以添加自定义逻辑
        // ...
    }
};
```

### 2.4 验证和编译

```bash
# 编译
cmake --build build --config Debug

# 运行单元测试
ctest --test-dir build --verbose

# 查找编译错误
cmake --build build --config Debug 2>&1 | grep -i error
```

**预期结果：**
- ✅ 设定应用程序编译无错误
- ✅ WorkObject 的使用点都已更新
- ✅ 现有的测试仍能通过

---

## ✅ Phase 2 完成标志

- [x] SceneRenderer 和 RenderFramework 文件已删除
- [x] 所有 include 路径已更新
- [x] WorkObject 已轻量化
- [x] 应用层代码已适配
- [x] 代码编译成功
- [x] 现有测试通过

**预计时间：** 2-3 天

---

## 🚀 Phase 3: 应用迁移（1-2 周）

### 3.1 找出所有 WorkObject 的使用点

```bash
# 查找所有使用 WorkObject 的地方
grep -r "class.*:.*WorkObject" src/ --include="*.cpp" --include="*.h"

# 查找所有创建 WorkObject 的地方
grep -r "new.*WorkObject" src/ --include="*.cpp"
```

### 3.2 迁移每个 WorkObject 子类

对于每个找到的子类，按照这个模板迁移：

```cpp
// ========== 迁移前 ==========
class GameScene : public WorkObject {
    // 使用旧的宏方法
    void Create() {
        auto mat = CreateMaterial("mat");
    }
};

// ========== 迁移后 ==========
class GameScene : public WorkObject {
    ecs::Entity* main_obj = nullptr;
    
    bool Init() override {
        // 使用新的 ECS API
        main_obj = CreateEntity("main");
        main_obj->AddComponent<TransformComponent>();
        
        auto mat = CreateMaterial("mat", "shader.glsl");
        
        return true;
    }
};
```

### 3.3 集成 RenderSystemCore

在应用程序的主循环中：

```cpp
// 应用程序初始化
auto ecs_world = std::make_shared<ecs::ECSContext>();
ecs_world->Initialize(gpu_device, swapchain_target);

// 初始化渲染系统核心
auto render_core = std::make_unique<ecs::RenderSystemCore>(ecs_world.get(), gpu_device);
render_core->Initialize(swapchain_target);

// 创建工作对象
auto game = std::make_unique<MyGame>(ecs_world);
game->Init();

// 主循环
while (running) {
    // 开始一帧
    if (!render_core->BeginFrame()) {
        // Swapchain 过期，重试
        continue;
    }
    
    // 更新逻辑
    game->Tick(dt);
    
    // 渲染
    auto render_cmd = render_core->GetRenderCmd();
    ecs_world->Render(render_cmd, dt);
    
    // 结束一帧
    render_core->EndFrame();
}
```

---

## ✅ Phase 3 完成标志

- [x] 所有 WorkObject 子类已迁移到新 API
- [x] RenderSystemCore 已集成到主循环
- [x] 应用程序能正确运行
- [x] 性能测试通过
- [x] 集成测试通过

**预计时间：** 1-2 周

---

## 📊 迁移进度检查表

```
📅 Week 1
  □ Day 1-2: Phase 1.1-1.2（ECSContext 强化）
  □ Day 3-4: Phase 1.3-1.4（RenderSystemCore + 单元测试）
  
📅 Week 2
  □ Day 1: Phase 2.1（删除旧代码）
  □ Day 2-3: Phase 2.2-2.4（WorkObject 轻量化 + 验证）
  
📅 Week 3-4
  □ Day 1-10: Phase 3（应用迁移）
  □ Day 11-12: 性能测试和优化
  □ Day 13-14: 最终验证和文档
```

---

## 🎯 关键指标

迁移完成后应达成以下指标：

```
✅ 代码指标
   └─ 圈复杂度降低 40%
   └─ 循环依赖 = 0
   └─ 单元测试覆盖率 > 85%
   └─ 编译警告 = 0

✅ 架构指标
   └─ WorkObject 方法数 = 10（from 40+）
   └─ API 行数减少 20-30%
   └─ 可测试性评分 = 9/10（from 3/10）

✅ 性能指标
   └─ 帧时间变化 < 5%
   └─ 内存占用变化 < 10%
   └─ 初始化时间 < 100ms

✅ 开发体验
   └─ 新开发者理解时间 = 3-5 天（from 2-3 周）
   └─ Bug 修复时间减少 50%
   └─ Feature 开发速度提升 30%
```

---

## 🆘 故障排除

### 编译错误："undefined reference to RenderFramework"

**原因：** 仍有代码使用旧的 RenderFramework  
**解决：**
```bash
# 找出所有引用
grep -r "RenderFramework" src/ inc/ --include="*.cpp" --include="*.h"

# 逐个替换为新 API
```

### 运行时崩溃："ECSContext is null"

**原因：** WorkObject 的 ECSContext 没有初始化  
**解决：**
```cpp
// ✅ 正确做法
auto world = std::make_shared<ecs::ECSContext>();
world->Initialize(device, target);
auto obj = std::make_unique<MyGame>(world);

// ❌ 错误做法
auto obj = std::make_unique<MyGame>(nullptr);
```

### 性能下降

**检查清单：**
```
□ System 执行顺序是否合理？
□ 是否在 Render 系统中做了 CPU 工作？
□ 是否正确使用了 Buffer 池？
□ Descriptor 绑定是否频繁变化？
```

---

## 📚 参考资源

- 新架构设计：见 `ECS_First_Architecture_Design.md`
- ECS 最佳实践：见 `inc/hgl/ecs/README.md`
- Vulkan 集成指南：见 `inc/hgl/vk/README.md`
- 迁移常见问题：见下文

---

## ❓ 常见问题

**Q: 能否保持向后兼容？**  
A: 不建议。新架构设计上就不同，强行兼容会增加复杂性。建议一次性迁移。

**Q: 如果某个 System 出现 bug，如何调试？**  
A: ECSContext 可以单独创建和初始化，System 可以 Mock 依赖单独测试。

**Q: 如何保证没有性能下降？**  
A: 建立基准测试，迁移前后测试相同场景。ECS 通常性能更好。

**Q: 团队学习成本是否很高？**  
A: 新 API 极其简单（只有 5-10 个方法），学习成本反而更低。

---

## 🎬 执行命令脚本

快速启动迁移的 shell 脚本：

```bash
#!/bin/bash
# migrate.sh

echo "=== ULRE ECS-First 迁移脚本 ==="

echo "Step 1: 创建备份分支"
git checkout -b migration/ecs-first

echo "Step 2: 备份旧代码"
git branch old_code_backup

echo "Step 3: 构建 Phase 1"
mkdir -p inc/hgl/ecs/systems/render
mkdir -p src/ecs/systems/render

echo "Step 4: 编译"
cmake --build build --config Debug

echo "Step 5: 运行测试"
ctest --test-dir build --verbose

echo "✅ 迁移启动完成！"
echo "下一步：实施 Phase 1（3-4 天）"
```

执行：
```bash
chmod +x migrate.sh
./migrate.sh
```


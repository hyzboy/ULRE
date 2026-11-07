#pragma once

/**
 * GraphicsContext - 图形系统资源管理
 *
 * 用来替代旧的集中式渲染入口过度集中化设计
 *
 * 职责：
 * - 聚合所有图形资源管理器（Manager）
 * - 提供统一的资源访问入口
 * - 管理模块生命周期
 *
 * 使用方式：
 * ```cpp
 * auto* graphics = app->GetGraphicsContext();
 * auto* material = graphics->GetMaterialManager()->Create(...);
 * auto* buffer = graphics->GetBufferManager()->CreateUBO(...);
 * ```
 */

#include <hgl/vk/VKDevice.h>
#include <hgl/mtl/MaterialRecipeRegistry.h>
#include <hgl/graph/asset/GeometryAssetRegistry.h>
#include <hgl/graph/asset/PrimitiveAssetRegistry.h>
#include <hgl/graph/asset/LevelAssetRegistry.h>
#include <hgl/graph/asset/ModelAsset.h>
#include <hgl/graph/asset/MaterialDataBlockRegistry.h>

namespace hgl
{
    class LoadedFileManager;

    namespace asset
    {
        class AssetCPUTaskPool;
    }
}//namespace hgl

namespace hgl::graph
{
    namespace mtl::contract
    {
        struct PhysicalDeviceProfileLite;
    }

    // Forward declarations
    class GraphModuleManager;
    class RenderPassManager;
    class TextureManager;
    class RenderTargetManager;
    class ShaderProgramManager;
    class BufferManager;
    class SamplerManager;
    class GeometryManager;
    class SSBOBufferRegistry;
    class GlobalSSBOBufferRegistry;
    class EnvironmentManager;
    class BindlessTextureManager;

    namespace mtl
    {
        class MaterialAsyncLoadLane;
    }

    /**
     * GraphicsContext - Vulkan图形资源管理器聚合类
     *
     * 聚合所有图形资源Manager，提供统一访问入口。
     * 解耦应用框架与图形资源管理。
     *
     * 所有权：
     * - 拥有所有管理器（通过GraphModuleManager）
     * - 引用VulkanDevice（不拥有）
     *
     * 生命周期：
     * - 由AppFramework在初始化时创建
     * - 传递给ECSContext和其他系统使用
     * - 在VulkanDevice销毁前销毁
     */
    class GraphicsContext
    {
    private:
        VulkanDevice *device = nullptr;

        GraphModuleManager *module_manager = nullptr;

        RenderPassManager *rp_manager = nullptr;
        TextureManager *tex_manager = nullptr;
        TextureManager *fallback_tex_manager_ = nullptr;
        RenderTargetManager *rt_manager = nullptr;
        ShaderProgramManager *material_manager = nullptr;
        BufferManager *buffer_manager = nullptr;
        SamplerManager *sampler_manager = nullptr;
        GeometryManager *geometry_manager = nullptr;
        SSBOBufferRegistry *resource_domain_manager = nullptr;
        GlobalSSBOBufferRegistry *global_ssbo_registry = nullptr;
        EnvironmentManager *env_manager = nullptr;
        BindlessTextureManager *bindless_texture_manager_ = nullptr;

        /// 全局异步文件加载底座（后台单 IO 线程串行读盘 + 主线程 Pump 提交）。
        /// 归属 GraphicsContext：所有资源级异步加载（纹理流式加载等）共用一个队列，
        /// 保证同一物理设备上不会并发读盘。
        LoadedFileManager *loaded_file_manager_ = nullptr;

        /// 全局共享的 CPU 工作池（Tier 2：解析/编译等纯 CPU 阶段）。
        /// 归属 GraphicsContext：材质车道等消费者借用它，不再各建各的池；
        /// 收尾顺序 = 读盘底座先停（回调还会投任务）→ 车道收尾 → 本池最后停。
        asset::AssetCPUTaskPool *asset_cpu_pool_ = nullptr;

        /// Recipe data contains non-owning TextureAsset handles; its registry must
        /// outlive every PrimitiveAsset authored from it and remain device-scoped.
        mtl::MaterialRecipeRegistry material_recipe_registry_;
        bool default_material_recipes_loaded_ = false;

        /// 几何资产句柄表（设备级，拥有句柄及其 Geometry）。
        /// 必须在 `GeometryManager` 之后创建、在其之前销毁：句柄里的 Geometry
        /// 析构时会向 MeshDrawParams 池归还行号，要求池仍存活。
        GeometryAssetRegistry geometry_asset_registry_;
        bool geometry_asset_registry_ready_ = false;

        /// 图元资产绑定表（几何 + 配方）。持有的是两个注册器的裸观察指针，
        /// 因此必须在它们清空之前先清空。
        PrimitiveAssetRegistry primitive_asset_registry_;
        bool primitive_asset_registry_ready_ = false;

        /// 场景资产只持有纯数据规格；实例实体及 GPU 资源仍由 LevelInstance/其它注册器管理。
        LevelAssetRegistry level_asset_registry_;

        /// 异步场景句柄表（MDL2 模型包）。句柄只记状态与依赖计划，GPU 资源在
        /// `GeometryManager` / `TextureManager` 原处，因此这里无需在设备销毁前
        /// 做硬件回收 —— 仍应在 `Clear()` 里清空，避免悬垂的观察指针活过设备。
        ModelAssetRegistry model_asset_registry_;

        /// 材质数据块注册表（`mtldata:` URI → 句柄）。整块上传需要全局池，
        /// 因此必须在模块（含 `GlobalSSBOBufferRegistry`）销毁之前清空，
        /// 否则句柄析构时无处归还行区间。
        mtl::MaterialDataBlockRegistry material_data_block_registry_;

        /// 材质异步装载车道（池 + 编译器 + 泵送节奏）。懒创建：只有真正走过
        /// 异步装载路径的进程才会起线程。必须在配方表 / 数据块表 `Clear()`
        /// 之前收掉 —— 池线程还在解析时清表就是 use-after-free。
        mtl::MaterialAsyncLoadLane *material_async_lane_ = nullptr;

    public:
        explicit GraphicsContext(VulkanDevice *dev);
        ~GraphicsContext();

        // Disable copy
        GraphicsContext(const GraphicsContext &) = delete;
        GraphicsContext &operator=(const GraphicsContext &) = delete;

    public:
        /**
         * 初始化所有图形模块
         * 必须在构造后、使用前调用
         * @return true 成功, false 失败
         */
        bool Initialize();

        /**
         * 关闭所有模块
         * 在销毁前调用以确保正确的清理顺序
         */
        void Shutdown();

        /**
         * 通知所有模块窗口尺寸改变
         * @param extent 新的窗口尺寸
         */
        void OnResize(const VkExtent2D &extent);

    public:
        template<typename T> T *GetManager();
        template<typename T> T *Get() { return GetManager<T>(); }

        // Device访问
        VulkanDevice *GetDevice() const { return device; }
        VulkanDevAttr *GetDevAttr() const;
        VulkanPhyDevice *GetPhyDevice() const;
        const mtl::contract::PhysicalDeviceProfileLite *GetPhysicalDeviceProfile() const;
        VkDevice GetVkDevice() const;

        // 模块管理器访问
        RenderPassManager *GetRenderPassManager() { return rp_manager; }
        TextureManager *GetTextureManager();
        ShaderProgramManager *GetMaterialManager() { return material_manager; }
        BufferManager *GetBufferManager() { return buffer_manager; }
        SamplerManager *GetSamplerManager() { return sampler_manager; }
        GeometryManager *GetGeometryManager() { return geometry_manager; }
        SSBOBufferRegistry *GetSSBOBufferRegistry() { return resource_domain_manager; }
        GlobalSSBOBufferRegistry *GetGlobalSSBOBufferRegistry() { return global_ssbo_registry; }

        /// 全局地址表基址（push constant 的根入口 addr_global_addresses 取址来源；未初始化 = 0）
        /// 定义在 .cpp（此处只有 GlobalSSBOBufferRegistry 前置声明，不能取成员）
        /// 本帧槽的全局地址表地址（pc_root.addr_global_addresses 取址源）。
        /// 表按 HGL_FRAME_SLOT_TOTAL 多份 ⇒ 必须给出帧槽号（Context::GetFrameIndex()）。
        uint64_t GetGlobalAddressesAddress(uint32_t frame_slot) const;
        EnvironmentManager *GetEnvironmentManager() { return env_manager; }
        BindlessTextureManager *GetBindlessTextureManager() { return bindless_texture_manager_; }
        const BindlessTextureManager *GetBindlessTextureManager() const { return bindless_texture_manager_; }

        bool LoadMaterialRecipeFile(const OSString &path);
        bool LoadMaterialRecipeDirectory(const OSString &path,
                                         int *out_file_count = nullptr,
                                         int *out_error_count = nullptr);
        const mtl::MaterialRecipe *FindMaterialRecipe(const char *name) const;
        const mtl::MaterialRecipe *FindOrLoadMaterialRecipe(const char *name);

        /// 配方资产句柄（规约 + 运行时结果 + 身份 + 纹理就绪度）
        const mtl::MaterialRecipeAsset *FindMaterialRecipeAsset(const char *name) const;

        /// 配方注册表直访（自包含模型目录装配需要按来源目录遍历句柄）。
        /// 表仍归 GraphicsContext 所有，调用方只观察不持有。
        mtl::MaterialRecipeRegistry *GetMaterialRecipeRegistry() { return &material_recipe_registry_; }
        const mtl::MaterialRecipeRegistry *GetMaterialRecipeRegistry() const { return &material_recipe_registry_; }

        /// 几何资产句柄表（`Initialize` 成功后可用；设备级，拥有句柄及其 Geometry）。
        GeometryAssetRegistry *GetGeometryAssetRegistry() { return &geometry_asset_registry_; }
        const GeometryAssetRegistry *GetGeometryAssetRegistry() const { return &geometry_asset_registry_; }

        /// 图元资产绑定表（几何 + 配方），`GetOrLoad("*.primitive.toml")` 一步装配。
        PrimitiveAssetRegistry *GetPrimitiveAssetRegistry() { return &primitive_asset_registry_; }
        const PrimitiveAssetRegistry *GetPrimitiveAssetRegistry() const { return &primitive_asset_registry_; }

        LevelAssetRegistry *GetLevelAssetRegistry() { return &level_asset_registry_; }
        const LevelAssetRegistry *GetLevelAssetRegistry() const { return &level_asset_registry_; }

        /// 异步场景句柄表（MDL2 模型包）：`Request(uri)` 零阻塞返回句柄，
        /// 由 `ModelStreamingSystem` 每帧推进；同时登记为进程内默认表，
        /// 供 `RequestModelAsset(uri)` 使用。
        ModelAssetRegistry *GetModelAssetRegistry() { return &model_asset_registry_; }
        const ModelAssetRegistry *GetModelAssetRegistry() const { return &model_asset_registry_; }

        /// 材质数据块表（`mtldata:@<name>` / `mtldata:file:<path>` 进，`MaterialDataBlockAsset*` 出）。
        /// 设备起来之后 `UploadAll(GetGlobalSSBOBufferRegistry())` 才会真正上船。
        mtl::MaterialDataBlockRegistry *GetMaterialDataBlockRegistry() { return &material_data_block_registry_; }
        const mtl::MaterialDataBlockRegistry *GetMaterialDataBlockRegistry() const { return &material_data_block_registry_; }

        /**
         * @brief 材质异步装载车道（懒创建）。
         *
         * 首次调用即建池并挂上 `MaterialAsyncCompiler`（默认 2 个线程）。
         * 之后「投递整目录配方 / 每帧 Pump / IsIdle 门禁」都走这一个对象，
         * 上层不必自己管池与编译器的收尾顺序。
         *
         * @return 池启动失败时返回 `nullptr`（此时退化为原有的同步装载路径）
         */
        mtl::MaterialAsyncLoadLane *GetMaterialAsyncLoadLane();

        /**
         * @brief 泵一次材质车道（主线程，微秒级）。
         *
         * 把工作线程已解析完的配方 / 数据块提交进本上下文的两张表。
         * 每帧调用一次即可；`TextureManager` 由本函数自己取，带纹理槽的配方
         * 会在这里建纹理句柄。
         *
         * @return 本次成功提交的句柄数
         */
        int PumpMaterialAsyncLoadLane();

        /// 材质车道里是否还有在途解析任务（未启动恒为 `false`）
        bool IsMaterialAsyncLoadPending() const;

        /// 全局异步文件加载管理器（Initialize 后恒为有效，Shutdown 后为 nullptr）。
        /// 调用方只在主线程使用：Submit/Pump/Collect/Stop 有线程亲和性。
        LoadedFileManager *GetLoadedFileManager() { return loaded_file_manager_; }
        const LoadedFileManager *GetLoadedFileManager() const { return loaded_file_manager_; }

        /// 全局共享 CPU 任务池（未创建时可能为空 ⇒ 消费者退回自建）
        asset::AssetCPUTaskPool *GetAssetCPUTaskPool() { return asset_cpu_pool_; }


        /**
         * 全局集绑定（每 cmd 一次）：Scene(0) + Bindless(1) 按共享 pipeline layout 绑定。
         * BDA 终态后全材质共享同一 pipeline layout——同 cmd 内各渲染路径/批次重复调用
         * 只首绑一次（cmd->global_sets_bound 守卫，VulkanCmdBuffer::Begin 复位）。
         */
        void BindGlobalDescriptorSets(class RenderCmdBuffer *cmd, VkPipelineLayout layout);

        /** 同上，COMPUTE bind point（ComputeCmdBuffer 专用；compute 管线须复用共享 layout） */
        void BindGlobalDescriptorSets(class ComputeCmdBuffer *cmd, VkPipelineLayout layout);

        // 扩展访问（不常用）
        GraphModuleManager *GetModuleManager() { return module_manager; }
        RenderTargetManager *GetRenderTargetManager() { return rt_manager; }

        /// 回设 RenderTargetManager。
        /// GraphicsContext::Initialize() 无法用 module_manager->GetOrCreate 创建它
        /// （其构造需要 ECSContext），故由 RenderTargetManager 构造时自行回设；
        /// 不回设则 GetRenderTargetManager() 恒为 nullptr。
        void SetRenderTargetManager(RenderTargetManager *m) { rt_manager = m; }

    };

    // 向后兼容别名

    template<typename T>
    inline T *GraphicsContext::GetManager()
    {
        return nullptr;
    }

    template<>
    inline RenderPassManager *GraphicsContext::GetManager<RenderPassManager>()
    {
        return GetRenderPassManager();
    }

    template<>
    inline TextureManager *GraphicsContext::GetManager<TextureManager>()
    {
        return GetTextureManager();
    }

    template<>
    inline RenderTargetManager *GraphicsContext::GetManager<RenderTargetManager>()
    {
        return GetRenderTargetManager();
    }

    template<>
    inline ShaderProgramManager *GraphicsContext::GetManager<ShaderProgramManager>()
    {
        return GetMaterialManager();
    }

    template<>
    inline BufferManager *GraphicsContext::GetManager<BufferManager>()
    {
        return GetBufferManager();
    }

    template<>
    inline SamplerManager *GraphicsContext::GetManager<SamplerManager>()
    {
        return GetSamplerManager();
    }

    template<>
    inline GeometryManager *GraphicsContext::GetManager<GeometryManager>()
    {
        return GetGeometryManager();
    }

    template<>
    inline SSBOBufferRegistry *GraphicsContext::GetManager<SSBOBufferRegistry>()
    {
        return GetSSBOBufferRegistry();
    }

    template<>
    inline GlobalSSBOBufferRegistry *GraphicsContext::GetManager<GlobalSSBOBufferRegistry>()
    {
        return GetGlobalSSBOBufferRegistry();
    }

    template<>
    inline GraphModuleManager *GraphicsContext::GetManager<GraphModuleManager>()
    {
        return GetModuleManager();
    }

    template<>
    inline EnvironmentManager *GraphicsContext::GetManager<EnvironmentManager>()
    {
        return GetEnvironmentManager();
    }

    template<>
    inline BindlessTextureManager *GraphicsContext::GetManager<BindlessTextureManager>()
    {
        return GetBindlessTextureManager();
    }

} // namespace hgl::graph


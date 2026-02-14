#pragma once

#include<hgl/graph/render/RenderContext.h>

VK_NAMESPACE_BEGIN

namespace graph
{
    /**
     * RenderAPI: 高级渲染 API 外观
     * 
     * 提供简洁、直观的渲染资源创建接口，隐藏 Manager 的复杂性。
     * 
     * 设计原则:
     * 1. 透明: 依赖显式清晰，通过构造参数注入，不隐藏依赖
     * 2. 简洁: 常用操作提供简便方法
     * 3. 分层: 提供多个抽象级别
     *    - 简便层: String 参数
     *    - 标准层: 完整参数
     *    - 底层: 直接访问 Manager
     * 
     * 使用示例:
     * ```cpp
     * // 获取 RenderAPI
     * auto* api = render_context->GetAPI();  // 或单独创建
     * 
     * // 简便方式: 使用默认设置
     * auto mtl = api->CreateMaterial("DiffuseShading");
     * auto ubo = api->CreateUBO<CameraData>("camera_ubo");
     * auto tex = api->LoadTexture2D("textures/wood.png");
     * 
     * // 需要精细控制时，回到 RenderContext
     * auto ctx = api->GetContext();
     * auto manager = ctx->GetMaterialManager();
     * // ...
     * ```
     */
    class RenderAPI
    {
    private:
        // 核心上下文
        RenderContext* context;

        // 便捷缓存（加快查找）
        MaterialManager* mat_manager;
        BufferManager* buf_manager;
        TextureManager* tex_manager;
        SamplerManager* sampler_manager;
        RenderPassManager* rp_manager;
        GeometryManager* geo_manager;
        PrimitiveManager* prim_manager;

    public:
        /**
         * 构造函数：从 RenderContext 创建
         * @param ctx 渲染执行上下文
         */
        explicit RenderAPI(RenderContext* ctx);

        virtual ~RenderAPI() = default;

        // 禁用复制
        RenderAPI(const RenderAPI&) = delete;
        RenderAPI& operator=(const RenderAPI&) = delete;

    public:
        // ===== 材质 API =====

        /**
         * 创建材质
         * @param name 材质名称
         * @return 材质句柄，失败返回 nullptr
         */
        Material* CreateMaterial(const AnsiString& name) {
            return context->CreateMaterial(name);
        }

        /**
         * 加载材质
         * @param path 材质文件路径
         * @return 材质句柄，失败返回 nullptr
         */
        Material* LoadMaterial(const OSString& path) {
            return context->LoadMaterial(path);
        }

        /**
         * 创建材质实例
         * @param material 基材质
         * @return 材质实例句柄，失败返回 nullptr
         */
        MaterialInstance* CreateMaterialInstance(Material* material) {
            return context->CreateMaterialInstance(material);
        }

        /**
         * 创建材质实例（从名称查找）
         * @param material_name 材质名称
         * @return 材质实例句柄，失败返回 nullptr
         */
        MaterialInstance* CreateMaterialInstance(const AnsiString& material_name);

    public:
        // ===== 缓冲区 API =====

        /**
         * 创建强类型 UBO
         * @param name 缓冲区名称
         * @return 设备缓冲区指针，失败返回 nullptr
         * 
         * 使用示例:
         * ```cpp
         * struct CameraData { glm::mat4 view; glm::mat4 proj; };
         * auto ubo = api->CreateUBO<CameraData>("camera");
         * ```
         */
        template<typename T>
        DeviceBuffer* CreateUBO(const AnsiString& name) {
            return context->CreateUBO(name, sizeof(T));
        }

        /**
         * 创建 UBO（指定大小）
         * @param name 缓冲区名称
         * @param size 缓冲区大小（字节）
         * @return 设备缓冲区指针，失败返回 nullptr
         */
        DeviceBuffer* CreateUBO(const AnsiString& name, VkDeviceSize size) {
            return context->CreateUBO(name, size);
        }

        /**
         * 创建强类型 SSBO
         * @param name 缓冲区名称
         * @return 设备缓冲区指针，失败返回 nullptr
         */
        template<typename T>
        DeviceBuffer* CreateSSBO(const AnsiString& name) {
            return context->CreateSSBO(name, sizeof(T));
        }

        /**
         * 创建 SSBO（指定大小）
         * @param name 缓冲区名称
         * @param size 缓冲区大小（字节）
         * @return 设备缓冲区指针，失败返回 nullptr
         */
        DeviceBuffer* CreateSSBO(const AnsiString& name, VkDeviceSize size) {
            return context->CreateSSBO(name, size);
        }

        /**
         * 创建索引缓冲区
         * @param size 缓冲区大小
         * @param type 索引类型（U8/U16/U32）
         * @return 索引缓冲区指针，失败返回 nullptr
         */
        IndexBuffer* CreateIBO(VkDeviceSize size, IndexType type = IndexType::U32) {
            return context->CreateIBO(size, type);
        }

        /**
         * 创建 VAB（顶点属性缓冲区）
         * @param name 缓冲区名称
         * @param size 缓冲区大小
         * @return VAB 指针，失败返回 nullptr
         */
        VAB* CreateVAB(const AnsiString& name, VkDeviceSize size);

    public:
        // ===== 纹理 API =====

        /**
         * 加载 2D 纹理
         * @param path        纹理文件路径
         * @param auto_mipmap 是否自动生成 MipMap
         * @return 纹理句柄，失败返回 nullptr
         */
        Texture2D* LoadTexture2D(const OSString& path, bool auto_mipmap = true) {
            return context->LoadTexture2D(path, auto_mipmap);
        }

        /**
         * 加载立方体纹理
         * @param base_path    基础路径（6 张纹理）
         * @param auto_mipmaps 是否自动生成 MipMap
         * @return 纹理句柄，失败返回 nullptr
         */
        TextureCube* LoadTextureCube(const OSString& base_path, bool auto_mipmaps = false) {
            return context->LoadTextureCube(base_path, auto_mipmaps);
        }

        /**
         * 创建 2D 数组纹理
         * @param name         纹理名称
         * @param width        宽度
         * @param height       高度
         * @param layer_count  层数
         * @param fmt          像素格式
         * @param auto_mipmaps 是否自动生成 MipMap
         * @return 纹理句柄，失败返回 nullptr
         */
        Texture2DArray* CreateTexture2DArray(const AnsiString& name,
                                            uint32_t width,
                                            uint32_t height,
                                            uint32_t layer_count,
                                            VkFormat fmt,
                                            bool auto_mipmaps = false) {
            return context->CreateTexture2DArray(name, width, height, layer_count, fmt, auto_mipmaps);
        }

        /**
         * 向数组纹理加载层数据
         * @param tex2d_array 目标纹理数组
         * @param layer       层索引
         * @param path        纹理文件路径
         * @return 成功返回 true，失败返回 false
         */
        bool LoadTexture2DArray(Texture2DArray* tex2d_array, uint32_t layer, const OSString& path) {
            return context->LoadTexture2DArray(tex2d_array, layer, path);
        }

    public:
        // ===== 采样器 API =====

        /**
         * 创建采样器（默认设置）
         * @return 采样器指针，失败返回 nullptr
         */
        Sampler* CreateSampler() {
            return context->CreateSampler();
        }

        /**
         * 创建采样器（自定义设置）
         * @param create_info Vulkan 采样器创建信息
         * @return 采样器指针，失败返回 nullptr
         */
        Sampler* CreateSampler(VkSamplerCreateInfo* create_info) {
            return context->CreateSampler(create_info);
        }

        /**
         * 创建采样器（从纹理）
         * @param texture 纹理对象
         * @return 采样器指针，失败返回 nullptr
         */
        Sampler* CreateSamplerForTexture(Texture* texture) {
            return context->CreateSampler(texture);
        }

    public:
        // ===== 几何体和图元 API =====

        /**
         * 创建顶点数据管理器
         * @param vil            顶点输入配置
         * @param vertices_count 顶点数量
         * @param indices_count  索引数量
         * @param type           索引类型
         * @return 顶点数据管理器指针
         */
        VertexDataManager* CreateVDM(const VertexInputLayout* vil,
                                     VkDeviceSize vertices_count,
                                     VkDeviceSize indices_count,
                                     IndexType type = IndexType::U16) {
            return context->CreateVDM(vil, vertices_count, indices_count, type);
        }

        /**
         * 创建几何体
         * @param name      几何体名称
         * @param vert_count 顶点数量
         * @param vil       顶点输入配置
         * @param vad_list  顶点属性数据
         * @return 几何体指针
         */
        Geometry* CreateGeometry(const AnsiString& name,
                                uint32_t vert_count,
                                const VertexInputLayout* vil,
                                const std::initializer_list<VertexAttribDataPtr>& vad_list) {
            return context->CreateGeometry(name, vert_count, vil, vad_list);
        }

        /**
         * 创建图元
         * @param name      图元名称
         * @param vert_count 顶点数量
         * @param mi        材质实例
         * @param pipeline  管线
         * @param vad_list  顶点属性数据
         * @return 图元指针
         */
        Primitive* CreatePrimitive(const AnsiString& name,
                                  uint32_t vert_count,
                                  MaterialInstance* mi,
                                  Pipeline* pipeline,
                                  const std::initializer_list<VertexAttribDataPtr>& vad_list) {
            return context->CreatePrimitive(name, vert_count, mi, pipeline, vad_list);
        }

    public:
        // ===== 管线 API =====

        /**
         * 创建渲染管线
         * @param material 材质
         * @param vil      顶点输入配置
         * @param pd       管线数据
         * @param prim_restart 是否启用基元重启
         * @return 管线指针，失败返回 nullptr
         */
        Pipeline* CreatePipeline(Material* material,
                                const VertexInputLayout* vil,
                                const PipelineData* pd,
                                bool prim_restart = false) {
            return context->CreatePipeline(material, vil, pd, prim_restart);
        }

    public:
        // ===== 低级访问 =====
        // 仅在需要进行高级定制时使用

        /**
         * 获取底层渲染执行上下文
         * @return RenderContext 指针
         * 
         * 使用此方法进行需要更多控制的高级操作:
         * ```cpp
         * auto ctx = api->GetContext();
         * auto manager = ctx->GetMaterialManager();
         * // ... 直接使用 manager
         * ```
         */
        RenderContext* GetContext() const { 
            return context; 
        }

        /**
         * 获取 Vulkan 设备
         * @return VulkanDevice 指针
         */
        VulkanDevice* GetDevice() const { 
            return context->GetDevice(); 
        }

    }; // class RenderAPI

} // namespace graph

VK_NAMESPACE_END

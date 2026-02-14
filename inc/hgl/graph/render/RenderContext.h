#pragma once

#include<hgl/vk/VKDevice.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/RenderPassManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKVertexInput.h>
#include<hgl/vk/VKVertexInputLayout.h>

namespace hgl
{
    namespace graph
    {
        class VILConfig;

        namespace mtl
        {
            class MaterialCreateInfo;
            struct Material2DCreateConfig;
            struct Material3DCreateConfig;
        }

    /**
     * RenderContext: 渲染执行上下文
     * 
     * 职责:
     * - 提供统一的资源访问接口（替代直接访问 RenderFramework）
     * - 管理渲染命令缓冲区和渲染目标
     * - 抽象底层 Vulkan 细节
     * - 支持多场景、多渲染目标
     * 
     * 特点:
     * - 显式依赖注入（消除隐晦的全局依赖）
     * - 职责清晰分离
     * - 易于测试和扩展
     * - API 透明而非通过宏隐藏
     * 
     * 使用示例:
     * ```cpp
     * RenderContext* ctx = gpu_framework->GetRenderContext();
     * 
     * // 创建资源
     * Material* mtl = ctx->CreateMaterial("myMat");
     * DeviceBuffer* ubo = ctx->CreateUBO("myUBO", sizeof(MyUBO));
     * Texture2D* tex = ctx->LoadTexture2D("texture.png");
     * 
     * // 访问底层
     * auto mat_mgr = ctx->GetMaterialManager();
     * auto buf_mgr = ctx->GetBufferManager();
     * ```
     */
        class RenderContext
        {
    private:
        // 资源管理器（不直接持有，通过引用管理）
        VulkanDevice* device;
        TextureManager* texture_manager;
        BufferManager* buffer_manager;
        MaterialManager* material_manager;
        SamplerManager* sampler_manager;
        RenderPassManager* render_pass_manager;
        GeometryManager* geometry_manager;
        PrimitiveManager* primitive_manager;

        // 当前渲染状态
        RenderCmdBuffer* current_render_cmd_buf = nullptr;
        IRenderTarget* current_render_target = nullptr;

    public:
        /**
         * 构造函数：显式注入所有依赖
         * 
         * @param dev           Vulkan 设备
         * @param tex_mgr       纹理管理器
         * @param buf_mgr       缓冲区管理器
         * @param mat_mgr       材质管理器
         * @param sampler_mgr   采样器管理器
         * @param rp_mgr        RenderPass 管理器
         * @param geo_mgr       几何管理器
         * @param prim_mgr      图元管理器
         */
        RenderContext(VulkanDevice* dev,
                     TextureManager* tex_mgr,
                     BufferManager* buf_mgr,
                     MaterialManager* mat_mgr,
                     SamplerManager* sampler_mgr,
                     RenderPassManager* rp_mgr,
                     GeometryManager* geo_mgr = nullptr,
                     PrimitiveManager* prim_mgr = nullptr);

        virtual ~RenderContext() = default;

        // 禁用复制构造和赋值
        RenderContext(const RenderContext&) = delete;
        RenderContext& operator=(const RenderContext&) = delete;

    public:
        // ===== 资源创建接口 =====

        /**
         * 创建材质
         * @param name 材质名称
         * @return 材质对象指针，失败返回 nullptr
         */
        Material* CreateMaterial(const AnsiString& name);

        Material* CreateMaterial(const AnsiString& name, const mtl::MaterialCreateInfo* mci);

        /**
         * 加载材质（从文件）
         * @param path 材质文件路径
         * @return 材质对象指针，失败返回 nullptr
         */
        Material* LoadMaterial(const OSString& path);

        Material* LoadMaterial(const AnsiString& name, mtl::Material2DCreateConfig* cfg);
        Material* LoadMaterial(const AnsiString& name, mtl::Material3DCreateConfig* cfg);

        /**
         * 创建材质实例
         * @param material 基材质
         * @return 材质实例指针，失败返回 nullptr
         */
        MaterialInstance* CreateMaterialInstance(Material* material);

        MaterialInstance* CreateMaterialInstance(Material* material, const VIL* vil);
        MaterialInstance* CreateMaterialInstance(Material* material, const VILConfig* vil_cfg);
        MaterialInstance* CreateMaterialInstance(Material* material, const VIL* vil, const void* data, const uint32 data_size);
        MaterialInstance* CreateMaterialInstance(Material* material, const VILConfig* vil_cfg, const void* data, const uint32 data_size);

        template<typename T>
        MaterialInstance* CreateMaterialInstance(Material* material, const VIL* vil, const T* data)
        {
            return CreateMaterialInstance(material, vil, data, sizeof(T));
        }

        template<typename T>
        MaterialInstance* CreateMaterialInstance(Material* material, const VILConfig* vil_cfg, const T* data)
        {
            return CreateMaterialInstance(material, vil_cfg, data, sizeof(T));
        }

        MaterialInstance* CreateMaterialInstance(const AnsiString& name, const mtl::MaterialCreateInfo* mci, const VILConfig* vil_cfg = nullptr);
        MaterialInstance* CreateMaterialInstance(const AnsiString& name, const mtl::MaterialCreateInfo* mci, const VILConfig* vil_cfg, const void* data, const uint32 data_size);
        MaterialInstance* CreateMaterialInstance(const AnsiString& name, mtl::Material2DCreateConfig* cfg, const VILConfig* vil_cfg, const void* data, const uint32 data_size);
        MaterialInstance* CreateMaterialInstance(const AnsiString& name, mtl::Material2DCreateConfig* cfg, const VILConfig* vil_cfg = nullptr)
        {
            return CreateMaterialInstance(name, cfg, vil_cfg, nullptr, 0);
        }
        MaterialInstance* CreateMaterialInstance(const AnsiString& name, mtl::Material3DCreateConfig* cfg, const VILConfig* vil_cfg, const void* data, const uint32 data_size);
        MaterialInstance* CreateMaterialInstance(const AnsiString& name, mtl::Material3DCreateConfig* cfg, const VILConfig* vil_cfg = nullptr)
        {
            return CreateMaterialInstance(name, cfg, vil_cfg, nullptr, 0);
        }

        /**
         * 创建 UBO 缓冲区
         * @param name   缓冲区名称
         * @param size   缓冲区大小
         * @return 设备缓冲区指针，失败返回 nullptr
         */
        DeviceBuffer* CreateUBO(const AnsiString& name, VkDeviceSize size);

        /**
         * 创建 SSBO 缓冲区
         * @param name   缓冲区名称
         * @param size   缓冲区大小
         * @return 设备缓冲区指针，失败返回 nullptr
         */
        DeviceBuffer* CreateSSBO(const AnsiString& name, VkDeviceSize size);

        DeviceBuffer* CreateINBO(const AnsiString& name, VkDeviceSize size);

        /**
         * 创建索引缓冲区
         * @param size 缓冲区大小
         * @param type 索引类型
         * @return 索引缓冲区指针，失败返回 nullptr
         */
        IndexBuffer* CreateIBO(VkDeviceSize size, IndexType type = IndexType::U32);

        VAB* CreateVAB(VkFormat format, uint32_t count, const void* data);
        VAB* CreateVAB(VkFormat format, uint32_t count) { return CreateVAB(format, count, nullptr); }

        /**
         * 加载 2D 纹理
         * @param path       纹理文件路径
         * @param auto_mipmap 是否自动生成 MipMap
         * @return 纹理对象指针，失败返回 nullptr
         */
        Texture2D* LoadTexture2D(const OSString& path, bool auto_mipmap = true);

        /**
         * 加载立方体纹理
         * @param base_path    基础路径（包含 6 个面的纹理）
         * @param auto_mipmaps 是否自动生成 MipMap
         * @return 纹理对象指针，失败返回 nullptr
         */
        TextureCube* LoadTextureCube(const OSString& base_path, bool auto_mipmaps = false);

        /**
         * 创建 2D 数组纹理
         * @param name         纹理名称
         * @param width        宽度
         * @param height       高度
         * @param layer        层数
         * @param fmt          像素格式
         * @param auto_mipmaps 是否自动生成 MipMap
         * @return 纹理对象指针，失败返回 nullptr
         */
        Texture2DArray* CreateTexture2DArray(const AnsiString& name,
                                            uint32_t width,
                                            uint32_t height,
                                            uint32_t layer,
                                            VkFormat fmt,
                                            bool auto_mipmaps = false);

        /**
         * 向 2D 数组纹理加载指定层数据
         * @param tex2d_array 目标纹理数组
         * @param layer       层索引
         * @param path        纹理文件路径
         * @return 成功返回 true，失败返回 false
         */
        bool LoadTexture2DArray(Texture2DArray* tex2d_array,
                               uint32_t layer,
                               const OSString& path);

        /**
         * 创建采样器
         * @param create_info Vulkan 采样器创建信息，可为 nullptr（使用默认）
         * @return 采样器指针，失败返回 nullptr
         */
        Sampler* CreateSampler(VkSamplerCreateInfo* create_info = nullptr);

        /**
         * 创建采样器（从纹理）
         * @param texture 纹理对象
         * @return 采样器指针，失败返回 nullptr
         */
        Sampler* CreateSampler(Texture* texture);

        /**
         * 创建管线
         * @param material 材质
         * @param vil      顶点输入配置
         * @param cd       管线数据
         * @param prim_restart 是否启用基元重启
         * @return 管线指针，失败返回 nullptr
         */
        Pipeline* CreatePipeline(Material* material,
                                const VertexInputLayout* vil,
                                const PipelineData* pd,
                                bool prim_restart = false);

        /**
         * 创建顶点数据管理器
         * @param vil            顶点输入配置
         * @param vertices_count 顶点数量
         * @param indices_count  索引数量
         * @param type           索引类型
         * @return 顶点数据管理器指针，失败返回 nullptr
         */
        VertexDataManager* CreateVDM(const VertexInputLayout* vil,
                                     VkDeviceSize vertices_count,
                                     VkDeviceSize indices_count,
                                     IndexType type = IndexType::U16);

        /**
         * 创建几何体
         * @param name      几何体名称
         * @param vert_count 顶点数量
         * @param vil       顶点输入配置
         * @param vad_list  顶点属性数据列表
         * @return 几何体指针，失败返回 nullptr
         */
        Geometry* CreateGeometry(const AnsiString& name,
                                uint32_t vert_count,
                                const VertexInputLayout* vil,
                                const std::initializer_list<VertexAttribDataPtr>& vad_list);

        /**
         * 创建图元
         * @param name      图元名称
         * @param vert_count 顶点数量
         * @param mi        材质实例
         * @param pipeline  管线
         * @param vad_list  顶点属性数据列表
         * @return 图元指针，失败返回 nullptr
         */
        Primitive* CreatePrimitive(const AnsiString& name,
                                  uint32_t vert_count,
                                  MaterialInstance* mi,
                                  Pipeline* pipeline,
                                  const std::initializer_list<VertexAttribDataPtr>& vad_list);

    public:
        // ===== 渲染目标和命令缓冲区管理 =====

        /**
         * 设置当前渲染目标
         * @param rt 渲染目标
         */
        void SetCurrentRenderTarget(IRenderTarget* rt);

        /**
         * 获取当前渲染目标
         * @return 当前渲染目标指针，未设置返回 nullptr
         */
        IRenderTarget* GetCurrentRenderTarget() const;

        /**
         * 设置当前渲染命令缓冲区
         * @param cmd 渲染命令缓冲区
         */
        void SetCurrentRenderCmdBuffer(RenderCmdBuffer* cmd);

        /**
         * 获取当前渲染命令缓冲区
         * @return 当前渲染命令缓冲区指针，未设置返回 nullptr
         */
        RenderCmdBuffer* GetCurrentRenderCmdBuffer() const;

    public:
        // ===== 低级管理器访问 =====
        // 仅在需要精细控制时使用，应倾向使用上面的高级接口

        VulkanDevice* GetDevice() const { return device; }
        TextureManager* GetTextureManager() const { return texture_manager; }
        BufferManager* GetBufferManager() const { return buffer_manager; }
        MaterialManager* GetMaterialManager() const { return material_manager; }
        SamplerManager* GetSamplerManager() const { return sampler_manager; }
        RenderPassManager* GetRenderPassManager() const { return render_pass_manager; }
        GeometryManager* GetGeometryManager() const { return geometry_manager; }
        PrimitiveManager* GetPrimitiveManager() const { return primitive_manager; }

        }; // class RenderContext

    } // namespace graph
} // namespace hgl

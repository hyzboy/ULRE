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
#include<hgl/vk/StructuredBufferAccessor.h>
#include<hgl/graph/mtl/ShaderBufferSource.h>

namespace hgl
{
    namespace graph
    {
        class VILConfig;
        class IGraphicsContext;

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
      * // 渲染状态
      * ctx->SetCurrentRenderTarget(rt);
      * ctx->SetCurrentRenderCmdBuffer(cmd);
     * 
      * // 资源访问
      * auto graphics = ctx->GetGraphicsContext();
      * auto mat_mgr = graphics ? graphics->GetMaterialManager() : nullptr;
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

        IGraphicsContext* graphics_context = nullptr;

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
        // ===== 渲染状态相关接口 =====

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

        void SetGraphicsContext(IGraphicsContext* ctx) { graphics_context = ctx; }
        IGraphicsContext* GetGraphicsContext() const { return graphics_context; }

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

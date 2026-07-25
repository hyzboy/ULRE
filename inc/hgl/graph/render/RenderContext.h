#pragma once

#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKVertexInput.h>
#include<hgl/vk/VKVertexInputLayout.h>
#include<hgl/mtl/new/NewDescriptorSetLayoutFactory.h>
#include<hgl/vk/VKBindlessTextureManager.h>

namespace hgl
{
    namespace graph
    {
        class VILConfig;
        class GraphicsContext;

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
    * - 管理渲染命令缓冲区和渲染目标
    * - 提供帧/Pass相关的渲染状态
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
        GraphicsContext* graphics_context = nullptr;

        // 当前渲染状态
        RenderCmdBuffer* current_render_cmd_buf = nullptr;
        IRenderTarget* current_render_target = nullptr;

        // 新 4-Set 管线布局 (与旧布局双轨共存)
        NewPipelineLayoutData* new_pipeline_layout_data_ = nullptr;

        // 全局 Bindless 纹理管理器（Set 4）
        BindlessTextureManager* bindless_texture_manager_ = nullptr;

    public:
        RenderContext() = default;

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
        Pipeline* CreatePipeline(MaterialProgram* material,
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
        void SetGraphicsContext(GraphicsContext* ctx) { graphics_context = ctx; }
        GraphicsContext* GetGraphicsContext() const { return graphics_context; }

    public:
        // ===== 新 4-Set 管线布局 (双轨共存) =====
        void SetNewPipelineLayoutData(NewPipelineLayoutData* pld) { new_pipeline_layout_data_ = pld; }
        NewPipelineLayoutData* GetNewPipelineLayoutData() const { return new_pipeline_layout_data_; }
        VkPipelineLayout GetNewPipelineLayout() const { return new_pipeline_layout_data_ ? new_pipeline_layout_data_->pipeline_layout : VK_NULL_HANDLE; }

    public:
        // ===== 全局 Bindless 纹理管理器 =====
        void SetBindlessTextureManager(BindlessTextureManager* mgr) { bindless_texture_manager_ = mgr; }
        BindlessTextureManager* GetBindlessTextureManager() const { return bindless_texture_manager_; }

        }; // class RenderContext

    } // namespace graph
} // namespace hgl

#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/mtl/ShaderResourceSchema.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/type/String.h>
#include<cstdint>
#include<vector>
#include<unordered_map>
#include<unordered_set>

namespace hgl::graph {
    template<typename> class StructuredBufferAccessor;
    struct ViewportInfo;
    class DeviceBuffer;
}

#ifndef ULRE_ECS_DEBUG_API
#define ULRE_ECS_DEBUG_API 1
#endif

namespace hgl::graph
{
    class RenderCmdBuffer;
    class ShaderProgram;
    class IRenderTarget;
    class IGPUBuffer;
    class Texture;
    class Sampler;
    class BindlessTextureManager;
}

namespace hgl::ecs
{
    class RenderItem;

    /**
     * RenderSceneUBOSystem（原 RenderDescriptorBindingSystem，2026-09-08 改名）
     *
     * BDA 终态后描述符绑定已全部退场，本系统只剩场景 UBO 数据流与资源注册职责：
     *   1. 持有 viewport UBO（跨 swapchain resize 稳定），RenderFrameSync 阶段把
     *      camera/sky/viewport buffer 挂进设备级 GlobalSceneUBOSet（一帧一次）；
     *   2. 材质化注册：纹理 bindless handle / 材质行结构 layout 登记
     *      （RegisterTextureResource / RegisterMaterialStructLayout，被
     *      RenderPrimitiveCollectSystem 消费）。
     */
    class RenderSceneUBOSystem : public System
    {
    private:

        // Viewport UBO — owned here, stable across swapchain resize.
        graph::StructuredBufferAccessor<graph::ViewportInfo> *viewport_ubo = nullptr;
        uint32_t pending_viewport_width  = 0;
        uint32_t pending_viewport_height = 0;
        // resource_id → bindless descriptor index (1-based, 0 = not found).
        // Filled by RegisterTexture2D(Array)Resource; consumed by
        // RenderPrimitiveCollectSystem::MaterializeRecipeRowsForPrimitive to
        // build per-primitive texture layer rows without a shared spec/cache.
        hgl::UnorderedMap<AnsiString, uint32_t> materialization_resource_handles;

    public:

        RenderSceneUBOSystem(const std::string& name = "RenderSceneUBOSystem");
        ~RenderSceneUBOSystem() override;

        graph::ViewportInfo *GetViewportInfo();
        void SetViewportExtent(uint32_t w, uint32_t h);

        // ViewUBOCommitSystem 专用：pass 开始时无条件全量写入 viewport UBO
        void CommitViewportUBO();

        void Update(float deltaTime) override;
        void Render(graph::RenderCmdBuffer *cmd, float deltaTime) override;

        bool RegisterMaterialStructLayout(graph::mtl::SSBOType ssbo_type,
                                          uint32_t ssbo_id,
                                          uint32_t byte_stride);

        /**
         * 便利版本：向 BindlessTextureManager 注册纹理并预注册 pool 映射。
         * 所有纹理（2D / 2DArray）统一注册到 sampler2DArray[]（binding=0）；2D 走单层 array view。
         * sampler 已由统一 Sampler 注册机制（binding=1）按名字提供，此处只注册纹理本身。
         * @return 1-based tex_handle，失败返回 0
         */
        uint32_t RegisterTextureResource(const std::string &resource_id,
                                         graph::Texture *tex,
                                         graph::BindlessTextureManager *bindless_mgr);

        /**
         * 查询 resource_id 对应的 bindless descriptor index（1-based）。
         * 注册期间 RegisterTextureResource 已把 handle 存入映射；
         * 未注册/不存在返回 0。MaterializeRecipeRowsForPrimitive 用它
         * 把纹理 handle 写入引擎管理的纹理行表域 SSBO。
         */
        uint32_t GetBindlessHandle(const AnsiString &resource_id) const;

    private:

        void EnsureViewportUBO();
        void ReleaseViewportUBO();
        void SyncBindingsForCurrentCommand();
        void ApplyResourceLayoutBindings();
        const graph::IGPUBuffer *ResolveViewportUBO() const;
        const graph::IGPUBuffer *ResolveCameraUBO() const;
        const graph::IGPUBuffer *ResolveSkyUBO();
    };
}

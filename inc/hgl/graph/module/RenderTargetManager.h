#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/render/RenderTargetDesc.h>
#include<memory>
#include<vector>

namespace hgl::ecs
{
    class ECSContext;
}

namespace hgl::graph{

class TextureManager;
class RenderPassManager;
class GraphicsContext;
class OffscreenRenderTarget;
struct RenderTargetData;

class RenderTargetManager;

/**
 * RenderTarget 的 RAII 句柄
 *
 * 释放时自动向创建它的 RenderTargetManager 注销并销毁 RT，
 * 调用方不再需要（也不应该）手动 delete。
 */
struct RenderTargetDeleter
{
    RenderTargetManager *manager = nullptr;

    void operator()(IRenderTarget *rt) const;   // 定义见 RenderTargetManager.cpp
};

using RenderTargetHandle = std::unique_ptr<IRenderTarget, RenderTargetDeleter>;

/**
 * 渲染目标管理器
 *
 * 职责：
 * - 唯一的离屏 RenderTarget 创建入口：`Create(const RenderTargetDesc &)`
 * - 持有并追踪所有由此创建的 RenderTarget（registry），提供统一回收与泄漏统计
 *
 * 所有权约定：
 * - 由本 Manager 创建的 RenderTarget 归 Manager 所有
 * - 推荐使用 RenderTargetHandle（RAII）；裸指针场景用 Destroy()，不得手动 delete
 */
GRAPH_MODULE_CLASS(RenderTargetManager)
{
    TextureManager *tex_manager;
    RenderPassManager *rp_manager;
    hgl::ecs::ECSContext *ecs_context=nullptr;

public:

    /// RT 注册表条目。持有 RenderTargetDesc，以支持按 desc 重建（resize）
    struct RenderTargetEntry
    {
        AnsiString       name;
        IRenderTarget    *rt=nullptr;
        RenderTargetDesc desc;
    };

private:

    std::vector<RenderTargetEntry> registry;

    /// 创建/重建纹理与 FBO 并写入 data。
    /// data 的 queue / cmd_buf / render_complete_semaphore 由调用方负责（重建时复用，
    /// 不重复创建，避免设备侧对象累积泄漏）。
    bool CreateAttachments(RenderTargetData *data,const AnsiString &name,const FramebufferInfo *fbi);

    /// 按新尺寸重建指定 RT 的纹理与 FBO
    bool RebuildOffscreenRT(OffscreenRenderTarget *rt,const RenderTargetDesc &desc);

public:

    RenderTargetManager(GraphicsContext *gc,hgl::ecs::ECSContext *ecs_ctx,TextureManager *tm,RenderPassManager *rpm);

    /// 析构时兜底回收（等价于 Release()，防止无人调用 Release 时泄漏）
    virtual ~RenderTargetManager();

public: //FrameBuffer相关

    Framebuffer *CreateFBO(RenderPass *rp,ImageView **color_list,const uint image_count,ImageView *depth);
    Framebuffer *CreateFBO(RenderPass *,ImageView *color,ImageView *depth);
    Framebuffer *CreateFBO(RenderPass *,ImageView *);

public: // 创建

    /// [标准入口] 按描述子创建离屏渲染目标，返回 RAII 句柄
    /// @param desc 描述子；未指定的格式字段由本函数按设备默认值解析
    /// @return 成功返回非空句柄；失败返回空句柄（desc 非法、设备/管理器缺失、附件创建失败）
    RenderTargetHandle Create(const RenderTargetDesc &desc);

    /// 内部实现：按已解析好的 FramebufferInfo 创建并登记
    /// @note 供 Create() 与旧的静态便捷入口使用，新代码请用 Create()
    OffscreenRenderTarget *CreateOffscreenRT(hgl::ecs::ECSContext *ecs_ctx,
                                             const AnsiString &name,
                                             const FramebufferInfo *fbi,
                                             const uint32_t fence_count=1);

    /// [Deprecated] 便捷静态入口，保留以兼容旧调用；新代码请用 Create(desc)
    static OffscreenRenderTarget *CreateRTFromGraphicsContext(GraphicsContext *gc, hgl::ecs::ECSContext *ecs_ctx,
                                                              const FramebufferInfo *fbi, const uint32_t fence_count=1);
    static OffscreenRenderTarget *CreateRTFromGraphicsContext(GraphicsContext *gc, hgl::ecs::ECSContext *ecs_ctx,
                                                              const AnsiString &name, const FramebufferInfo *fbi, const uint32_t fence_count=1);

public: // 生命周期

    /// 按新尺寸重建指定 RT（纹理与 FBO 重建；queue/cmd_buf/semaphore 复用）
    ///
    /// @warning 重建后 Texture2D 指针会变化，持有旧指针者必须重新
    ///          GetColorTexture() / GetDepthTexture() 并重新绑定材质
    /// @return 成功返回 true；RT 未登记 / desc.resizable 为假 / 重建失败返回 false
    bool Resize(IRenderTarget *rt,const uint32_t width,const uint32_t height);

    /// 销毁并注销指定的 RenderTarget（幂等；未登记者返回 false）
    bool Destroy(IRenderTarget *rt);

    /// 当前仍存活（未销毁）的 RT 数量，用于泄漏追踪
    uint32_t GetAliveCount()const{return static_cast<uint32_t>(registry.size());}

    /// 按名称查找已登记的 RT
    IRenderTarget *Find(const AnsiString &name)const;

    /// 统一回收所有仍登记的 RT（由 GraphModuleManager 在销毁前调用）
    void Release() override;

    /// 窗口尺寸改变：阶段 D 起按 desc 重建 resizable 的 RT，当前为 no-op 占位
    void OnResize(const VkExtent2D &extent) override;
};//class RenderTargetManager

}//namespace hgl::graph

#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<vector>

namespace hgl::ecs
{
    class ECSContext;
}

namespace hgl::graph{

class TextureManager;
class RenderPassManager;
class GraphicsContext;
class RenderTarget;

/**
 * 渲染目标管理器
 *
 * 职责（标准化阶段 A 起）：
 * - 唯一的离屏 RenderTarget 创建入口
 * - 持有并追踪所有由此创建的 RenderTarget（registry），提供统一回收与泄漏统计
 *
 * 所有权约定：
 * - 由本 Manager 创建的 RenderTarget 归 Manager 所有
 * - 调用方不得再手动 delete，应使用 Destroy()；Manager 的 Release() 统一回收
 */
GRAPH_MODULE_CLASS(RenderTargetManager)
{
    TextureManager *tex_manager;
    RenderPassManager *rp_manager;
    hgl::ecs::ECSContext *ecs_context=nullptr;

public:

    /// RT 注册表条目。阶段 A 仅追踪名称与指针；
    /// 阶段 B 引入 RenderTargetDesc 后改为持有 desc，以支持按 desc 重建（resize）。
    struct RenderTargetEntry
    {
        AnsiString   name;
        RenderTarget *rt=nullptr;
    };

private:

    std::vector<RenderTargetEntry> registry;

public:

    RenderTargetManager(GraphicsContext *gc,hgl::ecs::ECSContext *ecs_ctx,TextureManager *tm,RenderPassManager *rpm);

    /// 析构时兜底回收（等价于 Release()，防止无人调用 Release 时泄漏）
    virtual ~RenderTargetManager();

public: //FrameBuffer相关

    Framebuffer *CreateFBO(RenderPass *rp,ImageView **color_list,const uint image_count,ImageView *depth);
    Framebuffer *CreateFBO(RenderPass *,ImageView *color,ImageView *depth);
    Framebuffer *CreateFBO(RenderPass *,ImageView *);

public:

    /// 离屏 RT 创建实现（实例方法）：创建成功后登记入 registry
    RenderTarget *CreateOffscreenRT(hgl::ecs::ECSContext *ecs_ctx,
                                    const AnsiString &name,
                                    const FramebufferInfo *fbi,
                                    const uint32_t fence_count=1);

    /// 便捷静态入口：由 GraphicsContext 定位本 Manager 后委托给 CreateOffscreenRT
    /// @note 阶段 B 收敛为 Create(const RenderTargetDesc &) 后移除
    static RenderTarget *CreateRTFromGraphicsContext(GraphicsContext *gc, hgl::ecs::ECSContext *ecs_ctx,
                                                     const FramebufferInfo *fbi, const uint32_t fence_count=1);
    static RenderTarget *CreateRTFromGraphicsContext(GraphicsContext *gc, hgl::ecs::ECSContext *ecs_ctx,
                                                     const AnsiString &name, const FramebufferInfo *fbi, const uint32_t fence_count=1);

public: // 生命周期

    /// 销毁并注销指定的 RenderTarget（幂等；未登记者返回 false）
    bool Destroy(RenderTarget *rt);

    /// 当前仍存活的 RT 数量，用于泄漏追踪
    uint32_t GetAliveCount()const{return static_cast<uint32_t>(registry.size());}

    /// 按名称查找已登记的 RT
    RenderTarget *Find(const AnsiString &name)const;

    /// 统一回收所有仍登记的 RT（由 GraphModuleManager 在销毁前调用）
    void Release() override;

    /// 窗口尺寸改变：阶段 D 起按 desc 重建 resizable 的 RT，当前为 no-op 占位
    void OnResize(const VkExtent2D &extent) override;
};//class RenderTargetManager

}//namespace hgl::graph

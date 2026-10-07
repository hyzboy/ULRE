#include<hgl/ecs/systems/render/ViewUBOCommitSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/vk/VKRenderTarget.h>

namespace hgl::ecs
{
    ViewUBOCommitSystem::ViewUBOCommitSystem(const std::string &name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderBufferCommit);
    }

    void ViewUBOCommitSystem::Update(float /*deltaTime*/)
    {
        if (!context)
            return;

        if (auto rdb = context->GetSystem<RenderSceneUBOSystem>())
            rdb->CommitViewportUBO();

        // 本世界的影子数据（T6b/P4：世界私有，不再经 env profile）的上传点 = **本世界自己的帧 RT**：
        //   · 主世界交换链帧：acquire 之后写本帧槽（该槽上一帧已在 NextFrame 等完）；
        //   · **离屏世界（OffscreenWorld）的主帧**：同样写它自己 RT 的当前槽
        //     （槽数 1 ⇒ 上一帧已在 BeginRender 等过该槽 fence）。不加这一支的话，离屏世界的
        //     `addr_shadow` 永远停在建表时的零值 ⇒ **离屏世界不可能有阴影**（P4 遗留：提交点曾写成
        //     "仅交换链"，因为当时影子数据还挂在 profile 的 ring 上）。
        // 中间 pass 会把自己的身份标出来（`RenderTo(req)` 的 CSM 级联 / 同世界异目标 pass ⇒
        // `IsCurrentPassShadow()`）：那里写会踩在飞的主帧槽 ⇒ 跳过。
        // 天空（sky）是单份 buffer：数据变时 `MarkDirty` 即时写，不需要每帧重提。
        if (auto *rt = context->GetRenderTarget())
        {
            if (!context->IsCurrentPassShadow())
                context->CommitShadowInfo(rt->GetCurrentFrameIndex());
        }
    }
}//namespace hgl::ecs

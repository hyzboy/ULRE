#include <hgl/graph/render/RenderContext.h>

namespace hgl::graph
{
    // 原 SetCurrentRenderTarget / GetCurrentRenderTarget /
    // SetCurrentRenderCmdBuffer / GetCurrentRenderCmdBuffer 已删除：
    // 这两份状态与 ECSContext::render_target / current_render_cmd 完全重复，
    // 靠 RenderTargetSystem 每帧同步维持一致（不同步就是跨 Pass 管线误用 bug
    // 的根源，见 c778f1fb2）。权威唯一化到 ECSContext，消费者一律改走
    // ECSContext::GetRenderTarget() / GetCurrentRenderCmd()。
} // namespace hgl::graph

#pragma once

/**
 * RenderTargetDesc —— 渲染目标的声明式描述
 *
 * 标准化阶段 B 引入。目标：把散落在各处的"取设备格式 + 拼 FramebufferInfo +
 * 单独设清屏色 + 单独设环境"收敛为一份可复用的描述。
 *
 * 设计约定：
 * - has_color 为真且 color_formats 为空 → 使用设备默认 surface format 单颜色附件
 * - has_color 为假 → 零颜色附件（depth-only，如 shadow map），此时 color_formats 必须为空
 * - depth_format 为 PF_UNDEFINED 且 has_depth 为真 → 使用设备默认深度格式
 * - 至少要有一种附件（has_color 与 has_depth 不能同时为假）
 * - 设备相关字段（默认格式）在 RenderTargetManager::Create 内解析，desc 只表达意图
 */

#include<hgl/vk/VK.h>
#include<hgl/graph/ubo/EnvironmentInfo.h>
#include<hgl/color/Color4f.h>
#include<vector>

namespace hgl::graph{

enum class RenderTargetKind : uint8
{
    Offscreen = 0,   ///< 离屏纹理（由 RenderTargetManager 创建并持有）
    Swapchain = 1,   ///< 窗口交换链（由 SwapchainModule 创建）
};

struct RenderTargetDesc
{
    // ---- 基本 ----

    RenderTargetKind kind = RenderTargetKind::Offscreen;

    /// 资源名前缀，参与 Texture / Framebuffer / Queue 的命名，用于 GPU 对象追踪
    AnsiString name;

    uint32_t width  = 0;
    uint32_t height = 0;

    // ---- 附件 ----

    /// 颜色附件格式列表；has_color 为真且此处为空时，使用设备默认 surface format 的单附件
    std::vector<VkFormat> color_formats;

    /// 是否有颜色附件。false 表示零颜色附件（depth-only，如 shadow map），
    /// 此时 color_formats 必须为空——Create 不会补设备默认格式。
    bool has_color = true;

    /// 深度格式；PF_UNDEFINED 且 has_depth 为真时使用设备默认深度格式
    VkFormat depth_format = PF_UNDEFINED;

    bool has_depth = true;

    /// MSAA 采样数。当前 Create 仅支持 1（MSAA 留待阶段 D 实现，非 1 时创建失败）
    uint32_t samples = 1;

    // ---- 行为 ----

    /// 是否允许 Resize() 按本 desc 重建
    bool resizable = true;

    /// 是否在窗口尺寸变化时跟随重建（GraphicsContext::OnResize 会遍历这类 RT）。
    /// 默认 false —— 离屏 RT 尺寸通常与窗口无关（如固定 512x512 的 RTT），
    /// 不应因窗口缩放被连带改变。
    bool follow_window = false;

    /// in-flight 帧槽数（A7）。
    ///
    /// 每个槽独占一组 {cmd_buf, queue(1 fence), render_complete_semaphore}，按提交次数
    /// 轮转；复用某槽前等该槽自己的 fence（标准 WSI 模型）。1 = 每次提交前都等上一次
    /// 提交完成（旧行为，零重叠）；每帧对同一 RT 提交多次时（如 CSM 每级一次）应设为
    /// ≥ 每帧提交次数，才能换来真正的 CPU/GPU 重叠。
    ///
    /// 本值同时决定该 RT 占用的 per-frame 数据槽带宽度（见 RenderOptions.h 的槽划分）。
    uint32_t slot_count = 1;

    // ---- 渲染参数（原散落在 WorkObject / ECSContext / RenderSystemCore 三处）----

    Color4f      clear_color{0,0,0,1};
    EnvProfileID env_profile = kEnvProfileDefault;

public:

    /// 离屏 + 单颜色 + 深度（最常见的 RTT 场景）
    static RenderTargetDesc OffscreenColorDepth(uint32_t w, uint32_t h, const AnsiString &name = {}, VkFormat depth_fmt = PF_UNDEFINED)
    {
        RenderTargetDesc d;
        d.kind   = RenderTargetKind::Offscreen;
        d.name   = name;
        d.width  = w;
        d.height = h;
        d.has_color    = true;
        d.has_depth    = true;
        d.depth_format = depth_fmt;
        return d;
    }

    /// 离屏 + 单颜色 + 无深度（后处理等场景）
    static RenderTargetDesc OffscreenColorOnly(uint32_t w, uint32_t h, const AnsiString &name = {})
    {
        RenderTargetDesc d;
        d.kind   = RenderTargetKind::Offscreen;
        d.name   = name;
        d.width  = w;
        d.height = h;
        d.has_color = true;
        d.has_depth = false;
        return d;
    }

    /// 离屏 + 仅深度（shadow map 等）：零颜色附件。
    /// 渲染后深度会被转到可采样布局（DEPTH_STENCIL_READ_ONLY_OPTIMAL），可直接绑定采样。
    static RenderTargetDesc OffscreenDepthOnly(uint32_t w, uint32_t h, const AnsiString &name = {}, VkFormat depth_fmt = PF_UNDEFINED)
    {
        RenderTargetDesc d;
        d.kind   = RenderTargetKind::Offscreen;
        d.name   = name;
        d.width  = w;
        d.height = h;
        d.has_color    = false;
        d.has_depth    = true;
        d.depth_format = depth_fmt;
        return d;
    }

public:

    bool IsValid()const
    {
        if(width == 0 || height == 0)
            return(false);

        if(samples != 1)
            return(false);          // MSAA 尚未实现（阶段 D）

        if(!has_color && !has_depth)
            return(false);          // 既无颜色也无深度——空附件没有意义

        if(!has_color && !color_formats.empty())
            return(false);          // 声明无颜色却给了颜色格式

        if(!has_depth && depth_format != PF_UNDEFINED)
            return(false);          // 无深度却指定了深度格式

        for(const VkFormat fmt : color_formats)
        {
            if(IsDepthFormat(fmt) || IsStencilFormat(fmt))
                return(false);      // 深度/模板格式不能作为颜色附件
        }

        return(true);
    }

    uint32_t GetColorCount()const
    {
        return static_cast<uint32_t>(color_formats.size());
    }
};//struct RenderTargetDesc

}//namespace hgl::graph

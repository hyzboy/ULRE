#pragma once

/**
 * RenderTargetDesc —— 渲染目标的声明式描述
 *
 * 标准化阶段 B 引入。目标：把散落在各处的"取设备格式 + 拼 FramebufferInfo +
 * 单独设清屏色 + 单独设环境"收敛为一份可复用的描述。
 *
 * 设计约定：
 * - color_formats 为空  → 使用设备默认 surface format 单颜色附件
 * - depth_format 为 PF_UNDEFINED 且 has_depth 为真 → 使用设备默认深度格式
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

    /// 颜色附件格式列表；为空表示使用设备默认 surface format 的单附件
    std::vector<VkFormat> color_formats;

    /// 深度格式；PF_UNDEFINED 且 has_depth 为真时使用设备默认深度格式
    VkFormat depth_format = PF_UNDEFINED;

    bool has_depth = true;

    /// MSAA 采样数。当前 Create 仅支持 1（MSAA 留待阶段 D 实现，非 1 时创建失败）
    uint32_t samples = 1;

    // ---- 行为 ----

    /// 是否允许 OnResize 时按本 desc 重建（阶段 D 生效）
    bool resizable = true;

    uint32_t fence_count = 1;

    // ---- 渲染参数（原散落在 WorkObject / ECSContext / RenderSystemCore 三处）----

    Color4f      clear_color{0,0,0,1};
    EnvProfileID env_profile = kEnvProfileDefault;

public:

    /// 离屏 + 单颜色 + 深度（最常见的 RTT 场景）
    static RenderTargetDesc OffscreenColorDepth(uint32_t w, uint32_t h, const AnsiString &name = {})
    {
        RenderTargetDesc d;
        d.kind   = RenderTargetKind::Offscreen;
        d.name   = name;
        d.width  = w;
        d.height = h;
        d.has_depth = true;
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
        d.has_depth = false;
        return d;
    }

public:

    bool IsValid()const
    {
        if(width == 0 || height == 0)
            return(false);

        if(samples != 1)
            return(false);          // MSAA 尚未实现（阶段 D）

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

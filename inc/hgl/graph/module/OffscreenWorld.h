#pragma once

/**
 * OffscreenWorld —— 离屏世界（子世界）的标准设施
 *
 * 标准化阶段 C：把原先散落在 example/common/OffscreenWorldRuntime.h 的
 * "离屏 RT + 独立 ECSContext + 手工注册系统 + 手写十步渲染"收进引擎，
 * 应用侧只需描述需求即可得到一个可渲染的离屏世界。
 *
 * 典型用法：
 * @code
 *   graph::OffscreenWorldDesc d;
 *   d.width = d.height = 512;
 *   d.resource_prefix = "RTT:Offscreen";
 *   d.clear_color = GetColor4f(COLOR::LightSkyBlue, 1.0f);
 *
 *   auto offscreen = graph::OffscreenWorld::Create(gc, main_world, d);
 *
 *   Texture2D *tex = offscreen->GetColorTexture(0);
 *   offscreen->Render();            // 用 RT 上声明的清屏色
 * @endcode
 */

#include<hgl/graph/render/RenderTargetDesc.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/core/RenderPassRequest.h>

#include<memory>
#include<string>

namespace hgl::graph
{
    class GraphicsContext;
    class Texture2D;

    struct OffscreenWorldDesc
    {
        uint32_t width  = 512;
        uint32_t height = 512;

        /// 子世界名（ECSContext 名）
        std::string name = "OffscreenWorld";

        /// GPU 资源命名前缀，用于分层追踪（同时作为 RT 名）
        std::string resource_prefix = "OffscreenRT";

        /// 清屏色：写入 RenderTargetDesc，成为 RT 上的权威值
        /// @note depth_only 为真时本值无意义（没有颜色附件）
        Color4f clear_color{0,0,0,1};

        /// 环境 Profile：写入 RenderTargetDesc
        EnvProfileID env_profile = kEnvProfileDefault;

        /// 仅深度附件（shadow map 等）：零颜色附件，深度渲染后处于可采样布局，
        /// 可直接 GetDepthTexture() 绑定采样
        bool depth_only = false;

        /// 本世界 pass 的光栅化剔除模式（显式声明，引擎不做隐式推断）。
        /// @note 目标是否为深度-only 与剔除模式**无关**：深度用途若需要“渲染模型
        ///       背面”（阴影贴图），必须在这里显式写 ecs::CullMode::Front；
        ///       默认 Inherit 沿用材质配置。
        ecs::CullMode cull_mode = ecs::CullMode::Inherit;

        /// 深度格式；PF_UNDEFINED 表示由设备默认深度格式决定
        VkFormat depth_format = PF_UNDEFINED;

        bool register_input_system  = false;
        bool register_camera_system = true;
    };

    /**
     * 离屏世界：持有离屏 RenderTarget + 独立 ECSContext + 必要的渲染系统
     *
     * 所有权：
     * - RT 由 RenderTargetHandle 持有（归 RenderTargetManager 管理）
     * - ECSContext 由本对象持有，析构时 Shutdown 后释放
     *
     * 渲染：内部调用 ECSContext::RenderTo()，与主窗口路径共用同一套帧驱动
     */
    class OffscreenWorld
    {
    public:

        /// 创建离屏世界
        /// @param gc         图形上下文（与主世界共享）
        /// @param main_world 主世界，可选；提供时用于接线 RenderContext 并在渲染后恢复其 RT
        /// @param desc       描述
        /// @return 成功返回非空指针；失败返回 nullptr
        static std::unique_ptr<OffscreenWorld> Create(GraphicsContext *gc,
                                                      ecs::ECSContext *main_world,
                                                      const OffscreenWorldDesc &desc = {});

        ~OffscreenWorld();

        bool IsValid()const{return world_ && rt_;}

    public: // 渲染

        /// Tick + 渲染一帧，使用 RT 上声明的清屏色
        void Render(float delta_time = 0.0f);

        /// Tick + 渲染一帧，指定清屏色（覆盖 desc 中的声明）
        void Render(const Color4f &clear_color, float delta_time = 0.0f);

        /// 调整离屏 RT 尺寸（按 desc 重建纹理与 FBO）
        ///
        /// @warning 重建后 Texture2D 指针会变化：持有旧指针者必须重新
        ///          GetColorTexture() / GetDepthTexture() 并重新绑定材质，
        ///          然后重新 Render() 才有内容。
        /// @return 成功返回 true
        bool Resize(const uint32_t width, const uint32_t height);

    public: // 访问

        IRenderTarget * GetRenderTarget ()const{return rt_.get();}
        Texture2D *     GetColorTexture (const uint32_t index = 0)const;
        Texture2D *     GetDepthTexture ()const;

        ecs::ECSContext *                       GetWorld        ()const{return world_.get();}
        std::shared_ptr<ecs::CameraSystem>      GetCameraSystem ()const{return camera_system_;}

    private:

        OffscreenWorld()=default;

        bool Init(GraphicsContext *gc, ecs::ECSContext *main_world, const OffscreenWorldDesc &desc);

        /// 子世界的 RenderTargetSystem 会改写共享 RenderContext 的 current RT，
        /// 渲染结束后需恢复为主世界的 RT，避免主世界下一帧用到已销毁/错误的 RT
        void RestoreMainRenderContext();

    private:

        GraphicsContext *   gc_              = nullptr;
        ecs::ECSContext *   main_world_      = nullptr;
        RenderContext *     render_context_  = nullptr;

        /// RT 的 RAII 句柄；声明在 world_ 之前，故析构时晚于 world_ 释放
        RenderTargetHandle  rt_;

        /// 本世界 pass 的剔除模式（取自 desc，显式声明）
        ecs::CullMode       cull_mode_       = ecs::CullMode::Inherit;

        std::unique_ptr<ecs::ECSContext> world_;

        std::shared_ptr<ecs::RenderTargetSystem>            rt_system_;
        std::shared_ptr<ecs::RenderPrimitiveCollectSystem>  collect_system_;
        std::shared_ptr<ecs::RenderSceneUBOSystem>          ubo_system_;
        std::shared_ptr<ecs::CameraSystem>                  camera_system_;
    };//class OffscreenWorld

}//namespace hgl::graph

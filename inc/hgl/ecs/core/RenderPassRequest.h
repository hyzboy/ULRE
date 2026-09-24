#pragma once

#include<hgl/color/Color4f.h>
#include<vulkan/vulkan.h>

namespace hgl
{
    namespace graph
    {
        class IRenderTarget;
    }

    namespace ecs
    {
        class CameraComponent;

        /**
         * RenderPassRequest —— pass 级渲染请求的一等描述（RT 标准化 §3.4 收官）
         *
         * `ECSContext::RenderTo(request)` 是离屏/子 pass 的标准入口：
         * 一个 world 的一帧渲到指定 RT，可选指定本 pass 生效的相机。
         *
         * 典型用例（shadow map）：
         * @code
         *   ecs::RenderPassRequest req;
         *   req.target      = shadow_rt;
         *   req.camera      = light_camera;   // pass 级相机覆盖
         *   world->RenderTo(req);
         * @endcode
         */
        struct RenderPassRequest
        {
            /// 本 pass 渲染目标（必填）
            graph::IRenderTarget *target = nullptr;

            /// 清屏色覆盖。use_target_clear 为真（默认）时忽略本字段，
            /// 采用 target 上声明的值（RenderTargetDesc::clear_color / SetClearColor）
            Color4f clear{0,0,0,1};
            bool use_target_clear = true;

            float delta_time = 0.0f;

            /// pass 级相机覆盖：nullptr = 本世界主相机（is_main_camera）。
            /// 非空时本 pass 用它解算共享相机数据（CameraSystem::Update 期间
            /// 只处理该相机、强制重算矩阵、不吃用户输入）；pass 结束后
            /// 自动恢复主相机的共享数据。
            CameraComponent *camera = nullptr;

            /// 可选：是否保留原有深度内容（VK_ATTACHMENT_LOAD_OP_LOAD）
            /// 用于 CSM 增量滚动更新已有深度图，避免全图清空
            bool load_depth = false;

            /// 可选：是否限制光栅化区域（局部裁剪）
            bool use_scissor = false;
            VkRect2D scissor{};

            /// 可选：是否局部清空 scissor 区域的深度（Reversed-Z: 0.0f）
            /// 用于 CSM 增量滚动时仅清除新进入视野的条带
            bool clear_scissor_depth = false;

            /// 可选：物体移动性过滤（-1 = 全部，0 = 仅静态 Static，1 = 仅动态 Movable）
            /// 中远景 CSM 静态滚动缓存级联可设置为 0（仅绘制静态物体）
            int mobility_filter = -1;

            /// 可选：本 pass 的光栅化剔除模式覆盖（VkCullModeFlags）。
            /// -1（默认）= 自动：depth-only 目标（shadow map）渲染模型背面（剔除正面），
            /// 其余 pass 沿用材质配置。显式赋值可强制本 pass 的剔除行为。
            int cull_mode_override = -1;
        };
    }//namespace ecs
}//namespace hgl

#pragma once

#include<hgl/color/Color4f.h>

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
        };
    }//namespace ecs
}//namespace hgl

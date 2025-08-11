#pragma once

#include <hgl/io/event/EventDispatcher.h>
#include <hgl/io/event/WindowEvent.h>
#include <hgl/io/event/MouseEvent.h>
#include <hgl/io/event/KeyboardEvent.h>
#include <cstdio>

namespace hgl::example
{
    /**
     * 独占模式事件分发器示例
     * 演示如何使用独占模式功能
     */
    class ExclusiveEventDispatcherExample
    {
    public:
        /**
         * 游戏中的UI弹窗事件处理器
         * 当弹窗显示时，需要独占所有输入事件
         */
        class DialogEventHandler : public io::WindowEvent
        {
            bool is_dialog_open = false;

        public:
            void OpenDialog()
            {
                is_dialog_open = true;
                printf("Dialog opened - entering exclusive mode\n");
                // 请求在父级别处于独占状态，这样游戏主循环就不会收到事件
                RequestExclusiveAtHigherLevel(1);
            }

            void CloseDialog()
            {
                is_dialog_open = false;
                printf("Dialog closed - exiting exclusive mode\n");
                // 释放独占状态，恢复正常事件分发
                ReleaseExclusiveAtHigherLevel(1);
            }

            void OnKeyDown(uint key) override
            {
                if (is_dialog_open)
                {
                    printf("Dialog received key: %u\n", key);
                    if (key == 27) // ESC键
                    {
                        CloseDialog();
                    }
                    // 其他按键处理对话框逻辑
                    // 由于处于独占模式，这些事件不会传递给游戏主循环
                }
            }

            void OnMouseDown(uint button) override
            {
                if (is_dialog_open)
                {
                    printf("Dialog received mouse button: %u\n", button);
                    // 处理对话框中的鼠标点击
                    // 事件被独占，不会影响游戏世界中的交互
                }
            }
        };

        /**
         * 游戏主循环事件处理器
         */
        class GameMainEventHandler : public io::WindowEvent
        {
        public:
            void OnKeyDown(uint key) override
            {
                // 正常情况下处理游戏按键
                // 当对话框打开时，由于独占模式，这里不会收到事件
                printf("Game received key: %u\n", key);
            }

            void OnMouseDown(uint button) override
            {
                // 正常情况下处理游戏鼠标点击
                // 当对话框打开时，这里不会收到事件
                printf("Game received mouse button: %u\n", button);
            }
        };

        /**
         * 摄像机控制器
         * 需要在更高级别独占鼠标输入以进行视角控制
         */
        class CameraController : public io::MouseEvent
        {
            bool is_camera_control_active = false;

        public:
            void StartCameraControl()
            {
                is_camera_control_active = true;
                printf("Camera control started - requesting exclusive mode\n");
                // 请求在更高级别（如RenderFramework级别）独占鼠标事件
                RequestExclusiveAtHigherLevel(2);
            }

            void StopCameraControl()
            {
                is_camera_control_active = false;
                printf("Camera control stopped - releasing exclusive mode\n");
                // 释放独占状态
                ReleaseExclusiveAtHigherLevel(2);
            }

            void OnMouseMove(int x, int y) override
            {
                if (is_camera_control_active)
                {
                    // 处理摄像机旋转
                    auto delta = GetMouseDelta();
                    printf("Camera rotation: dx=%d, dy=%d\n", delta.x, delta.y);
                }
            }

            void OnMouseDown(uint button) override
            {
                if (button == 1) // 右键
                {
                    StartCameraControl();
                }
            }

            void OnMouseUp(uint button) override
            {
                if (button == 1) // 右键
                {
                    StopCameraControl();
                }
            }
        };

        /**
         * 演示独占模式的使用
         */
        static void DemonstrateExclusiveMode()
        {
            // 创建层级结构：Window -> GameMain -> Dialog
            io::WindowEvent window_root;
            GameMainEventHandler game_main;
            DialogEventHandler dialog;
            CameraController camera;

            // 建立层级关系
            window_root.AddChildDispatcher(&game_main);
            game_main.AddChildDispatcher(&dialog);
            game_main.AddChildDispatcher(&camera);

            printf("=== 正常模式测试 ===\n");
            // 正常模式下，事件会传递给游戏主循环
            window_root.TriggerKeyDown(32); // 空格键
            window_root.TriggerMouseDown(0); // 左键

            printf("\n=== 对话框独占模式测试 ===\n");
            // 打开对话框，进入独占模式
            dialog.OpenDialog();

            // 现在事件只会传递给对话框，游戏主循环不会收到
            window_root.TriggerKeyDown(32); // 空格键
            window_root.TriggerMouseDown(0); // 左键

            // 按ESC关闭对话框
            window_root.TriggerKeyDown(27); // ESC键

            printf("\n=== 恢复正常模式测试 ===\n");
            // 对话框关闭后，恢复正常事件分发
            window_root.TriggerKeyDown(32); // 空格键
            window_root.TriggerMouseDown(0); // 左键

            printf("\n=== 摄像机独占模式测试 ===\n");
            // 测试摄像机控制的独占模式
            window_root.TriggerMouseDown(1); // 右键，开始摄像机控制
            window_root.TriggerMouseMove(100, 150);
            window_root.TriggerMouseMove(120, 180);
            window_root.TriggerMouseUp(1); // 右键释放，结束摄像机控制

            printf("\n=== 多级独占模式测试 ===\n");
            // 测试在摄像机控制期间打开对话框
            window_root.TriggerMouseDown(1); // 开始摄像机控制
            dialog.OpenDialog(); // 对话框会在更高级别设置独占
            window_root.TriggerMouseMove(200, 250); // 这个移动事件会被对话框独占
            dialog.CloseDialog(); // 关闭对话框
            window_root.TriggerMouseMove(300, 350); // 现在摄像机又能收到事件了
            window_root.TriggerMouseUp(1); // 结束摄像机控制
        }
    };

} // namespace hgl::example
#pragma once

#include<hgl/framework/WorkObject.h>
#include<hgl/framework/AppFramework.h>
#include<hgl/time/Time.h>
#include<memory>

namespace hgl
{
    /**
    * 工作管理器，驱动单个 WorkObject 的帧循环。<br>
    *
    * 原生路径由 RunFramework 托管（内部创建并 Run）；外部事件循环
    * （Qt/GTK）路径由外壳每帧调用 RunFrame。
    */
    class WorkManager
    {
    private:

        AppFramework *app_framework = nullptr;

        uint fps = 60;
        double frame_time = 1.0 / double(fps);

        double last_frame_time = -1;
        double cur_time = 0;

        WorkObject *cur_work_object = nullptr;

    public:

        WorkManager(AppFramework *af)
            : app_framework(af)
        {
        }

        ~WorkManager();

        /**
         * 外部事件循环（Qt/GTK 等）驱动的单帧推进：
         * 更新节流 → Tick → Render。
         * 由外部定时器每帧调用；wo 需先 Init()。
         */
        void RunFrame(WorkObject *wo);

        /**
         * 原生阻塞式帧循环：内部反复 RunFrame 直到窗口关闭。
         * 窗口最小化/隐藏时跳过整帧。
         */
        void Run(WorkObject *wo);
    };//class WorkManager

    /**
     * 开启对象追踪器（泄漏诊断）。HGL_ENABLE_OBJECT_TRACKER 关闭时为空实现。
     * 刻意不放在 RunFramework 内：它只服务诊断，不该出现在正常启动路径上。
     */
    void EnableObjectTracking();
    void DisableObjectTracking();

    template<typename WO> int RunFramework(const OSString &title,int argc,os_char **argv,uint width=1280,uint height=720)
    {
        int result = 0;

        // 显式作用域：保证 AppFramework/WorkObject 析构先于返回码结算
        {
            AppFramework app(title);

            if(!app.Init(width,height,argc,argv))
            {
                result = -1;
            }
            else
            {
                WorkManager wm(&app);

                // 所有权：RunFramework 创建 ⇒ RunFramework 必须销毁，且**必须早于 app**。
                // WorkObject 常持有它自己建的世界（如 OffscreenWorld 内的子 ECSContext，
                // 其系统持有 VkFence / VkBuffer / 设备内存）。原先正常退出路径没有
                // delete wo，这些设备对象就活到 app 析构之后 ⇒ 子世界资源越过
                // vkDestroyDevice，被校验层报成 "<Device> has N leaked objects"
                //（DualWorldShadows 实测 30 个：8 × VkFence + 子世界 cull 缓冲的设备内存）。
                // unique_ptr 声明在 wm 之后 ⇒ 逆序析构时 wo 最先走，早于 wm、更早于 app。
                std::unique_ptr<WO> wo(new WO());
                wo->SetECSContext(app.GetECSContext());

                if(!wo->Init())
                {
                    result = -2;
                }
                else
                {
                    wm.Run(wo.get());
                }
            }
        }

        return result;
    }
}//namespace hgl
#pragma once
#include"WorkObject.h"

namespace hgl
{
    class WorkManager
    {
        graph::RenderFramework *render_framework;

        uint fps=60;
        double frame_time=1.0f/double(fps);

        double last_update_time=0;
        double last_render_time=0;
        double cur_time=0;

        WorkObject *cur_work_object=nullptr;

    public:

        WorkManager(graph::RenderFramework *rf)
        {
            render_framework=rf;
        }

        virtual ~WorkManager()
        {
            SAFE_CLEAR(cur_work_object);
        }

        void SetFPS(uint f)
        {
            fps=f;
            frame_time=1.0f/double(fps);
        }

        void Tick(WorkObject *wo);
        void Render(WorkObject *wo);

        void Run();

        void Start(WorkObject *wo);
    };//class WorkManager
}//namespcae hgl

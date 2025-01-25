#pragma once
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/Time.h>

namespace hgl
{
    class WorkObject
    {
        RenderFramework *render_framework;

        bool destroy_flag=false;

        bool tickable=true;
        bool renderable=true;

    public:

        const bool IsDestroy()const{return destroy_flag;}
        const bool IsTickable()const{return tickable;}
        const bool IsRenderable()const{return renderable;}

        void MarkDestory(){destroy_flag=true;}
        void SetTickable(bool t){tickable=t;}
        void SetRenderable(bool r){renderable=r;}

    public:

        virtual ~WorkObject()=default;

        virtual void Join(RenderFramework *rf)
        {
            render_framework=rf;
        }

        virtual void Tick(double delta_time)=0;
        virtual void Render(double delta_time)=0;
    };//class WorkObject

    class WorkManager
    {
        graph::RenderFramework *render_framework;

        uint fps=60;
        double frame_time=1.0f/double(fps);

        double last_update_time=0;
        double last_render_time=0;

        WorkObject *cur_work_object=nullptr;

    public:

        WorkManager(graph::RenderFramework *rf)
        {
            render_framework=rf;
        }

        void SetFPS(uint f)
        {
            fps=f;
            frame_time=1.0f/double(fps);
        }

        void Update(WorkObject *wo)
        {
            double cur_time=GetDoubleTime();
            double delta_time;

            if(wo->IsTickable())
            {
                delta_time=cur_time-last_update_time;

                if(delta_time>=frame_time)
                {
                    last_update_time=cur_time;
                    wo->Tick(delta_time);
                }
            }

            if(wo->IsRenderable())
            {
                delta_time=cur_time-last_render_time;

                if(delta_time>=frame_time)
                {
                    last_render_time=cur_time;
                    wo->Render(delta_time);
                }
            }
        }

        void Run()
        {
            if(!cur_work_object)
                return;

            while(!cur_work_object->IsDestroy())
            {
                Update(cur_work_object);
            }
        }

        void Start(WorkObject *wo)
        {
            if(!wo)return;

            last_update_time=last_render_time=0;

            cur_work_object=wo;

            wo->Join(render_framework);

            Run();
        }
    };//class WorkManager
}//namespcae hgl

#pragma once

/**
 * thank for LearnOpenGL
 * link: https://learnopengl.com/Getting-started/Camera
 */

#include<hgl/graph/camera/CameraControl.h>
#include<hgl/io/event/KeyboardEvent.h>
#include<hgl/io/event/MouseEvent.h>
#include<hgl/Time.h>

namespace hgl::graph
{
    class FirstPersonCameraControl:public CameraControl
    {
        float pitch;        ///<抬头角度(绕X轴旋转角度(X轴左右))
        float yaw;          ///<左右角度(绕Z轴旋转角度(Z轴向上))
        float roll;         ///<歪头角度(绕Y轴旋转角度(Y轴向前))

        Vector3f front;
        Vector3f right;
        Vector3f up;

        Vector3f distance;          ///<相机到观察点的距离

        Vector3f target;            ///<目标点坐标

        Vector2f ReverseDirection;  ///<是否反转方向

    public:

        static CameraControlIDName StaticControlName()
        {
            static CameraControlIDName FPCC("FirstPersonCameraControl");

            return FPCC;
        }

        const CameraControlIDName GetControlName() const override 
        {
            return StaticControlName();
        }

    public:

        FirstPersonCameraControl(ViewportInfo *v,Camera *c,UBOCameraInfo *ci):CameraControl(v,c,ci)
        {
            target=Vector3f(0.0f);
            up=Vector3f(0,0,1);
            distance=Vector3f(0,0,0);

            pitch=0;
            yaw  =deg2rad(-90.0f);
            roll =0;

            ReverseDirection.x=-1;
            ReverseDirection.y=1;

            UpdateCameraVector();
        }
        virtual ~FirstPersonCameraControl()=default;
            
        void SetReserveDirection(bool x,bool y)
        {
            ReverseDirection.x=x?-1:1;
            ReverseDirection.y=y?-1:1;
        }

        void SetTarget(const Vector3f &t) override
        {
            if(!camera) return;

            front   =normalize(t-camera->pos);
            right   =normalize(cross(front,camera->world_up));
            up      =normalize(cross(right,front)); 

            camera->view_line=normalize(camera->pos-t);

            pitch   =asin(front.z);
            yaw     =atan2(front.x,front.y);

            UpdateCameraVector();

            // avoid division by zero: only compute component-wise distance when front is non-zero
            distance = Vector3f(0.0f);
            if(fabs(front.x) > 1e-6f) distance.x = (t.x - camera->pos.x) / front.x;
            if(fabs(front.y) > 1e-6f) distance.y = (t.y - camera->pos.y) / front.y;
            if(fabs(front.z) > 1e-6f) distance.z = (t.z - camera->pos.z) / front.z;
        }

        void Refresh() override
        {
            if(!camera || !camera_info) return;

            target=camera->pos+front*distance;

            camera_info->view       =LookAtMatrix(camera->pos,target,camera->world_up);

            RefreshCameraInfo(camera_info,vi,camera);

            if(ubo_camera_info) ubo_camera_info->Update();
        }

    public: //移动
            
        void UpdateCameraVector()
        {
            if(!camera) return;

            front   =PolarToVector(yaw,pitch);

            right   =normalize(cross(front,camera->world_up));
            up      =normalize(cross(right,front));
        }

        void Forward(float move_step)
        {
            if(!camera) return;
            camera->pos+=front*move_step;
        }

        void Backward(float move_step)
        {
            if(!camera) return;
            camera->pos-=front*move_step;
        }

        void Left(float move_step)
        {
            if(!camera) return;
            camera->pos-=right*move_step;
        }

        void Right(float move_step)
        {
            if(!camera) return;
            camera->pos+=right*move_step;
        }

    public: //旋转

        void Rotate(const Vector2f &axis)
        {
            constexpr double top_limit      =deg2rad( 89.0f);
            constexpr double bottom_limit   =deg2rad(-89.0f);

            yaw     -=axis.x*ReverseDirection.x/180.0f;
            pitch   -=axis.y*ReverseDirection.y/180.0f;

            if(pitch>top_limit      )pitch=top_limit;
            if(pitch<bottom_limit   )pitch=bottom_limit;
            
            UpdateCameraVector();
        }

        void Move(const Vector3f &delta)
        {
            if(!camera) return;
            camera->pos+=delta;
        }
    };//class FirstPersonCameraControl:public CameraControl

    class CameraKeyboardControl:public io::KeyboardStateEvent
    {
        FirstPersonCameraControl *camera;
        float move_speed;

    public:

        CameraKeyboardControl(FirstPersonCameraControl *wc)
        {
            camera=wc;
            move_speed=1.0f;
        }

        io::EventProcResult OnPressed(const io::KeyboardButton &kb)override
        {
            if(io::KeyboardStateEvent::OnPressed(kb)==io::EventProcResult::Continue)
                return(io::EventProcResult::Continue);

            if(kb==io::KeyboardButton::Minus    )move_speed*=0.9f;else
            if(kb==io::KeyboardButton::Equals   )move_speed*=1.1f;

            return(io::EventProcResult::Break);
        }

        bool Update() override
        {
            // allow multiple keys to be processed simultaneously (remove else-if chain)
            if(HasPressed(io::KeyboardButton::W     )) { if(camera) camera->Forward   (move_speed); }
            if(HasPressed(io::KeyboardButton::S     )) { if(camera) camera->Backward  (move_speed); }
            if(HasPressed(io::KeyboardButton::A     )) { if(camera) camera->Left      (move_speed); }
            if(HasPressed(io::KeyboardButton::D     )) { if(camera) camera->Right     (move_speed); }

            return(true);
        }
    };//class CameraKeyboardControl:public io::KeyboardStateEvent

    class CameraMouseControl:public io::MouseEvent
    {
        FirstPersonCameraControl *camera;
        double cur_time;
        double last_time;

        Vector2f mouse_pos;
        Vector2f mouse_last_pos;

    protected:

        io::EventProcResult OnPressed(const Vector2i &mouse_coord,io::MouseButton) override
        {
            mouse_last_pos=mouse_coord;

            last_time=cur_time;

            return(io::EventProcResult::Break);
        }
    
        io::EventProcResult OnWheel(const Vector2i &mouse_coord) override
        {
            if(mouse_coord.y==0) return(io::EventProcResult::Continue);
            if(camera) camera->Forward(float(mouse_coord.y)/100.0f);

            return(io::EventProcResult::Break);
        }

        io::EventProcResult OnMove(const Vector2i &mouse_coord) override
        {
            mouse_pos=mouse_coord;

            bool left =HasPressed(io::MouseButton::Left);
            bool right=HasPressed(io::MouseButton::Right);
        
            Vector2f pos=mouse_coord;
            Vector2f gap=pos-mouse_last_pos;
        
            if(left)
            {
                gap/=-5.0f;

                if(camera) camera->Rotate(gap);
            }
            else
            if(right)
            {
                gap/=10.0f;

                if(camera) camera->Move(Vector3f(gap.x,0,gap.y));
            }

            last_time=cur_time;
            mouse_last_pos=pos;

            return(io::EventProcResult::Continue);
        }

    public:

        CameraMouseControl(FirstPersonCameraControl *wc)
        {
            camera=wc;
            cur_time=0;
            last_time=0;
        }

        const Vector2f &GetMouseCoord()const{return mouse_pos;}

        bool Update() override
        {
            cur_time=GetPreciseTime();

            return(true);
        }
    };//class CameraMouseControl:public io::MouseEvent
}//namespace hgl::graph

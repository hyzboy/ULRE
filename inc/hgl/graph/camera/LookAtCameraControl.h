#pragma once

#include<hgl/graph/camera/CameraControl.h>
namespace hgl::graph
{
    class LookAtCameraControl:public CameraControl
    {
    protected:

        Vector3f direction;
        Vector3f right;
        Vector3f up;

        Vector3f target;            ///<目标点坐标

    public:

        using CameraControl::CameraControl;
        virtual ~LookAtCameraControl()=default;

        void Refresh() override
        {
            if(!camera)return;

            camera->info.view_line  =camera->pos-target;
            camera->info.view       =glm::lookAtRH(camera->pos,target,camera->world_up);

            direction               =normalized(camera->view_line);
            right                   =normalized(cross(camera->world_up, direction));
            up                      =normalized(cross(right,            direction));

            camera->RefreshCameraInfo();
        }

        void SetTarget(const Vector3f &t)
        {
            target=t;
        }

    public:

        /**
            * 向指定向量移动
            * @param move_dist 移动距离
            */
        void Move(const Vector3f &move_dist)
        {
            camera->pos+=move_dist;
            target+=move_dist;
        }

        /**
            * 以自身为中心旋转
            * @param ang 角度
            * @param axis 旋转轴
            */
        void Rotate(double ang,const Vector3f &axis)
        {
            normalize(axis);

            const Matrix3f mat=rotate(deg2rad(ang),axis);

            target=camera->pos+mat*(target-camera->pos);
        }

        /**
            * 以目标为中心旋转
            * @param ang 角度
            * @param axis 旋转轴
            */
        void WrapRotate(double ang,const Vector3f &axis)
        {
            normalize(axis);

            const Matrix3f mat=rotate(deg2rad(ang),axis);

            camera->pos=target+mat*(camera->pos-target);
        }

    public: //距离

        const float GetDistance()const{return length(camera->pos-target);}                      ///<获取视线长度(摄像机到目标点)

        /**
            * 调整距离
            * @param rate 新距离与原距离的比例
            */
        void Distance(float rate)                                                               ///<调整距离
        {
            if(rate==1.0)return;

            camera->pos=target+(camera->pos-target)*rate;
        }
    };//class LookAtCameraControl:public CameraControl
}//namespace hgl::graph

#pragma once

#include<hgl/graph/camera/CameraControl.h>
#include<hgl/math/Projection.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
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
            if(!camera || !camera_info || !vi) return;

            camera->viewDirection = Normalize(camera->pos - target);
            camera_info->view     = glm::lookAtRH(camera->pos, target, camera->world_up);

            direction             = camera->viewDirection;
            right              = Normalized(Cross(camera->world_up, direction));
            up                 = Normalized(Cross(right,            direction));

            RefreshCameraInfo(camera_info, vi, camera);

            if(ubo_camera_info) ubo_camera_info->Update();
        }

        void SetTarget(const Vector3f &t) override
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
            if(!camera) return;
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
            if(!camera) return;

            Vector3f a = axis;
            Normalize(a);

            // Use helper that returns a 3x3 rotation matrix (degrees)
            Matrix3f rot = hgl::AxisRotate3Deg(static_cast<float>(ang), Vector3f(a.x,a.y,a.z));

            target = camera->pos + rot * (target - camera->pos);
        }

        /**
            * 以目标为中心旋转(公转)
            * @param ang 角度
            * @param axis 旋转轴
            */
        void OrbitRotate(double ang,const Vector3f &axis)
        {
            if(!camera) return;

            Vector3f a = axis;
            Normalize(a);

            Matrix3f rot = hgl::AxisRotate3Deg(static_cast<float>(ang), Vector3f(a.x,a.y,a.z));

            camera->pos = target + rot * (camera->pos - target);
        }

    public: //距离

        const float GetDistance()const{return camera?Length(camera->pos-target):0.0f;}                      ///<获取视线长度(摄像机到目标点)

        /**
            * 调整距离
            * @param rate 新距离与原距离的比例
            */
        void Distance(float rate)                                                               ///<调整距离
        {
            if(!camera) return;
            if(rate==1.0f)return;

            camera->pos=target+(camera->pos-target)*rate;
        }
    };//class LookAtCameraControl:public CameraControl
}//namespace hgl::graph

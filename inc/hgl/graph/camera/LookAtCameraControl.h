#pragma once

#include<hgl/graph/camera/CameraControl.h>
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

            camera->view_line  = camera->pos - target;
            camera_info->view  = glm::lookAtRH(camera->pos, target, camera->world_up);

            direction               = normalized(camera->view_line);
            right                   = normalized(cross(camera->world_up, direction));
            up                      = normalized(cross(right,            direction));

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
            normalize(a);

            // Use glm::rotate on a mat4 then convert to mat3
            const float rad = glm::radians(static_cast<float>(ang));
            glm::mat3 rot = glm::mat3(glm::rotate(glm::mat4(1.0f), rad, Vector3f(a.x,a.y,a.z)));

//             Rodrigues' rotation formula applied to (target - camera->pos)
            const double c = cos(rad);
            const double s = sin(rad);

            Vector3f rel = target - camera->pos;
            Vector3f k = a; // normalized axis

            Vector3f rotated = rel * float(c) + cross(k, rel) * float(s) + k * (dot(k, rel) * float(1.0 - c));

            target = camera->pos + rotated;
        }

        /**
            * 以目标为中心旋转
            * @param ang 角度
            * @param axis 旋转轴
            */
        void WrapRotate(double ang,const Vector3f &axis)
        {
            if(!camera) return;

            Vector3f a = axis;
            normalize(a);

            const float rad = glm::radians(static_cast<float>(ang));
            glm::mat3 rot = glm::mat3(glm::rotate(glm::mat4(1.0f), rad, Vector3f(a.x,a.y,a.z)));

            camera->pos = target + rot * (camera->pos - target);

//             const double c = cos(rad);
//             const double s = sin(rad);
//
//             Vector3f rel = camera->pos - target;
//             Vector3f k = a;
//
//             Vector3f rotated = rel * float(c) + cross(k, rel) * float(s) + k * (dot(k, rel) * float(1.0 - c));
//
//             camera->pos = target + rotated;
        }

    public: //距离

        const float GetDistance()const{return camera?length(camera->pos-target):0.0f;}                      ///<获取视线长度(摄像机到目标点)

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

#pragma once

#include<hgl/math/TransformAction.h>
#include<hgl/graph/CameraInfo.h>

namespace hgl::graph
{
    using namespace math;

    /**
    * 永远转向摄像机的变换节点
    */
    class TransformFaceToCamera:public TransformAction
    {
        CameraInfo *camera_info=nullptr;

        math::Matrix4f last_view_matrix;

    protected:

        void MakeNewestData(Matrix4f &mat) override
        {
            if(camera_info)
                mat=ToMatrix(CalculateFacingRotationQuat(WorldPosition,camera_info->view,WorldNormal));
            else
                mat=Identity4f;
        }

    public:

        using TransformAction::TransformAction;

        constexpr const size_t GetTypeHash()const override { return hgl::GetTypeHash<TransformFaceToCamera>(); }

        TransformFaceToCamera():TransformAction()
        {
            camera_info=nullptr;

            last_view_matrix=Identity4f;
        }

        TransformFaceToCamera(CameraInfo *ci):TransformAction()
        {
            camera_info=ci;

            last_view_matrix=Identity4f;
        }

        TransformAction *CloneSelf()const override
        {
            return(new TransformFaceToCamera(camera_info));
        }

        void SetCameraInfo(CameraInfo *ci)
        {
            if(camera_info==ci)return;

            camera_info=ci;

            UpdateVersion();
        }

        bool Update() override
        {
            if(!camera_info)
            {
                return(false);
            }

            if(IsNearlyEqual(last_view_matrix,camera_info->view))
                return(false);

            last_view_matrix=camera_info->view;

            UpdateVersion();
            return(true);
        }
    };//class TransformFaceToCamera:public TransformAction
}//namespace hgl::graph

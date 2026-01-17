#pragma once

#include<hgl/graph/ViewportInfo.h>
#include<hgl/graph/CameraInfo.h>

namespace hgl::graph
{
    /**
     * 摄像机数据结构
     */
    struct Camera
    {
        float fovY;                 ///<垂直视场角(degrees)
        float znear,zfar;           ///<Z轴上离摄像机的距离(注：因znear会参与计算，为避免除0操作，请不要使用0或过于接近0的值)

        Vector3f pos;               ///<摄像机坐标

        Vector3f world_up;          ///<向上量(默认0,0,1，Z轴向上)

        Vector3f viewDirection;     ///<视线方向 normalize(camera_pos - target)，从相机指向远方

    public:

        Camera()
            : fovY(45.0f)
            , znear(0.1f)
            , zfar(10240.0f)
            , pos(0.0f, 0.0f, 0.0f)
            , world_up(0.0f, 0.0f, 1.0f)
            , viewDirection(1.0f, 0.0f, 0.0f)
        {
        }
    };//struct Camera

    void RefreshCameraInfo(CameraInfo *,const ViewportInfo *,const Camera *);
}//namespace hgl::graph

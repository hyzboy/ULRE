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
        float Yfov;                 ///<水平FOV
        float znear,zfar;           ///<Z轴上离摄像机的距离(注：因znear会参与计算，为避免除0操作，请不要使用0或过于接近0的值)

        Vector3f pos;               ///<摄像机坐标

        Vector3f world_up;          ///<向上量(默认0,0,1)
            
        Vector3f view_line;         ///<视线 normalize(eye-target)

    public:

        Camera()
        {
            hgl_zero(*this);

            Yfov        =45;
            znear       =0.1f;
            zfar        =10240;
            world_up.z  =1.0f;
        }
    };//struct Camera

    void RefreshCameraInfo(CameraInfo *,const ViewportInfo *,const Camera *);
}//namespace hgl::graph

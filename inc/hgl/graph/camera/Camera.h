#pragma once

#include<hgl/graph/ubo/ViewportInfo.h>
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

        Vector3f pos;               ///<摄像机坐标（float，常规使用）

        Vector3f world_up;          ///<向上量(默认0,0,1，Z轴向上)

        Vector3f viewDirection;     ///<视线方向 normalize(camera_pos - target)，从相机指向远方

        bool use_reversed_z;        ///<是否使用 Reversed-Z + Infinite Far（引擎当前全局启用，默认 true）

        Vector3d world_position_double; ///<double 精度世界坐标（Camera-Relative Rendering 用）

    public:

        Camera()
            : fovY(45.0f)
            , znear(0.1f)
            , zfar(10240.0f)
            , pos(0.0f, 0.0f, 0.0f)
            , world_up(0.0f, 0.0f, 1.0f)
            , viewDirection(1.0f, 0.0f, 0.0f)
            , use_reversed_z(true)
            , world_position_double(0.0, 0.0, 0.0)
        {
        }

        Vector3d GetWorldPositionDouble() const { return world_position_double; }
    };//struct Camera

    /**
     * 由 ci->view / ci->projection 推导全部派生量（inverse_* / vp / frustum_planes / sky）
     *
     * @note 不修改 view / projection 本身。自定义矩阵相机（如 CSM 光源相机）应先写入
     *       自己的 view / projection，再调用本函数，得到与主相机路径完全一致的派生量。
     */
    void RefreshCameraInfoDerived(CameraInfo *);

    /**
     * 由 Camera 源数据填充 CameraInfo 的非矩阵字段
     * （pos / view_line / world_up / camera_facing_right,up / znear / zfar /
     *   use_reversed_z / _pad_ci0 / camera_world_pos）
     *
     * @note 这些字段是 shader 的权威输入（阴影级联选级用 camera_world_pos + view_line，
     *       billboard 用 camera_facing_*），主相机路径与自定义矩阵路径必须共用本函数，
     *       禁止各自复制一份实现——历史上两条路径漂移过，导致同一帧内不同消费者
     *       看到不一致的相机基准。
     */
    void RefreshCameraInfoCamera(CameraInfo *,const Camera *);

    void RefreshCameraInfo(CameraInfo *,const ViewportInfo *,const Camera *);
}//namespace hgl::graph

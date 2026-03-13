#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/camera/Camera.h>
#include<hgl/graph/camera/ReversedZProj.h>
#include<hgl/math/geometry/Frustum.h>
#include<hgl/graph/camera/ViewportInfo.h>

namespace hgl::graph
{
    void RefreshCameraInfo(CameraInfo *ci,const ViewportInfo *vi,const Camera *cam)
    {
        if(!ci || !vi || !cam) return;
        if(cam->znear <= 0.0f) return;

        if(cam->use_reversed_z)
        {
            const float fov_radians = cam->fovY * (3.14159265358979323846f / 180.0f);
            ci->projection = MakeInfiniteReversedZProj(fov_radians, vi->GetAspectRatio(), cam->znear);
        }
        else
        {
            if(cam->zfar <= cam->znear) return;
            ci->projection = math::PerspectiveMatrix(cam->fovY, vi->GetAspectRatio(), cam->znear, cam->zfar);
        }

        ci->inverse_projection     =Inverse(ci->projection);

        ci->inverse_view           =Inverse(ci->view);

        ci->vp                     =ci->projection*ci->view;
        ci->inverse_vp             =Inverse(ci->vp);

        GetFrustumPlanes(ci->frustum_planes,ci->vp);

        {
            glm::mat4 tmp=ci->view;
            tmp[3]=glm::vec4(0,0,0,1);

            ci->sky=ci->projection*tmp;
        }

        ci->pos                    =cam->pos;
        ci->view_line              =cam->viewDirection;
        ci->world_up               =cam->world_up;

        // http://www.opengl-tutorial.org/intermediate-tutorials/billboards-particles/billboards/

        ci->billboard_right        =math::Vector3f(ci->view[0][0],ci->view[1][0],ci->view[2][0]);
        ci->billboard_up           =math::Vector3f(ci->view[0][1],ci->view[1][1],ci->view[2][1]);

        ci->znear                  =cam->znear;
        ci->zfar                   =cam->zfar;

        ci->use_reversed_z         =cam->use_reversed_z ? 1 : 0;
        ci->_pad_ci0               =0.0f;

        // Camera-Relative Rendering: 记录绝对世界坐标（低精度），供 fog/terrain 等 shader 使用
        {
            const Vector3d wp = cam->world_position_double;
            ci->camera_world_pos = math::Vector3f(static_cast<float>(wp.x),
                                                   static_cast<float>(wp.y),
                                                   static_cast<float>(wp.z));
        }

        // NOTE: Camera-Relative view 矩阵平移归零和 pos=0 暂不启用，
        // 需等 TransformAssignmentBuffer::SetCameraOffset 完整接入后再启用。
    }
}//namespace hgl::graph

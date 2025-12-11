#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/camera/Camera.h>
#include<hgl/math/geometry/Frustum.h>
#include<hgl/graph/ViewportInfo.h>

namespace hgl::graph
{
    void RefreshCameraInfo(CameraInfo *ci,const ViewportInfo *vi,const Camera *cam)
    {
        if(!ci || !vi || !cam) return;
        if(cam->znear <= 0.0f || cam->zfar <= cam->znear) return;

        ci->projection             =math::PerspectiveMatrix(cam->fovY,vi->GetAspectRatio(),cam->znear,cam->zfar);

        ci->inverse_projection     =inverse(ci->projection);

        ci->inverse_view           =inverse(ci->view);

        ci->vp                     =ci->projection*ci->view;
        ci->inverse_vp             =inverse(ci->vp);

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
    }
}//namespace hgl::graph

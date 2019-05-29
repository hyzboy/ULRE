#include<hgl/graph/Camera.h>
namespace hgl
{
    namespace graph
    {
        inline Matrix4f LookAt(const Vector3f &eye,const Vector3f &target,const Vector3f &up)
        {
            Vector3f forward=target-eye;

            normalize(forward);

            Vector3f side=cross(forward,up);

            normalize(side);

            Vector3f nup=cross(side,forward);

            Matrix4f result(side.x,     side.y,     side.z,     0.0f,
                            nup.x,      nup.y,      nup.z,      0.0f,
                            -forward.x, -forward.y, -forward.z, 0.0f,
                            0.0f,       0.0f,       0.0f,       1.0f);

            return result*translate(-eye);
        }

        void Camera::Refresh()
        {
            if(type==CameraType::Perspective)
                matrix.projection=perspective(fov,width/height,znear,zfar);
            else
                matrix.projection=ortho(width,height,znear,zfar);               //这个算的不对

            matrix.modelview=hgl::graph::LookAt(eye,center,up_vector);

            matrix.mvp=matrix.projection*matrix.modelview;

            matrix.two_dim=ortho(width,height,znear,zfar);

            frustum.SetVerticalFovAndAspectRatio(DegToRad(fov),width/height);
            frustum.SetViewPlaneDistances(znear,zfar);
        }
    }//namespace graph
}//namespace hgl

#include<hgl/graph/Camera.h>
namespace hgl
{
    namespace graph
    {
        inline Matrix4f LookAt(const Vector4f &eye,const Vector4f &target,const Vector4f &up)
        {
            Vector4f forward=target-eye;

            normalize(forward);

            Vector4f side=cross(forward,up);

            normalize(side);

            Vector4f nup=cross(side,forward);

            Matrix4f result(side.x,         side.y,         side.z,             0.0f,
                            nup.x,          nup.y,          nup.z,              0.0f,
                            -forward.x,     -forward.y,     -forward.z,         0.0f,
                            -dot(side,eye), -dot(nup,eye),  dot(forward,eye),   1.0f);

            return result*translate(-eye.xyz());
        }

        void Camera::Refresh()
        {
            if(type==CameraType::Perspective)
                matrix.projection=perspective(fov,width/height,znear,zfar);
            else
                matrix.projection=ortho(width,height,znear,zfar);               //这个算的不对

            //matrix.inverse_projection=matrix.projection.Inverted();

            matrix.modelview=hgl::graph::LookAt(eye,center,up_vector);

            matrix.mvp=matrix.projection*matrix.modelview;

            //注意： C++中要 projection * model_view * local_to_world * position
            //而GLSL中要 position * local_to_world * model_view * projection

            matrix.two_dim=ortho(width,height,znear,zfar);

            frustum.SetVerticalFovAndAspectRatio(DegToRad(fov),width/height);
            frustum.SetViewPlaneDistances(znear,zfar);
        }
    }//namespace graph
}//namespace hgl

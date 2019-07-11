#include<hgl/graph/Camera.h>
namespace hgl
{
    namespace graph
    {
        template<typename V>
        inline Matrix4f LookAt(const V &eye,const V &target,const V &up)
        {
            V forward=target-eye;

            normalize(forward);

            V side=cross(forward,up);

            normalize(side);

            V nup=cross(side,forward);

            Matrix4f result( side.x,         side.y,         side.z,            0.0f,
                             nup.x,          nup.y,          nup.z,             0.0f,
                            -forward.x,     -forward.y,     -forward.z/2.0f,    0.0f,
                             0.0f,           0.0f,           0.0f,              1.0f);

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
            //matrix.modelview=Matrix4f::LookAt(eye.xyz(),center.xyz(),forward_vector.xyz(),up_vector.xyz(),up_vector.xyz());

            matrix.mvp=matrix.projection*matrix.modelview;

            //注意： C++中要 projection * model_view * local_to_world * position
            //而GLSL中要 position * local_to_world * model_view * projection

            matrix.two_dim=ortho(width,height,znear,zfar);

            matrix.view_pos=eye;

            frustum.SetVerticalFovAndAspectRatio(DegToRad(fov),width/height);
            frustum.SetViewPlaneDistances(znear,zfar);
        }
    }//namespace graph
}//namespace hgl

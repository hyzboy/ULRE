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

            Matrix4f result( side.x,         side.y,         side.z,            1.0f,
                             nup.x,          nup.y,          nup.z,             1.0f,
                            -forward.x,     -forward.y,     -forward.z/2.0f,    1.0f,
                             0.0f,           0.0f,           0.0f,              1.0f);
                                                        //  ^^^^^^
                                                        //  某些引擎这里为0.5，那是因为他们是 -1 to 1 的Z值设定，而我们是0 to 1，所以这里不用乘
                                                        //  同理，camera的znear为接近0的正数，zfar为一个较大的正数，默认使用16/256

            return result*translate(-eye.xyz());
        }

        void Camera::Refresh()
        {
            if(type==CameraType::Perspective)
                matrix.projection=perspective(fov,width/height,znear,zfar);
            else
                matrix.projection=ortho(width,height,znear,zfar);               //这个算的不对

            matrix.inverse_projection=matrix.projection.Inverted();

            matrix.modelview=hgl::graph::LookAt(eye,center,up_vector);
            //matrix.modelview=Matrix4f::LookAt(eye.xyz(),center.xyz(),forward_vector.xyz(),up_vector.xyz(),up_vector.xyz());
            matrix.inverse_modelview=matrix.modelview.Inverted();

            matrix.mvp=matrix.projection*matrix.modelview;
            matrix.inverse_map=matrix.mvp.Inverted();

            //注意： C++中要 projection * model_view * local_to_world * position
            //而GLSL中要 position * local_to_world * model_view * projection

            matrix.ortho=ortho(width,height);

            matrix.view_pos=eye;
            matrix.resolution.x=width;
            matrix.resolution.y=height;

            frustum.SetVerticalFovAndAspectRatio(DegToRad(fov),width/height);
            frustum.SetViewPlaneDistances(znear,zfar);
        }
    }//namespace graph
}//namespace hgl

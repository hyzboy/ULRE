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

        void CameraToFrustum(Frustum *f,const Camera *cam)
        {
            if(!f||!cam)return;

            f->SetVerticalFovAndAspectRatio(DegToRad(cam->fov),cam->width/cam->height);
            f->SetViewPlaneDistances(cam->znear,cam->zfar);

            //Matrix4f projection_matrix=f->ProjectionMatrix();     //可以用Frustum来算projection matrix
        }

        void MakeCameraMatrix(Matrix4f *proj,Matrix4f *mv,const Camera *cam)
        {
            *proj=perspective(cam->width/cam->height,cam->fov,cam->znear,cam->zfar);
            *mv=hgl::graph::LookAt(cam->eye,cam->center,cam->up_vector);
        }
    }//namespace graph
}//namespace hgl

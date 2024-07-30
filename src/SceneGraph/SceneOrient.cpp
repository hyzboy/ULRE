#include<hgl/graph/SceneOrient.h>
namespace hgl
{
    namespace graph
    {
        SceneOrient::SceneOrient()
        {
            Position=Vector3f(0.0f);
            Direction=Vector3f(0.0f);

            IdentityLocalMatrix=true;

            LocalMatrix                 =Identity4f;
            LocalToWorldMatrix          =Identity4f;
            InverseLocalMatrix          =Identity4f;
            InverseLocalToWorldMatrix   =Identity4f;
        }
        
        SceneOrient::SceneOrient(const SceneOrient &so)
        {
            hgl_cpy(*this,so);
        }

        SceneOrient::SceneOrient(const Matrix4f &mat)
        {
            SetLocalMatrix(mat);

            LocalToWorldMatrix          =Identity4f;
            InverseLocalToWorldMatrix   =Identity4f;
        }

        Matrix4f &SceneOrient::SetLocalMatrix(const Matrix4f &m)
        {
            LocalMatrix=m;

            IdentityLocalMatrix=IsIdentity(m);

            InverseLocalMatrix=inverse(LocalMatrix);

            return LocalMatrix;
        }

        Matrix4f &SceneOrient::SetLocalToWorldMatrix(const Matrix4f &m)
        {
            LocalToWorldMatrix=m;

            InverseLocalToWorldMatrix=inverse(LocalToWorldMatrix);

            return LocalToWorldMatrix;
        }

        /**
         * 刷新世界矩阵
         * @param m 上一级local to world矩阵
         */
        void SceneOrient::RefreshLocalToWorldMatrix(const Matrix4f *m)
        {
            if(IdentityLocalMatrix)
                SetLocalToWorldMatrix(*m);
            else
                SetLocalToWorldMatrix(TransformMatrix(*m,LocalMatrix));
        }
    }//namespace graph
}//namespace hgl

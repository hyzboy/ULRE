#include<hgl/graph/SceneOrient.h>
namespace hgl
{
    namespace graph
    {
        SceneOrient::SceneOrient()
        {
            LocalMatrix                 =Matrix4f(1.0f);
            LocalToWorldMatrix          =Matrix4f(1.0f);
            InverseLocalMatrix          =Matrix4f(1.0f);
            InverseLocalToWorldMatrix   =Matrix4f(1.0f);
        }

        SceneOrient::SceneOrient(const Matrix4f &mat)
        {
            SetLocalMatrix(mat);

            LocalToWorldMatrix          =Matrix4f(1.0f);
            InverseLocalToWorldMatrix   =Matrix4f(1.0f);
        }

        Matrix4f &SceneOrient::SetLocalMatrix(const Matrix4f &m)
        {
            LocalMatrix=m;

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
            SetLocalToWorldMatrix((*m)*LocalMatrix);
        }
    }//namespace graph
}//namespace hgl

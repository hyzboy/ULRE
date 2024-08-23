#include<hgl/graph/SceneOrient.h>
namespace hgl
{
    namespace graph
    {
        SceneOrient::SceneOrient()
        {
            LocalMatrix=Identity4f;

            SetWorldMatrix(Identity4f);
        }
        
        SceneOrient::SceneOrient(const SceneOrient &so)
        {
            hgl_cpy(*this,so);
        }

        SceneOrient::SceneOrient(const Matrix4f &mat)
        {
            SetLocalMatrix(mat);
        }

        void SceneOrient::SetLocalMatrix(const Matrix4f &mat)
        {
            if(IsNearlyEqual(LocalMatrix,mat))
                return;

            LocalMatrix=mat;
        }

        void SceneOrient::SetWorldMatrix(const Matrix4f &wm)
        {
            LocalToWorldMatrix                  =wm;
            InverseLocalToWorldMatrix           =inverse(LocalToWorldMatrix);
            InverseTransposeLocalToWorldMatrix  =transpose(InverseLocalToWorldMatrix);
        }

        /**
         * 刷新世界变换矩阵
         * @param parent_matrix 上一级LocalToWorld变换矩阵
         */
        bool SceneOrient::RefreshMatrix(const Matrix4f &parent_matrix)
        {
            if(hgl_cmp(ParentMatrix,parent_matrix)==0)      //如果上一级到这一级没变，自然是可以用memcmp比较的
                return(true);

            ParentMatrix=parent_matrix;

            if(IsIdentityMatrix(LocalMatrix))
            {
                SetWorldMatrix(parent_matrix);
            }
            else
            if(IsIdentityMatrix(parent_matrix))
            {
                SetWorldMatrix(LocalMatrix);
            }
            else
            {
                SetWorldMatrix(MatrixMult(parent_matrix,LocalMatrix));
            }

            return(true);
        }
    }//namespace graph
}//namespace hgl

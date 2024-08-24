#include<hgl/graph/SceneOrient.h>
namespace hgl
{
    namespace graph
    {
        SceneOrient::SceneOrient()
        {
            parent_matrix=Identity4f;
            local_matrix=Identity4f;

            SetWorldMatrix(Identity4f);

            parent_matrix_dirty=false;
            local_matrix_dirty=false;
            transform_version=transform_manager.GetCurrentVersion();
        }
        
        SceneOrient::SceneOrient(const SceneOrient &so)
        {
        #define SO_COPY(value)  value=so.value;

            SO_COPY(parent_matrix)
            SO_COPY(parent_matrix_dirty)

            SO_COPY(local_matrix)
            SO_COPY(local_matrix_dirty)

            SO_COPY(transform_manager)
            SO_COPY(transform_version)

            SO_COPY(local_to_world_matrix)
            SO_COPY(inverse_local_to_world_matrix)
            SO_COPY(inverse_transpose_local_to_world_matrix)

        #undef SO_COPY
        }

        SceneOrient::SceneOrient(const Matrix4f &mat):SceneOrient()
        {
            SetLocalMatrix(mat);
        }

        void SceneOrient::SetLocalMatrix(const Matrix4f &mat)
        {
            if(IsNearlyEqual(local_matrix,mat))
                return;

            local_matrix=mat;
        }

        void SceneOrient::SetWorldMatrix(const Matrix4f &wm)
        {
            local_to_world_matrix                  =wm;
            inverse_local_to_world_matrix          =inverse(local_to_world_matrix);
            inverse_transpose_local_to_world_matrix=transpose(inverse_local_to_world_matrix);
        }

        /**
         * 刷新世界变换矩阵
         * @param parent_matrix 上一级LocalToWorld变换矩阵
         */
        bool SceneOrient::RefreshMatrix(const Matrix4f &pm)
        {
            if(hgl_cmp(pm,parent_matrix)==0)      //如果上一级到这一级没变，自然是可以用memcmp比较的
                return(true);

            parent_matrix=pm;

            Matrix4f tm;
            
            transform_manager.GetMatrix(tm);

            SetWorldMatrix(parent_matrix*local_matrix*tm);

            //if(IsIdentityMatrix(LocalMatrix))
            //{
            //    SetWorldMatrix(parent_matrix);
            //    return(true);
            //}
            //

            //if(IsIdentityMatrix(parent_matrix))
            //{
            //    SetWorldMatrix(LocalMatrix);
            //}
            //else
            //{
            //    SetWorldMatrix(parent_matrix*LocalMatrix);
            //}

            return(true);
        }
    }//namespace graph
}//namespace hgl

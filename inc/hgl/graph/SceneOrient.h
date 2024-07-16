#ifndef HGL_GRAPH_SCENE_ORIENT_INCLUDE
#define HGL_GRAPH_SCENE_ORIENT_INCLUDE

//#include<hgl/type/List.h>
#include<hgl/math/Math.h>
#include<hgl/graph/VK.h>
//#include<hgl/graph/Transform.h>
namespace hgl
{
    namespace graph
    {
        /**
        * 方向定位数据基类
        */
        class SceneOrient                                                                                               ///场景定位类
        {
        protected:

            //ObjectList<Transform> TransformList;

            Vector3f Position;                                                                                          ///<坐标
            Vector3f Direction;                                                                                         ///<方向

            Matrix4f LocalMatrix;                                                                                       ///<当前矩阵(指相对上一级的变换矩阵)
            Matrix4f LocalToWorldMatrix;                                                                                ///<当前到世界矩阵

            Matrix4f InverseLocalMatrix;                                                                                ///<反向当前矩阵
            Matrix4f InverseLocalToWorldMatrix;                                                                         ///<反向当前到世界矩阵

        public:

            SceneOrient();
            SceneOrient(const Matrix4f &mat);
            virtual ~SceneOrient()=default;

                    void        SetPosition                 (const Vector3f &pos){Position=pos;}
                    void        SetDirection                (const Vector3f &dir){Direction=dir;}

            const   Vector3f &  GetLocalPosition            ()const{return Position;}
            const   Vector3f &  GetLocalDirection           ()const{return Direction;}
            const   Vector3f &  GetWorldPosition            ()const{return TransformPosition(LocalToWorldMatrix,Position);}
            const   Vector3f &  GetWorldDirection           ()const{return TransformDirection(LocalToWorldMatrix,Direction);}

        public: 

                    Matrix4f &  SetLocalMatrix              (const Matrix4f &);                                         ///<设定当前节点矩阵
                    Matrix4f &  SetLocalToWorldMatrix       (const Matrix4f &);                                         ///<设定当前节点到世界矩阵

            const   Matrix4f &  GetLocalMatrix              ()const {return LocalMatrix;}
            const   Matrix4f &  GetLocalToWorldMatrix       ()const {return LocalToWorldMatrix;}

            const   Matrix4f &  GetInverseLocalMatrix       ()const {return InverseLocalMatrix;}
            const   Matrix4f &  GetInverseLocalToWorldMatrix()const {return InverseLocalToWorldMatrix;}

        public:

            virtual void        RefreshLocalToWorldMatrix   (const Matrix4f *);                                         ///<刷新到世界空间矩阵
        };//class SceneOrient
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_ORIENT_INCLUDE

#ifndef HGL_GRAPH_SCENE_ORIENT_INCLUDE
#define HGL_GRAPH_SCENE_ORIENT_INCLUDE

//#include<hgl/type/List.h>
#include<hgl/math/Math.h>
#include<hgl/graph/VK.h>
namespace hgl
{
    namespace graph
    {
        /**
        * 方向定位数据基类
        */
        class SceneOrient                                                                                                   ///场景定位类
        {
        protected:

            Vector3f Position;                                                                                              ///<坐标
            Vector3f Direction;                                                                                             ///<方向

            Transform LocalTransform;                                                                                       ///<当前变换(指相对上一级的变换)
            Transform WorldTransform;                                                                                       ///<当前到世界变换

        protected:

                    void        SetWorldTransform           (const Transform &);                                            ///<设定当前节点到世界矩阵

        public:

            SceneOrient();
            SceneOrient(const SceneOrient &);
            SceneOrient(const Transform &);
            virtual ~SceneOrient()=default;

                    void        SetPosition                 (const Vector3f &pos){Position=pos;}
                    void        SetDirection                (const Vector3f &dir){Direction=dir;}

            const   Vector3f &  GetLocalPosition            ()const {return Position;}
            const   Vector3f &  GetLocalDirection           ()const {return Direction;}
            const   Vector3f    GetWorldPosition            ()      {return WorldTransform.TransformPosition(Position);}
            const   Vector3f    GetWorldDirection           ()      {return WorldTransform.TransformDirection(Direction);}

        public: 

                    void        SetLocalTransform           (const Transform &);                                            ///<设定当前节点矩阵

             const  Transform & GetLocalTransform           ()const {return LocalTransform;}                                ///<取得当前节点矩阵
             const  Transform & GetWorldTransform           ()const {return WorldTransform;}                                ///<取得当前节点到世界矩阵
    
                    Transform & GetLocalTransform           ()      {LocalTransform.UpdateMatrix();return LocalTransform;}  ///<取得当前节点矩阵
                    Transform & GetWorldTransform           ()      {WorldTransform.UpdateMatrix();return WorldTransform;}  ///<取得当前节点到世界矩阵

        public:

            virtual bool        RefreshTransform            (const Transform &);                                            ///<刷新到世界空间变换
        };//class SceneOrient
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_ORIENT_INCLUDE

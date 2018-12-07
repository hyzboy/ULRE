#ifndef HGL_VERTEX_BUFFER_BASE_INCLUDE
#define HGL_VERTEX_BUFFER_BASE_INCLUDE

#include<hgl/type/DataType.h>
namespace hgl
{
    namespace graph
    {
        class VertexBufferControl;

        class VertexBufferBase
        {
            void *mem_data;                                                                         ///<内存中的数据

        protected:

            uint vb_type;                                                                           ///<缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)

            uint dc_num;                                                                            ///<每个数据成员数(比如二维坐标为2、三维坐标为3)
            uint data_type;                                                                         ///<单个数据类型(GL_BYTE,GL_UNSIGNED_SHORT等)
            uint data_bytes;

            uint count;                                                                             ///<数据个数

            uint total_bytes;                                                                       ///<总据总字节数

            void *mem_end;                                                                          ///<内存数据区访问结束地址

        protected:

            uint data_usage;                                                                        ///<数据使用方式

            VertexBufferControl *vbc;                                                               ///<顶点缓冲区控制器

        protected:

            void SetDataSize(int size);

        public:

            /**
             * @param type 缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
             * @param dt 单个数据类型 (GL_BYTE,GL_UNSIGNED_SHORT,GL_FLOAT等)
             * @param dbyte 单个数据字节数 (GL_BYTE为1,GL_UNSIGNED_SHORT为2，GL_FLOAT为4等)
             * @param dcm 数据成员数 (1/2/3/4，如2D纹理坐标用2，3D坐标用3)
             * @param size 数据数量
             * @param usage 存取使用方式(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)
             */
            VertexBufferBase(uint type,uint dt,uint dbyte,uint dcm,uint size,uint usage);
            virtual ~VertexBufferBase();

            uint    GetBufferType   ()const         {return vb_type;}                               ///<取得缓冲区类型
            uint    GetDataType     ()const         {return data_type;}                             ///<取得数据类型
            uint    GetComponent    ()const         {return dc_num;}                                ///<取数每一组数据中的数据数量
            uint    GetCount        ()const         {return count;}                                 ///<取得数据数量
            uint    GetStride       ()const         {return dc_num*data_bytes;}                     ///<取得每一组数据字节数
            uint    GetTotalBytes   ()const         {return total_bytes;}                           ///<取得数据总字节数
            void *  GetData         ()const         {return mem_data;}                              ///<取得数据指针
            void *  GetData         (const uint off){return ((char *)mem_data)+data_bytes*off;}     ///<取得数据指针

        public:

            void    Update();                                                                       ///<完整更新内存中的数据到显示

            void    Change(int,int,void *);
            //void  BindVertexBuffer();
            int     GetBufferIndex()const;                                                          ///<取得缓冲区索引
        };//class VertexBufferBase
    }//namespace graph
}//namespace hgl
#endif//HGL_VERTEX_BUFFER_BASE_INCLUDE

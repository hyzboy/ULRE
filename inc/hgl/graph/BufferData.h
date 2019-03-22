#ifndef HGL_GRAPH_BUFFER_DATA_INCLUDE
#define HGL_GRAPH_BUFFER_DATA_INCLUDE

#include<GLEWCore/glew.h>
#include<hgl/type/DataType.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 缓冲区数据管理类
         */
        class BufferData
        {
        protected:

            GLenum      data_type;                                                                  ///<单个数据类型 (GL_BYTE,GL_UNSIGNED_SHORT,GL_FLOAT等)
            uint        data_bytes;                                                                 ///<单个数据字节数 (GL_BYTE为1,GL_UNSIGNED_SHORT为2,GL_FLOAT为4等)
            uint        data_comp;                                                                  ///<数据成员数 (1/2/3/4，如2D纹理坐标用2，3D坐标/法线用3)

            uint        data_stride;                                                                ///<每组数据字节数

            GLsizeiptr  data_count;                                                                 ///<数据数量
            GLsizeiptr  total_bytes;                                                                ///<数据总字节数

            char *      buffer_data;

        protected:

            friend BufferData *CreateBufferData(const GLenum &dt,const uint &dbytes,const uint &dcm,const GLsizeiptr &count);
            friend BufferData *CreateBufferData(void *data,const GLenum &dt,const uint &dbytes,const uint &dcm,const GLsizeiptr &count);

            BufferData(const GLenum &dt,const uint &dbytes,const uint &dcm,const GLsizeiptr &count,char *data)
            {
                data_type   =dt;
                data_bytes  =dbytes;
                data_comp   =dcm;

                data_stride =data_comp*data_bytes;

                data_count  =count;
                total_bytes =data_stride*data_count;

                buffer_data =data;
            }

        public:

            virtual ~BufferData()=default;

            GLenum      GetDataType     ()const {return data_type;}                                 ///<取得数据类型
            uint        GetComponent    ()const {return data_comp;}                                 ///<取数每一组数据中的数据数量
            uint        GetStride       ()const {return data_stride;}                               ///<取得每一组数据字节数

            GLsizeiptr  GetCount        ()const {return data_count;}                                ///<取得数据数量
            GLsizeiptr  GetTotalBytes   ()const {return total_bytes;}                               ///<取得数据总字节数

        public:

            void *      GetData         ()const {return buffer_data;}                               ///<取得数据指针
        };//class BufferData

        /**
         * 创建一个数据缓冲区<br>
         * 这种方式创建的缓冲区，它会自行分配内存，最终释放
         * @param dt 单个数据类型 (GL_BYTE,GL_UNSIGNED_SHORT,GL_FLOAT等)
         * @param dbytes 单个数据字节数 (GL_BYTE为1,GL_UNSIGNED_SHORT为2,GL_FLOAT为4等)
         * @param dcm 数据成员数 (1/2/3/4，如2D纹理坐标用2，3D坐标/法线用3)
         * @param count 数据数量
         */
        BufferData *CreateBufferData(const GLenum &dt,const uint &dbytes,const uint &dcm,const GLsizeiptr &count);

        /**
         * 创建一个数据缓冲区
         * @param data 数据指针
         * @param dt 单个数据类型 (GL_BYTE,GL_UNSIGNED_SHORT,GL_FLOAT等)
         * @param dbytes 单个数据字节数 (GL_BYTE为1,GL_UNSIGNED_SHORT为2,GL_FLOAT为4等)
         * @param dcm 数据成员数 (1/2/3/4，如2D纹理坐标用2，3D坐标/法线用3)
         * @param count 数据数量
         */
        BufferData *CreateBufferData(void *data,const GLenum &dt,const uint &dbytes,const uint &dcm,const GLsizeiptr &count);
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_BUFFER_DATA_INCLUDE

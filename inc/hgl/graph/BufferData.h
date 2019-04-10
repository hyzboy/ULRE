﻿#ifndef HGL_GRAPH_BUFFER_DATA_INCLUDE
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

            GLsizeiptr  total_bytes;                                                                ///<数据总字节数

            char *      buffer_data;

        protected:

            friend BufferData *CreateBufferData(void *data,const GLsizeiptr &length);

            BufferData(char *data,const GLsizeiptr &length)
            {   
                total_bytes =length;
                buffer_data =data;
            }

        public:

            virtual ~BufferData()=default;

            GLsizeiptr  GetTotalBytes   ()const {return total_bytes;}                               ///<取得数据总字节数
            void *      GetData         ()const {return buffer_data;}                               ///<取得数据指针
        };//class BufferData

        BufferData *CreateBufferData(const GLsizeiptr &length);
        BufferData *CreateBufferData(void *data,const GLsizeiptr &length);

        class VertexBufferData:public BufferData
        {
            GLenum      data_type;                                                                  ///<单个数据类型 (GL_BYTE,GL_UNSIGNED_SHORT,GL_FLOAT等)
            uint        data_bytes;                                                                 ///<单个数据字节数 (GL_BYTE为1,GL_UNSIGNED_SHORT为2,GL_FLOAT为4等)
            uint        data_comp;                                                                  ///<数据成员数 (1/2/3/4，如2D纹理坐标用2，3D坐标/法线用3)

            uint        data_stride;                                                                ///<每组数据字节数

            GLsizeiptr  data_count;                                                                 ///<数据数量

        protected:

            friend VertexBufferData *CreateVertexBufferData(void *data,const GLenum &dt,const uint &dbytes,const uint &dcm,const GLsizeiptr &count);

            VertexBufferData(const GLenum &dt,const uint &dbytes,const uint &dcm,const GLsizeiptr &count,char *data):BufferData(data,dbytes*dcm*count)
            {
                data_type=dt;
                data_bytes=dbytes;
                data_comp=dcm;

                data_stride=data_comp*data_bytes;

                data_count=count;
            }

        public:

            virtual ~VertexBufferData()=default;

            GLenum      GetDataType     ()const{return data_type;}                                  ///<取得数据类型
            uint        GetComponent    ()const{return data_comp;}                                  ///<取数每一组数据中的数据数量
            uint        GetStride       ()const{return data_stride;}                                ///<取得每一组数据字节数

            GLsizeiptr  GetCount        ()const{return data_count;}                                 ///<取得数据数量
            GLsizeiptr  GetTotalBytes   ()const{return total_bytes;}                                ///<取得数据总字节数
        };

        /**
         * 创建一个顶点数据缓冲区<br>
         * 这种方式创建的缓冲区，它会自行分配内存，最终释放
         * @param dt 单个数据类型 (GL_BYTE,GL_UNSIGNED_SHORT,GL_FLOAT等)
         * @param dbytes 单个数据字节数 (GL_BYTE为1,GL_UNSIGNED_SHORT为2,GL_FLOAT为4等)
         * @param dcm 数据成员数 (1/2/3/4，如2D纹理坐标用2，3D坐标/法线用3)
         * @param count 数据数量
         */
        VertexBufferData *CreateVertexBufferData(const GLenum &dt,const uint &dbytes,const uint &dcm,const GLsizeiptr &count);

        /**
         * 创建一个顶点数据缓冲区
         * @param data 数据指针
         * @param dt 单个数据类型 (GL_BYTE,GL_UNSIGNED_SHORT,GL_FLOAT等)
         * @param dbytes 单个数据字节数 (GL_BYTE为1,GL_UNSIGNED_SHORT为2,GL_FLOAT为4等)
         * @param dcm 数据成员数 (1/2/3/4，如2D纹理坐标用2，3D坐标/法线用3)
         * @param count 数据数量
         */
        VertexBufferData *CreateVertexBufferData(void *data,const GLenum &dt,const uint &dbytes,const uint &dcm,const GLsizeiptr &count);

    #define VBDATA_CREATE_FUNC(type,gl_type,short_name) \
        inline VertexBufferData *VB1##short_name(const GLsizeiptr &count){return CreateVertexBufferData(gl_type,sizeof(type),1,count);} \
        inline VertexBufferData *VB2##short_name(const GLsizeiptr &count){return CreateVertexBufferData(gl_type,sizeof(type),2,count);} \
        inline VertexBufferData *VB3##short_name(const GLsizeiptr &count){return CreateVertexBufferData(gl_type,sizeof(type),3,count);} \
        inline VertexBufferData *VB4##short_name(const GLsizeiptr &count){return CreateVertexBufferData(gl_type,sizeof(type),4,count);} \
        \
        inline VertexBufferData *VB1##short_name(const GLsizeiptr &count,const type *data){return CreateVertexBufferData((void *)data,gl_type,sizeof(type),1,count);} \
        inline VertexBufferData *VB2##short_name(const GLsizeiptr &count,const type *data){return CreateVertexBufferData((void *)data,gl_type,sizeof(type),2,count);} \
        inline VertexBufferData *VB3##short_name(const GLsizeiptr &count,const type *data){return CreateVertexBufferData((void *)data,gl_type,sizeof(type),3,count);} \
        inline VertexBufferData *VB4##short_name(const GLsizeiptr &count,const type *data){return CreateVertexBufferData((void *)data,gl_type,sizeof(type),4,count);}  

        VBDATA_CREATE_FUNC(int8,  GL_BYTE,    i8)
        VBDATA_CREATE_FUNC(int8,  GL_BYTE,    b)
        VBDATA_CREATE_FUNC(int16, GL_SHORT,   i16)
        VBDATA_CREATE_FUNC(int16, GL_SHORT,   s)
        VBDATA_CREATE_FUNC(int32, GL_INT,     i32)
        VBDATA_CREATE_FUNC(int32, GL_INT,     i)

        VBDATA_CREATE_FUNC(uint8, GL_UNSIGNED_BYTE,   u8)
        VBDATA_CREATE_FUNC(uint8, GL_UNSIGNED_BYTE,   ub)
        VBDATA_CREATE_FUNC(uint16,GL_UNSIGNED_SHORT,  u16)
        VBDATA_CREATE_FUNC(uint16,GL_UNSIGNED_SHORT,  us)
        VBDATA_CREATE_FUNC(uint32,GL_UNSIGNED_INT,    u32)
        VBDATA_CREATE_FUNC(uint32,GL_UNSIGNED_INT,    ui)

        VBDATA_CREATE_FUNC(uint16,GL_HALF_FLOAT,  hf)
        VBDATA_CREATE_FUNC(uint16,GL_HALF_FLOAT,  f16)
        VBDATA_CREATE_FUNC(float, GL_FLOAT,       f)
        VBDATA_CREATE_FUNC(float, GL_FLOAT,       f32)
        VBDATA_CREATE_FUNC(double,GL_DOUBLE,      d)
        VBDATA_CREATE_FUNC(double,GL_DOUBLE,      f64)        
    #undef VBDATA_CREATE_FUNC
        
        inline VertexBufferData *EB16(const uint16 &count){return CreateVertexBufferData(GL_UNSIGNED_SHORT, 2,1,count);}
        inline VertexBufferData *EB32(const uint32 &count){return CreateVertexBufferData(GL_UNSIGNED_INT,   4,1,count);}
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_BUFFER_DATA_INCLUDE
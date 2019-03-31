#ifndef HGL_GRAPH_BUFFER_OBJECT_INCLUDE
#define HGL_GRAPH_BUFFER_OBJECT_INCLUDE

#include<hgl/graph/BufferData.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 显存数据缓冲区对象<br>
         * 负责对象与API的交接
         */
        class BufferObject
        {
        protected:

            GLuint      buffer_index;                                                               ///<缓冲区索引
            GLenum      buffer_type;                                                                ///<缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
            GLenum      user_pattern;                                                               ///<数据存储区使用模式(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)

            GLsizeiptr  buffer_bytes;
            const BufferData *buffer_data;

        public:

            BufferObject(GLenum type);
            virtual ~BufferObject();

        public:

                    GLuint      GetBufferIndex  ()const {return buffer_index;}                      ///<取得OpenGL缓冲区
                    GLenum      GetBufferType   ()const {return buffer_type;}                       ///<取得缓冲区类型
                    GLenum      GetUserPattern  ()const {return user_pattern;}                      ///<取得缓冲区使用方法

            const   BufferData *GetBufferData   ()const {return buffer_data;}                       ///<取得缓冲数区(这里返回const是为了不想让只有BufferObject的模块可以修改数据)
            const   GLsizeiptr  GetBufferSize   ()const {return buffer_bytes;}                      ///<取得缓冲区总计字数

        public:

                    bool        Create          (GLsizeiptr);                                       ///<创建数据区
                    bool        Submit          (void *,GLsizeiptr,GLenum up=GL_STATIC_DRAW);       ///<提交数据
                    bool        Submit          (const BufferData *,GLenum up=GL_STATIC_DRAW);      ///<提交数据
                    bool        Change          (void *,GLsizeiptr,GLsizeiptr);                     ///<修改数据
        };//class BufferObject

        /**
         * 显存顶点属性数据缓冲区对象
         */
        class VertexBufferObject:public BufferObject
        {
        public:

            using BufferObject::BufferObject;
            ~VertexBufferObject()=default;

            const   VertexBufferData *GetVertexBufferData()const{return (const VertexBufferData *)buffer_data;}

        #define VBD_FUNC_COPY(type,name)    type Get##name()const{return buffer_data?((const VertexBufferData *)buffer_data)->Get##name():0;}

            VBD_FUNC_COPY(GLenum,DataType)
            VBD_FUNC_COPY(uint,Component)
            VBD_FUNC_COPY(uint,Stride)
            VBD_FUNC_COPY(GLsizeiptr,Count)

        #undef VBD_FUNC_COPY
        };//class VertexBufferObject:public BufferObject

        /**
         * 创建一个缓冲区对象
         * @param buf_type 缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
         * @param user_pattern 数据存储区使用模式(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)
         * @param buf 数据缓冲区
         */
        template<typename BO,typename BD>
        inline BO *_CreateBufferObject(BD *buf=nullptr,const GLenum &user_pattern=GL_STATIC_DRAW)
        {
            BO *obj=new BO();

            if(buf)
                obj->Submit(buf,user_pattern);

            return(obj);
        }

        ///**
        // * 创建一个缓冲区对象
        // * @param buf_type 缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
        // * @param user_pattern 数据存储区使用模式(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)
        // * @param total_bytes 数据总字节数
        // */
        //template<typename BO>
        //inline BO *_CreateBufferObject(const GLenum &buf_type,const GLenum &user_pattern,const GLsizeiptr &total_bytes)
        //{
        //    if(total_bytes<=0)return(nullptr);

        //    BO *buf=new BO(buf_type);

        //    //if(buf->Create(total_bytes,user_pattern))
        //    //    return buf;

        //    delete buf;
        //    return(nullptr);
        //}

        /**
         * 创建一个缓冲区对象
         * @param buf_type 缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
         * @param user_pattern 数据存储区使用模式(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)
         * @param total_bytes 数据总字节数
         * @param data 数据指针
         */
        template<typename BO>
        inline BO *_CreateBufferObject(void *data,const GLsizeiptr &total_bytes,const GLenum &user_pattern=GL_STATIC_DRAW)
        {
            if(total_bytes<=0)return(nullptr);
            if(!data)return(nullptr);

            BO *buf=new BO();

            //if(buf->Create(total_bytes,user_pattern))
            //    return buf;

            if(buf->Submit(data,total_bytes,user_pattern))
                return buf;

            delete buf;
            return(nullptr);
        }

    #define VBCLASS_DEF(buffer_gl_type,buffer_class_name,BASE,data_name,short_name)  \
        class buffer_class_name:public BASE   \
        {   \
        public: \
            \
            buffer_class_name():BASE(buffer_gl_type){}    \
            ~buffer_class_name()=default;   \
        };  \
        \
        inline buffer_class_name *Create##short_name() \
        {   \
            return(new buffer_class_name());   \
        };  \
        \
        inline buffer_class_name *Create##short_name(data_name *buf=nullptr,const GLenum user_pattern=GL_STATIC_DRAW)   \
        {   \
            return _CreateBufferObject<buffer_class_name,data_name>(buf,user_pattern);   \
        };  \
        \
        inline buffer_class_name *Create##short_name(void *data,const GLsizeiptr &size,const GLenum &user_pattern=GL_STATIC_DRAW)   \
        {   \
            return _CreateBufferObject<buffer_class_name>(data,size,user_pattern); \
        }

        //ps.在这里用宏了再用模板本是多此一举，但使用模板函数易于调试器中进行逐行调试，同时因为INLINE编译编译器也会自动展开代码，不用担心效率

        VBCLASS_DEF(GL_ARRAY_BUFFER,            ArrayBuffer,        VertexBufferObject, VertexBufferData,   VBO)
        VBCLASS_DEF(GL_ELEMENT_ARRAY_BUFFER,    ElementBuffer,      VertexBufferObject, VertexBufferData,   EBO)
        VBCLASS_DEF(GL_UNIFORM_BUFFER,          UniformBuffer,      BufferObject,       BufferData,         UBO)
        VBCLASS_DEF(GL_SHADER_STORAGE_BUFFER,   ShaderStorageBuffer,BufferObject,       BufferData,         SSBO)

    #undef VBCLASS_DEF
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_BUFFER_OBJECT_INCLUDE

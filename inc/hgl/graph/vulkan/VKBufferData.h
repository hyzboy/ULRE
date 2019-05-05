#ifndef HGL_GRAPH_VULKAN_BUFFER_DATA_INCLUDE
#define HGL_GRAPH_BUFFER_DATA_INCLUDE

#include"VK.h"
VK_NAMESPACE_BEGIN
    /**
        * 缓冲区数据管理类
        */
    class BufferData
    {
    protected:

        VkFormat    format;                                                                         ///<数据格式

        uint32_t    count;                                                                          ///<数据个数
        uint32_t    stride;                                                                         ///<单个数据字节数

        uint8_t *   buffer_data;                                                                    ///<缓冲区数据
        uint32_t    total_bytes;                                                                    ///<数据总字节数

    protected:

        friend BufferData *CreateBufferData(void *data,const VkFormat f,const uint32_t count,const uint32_t stride);

        BufferData(uint8_t *data,const VkFormat f,const uint32_t c,const uint32_t s)
        {
            buffer_data =data;

            format=f;
            count=c;
            stride=s;

            total_bytes=stride*count;
        }

    public:

        virtual ~BufferData()=default;

        uint      GetStride     ()const { return data_stride; }                                     ///<取得每一组数据字节数
        uint32_t  GetCount      ()const { return data_count; }                                      ///<取得数据数量
        uint32_t  GetTotalBytes ()const { return total_bytes; }                                     ///<取得数据总字节数
        void *    GetData       ()const { return buffer_data; }                                     ///<取得数据指针
    };//class BufferData

    BufferData *CreateBufferData(const uint32_t &length);
    BufferData *CreateBufferData(void *data,const uint32_t &length);

    #define DATA_COMPOMENT_RED      0x01
    #define DATA_COMPOMENT_GREEN    0x02
    #define DATA_COMPOMENT_BLUE     0x04
    #define DATA_COMPOMENT_ALPHA    0x08
    #define DATA_COMPOMENT_X        DATA_COMPOMENT_RED
    #define DATA_COMPOMENT_Y        DATA_COMPOMENT_GREEN
    #define DATA_COMPOMENT_Z        DATA_COMPOMENT_BLUE
    #define DATA_COMPOMENT_W        DATA_COMPOMENT_ALPHA

    #define DATA_COMPOMENT_DEPTH    0x10

    /**
     * 非打包型数据<Br>
     * 该类数据未被打包，所以可以直接对成份数据逐一访问
     */
    class BufferDataDirect:public BufferData
    {
        VkFormat
    };//

    /**
     * 打包型数据<Br>
     * 该类数据由于被打包，所以无法直接进行读写
     */
    class BufferDataPack:public BufferData
    {
        VkFormat format;                                                                            ///<数据格式
        uint byte;                                                                                  ///<单个数据字节数

        uint compoment;                                                                             ///<数据成份

    public:


    };//

    class VertexBufferData:public BufferData
    {
        uint32_t    data_type;                                                                      ///<单个数据类型 (GL_BYTE,GL_UNSIGNED_SHORT,GL_FLOAT等)
        uint        data_bytes;                                                                     ///<单个数据字节数 (GL_BYTE为1,GL_UNSIGNED_SHORT为2,GL_FLOAT为4等)
        uint        data_comp;                                                                      ///<数据成员数 (1/2/3/4，如2D纹理坐标用2，3D坐标/法线用3)

        uint        data_stride;                                                                    ///<每组数据字节数

        uint32_t    data_count;                                                                     ///<数据数量

    protected:

        friend VertexBufferData *CreateVertexBufferData(void *data,const uint32_t &dt,const uint &dbytes,const uint &dcm,const uint32_t &count);

        VertexBufferData(const uint32_t &dt,const uint &dbytes,const uint &dcm,const uint32_t &count,char *data):BufferData(data,dbytes*dcm*count)
        {
            data_type=dt;
            data_bytes=dbytes;
            data_comp=dcm;

            data_stride=data_comp*data_bytes;

            data_count=count;
        }

    public:

        virtual ~VertexBufferData()=default;

        uint32_t    GetDataType     ()const{return data_type;}                                      ///<取得数据类型
        uint        GetComponent    ()const{return data_comp;}                                      ///<取数每一组数据中的数据数量
        uint        GetStride       ()const{return data_stride;}                                    ///<取得每一组数据字节数

        uint32_t    GetCount        ()const{return data_count;}                                     ///<取得数据数量
        uint32_t    GetTotalBytes   ()const{return total_bytes;}                                    ///<取得数据总字节数
    };

    /**
        * 创建一个顶点数据缓冲区<br>
        * 这种方式创建的缓冲区，它会自行分配内存，最终释放
        * @param dt 单个数据类型 (GL_BYTE,GL_UNSIGNED_SHORT,GL_FLOAT等)
        * @param dbytes 单个数据字节数 (GL_BYTE为1,GL_UNSIGNED_SHORT为2,GL_FLOAT为4等)
        * @param dcm 数据成员数 (1/2/3/4，如2D纹理坐标用2，3D坐标/法线用3)
        * @param count 数据数量
        */
    VertexBufferData *CreateVertexBufferData(const uint32_t &dt,const uint &dbytes,const uint &dcm,const uint32_t &count);

    /**
        * 创建一个顶点数据缓冲区
        * @param data 数据指针
        * @param dt 单个数据类型 (GL_BYTE,GL_UNSIGNED_SHORT,GL_FLOAT等)
        * @param dbytes 单个数据字节数 (GL_BYTE为1,GL_UNSIGNED_SHORT为2,GL_FLOAT为4等)
        * @param dcm 数据成员数 (1/2/3/4，如2D纹理坐标用2，3D坐标/法线用3)
        * @param count 数据数量
        */
    VertexBufferData *CreateVertexBufferData(void *data,const uint32_t &dt,const uint &dbytes,const uint &dcm,const uint32_t &count);

#define VBDATA_CREATE_FUNC(short_name,type,comp_count,vk_type) \
    inline VertexBufferData *VB##comp_count##short_name(const uint32_t &count){return CreateVertexBufferData(vk_type,sizeof(type),comp_count,count);} \
    inline VertexBufferData *VB##comp_count##short_name(const uint32_t &count,const type *data){return CreateVertexBufferData((void *)data,vk_type,sizeof(type),comp_count,count);}

    // UNORM 指输入无符号数，自动转换为 0.0 to  1.0 的浮点数
    // SNORM 指输入有符号数，自动转换为-1.0 to +1.0 的浮点数



#define VBDATA_NSI(comp_count,type,vk_type_start)   \
    VBDATA_CREATE_FUNC(type,comp_count,


#define VBDATA_UIF(comp_count,utype,itype,ftype,vk_type_start)    \
    VBDATA_CREATE_FUNC(u,utype,comp_count,vk_type_start);
    VBDATA_CREATE_FUNC(i,itype,comp_count,vk_type_start+1);
    VBDATA_CREATE_FUNC(f,ftype,comp_count,vk_type_start+2);

#define VBDATA_UIF_1234(utype,itype,ftype,vk_type_start)   \
    VBDATA_UIF(1,utype,itype,ftype,vk_type_start)  \
    VBDATA_UIF(2,utype,itype,ftype,vk_type_start+3)  \
    VBDATA_UIF(3,utype,itype,ftype,vk_type_start+6)  \
    VBDATA_UIF(4,utype,itype,ftype,vk_type_start+9)

    VBDATA_UIF_1234(uint32,int32,float ,VK_FORMAT_R32_UINT)
    VBDATA_UIF_1234(uint64,int64,double,VK_FORMAT_R64_UINT)
#undef VBDATA_CREATE_FUNC

    //inline VertexBufferData *EB16(const uint16 &count){return CreateVertexBufferData(GL_UNSIGNED_SHORT, 2,1,count);}
    //inline VertexBufferData *EB32(const uint32 &count){return CreateVertexBufferData(GL_UNSIGNED_INT,   4,1,count);}
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_BUFFER_DATA_INCLUDE

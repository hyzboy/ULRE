#include<hgl/io/FileInputStream.h>
#include<hgl/type/String.h>
#include<hgl/log/Log.h>
#include<hgl/graph/VKGeometry.h>
#include<hgl/graph/VKPrimitiveType.h>
#include<hgl/graph/VKVertexInputLayout.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/graph/Bounds.h>
#include"VKGeometryData.h"

DEFINE_LOGGER_MODULE(LoadGeometry)

VK_NAMESPACE_BEGIN
namespace
{
    constexpr const char   GeometryFileMagic[]      = "Geometry\x1A";
    constexpr const size_t GeometryFileMagicBytes   = sizeof(GeometryFileMagic) - 1;

#pragma pack(push,1)
    struct GeometryFileHeader
    {
        uint16_t version;        // 1
        uint8_t  primitiveType;  // PrimitiveType as uint8_t
        uint32_t vertexCount;    // Number of vertices
        uint8_t  indexStride;    // 0 if no indices, otherwise 1,2,4
        uint32_t indexCount;     // Number of indices (0 if no indices)
        uint8_t  attributeCount; // Number of attributes
    };
#pragma pack(pop)

    bool LoadBounds(Bounds &bounds,io::InputStream *is)
    {
        BoundsData data;

        if(is->Read(&data,sizeof(BoundsData))!=BoundsDataBytes)
            return(false);

        bounds.aabb.SetMinMax(Vector3f(data.aabb.min[0],data.aabb.min[1],data.aabb.min[2]),
                              Vector3f(data.aabb.max[0],data.aabb.max[1],data.aabb.max[2]));

        bounds.obb.Set( Vector3f(data.obb.center[0],data.obb.center[1],data.obb.center[2]),
                        Vector3f(data.obb.axis[0][0],data.obb.axis[0][1],data.obb.axis[0][2]),
                        Vector3f(data.obb.axis[1][0],data.obb.axis[1][1],data.obb.axis[1][2]),
                        Vector3f(data.obb.axis[2][0],data.obb.axis[2][1],data.obb.axis[2][2]),
                        Vector3f(data.obb.half_size[0],data.obb.half_size[1],data.obb.half_size[2]));

        bounds.bsphere.center = Vector3f(data.bsphere.center[0],data.bsphere.center[1],data.bsphere.center[2]);
        bounds.bsphere.radius = data.bsphere.radius;

        return(true);
    }
}//namespace

Geometry *LoadGeometry(VulkanDevice *device,const VIL *vil,const OSString &filename)
{
    io::OpenFileInputStream fis(filename);

    if(!fis)
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot open file ") + filename + OS_TEXT(" for reading."));
        return(nullptr);
    }

    char file_magic[GeometryFileMagicBytes];

    if(fis->Read(file_magic,GeometryFileMagicBytes)!=GeometryFileMagicBytes)
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot read file ID from file ") + filename);
        return(nullptr);
    }

    if(memcmp(file_magic,GeometryFileMagic,GeometryFileMagicBytes)!=0)
    {
        MLogError(LoadGeometry,OS_TEXT("File ID mismatch in file ") + filename);
        return(nullptr);
    }

    GeometryFileHeader header;

    if(fis->Read(&header,sizeof(GeometryFileHeader))!=sizeof(GeometryFileHeader))
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot read header from file ") + filename);
        return(nullptr);
    }

    if(header.version!=1)
    {
        MLogError(LoadGeometry,OS_TEXT("Unsupported version in file ") + filename);
        return(nullptr);
    }

    if(header.primitiveType>static_cast<uint8_t>(PrimitiveType::Patchs))
    {
        MLogError(LoadGeometry,OS_TEXT("Unsupported primitive type ")+OSString::numberOf(header.primitiveType)+OS_TEXT(" in file ") + filename);
        return(nullptr);
    }

    Bounds bounds;

    if(!LoadBounds(bounds,&fis))
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot read bounds from file ") + filename);
        return(nullptr);
    }

    GeometryData *geo_data=CreateGeometryData(device,vil,header.vertexCount);

    if(!geo_data)
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot create GeometryData for file ") + filename);
        return(nullptr);
    }

    if(!geo_data->CreateAllVAB())
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot create VAB for file ") + filename);
        delete geo_data;
        return(nullptr);
    }

    if(header.attributeCount>0)     //为0的情况也是存在的
    {
        AutoDeleteArray<uint8_t> attribute_format(header.attributeCount);
        AutoDeleteArray<uint8_t> attribute_name_length(header.attributeCount);
        AutoDeleteArray<char *>  attribute_name(header.attributeCount);

        if(fis->Read(attribute_format,header.attributeCount)!=header.attributeCount)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot read attribute formats from file ") + filename);
            return(nullptr);
        }

        if(fis->Read(attribute_name_length,header.attributeCount)!=header.attributeCount)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot read attribute name lengths from file ") + filename);
            return(nullptr);
        }

        uint total_name_length;

        sum(&total_name_length,attribute_name_length.data(),header.attributeCount);

        total_name_length += header.attributeCount; //加上每个名称的结尾0

        AutoDeleteArray<char> name_buffer(total_name_length);

        if(fis->Read(name_buffer,total_name_length)!=total_name_length)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot read attribute names from file ") + filename);
            return(nullptr);
        }

        char *p = name_buffer;
        for(size_t i = 0;i < header.attributeCount;++i)
        {
            attribute_name[i] = p;
            p += attribute_name_length[i] + 1;
        }

        // 读取属性数据
        for(size_t i = 0;i < header.attributeCount;++i)
        {
            const int vab_index = geo_data->GetVABIndex(AnsiString(attribute_name[i]));

            const size_t attribute_size = header.vertexCount * GetStrideByFormat(static_cast<VkFormat>(attribute_format[i]));

            if(vab_index<0)
            {
                //文件有，材质没有

                MLogNotice(LoadGeometry,OS_TEXT("Attribute name '") + ToOSString(attribute_name[i]) + OS_TEXT("' not found in VIL, file ") + filename);

                fis->Skip(attribute_size);
                continue;
                //delete geo_data;
                //return(nullptr);
            }

            const VIF *vif = vil->GetConfig(vab_index);

            if(vif->format != static_cast<VkFormat>(attribute_format[i]))
            {
                MLogError(LoadGeometry,OS_TEXT("Attribute format mismatch for attribute '") + ToOSString(attribute_name[i]) + OS_TEXT("' in file ") + filename);
                delete geo_data;
                return(nullptr);
            }

            VAB *vab = geo_data->GetVAB(vab_index);

            if(!vab)
            {
                MLogError(LoadGeometry,OS_TEXT("Cannot get VAB for attribute '") + ToOSString(attribute_name[i]) + OS_TEXT("' in file ") + filename);
                delete geo_data;
                return(nullptr);
            }

            void *vab_ptr=vab->Map(0,header.vertexCount);

            if(!vab_ptr)
            {
                MLogError(LoadGeometry,OS_TEXT("Cannot map VAB for attribute '") + ToOSString(attribute_name[i]) + OS_TEXT("' in file ") + filename);
                delete geo_data;
                return(nullptr);
            }

            if(fis->Read(vab_ptr,attribute_size)!=attribute_size)
            {
                MLogError(LoadGeometry,OS_TEXT("Cannot read attribute data for attribute '") + ToOSString(attribute_name[i]) + OS_TEXT("' from file ") + filename);
                delete geo_data;
                return(nullptr);
            }

            vab->Unmap();
        }
    }//if(header.attributeCount>0)

    if(header.indexCount>0)
    {
        IndexType index_type;
        if(header.indexStride==1)index_type=IndexType::U8;else
        if(header.indexStride==2)index_type=IndexType::U16;else
        if(header.indexStride==4)index_type=IndexType::U32;else
        {
            MLogError(LoadGeometry,OS_TEXT("Unsupported index stride ")+OSString::numberOf(header.indexStride)+OS_TEXT(" in file ") + filename);
            delete geo_data;
            return(nullptr);
        }

        IndexBuffer *ibo=geo_data->InitIBO(header.indexCount,index_type);
        if(!ibo)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot create IBO for file ") + filename);
            delete geo_data;
            return(nullptr);
        }

        void *ibo_ptr=ibo->Map(0,header.indexCount);
        if(!ibo_ptr)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot map IBO for file ") + filename);
            delete geo_data;
            return(nullptr);
        }

        const size_t index_size = header.indexCount * header.indexStride;
        if(fis->Read(ibo_ptr,index_size)!=index_size)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot read index data from file ") + filename);
            delete geo_data;
            return(nullptr);
        }

        ibo->Unmap();
    }

    const U8String geo_name=ToU8String(filename);

    Geometry *geometry = new Geometry((char *)geo_name.c_str(),geo_data);

    if(!geometry)
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot create Geometry object for file ") + filename);
        delete geo_data;
        return(nullptr);
    }

    geometry->SetBoundingBox(bounds.aabb);

    return geometry;
}
VK_NAMESPACE_END

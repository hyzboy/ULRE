#include<hgl/io/FileInputStream.h>
#include<hgl/type/String.h>
#include<hgl/log/Log.h>
#include<hgl/graph/VKGeometry.h>
#include<hgl/graph/VKPrimitiveType.h>
#include<hgl/graph/VKVertexInputLayout.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/graph/Bounds.h>
#include"VKGeometryData.h"
#include<hgl/io/MiniPack.h>
#include<hgl/io/MemoryInputStream.h>

DEFINE_LOGGER_MODULE(LoadGeometry)

VK_NAMESPACE_BEGIN
namespace
{
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
}

Geometry *LoadGeometry(VulkanDevice *device,const VIL *vil,const OSString &filename)
{
    using namespace hgl::io::minipack;

    MiniPackReader *mpr = GetMiniPackReader(filename);

    if(!mpr)
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot open minipack file ") + filename + OS_TEXT(" for reading."));
        return(nullptr);
    }

    auto MakeU8FromAscii = [](const char *s)->hgl::U8String
    {
        if(!s) return hgl::U8String();
        const int len = hgl::strlen(s);
        hgl::U8String r;
        hgl::u8char *dst = reinterpret_cast<hgl::u8char *>(r.Resize(len));
        for(int i=0;i<len;++i) dst[i] = static_cast<hgl::u8char>(s[i]);
        return r;
    };

    // 1) Read GeometryHeader
    const int32 header_index = mpr->FindFile(MakeU8FromAscii("GeometryHeader"));
    if(header_index < 0)
    {
        MLogError(LoadGeometry,OS_TEXT("GeometryHeader not found in file ") + filename);
        delete mpr;
        return(nullptr);
    }

    if(mpr->GetFileLength(header_index) != sizeof(GeometryFileHeader))
    {
        MLogError(LoadGeometry,OS_TEXT("GeometryHeader size mismatch in file ") + filename);
        delete mpr;
        return(nullptr);
    }

    GeometryFileHeader header{};
    if(mpr->ReadFile(header_index, &header, 0, sizeof(header)) != sizeof(header))
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot read GeometryHeader from file ") + filename);
        delete mpr;
        return(nullptr);
    }

    if(header.version!=1)
    {
        MLogError(LoadGeometry,OS_TEXT("Unsupported version in file ") + filename);
        delete mpr;
        return(nullptr);
    }

    if(header.primitiveType>static_cast<uint8_t>(PrimitiveType::Patchs))
    {
        MLogError(LoadGeometry,OS_TEXT("Unsupported primitive type ")+OSString::numberOf(header.primitiveType)+OS_TEXT(" in file ") + filename);
        delete mpr;
        return(nullptr);
    }

    // 2) Read Bounds
#pragma pack(push,1)
    struct PackedBounds
    {
        double aabbMin[3];
        double aabbMax[3];
        double obbCenter[3];
        double obbAxisX[3];
        double obbAxisY[3];
        double obbAxisZ[3];
        double obbHalfSize[3];
        double sphereCenter[3];
        double sphereRadius;
    };
#pragma pack(pop)

    const int32 bounds_index = mpr->FindFile(MakeU8FromAscii("Bounds"));
    if(bounds_index < 0)
    {
        MLogError(LoadGeometry,OS_TEXT("Bounds not found in file ") + filename);
        delete mpr;
        return(nullptr);
    }

    if(mpr->GetFileLength(bounds_index) != sizeof(PackedBounds))
    {
        MLogError(LoadGeometry,OS_TEXT("Bounds size mismatch in file ") + filename);
        delete mpr;
        return(nullptr);
    }

    PackedBounds pb{};
    if(mpr->ReadFile(bounds_index, &pb, 0, sizeof(PackedBounds)) != sizeof(PackedBounds))
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot read Bounds from file ") + filename);
        delete mpr;
        return(nullptr);
    }

    Bounds bounds;
    bounds.aabb.SetMinMax(
        Vector3f(float(pb.aabbMin[0]), float(pb.aabbMin[1]), float(pb.aabbMin[2])),
        Vector3f(float(pb.aabbMax[0]), float(pb.aabbMax[1]), float(pb.aabbMax[2])));
    bounds.obb.Set(
        Vector3f(float(pb.obbCenter[0]), float(pb.obbCenter[1]), float(pb.obbCenter[2])),
        Vector3f(float(pb.obbAxisX[0]), float(pb.obbAxisX[1]), float(pb.obbAxisX[2])),
        Vector3f(float(pb.obbAxisY[0]), float(pb.obbAxisY[1]), float(pb.obbAxisY[2])),
        Vector3f(float(pb.obbAxisZ[0]), float(pb.obbAxisZ[1]), float(pb.obbAxisZ[2])),
        Vector3f(float(pb.obbHalfSize[0]), float(pb.obbHalfSize[1]), float(pb.obbHalfSize[2])));
    bounds.bsphere.center = Vector3f(float(pb.sphereCenter[0]), float(pb.sphereCenter[1]), float(pb.sphereCenter[2]));
    bounds.bsphere.radius = float(pb.sphereRadius);

    // 3) Create GeometryData and VABs
    GeometryData *geo_data=CreateGeometryData(device,vil,header.vertexCount);

    if(!geo_data)
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot create GeometryData for file ") + filename);
        delete mpr;
        return(nullptr);
    }

    if(!geo_data->CreateAllVAB())
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot create VAB for file ") + filename);
        delete geo_data;
        delete mpr;
        return(nullptr);
    }

    // 4) Read AttributeMeta if any
    AutoDeleteArray<uint8_t> attribute_format(header.attributeCount);
    AutoDeleteArray<uint8_t> attribute_name_length(header.attributeCount);
    AutoDeleteArray<char *>  attribute_name(header.attributeCount);

    if(header.attributeCount>0)
    {
        const int32 attrmeta_index = mpr->FindFile(MakeU8FromAscii("AttributeMeta"));
        if(attrmeta_index < 0)
        {
            MLogError(LoadGeometry,OS_TEXT("AttributeMeta not found in file ") + filename);
            delete geo_data;
            delete mpr;
            return(nullptr);
        }

        const uint32 attrmeta_size = mpr->GetFileLength(attrmeta_index);
        if(attrmeta_size < header.attributeCount*2)
        {
            MLogError(LoadGeometry,OS_TEXT("AttributeMeta too small in file ") + filename);
            delete geo_data;
            delete mpr;
            return(nullptr);
        }

        std::vector<uint8_t> attrmeta(attrmeta_size);
        if(mpr->ReadFile(attrmeta_index, attrmeta.data(), 0, attrmeta_size) != attrmeta_size)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot read AttributeMeta from file ") + filename);
            delete geo_data;
            delete mpr;
            return(nullptr);
        }

        // parse: [formats N][name_lengths N][names with trailing zeros]
        const uint8_t *p = attrmeta.data();
        const uint8_t *end = p + attrmeta.size();

        if(static_cast<size_t>(end - p) < header.attributeCount)
        {
            MLogError(LoadGeometry,OS_TEXT("AttributeMeta missing formats in file ") + filename);
            delete geo_data;
            delete mpr;
            return(nullptr);
        }
        memcpy(attribute_format.data(), p, header.attributeCount);
        p += header.attributeCount;

        if(static_cast<size_t>(end - p) < header.attributeCount)
        {
            MLogError(LoadGeometry,OS_TEXT("AttributeMeta missing name lengths in file ") + filename);
            delete geo_data;
            delete mpr;
            return(nullptr);
        }
        memcpy(attribute_name_length.data(), p, header.attributeCount);
        p += header.attributeCount;

        uint total_name_length = 0;
        sum(&total_name_length, attribute_name_length.data(), header.attributeCount);
        total_name_length += header.attributeCount; // for trailing zeros

        if(static_cast<size_t>(end - p) < total_name_length)
        {
            MLogError(LoadGeometry,OS_TEXT("AttributeMeta names section too small in file ") + filename);
            delete geo_data;
            delete mpr;
            return(nullptr);
        }

        AutoDeleteArray<char> name_buffer(total_name_length);
        memcpy(name_buffer, p, total_name_length);

        char *np = name_buffer;
        for(size_t i = 0;i < header.attributeCount;++i)
        {
            attribute_name[i] = np;
            np += attribute_name_length[i] + 1;
        }

        // Read attribute data entries from minipack
        for(size_t i = 0;i < header.attributeCount;++i)
        {
            const int vab_index = geo_data->GetVABIndex(AnsiString(attribute_name[i]));

            const size_t attribute_size = size_t(header.vertexCount) * GetStrideByFormat(static_cast<VkFormat>(attribute_format[i]));

            // Build entry name: attr_<i>_<name>
            std::string entry_name_str = std::string("attr_") + std::to_string(i) + std::string("_") + attribute_name[i];

            // Convert to U8String by copying bytes
            hgl::U8String entry_name_u8;
            {
                const int len = static_cast<int>(entry_name_str.size());
                hgl::u8char *dst = reinterpret_cast<hgl::u8char *>(entry_name_u8.Resize(len));
                for(int k=0;k<len;++k) dst[k] = static_cast<hgl::u8char>(entry_name_str[static_cast<size_t>(k)]);
            }
            const int32 attr_entry_index = mpr->FindFile(entry_name_u8);

            if(vab_index<0)
            {
                // 文件有，材质没有
                MLogNotice(LoadGeometry,OS_TEXT("Attribute name '") + ToOSString(attribute_name[i]) + OS_TEXT("' not found in VIL, file ") + filename);
                // skip reading this attribute data
                continue;
            }

            if(attr_entry_index < 0)
            {
                MLogError(LoadGeometry,OS_TEXT("Attribute entry '") + ToOSString(attribute_name[i]) + OS_TEXT("' not found in minipack, file ") + filename);
                delete geo_data;
                delete mpr;
                return(nullptr);
            }

            const VIF *vif = vil->GetConfig(vab_index);

            if(vif->format != static_cast<VkFormat>(attribute_format[i]))
            {
                MLogError(LoadGeometry,OS_TEXT("Attribute format mismatch for attribute '") + ToOSString(attribute_name[i]) + OS_TEXT("' in file ") + filename);
                delete geo_data;
                delete mpr;
                return(nullptr);
            }

            VAB *vab = geo_data->GetVAB(vab_index);

            if(!vab)
            {
                MLogError(LoadGeometry,OS_TEXT("Cannot get VAB for attribute '") + ToOSString(attribute_name[i]) + OS_TEXT("' in file ") + filename);
                delete geo_data;
                delete mpr;
                return(nullptr);
            }

            void *vab_ptr=vab->Map(0,header.vertexCount);

            if(!vab_ptr)
            {
                MLogError(LoadGeometry,OS_TEXT("Cannot map VAB for attribute '") + ToOSString(attribute_name[i]) + OS_TEXT("' in file ") + filename);
                delete geo_data;
                delete mpr;
                return(nullptr);
            }

            if(mpr->ReadFile(attr_entry_index, vab_ptr, 0, static_cast<uint32>(attribute_size)) != attribute_size)
            {
                MLogError(LoadGeometry,OS_TEXT("Cannot read attribute data for attribute '") + ToOSString(attribute_name[i]) + OS_TEXT("' from file ") + filename);
                vab->Unmap();
                delete geo_data;
                delete mpr;
                return(nullptr);
            }

            vab->Unmap();
        }
    }//if(header.attributeCount>0)

    // 5) Indices
    if(header.indexCount>0)
    {
        IndexType index_type;
        if(header.indexStride==1)index_type=IndexType::U8;else
        if(header.indexStride==2)index_type=IndexType::U16;else
        if(header.indexStride==4)index_type=IndexType::U32;else
        {
            MLogError(LoadGeometry,OS_TEXT("Unsupported index stride ")+OSString::numberOf(header.indexStride)+OS_TEXT(" in file ") + filename);
            delete geo_data;
            delete mpr;
            return(nullptr);
        }

        IndexBuffer *ibo=geo_data->InitIBO(header.indexCount,index_type);
        if(!ibo)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot create IBO for file ") + filename);
            delete geo_data;
            delete mpr;
            return(nullptr);
        }

        void *ibo_ptr=ibo->Map(0,header.indexCount);
        if(!ibo_ptr)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot map IBO for file ") + filename);
            delete geo_data;
            delete mpr;
            return(nullptr);
        }

        const size_t index_size = static_cast<size_t>(header.indexCount) * header.indexStride;

        const int32 indices_index = mpr->FindFile(MakeU8FromAscii("indices"));
        if(indices_index < 0)
        {
            MLogError(LoadGeometry,OS_TEXT("indices entry not found in file ") + filename);
            delete geo_data;
            delete mpr;
            return(nullptr);
        }

        if(mpr->GetFileLength(indices_index) != index_size)
        {
            MLogError(LoadGeometry,OS_TEXT("Index data size mismatch in file ") + filename);
            delete geo_data;
            delete mpr;
            return(nullptr);
        }

        if(mpr->ReadFile(indices_index, ibo_ptr, 0, static_cast<uint32>(index_size)) != index_size)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot read index data from file ") + filename);
            delete geo_data;
            delete mpr;
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
        delete mpr;
        return(nullptr);
    }

    geometry->SetBoundingBox(bounds.aabb);

    delete mpr;

    return geometry;
}
VK_NAMESPACE_END

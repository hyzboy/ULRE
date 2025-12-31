#include<hgl/io/FileInputStream.h>
#include<hgl/type/String.h>
#include<hgl/log/Log.h>
#include<hgl/math/Sum.h>
#include<hgl/graph/VKGeometry.h>
#include<hgl/graph/VKPrimitiveType.h>
#include<hgl/graph/VKVertexInputLayout.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/math/geometry/BoundingVolumes.h>
#include"VKGeometryData.h"
#include<hgl/io/MiniPack.h>

DEFINE_LOGGER_MODULE(LoadGeometry)

VK_NAMESPACE_BEGIN
namespace
{
#pragma pack(push,1)
    struct GeometryHeader
    {
        uint16_t version;        // 1
        uint8_t  primitiveType;  // PrimitiveType as uint8_t
        uint32_t vertexCount;    // Number of vertices
        uint8_t  indexStride;    // 0 if no indices, otherwise 1,2,4
        uint32_t indexCount;     // Number of indices (0 if no indices)
        uint8_t  attributeCount; // Number of attributes
        uint8_t  texCoordCount;  // Number of TEXCOORD sets (attributes with names starting with "TEXCOORD")
    };
#pragma pack(pop)

    // Read and validate GeometryHeader from MiniPack
    bool ReadGeometryHeader(hgl::io::minipack::MiniPackReader *mpr, GeometryHeader &header, const OSString &filename)
    {
        const int32 header_index = mpr->FindFile(AnsiStringView("GeometryHeader"));
        if(header_index < 0)
        {
            MLogError(LoadGeometry,OS_TEXT("GeometryHeader not found in file ") + filename);
            return false;
        }

        if(mpr->GetFileLength(header_index) != sizeof(GeometryHeader))
        {
            MLogError(LoadGeometry,OS_TEXT("GeometryHeader size mismatch in file ") + filename);
            return false;
        }

        if(mpr->ReadFile(header_index, &header, 0, sizeof(header)) != sizeof(header))
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot read GeometryHeader from file ") + filename);
            return false;
        }

        if(header.version!=1)
        {
            MLogError(LoadGeometry,OS_TEXT("Unsupported version in file ") + filename);
            return false;
        }

        if(header.primitiveType>static_cast<uint8_t>(PrimitiveType::Patchs))
        {
            MLogError(LoadGeometry,OS_TEXT("Unsupported primitive type ")+OSString::numberOf(header.primitiveType)+OS_TEXT(" in file ") + filename);
            return false;
        }

        return true;
    }

    // Read and convert BoundingVolumes from MiniPack
    bool ReadBoundingVolumes(hgl::io::minipack::MiniPackReader *mpr, ::hgl::math::BoundingVolumes &bounds, const OSString &filename)
    {
        const int32 bounds_index = mpr->FindFile(AnsiStringView("BoundingVolumes"));
        if(bounds_index < 0)
        {
            MLogError(LoadGeometry,OS_TEXT("BoundingVolumes not found in file ") + filename);
            return false;
        }

        if(mpr->GetFileLength(bounds_index) != sizeof(::hgl::math::BoundingVolumesData))
        {
            MLogError(LoadGeometry,OS_TEXT("BoundingVolumes size mismatch in file ") + filename);
            return false;
        }

        math::BoundingVolumesData pb{};
        if(mpr->ReadFile(bounds_index, &pb, 0, sizeof(math::BoundingVolumesData)) != sizeof(math::BoundingVolumesData))
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot read BoundingVolumes from file ") + filename);
            return false;
        }

        pb.To(&bounds);
        return true;
    }

    // Read AttributeMeta block and expose parsed views
    bool ReadAttributeMeta(hgl::io::minipack::MiniPackReader *mpr,
                           const GeometryHeader &header,
                           const OSString &filename,
                           std::vector<uint8_t> &attrmeta,
                           const uint8_t *&attribute_format,
                           const uint8_t *&attribute_name_length,
                           const char *&names_ptr)
    {
        const int32 attrmeta_index = mpr->FindFile(AnsiStringView("AttributeMeta"));
        if(attrmeta_index < 0)
        {
            MLogError(LoadGeometry,OS_TEXT("AttributeMeta not found in file ") + filename);
            return false;
        }

        const uint32 attrmeta_size = mpr->GetFileLength(attrmeta_index);
        if(attrmeta_size < header.attributeCount*2)
        {
            MLogError(LoadGeometry,OS_TEXT("AttributeMeta too small in file ") + filename);
            return false;
        }

        attrmeta.resize(attrmeta_size);
        if(mpr->ReadFile(attrmeta_index, attrmeta.data(), 0, attrmeta_size) != attrmeta_size)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot read AttributeMeta from file ") + filename);
            return false;
        }

        const uint8_t *meta = attrmeta.data();
        const uint8_t *meta_end = meta + attrmeta.size();

        // formats
        if(static_cast<size_t>(meta_end - meta) < header.attributeCount)
        {
            MLogError(LoadGeometry,OS_TEXT("AttributeMeta missing formats in file ") + filename);
            return false;
        }
        attribute_format = meta;
        meta += header.attributeCount;

        // name lengths
        if(static_cast<size_t>(meta_end - meta) < header.attributeCount)
        {
            MLogError(LoadGeometry,OS_TEXT("AttributeMeta missing name lengths in file ") + filename);
            return false;
        }
        attribute_name_length = meta;
        meta += header.attributeCount;

        // names block
        uint total_name_length = 0;
        math::sum(&total_name_length, attribute_name_length, header.attributeCount);
        total_name_length += header.attributeCount; // trailing zeros

        if(static_cast<size_t>(meta_end - meta) < total_name_length)
        {
            MLogError(LoadGeometry,OS_TEXT("AttributeMeta names section too small in file ") + filename);
            return false;
        }
        names_ptr = reinterpret_cast<const char *>(meta);
        return true;
    }

    // Read attributes/VBOs using AttributeMeta views
    bool ReadAttributesVBO(hgl::io::minipack::MiniPackReader *mpr,
                           GeometryData *geo_data,
                           const VIL *vil,
                           const GeometryHeader &header,
                           const uint8_t *attribute_format,
                           const uint8_t *attribute_name_length,
                           const char *names_ptr,
                           const OSString &filename)
    {
        if(!vil)
        {
            MLogError(LoadGeometry,OS_TEXT("VIL is null for file ") + filename);
            return false;
        }

        const uint32_t vil_attr_count = vil->GetVertexAttribCount();
        if(header.attributeCount < vil_attr_count)
        {
            MLogNotice(LoadGeometry,OS_TEXT("File has fewer attributes(") + OSString::numberOf(header.attributeCount) + OS_TEXT(") than VIL(") + OSString::numberOf(vil_attr_count) + OS_TEXT(") in ") + filename);
        }

        // helper to find meta index by name
        auto find_meta_index_by_name = [&](const char *target_name, uint8_t &out_len)->int
        {
            const char *p = names_ptr;
            for(uint32_t i = 0; i < header.attributeCount; ++i)
            {
                const uint8_t len = attribute_name_length[i];
                // compare lengths first
                if(static_cast<size_t>(len) == hgl::strlen(target_name))
                {
                    if(::hgl::stricmp(p, target_name, len) == 0)
                    {
                        out_len = len;
                        return static_cast<int>(i);
                    }
                }
                p += len + 1; // skip 0
            }
            return -1;
        };

        for(uint32_t vab_index = 0; vab_index < vil_attr_count; ++vab_index)
        {
            const VIF *vif = vil->GetConfig(vab_index);
            if(!vif || !vif->name)
            {
                MLogError(LoadGeometry,OS_TEXT("Invalid VIF at index ") + OSString::numberOf(vab_index) + OS_TEXT(" in file ") + filename);
                return false;
            }

            if(vif->group==VertexInputGroup::TransformID
             ||vif->group==VertexInputGroup::MaterialInstanceID)
                continue;

            const char *attr_name = vif->name;

            // find this attribute in meta by name
            uint8_t meta_name_len = 0;
            int meta_index = find_meta_index_by_name(attr_name, meta_name_len);
            if(meta_index < 0)
            {
                // attribute expected by VIL but not present in file meta
                MLogNotice(LoadGeometry,OS_TEXT("Attribute name '") + ToOSString(attr_name) + OS_TEXT("' not found in AttributeMeta, file ") + filename);
                // try to read by name anyway; if missing, we'll error below
            }
            else
            {
                // verify format match if meta has it
                if(vif->format != static_cast<VkFormat>(attribute_format[meta_index]))
                {
                    MLogError(LoadGeometry,OS_TEXT("Attribute format mismatch for attribute '") + ToOSString(attr_name) + OS_TEXT("' in file ") + filename);
                    return false;
                }
            }

            // locate entry in minipack by this VIL-provided name
            const int32 attr_entry_index = mpr->FindFile(AnsiStringView(attr_name));
            if(attr_entry_index < 0)
            {
                MLogError(LoadGeometry,OS_TEXT("Attribute entry '") + ToOSString(attr_name) + OS_TEXT("' not found in minipack, file ") + filename);
                return false;
            }

            VAB *vab = geo_data->GetVAB(vab_index);
            if(!vab)
            {
                MLogError(LoadGeometry,OS_TEXT("Cannot get VAB for attribute '") + ToOSString(attr_name) + OS_TEXT("' in file ") + filename);
                return false;
            }

            void *vab_ptr = vab->Map(0, header.vertexCount);
            if(!vab_ptr)
            {
                MLogError(LoadGeometry,OS_TEXT("Cannot map VAB for attribute '") + ToOSString(attr_name) + OS_TEXT("' in file ") + filename);
                return false;
            }

            const size_t attribute_size = size_t(header.vertexCount) * GetStrideByFormat(vif->format);
            if(mpr->ReadFile(attr_entry_index, vab_ptr, 0, static_cast<uint32>(attribute_size)) != attribute_size)
            {
                MLogError(LoadGeometry,OS_TEXT("Cannot read attribute data for attribute '") + ToOSString(attr_name) + OS_TEXT("' from file ") + filename);
                vab->Unmap();
                return false;
            }

            vab->Unmap();
        }

        return true;
    }

    // Read indices into IBO
    bool ReadIndicesData(hgl::io::minipack::MiniPackReader *mpr,
                         GeometryData *geo_data,
                         const GeometryHeader &header,
                         const OSString &filename)
    {
        IndexType index_type;
        if(header.indexStride==1)index_type=IndexType::U8;else
        if(header.indexStride==2)index_type=IndexType::U16;else
        if(header.indexStride==4)index_type=IndexType::U32;else
        {
            MLogError(LoadGeometry,OS_TEXT("Unsupported index stride ")+OSString::numberOf(header.indexStride)+OS_TEXT(" in file ") + filename);
            return false;
        }

        IndexBuffer *ibo=geo_data->InitIBO(header.indexCount,index_type);
        if(!ibo)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot create IBO for file ") + filename);
            return false;
        }

        void *ibo_ptr=ibo->Map(0,header.indexCount);
        if(!ibo_ptr)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot map IBO for file ") + filename);
            return false;
        }

        const size_t index_size = static_cast<size_t>(header.indexCount) * header.indexStride;

        const int32 indices_index = mpr->FindFile(AnsiStringView("indices"));
        if(indices_index < 0)
        {
            MLogError(LoadGeometry,OS_TEXT("indices entry not found in file ") + filename);
            return false;
        }

        if(mpr->GetFileLength(indices_index) != index_size)
        {
            MLogError(LoadGeometry,OS_TEXT("Index data size mismatch in file ") + filename);
            return false;
        }

        if(mpr->ReadFile(indices_index, ibo_ptr, 0, static_cast<uint32>(index_size)) != index_size)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot read index data from file ") + filename);
            return false;
        }

        ibo->Unmap();
        return true;
    }

    // Orchestrate reading attributes and indices into GeometryData
    bool ReadGeometryData(hgl::io::minipack::MiniPackReader *mpr,
                          GeometryData *geo_data,
                          const VIL *vil,
                          const GeometryHeader &header,
                          const OSString &filename)
    {
        if(!geo_data)
        {
            MLogError(LoadGeometry,OS_TEXT("GeometryData is null for file ") + filename);
            return false;
        }

        if(header.attributeCount>0)
        {
            std::vector<uint8_t> attrmeta;
            const uint8_t *attribute_format = nullptr;
            const uint8_t *attribute_name_length = nullptr;
            const char *names_ptr = nullptr;

            if(!ReadAttributeMeta(mpr, header, filename, attrmeta, attribute_format, attribute_name_length, names_ptr))
                return false;

            if(!ReadAttributesVBO(mpr, geo_data, vil, header, attribute_format, attribute_name_length, names_ptr, filename))
                return false;
        }

        if(header.indexCount>0)
        {
            if(!ReadIndicesData(mpr, geo_data, header, filename))
                return false;
        }

        return true;
    }
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

    // 1) Read GeometryHeader
    GeometryHeader header{};
    if(!ReadGeometryHeader(mpr, header, filename))
    {
        delete mpr;
        return nullptr;
    }

    // 2) Read BoundingVolumes
    math::BoundingVolumes bounding_volumes;

    if(!ReadBoundingVolumes(mpr, bounding_volumes, filename))
    {
        delete mpr;
        return nullptr;
    }

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

    // 4) Read attributes and indices into GeometryData
    if(!ReadGeometryData(mpr, geo_data, vil, header, filename))
    {
        delete geo_data;
        delete mpr;
        return nullptr;
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

    geometry->SetBoundingVolumes(bounding_volumes);

    delete mpr;

    return geometry;
}
VK_NAMESPACE_END

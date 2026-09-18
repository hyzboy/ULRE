#include<hgl/io/FileInputStream.h>
#include<hgl/type/String.h>
#include<hgl/type/Smart.h>
#include<hgl/type/ValueArray.h>
#include<hgl/log/Log.h>
#include<hgl/math/Sum.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/VKPrimitiveType.h>
#include<hgl/vk/VKRenderAssign.h>
#include<hgl/math/geometry/BoundingVolumes.h>
#include<hgl/graph/geo/VKGeometryData.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/io/MiniPack.h>
#include<hgl/io/MemoryInputStream.h>

DEFINE_LOGGER_MODULE(LoadGeometry)

namespace hgl::graph{
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

    struct FileAttribute
    {
        char name[VERTEX_ATTRIB_NAME_MAX_LENGTH];
        VkFormat format;
        int32 entry_index;
        uint32 entry_size;
        VertexSemantic semantic;

        bool operator==(const FileAttribute &rhs) const
        {
            return entry_index == rhs.entry_index && format == rhs.format && semantic == rhs.semantic;
        }
    };

    static VertexSemantic ParseSemanticFromName(const char *name)
    {
        if(!name || !*name)
            return VertexSemantic::Unknown;

        if(hgl::stricmp(name, "POSITION") == 0)
            return VertexSemantic::Position;
        if(hgl::stricmp(name, "NORMAL") == 0)
            return VertexSemantic::Normal;
        if(hgl::stricmp(name, "TANGENT") == 0)
            return VertexSemantic::Tangent;
        if(hgl::stricmp(name, "BITANGENT") == 0)
            return VertexSemantic::Bitangent;
        if(hgl::stricmp(name, "COLOR", 5) == 0)
            return VertexSemantic::Color;
        if(hgl::stricmp(name, "TEXCOORD", 8) == 0)
            return VertexSemantic::TexCoord;

        return VertexSemantic::Unknown;
    }

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

        if(header.primitiveType>static_cast<uint8_t>(PrimitiveType::Fan))
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

    // Parse AttributeMeta block and extract attribute metadata
    bool ParseAttributeMeta(hgl::io::minipack::MiniPackReader *mpr,
                            const GeometryHeader &header,
                            const OSString &filename,
                            ValueArray<FileAttribute> &file_attributes)
    {
        file_attributes.Resize(0);
        if(header.attributeCount == 0)
            return true;

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

        AutoDeleteArray<uint8_t> attrmeta(attrmeta_size);
        if(mpr->ReadFile(attrmeta_index, attrmeta.data(), 0, attrmeta_size) != attrmeta_size)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot read AttributeMeta from file ") + filename);
            return false;
        }

        const uint8_t *meta = attrmeta.data();
        const uint8_t *meta_end = meta + attrmeta_size;

        // formats
        if(static_cast<size_t>(meta_end - meta) < header.attributeCount)
        {
            MLogError(LoadGeometry,OS_TEXT("AttributeMeta missing formats in file ") + filename);
            return false;
        }
        const uint8_t *attribute_format = meta;
        meta += header.attributeCount;

        // name lengths
        if(static_cast<size_t>(meta_end - meta) < header.attributeCount)
        {
            MLogError(LoadGeometry,OS_TEXT("AttributeMeta missing name lengths in file ") + filename);
            return false;
        }
        const uint8_t *attribute_name_length = meta;
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
        const char *name_ptr = reinterpret_cast<const char *>(meta);

        for(uint8_t i = 0; i < header.attributeCount; ++i)
        {
            FileAttribute attr{};
            attr.format = static_cast<VkFormat>(attribute_format[i]);

            const uint8_t name_len = attribute_name_length[i];
            const uint8_t copy_len = name_len < (VERTEX_ATTRIB_NAME_MAX_LENGTH - 1) ? name_len : (VERTEX_ATTRIB_NAME_MAX_LENGTH - 1);
            memcpy(attr.name, name_ptr, copy_len);
            attr.name[copy_len] = '\0';

            attr.semantic = ParseSemanticFromName(attr.name);
            attr.entry_index = mpr->FindFile(AnsiStringView(attr.name, name_len));
            if(attr.entry_index >= 0)
                attr.entry_size = mpr->GetFileLength(attr.entry_index);

            file_attributes.Add(attr);
            name_ptr += name_len + 1;
        }

        return true;
    }

    // Read attributes/VBOs by semantic mapping, with automatic format fallback conversion
    bool ReadAttributesVBO(hgl::io::minipack::MiniPackReader *mpr,
                           GeometryData *geo_data,
                           const GeometryVertexFormat &geometry_vertex_format,
                           const GeometryHeader &header,
                           const ValueArray<FileAttribute> &file_attributes,
                           const OSString &filename)
    {
        const uint32_t gvf_attr_count = geometry_vertex_format.GetCount();
        if(gvf_attr_count == 0)
        {
            MLogError(LoadGeometry,OS_TEXT("GeometryVertexFormat has no attributes for file ") + filename);
            return false;
        }

        for(uint32_t vab_index = 0; vab_index < gvf_attr_count; ++vab_index)
        {
            const GeometryVertexAttributeFormat *geometry_attribute = geometry_vertex_format.Get(vab_index);
            if(!geometry_attribute)
            {
                MLogError(LoadGeometry,OS_TEXT("Invalid GeometryVertexFormat attribute index ") + OSString::numberOf(vab_index) + OS_TEXT(" in file ") + filename);
                return false;
            }

            // Find matching file attribute by semantic
            const FileAttribute *src_attr = nullptr;
            for(int i = 0; i < file_attributes.GetCount(); ++i)
            {
                if(file_attributes[i].semantic == geometry_attribute->semantic)
                {
                    src_attr = &file_attributes[i];
                    break;
                }
            }

            if(!src_attr || src_attr->entry_index < 0)
            {
                MLogError(LoadGeometry,OS_TEXT("Required vertex semantic ")
                    + ToOSString(GetVertexSemanticName(geometry_attribute->semantic))
                    + OS_TEXT(" not found in file ") + filename);
                return false;
            }

            VAB *vab = geo_data->GetVAB(vab_index);
            if(!vab)
            {
                MLogError(LoadGeometry,OS_TEXT("Cannot get VAB for attribute index ") + OSString::numberOf(vab_index) + OS_TEXT(" in file ") + filename);
                return false;
            }

            void *vab_ptr = vab->Map(0, header.vertexCount);
            if(!vab_ptr)
            {
                MLogError(LoadGeometry,OS_TEXT("Cannot map VAB for attribute index ") + OSString::numberOf(vab_index) + OS_TEXT(" in file ") + filename);
                return false;
            }

            const size_t target_stride = GetStrideByFormat(geometry_attribute->format);
            const size_t target_size = size_t(header.vertexCount) * target_stride;

            if(geometry_attribute->format == src_attr->format)
            {
                // Exact format match: read directly into VAB
                if(mpr->ReadFile(src_attr->entry_index, vab_ptr, 0, static_cast<uint32>(target_size)) != target_size)
                {
                    MLogError(LoadGeometry,OS_TEXT("Cannot read attribute data for ") + ToOSString(src_attr->name) + OS_TEXT(" from file ") + filename);
                    vab->Unmap();
                    return false;
                }
            }
            else if(geometry_attribute->semantic == VertexSemantic::Normal)
            {
                // Normal conversion: float3 (VK_FORMAT_R32G32B32_SFLOAT) -> V2UN8 (VK_FORMAT_R8G8_UNORM) or V2HF (VK_FORMAT_R16G16_SFLOAT)
                if(src_attr->format == VK_FORMAT_R32G32B32_SFLOAT)
                {
                    const size_t src_size = size_t(header.vertexCount) * sizeof(float) * 3;
                    AutoDeleteArray<float> src_normals(header.vertexCount * 3);
                    if(mpr->ReadFile(src_attr->entry_index, src_normals.data(), 0, static_cast<uint32>(src_size)) != src_size)
                    {
                        MLogError(LoadGeometry,OS_TEXT("Cannot read source float3 normals from file ") + filename);
                        vab->Unmap();
                        return false;
                    }

                    if(geometry_attribute->format == VK_FORMAT_R8G8_UNORM)
                    {
                        // Octahedral encoding + uint8 quantization (2B/vert)
                        EncodeNormalsToRG8(src_normals.data(), header.vertexCount, reinterpret_cast<uint8_t*>(vab_ptr));
                    }
                    else if(geometry_attribute->format == VK_FORMAT_R16G16_SFLOAT)
                    {
                        // Octahedral encoding + half2 conversion (4B/vert)
                        uint16_t *dst = reinterpret_cast<uint16_t*>(vab_ptr);
                        for(uint32_t vi = 0; vi < header.vertexCount; ++vi)
                        {
                            const float *n = src_normals.data() + vi * 3;
                            float p, q;
                            EncodeOctahedralNormal(n[0], n[1], n[2], p, q);
                            dst[vi * 2 + 0] = FloatToHalf(p);
                            dst[vi * 2 + 1] = FloatToHalf(q);
                        }
                    }
                    else
                    {
                        MLogError(LoadGeometry,OS_TEXT("Unsupported normal target format mismatch at index ") + OSString::numberOf(vab_index) + OS_TEXT(" in file ") + filename);
                        vab->Unmap();
                        return false;
                    }
                }
                else
                {
                    MLogError(LoadGeometry,OS_TEXT("Incompatible normal source format at index ") + OSString::numberOf(vab_index) + OS_TEXT(" in file ") + filename);
                    vab->Unmap();
                    return false;
                }
            }
            else if(geometry_attribute->semantic == VertexSemantic::TexCoord)
            {
                // UV conversion: float2 (VK_FORMAT_R32G32_SFLOAT) -> V2HF (VK_FORMAT_R16G16_SFLOAT)
                if(src_attr->format == VK_FORMAT_R32G32_SFLOAT && geometry_attribute->format == VK_FORMAT_R16G16_SFLOAT)
                {
                    const size_t src_size = size_t(header.vertexCount) * sizeof(float) * 2;
                    AutoDeleteArray<float> src_uvs(header.vertexCount * 2);
                    if(mpr->ReadFile(src_attr->entry_index, src_uvs.data(), 0, static_cast<uint32>(src_size)) != src_size)
                    {
                        MLogError(LoadGeometry,OS_TEXT("Cannot read source float2 UVs from file ") + filename);
                        vab->Unmap();
                        return false;
                    }

                    uint16_t *dst = reinterpret_cast<uint16_t*>(vab_ptr);
                    for(uint32_t vi = 0; vi < header.vertexCount; ++vi)
                    {
                        dst[vi * 2 + 0] = FloatToHalf(src_uvs[vi * 2 + 0]);
                        dst[vi * 2 + 1] = FloatToHalf(src_uvs[vi * 2 + 1]);
                    }
                }
                else
                {
                    MLogError(LoadGeometry,OS_TEXT("Incompatible UV source/target format at index ") + OSString::numberOf(vab_index) + OS_TEXT(" in file ") + filename);
                    vab->Unmap();
                    return false;
                }
            }
            else
            {
                MLogError(LoadGeometry,OS_TEXT("Attribute format mismatch at index ") + OSString::numberOf(vab_index) + OS_TEXT(" in file ") + filename);
                vab->Unmap();
                return false;
            }

            vab->Unmap();
        }

        return true;
    }

    // Read indices into IBO (uniform uint32 indices)
    bool ReadIndicesData(hgl::io::minipack::MiniPackReader *mpr,
                         GeometryData *geo_data,
                         const GeometryHeader &header,
                         const OSString &filename)
    {
        if(header.indexStride!=1 && header.indexStride!=2 && header.indexStride!=4)
        {
            MLogError(LoadGeometry,OS_TEXT("Unsupported index stride ")+OSString::numberOf(header.indexStride)+OS_TEXT(" in file ") + filename);
            return false;
        }

        IndexBuffer *ibo=geo_data->InitIBO(header.indexCount,IndexType::U32,"LoadGeometry:IBO");
        if(!ibo)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot create IBO for file ") + filename);
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

        AutoDeleteArray<uint8_t> raw(index_size);
        if(mpr->ReadFile(indices_index, raw.data(), 0, static_cast<uint32>(index_size)) != index_size)
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot read index data from file ") + filename);
            return false;
        }

        // stride 1/2/4 -> uint32 统一展开（引擎废弃 U8/U16 索引）
        AutoDeleteArray<uint32_t> idx(header.indexCount);
        if(header.indexStride==4)
        {
            memcpy(idx.data(), raw.data(), index_size);
        }
        else if(header.indexStride==2)
        {
            const uint16_t *src16 = reinterpret_cast<const uint16_t*>(raw.data());
            for(uint32_t i=0;i<header.indexCount;++i)
                idx[i] = static_cast<uint32_t>(src16[i]);
        }
        else
        {
            for(uint32_t i=0;i<header.indexCount;++i)
                idx[i] = static_cast<uint32_t>(raw[i]);
        }

        if(!ibo->Write(idx.data(), header.indexCount))
        {
            MLogError(LoadGeometry,OS_TEXT("Cannot write index data for file ") + filename);
            return false;
        }

        return true;
    }

    // Orchestrate reading attributes and indices into GeometryData
    bool ReadGeometryData(hgl::io::minipack::MiniPackReader *mpr,
                          GeometryData *geo_data,
                          const GeometryVertexFormat &geometry_vertex_format,
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
            ValueArray<FileAttribute> file_attributes;
            if(!ParseAttributeMeta(mpr, header, filename, file_attributes))
                return false;

            if(!ReadAttributesVBO(mpr, geo_data, geometry_vertex_format, header, file_attributes, filename))
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

static Geometry *LoadGeometryFromReader(VulkanDevice *device,const GeometryVertexFormat &geometry_vertex_format,hgl::io::minipack::MiniPackReader *mpr,const OSString &debug_name)
{
    // 1) Read GeometryHeader
    GeometryHeader header{};
    if(!ReadGeometryHeader(mpr, header, debug_name))
        return nullptr;

    // 2) Read BoundingVolumes
    math::BoundingVolumes bounding_volumes;

    if(!ReadBoundingVolumes(mpr, bounding_volumes, debug_name))
        return nullptr;

    // 3) Create GeometryData and VABs
    GeometryData *geo_data=CreateGeometryData(device,geometry_vertex_format,header.vertexCount);

    if(!geo_data)
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot create GeometryData for source ") + debug_name);
        return(nullptr);
    }

    if(!geo_data->CreateAllVAB())
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot create VAB for source ") + debug_name);
        delete geo_data;
        return(nullptr);
    }

    // 4) Read attributes and indices into GeometryData
    if(!ReadGeometryData(mpr, geo_data, geometry_vertex_format, header, debug_name))
    {
        delete geo_data;
        return nullptr;
    }

    const U8String geo_name=ToU8String(debug_name);

    Geometry *geometry = new Geometry((char *)geo_name.c_str(),geo_data);

    if(!geometry)
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot create Geometry object for source ") + debug_name);
        delete geo_data;
        return(nullptr);
    }

    geometry->SetBoundingVolumes(bounding_volumes);

    // 5) Read Meshlet data if present
    const int32 meshlets_idx = mpr->FindFile(AnsiStringView("meshlets"));
    const int32 meshlet_vertices_idx = mpr->FindFile(AnsiStringView("meshlet_vertices"));
    const int32 meshlet_triangles_idx = mpr->FindFile(AnsiStringView("meshlet_triangles"));

    if (meshlets_idx >= 0 && meshlet_vertices_idx >= 0 && meshlet_triangles_idx >= 0)
    {
        const uint32 meshlets_bytes = mpr->GetFileLength(meshlets_idx);
        const uint32 meshlet_vertices_bytes = mpr->GetFileLength(meshlet_vertices_idx);
        const uint32 meshlet_triangles_bytes = mpr->GetFileLength(meshlet_triangles_idx);

        if (meshlets_bytes > 0 && (meshlets_bytes % sizeof(MeshletDescriptor) == 0))
        {
            const uint32 meshlet_count = meshlets_bytes / static_cast<uint32>(sizeof(MeshletDescriptor));

            AutoDeleteArray<uint8_t> raw_desc(meshlets_bytes);
            AutoDeleteArray<uint8_t> raw_verts(meshlet_vertices_bytes);
            AutoDeleteArray<uint8_t> raw_tris(meshlet_triangles_bytes);

            if (mpr->ReadFile(meshlets_idx, raw_desc.data(), 0, meshlets_bytes) == meshlets_bytes
             && mpr->ReadFile(meshlet_vertices_idx, raw_verts.data(), 0, meshlet_vertices_bytes) == meshlet_vertices_bytes
             && mpr->ReadFile(meshlet_triangles_idx, raw_tris.data(), 0, meshlet_triangles_bytes) == meshlet_triangles_bytes)
            {
                DeviceBuffer *mb = device->CreateSSBO(meshlets_bytes, raw_desc.data());
                DeviceBuffer *mvb = device->CreateSSBO(meshlet_vertices_bytes, raw_verts.data());
                DeviceBuffer *mtb = device->CreateSSBO(meshlet_triangles_bytes, raw_tris.data());
                DeviceBuffer *mbb = nullptr;

                const int32 meshlet_bounds_idx = mpr->FindFile(AnsiStringView("meshlet_bounds"));
                if (meshlet_bounds_idx >= 0)
                {
                    const uint32 bounds_bytes = mpr->GetFileLength(meshlet_bounds_idx);
                    if (bounds_bytes > 0 && bounds_bytes == meshlet_count * sizeof(MeshletBounds))
                    {
                        AutoDeleteArray<uint8_t> raw_bounds(bounds_bytes);
                        if (mpr->ReadFile(meshlet_bounds_idx, raw_bounds.data(), 0, bounds_bytes) == bounds_bytes)
                        {
                            mbb = device->CreateSSBO(bounds_bytes, raw_bounds.data());
                        }
                    }
                }

                if (mb && mvb && mtb)
                {
                    geometry->SetMeshlets(meshlet_count, mb, mvb, mtb, mbb);
                    MLogInfo(LoadGeometry, OS_TEXT("Loaded %u meshlets for ") + debug_name);
                }
                else
                {
                    SAFE_CLEAR(mb);
                    SAFE_CLEAR(mvb);
                    SAFE_CLEAR(mtb);
                    SAFE_CLEAR(mbb);
                }
            }
        }
    }

    return geometry;
}

Geometry *LoadGeometry(VulkanDevice *device,const GeometryVertexFormat &geometry_vertex_format,const OSString &filename)
{
    using namespace hgl::io::minipack;

    MiniPackReader *mpr = GetMiniPackReader(filename);

    if(!mpr)
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot open minipack file ") + filename + OS_TEXT(" for reading."));
        return(nullptr);
    }

    Geometry *geometry = LoadGeometryFromReader(device,geometry_vertex_format,mpr,filename);

    delete mpr;
    return geometry;
}

Geometry *LoadGeometryFromMiniPackBytes(VulkanDevice *device,const GeometryVertexFormat &geometry_vertex_format,const void *bytes,const uint32 size,const OSString &debug_name)
{
    using namespace hgl::io;
    using namespace hgl::io::minipack;

    if(!bytes || size == 0)
    {
        MLogError(LoadGeometry,OS_TEXT("LoadGeometryFromMiniPackBytes: empty source for ") + debug_name);
        return nullptr;
    }

    MemoryInputStream *mis = new MemoryInputStream;
    if(!mis->Link(bytes,size))
    {
        delete mis;
        MLogError(LoadGeometry,OS_TEXT("LoadGeometryFromMiniPackBytes: cannot link memory stream for ") + debug_name);
        return nullptr;
    }

    MiniPackReader *mpr = GetMiniPackReader(static_cast<InputStream *>(mis));
    if(!mpr)
    {
        delete mis;
        MLogError(LoadGeometry,OS_TEXT("LoadGeometryFromMiniPackBytes: cannot parse minipack from memory for ") + debug_name);
        return nullptr;
    }

    Geometry *geometry = LoadGeometryFromReader(device,geometry_vertex_format,mpr,debug_name);

    delete mpr;
    return geometry;
}
}//namespace hgl::graph

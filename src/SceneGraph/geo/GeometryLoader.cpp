#include<hgl/graph/geo/LoadGeometry.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/type/String.h>
#include<hgl/type/Smart.h>
#include<hgl/type/ValueArray.h>
#include<hgl/log/Log.h>
#include<hgl/math/Sum.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/common/PrimitiveType.h>
#include<hgl/math/geometry/BoundingVolumes.h>
#include<hgl/graph/geo/VKGeometryData.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/io/MiniPack.h>
#include<hgl/io/MemoryInputStream.h>
#include<hgl/graph/module/GpuUploadQueue.h>
#include<hgl/graph/asset/AssetSourceMount.h>

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
                         const OSString &filename,
                         ValueArray<uint32_t> *meshlet_indices)
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

        if (meshlet_indices)
        {
            meshlet_indices->Clear();
            meshlet_indices->Add(idx.data(), static_cast<int>(header.indexCount));
        }

        return true;
    }

    bool ReadMeshletData(hgl::io::minipack::MiniPackReader *mpr,
                         const OSString &filename,
                         const uint32_t geometry_vertex_count,
                         MeshletGeometryData &out,
                         bool &has_meshlets)
    {
        constexpr uint32_t kFileDescriptorBytes = 16;
        constexpr uint32_t kFileBoundsBytes = 48;

#pragma pack(push, 1)
        struct FileMeshletDescriptor
        {
            uint32_t vertex_offset;
            uint32_t triangle_offset;
            uint8_t vertex_count;
            uint8_t triangle_count;
            uint16_t reserved16;
            uint32_t reserved32;
        };
        struct FileMeshletBounds
        {
            float center[3];
            float radius;
            float cone_apex[3];
            float cone_axis[3];
            float cone_cutoff;
            int8_t cone_axis_s8[3];
            int8_t cone_cutoff_s8;
        };
#pragma pack(pop)
        static_assert(sizeof(FileMeshletDescriptor) == kFileDescriptorBytes);
        static_assert(sizeof(FileMeshletBounds) == kFileBoundsBytes);

        const int32_t descriptor_file = mpr->FindFile(AnsiStringView("meshlets"));
        const int32_t vertex_file = mpr->FindFile(AnsiStringView("meshlet_vertices"));
        const int32_t triangle_file = mpr->FindFile(AnsiStringView("meshlet_triangles"));
        const int32_t bounds_file = mpr->FindFile(AnsiStringView("meshlet_bounds"));
        has_meshlets = descriptor_file >= 0 || vertex_file >= 0
                    || triangle_file >= 0 || bounds_file >= 0;
        if (!has_meshlets)
            return true;

        if (descriptor_file < 0 || vertex_file < 0
            || triangle_file < 0 || bounds_file < 0)
        {
            MLogError(LoadGeometry, OS_TEXT("Meshlet 数据段不完整：") + filename);
            return false;
        }

        const uint32_t descriptor_bytes = mpr->GetFileLength(descriptor_file);
        const uint32_t vertex_bytes = mpr->GetFileLength(vertex_file);
        const uint32_t triangle_bytes = mpr->GetFileLength(triangle_file);
        const uint32_t bounds_bytes = mpr->GetFileLength(bounds_file);
        if (descriptor_bytes == 0 || descriptor_bytes % kFileDescriptorBytes != 0
            || vertex_bytes == 0 || vertex_bytes % sizeof(uint32_t) != 0
            || triangle_bytes == 0 || triangle_bytes % 3 != 0
            || bounds_bytes == 0 || bounds_bytes % kFileBoundsBytes != 0
            || bounds_bytes / kFileBoundsBytes != descriptor_bytes / kFileDescriptorBytes)
        {
            MLogError(LoadGeometry, OS_TEXT("Meshlet 数据段尺寸无效：") + filename);
            return false;
        }
        if (descriptor_bytes > 0x7FFFFFFFu || vertex_bytes > 0x7FFFFFFFu
            || triangle_bytes > 0x7FFFFFFFu || bounds_bytes > 0x7FFFFFFFu)
        {
            MLogError(LoadGeometry, OS_TEXT("Meshlet 数据段超过 CPU 容器可寻址范围：") + filename);
            return false;
        }

        ValueArray<uint8_t> descriptor_data;
        ValueArray<uint8_t> vertex_data;
        ValueArray<uint8_t> triangle_data;
        ValueArray<uint8_t> bounds_data;
        descriptor_data.Resize(static_cast<int>(descriptor_bytes));
        vertex_data.Resize(static_cast<int>(vertex_bytes));
        triangle_data.Resize(static_cast<int>(triangle_bytes));
        bounds_data.Resize(static_cast<int>(bounds_bytes));
        if (mpr->ReadFile(descriptor_file, descriptor_data.GetData(), 0, descriptor_bytes) != descriptor_bytes
            || mpr->ReadFile(vertex_file, vertex_data.GetData(), 0, vertex_bytes) != vertex_bytes
            || mpr->ReadFile(triangle_file, triangle_data.GetData(), 0, triangle_bytes) != triangle_bytes
            || mpr->ReadFile(bounds_file, bounds_data.GetData(), 0, bounds_bytes) != bounds_bytes)
        {
            MLogError(LoadGeometry, OS_TEXT("无法读取 Meshlet 数据段：") + filename);
            return false;
        }

        const uint32_t descriptor_count = descriptor_bytes / kFileDescriptorBytes;
        const uint32_t vertex_count = vertex_bytes / sizeof(uint32_t);
        const uint32_t triangle_count = triangle_bytes / 3;
        if (descriptor_count > 0x7fffffffu || vertex_count > 0x7fffffffu
            || triangle_count > 0x7fffffffu)
        {
            MLogError(LoadGeometry, OS_TEXT("Meshlet 数据段超过可寻址范围：") + filename);
            return false;
        }

        out.descriptors.Resize(static_cast<int>(descriptor_count));
        out.bounds.Resize(static_cast<int>(descriptor_count));
        out.vertices.Resize(static_cast<int>(vertex_count));
        out.triangles.Resize(static_cast<int>(triangle_count));
        const auto *source_descriptors =
            reinterpret_cast<const FileMeshletDescriptor *>(descriptor_data.GetData());
        const auto *source_vertices =
            reinterpret_cast<const uint32_t *>(vertex_data.GetData());
        const auto *source_bounds =
            reinterpret_cast<const FileMeshletBounds *>(bounds_data.GetData());

        for (uint32_t i = 0; i < descriptor_count; ++i)
        {
            const FileMeshletDescriptor &source = source_descriptors[i];
            if (source.vertex_count == 0 || source.vertex_count > 64
                || source.triangle_count == 0 || source.triangle_count > 124
                || uint64_t(source.vertex_offset) + source.vertex_count > vertex_count
                || uint64_t(source.triangle_offset) + source.triangle_count > triangle_count)
            {
                MLogError(LoadGeometry, OS_TEXT("Meshlet 描述符越界或计数无效：") + filename);
                out.Clear();
                return false;
            }

            for (uint32_t vertex = 0; vertex < source.vertex_count; ++vertex)
            {
                if (source_vertices[source.vertex_offset + vertex] >= geometry_vertex_count)
                {
                    MLogError(LoadGeometry, OS_TEXT("Meshlet 顶点映射超出 Geometry 范围：") + filename);
                    out.Clear();
                    return false;
                }
            }

            for (uint32_t triangle = 0; triangle < source.triangle_count; ++triangle)
            {
                const uint32_t byte_offset = (source.triangle_offset + triangle) * 3u;
                if (triangle_data[static_cast<int>(byte_offset)] >= source.vertex_count
                    || triangle_data[static_cast<int>(byte_offset + 1u)] >= source.vertex_count
                    || triangle_data[static_cast<int>(byte_offset + 2u)] >= source.vertex_count)
                {
                    MLogError(LoadGeometry, OS_TEXT("Meshlet 三角形局部索引越界：") + filename);
                    out.Clear();
                    return false;
                }
            }

            ssbo::MeshletDescriptorRow &descriptor = out.descriptors[static_cast<int>(i)];
            descriptor.vertex_offset = source.vertex_offset;
            descriptor.triangle_offset = source.triangle_offset;
            descriptor.bounds_index = i;
            descriptor.packed_counts =
                uint32_t(source.vertex_count) | (uint32_t(source.triangle_count) << 8);

            const FileMeshletBounds &source_bound = source_bounds[i];
            ssbo::MeshletBoundsRow &bound = out.bounds[static_cast<int>(i)];
            bound.center_radius = math::Vector4f(source_bound.center[0],
                                                 source_bound.center[1],
                                                 source_bound.center[2],
                                                 source_bound.radius);
            bound.cone_apex[0] = source_bound.cone_apex[0];
            bound.cone_apex[1] = source_bound.cone_apex[1];
            bound.cone_apex[2] = source_bound.cone_apex[2];
            bound.cone_axis_cutoff_packed =
                uint32_t(static_cast<uint8_t>(source_bound.cone_axis_s8[0]))
                | (uint32_t(static_cast<uint8_t>(source_bound.cone_axis_s8[1])) << 8)
                | (uint32_t(static_cast<uint8_t>(source_bound.cone_axis_s8[2])) << 16)
                | (uint32_t(static_cast<uint8_t>(source_bound.cone_cutoff_s8)) << 24);
        }

        for (uint32_t i = 0; i < vertex_count; ++i)
            out.vertices[static_cast<int>(i)].vertex_index = source_vertices[i];
        for (uint32_t i = 0; i < triangle_count; ++i)
        {
            const uint32_t byte_index = i * 3;
            out.triangles[static_cast<int>(i)].packed_indices =
                uint32_t(triangle_data[static_cast<int>(byte_index)])
                | (uint32_t(triangle_data[static_cast<int>(byte_index + 1)]) << 8)
                | (uint32_t(triangle_data[static_cast<int>(byte_index + 2)]) << 16);
        }

        return out.IsValid();
    }

    // Orchestrate reading attributes and indices into GeometryData
    bool ReadGeometryData(hgl::io::minipack::MiniPackReader *mpr,
                          GeometryData *geo_data,
                          const GeometryVertexFormat &geometry_vertex_format,
                          const GeometryHeader &header,
                          const OSString &filename,
                          ValueArray<float> &meshlet_positions,
                          ValueArray<uint32_t> &meshlet_indices)
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

            for (int i = 0; i < file_attributes.GetCount(); ++i)
            {
                const FileAttribute &attribute = file_attributes[i];
                if (attribute.semantic != VertexSemantic::Position
                    || attribute.entry_index < 0)
                    continue;

                if (attribute.format == VK_FORMAT_R32G32B32_SFLOAT)
                {
                    AutoDeleteArray<float> source(header.vertexCount * 3u);
                    const uint32_t bytes = header.vertexCount * 3u * sizeof(float);
                    if (mpr->ReadFile(attribute.entry_index, source.data(), 0, bytes) != bytes)
                        return false;
                    meshlet_positions.Add(source.data(), static_cast<int>(header.vertexCount * 3u));
                }
                else if (attribute.format == VK_FORMAT_R32G32B32A32_SFLOAT)
                {
                    AutoDeleteArray<float> source(header.vertexCount * 4u);
                    const uint32_t bytes = header.vertexCount * 4u * sizeof(float);
                    if (mpr->ReadFile(attribute.entry_index, source.data(), 0, bytes) != bytes)
                        return false;
                    for (uint32_t vertex = 0; vertex < header.vertexCount; ++vertex)
                    {
                        meshlet_positions.Add(source[vertex * 4u]);
                        meshlet_positions.Add(source[vertex * 4u + 1u]);
                        meshlet_positions.Add(source[vertex * 4u + 2u]);
                    }
                }
                break;
            }
        }

        if(header.indexCount>0)
        {
            if(!ReadIndicesData(mpr, geo_data, header, filename, &meshlet_indices))
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

    MeshletGeometryData meshlet_data;
    bool has_prebuilt_meshlets = false;
    if (header.primitiveType == static_cast<uint8_t>(PrimitiveType::Triangles)
        && !ReadMeshletData(mpr, debug_name, header.vertexCount,
                            meshlet_data, has_prebuilt_meshlets))
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
    ValueArray<float> meshlet_positions;
    ValueArray<uint32_t> meshlet_indices;
    if(!ReadGeometryData(mpr, geo_data, geometry_vertex_format, header, debug_name,
                         meshlet_positions, meshlet_indices))
    {
        delete geo_data;
        return nullptr;
    }

    if (header.primitiveType == static_cast<uint8_t>(PrimitiveType::Triangles)
        && !has_prebuilt_meshlets)
    {
        if (meshlet_positions.GetCount() != static_cast<int>(header.vertexCount * 3u))
        {
            MLogError(LoadGeometry, OS_TEXT("三角形几何缺少可构建 Meshlet 的 3D POSITION：") + debug_name);
            delete geo_data;
            return nullptr;
        }

        ValueArray<uint32_t> generated_indices;
        if (header.indexCount == 0)
        {
            if (header.vertexCount == 0 || header.vertexCount % 3u != 0)
            {
                MLogError(LoadGeometry, OS_TEXT("非索引三角形几何顶点数无效：") + debug_name);
                delete geo_data;
                return nullptr;
            }
            generated_indices.Resize(static_cast<int>(header.vertexCount));
            for (uint32_t i = 0; i < header.vertexCount; ++i)
                generated_indices[static_cast<int>(i)] = i;
            meshlet_indices = generated_indices;
        }

        if (!BuildMeshletGeometryData(meshlet_positions.GetData(),
                                      header.vertexCount,
                                      meshlet_indices.GetData(),
                                      static_cast<uint32_t>(meshlet_indices.GetCount()),
                                      meshlet_data))
        {
            MLogError(LoadGeometry, OS_TEXT("无法为三角形几何构建 Meshlet：") + debug_name);
            delete geo_data;
            return nullptr;
        }
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
    if (meshlet_data.IsValid() && !geometry->SetMeshletData(meshlet_data))
    {
        MLogError(LoadGeometry, OS_TEXT("无法绑定 Meshlet 数据到几何体：") + debug_name);
        delete geometry;
        return nullptr;
    }

    return geometry;
}

Geometry *LoadGeometry(VulkanDevice *device,const GeometryVertexFormat &geometry_vertex_format,const OSString &filename)
{
    // 统一取流：`filename` 既可以是历史裸路径，也可以是
    // `pack(res):geo/presets/torus`（无扩展名逻辑路径）或 `pack(res):id:<32 hex>`。
    // 这样从「目录包裹」切到「真实打包文件」时本函数一行都不用改。
    AnsiString open_error;

    hgl::io::InputStream *stream=OpenAssetInputStream(filename,asset::AssetType::Geometry,&open_error);

    if(!stream)
    {
        MLogError(LoadGeometry,OS_TEXT("Cannot open geometry asset ") + filename
                              + OS_TEXT(" : ") + OSString(ToOSString(open_error)));
        return(nullptr);
    }

    return LoadGeometryFromStream(device,geometry_vertex_format,stream,filename);
}

Geometry *LoadGeometryFromStream(VulkanDevice *device,const GeometryVertexFormat &geometry_vertex_format,hgl::io::InputStream *stream,const OSString &debug_name)
{
    using namespace hgl::io::minipack;

    if(!stream)
    {
        MLogError(LoadGeometry,OS_TEXT("LoadGeometryFromStream: null stream for ") + debug_name);
        return(nullptr);
    }

    // 所有权移交：GetMiniPackReader 成功时由读取器释放，失败时就地释放。
    // 因此这里无论成败都不得再 delete stream。
    MiniPackReader *mpr = GetMiniPackReader(stream);

    if(!mpr)
    {
        MLogError(LoadGeometry,OS_TEXT("LoadGeometryFromStream: cannot parse minipack for ") + debug_name);
        return(nullptr);
    }

    Geometry *geometry = LoadGeometryFromReader(device,geometry_vertex_format,mpr,debug_name);

    delete mpr;
    return geometry;
}

Geometry *LoadGeometryFromMiniPackBytes(VulkanDevice *device,const GeometryVertexFormat &geometry_vertex_format,const void *bytes,const uint32 size,const OSString &debug_name)
{
    using namespace hgl::io;

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

    return LoadGeometryFromStream(device,geometry_vertex_format,static_cast<InputStream *>(mis),debug_name);
}
    // ==================== Direct-to-Staging 异步装载（GpuUploadQueue DMA） ====================

    namespace
    {
        using hgl::io::minipack::MiniPackReader;
        using hgl::io::minipack::MiniPackMemory;

        /**
         * MiniPackMemory -> MiniPackReader 适配器。
         *
         * DMA 资格判定要复用经典路径的同一套解析辅助函数（它们经 ReadFile 逐条读取），
         * 而 MiniPackMemory 只有 Map 指针访问。本适配器把 ReadFile 变成 Map + memcpy，
         * 使内存视图拥有与磁盘读取器完全一致的解析语义，不复制任何校验逻辑。
         * 不持有 mpm 的生命周期：它的存活由调用方保证（视图指向已 Map 的 staging）。
         */
        class MemoryAsReader final : public MiniPackReader
        {
            MiniPackMemory *mp;

        public:

            explicit MemoryAsReader(MiniPackMemory *m) : mp(m) {}

            uint GetFileCount() const override { return mp->GetFileCount(); }
            int32 FindFile(const AnsiStringView &name) const override { return mp->FindFile(name); }
            uint32 GetFileLength(int32 index) const override { return mp->GetFileLength(index); }

            uint32 ReadFile(int32 index, void *buf, uint32 start, uint32 size) override
            {
                if (index < 0 || !buf)
                    return 0;

                const uint32 len = mp->GetFileLength(index);

                if (start >= len)
                    return 0;

                if (size > len - start)
                    size = len - start;

                const void *src = mp->Map(index);

                if (!src)
                    return 0;

                memcpy(buf, static_cast<const uint8 *>(src) + start, size);
                return size;
            }
        };

        struct StagedGeometryJob
        {
            Geometry            *geometry     = nullptr;   // DMA 目标几何（含 GPUDeviceLocal 缓冲）
            DeviceBuffer        *staging      = nullptr;   // 移交所有权；finalize 时 Unmap+delete
            int                  pending      = 0;         // 未完成的上传任务数
            bool                 dispatching  = false;     // 入队阶段抑制过早 finalize
            bool                 failed       = false;
            GeometryLoadComplete on_complete  = nullptr;
            void                *user_data    = nullptr;
        };

        void FinalizeStagedGeometryJob(StagedGeometryJob *job)
        {
            Geometry *geometry = job->geometry;
            const bool success = !job->failed && geometry;

            if (job->staging)
            {
                job->staging->Unmap();
                delete job->staging;
            }

            if (!success && geometry)
                delete geometry;    // 失败回滚：连带删除半成品 GeometryData 与显存缓冲

            if (job->on_complete)
                job->on_complete(success ? geometry : nullptr, success, job->user_data);

            delete job;
        }

        void OnStagedGeometryTaskDone(GpuUploadTask *task, void *user_data)
        {
            StagedGeometryJob *job = static_cast<StagedGeometryJob *>(user_data);

            if (!task || task->state != UploadTaskState::Completed)
                job->failed = true;

            --job->pending;

            if (!job->dispatching && job->pending <= 0)
                FinalizeStagedGeometryJob(job);
        }

        // 判定 minipack 内容能否原样 DMA：每个 gvf 属性都要有语义匹配、格式精确一致、
        // 长度吻合的源条目；索引只接受 U32 直通（U8/U16 需展开，走同步回退）。
        bool CanDirectDMAFromMemory(hgl::io::minipack::MiniPack *mpm,
                                    const GeometryHeader &header,
                                    const GeometryVertexFormat &geometry_vertex_format,
                                    const ValueArray<FileAttribute> &file_attributes)
        {
            if (header.indexCount > 0 && header.indexStride != 4)
                return false;

            const uint32_t gvf_attr_count = geometry_vertex_format.GetCount();
            if (gvf_attr_count == 0 || header.attributeCount == 0)
                return false;

            for (uint32_t vab_index = 0; vab_index < gvf_attr_count; ++vab_index)
            {
                const GeometryVertexAttributeFormat *geometry_attribute = geometry_vertex_format.Get(vab_index);
                if (!geometry_attribute)
                    return false;

                const FileAttribute *src_attr = nullptr;
                for (int i = 0; i < file_attributes.GetCount(); ++i)
                {
                    if (file_attributes[i].semantic == geometry_attribute->semantic)
                    {
                        src_attr = &file_attributes[i];
                        break;
                    }
                }

                if (!src_attr || src_attr->entry_index < 0)
                    return false;

                if (src_attr->format != geometry_attribute->format)
                    return false;

                const size_t stride = GetStrideByFormat(geometry_attribute->format);
                if (stride == 0 || src_attr->entry_size != static_cast<uint32>(header.vertexCount * stride))
                    return false;
            }

            return true;
        }
    }//anonymous namespace

    void LoadGeometryFromStagingAsync(VulkanDevice *device,
                                      const GeometryVertexFormat &geometry_vertex_format,
                                      GpuUploadQueue *queue,
                                      DeviceBuffer *staging,
                                      const void *bytes,
                                      uint32 size,
                                      const OSString &debug_name,
                                      GeometryLoadComplete on_complete,
                                      void *user_data)
    {
        using namespace hgl::io::minipack;

        auto fail_sync = [&](bool release_staging)
        {
            if (release_staging && staging)
            {
                staging->Unmap();
                delete staging;
            }
            if (on_complete)
                on_complete(nullptr, false, user_data);
        };

        if (!on_complete)
            return;    // 调用方错误；不触碰 staging，交由调用方清理

        if (!device || !bytes || size == 0)
        {
            fail_sync(true);
            return;
        }

        // 解析失败/需要格式转换/无队列 ⇒ 同步回退（经典路径，含转换逻辑），随后释放 staging
        bool classic_fallback = true;

        MiniPackMemory *mpm = nullptr;
        GeometryHeader header{};
        math::BoundingVolumes bounding_volumes;
        ValueArray<FileAttribute> file_attributes;

        if (queue)
        {
            mpm = GetMiniPackMemoryFromBytes(debug_name, bytes, size);

            MemoryAsReader reader(mpm);
            MiniPackReader *mpr = mpm ? &reader : nullptr;

            if (mpm
             && !(mpm->FindFile(AnsiStringView("meshlets")) >= 0
               || mpm->FindFile(AnsiStringView("meshlet_vertices")) >= 0
               || mpm->FindFile(AnsiStringView("meshlet_triangles")) >= 0)
             && ReadGeometryHeader(mpr, header, debug_name)
             && ReadBoundingVolumes(mpr, bounding_volumes, debug_name)
             && ParseAttributeMeta(mpr, header, debug_name, file_attributes)
             && CanDirectDMAFromMemory(mpm, header, geometry_vertex_format, file_attributes))
            {
                classic_fallback = false;
            }
        }

        if (classic_fallback)
        {
            if (mpm)
                delete mpm;

            Geometry *geometry = LoadGeometryFromMiniPackBytes(device, geometry_vertex_format, bytes, size, debug_name);

            if (staging)
            {
                staging->Unmap();
                delete staging;
            }

            on_complete(geometry, geometry != nullptr, user_data);
            return;
        }

        // ── DMA 路径：纯 device-local 几何 + 每 VAB/IBO 一个 BufferToBuffer 任务 ──
        GeometryData *geo_data = CreateGeometryData(device, geometry_vertex_format, header.vertexCount,
                                                    BufferAllocPolicy::GPUDeviceLocal);
        if (!geo_data)
        {
            MLogError(LoadGeometry, OS_TEXT("Cannot create device-local GeometryData for ") + debug_name);
            delete mpm;
            fail_sync(true);
            return;
        }

        if (!geo_data->CreateAllVAB())
        {
            MLogError(LoadGeometry, OS_TEXT("Cannot create VAB for ") + debug_name);
            delete geo_data;
            delete mpm;
            fail_sync(true);
            return;
        }

        const U8String geo_name = ToU8String(debug_name);
        Geometry *geometry = new Geometry((char *)geo_name.c_str(), geo_data);
        geometry->SetBoundingVolumes(bounding_volumes);

        StagedGeometryJob *job = new StagedGeometryJob;
        job->geometry    = geometry;
        job->staging     = staging;
        job->on_complete = on_complete;
        job->user_data   = user_data;
        job->dispatching = true;    // 入队期间不允许 finalize

        const uint8_t *base = static_cast<const uint8_t *>(bytes);
        const uint32_t gvf_attr_count = geometry_vertex_format.GetCount();

        for (uint32_t vab_index = 0; vab_index < gvf_attr_count; ++vab_index)
        {
            const GeometryVertexAttributeFormat *geometry_attribute = geometry_vertex_format.Get(vab_index);

            const FileAttribute *src_attr = nullptr;
            for (int i = 0; i < file_attributes.GetCount(); ++i)
            {
                if (file_attributes[i].semantic == geometry_attribute->semantic)
                {
                    src_attr = &file_attributes[i];
                    break;
                }
            }

            VAB *vab = geo_data->GetVAB((int)vab_index);
            const void *src_ptr = src_attr ? mpm->Map(src_attr->entry_index) : nullptr;

            if (!vab || !src_ptr)
            {
                job->failed = true;
                break;
            }

            const VkDeviceSize src_offset = (const uint8_t *)src_ptr - base;
            const VkDeviceSize copy_size  = (VkDeviceSize)src_attr->entry_size;

            const uint64_t id = queue->EnqueueBufferCopy(staging, src_offset, vab, 0, copy_size,
                                                         UploadPriority::Normal,
                                                         OnStagedGeometryTaskDone, job, false);
            if (id == 0)
            {
                job->failed = true;
                break;
            }
            ++job->pending;
        }

        if (!job->failed && header.indexCount > 0)
        {
            const int32 indices_index = mpm->FindFile(AnsiStringView("indices"));
            IndexBuffer *ibo = (indices_index >= 0)
                ? geo_data->InitIBO((int)header.indexCount, IndexType::U32, "GeometryAsync:IBO")
                : nullptr;
            const void *src_ptr = ibo ? mpm->Map(indices_index) : nullptr;

            if (!src_ptr)
            {
                job->failed = true;
            }
            else
            {
                const VkDeviceSize src_offset = (const uint8_t *)src_ptr - base;
                const uint64_t id = queue->EnqueueBufferCopy(staging, src_offset, ibo, 0,
                                                             (VkDeviceSize)header.indexCount * 4,
                                                             UploadPriority::Normal,
                                                             OnStagedGeometryTaskDone, job, false);
                if (id == 0)
                    job->failed = true;
                else
                    ++job->pending;
            }
        }

        delete mpm;

        job->dispatching = false;
        if (job->pending <= 0)
        {
            job->failed = true;    // 一个任务都没挂上：视为失败，统一回滚
            FinalizeStagedGeometryJob(job);
            return;
        }
        // 否则：等待最后一个上传任务回调 finalize
    }
}//namespace hgl::graph

#pragma once

#include <hgl/common/VertexAttribDef.h>
#include <hgl/vk/VKVertexInputLayout.h>
#include <vector>
#include <cstring>

namespace hgl::graph
{
    enum class GeometryVertexSemantic : uint8
    {
        Unknown = 0,
        Position,
        Normal,
        Tangent,
        Bitangent,
        Color,
        Luminance,
        TexCoord,
        AO,
        Size,
        Rotation,
        Assign,
        JointID,
        JointWeight
    };

    inline const char *GetGeometryVertexSemanticName(const GeometryVertexSemantic semantic)
    {
        switch (semantic)
        {
        case GeometryVertexSemantic::Position: return "Position";
        case GeometryVertexSemantic::Normal: return "Normal";
        case GeometryVertexSemantic::Tangent: return "Tangent";
        case GeometryVertexSemantic::Bitangent: return "Bitangent";
        case GeometryVertexSemantic::Color: return "Color";
        case GeometryVertexSemantic::Luminance: return "Luminance";
        case GeometryVertexSemantic::TexCoord: return "TexCoord";
        case GeometryVertexSemantic::AO: return "AO";
        case GeometryVertexSemantic::Size: return "Size";
        case GeometryVertexSemantic::Rotation: return "Rotation";
        case GeometryVertexSemantic::Assign: return "Assign";
        case GeometryVertexSemantic::JointID: return "JointID";
        case GeometryVertexSemantic::JointWeight: return "JointWeight";
        default: return "Unknown";
        }
    }

    inline GeometryVertexSemantic GetGeometryVertexSemanticByName(const char *name)
    {
        if (!name || !*name)
            return GeometryVertexSemantic::Unknown;

        if (std::strcmp(name, VAN::Position) == 0) return GeometryVertexSemantic::Position;
        if (std::strcmp(name, VAN::Normal) == 0) return GeometryVertexSemantic::Normal;
        if (std::strcmp(name, VAN::Tangent) == 0) return GeometryVertexSemantic::Tangent;
        if (std::strcmp(name, VAN::Bitangent) == 0) return GeometryVertexSemantic::Bitangent;
        if (std::strcmp(name, VAN::Color) == 0) return GeometryVertexSemantic::Color;
        if (std::strcmp(name, VAN::Luminance) == 0) return GeometryVertexSemantic::Luminance;
        if (std::strcmp(name, VAN::TexCoord) == 0) return GeometryVertexSemantic::TexCoord;
        if (std::strcmp(name, VAN::AO) == 0) return GeometryVertexSemantic::AO;
        if (std::strcmp(name, VAN::Size) == 0) return GeometryVertexSemantic::Size;
        if (std::strcmp(name, VAN::Rotation) == 0) return GeometryVertexSemantic::Rotation;
        if (std::strcmp(name, VAN::Assign) == 0) return GeometryVertexSemantic::Assign;
        if (std::strcmp(name, VAN::JointID) == 0) return GeometryVertexSemantic::JointID;
        if (std::strcmp(name, VAN::JointWeight) == 0) return GeometryVertexSemantic::JointWeight;

        return GeometryVertexSemantic::Unknown;
    }

    struct GeometryVertexAttributeFormat
    {
        char name[VERTEX_ATTRIB_NAME_MAX_LENGTH]{};
        GeometryVertexSemantic semantic = GeometryVertexSemantic::Unknown;
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint8_t vec_size = 0;
        uint32_t stride = 0;
    };

    inline void CopyGeometryVertexAttributeName(char (&dst)[VERTEX_ATTRIB_NAME_MAX_LENGTH], const char *src)
    {
        dst[0] = 0;

        if (!src || !*src)
            return;

        std::strncpy(dst, src, VERTEX_ATTRIB_NAME_MAX_LENGTH - 1);
        dst[VERTEX_ATTRIB_NAME_MAX_LENGTH - 1] = 0;
    }

    class GeometryVertexFormat
    {
    private:

        std::vector<GeometryVertexAttributeFormat> attributes;

    public:

        void Clear()
        {
            attributes.clear();
        }

        uint32_t GetCount() const
        {
            return static_cast<uint32_t>(attributes.size());
        }

        const GeometryVertexAttributeFormat *Get(const uint32_t index) const
        {
            return index < attributes.size() ? &attributes[index] : nullptr;
        }

        const GeometryVertexAttributeFormat *Find(const char *name) const
        {
            if (!name || !*name)
                return nullptr;

            for (const auto &attribute : attributes)
            {
                if (std::strcmp(attribute.name, name) == 0)
                    return &attribute;
            }

            return nullptr;
        }

        bool Add(const char *name, const VkFormat format, const uint8_t vec_size, const uint32_t stride)
        {
            if (!name || !*name || Find(name))
                return false;

            GeometryVertexAttributeFormat attribute;
            CopyGeometryVertexAttributeName(attribute.name, name);
            attribute.semantic = GetGeometryVertexSemanticByName(name);
            attribute.format = format;
            attribute.vec_size = vec_size;
            attribute.stride = stride;

            attributes.push_back(attribute);
            return true;
        }

        static GeometryVertexFormat FromVIL(const VIL &vil)
        {
            GeometryVertexFormat result;

            for (uint32_t i = 0; i < vil.GetVertexAttribCount(); ++i)
            {
                const VertexInputFormat *vif = vil.GetConfig(i);

                if (!vif || !vif->name || !*vif->name)
                    continue;

                result.Add(vif->name, vif->format, uint8_t(vif->vec_size), uint32_t(vif->stride));
            }

            return result;
        }
    };

    enum class GeometryVertexMatchKind : uint8
    {
        Exact = 0,
        Compatible,
        Unsupported,
        Mismatch
    };

    inline const char *GetGeometryVertexMatchKindName(const GeometryVertexMatchKind kind)
    {
        switch (kind)
        {
        case GeometryVertexMatchKind::Exact: return "Exact";
        case GeometryVertexMatchKind::Compatible: return "Compatible";
        case GeometryVertexMatchKind::Unsupported: return "Unsupported";
        default: return "Mismatch";
        }
    }

    struct GeometryVertexAttributeMatch
    {
        GeometryVertexMatchKind kind = GeometryVertexMatchKind::Mismatch;
        GeometryVertexSemantic semantic = GeometryVertexSemantic::Unknown;
        char name[VERTEX_ATTRIB_NAME_MAX_LENGTH]{};
        VkFormat geometry_format = VK_FORMAT_UNDEFINED;
        VkFormat material_format = VK_FORMAT_UNDEFINED;
        uint32_t geometry_stride = 0;
        uint32_t material_stride = 0;
        uint8_t geometry_vec_size = 0;
        uint8_t material_vec_size = 0;
    };

    struct GeometryVertexFormatMatch
    {
        GeometryVertexMatchKind overall = GeometryVertexMatchKind::Exact;
        std::vector<GeometryVertexAttributeMatch> attributes;

        const GeometryVertexAttributeMatch *FirstNonExact() const
        {
            for (const auto &attribute : attributes)
            {
                if (attribute.kind != GeometryVertexMatchKind::Exact)
                    return &attribute;
            }

            return nullptr;
        }
    };

    inline bool SupportsFutureFormatCompatibility(const GeometryVertexSemantic semantic)
    {
        switch (semantic)
        {
        case GeometryVertexSemantic::Normal:
        case GeometryVertexSemantic::Tangent:
        case GeometryVertexSemantic::Bitangent:
        case GeometryVertexSemantic::Color:
        case GeometryVertexSemantic::TexCoord:
            return true;
        default:
            return false;
        }
    }

    inline GeometryVertexMatchKind MatchGeometryVertexAttribute(
        const GeometryVertexAttributeFormat &geometry_attribute,
        const VertexInputFormat &material_attribute)
    {
        if (geometry_attribute.format == material_attribute.format
         && geometry_attribute.stride == material_attribute.stride
         && geometry_attribute.vec_size == material_attribute.vec_size)
        {
            return GeometryVertexMatchKind::Exact;
        }

        if (geometry_attribute.vec_size == material_attribute.vec_size
         && geometry_attribute.stride == material_attribute.stride
         && SupportsFutureFormatCompatibility(geometry_attribute.semantic))
        {
            return GeometryVertexMatchKind::Unsupported;
        }

        return GeometryVertexMatchKind::Mismatch;
    }

    inline GeometryVertexFormatMatch MatchGeometryVertexFormat(
        const GeometryVertexFormat &geometry_format,
        const VIL &material_vil)
    {
        GeometryVertexFormatMatch result;

        for (uint32_t i = 0; i < material_vil.GetVertexAttribCount(); ++i)
        {
            const VertexInputFormat *material_attribute = material_vil.GetConfig(i);
            if (!material_attribute || !material_attribute->name || !*material_attribute->name)
                continue;

            GeometryVertexAttributeMatch match;
            CopyGeometryVertexAttributeName(match.name, material_attribute->name);
            match.semantic = GetGeometryVertexSemanticByName(material_attribute->name);
            match.material_format = material_attribute->format;
            match.material_stride = uint32_t(material_attribute->stride);
            match.material_vec_size = uint8_t(material_attribute->vec_size);

            const GeometryVertexAttributeFormat *geometry_attribute = geometry_format.Find(material_attribute->name);
            if (!geometry_attribute)
            {
                match.kind = GeometryVertexMatchKind::Mismatch;
            }
            else
            {
                match.geometry_format = geometry_attribute->format;
                match.geometry_stride = geometry_attribute->stride;
                match.geometry_vec_size = geometry_attribute->vec_size;
                match.kind = MatchGeometryVertexAttribute(*geometry_attribute, *material_attribute);
            }

            if (match.kind == GeometryVertexMatchKind::Mismatch)
                result.overall = GeometryVertexMatchKind::Mismatch;
            else if (match.kind == GeometryVertexMatchKind::Unsupported
                  && result.overall != GeometryVertexMatchKind::Mismatch)
                result.overall = GeometryVertexMatchKind::Unsupported;
            else if (match.kind == GeometryVertexMatchKind::Compatible
                  && result.overall == GeometryVertexMatchKind::Exact)
                result.overall = GeometryVertexMatchKind::Compatible;

            result.attributes.push_back(match);
        }

        return result;
    }
}

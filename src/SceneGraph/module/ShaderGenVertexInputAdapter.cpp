#include <hgl/graph/module/ShaderGenVertexInputAdapter.h>
#include <hgl/vk/VKRenderAssign.h>
#include <cstdio>
#include <vector>

namespace hgl::graph
{
    VertexInputGroup ResolveVertexInputGroupBySemantic(std::string_view semantic)
    {
        if (semantic == Assign::TransformID::VIS_NAME)
            return VertexInputGroup::TransformID;

        if (semantic == Assign::MaterialInstanceID::VIS_NAME)
            return VertexInputGroup::MaterialInstanceID;

        if (semantic == VAN::JointID)
            return VertexInputGroup::JointID;

        if (semantic == VAN::JointWeight)
            return VertexInputGroup::JointWeight;

        return VertexInputGroup::Basic;
    }

    bool BuildVertexInputFromContractLayout(const mtl::contract::VertexInputLayout &layout,
                                            VIAArray &out_input,
                                            std::string &reason)
    {
        out_input.Clear();

        const uint32_t attr_count = static_cast<uint32_t>(layout.attributes.size());
        if (attr_count == 0)
        {
            reason = "mirror result has no vertex attributes";
            return false;
        }

        if (!out_input.Init(attr_count))
        {
            reason = "failed to allocate mirror vertex input array";
            return false;
        }

        std::vector<uint8_t> location_seen(256, 0);

        for (uint32_t i = 0; i < attr_count; ++i)
        {
            const auto &src_attr = layout.attributes[i];

            if (src_attr.location > 255)
            {
                reason = "mirror vertex location overflow";
                return false;
            }

            if (src_attr.input_rate > 255)
            {
                reason = "mirror vertex input_rate overflow";
                return false;
            }

            if (src_attr.semantic.empty())
            {
                reason = "mirror vertex semantic is empty";
                return false;
            }

            if (src_attr.semantic.size() >= VERTEX_ATTRIB_NAME_MAX_LENGTH)
            {
                reason = "mirror vertex semantic is too long";
                return false;
            }

            const uint8_t loc = static_cast<uint8_t>(src_attr.location);
            if (location_seen[loc])
            {
                reason = "mirror vertex location duplicated";
                return false;
            }
            location_seen[loc] = 1;

            VAType parsed_type;
            if (!ParseVertexAttribType(&parsed_type, src_attr.type_name.c_str()))
            {
                reason = "unrecognized mirror vertex type_name";
                return false;
            }

            VIA dst{};
            std::snprintf(dst.name, sizeof(dst.name), "%s", src_attr.semantic.c_str());
            dst.location = loc;
            dst.basetype = static_cast<uint8_t>(parsed_type.basetype);
            dst.vec_size = parsed_type.vec_size;
            dst.input_rate = static_cast<uint8_t>(src_attr.input_rate);
            dst.group = ResolveVertexInputGroupBySemantic(src_attr.semantic);
            dst.interpolation = Interpolation::Smooth;

            out_input.items[i] = dst;
        }

        return true;
    }
}

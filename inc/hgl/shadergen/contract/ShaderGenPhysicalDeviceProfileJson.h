#pragma once

namespace hgl::graph::mtl {}

#include <hgl/shadergen/contract/ShaderGenContract.h>
#include <hgl/shadergen/contract/ShaderGenProfileTargetVersion.h>
#include <cctype>
#include <cstdint>
#include <string>

namespace hgl::graph::shadergen::contract
{
    using namespace hgl::graph::mtl;
    namespace detail
    {
        inline bool FindJsonKeyPos(const std::string &json, const std::string &key, std::size_t &value_pos)
        {
            const std::string marker = "\"" + key + "\"";
            const std::size_t key_pos = json.find(marker);
            if (key_pos == std::string::npos)
                return false;

            const std::size_t colon_pos = json.find(':', key_pos + marker.size());
            if (colon_pos == std::string::npos)
                return false;

            value_pos = colon_pos + 1;
            while (value_pos < json.size() && std::isspace(static_cast<unsigned char>(json[value_pos])))
                ++value_pos;

            return value_pos < json.size();
        }

        inline bool ReadJsonString(const std::string &json, const std::string &key, std::string &out)
        {
            std::size_t pos = 0;
            if (!FindJsonKeyPos(json, key, pos) || json[pos] != '"')
                return false;

            ++pos;
            std::string value;
            value.reserve(64);

            while (pos < json.size())
            {
                const char ch = json[pos++];
                if (ch == '"')
                {
                    out = std::move(value);
                    return true;
                }

                if (ch == '\\' && pos < json.size())
                {
                    const char esc = json[pos++];
                    switch (esc)
                    {
                        case '"': value.push_back('"'); break;
                        case '\\': value.push_back('\\'); break;
                        case 'n': value.push_back('\n'); break;
                        case 'r': value.push_back('\r'); break;
                        case 't': value.push_back('\t'); break;
                        default: value.push_back(esc); break;
                    }
                }
                else
                {
                    value.push_back(ch);
                }
            }

            return false;
        }

        inline bool ReadJsonUint64(const std::string &json, const std::string &key, uint64_t &out)
        {
            std::size_t pos = 0;
            if (!FindJsonKeyPos(json, key, pos))
                return false;

            uint64_t value = 0;
            bool has_digit = false;

            while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos])))
            {
                has_digit = true;
                value = value * 10u + static_cast<uint64_t>(json[pos] - '0');
                ++pos;
            }

            if (!has_digit)
                return false;

            out = value;
            return true;
        }

        inline bool ReadJsonBool(const std::string &json, const std::string &key, bool &out)
        {
            std::size_t pos = 0;
            if (!FindJsonKeyPos(json, key, pos))
                return false;

            if (json.compare(pos, 4, "true") == 0)
            {
                out = true;
                return true;
            }

            if (json.compare(pos, 5, "false") == 0)
            {
                out = false;
                return true;
            }

            return false;
        }

        inline uint32_t CountQueueFamilies(const std::string &json)
        {
            const std::size_t qf_pos = json.find("\"queue_families\"");
            if (qf_pos == std::string::npos)
                return 0;

            const std::size_t arr_begin = json.find('[', qf_pos);
            const std::size_t arr_end = json.find(']', arr_begin == std::string::npos ? qf_pos : arr_begin);
            if (arr_begin == std::string::npos || arr_end == std::string::npos || arr_end <= arr_begin)
                return 0;

            uint32_t count = 0;
            std::size_t cursor = arr_begin;
            while (true)
            {
                cursor = json.find("\"queueFlags\"", cursor + 1);
                if (cursor == std::string::npos || cursor > arr_end)
                    break;
                ++count;
            }

            return count;
        }
    }

    inline bool BuildPhysicalDeviceProfileFromCollectorJson(const std::string &json,
                                                            PhysicalDeviceProfileLite &out_profile)
    {
        out_profile = PhysicalDeviceProfileLite{};

        uint64_t device_count = 0;
        if (!detail::ReadJsonUint64(json, "device_count", device_count) || device_count == 0)
            return false;

        if (!detail::ReadJsonString(json, "name", out_profile.name))
            return false;

        detail::ReadJsonString(json, "device_type", out_profile.device_type);
        detail::ReadJsonString(json, "capability_tier", out_profile.capability_tier);

        {
            uint64_t v = 0;
            if (detail::ReadJsonUint64(json, "vendor_id", v)) out_profile.vendor_id = static_cast<uint32_t>(v);
            if (detail::ReadJsonUint64(json, "device_id", v)) out_profile.device_id = static_cast<uint32_t>(v);
            if (detail::ReadJsonUint64(json, "api_version", v)) out_profile.api_version = static_cast<uint32_t>(v);

            if (detail::ReadJsonUint64(json, "maxImageDimension2D", v)) out_profile.limits.max_image_dimension_2d = static_cast<uint32_t>(v);
            if (detail::ReadJsonUint64(json, "maxPushConstantsSize", v)) out_profile.limits.max_push_constants_size = static_cast<uint32_t>(v);
            if (detail::ReadJsonUint64(json, "maxVertexInputAttributes", v)) out_profile.limits.max_vertex_input_attributes = static_cast<uint32_t>(v);
            if (detail::ReadJsonUint64(json, "maxBoundDescriptorSets", v)) out_profile.limits.max_bound_descriptor_sets = static_cast<uint32_t>(v);
            if (detail::ReadJsonUint64(json, "maxUniformBufferRange", v)) out_profile.limits.max_uniform_buffer_range = v;
            if (detail::ReadJsonUint64(json, "maxStorageBufferRange", v)) out_profile.limits.max_storage_buffer_range = v;
        }

        ResolveShaderTargetVersions(out_profile,
                                    out_profile.target_vulkan_version,
                                    out_profile.target_spv_version);

        {
            uint64_t v = 0;
            if (detail::ReadJsonUint64(json, "target_vulkan_version", v)) out_profile.target_vulkan_version = static_cast<uint32_t>(v);
            if (detail::ReadJsonUint64(json, "target_spv_version", v)) out_profile.target_spv_version = static_cast<uint32_t>(v);
        }

        detail::ReadJsonBool(json, "geometryShader", out_profile.features.geometry_shader);
        detail::ReadJsonBool(json, "tessellationShader", out_profile.features.tessellation_shader);
        detail::ReadJsonBool(json, "wideLines", out_profile.features.wide_lines);
        detail::ReadJsonBool(json, "samplerAnisotropy", out_profile.features.sampler_anisotropy);
        detail::ReadJsonBool(json, "indexTypeUint8", out_profile.features.index_type_uint8);
        detail::ReadJsonBool(json, "descriptorIndexing", out_profile.features.descriptor_indexing);
        detail::ReadJsonBool(json, "samplerMirrorClampToEdge", out_profile.features.sampler_mirror_clamp_to_edge);

        out_profile.queue_family_count = detail::CountQueueFamilies(json);
        return true;
    }
}

#include <hgl/vk/VKInstance.h>
#include <hgl/vk/VKPhysicalDevice.h>

#include <cstdio>
#include <ctime>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>

using namespace hgl::graph;

namespace
{
    class StdoutSilencer
    {
    public:
        StdoutSilencer()
        {
            old_buf = std::cout.rdbuf(null_stream.rdbuf());
        }

        ~StdoutSilencer()
        {
            std::cout.rdbuf(old_buf);
        }

    private:
        std::streambuf *old_buf = nullptr;
        std::ostringstream null_stream;
    };

    static std::string JsonEscape(const char *text)
    {
        if (!text)
            return "";

        std::string out;
        for (const unsigned char ch : std::string(text))
        {
            switch (ch)
            {
                case '\\': out += "\\\\"; break;
                case '"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (ch < 0x20)
                    {
                        char buf[8] = {};
                        std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
                        out += buf;
                    }
                    else
                    {
                        out += static_cast<char>(ch);
                    }
                    break;
            }
        }
        return out;
    }

    static const char *DeviceTypeName(const VkPhysicalDeviceType type)
    {
        switch (type)
        {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
            case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
            case VK_PHYSICAL_DEVICE_TYPE_OTHER: return "other";
            default: return "unknown";
        }
    }

    static const char *CapabilityTier(const VulkanPhyDevice &pd)
    {
        const auto &limits = pd.GetLimits();

        const bool high =
            pd.isDiscreteGPU() &&
            limits.maxImageDimension2D >= 8192 &&
            limits.maxUniformBufferRange >= (64u * 1024u) &&
            limits.maxStorageBufferRange >= (64u * 1024u * 1024u);

        if (high)
            return "high";

        const bool medium =
            limits.maxImageDimension2D >= 4096 &&
            limits.maxUniformBufferRange >= (32u * 1024u);

        if (medium)
            return "medium";

        return "low";
    }

    static void WriteQueueFamilies(std::ostream &out, const VulkanPhyDevice &pd)
    {
        const auto &families = pd.GetQueueFamilyProperties();

        out << "      \"queue_families\": [\n";
        for (int i = 0; i < (int)families.size(); ++i)
        {
            const auto &q = families[i];
            out << "        {\"index\": " << i
                << ", \"queueFlags\": " << q.queueFlags
                << ", \"queueCount\": " << q.queueCount
                << ", \"timestampValidBits\": " << q.timestampValidBits
                << "}";
            if (i + 1 < (int)families.size())
                out << ",";
            out << "\n";
        }
        out << "      ]\n";
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    VulkanInstance *instance = nullptr;
    {
        StdoutSilencer silencer;

        InitVulkanInstanceProperties();

        const hgl::U8String app_name((const hgl::u8char*)u8"ULRE.VulkanPhysicalDeviceProfileCollector");
        instance = CreateInstance(app_name, nullptr, nullptr);
    }

    if (!instance)
    {
        std::fprintf(stderr, "[FAIL] CreateInstance failed\n");
        return 1;
    }

    const auto &devices = instance->GetDeviceList();
    const int device_count = devices.GetCount();

    std::ostringstream json_out;

    std::time_t now = std::time(nullptr);

    json_out << "{\n";
    json_out << "  \"schema_version\": 1,\n";
    json_out << "  \"status\": \"PASS\",\n";
    json_out << "  \"collected_at_epoch\": " << static_cast<long long>(now) << ",\n";
    json_out << "  \"device_count\": " << device_count << ",\n";
    json_out << "  \"devices\": [\n";


    for (int i = 0; i < device_count; ++i)
    {
        const VulkanPhyDevice *pd = devices[i];
        if (!pd)
            continue;

        const auto &props = pd->GetProperties();
        const auto &limits = pd->GetLimits();
        const auto &f10 = pd->GetFeatures10();
        const auto &f12 = pd->GetFeatures12();
        const auto &f14 = pd->GetFeatures14();

        json_out << "    {\n";
        json_out << "      \"index\": " << i << ",\n";
        json_out << "      \"name\": \"" << JsonEscape(pd->GetDeviceName()) << "\",\n";
        json_out << "      \"device_type\": \"" << DeviceTypeName(pd->GetDeviceType()) << "\",\n";
        json_out << "      \"vendor_id\": " << props.vendorID << ",\n";
        json_out << "      \"device_id\": " << props.deviceID << ",\n";
        json_out << "      \"api_version\": " << pd->GetVulkanVersion() << ",\n";
        json_out << "      \"capability_tier\": \"" << CapabilityTier(*pd) << "\",\n";
        json_out << "      \"limits\": {\n";
        json_out << "        \"maxImageDimension2D\": " << limits.maxImageDimension2D << ",\n";
        json_out << "        \"maxUniformBufferRange\": " << limits.maxUniformBufferRange << ",\n";
        json_out << "        \"maxStorageBufferRange\": " << limits.maxStorageBufferRange << ",\n";
        json_out << "        \"maxPushConstantsSize\": " << limits.maxPushConstantsSize << ",\n";
        json_out << "        \"maxVertexInputAttributes\": " << limits.maxVertexInputAttributes << ",\n";
        json_out << "        \"maxBoundDescriptorSets\": " << limits.maxBoundDescriptorSets << "\n";
        json_out << "      },\n";
        json_out << "      \"features\": {\n";
        json_out << "        \"geometryShader\": " << (f10.geometryShader ? "true" : "false") << ",\n";
        json_out << "        \"tessellationShader\": " << (f10.tessellationShader ? "true" : "false") << ",\n";
        json_out << "        \"wideLines\": " << (f10.wideLines ? "true" : "false") << ",\n";
        json_out << "        \"samplerAnisotropy\": " << (f10.samplerAnisotropy ? "true" : "false") << ",\n";
        json_out << "        \"indexTypeUint8\": " << (f14.indexTypeUint8 ? "true" : "false") << ",\n";
        json_out << "        \"descriptorIndexing\": " << (f12.descriptorIndexing ? "true" : "false") << ",\n";
        json_out << "        \"samplerMirrorClampToEdge\": " << (f12.samplerMirrorClampToEdge ? "true" : "false") << "\n";
        json_out << "      },\n";

        WriteQueueFamilies(json_out, *pd);

        json_out << "    }";
        if (i + 1 < devices.GetCount())
            json_out << ",";
        json_out << "\n";

    }

    json_out << "  ]\n";
    json_out << "}\n";

    delete instance;

    std::cout << json_out.str();
    return 0;
}

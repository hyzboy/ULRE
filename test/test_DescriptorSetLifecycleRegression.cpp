#include <hgl/vk/IGPUBuffer.h>
#include <hgl/vk/VKDescriptorSet.h>

#include <hgl/vk/VKSampler.h>
#include <hgl/vk/VKTexture.h>

#include <cstdint>
#include <cstdio>
#include <vector>

using namespace hgl::graph;

class FakeGPUBuffer final : public IGPUBuffer
{
    VkDescriptorBufferInfo info{};

public:
    explicit FakeGPUBuffer(uint64_t id)
        : IGPUBuffer("FakeGPUBuffer")
    {
        info.buffer = reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(id));
        info.offset = 0;
        info.range = 256;
    }

    bool Write(const void *, VkDeviceSize, VkDeviceSize) override { return true; }
    void *Map(VkDeviceSize, VkDeviceSize) override { return nullptr; }
    void Unmap() override {}

    void MarkDirty(VkDeviceSize, VkDeviceSize) override {}
    bool IsDirty() const override { return false; }
    void ClearDirty() override {}

    void CopyToDevice(VkCommandBuffer) override {}

    VkDeviceSize GetSize() const override { return info.range; }
    VkBuffer GetVkDeviceBuffer() const override { return info.buffer; }
    VkDescriptorBufferInfo GetDescriptorBufferInfo() const override { return info; }
};

class FakeTexture final : public Texture
{
public:
    FakeTexture()
        : Texture(nullptr, 0, nullptr)
    {}
};

class FakeSampler final : public Sampler
{
public:
    FakeSampler()
        : Sampler(VK_NULL_HANDLE, VK_NULL_HANDLE)
    {}
};

static bool ValidateBufferPointers(const DescriptorSet &ds)
{
    const auto &buffer_list = ds.DebugGetBufferInfoList();
    const auto &write_list = ds.DebugGetWriteSetList();

    const auto *buffer_begin = buffer_list.GetData();
    const auto *buffer_end = buffer_begin ? buffer_begin + buffer_list.GetCount() : nullptr;

    bool ok = true;

    for (int i = 0; i < write_list.GetCount(); ++i)
    {
        const VkWriteDescriptorSet &wds = write_list[i];

        const bool is_buffer_desc =
            wds.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
            wds.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
            wds.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
            wds.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;

        if (!is_buffer_desc)
            continue;

        if (!wds.pBufferInfo)
        {
            std::printf("[FAIL] buffer descriptor has null pBufferInfo (idx=%d, binding=%u)\n", i, wds.dstBinding);
            ok = false;
            continue;
        }

        if (!buffer_begin || wds.pBufferInfo < buffer_begin || wds.pBufferInfo >= buffer_end)
        {
            std::printf("[FAIL] stale pBufferInfo pointer (idx=%d, binding=%u, ptr=%p, range=[%p,%p))\n",
                        i,
                        wds.dstBinding,
                        static_cast<const void *>(wds.pBufferInfo),
                        static_cast<const void *>(buffer_begin),
                        static_cast<const void *>(buffer_end));
            ok = false;
        }
    }

    return ok;
}

static bool ValidateImagePointers(const DescriptorSet &ds)
{
    const auto &image_list = ds.DebugGetImageInfoList();
    const auto &write_list = ds.DebugGetWriteSetList();

    const auto *image_begin = image_list.GetData();
    const auto *image_end = image_begin ? image_begin + image_list.GetCount() : nullptr;

    bool ok = true;

    for (int i = 0; i < write_list.GetCount(); ++i)
    {
        const VkWriteDescriptorSet &wds = write_list[i];

        const bool is_image_desc =
            wds.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
            wds.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
            wds.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

        if (!is_image_desc)
            continue;

        if (!wds.pImageInfo)
        {
            std::printf("[FAIL] image descriptor has null pImageInfo (idx=%d, binding=%u)\n", i, wds.dstBinding);
            ok = false;
            continue;
        }

        if (!image_begin || wds.pImageInfo < image_begin || wds.pImageInfo >= image_end)
        {
            std::printf("[FAIL] stale pImageInfo pointer (idx=%d, binding=%u, ptr=%p, range=[%p,%p))\n",
                        i,
                        wds.dstBinding,
                        static_cast<const void *>(wds.pImageInfo),
                        static_cast<const void *>(image_begin),
                        static_cast<const void *>(image_end));
            ok = false;
        }
    }

    return ok;
}

static bool RunBufferLifetimeRegression()
{
    std::printf("\n[Test] Buffer descriptor pointer lifetime (BindUBO/BindSSBO)\n");

    DescriptorSet ds(VK_NULL_HANDLE, 1, VK_NULL_HANDLE, VK_NULL_HANDLE);

    std::vector<FakeGPUBuffer> buffers;
    buffers.reserve(128);
    for (uint64_t i = 0; i < 128; ++i)
        buffers.emplace_back(i + 1);

    bool ok = true;

    for (int i = 0; i < 64; ++i)
    {
        if (!ds.BindUBO(i, &buffers[size_t(i)], false))
        {
            std::printf("[FAIL] BindUBO failed at binding=%d\n", i);
            ok = false;
            break;
        }

        if (!ValidateBufferPointers(ds))
        {
            ok = false;
            break;
        }
    }

    for (int i = 0; ok && i < 64; ++i)
    {
        const int binding = 100 + i;
        if (!ds.BindSSBO(binding, &buffers[size_t(i)], false))
        {
            std::printf("[FAIL] BindSSBO failed at binding=%d\n", binding);
            ok = false;
            break;
        }

        if (!ValidateBufferPointers(ds))
        {
            ok = false;
            break;
        }
    }

    if (ok)
        std::printf("[PASS] Buffer descriptor pointers remain valid across growth\n");

    return ok;
}

static bool RunImageLifetimeRegression()
{
    std::printf("\n[Test] Image descriptor pointer lifetime (BindTextureSampler)\n");

    DescriptorSet ds(VK_NULL_HANDLE, 1, VK_NULL_HANDLE, VK_NULL_HANDLE);

    auto *tex = new FakeTexture();
    auto *sampler = new FakeSampler();

    bool ok = true;

    for (int i = 0; i < 96; ++i)
    {
        if (!ds.BindTextureSampler(i, tex, sampler))
        {
            std::printf("[FAIL] BindTextureSampler failed at binding=%d\n", i);
            ok = false;
            break;
        }

        if (!ValidateImagePointers(ds))
        {
            ok = false;
            break;
        }
    }

    if (ok)
        std::printf("[PASS] Image descriptor pointers remain valid across growth\n");

    return ok;
}

int main()
{
    bool ok = true;

    ok = ok && RunBufferLifetimeRegression();
    ok = ok && RunImageLifetimeRegression();

    std::printf("\n=== Descriptor lifecycle regression summary: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

#include<hgl/ecs/systems/render/ColorPaletteSystem.h>
#include<hgl/color/Color.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    ColorPaletteSystem::ColorPaletteSystem(const std::string &name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderPreBeginFrame);

        // 使用 Color.h 的命名颜色表填充默认调色板（不足 256 项时循环填充）。
        const int color_count = int(hgl::COLOR::RANGE_SIZE);
        for (int i = 0; i < graph::ColorPalette::kSize; ++i)
            palette_cpu_.color[i] = hgl::GetABGR(static_cast<hgl::COLOR>(i % color_count));

        // 初始白板需在首次 EnsureResources 后写入 GPU。
        palette_dirty_ = true;
    }

    ColorPaletteSystem::~ColorPaletteSystem()
    {
        if (palette_ubo)
        {
            graph::BufferOwner *buf = palette_ubo->GetBuffer();
            delete palette_ubo;
            palette_ubo = nullptr;

            if (palette_ubo_managed && buf)
            {
                graph::BufferManager *buffer_manager = nullptr;
                if (render_context)
                {
                    if (auto *gc = render_context->GetGraphicsContext())
                        buffer_manager = gc->GetBufferManager();
                }
                if (!buffer_manager && context)
                {
                    if (auto *gc = context->GetGraphicsContext())
                        buffer_manager = gc->GetBufferManager();
                }

                if (buffer_manager)
                    buffer_manager->Release(buf);
            }
            palette_ubo_managed = false;
        }
    }

    void ColorPaletteSystem::SetColor(int index, const hgl::Color4f &color)
    {
        if (index < 0 || index >= graph::ColorPalette::kSize)
        {
            GLogWarning(u8"[ColorPaletteSystem] SetColor: index %d out of range", index);
            return;
        }

        palette_cpu_.color[index] = color.toABGR8();
        palette_dirty_ = true;

        // 资源就绪后立即提交（与 LineRenderPipeline 原行为一致）。
        if (palette_ubo)
            Flush();
    }

    void ColorPaletteSystem::ResetToWhite()
    {
        const uint32 white = hgl::Color4f(1.0f, 1.0f, 1.0f, 1.0f).toABGR8();
        for (int i = 0; i < graph::ColorPalette::kSize; ++i)
            palette_cpu_.color[i] = white;

        palette_dirty_ = true;
        if (palette_ubo)
            Flush();
    }

    void ColorPaletteSystem::Flush()
    {
        if (!palette_dirty_ || !palette_ubo)
            return;

        const bool ok = palette_ubo->Write(palette_cpu_.color, 0, uint32_t(sizeof(graph::ColorPalette)));
        GLogInfo(u8"[ColorPaletteSystem] Flush: Write result=%d size=%zu bytes",
                 ok ? 1 : 0, sizeof(graph::ColorPalette));
        palette_dirty_ = false;
    }

    void ColorPaletteSystem::Initialize()
    {
        EnsureResources();
        Flush();
    }

    void ColorPaletteSystem::Update(float /*deltaTime*/)
    {
        EnsureResources();
        Flush();
    }

    void ColorPaletteSystem::EnsureResources()
    {
        if (palette_ubo)
            return;

        if (!render_context && context)
            render_context = context->GetRenderContext();

        auto *gc = context ? context->GetGraphicsContext() : nullptr;
        if (!gc && render_context)
            gc = render_context->GetGraphicsContext();

        if (!gc)
            return;

        auto *bm = gc->GetBufferManager();
        if (!bm)
            return;

        // BDA：buffer 必须以 SHADER_DEVICE_ADDRESS usage 创建（CreateSSBO 全带，
        // 内存分配 flag 由 usage 派生）——shader 侧无条件解引用该地址，
        // 地址取不到即 fail-fast。
        auto *buf = bm->CreateSSBO("ColorPaletteSSBO", UBOColorPalette::GetSize());
        if (!buf)
            return;

        buf->SetUpdateClass(graph::BufferUpdateClass::Default);

        palette_ubo = UBOColorPalette::Create(buf, false);
        if (!palette_ubo)
            return;

        palette_ubo_managed = true;

        // 地址注册进全局地址表：调色板属于「长期有效」的数据，**不进 pc_root**
        //（后者只承载每 pass / 每帧变化的地址）。buffer 全程不 realloc ⇒ 地址稳定。
        graph::VulkanDevice *device = bm->GetDevice();
        if (!device)
            return;

        const uint64_t palette_addr =
            device->GetBufferDeviceAddressAligned16(buf->GetBuffer());
        if (palette_addr == 0)
        {
            GLogError(u8"[ColorPaletteSystem] ColorPaletteSSBO 取不到设备地址（usage/16B 对齐不满足）");
            return;
        }

        if (auto *registry = gc->GetGlobalSSBOBufferRegistry())
            registry->UpdateColorPaletteAddress(palette_addr);

        GLogInfo(u8"[ColorPaletteSystem] ColorPaletteSSBO 就绪: addr=0x%llx size=%zu",
                 static_cast<unsigned long long>(palette_addr),
                 sizeof(graph::ColorPalette));
    }
}//namespace hgl::ecs

#pragma once

/**
 * GraphicsContext - 图形系统统一接口
 *
 * Phase 2: 用来替代旧的 RenderFramework 过度集中化设计
 *
 * 设计原则：
 * - 单一职责：只提供图形资源访问接口
 * - 依赖注入：所有模块都通过构造函数注入
 * - 非法人模式：ECSContext 持有真实实现，GraphicsContext 是接口
 */

#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKRenderPass.h>
#include <hgl/vk/StructuredBufferAccessor.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/mtl/ShaderBufferSource.h>

namespace hgl::graph
{
    // Forward declarations
    class Material;
    class MaterialInstance;
    class Texture;
    class Texture2D;
    class TextureCube;
    class Texture2DArray;
    class Sampler;
    class GeometryCreater;
    class Pipeline;
    class VertexDataManager;
    class Geometry;
    class Primitive;
    class TextRender;
    class FontSource;
    class MaterialManager;
    class BufferManager;
    class TextureManager;
    class GeometryManager;
    class PrimitiveManager;
    class SamplerManager;
    class RenderPassManager;
    class RenderTargetManager;
    enum IndexType;
    enum class InlinePipeline;

    namespace mtl
    {
        class MaterialCreateInfo;
        struct Material2DCreateConfig;
        struct Material3DCreateConfig;
    }

    class VILConfig;

    /**
     * GraphicsContext - 图形资源统一访问接口
     *
     * 这个接口将 RenderFramework 的职责分解为专门的模块，
     * 每个模块都有明确的责任。
     *
     * 使用方式：
     * ```cpp
     * auto* graphics = ecs_context->GetGraphicsContext();
     * auto* material = graphics->CreateMaterial(...);
     * auto* buffer = graphics->CreateUBO(...);
     * ```
     */
    class IGraphicsContext
    {
    public:

        virtual ~IGraphicsContext()=default;

        // Device access
        virtual class VulkanDevice *GetDevice() const=0;
        virtual class VulkanDevAttr *GetDevAttr() const=0;
        virtual class VulkanPhyDevice *GetPhyDevice() const=0;
        virtual VkDevice                     GetVkDevice() const=0;

        // Module managers
        virtual RenderPassManager *GetRenderPassManager()=0;
        virtual TextureManager *GetTextureManager()=0;
        virtual MaterialManager *GetMaterialManager()=0;
        virtual BufferManager *GetBufferManager()=0;
        virtual SamplerManager *GetSamplerManager()=0;
        virtual GeometryManager *GetGeometryManager()=0;
        virtual PrimitiveManager *GetPrimitiveManager()=0;

    }; // class IGraphicsContext

} // namespace hgl::graph

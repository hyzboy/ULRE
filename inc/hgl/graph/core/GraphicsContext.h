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

        // Material
        virtual Material *CreateMaterial(const AnsiString &name)=0;
        virtual Material *CreateMaterial(const AnsiString &name,const mtl::MaterialCreateInfo *mci)=0;
        virtual Material *CreateMaterial(const mtl::MaterialCreateInfo *mci)=0;

        virtual Material *LoadMaterial(const OSString &path)=0;
        virtual Material *LoadMaterial(const AnsiString &name)=0;
        virtual Material *LoadMaterial(const AnsiString &name,mtl::Material2DCreateConfig *cfg)=0;
        virtual Material *LoadMaterial(const AnsiString &name,mtl::Material3DCreateConfig *cfg)=0;

        virtual MaterialInstance *CreateMaterialInstance(Material *mat)=0;
        virtual MaterialInstance *CreateMaterialInstance(Material *mat,const VIL *vil)=0;
        virtual MaterialInstance *CreateMaterialInstance(Material *mat,const VILConfig *vil_cfg)=0;
        virtual MaterialInstance *CreateMaterialInstance(Material *mat,const VIL *vil,const void *data,const uint32 data_size)=0;
        virtual MaterialInstance *CreateMaterialInstance(Material *mat,const VILConfig *vil_cfg,const void *data,const uint32 data_size)=0;

        virtual MaterialInstance *CreateMaterialInstance(const AnsiString &name,const mtl::MaterialCreateInfo *mci,const VILConfig *vil_cfg)=0;
        virtual MaterialInstance *CreateMaterialInstance(const AnsiString &name,const mtl::MaterialCreateInfo *mci,const VILConfig *vil_cfg,const void *data,const uint32 data_size)=0;
        virtual MaterialInstance *CreateMaterialInstance(const AnsiString &name,mtl::Material2DCreateConfig *cfg,const VILConfig *vil_cfg,const void *data,const uint32 data_size)=0;
        virtual MaterialInstance *CreateMaterialInstance(const AnsiString &name,mtl::Material3DCreateConfig *cfg,const VILConfig *vil_cfg,const void *data,const uint32 data_size)=0;

        // Buffer
        virtual class VKBuffer *CreateVAB(const void *data=nullptr,VkDeviceSize size=0)=0;
        virtual class DeviceBuffer *CreateUBO(VkDeviceSize size)=0;
        virtual class DeviceBuffer *CreateSSBO(VkDeviceSize size)=0;
        virtual class DeviceBuffer *CreateINBO(VkDeviceSize size)=0;

        virtual class DeviceBuffer *CreateUBO(const AnsiString &name,VkDeviceSize size)=0;
        virtual class DeviceBuffer *CreateSSBO(const AnsiString &name,VkDeviceSize size)=0;
        virtual class DeviceBuffer *CreateINBO(const AnsiString &name,VkDeviceSize size)=0;

        template<typename T>
        StructuredBufferAccessor<T>* CreateUBOAccessor(const AnsiString& name,
                                                       const ShaderBufferDesc* desc,
                                                       BufferUpdateClass update_class = BufferUpdateClass::Default)
        {
            auto* buffer_manager = GetBufferManager();
            if (!buffer_manager)
                return nullptr;

            DeviceBuffer* buf = buffer_manager->CreateUBO(name, StructuredBufferAccessor<T>::GetSize());
            if (!buf)
                return nullptr;

            buf->SetUpdateClass(update_class);
            return StructuredBufferAccessor<T>::Create(buf, desc, false);
        }


        virtual class IndexBuffer *CreateIBO(const void *indices,uint32_t count)=0;
        virtual class IndexBuffer *CreateIBO8(const void *indices,uint32_t count)=0;
        virtual class IndexBuffer *CreateIBO16(const void *indices,uint32_t count)=0;
        virtual class IndexBuffer *CreateIBO32(const void *indices,uint32_t count)=0;
        virtual class IndexBuffer *CreateIBO(VkDeviceSize size,IndexType type)=0;

        // Geometry & Primitive
        virtual VertexDataManager *CreateVDM(const VIL *vil,VkDeviceSize vertices,VkDeviceSize indices=0,IndexType type=IndexType::U16)=0;
        virtual SharedPtr<GeometryCreater> GetGeometryCreater(Material *mat)=0;
        virtual SharedPtr<GeometryCreater> GetGeometryCreater(MaterialInstance *mi)=0;

        virtual Geometry *CreateGeometry(const AnsiString &name,uint32_t vertex_count,const VIL *vil,
                 const std::initializer_list<class VertexAttribDataPtr> &vad_list)=0;
        virtual Primitive *CreatePrimitive(const AnsiString &name,uint32_t vertex_count,MaterialInstance *mi,
                                           Pipeline *p,const std::initializer_list<class VertexAttribDataPtr> &vad_list)=0;
        virtual Primitive *CreatePrimitive(Geometry *geo,MaterialInstance *mi,Pipeline *p)=0;
        virtual Primitive *CreatePrimitive(GeometryCreater *pc,MaterialInstance *mi,Pipeline *p)=0;

        // Pipeline
        virtual Pipeline *CreatePipeline(Material *mat,const InlinePipeline &ip)=0;
        virtual Pipeline *CreatePipeline(MaterialInstance *mi,const InlinePipeline &ip)=0;
        virtual RenderPass *GetDefaultRenderPass()=0;

        // Texture
        virtual Texture2D *LoadTexture2D(const OSString &filename,bool auto_mipmap=true)=0;
        virtual TextureCube *LoadTextureCube(const OSString &filename,bool auto_mipmap=false)=0;
        virtual Texture2DArray *CreateTexture2DArray(const AnsiString &name,uint32_t width,uint32_t height,
                                                     uint32_t layer,VkFormat fmt,bool auto_mipmap=false)=0;
        virtual bool                    LoadTexture2DArray(Texture2DArray *tex,uint32_t layer,const OSString &filename)=0;

        // Sampler
        virtual Sampler *CreateSampler(VkSamplerCreateInfo *sci=nullptr)=0;
        virtual Sampler *CreateSampler(Texture *tex)=0;

        // Text rendering
        virtual TextRender *CreateTextRender(FontSource *fs,int limit=1024)=0;

        // Geometry Management
        virtual void                    Add(Geometry *geometry)=0;

        template<typename T>
        MaterialInstance *CreateMaterialInstance(Material *mat,const VIL *vil,const T *data)
        {
            return CreateMaterialInstance(mat, vil, data, sizeof(T));
        }

        template<typename T>
        MaterialInstance *CreateMaterialInstance(Material *mat,const VILConfig *vil_cfg,const T *data)
        {
            return CreateMaterialInstance(mat, vil_cfg, data, sizeof(T));
        }

    }; // class IGraphicsContext

} // namespace hgl::graph

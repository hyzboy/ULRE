#pragma once

#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/type/String.h>
#include <cstdint>

namespace hgl
{
    class VulkanDevice;

    namespace io
    {
        class InputStream;
    }//namespace io
}

namespace hgl::graph
{
    class DeviceBuffer;
    class GpuUploadQueue;

    Geometry *LoadGeometry(VulkanDevice *device,
                           const GeometryVertexFormat &geometry_vertex_format,
                           const OSString &filename);

    /**
     * @brief 从**已打开的输入流**装载几何（流化路径 / 包裹化资产的统一入口）。
     *
     * 与物理路径版本解析逻辑完全一致，只是数据来源从「内部打开的文件」换成
     * 调用方给的流 —— 因而可以直接喂资产包内的 `OpenAssetStream`，无需先落盘
     * 或拼出物理路径。``MiniPackReader`` 只在这一层存在，解析代码对来源无感。
     *
     * @param stream     已定位到 minipack 首字节的输入流，**必须 `new` 出来**
     * @param debug_name 仅用于日志与几何名，可以是逻辑路径而非真实文件名
     * @return 成功返回新几何（已 `Add` 由调用方负责），失败返回 `nullptr`
     *
     * @note 调用即移交流的**所有权**（成功由 minipack 读取器释放，失败就地释放），
     *       调用方在返回后无论成败都不得再 delete、读取或复用该流。
     */
    Geometry *LoadGeometryFromStream(VulkanDevice *device,
                                     const GeometryVertexFormat &geometry_vertex_format,
                                     io::InputStream *stream,
                                     const OSString &debug_name);

    Geometry *LoadGeometryFromMiniPackBytes(VulkanDevice *device,
                                            const GeometryVertexFormat &geometry_vertex_format,
                                            const void *bytes,
                                            uint32 size,
                                            const OSString &debug_name);

    /**
     * 异步几何装载完成回调。
     * geometry 非空且 success 为 true 时几何已可直接绑定渲染；所有权移交调用方。
     * success 为 false 时 geometry 为 nullptr（内部已回滚半成品的几何与缓冲）。
     */
    using GeometryLoadComplete = void (*)(Geometry *geometry, bool success, void *user_data);

    /**
     * @brief Direct-to-Staging 异步几何装载（GpuUploadQueue BufferToBuffer DMA 路径）。
     *
     * staging 缓冲所有权**移交**本函数：无论同步回退还是异步完成，最终都由内部
     * Unmap + delete（上传任务以 owns_staging=false 挂接，避免双重释放）。
     *
     * 全部顶点属性格式与 geometry_vertex_format 精确一致、且索引为 U32（或无索引）时，
     * 创建纯 device-local 的 GeometryData（BufferAllocPolicy::GPUDeviceLocal），
     * 由上传队列把各 VAB/IBO 从 staging DMA 填充；任一属性需要格式转换（如
     * Normal float3→RG8/Half2、TexCoord→Half2、U8/U16 索引展开）则回退到
     * 同步解析 LoadGeometryFromMiniPackBytes。两条路径 on_complete 都**恰好回调一次**
     * （回退路径在返回前同步回调）。
     *
     * @param queue       上传队列；为空则直接走同步回退
     * @param staging     已 Map 的 TRANSFER_SRC staging 缓冲（host 可见），bytes 指向其映射内存
     * @param bytes,size  staging 内的 minipack 字节（调用方从 Map 得到）
     */
    void LoadGeometryFromStagingAsync(VulkanDevice *device,
                                      const GeometryVertexFormat &geometry_vertex_format,
                                      GpuUploadQueue *queue,
                                      DeviceBuffer *staging,
                                      const void *bytes,
                                      uint32 size,
                                      const OSString &debug_name,
                                      GeometryLoadComplete on_complete,
                                      void *user_data);
}

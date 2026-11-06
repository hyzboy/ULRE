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
}

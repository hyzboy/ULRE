#pragma once

#include <hgl/graph/asset/GeometryAssetSpec.h>

namespace hgl::graph
{
    class GeometryCreater;
    /**
     * @brief 从 TOML 文件加载几何体资产规约
     *
     * 结构示例：
     * ```toml
     * schema = 1
     * name = "Sphere64"
     * type = "builtin"
     * shape = "sphere"
     *
     * [builtin.sphere]
     * slices = 64
     * ```
     */
    bool LoadGeometryAssetSpecFromTOML(const OSString &filename, GeometryAssetSpec &out_spec);

    /**
     * @brief 从内存字符串解析几何体资产规约
     */
    bool ParseGeometryAssetSpec(const char *content, size_t size, const os_char *debug_name, GeometryAssetSpec &out_spec);

    /**
     * @brief 把规约里的内置形状参数**创建进调用方给的** `GeometryCreater`。
     *
     * 与 `CreateGeometryFromSpec(device, bm, ...)` 的形状分发完全同源，区别只在
     * 「谁拥有 VDM/缓冲池」：
     *
     * - 本重载用于**多个几何共享同一个 VertexDataManager** 的场合（示例里的合并批次、
     *   MixedBufferMeshes 等）。此时必须由调用方持有 creater，否则每个几何会各建一套 VDM，
     *   既浪费又破坏「混合缓冲」的前提。
     * - 文件来源（`type = "file"`）无法在外部 creater 里建，会返回 `nullptr` 并留下 error 日志；
     *   这种情况请用 device 重载或异步流送。
     */
    Geometry *CreateGeometryFromSpec(GeometryCreater *creater,
                                     const GeometryAssetSpec &spec);

    /**
     * @brief 读 `*.geo.toml` 并直接创建进给定 creater（示例/工具的一行式入口）。
     *
     * 顶点布局由 **creater 决定**，因此本入口忽略资产里的 `[vertex_format]` —— 这一点
     * 与共享 VDM 的设计一致：一批几何必须同布局。
     */
    Geometry *CreateGeometryFromAssetFile(GeometryCreater *creater,
                                          const OSString &toml_filename);

}//namespace hgl::graph

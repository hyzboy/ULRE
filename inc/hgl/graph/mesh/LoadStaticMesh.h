#pragma once

#include <hgl/type/String.h>

namespace hgl::graph
{
    class StaticMesh;
    class VulkanDevice;
    class GeometryManager;
    class MaterialInstance;
    class VIL;

    // Forward-declared in VKPipeline.h
    class Pipeline;

    /**
     * LoadStaticMeshScene - 从 .scene minipack 文件加载场景树到 StaticMesh
     *
     * @param device          Vulkan 设备，用于加载 Geometry
     * @param geo_mgr         GeometryManager，加载的 Geometry 注册到此（接管生命周期）
     * @param vil             顶点输入布局，必须与 default_mi 的材质匹配
     * @param default_mi      默认材质实例，暂时对所有 primitive 使用同一材质
     * @param default_pipeline 默认管线，暂时对所有 primitive 使用同一管线
     * @param pack_path       .scene minipack 文件的完整路径
     * @param base_dir        .geometry 文件所在目录（通常就是 pack 文件所在目录）
     *
     * @return 加载成功返回堆上的 StaticMesh*，失败返回 nullptr
     *         调用者负责 delete（StaticMesh 内部持有 Primitive* 会在析构时释放，
     *         Geometry* 的生命周期由 geo_mgr 管理）
     */
    StaticMesh *LoadStaticMeshScene(
        VulkanDevice        *device,
        GeometryManager     *geo_mgr,
        const VIL           *vil,
        MaterialInstance    *default_mi,
        Pipeline            *default_pipeline,
        const OSString      &pack_path,
        const OSString      &base_dir);

}//namespace hgl::graph

#pragma once

#include <hgl/asset/LoadedFileHandler.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/mesh/StaticMesh.h>
#include <hgl/type/String.h>
#include <vector>

namespace hgl
{
    class LoadedFileManager;
}

namespace hgl::graph
{
    class VulkanDevice;
    class GeometryManager;
    class GeometryVertexFormat;
    enum class SceneStreamingState : uint8
    {
        Idle = 0,       ///<尚未发起
        Loading,        ///<后台读盘在途
        Ready,          ///<字节已就绪，可 `Build()`
        Failed,         ///<读盘失败（不自动重试）
        Built,          ///<已解析成资产/节点（结果被取走）
    };

    /**
     * 异步场景加载请求 —— 把 `.scene` 包的**读盘**搬到后台 IO 线程。
     *
     * 为什么只搬读盘：`.scene` 里的几何体会在解析期建 GPU buffer，那必须留在主线程。
     * 但读盘本身（动辄几十 MB 的包）不该卡住主线程，因此拆成两段：
     *
     * ```
     * Begin(lfm, path)    → 提交读盘（状态 Loading）
     * 引擎每帧 Pump()     → OnFileLoaded 把字节拷进请求自己持有的缓冲（状态 Ready）
     * Build(...)          → 主线程解析 + 建 GPU 资源（状态 Built）
     * ```
     *
     * 「引擎每帧 Pump()」不需要额外驱动：`TextureStreamingSystem` /
     * `GeometryStreamingSystem` 已经在 `RenderPreBeginFrame` 调 `Pump()`，
     * 它派发的是**所有**完成的请求，本请求自然被覆盖。
     *
     * 生命周期：本对象必须比它的读盘请求活得久（`LoadedFileManager` 只持裸指针）。
     * 典型用法是把它作为 App 的成员，`Build()` 之后也保留到退出。
     */
    class SceneStreamingRequest final : public hgl::LoadedFileHandler
    {
    private:

        hgl::LoadedFileManager *load_manager = nullptr;

        OSString        pack_path;
        FileLoadId      request_id = 0;
        SceneStreamingState state = SceneStreamingState::Idle;

        /// 读盘完成后由回调填入（自有缓冲：`LoadedFile` 只在回调期间有效）。
        void *          pack_bytes = nullptr;
        uint32          pack_size  = 0;

    private:

        void ReleaseBytes();

    public:

        SceneStreamingRequest()=default;
        ~SceneStreamingRequest() override;

        SceneStreamingRequest(const SceneStreamingRequest &)=delete;
        SceneStreamingRequest &operator=(const SceneStreamingRequest &)=delete;

    public:

        /// 发起异步读取（重复调用同一次加载是幂等的）
        bool Begin(hgl::LoadedFileManager *lfm,const OSString &path);

        /// 取消在途请求（状态回到 `Idle`，缓存的字节被释放）
        void Cancel();

        /**
         * @brief 灌入调用方已经读到的字节（`pack(...)` URI 的兜底路径）。
         *
         * `LoadedFileManager::Submit` 只接受**物理路径**；包裹内路径
         * （`pack(res):xxx/yyy`）只能由上层经 `OpenAssetInputStream` 取流。
         * 取到的字节交给这里后状态直接置 `Ready`，后续 `Build()` 与异步路径
         * 完全一致 —— 上层不必为两种来源各写一套解析。
         *
         * 字节会被**拷进自有缓冲**，调用方在返回后即可释放自己的副本。
         */
        bool SetPackBytes(const void *data, uint32 size);

        /// `Ready` 期间的有效字节；`Build()`/`Cancel()` 之后为 nullptr
        const void *GetPackBytes()const { return pack_bytes; }
        uint32      GetPackSize ()const { return pack_size; }

    public:     // 观察

        SceneStreamingState GetState()const{return state;}
        bool IsLoading()const{return state==SceneStreamingState::Loading;}
        bool IsReady()const{return state==SceneStreamingState::Ready;}
        bool HasFailed()const{return state==SceneStreamingState::Failed;}
        bool IsFinished()const{return state==SceneStreamingState::Built;}

        const OSString &GetPackPath()const{return pack_path;}

        /// 在途读盘请求号（0 表示没有在途请求）；用于把回调反查回句柄
        FileLoadId GetRequestID()const{return request_id;}

    public:

        /**
         * @brief 主线程解析已就绪的字节，得到图元资产与场景节点树。
         *
         * 与同步入口 `LoadStaticMeshSceneAsPrimitiveAssets` 走同一段解析代码。
         * 成功返回 true 并置状态为 `Built`（字节随之释放）。
         *
         * @param base_dir 场景包内几何文件的基准目录（通常是包的父目录）
         * @param material_recipes 按源场景材质下标索引的配方表；为空则整场景共用
         *        `recipe`（多材质场景必须传，否则只有第 0 号材质正确）
         */
        bool Build(VulkanDevice *device,
                   GeometryManager *geo_mgr,
                   const GeometryVertexFormat &geometry_vertex_format,
                   const mtl::MaterialRecipe *recipe,
                   const OSString &base_dir,
                   std::vector<PrimitiveAsset> &out_assets,
                   std::vector<StaticMeshNode> &out_nodes,
                   std::vector<int32_t>        &out_root_nodes,
                   const mtl::MaterialRecipe *const *material_recipes = nullptr,
                   const uint32_t material_recipe_count = 0);

    public:     // LoadedFileHandler

        void OnFileLoaded   (const FileLoadTicket &ticket,LoadedFile *file) override;
        void OnFileFailed   (const FileLoadTicket &ticket,io::AsyncFileLoadError error) override;
        void OnFileCancelled(const FileLoadTicket &ticket) override;
    };//class SceneStreamingRequest
}//namespace hgl::graph

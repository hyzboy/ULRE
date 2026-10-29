#include<hgl/graph/mesh/SceneStreaming.h>
#include<hgl/graph/mesh/LoadStaticMesh.h>
#include<hgl/framework/LoadedFileManager.h>
#include<hgl/log/Log.h>
#include<hgl/utf.h>

#include<cstring>

namespace hgl::graph
{
    SceneStreamingRequest::~SceneStreamingRequest()
    {
        Cancel();
    }

    void SceneStreamingRequest::ReleaseBytes()
    {
        if(pack_bytes)
        {
            hgl_free(pack_bytes);
            pack_bytes=nullptr;
        }

        pack_size=0;
    }

    bool SceneStreamingRequest::Begin(hgl::LoadedFileManager *lfm,const OSString &path)
    {
        if(!lfm||path.IsEmpty())
        {
            GLogError(u8"[SceneStreaming] Begin 收到空管理器或空路径");
            return(false);
        }

        // 幂等：同一条路径在途/已就绪时不重复提交。
        if(pack_path==path
         &&(state==SceneStreamingState::Loading
          ||state==SceneStreamingState::Ready
          ||state==SceneStreamingState::Built))
            return(true);

        Cancel();

        load_manager=lfm;
        pack_path   =path;
        request_id  =lfm->Submit(path,this,io::AsyncFilePriority::Normal);

        if(request_id==0)
        {
            GLogError(u8"[SceneStreaming] 提交读盘失败：%s",
                      reinterpret_cast<const char *>(ToU8String(path).c_str()));
            state=SceneStreamingState::Failed;
            return(false);
        }

        state=SceneStreamingState::Loading;

        GLogDebug(u8"[SceneStreaming] 提交读取：%s（request=%llu）",
                  reinterpret_cast<const char *>(ToU8String(path).c_str()),
                  static_cast<unsigned long long>(request_id));

        return(true);
    }

    void SceneStreamingRequest::Cancel()
    {
        if(load_manager&&request_id)
            load_manager->Cancel(request_id);

        request_id=0;
        ReleaseBytes();

        if(state==SceneStreamingState::Loading)
            state=SceneStreamingState::Idle;
    }

    void SceneStreamingRequest::OnFileLoaded(const FileLoadTicket &ticket,LoadedFile *file)
    {
        if(ticket.id!=request_id||!file)
            return;

        request_id=0;

        const uint64 size=file->GetSize();

        if(size==0||size>static_cast<uint64>(UINT32_MAX))
        {
            GLogError(u8"[SceneStreaming] 场景包尺寸非法（%llu 字节）：%s",
                      static_cast<unsigned long long>(size),
                      reinterpret_cast<const char *>(ToU8String(pack_path).c_str()));
            state=SceneStreamingState::Failed;
            return;
        }

        // 拷一份自有副本：`LoadedFile` 只在回调期间有效，而解析发生在之后的 `Build()`。
        // 用 `hgl_malloc`/`hgl_free` 与引擎其余裸缓冲一致（不引入 STL 容器）。
        pack_bytes=hgl_malloc(static_cast<size_t>(size));

        if(!pack_bytes)
        {
            GLogError(u8"[SceneStreaming] 场景包缓冲分配失败（%llu 字节）",
                      static_cast<unsigned long long>(size));
            state=SceneStreamingState::Failed;
            return;
        }

        memcpy(pack_bytes,file->GetData(),static_cast<size_t>(size));
        pack_size=static_cast<uint32>(size);

        state=SceneStreamingState::Ready;

        GLogDebug(u8"[SceneStreaming] 字节就绪：%s（%u 字节）",
                  reinterpret_cast<const char *>(ToU8String(pack_path).c_str()),
                  pack_size);
    }

    void SceneStreamingRequest::OnFileFailed(const FileLoadTicket &ticket,io::AsyncFileLoadError error)
    {
        if(ticket.id!=request_id)
            return;

        request_id=0;

        GLogError(u8"[SceneStreaming] 读盘失败（AsyncFileLoadError=%d）：%s",
                  static_cast<int>(error),
                  reinterpret_cast<const char *>(ToU8String(pack_path).c_str()));

        state=SceneStreamingState::Failed;
    }

    void SceneStreamingRequest::OnFileCancelled(const FileLoadTicket &ticket)
    {
        if(ticket.id!=request_id)
            return;

        request_id=0;
        state=SceneStreamingState::Idle;
    }

    bool SceneStreamingRequest::Build(VulkanDevice *device,
                                      GeometryManager *geo_mgr,
                                      const GeometryVertexFormat &geometry_vertex_format,
                                      const mtl::MaterialRecipe *recipe,
                                      const OSString &base_dir,
                                      std::vector<PrimitiveAsset> &out_assets,
                                      std::vector<StaticMeshNode> &out_nodes,
                                      std::vector<int32_t>        &out_root_nodes)
    {
        if(state!=SceneStreamingState::Ready||!pack_bytes||pack_size==0)
        {
            GLogError(u8"[SceneStreaming] Build 时机不对（当前状态 %d）",
                      static_cast<int>(state));
            return(false);
        }

        const bool ok=LoadStaticMeshSceneFromMemory(device,
                                                    geo_mgr,
                                                    geometry_vertex_format,
                                                    recipe,
                                                    pack_bytes,
                                                    pack_size,
                                                    pack_path,
                                                    base_dir,
                                                    out_assets,
                                                    out_nodes,
                                                    out_root_nodes);

        ReleaseBytes();

        state=ok?SceneStreamingState::Built:SceneStreamingState::Failed;

        return(ok);
    }
}//namespace hgl::graph

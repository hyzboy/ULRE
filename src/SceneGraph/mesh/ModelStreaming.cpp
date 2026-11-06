#include<hgl/graph/mesh/ModelStreaming.h>
#include<hgl/graph/mesh/LoadStaticMesh.h>
#include<hgl/asset/LoadedFileManager.h>
#include<hgl/log/Log.h>
#include<hgl/utf.h>

#include<cstring>

namespace hgl::graph
{
    ModelStreamingRequest::~ModelStreamingRequest()
    {
        Cancel();
    }

    void ModelStreamingRequest::ReleaseBytes()
    {
        if(pack_bytes)
        {
            hgl_free(pack_bytes);
            pack_bytes=nullptr;
        }

        pack_size=0;
    }

    bool ModelStreamingRequest::Begin(hgl::LoadedFileManager *lfm,const OSString &path)
    {
        if(!lfm||path.IsEmpty())
        {
            GLogError(u8"[ModelStreaming] Begin 收到空管理器或空路径");
            return(false);
        }

        // 幂等：同一条路径在途/已就绪时不重复提交。
        if(pack_path==path
         &&(state==ModelStreamingState::Loading
          ||state==ModelStreamingState::Ready
          ||state==ModelStreamingState::Built))
            return(true);

        Cancel();

        load_manager=lfm;
        pack_path   =path;
        request_id  =lfm->Submit(path,this,io::AsyncFilePriority::Normal);

        if(request_id==0)
        {
            GLogError(u8"[ModelStreaming] 提交读盘失败：%s",
                      reinterpret_cast<const char *>(ToU8String(path).c_str()));
            state=ModelStreamingState::Failed;
            return(false);
        }

        state=ModelStreamingState::Loading;

        GLogDebug(u8"[ModelStreaming] 提交读取：%s（request=%llu）",
                  reinterpret_cast<const char *>(ToU8String(path).c_str()),
                  static_cast<unsigned long long>(request_id));

        return(true);
    }

    void ModelStreamingRequest::Cancel()
    {
        if(load_manager&&request_id)
            load_manager->Cancel(request_id);

        request_id=0;
        ReleaseBytes();

        if(state==ModelStreamingState::Loading)
            state=ModelStreamingState::Idle;
    }

    bool ModelStreamingRequest::SetPackBytes(const void *data,uint32 size)
    {
        if(!data||size==0)
        {
            GLogError(u8"[ModelStreaming] SetPackBytes 收到空字节");
            return(false);
        }

        Cancel();

        pack_bytes=hgl_malloc(static_cast<size_t>(size));

        if(!pack_bytes)
        {
            GLogError(u8"[ModelStreaming] 模型包缓冲分配失败（%u 字节）",size);
            state=ModelStreamingState::Failed;
            return(false);
        }

        memcpy(pack_bytes,data,static_cast<size_t>(size));
        pack_size=size;
        state=ModelStreamingState::Ready;

        return(true);
    }

    void ModelStreamingRequest::OnFileLoaded(const FileLoadTicket &ticket,LoadedFile *file)
    {
        if(ticket.id!=request_id||!file)
            return;

        request_id=0;

        const uint64 size=file->GetSize();

        if(size==0||size>static_cast<uint64>(UINT32_MAX))
        {
            GLogError(u8"[ModelStreaming] 模型包尺寸非法（%llu 字节）：%s",
                      static_cast<unsigned long long>(size),
                      reinterpret_cast<const char *>(ToU8String(pack_path).c_str()));
            state=ModelStreamingState::Failed;
            return;
        }

        // 拷一份自有副本：`LoadedFile` 只在回调期间有效，而解析发生在之后的 `Build()`。
        // 用 `hgl_malloc`/`hgl_free` 与引擎其余裸缓冲一致（不引入 STL 容器）。
        pack_bytes=hgl_malloc(static_cast<size_t>(size));

        if(!pack_bytes)
        {
            GLogError(u8"[ModelStreaming] 模型包缓冲分配失败（%llu 字节）",
                      static_cast<unsigned long long>(size));
            state=ModelStreamingState::Failed;
            return;
        }

        memcpy(pack_bytes,file->GetData(),static_cast<size_t>(size));
        pack_size=static_cast<uint32>(size);

        state=ModelStreamingState::Ready;

        GLogDebug(u8"[ModelStreaming] 字节就绪：%s（%u 字节）",
                  reinterpret_cast<const char *>(ToU8String(pack_path).c_str()),
                  pack_size);
    }

    void ModelStreamingRequest::OnFileFailed(const FileLoadTicket &ticket,io::AsyncFileLoadError error)
    {
        if(ticket.id!=request_id)
            return;

        request_id=0;

        GLogError(u8"[ModelStreaming] 读盘失败（AsyncFileLoadError=%d）：%s",
                  static_cast<int>(error),
                  reinterpret_cast<const char *>(ToU8String(pack_path).c_str()));

        state=ModelStreamingState::Failed;
    }

    void ModelStreamingRequest::OnFileCancelled(const FileLoadTicket &ticket)
    {
        if(ticket.id!=request_id)
            return;

        request_id=0;
        state=ModelStreamingState::Idle;
    }

    bool ModelStreamingRequest::Build(VulkanDevice *device,
                                      GeometryManager *geo_mgr,
                                      const GeometryVertexFormat &geometry_vertex_format,
                                      const mtl::MaterialRecipe *recipe,
                                      const OSString &base_dir,
                                      std::vector<PrimitiveAsset> &out_assets,
                                      std::vector<StaticMeshNode> &out_nodes,
                                      std::vector<int32_t>        &out_root_nodes,
                                      const mtl::MaterialRecipe *const *material_recipes,
                                      const uint32_t material_recipe_count)
    {
        if(state!=ModelStreamingState::Ready||!pack_bytes||pack_size==0)
        {
            GLogError(u8"[ModelStreaming] Build 时机不对（当前状态 %d）",
                      static_cast<int>(state));
            return(false);
        }

        const bool ok=LoadModelFromMemory(device,
                                                    geo_mgr,
                                                    geometry_vertex_format,
                                                    recipe,
                                                    pack_bytes,
                                                    pack_size,
                                                    pack_path,
                                                    base_dir,
                                                    out_assets,
                                                    out_nodes,
                                                    out_root_nodes,
                                                    material_recipes,
                                                    material_recipe_count);

        ReleaseBytes();

        state=ok?ModelStreamingState::Built:ModelStreamingState::Failed;

        return(ok);
    }
}//namespace hgl::graph

#include<hgl/graph/asset/GeometryAsset.h>
#include<hgl/graph/asset/GeometryAssetRegistry.h>
#include<hgl/graph/asset/GeometryAssetFile.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/log/Log.h>
#include<hgl/utf.h>

namespace hgl::graph
{
    GeometryAsset::GeometryAsset(GeometryAssetRegistry *reg,VulkanDevice *dev,BufferManager *bm)
        :registry(reg)
        ,device(dev)
        ,buffer_manager(bm)
    {
        // 句柄由注册器的表拥有（裸指针 + 显式 delete），不走引用计数销毁路径。
        // 保留 KeepMetaUnloadPayload：即便有人误用 RefPtr，归零也只会卸载负载。
        SetRefZeroPolicy(RefZeroPolicy::KeepMetaUnloadPayload);
    }

    GeometryAsset::~GeometryAsset()
    {
        // 直接 delete 绕过 payload 状态机时也要把几何体带走，否则必然泄漏。
        if(geometry)
        {
            delete geometry;
            geometry=nullptr;
        }
    }

    bool GeometryAsset::OnInitMeta()
    {
        return(true);
    }

    void GeometryAsset::OnReleaseMeta()
    {
        geometry        =nullptr;
        async_requested =false;
        async_request_id=0;
    }

    bool GeometryAsset::OnLoadPayload()
    {
        if(geometry)
            return(true);

        // 内置几何体：纯内存计算，没有可等待的 IO，直接建。
        if(spec.kind==GeometryAssetKind::Builtin)
        {
            Geometry *geo=CreateGeometryFromSpec(device,
                                                 buffer_manager,
                                                 spec,
                                                 &vertex_format);

            if(!geo)
            {
                GLogError(u8"[GeometryAsset] 内置几何体创建失败（%s）",
                          reinterpret_cast<const char *>(GetResourceId().c_str()));
                payload_state=PayloadState::LoadFailed;
                return(false);
            }

            return CommitAsyncGeometry(geo);
        }

        // 外部文件：同步兜底只给工具/测试用，正常路径是流送系统读盘后提交字节。
        Geometry *geo=CreateGeometryFromSpec(device,
                                             buffer_manager,
                                             spec,
                                             &vertex_format);

        if(!geo)
        {
            GLogError(u8"[GeometryAsset] 几何文件加载失败（%s）",
                      reinterpret_cast<const char *>(GetResourceId().c_str()));
            payload_state=PayloadState::LoadFailed;
            return(false);
        }

        return CommitAsyncGeometry(geo);
    }

    void GeometryAsset::OnUnloadPayload()
    {
        // 几何体归本句柄所有（见头文件说明），因此这里必须真正销毁。
        if(geometry)
        {
            delete geometry;
            geometry=nullptr;
        }

        async_requested =false;
        async_request_id=0;
    }

    bool GeometryAsset::SetupFromSpec(const GeometryAssetSpec &new_spec,
                                     const GeometryVertexFormat *format_override)
    {
        if(!device)
        {
            GLogError(u8"[GeometryAsset] SetupFromSpec 时未绑定设备");
            return(false);
        }

        spec = new_spec;

        vertex_format = format_override
            ? *format_override
            : GetStandardGeometryVertexFormat(spec.normal_format);

        if(vertex_format.GetCount()==0)
        {
            GLogError(u8"[GeometryAsset] SetupFromSpec 得到空的顶点布局");
            return(false);
        }

        if(spec.kind==GeometryAssetKind::File)
        {
            if(spec.file_path.IsEmpty())
            {
                GLogError(u8"[GeometryAsset] 文件几何体缺少路径");
                return(false);
            }

            meta.source_path=spec.file_path;
            meta.asset_type =OS_TEXT("GeometryFile");

            if(!Init())
                return(false);

            // 停在 MetaOnly：等 GeometryStreamingSystem 提交读盘。
            return(true);
        }

        meta.asset_type=OS_TEXT("BuiltinGeometry");

        if(!Init())
            return(false);

        // 内置几何体没有磁盘负载 ⇒ 创建即 Resident，同一调用返回即可画。
        return OnLoadPayload();
    }

    bool GeometryAsset::SetupFromMiniPackFile(const OSString &filename,
                                              const GeometryVertexFormat &format)
    {
        if(filename.IsEmpty())
        {
            GLogError(u8"[GeometryAsset] SetupFromMiniPackFile 收到空文件名");
            return(false);
        }

        GeometryAssetSpec file_spec;

        file_spec.kind      =GeometryAssetKind::File;
        file_spec.file_path =filename;
        file_spec.name      =AnsiString(reinterpret_cast<const char *>(ToU8String(filename).c_str()));

        return SetupFromSpec(file_spec,&format);
    }

    void GeometryAsset::MarkAsyncRequested(const uint64 request_id)
    {
        async_requested =true;
        async_request_id=request_id;

        if(payload_state==PayloadState::MetaOnly
         ||payload_state==PayloadState::LoadFailed)
            payload_state=PayloadState::Loading;
    }

    void GeometryAsset::ClearAsyncRequest()
    {
        async_requested =false;
        async_request_id=0;

        if(payload_state==PayloadState::Loading)
            payload_state=PayloadState::MetaOnly;
    }

    bool GeometryAsset::CommitAsyncGeometry(Geometry *geo)
    {
        if(!geo)
        {
            FailAsyncLoad("CommitAsyncGeometry 收到空几何体");
            return(false);
        }

        if(geometry)
        {
            // 同一句柄被提交两次（读盘与同步兜底撞车）：保留先到的那一份，丢弃后到的。
            GLogWarning(u8"[GeometryAsset] 重复提交几何体，忽略后到的（%s）",
                        reinterpret_cast<const char *>(GetResourceId().c_str()));
            delete geo;
            return(true);
        }

        geometry        =geo;
        async_requested =false;
        async_request_id=0;
        payload_state   =PayloadState::Resident;

        return(true);
    }

    void GeometryAsset::FailAsyncLoad(const char *reason)
    {
        GLogError(u8"[GeometryAsset] 加载失败：%s（%s）",
                  reinterpret_cast<const char *>(GetResourceId().c_str()),
                  reason?reason:"未指明原因");

        geometry        =nullptr;
        async_requested =false;
        async_request_id=0;
        payload_state   =PayloadState::LoadFailed;
    }

    U8String GeometryAsset::GetResourceId()const
    {
        if(spec.kind==GeometryAssetKind::File)
        {
            if(spec.file_path.IsEmpty())
                return U8String();

            U8String id(u8"geofile:");

            id+=ToU8String(spec.file_path);
            return id;
        }

        U8String id(u8"geo:");

        if(!spec.name.IsEmpty())
        {
            id+=U8String(u8"@");
            id+=ToU8String(AnsiString(spec.name));
            return id;
        }

        // 无名内置几何体：退回形状名兜底。同一份 `GeometryAssetSpec` 的两次请求
        // 本来就该各自独立建几何，因此这里只要求「同一个句柄内稳定」。
        id+=ToU8String(AnsiString(GetBuiltinShapeKindName(spec.shape)));
        return id;
    }
}//namespace hgl::graph

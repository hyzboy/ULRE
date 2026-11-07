#include<hgl/graph/asset/PackReadTask.h>

#include<hgl/graph/asset/AssetSourceMount.h>
#include<hgl/io/InputStream.h>
#include<hgl/platform/Platform.h>

namespace hgl::graph
{
    PackReadTask::PackReadTask(const OSString &uri,const asset::AssetType type)
        : AssetCPUTask(asset::AssetCPUTaskKind::Custom)
        , uri_text(uri)
        , default_type(type)
    {
        // 取流是阻塞 IO，不占 GPU 上下文，任意 CPU 工作线程都行；
        // 成品是一段字节，主线程拿去做解析，因此结果落在主线程。
        affinity        = asset::ExecutionAffinity::WorkerAny;
        result_affinity = asset::ExecutionAffinity::MainThread;
    }

    PackReadTask::~PackReadTask()
    {
        if(data)
            hgl_free(data);
    }

    bool PackReadTask::Execute()
    {
        if(data)
        {
            hgl_free(data);
            data = nullptr;
            data_size = 0;
        }

        AnsiString open_error;

        io::InputStream *stream_in = OpenAssetInputStream(uri_text,default_type,&open_error);

        if(stream_in == nullptr)
        {
            error = open_error.IsEmpty() ? AnsiString("无法打开资产流") : open_error;
            return false;
        }

        const int64 size = stream_in->GetSize();

        // 上限沿用 `SetPackBytes(const void *,uint32)` 的口径：超过 4GiB 的
        // 单份资产在任何消费端都会溢出，早失败比晚溢出好定位。
        if(size <= 0 || size > static_cast<int64>(UINT32_MAX))
        {
            delete stream_in;

            error = AnsiString("资产流尺寸非法（") + AnsiString::numberOf(size) + ")";
            return false;
        }

        data = static_cast<uint8 *>(hgl_malloc(static_cast<size_t>(size)));

        if(data == nullptr)
        {
            delete stream_in;

            error = AnsiString("资产流缓冲分配失败（") + AnsiString::numberOf(size) + " 字节)";
            return false;
        }

        if(stream_in->Read(data,size) != size)
        {
            delete stream_in;

            hgl_free(data);
            data = nullptr;

            error = AnsiString("资产流读取不完整（") + AnsiString::numberOf(size) + " 字节)";
            return false;
        }

        delete stream_in;

        data_size = size;

        return true;
    }
}//namespace hgl::graph

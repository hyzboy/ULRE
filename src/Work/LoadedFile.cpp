#include<hgl/framework/LoadedFileManager.h>

namespace hgl
{
    LoadedFile::LoadedFile(const OSString &filename)
        :request(filename)
    {
        meta.source_path=filename;
    }

    bool LoadedFile::OnLoadPayload()
    {
        // 字节已由读盘线程写入 request；空文件(0 字节)同样视为成功
        return(meta.source_size==0||request.GetData()!=nullptr);
    }

    void LoadedFile::OnUnloadPayload()
    {
        request.ReleaseData();
    }

    void LoadedFile::OnRefZero()
    {
        if(owner)
            owner->OnFileRefZero(this);         // 只入队，真正的 Release/delete 由 Collect() 完成
        else
        {
            Release();                          // 管理器已不存在：按 AutoDestroy 的老规矩自行了断
            delete this;
        }
    }
}//namespace hgl
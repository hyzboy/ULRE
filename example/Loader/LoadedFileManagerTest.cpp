// LoadedFileManager 单元测试
//
// 覆盖两段式异步文件加载的完整语义：
//   ① 读取成功 → Pump 回调 OnFileLoaded，字节/META 正确；
//   ② 文件不存在 → OnFileFailed；
//   ③ 协作式取消 → OnFileCancelled；
//   ④ 引用归零 → 进待释放队列，Collect() 才真正释放；
//   ⑤ 待释放期间被 Find() 命中 → 复活，Collect() 放过它；
//   ⑥ 命中缓存 → 不重复读盘(cache_hits 增加)；
//   ⑦ Pump(max_dispatch) 限量分发；
//   ⑧ Stop(Drain) 后仍能 Pump 取走已完成结果；
//   ⑨ Stop(CancelPending) 之后每个请求都恰好回调一次，不丢不重。
//
// 返回值：0 表示全部通过，非 0 为失败阶段编号。

#include<hgl/framework/LoadedFileManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/io/FileOutputStream.h>
#include<hgl/time/Time.h>

using namespace hgl;

namespace
{
    // 16 字节固定负载：既好比对，也不依赖任何引擎资产格式
    constexpr uint8 kPayload[16]={'H','G','L','_','L','O','A','D','E','D','_','F','I','L','E','!'};

    const OSString kGoodFile   =OS_TEXT("LoadedFileManagerTest.bin");
    const OSString kMissingFile=OS_TEXT("LoadedFileManagerTest.missing.bin");
    const OSString kBigFile    =OS_TEXT("res/image/PeachWine.Tex2D");

    /**
     * 记录回调结果的 handler。
     *
     * 三个回调都只在 Pump() 所在线程触发，因此内部计数无需原子化。
     */
    class RecordHandler:public LoadedFileHandler
    {
    public:

        int     loaded   =0;                ///<OnFileLoaded 次数
        int     failed   =0;                ///<OnFileFailed 次数
        int     cancelled=0;                ///<OnFileCancelled 次数

        LoadedFilePtr keep;                 ///<需要跨阶段保留文件时挂在这里

        OSString                last_filename;
        uint64                  last_size=0;
        bool                    last_bytes_ok=false;
        io::AsyncFileLoadError  last_error=io::AsyncFileLoadError::None;

        int GetTotal()const{return loaded+failed+cancelled;}

        void OnFileLoaded(const FileLoadTicket &,LoadedFile *file)override
        {
            ++loaded;

            last_filename=file?file->GetFilename():OSString();
            last_size=file?file->GetSize():0;
            last_bytes_ok=false;

            if(file&&file->GetSize()==sizeof(kPayload))
            {
                const uint8 *data=file->GetData();

                if(data)
                {
                    last_bytes_ok=true;

                    for(uint i=0;i<sizeof(kPayload);i++)
                        if(data[i]!=kPayload[i])
                        {
                            last_bytes_ok=false;
                            break;
                        }
                }
            }
        }

        void OnFileFailed(const FileLoadTicket &,io::AsyncFileLoadError error)override
        {
            ++failed;
            last_error=error;
        }

        void OnFileCancelled(const FileLoadTicket &)override
        {
            ++cancelled;
        }
    };//class RecordHandler

    /**
     * 反复 Pump() 直到 handler 累计回调达到 expected 次或超时。
     */
    bool WaitDispatch(LoadedFileManager &manager,RecordHandler &handler,int expected,double timeout_seconds=5.0)
    {
        const double deadline=GetUptimeSec()+timeout_seconds;

        while(true)
        {
            manager.Pump();

            if(handler.GetTotal()>=expected)
                return(true);

            if(GetUptimeSec()>=deadline)
                return(false);

            SleepSecond(0.002);
        }
    }

    bool WritePayloadFile(const OSString &filename)
    {
        io::OpenFileOutputStream output(filename,io::FileOpenMode::CreateTrunc);

        if(!output)
            return(false);

        return(output->Write(kPayload,sizeof(kPayload))==(int)sizeof(kPayload));
    }
}//namespace

int main()
{
    // ---------------------------------------------------------------- 准备
    if(!WritePayloadFile(kGoodFile))
    {
        filesystem::FileDelete(kGoodFile);
        return(1);
    }

    LoadedFileManager manager;
    RecordHandler handler;

    int result=0;

    if(!manager.Start(2))
        result=2;

    // ---------------------------------------------------------------- ① 读取成功
    FileLoadId first_id=0;

    if(result==0)
    {
        first_id=manager.Submit(kGoodFile,&handler);

        if(first_id==0)
            result=3;
    }

    if(result==0&&!WaitDispatch(manager,handler,1))
        result=4;

    if(result==0&&(handler.loaded!=1||handler.failed!=0||handler.cancelled!=0))
        result=5;

    if(result==0&&(!handler.last_bytes_ok||!(handler.last_filename==kGoodFile)))
        result=6;

    // 回调结束后 handler 已放手，文件应停留在待释放队列里
    if(result==0)
    {
        const LoadedFileManagerStatistics stats=manager.GetStatistics();

        if(stats.pending_release!=1||stats.alive_files!=1||stats.submitted!=1)
            result=7;
    }

    // ---------------------------------------------------------------- ⑤ 复活
    LoadedFilePtr revived;

    if(result==0)
    {
        revived=manager.Find(kGoodFile);

        if(!revived||revived->GetSize()!=sizeof(kPayload))
            result=8;
    }

    // 引用仍在 ⇒ Collect 不得释放
    if(result==0&&manager.Collect()!=0)
        result=9;

    // ---------------------------------------------------------------- ⑥ 缓存命中
    RecordHandler cache_handler;

    if(result==0)
    {
        if(manager.Submit(kGoodFile,&cache_handler,io::AsyncFilePriority::High)==0)
            result=10;
    }

    if(result==0&&!WaitDispatch(manager,cache_handler,1))
        result=11;

    if(result==0&&(cache_handler.loaded!=1||!cache_handler.last_bytes_ok))
        result=12;

    if(result==0)
    {
        const LoadedFileManagerStatistics stats=manager.GetStatistics();

        if(stats.cache_hits!=1)
            result=13;
    }

    // ---------------------------------------------------------------- ② 文件不存在
    RecordHandler missing_handler;

    if(result==0&&manager.Submit(kMissingFile,&missing_handler)==0)
        result=14;

    if(result==0&&!WaitDispatch(manager,missing_handler,1))
        result=15;

    if(result==0&&(missing_handler.failed!=1||missing_handler.loaded!=0))
        result=16;

    if(result==0&&missing_handler.last_error==io::AsyncFileLoadError::None)
        result=17;

    // ---------------------------------------------------------------- ③ 取消
    RecordHandler cancel_handler;
    FileLoadId cancel_id=0;

    if(result==0)
    {
        cancel_id=manager.Submit(kGoodFile,&cancel_handler);

        if(cancel_id==0||!manager.Cancel(cancel_id))
            result=18;
    }

    if(result==0&&!WaitDispatch(manager,cancel_handler,1))
        result=19;

    if(result==0&&(cancel_handler.cancelled!=1||cancel_handler.loaded!=0))
        result=20;

    // 已回调的编号不能再取消
    if(result==0&&manager.Cancel(cancel_id))
        result=21;

    // ---------------------------------------------------------------- ⑦ Pump 限量
    RecordHandler bulk_handler;

    if(result==0)
        for(int i=0;i<3;i++)
            if(manager.Submit(kGoodFile,&bulk_handler)==0)
                result=22;

    if(result==0)
    {
        if(manager.Pump(1)!=1)
            result=23;
        else
        if(!WaitDispatch(manager,bulk_handler,3))
            result=24;
        else
        if(bulk_handler.loaded!=3)
            result=25;
    }

    // ---------------------------------------------------------------- ④ 延迟释放
    if(result==0)
    {
        manager.Collect();                              // 先清掉"文件不存在"那次失败请求遗留的待释放项

        handler.keep=manager.Find(kGoodFile);           // 重新持有引用

        if(!handler.keep)
            result=26;
    }

    if(result==0&&manager.Collect()!=0)                 // 有人持有 ⇒ 不释放
        result=27;

    if(result==0)
    {
        handler.keep.Reset();                           // 放手
        revived.Reset();
    }

    if(result==0&&manager.Collect()!=1)                 // 无人持有 ⇒ 释放
        result=28;

    if(result==0)
    {
        const LoadedFileManagerStatistics stats=manager.GetStatistics();

        if(stats.alive_files!=0||stats.pending_release!=0||stats.released_files<1)
            result=29;
    }

    // ---------------------------------------------------------------- ⑧ Stop(Drain)
    if(result==0&&!manager.Stop(thread::ShutdownMode::Drain))
        result=30;

    if(result==0&&manager.Pump()!=0)
        result=31;

    if(result==0)
    {
        LoadedFilePtr gone=manager.Find(kGoodFile);      // 已释放：表里不应再有它

        if(gone)
            result=36;
    }

    // ---------------------------------------------------------------- ⑨ Stop(CancelPending)
    {
        LoadedFileManager pending_manager;
        RecordHandler pending_handler;

        if(result==0)
        {
            if(!pending_manager.Start(1))
                result=32;
            else
            if(pending_manager.Submit(kBigFile,&pending_handler,io::AsyncFilePriority::Low)==0)
                result=33;
            else
            if(!pending_manager.Stop(thread::ShutdownMode::CancelPending))
                result=34;
            else
            {
                pending_manager.Pump(16);               // 取出已完成/已取消的结果

                // 无论读完了还是被取消，都必须恰好回调一次
                if(pending_handler.GetTotal()!=1)
                    result=35;
            }
        }
    }

    filesystem::FileDelete(kGoodFile);
    return(result);
}
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
//   ⑨ Stop(CancelPending) 之后每个请求都恰好回调一次，不丢不重；
//   ⑩ 在途去重：同帧同文件多次 Submit（含大小写/分隔符变体）只读一次盘；
//   ⑪ 单 IO worker 下按提交顺序串行完成并回调。
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

    // 在途去重用：同一文件的三种子路径写法
    const OSString kDedupDir    =OS_TEXT("LoadedFileManagerTestDir");
    const OSString kDedupFile   =OS_TEXT("LoadedFileManagerTestDir/sub.bin");
    const OSString kDedupFileRepeat =OS_TEXT("LoadedFileManagerTestDir//sub.bin");

#if HGL_OS == HGL_OS_Windows
    const OSString kDedupFileVariant=OS_TEXT("LOADEDFILEMANAGERTESTDIR\\SUB.BIN");   // 大小写 + 分隔符变体
#else
    const OSString kDedupFileVariant=OS_TEXT("LoadedFileManagerTestDir///sub.bin");  // 仅分隔符重复变体
#endif

    // 串行顺序用：三个不同的文件，按提交顺序回调
    const OSString kOrderFile0=OS_TEXT("LoadedFileManagerTestDir/order0.bin");
    const OSString kOrderFile1=OS_TEXT("LoadedFileManagerTestDir/order1.bin");
    const OSString kOrderFile2=OS_TEXT("LoadedFileManagerTestDir/order2.bin");

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

        OSString order_files[3];                            ///<按回调顺序记录的最近三个文件名
        int      order_count=0;
        bool     record_order=false;

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

            if(record_order)
            {
                if(order_count<3)
                    order_files[order_count]=last_filename;

                ++order_count;
            }

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

    if(!manager.Start())      // 默认单线程串行读盘
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

    // ---------------------------------------------------------------- ⑩ 在途去重
    // 同一路径在读盘完成前的多次 Submit（含大小写/分隔符变体）只读一次盘，
    // 每个请求各自收到一次 OnFileLoaded，共享同一份字节。
    if(result==0)
    {
        LoadedFileManager dedup_manager;
        RecordHandler dedup_a;
        RecordHandler dedup_b;
        RecordHandler dedup_c;

        if(!filesystem::MakePath(kDedupDir)||!WritePayloadFile(kDedupFile))
            result=37;
        else
        if(!dedup_manager.Start())                      // 默认单线程串行读盘
            result=38;
        else
        if(dedup_manager.Submit(kDedupFile,&dedup_a)==0)
            result=39;
        else
        if(dedup_manager.Submit(kDedupFileRepeat,&dedup_b)==0)
            result=40;
        else
        if(dedup_manager.Submit(kDedupFileVariant,&dedup_c)==0)
            result=41;
        else
        if(dedup_manager.GetStatistics().merged_in_flight!=2)
            result=42;
        else
        if(!WaitDispatch(dedup_manager,dedup_a,1))
            result=43;
        else
        if(dedup_a.loaded!=1||dedup_b.loaded!=1||dedup_c.loaded!=1
         ||dedup_a.failed!=0||dedup_b.failed!=0||dedup_c.failed!=0
         ||!dedup_a.last_bytes_ok||!dedup_b.last_bytes_ok||!dedup_c.last_bytes_ok)
            result=44;
        else
        if(dedup_manager.GetStatistics().io.submitted!=1)   // 底层只读了一次
            result=45;
        else
        if(dedup_a.last_filename!=kDedupFile             // 报告的是首次 Submit 的原始拼写
         ||dedup_b.last_filename!=kDedupFile
         ||dedup_c.last_filename!=kDedupFile)
            result=46;
    }

    // ---------------------------------------------------------------- ⑪ 单线程串行顺序
    // 单 IO worker 下，先后 Submit 的三个不同文件应按提交顺序完成并回调。
    if(result==0)
    {
        LoadedFileManager order_manager;
        RecordHandler order_handler;

        order_handler.record_order=true;

        if(!filesystem::MakePath(kDedupDir)
         ||!WritePayloadFile(kOrderFile0)||!WritePayloadFile(kOrderFile1)||!WritePayloadFile(kOrderFile2))
            result=47;
        else
        if(!order_manager.Start(1))
            result=48;
        else
        if(order_manager.Submit(kOrderFile0,&order_handler)==0
         ||order_manager.Submit(kOrderFile1,&order_handler)==0
         ||order_manager.Submit(kOrderFile2,&order_handler)==0)
            result=49;
        else
        if(!WaitDispatch(order_manager,order_handler,3))
            result=50;
        else
        if(order_manager.GetStatistics().merged_in_flight!=0)   // 不同文件不应被合并
            result=51;
        else
        if(order_handler.order_count!=3
         ||order_handler.order_files[0]!=kOrderFile0
         ||order_handler.order_files[1]!=kOrderFile1
         ||order_handler.order_files[2]!=kOrderFile2)
            result=52;
    }

    filesystem::FileDelete(kOrderFile0);
    filesystem::FileDelete(kOrderFile1);
    filesystem::FileDelete(kOrderFile2);
    filesystem::FileDelete(kDedupFile);
    filesystem::FileDelete(kGoodFile);
    return(result);
}
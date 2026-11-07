#pragma once

/**
 * 包裹取流的 CPU 阶段任务（Tier 2）。
 *
 * ## 为什么需要它
 *
 * `ResolveAssetPhysicalPath()` 能把目录型包裹的 URI 折成物理路径，于是那条路可以走
 * `LoadedFileManager` 的异步读盘。但**索引里查不到物理路径**的情形一直存在：
 *
 *   · `pack(res):id:<32 hex>` —— 身份寻址，磁盘上没有以 ID 命名的文件；
 *   · 归档型包裹（单文件 `.pack`）—— 根本没有逐文件的物理路径；
 *   · `builtin` / `rt` 这类运行时来源 —— 不存在于文件系统。
 *
 * 历史实现在这些情形下由主线程直接 `OpenAssetInputStream()` + `Read()` 整包同步读，
 * 大模型一帧几十毫秒的卡顿就是这么来的。
 *
 * 本任务把「取流 + 读全」搬到 CPU 工作线程：主线程只负责创建任务与提交，
 * 字节到手后由 Tier 3 用 `GetData()` 消费。
 *
 * ## 所有权
 *
 * 缓冲归任务自己（析构释放）。消费方若需要长期持有字节，必须自己拷贝 ——
 * 这与 `ModelStreamingRequest::SetPackBytes()` 的「拷进自有缓冲」语义正好吻合，
 * 因此消费完即可 `delete` 任务。
 */

#include<hgl/asset/AssetCPUTask.h>
#include<hgl/asset/AssetURI.h>
#include<hgl/type/String.h>

namespace hgl::graph
{
    /**
     * 用 `OpenAssetInputStream()` 读取一份无法折成物理路径的资产，产出整段字节。
     *
     * `Execute()` 在工作线程运行，内部是阻塞式同步读 —— 这正是它被移出主线程的原因。
     */
    class PackReadTask final : public asset::AssetCPUTask
    {
        NO_COPY_NO_MOVE(PackReadTask)

    public:

        /**
         * @param uri          资产 URI（`pack(res):id:...` / 归档内路径 / 裸路径均可）
         * @param default_type URI 省略类型段时补上的类型
         */
        PackReadTask(const OSString &uri,asset::AssetType default_type);
        ~PackReadTask() override;

        bool Execute() override;

        /// 读到的字节（未执行或失败时为 nullptr）
        const void *GetData()const{return data;}

        /// 字节数
        int64 GetDataSize()const{return data_size;}

    private:

        OSString uri_text;
        asset::AssetType default_type;

        uint8 *data = nullptr;
        int64 data_size = 0;
    };//class PackReadTask
}//namespace hgl::graph

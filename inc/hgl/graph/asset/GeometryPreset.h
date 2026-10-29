#pragma once

#include <hgl/graph/asset/GeometryAssetSpec.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/String.h>

namespace hgl::graph
{
    class GeometryCreater;

    /**
     * 几何形状**预设库** —— 把「内置形状 + 参数」从 C++ 里搬进 TOML 的入口。
     *
     * 与 `GeometryAssetRegistry` 的分工：
     *
     * | | GeometryAssetRegistry | GeometryPresetLibrary |
     * |---|---|---|
     * | 谁拥有 GPU 资源 | 句柄（每资产一套 VDM/缓冲） | **调用方的** `GeometryCreater` |
     * | 适合 | 独立资产、异步流送、图元资产引用 | 一批几何共享同一个 VDM 的场合（示例批次、混合缓冲） |
     *
     * 为什么需要它：示例里 `CreateCube(pc, &cci)` 这类调用把形状参数硬编码在 C++ 里，
     * 而且必须由调用方保证「同一批几何同布局」。本库只负责**把名字翻译成形状参数**，
     * GPU 资源归属不变 ⇒ 迁移是纯粹的数据外置，不改变渲染架构。
     *
     * 规约：`<root>/<name>.geo.toml`，与独立的几何资产同一套 schema（`schema = 1` 等）。
     * `root` 默认 `res/geo/presets/`，可用 `SetRoot()` 改写。
     *
     * 解析结果按名字缓存（示例启动期只读一次盘）；文件不存在/解析失败会留下 error 日志
     * 并返回 nullptr —— 不静默退化，否则 TOML 拼错会变成「画面上少一个物体」这种难查现象。
     */
    class GeometryPresetLibrary
    {
    public:

        struct Preset
        {
            AnsiString        name;
            GeometryAssetSpec spec;
            bool              failed = false;   ///<已尝试加载且失败(不重复刷日志)
        };

    private:

        OSString                    root;
        hgl::ManagedArray<Preset>   presets;

    private:

        const Preset * FindOrLoad(const char *name);

    public:

        GeometryPresetLibrary();
        ~GeometryPresetLibrary()=default;

        GeometryPresetLibrary(const GeometryPresetLibrary &)=delete;
        GeometryPresetLibrary &operator=(const GeometryPresetLibrary &)=delete;

    public:

        /// 同一进程内共享的实例（预设是只读数据，跨 App/世界复用无副作用）
        static GeometryPresetLibrary &Instance();

        /// 预设根目录（默认 `res/geo/presets/`）；切换会清空缓存
        void SetRoot(const OSString &new_root);
        const OSString &GetRoot()const{return root;}

        /// 按名字取规约（不存在返回 nullptr）
        const GeometryAssetSpec * Get(const char *name);

        /// 按名字创建几何进给定 creater（`type = "file"` 的预设不支持，返回 nullptr）
        Geometry * Create(GeometryCreater *creater,const char *name);

        /// 丢弃全部缓存（热重载/测试用）
        void Clear();
    };//class GeometryPresetLibrary

    /// 便捷入口：把「预设名」直接建成几何（等价于 `GeometryPresetLibrary::Instance().Create`）
    inline Geometry *CreateGeometryPreset(GeometryCreater *creater,const char *name)
    {
        return GeometryPresetLibrary::Instance().Create(creater,name);
    }
}//namespace hgl::graph

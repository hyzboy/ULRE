#pragma once
// ModelLoadPolicy.h —— 模型自动装配的拦截/优先级策略接口（落地计划 A.1）
//
// 组件（`ModelRefComponent`）只描述「我要这个模型」；什么时候允许请求、
// 每段产物到位时世界侧要做什么，由策略决定。典型需求：
//   · 「某些资产必须先显示」⇒ `OnBeforeRequest` 返回高优先级，
//     `OnSkeletonReady` 里立刻做相机对焦/占位；
//   · 关卡批量装配 ⇒ 全部回调 no-op（默认基类即可）。
//
// 线程契约：所有回调都在 `ModelStreamingSystem::Update`（主线程）内被调用，
// 实现里不得做阻塞 IO。
//
// 生命期：策略对象由挂组件的一方持有（不拥有⇒系统只借指针）；
// 组件销毁或实体先于策略析构时，宿主必须先把组件的 `policy` 置空。

#include <hgl/type/String.h>

namespace hgl::graph
{
    class ModelAsset;
}

namespace hgl::ecs
{
    struct ModelRefComponent;

    /**
     * @brief 模型装配策略基类：全部虚函数默认 no-op，按需覆写。
     */
    class ModelLoadPolicy
    {
    public:

        virtual ~ModelLoadPolicy() = default;

        /**
         * @brief 系统准备把 uri 投给句柄表之前询问（可拦截/排序）。
         *
         * 返回值即该请求在流送队列里的提交优先级：数值越大越先派发。
         * 返回负数 ⇒ 本帧暂不请求（下帧再问），用于宿主自己控制节奏。
         */
        virtual int OnBeforeRequest(const ModelRefComponent & /*component*/,
                                    const AnsiString & /*uri*/)
        {
            return 0;
        }

        /// 骨架段完成（空壳实体 + Transform 级联已入世界）
        virtual void OnSkeletonReady(ModelRefComponent & /*component*/) {}

        /// 几何段完成（本帧有新挂上的 GeometryData）
        virtual void OnGeometryAttached(ModelRefComponent & /*component*/) {}

        /// 材质段完成（本帧有新挂上的 MaterialData）
        virtual void OnMaterialAttached(ModelRefComponent & /*component*/) {}

        /// 全段落定且句柄 `CommitModel` 就绪 —— 模型可用（只报一次）
        virtual void OnModelReady(ModelRefComponent & /*component*/) {}

        /// 装配失败（句柄掉 `Failed` 或请求无法成立；只报一次）
        virtual void OnModelFailed(ModelRefComponent & /*component*/,
                                   const char * /*reason*/) {}
    };//class ModelLoadPolicy
}//namespace hgl::ecs

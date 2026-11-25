#pragma once

#include<hgl/ecs/support/material/MaterialRuntimeTable.h>
#include<hgl/type/ValueArray.h>

namespace hgl::ecs
{
    /**
     * 收集系统的物化世代与"本帧已刷新的模板纹理行"记账（收集系统私有）。
     *
     * 三件事必须同步推进，所以合成一个对象：
     * - **物化世代**：一帧里只要真的重建了全局物化表就 +1；`last_materialize_epoch`
     *   与之不等的实例必须重新物化（它们的行是被跳过期间重建的）。
     * - **本帧已刷新的模板纹理行**：模板驱动的材质行可能被多个实体共享，
     *   同一帧内只物化一次纹理配置（否则同 hash 反复 retire/重建）。
     * - **环境层 ambient provider 快照**：provider 由世界决定、不属材质授权态，
     *   名字热切后 A3 快路径仍会命中旧 program ⇒ 这里用"哈希变了就说变了"兜底，
     *   调用方据此把全体 slot 的 program 置脏。每帧代价 = 一次哈希比较。
     */
    class MaterializationEpochState
    {
    private:
        uint64_t epoch = 0;
        ValueArray<MaterialRuntimeRowID> refreshed_template_textures;

        uint64_t ambient_provider_hash = 0;
        bool ambient_provider_valid = false;

    public:
        uint64_t Epoch() const { return epoch; }
        void BumpEpoch() { ++epoch; }

        /// 每帧开始：清掉上一帧"已刷新的模板纹理行"
        void BeginFrame() { refreshed_template_textures.Clear(); }

        bool IsTemplateTextureRefreshed(MaterialRuntimeRowID id) const
        {
            return refreshed_template_textures.Contains(id);
        }

        void MarkTemplateTextureRefreshed(MaterialRuntimeRowID id)
        {
            refreshed_template_textures.Add(id);
        }

        /// 推进 ambient provider 快照；返回 true = provider 变了（首次视为变化）
        bool UpdateAmbientProvider(uint64_t hash)
        {
            const bool changed = !ambient_provider_valid || ambient_provider_hash != hash;

            if (changed)
            {
                ambient_provider_hash = hash;
                ambient_provider_valid = true;
            }

            return changed;
        }
    };
}
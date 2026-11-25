#include <hgl/ecs/support/assembly/ModelAssemblyStage.h>

#include <hgl/ecs/core/Context.h>
#include <hgl/graph/asset/ModelAsset.h>
#include <hgl/log/Log.h>

namespace hgl::ecs::assembly
{
    const char *GetModelAssemblyStageName(ModelAssemblyStage stage)
    {
        switch (stage)
        {
            case ModelAssemblyStage::Request:  return "Request";
            case ModelAssemblyStage::Skeleton: return "Skeleton";
            case ModelAssemblyStage::Geometry: return "Geometry";
            case ModelAssemblyStage::Material: return "Material";
            case ModelAssemblyStage::Settled:  break;
        }

        return "Settled";
    }

    ModelAssemblyStage GetModelAssemblyStage(const ModelRefComponent &ref)
    {
        if (ref.IsSettled())
            return ModelAssemblyStage::Settled;

        if (!ref.skeleton_done)
            return ModelAssemblyStage::Request;

        if (!ref.geometry_done)
            return ModelAssemblyStage::Skeleton;

        if (!ref.material_done)
            return ModelAssemblyStage::Geometry;

        return ModelAssemblyStage::Material;
    }

    void FailModelAssembly(ModelRefComponent &ref, const char *reason)
    {
        GLogError(u8"ModelRef 装配失败：%s", reason);

        ref.failed_reported = true;
        ref.asset = nullptr;        ///< 句柄归 registry；组件不再观察它

        if (ref.policy)
            ref.policy->OnModelFailed(ref, reason);
    }

    bool AdvanceModelAssembly(ECSContext &context,
                              ModelRefComponent &ref,
                              graph::ModelAssetRegistry *registry,
                              const MaterialRowsProvider &provider)
    {
        if (ref.IsSettled())
            return false;

        graph::ModelAsset *asset = ref.asset;

        if (asset && (!registry || !registry->GetAssets().Contains(asset)))
        {
            FailModelAssembly(ref, "句柄已被 registry 回收（Clear?）");
            return true;
        }

        // ── 请求段：先问策略，再投句柄表 ────────────────────────────────
        if (!asset)
        {
            if (ref.request_started || ref.uri.IsEmpty())
                return false;   ///< 请求曾失败，或调用方自己管句柄：停在原地

            const int priority = ref.policy
                               ? ref.policy->OnBeforeRequest(ref, ref.uri)
                               : 0;
            if (priority < 0)
                return false;   ///< 策略压下了：下帧再问（宿主自己控制节奏）

            ref.request_started = true;
            asset = ref.asset = graph::RequestModelAsset(ref.uri);
            if (!asset)
            {
                FailModelAssembly(ref, "句柄表未受理请求（默认表未登记？）");
                return true;
            }
        }

        // ── 句柄失败 ⇒ 终态（registry 不自动重试，这里同样不重试）──────
        if (asset->HasFailed())
        {
            FailModelAssembly(ref, asset->GetFailReason().c_str());
            return true;
        }

        const graph::ModelAssetContent &content = asset->GetContent();

        // ── 段一：骨架（节点树到位即建，一次）──────────────────────────
        if (!ref.skeleton_done)
        {
            if (content.GetNodeCount() <= 0)
                return false;   ///< 解析产物还没出来

            if (!ref.instance.InstantiateSkeleton(&context, content.nodes,
                                                  content.root_nodes,
                                                  ref.skeleton_options))
            {
                FailModelAssembly(ref, "InstantiateSkeleton 失败");
                return true;
            }

            ref.skeleton_done = true;
            if (!ref.skeleton_reported)
            {
                ref.skeleton_reported = true;
                if (ref.policy)
                    ref.policy->OnSkeletonReady(ref);
            }

            return true;        ///< 每帧至多推进一段（与示例原行为一致）
        }

        // ── 段二：几何（增量挂接，句柄 Ready 时收口）───────────────────
        if (!ref.geometry_done)
        {
            if (content.GetAssetCount() > 0 &&
                ref.instance.AttachGeometries(content.assets, content.nodes) &&
                !ref.geometry_reported)
            {
                ref.geometry_reported = true;
                if (ref.policy)
                    ref.policy->OnGeometryAttached(ref);
            }

            if (!asset->IsReady())
                return false;   ///< 子项仍在途：下帧接着挂

            ref.geometry_done = true;
        }

        // ── 段三：材质（几何收口后一次）────────────────────────────────
        if (!ref.material_done)
        {
            // 调用方没给行绑定 ⇒ 取宿主注入的来源（流送车道自装配的结果）。
            // 绑定只在 attach 时被读值，无需长期持有。
            if (!ref.options.material_rows && provider.resolve)
            {
                const graph::GlobalSSBOBinding *rows = nullptr;
                uint32_t row_count = 0;

                if (provider.resolve(provider.user, asset, rows, row_count))
                {
                    ref.options.material_rows          = rows;
                    ref.options.material_row_count     = row_count;
                    ref.options.rows_by_material_index = true;
                }
            }

            if (!ref.instance.AttachMaterials(ref.options))
            {
                FailModelAssembly(ref, "AttachMaterials 失败（无已挂几何可补材质？）");
                return true;
            }

            ref.material_done = true;
            if (!ref.material_reported)
            {
                ref.material_reported = true;
                if (ref.policy)
                    ref.policy->OnMaterialAttached(ref);
            }
        }

        // ── 就绪上报：句柄 Ready 且三段落定（只报一次）─────────────────
        if (!ref.ready_reported && asset->IsReady())
        {
            ref.ready_reported = true;
            if (ref.policy)
                ref.policy->OnModelReady(ref);
        }

        return true;
    }
}//namespace hgl::ecs::assembly

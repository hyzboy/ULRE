#include<hgl/ecs/systems/render/MaterialResolveSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialRecipeRegistry.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/mtl/MaterialRecipeStore.h>
#include<hgl/time/Time.h>
#include<hgl/vk/VKVertexInputLayout.h>
#include<hgl/log/Log.h>
#include "internal/MaterialResolveInternals.h"
#include "internal/MaterialResolvePlanBuilder.h"
#include "internal/MaterialResolveBucketResolver.h"
#include "internal/MaterialResolveApplyStage.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace hgl::ecs
{
	namespace
	{
		using internal::PrototypeKey;
		using internal::PrototypeKeyHash;
		using internal::ApplyStageStats;
		using internal::ResolvedMIBucket;
		using internal::ResolveTask;
	}

	MaterialResolveSystem::MaterialResolveSystem(const std::string &name)
		: System(name)
	{
		SetSystemType(SystemType::ShaderMaterialProgram);
		SetExecutionOrder(ExecutionPhase::RenderMaterialBind);
		SetRenderElementType("Primitive");
	}

	void MaterialResolveSystem::Update(float /*deltaTime*/)
	{
		if (!world)
			return;

		auto *gc = world->GetGraphicsContext();
		if (!gc)
			return;

		auto *registry = gc->GetMaterialAssetRegistry();
		auto *prim_mgr = gc->GetPrimitiveManager();
		auto *recipe_store = gc->GetRecipeStore();
		if (!registry || !prim_mgr)
			return;

		std::vector<std::shared_ptr<PrimitiveComponent>> primitives;
		world->GetComponents<PrimitiveComponent>(primitives);

		std::vector<ResolveTask> tasks;
		std::unordered_map<PrototypeKey, std::vector<size_t>, PrototypeKeyHash> prototype_buckets;
		std::vector<ResolvedMIBucket> resolved_buckets;

		uint32_t resolve_input_count = 0;
		uint32_t resolved_count = 0;
		uint32_t resolve_fail_count = 0;
		uint32_t primitive_created_count = 0;
		uint32_t primitive_updated_count = 0;
		uint32_t primitive_recreated_count = 0;
		uint32_t mi_bucket_count = 0;
        ApplyStageStats apply_stats{};

		internal::BuildResolvePlan(primitives,
			recipe_store,
			tasks,
			prototype_buckets,
			resolve_input_count);

		internal::ResolveMIBuckets(tasks,
			prototype_buckets,
			registry,
			resolved_buckets,
			mi_bucket_count,
			resolve_fail_count);

        internal::ApplyResolvedMIBuckets(tasks,
            resolved_buckets,
            prim_mgr,
            apply_stats,
            resolve_fail_count);

        resolved_count = apply_stats.resolved_count;
        primitive_created_count = apply_stats.primitive_created_count;
        primitive_updated_count = apply_stats.primitive_updated_count;
        primitive_recreated_count = apply_stats.primitive_recreated_count;

		if (resolve_input_count > 0)
		{
			LogDebug("[ECS::MaterialResolveSystem] Stage2 summary: inputs=%u prototype_buckets=%zu mi_buckets=%u resolved=%u fail=%u created=%u updated=%u recreated=%u",
				resolve_input_count,
				prototype_buckets.size(),
				mi_bucket_count,
				resolved_count,
				resolve_fail_count,
				primitive_created_count,
				primitive_updated_count,
				primitive_recreated_count);
		}

		if (decoupled_cache_enabled)
		{
			const uint64_t now_ms = hgl::GetTimeMs();
			if (now_ms - last_cache_stats_log_ms >= cache_stats_log_interval_ms)
			{
				last_cache_stats_log_ms = now_ms;
				const auto stats = gc->GetMaterialResolveTieredCacheStats();

				LogInfo("[ECS::MaterialResolveSystem][R1.2] tiered_cache_stats: "
				        "program(req=%llu hit=%llu miss=%llu create=%llu) "
				        "payload(req=%llu hit=%llu miss=%llu create=%llu) "
				        "binding(req=%llu hit=%llu miss=%llu create=%llu)",
				        static_cast<unsigned long long>(stats.program_requests),
				        static_cast<unsigned long long>(stats.program_hits),
				        static_cast<unsigned long long>(stats.program_misses),
				        static_cast<unsigned long long>(stats.program_creates),
				        static_cast<unsigned long long>(stats.payload_requests),
				        static_cast<unsigned long long>(stats.payload_hits),
				        static_cast<unsigned long long>(stats.payload_misses),
				        static_cast<unsigned long long>(stats.payload_creates),
				        static_cast<unsigned long long>(stats.binding_requests),
				        static_cast<unsigned long long>(stats.binding_hits),
				        static_cast<unsigned long long>(stats.binding_misses),
				        static_cast<unsigned long long>(stats.binding_creates));
			}
		}
	}
}//namespace hgl::ecs

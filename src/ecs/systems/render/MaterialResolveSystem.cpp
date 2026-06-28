#include<hgl/ecs/systems/render/MaterialResolveSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialResolveTieredCache.h>
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

		static graph::ProgramCacheKey BuildProgramCacheKey(const ResolveTask &task)
		{
			graph::ProgramCacheKey key{};
			key.material_key = task.material_key;
			key.gvf_hash = task.gvf_hash;
			key.feature_mask = 0;
			key.rule_version = 0;
			key.capability_mask = 0;
			return key;
		}

		static graph::PayloadCacheKey BuildPayloadCacheKey(const ResolveTask &task)
		{
			graph::PayloadCacheKey key{};
			key.recipe_id = static_cast<uint32_t>(task.slot ? task.slot->recipe_id : hgl::graph::mtl::kInvalidMaterialRecipeID);
			key.instance_data_hash = task.instance_hash;
			key.domain_id = task.slot ? task.slot->resolved_domain_id : 0xFFFFFFFFu;
			key.runtime_texture_generation = task.comp ? task.comp->GetRuntimeTextureBindingGeneration() : 0;
			return key;
		}

		static graph::BindingCacheKey BuildBindingCacheKey(const ResolveTask &task,
		                                                  const graph::MaterialInstancePayload *payload)
		{
			graph::BindingCacheKey key{};
			key.program = task.slot ? task.slot->resolved_material : nullptr;
			key.payload_id = payload ? payload->id : 0;
			key.domain_id = task.slot ? task.slot->resolved_domain_id : 0xFFFFFFFFu;
			key.layout_signature = 0;
			return key;
		}
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

		if (decoupled_cache_enabled)
		{
			auto &tiered_cache = gc->GetMaterialResolveTieredCache();

			for (auto &task : tasks)
			{
				if (!task.slot)
					continue;

				++r21_dry_run_stats.tasks_seen;

				const auto program_key = BuildProgramCacheKey(task);
				auto *cached_program = tiered_cache.FindProgram(program_key);
				if (cached_program)
					++r21_dry_run_stats.candidate_program_hits;

				auto *legacy_program = task.slot->resolved_material;
				auto *legacy_domain = task.slot->resolved_domain;
				const uint32_t legacy_domain_id = task.slot->resolved_domain_id;
				const uint64_t legacy_instance_hash = task.instance_hash;
				if (!cached_program && task.slot->resolved_material)
				{
					tiered_cache.UpsertProgram(program_key, task.slot->resolved_material, true);
					cached_program = task.slot->resolved_material;
				}

				if (cached_program && cached_program == legacy_program)
					++r21_dry_run_stats.program_match_with_legacy;

				const auto payload_key = BuildPayloadCacheKey(task);
				auto *cached_payload = tiered_cache.FindPayload(payload_key);
				if (cached_payload)
					++r21_dry_run_stats.candidate_payload_hits;
				if (!cached_payload)
				{
					auto payload_it = shadow_payload_index.find(payload_key);
					if (payload_it != shadow_payload_index.end())
					{
						cached_payload = payload_it->second;
					}
					else
					{
						auto &payload = shadow_payload_storage.emplace_back();
						payload.id = next_shadow_payload_id++;
						payload.schema_version = 0;
						payload.domain_id = payload_key.domain_id;
						payload.instance_hash = payload_key.instance_data_hash;
						if (task.slot->GetInstanceDataPtr() && task.slot->GetInstanceDataSize() > 0)
						{
							const auto *data = static_cast<const uint8_t *>(task.slot->GetInstanceDataPtr());
							payload.bytes.assign(data, data + task.slot->GetInstanceDataSize());
						}

						cached_payload = &payload;
						shadow_payload_index[payload_key] = cached_payload;
					}

					tiered_cache.UpsertPayload(payload_key, cached_payload, true);
				}

				if (cached_payload
				 && cached_payload->instance_hash == legacy_instance_hash
				 && cached_payload->domain_id == legacy_domain_id)
				{
					++r21_dry_run_stats.payload_match_with_legacy;
				}

				if (!cached_program || !cached_payload)
					continue;

				const auto binding_key = BuildBindingCacheKey(task, cached_payload);
				auto *cached_binding = tiered_cache.FindBinding(binding_key);
				if (cached_binding)
					++r21_dry_run_stats.candidate_binding_hits;
				if (!cached_binding)
				{
					auto binding_it = shadow_binding_index.find(binding_key);
					if (binding_it != shadow_binding_index.end())
					{
						cached_binding = binding_it->second;
					}
					else
					{
						auto &binding = shadow_binding_storage.emplace_back();
						binding.id = next_shadow_binding_id++;
						binding.program = cached_program;
						binding.payload = cached_payload;
						binding.domain = task.slot->resolved_domain;
						binding.layout_signature = 0;

						cached_binding = &binding;
						shadow_binding_index[binding_key] = cached_binding;
					}

					tiered_cache.UpsertBinding(binding_key, cached_binding, true);
				}

				if (cached_binding
				 && cached_binding->program == legacy_program
				 && cached_binding->domain == legacy_domain)
				{
					++r21_dry_run_stats.binding_match_with_legacy;
				}

				if (cached_program
				 && cached_program == legacy_program
				 && cached_payload
				 && cached_payload->instance_hash == legacy_instance_hash
				 && cached_payload->domain_id == legacy_domain_id
				 && cached_binding
				 && cached_binding->program == legacy_program
				 && cached_binding->domain == legacy_domain)
				{
					++r21_dry_run_stats.dry_run_short_circuit_eligible;
				}
			}
		}

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

				LogInfo("[ECS::MaterialResolveSystem][R2.1] dry_run_compare: "
				        "tasks=%llu candidate_hits(program=%llu payload=%llu binding=%llu) "
				        "legacy_match(program=%llu payload=%llu binding=%llu) "
				        "short_circuit_eligible=%llu",
				        static_cast<unsigned long long>(r21_dry_run_stats.tasks_seen),
				        static_cast<unsigned long long>(r21_dry_run_stats.candidate_program_hits),
				        static_cast<unsigned long long>(r21_dry_run_stats.candidate_payload_hits),
				        static_cast<unsigned long long>(r21_dry_run_stats.candidate_binding_hits),
				        static_cast<unsigned long long>(r21_dry_run_stats.program_match_with_legacy),
				        static_cast<unsigned long long>(r21_dry_run_stats.payload_match_with_legacy),
				        static_cast<unsigned long long>(r21_dry_run_stats.binding_match_with_legacy),
				        static_cast<unsigned long long>(r21_dry_run_stats.dry_run_short_circuit_eligible));

				r21_dry_run_stats = {};
			}
		}
	}
}//namespace hgl::ecs

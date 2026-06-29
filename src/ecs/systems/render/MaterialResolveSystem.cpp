#include<hgl/ecs/systems/render/MaterialResolveSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialResolveTieredCache.h>
#include<hgl/graph/module/MaterialRecipeRegistry.h>
#include<hgl/graph/module/MaterialBindingInstanceInternalAccess.h>
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
#include <cctype>
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

		static uint32_t HashDomainIDString(const std::string &did)
		{
			uint32_t h = 2166136261u;
			for (unsigned char c : did)
				h = (h ^ c) * 16777619u;

			return h == 0 ? 1u : h;
		}

		static bool TryParseDomainID(const std::string &did, uint32_t &out_id)
		{
			if (did.empty())
			{
				out_id = 0;
				return true;
			}

			uint64_t value = 0;
			for (char ch : did)
			{
				if (!std::isdigit(static_cast<unsigned char>(ch)))
					return false;

				value = value * 10 + uint64_t(ch - '0');
				if (value > 0xFFFFFFFFull)
					return false;
			}

			out_id = static_cast<uint32_t>(value);
			return true;
		}

		static uint32_t ResolveStableDomainID(const ResolveTask &task)
		{
			if (!task.recipe)
				return 0xFFFFFFFFu;

			uint32_t parsed = 0;
			if (TryParseDomainID(task.recipe->domain_id, parsed))
				return parsed;

			return HashDomainIDString(task.recipe->domain_id);
		}

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
			key.domain_id = ResolveStableDomainID(task);
			key.runtime_texture_generation = task.comp ? task.comp->GetRuntimeTextureBindingGeneration() : 0;
			return key;
		}

		static graph::BindingCacheKey BuildBindingCacheKey(const ResolveTask &task,
		                                                  const graph::ShaderMaterialProgram *program,
		                                                  const graph::MaterialInstancePayload *payload)
		{
			graph::BindingCacheKey key{};
			key.program = program;
			key.payload_id = payload ? payload->id : 0;
			key.domain_id = ResolveStableDomainID(task);
			key.layout_signature = 0;
			return key;
		}

		static bool TryApplyCachedLegacyBinding(const ResolveTask &task,
		                                       graph::PrimitiveManager *prim_mgr,
		                                       graph::ProgramInstanceBinding *cached_binding)
		{
			if (!task.comp || !task.slot || !task.geometry || !prim_mgr || !cached_binding)
				return false;

			auto *mi = cached_binding->legacy_binding_instance;
			if (!mi)
				return false;

			task.slot->resolved_program_binding = cached_binding;
			task.slot->resolved_program = cached_binding->program;
			task.slot->resolved_payload = cached_binding->payload;
			task.slot->resolved_binding_id = cached_binding->id;
			task.slot->resolved_payload_id = cached_binding->payload ? cached_binding->payload->id : 0;
			task.slot->resolved_binding_instance = mi;
			// Legacy bridge only: keep mirrored for compatibility while runtime reads use resolved_program.
			task.slot->resolved_material = task.slot->resolved_program;
			task.slot->resolved_domain = graph::MaterialBindingInstanceInternalAccess::GetDomain(mi);
			task.slot->resolved_domain_id = graph::MaterialBindingInstanceInternalAccess::GetDomainID(mi);
			task.slot->resolved_vil = cached_binding->legacy_vil;
			task.slot->resolved_mi_id = mi->GetMIID();
			task.slot->resolved_preset = mi->GetRenderPreset();
			task.slot->dirty = false;

			if (task.comp->GetUnresolvedGeometry())
			{
				auto *prim = prim_mgr->CreatePrimitive(task.geometry, mi, task.slot->resolved_vil);
				if (!prim)
					return false;

				task.comp->SetPrimitive(prim);
				task.comp->SetUnresolvedGeometry(nullptr);
			}
			else if (auto *existing_prim = task.comp->GetPrimitive())
			{
				if (!existing_prim->ChangeMaterialInstance(mi))
				{
					auto *replacement = prim_mgr->CreatePrimitive(task.geometry, mi, task.slot->resolved_vil);
					if (!replacement)
						return false;

					task.comp->SetPrimitive(replacement);
					prim_mgr->Release(existing_prim);
				}
			}

			auto *resolved_primitive = task.comp->GetPrimitive();
			if (resolved_primitive)
			{
				PrimitiveComponent::ResolvedMaterialState staged_state{};
				staged_state.program_binding = task.slot->resolved_program_binding;
				staged_state.program = task.slot->resolved_program;
				staged_state.payload = task.slot->resolved_payload;
				staged_state.binding_id = task.slot->resolved_binding_id;
				staged_state.payload_id = task.slot->resolved_payload_id;
				staged_state.binding_instance = task.slot->resolved_binding_instance;
				staged_state.material = staged_state.program;
				staged_state.domain = task.slot->resolved_domain;
				staged_state.domain_id = task.slot->resolved_domain_id;
				staged_state.vil = task.slot->resolved_vil;
				staged_state.mi_id = task.slot->resolved_mi_id;
				staged_state.preset = task.slot->resolved_preset;

				task.comp->SetStagingRenderState(staged_state, resolved_primitive);
			}

			return true;
		}

		static void EmitPeriodicDiagnosticsLogs(graph::GraphicsContext *gc,
		                                       MaterialResolveDiagnostics *diag,
		                                       const MaterialResolveR21DryRunStats &r21_dry_run_stats,
		                                       uint32_t short_circuit_executed_frame,
		                                       uint32_t short_circuit_failed_frame)
		{
			if (!gc || !diag || !diag->IsDecoupledCacheEnabled())
				return;

			const uint64_t now_ms = hgl::GetTimeMs();
			if (!diag->ShouldEmitPeriodicStatsLog(now_ms))
				return;

			const auto stats = gc->GetMaterialResolveTieredCacheStats();

			GLogInfo("[ECS::MaterialResolveSystem][R1.2] tiered_cache_stats: "
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

			GLogInfo("[ECS::MaterialResolveSystem][R2.1] dry_run_compare: "
			        "tasks=%llu candidate_hits(program=%llu payload=%llu binding=%llu) "
			        "miss(program=%llu payload=%llu binding=%llu) "
			        "legacy_match(program=%llu payload=%llu binding=%llu) "
			        "short_circuit(checks=%llu eligible=%llu would_execute=%llu executed=%llu failed=%llu auto_disable=%llu auto_reenable=%llu blocked_program=%llu blocked_payload=%llu blocked_binding=%llu guard_dirty=%llu guard_domain=%llu guard_vil=%llu guard_primitive=%llu)",
			        static_cast<unsigned long long>(r21_dry_run_stats.tasks_seen),
			        static_cast<unsigned long long>(r21_dry_run_stats.candidate_program_hits),
			        static_cast<unsigned long long>(r21_dry_run_stats.candidate_payload_hits),
			        static_cast<unsigned long long>(r21_dry_run_stats.candidate_binding_hits),
			        static_cast<unsigned long long>(r21_dry_run_stats.miss_program),
			        static_cast<unsigned long long>(r21_dry_run_stats.miss_payload),
			        static_cast<unsigned long long>(r21_dry_run_stats.miss_binding),
			        static_cast<unsigned long long>(r21_dry_run_stats.program_match_with_legacy),
			        static_cast<unsigned long long>(r21_dry_run_stats.payload_match_with_legacy),
			        static_cast<unsigned long long>(r21_dry_run_stats.binding_match_with_legacy),
			        static_cast<unsigned long long>(r21_dry_run_stats.short_circuit_checks),
			        static_cast<unsigned long long>(r21_dry_run_stats.dry_run_short_circuit_eligible),
			        static_cast<unsigned long long>(r21_dry_run_stats.would_short_circuit_execute),
			        static_cast<unsigned long long>(r21_dry_run_stats.short_circuit_executed),
			        static_cast<unsigned long long>(r21_dry_run_stats.short_circuit_apply_failures),
			        static_cast<unsigned long long>(r21_dry_run_stats.short_circuit_auto_disable_events),
			        static_cast<unsigned long long>(r21_dry_run_stats.short_circuit_auto_reenable_events),
			        static_cast<unsigned long long>(r21_dry_run_stats.short_circuit_blocked_by_program),
			        static_cast<unsigned long long>(r21_dry_run_stats.short_circuit_blocked_by_payload),
			        static_cast<unsigned long long>(r21_dry_run_stats.short_circuit_blocked_by_binding),
			        static_cast<unsigned long long>(r21_dry_run_stats.short_circuit_blocked_by_guard_dirty),
			        static_cast<unsigned long long>(r21_dry_run_stats.short_circuit_blocked_by_guard_domain),
			        static_cast<unsigned long long>(r21_dry_run_stats.short_circuit_blocked_by_guard_vil),
			        static_cast<unsigned long long>(r21_dry_run_stats.short_circuit_blocked_by_guard_primitive));

			if (short_circuit_executed_frame > 0 || short_circuit_failed_frame > 0)
			{
				GLogInfo("[ECS::MaterialResolveSystem][R2-C] execute_summary: executed=%u failed=%u consecutive_failures=%u execute_enabled=%u",
				        static_cast<unsigned>(short_circuit_executed_frame),
				        static_cast<unsigned>(short_circuit_failed_frame),
				        static_cast<unsigned>(diag->GetConsecutiveShortCircuitFailures()),
				        diag->IsDecoupledCacheExecuteShortCircuitEnabled() ? 1u : 0u);
			}

			uint64_t failure_rate_percent = 0;
			uint64_t short_circuit_samples = 0;
			if (diag->TryConsumeFailureRateWarning(now_ms,
			                                   r21_dry_run_stats.short_circuit_executed,
			                                   r21_dry_run_stats.short_circuit_apply_failures,
			                                   failure_rate_percent,
			                                   short_circuit_samples))
			{
				GLogWarning("[ECS::MaterialResolveSystem][R2-C] short-circuit failure-rate warning: failed=%llu executed=%llu failure_rate=%llu%% threshold=%u%% samples=%llu",
				        static_cast<unsigned long long>(r21_dry_run_stats.short_circuit_apply_failures),
				        static_cast<unsigned long long>(r21_dry_run_stats.short_circuit_executed),
				        static_cast<unsigned long long>(failure_rate_percent),
				        static_cast<unsigned>(diag->GetDecoupledCacheExecuteShortCircuitWarningFailureRatePercentThreshold()),
				        static_cast<unsigned long long>(short_circuit_samples));
			}

			diag->ResetFrameStats();
		}
	}

	MaterialResolveSystem::MaterialResolveSystem(const std::string &name)
		: System(name)
	{
		SetSystemType(SystemType::ShaderMaterialProgram);
		SetExecutionOrder(ExecutionPhase::RenderMaterialBind);
		SetRenderElementType("Primitive");
	}

	MaterialResolveDiagnostics *MaterialResolveSystem::GetDiagnostics()
	{
		return world ? &world->GetMaterialResolveDiagnostics() : nullptr;
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
		auto *diag = GetDiagnostics();
		if (!registry || !prim_mgr || !diag)
			return;

		auto &r21_dry_run_stats = diag->GetFrameStats();

		const uint64_t now_ms_for_state = hgl::GetTimeMs();
		if (diag->TryAutoReenableShortCircuit(now_ms_for_state))
		{
			LogInfo("[ECS::MaterialResolveSystem][R2-C] auto-reenable short-circuit after cooldown=%llu ms",
			        static_cast<unsigned long long>(diag->GetDecoupledCacheExecuteShortCircuitCooldownMs()));
		}

		if (diag->TryObserveManualDisableBypass())
		{
			LogInfo("[ECS::MaterialResolveSystem][R2-C] execute short-circuit is manually disabled; auto-reenable is bypassed");
		}

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
		uint32_t short_circuit_executed_frame = 0;
		uint32_t short_circuit_failed_frame = 0;
        ApplyStageStats apply_stats{};

		internal::BuildResolvePlan(primitives,
			recipe_store,
			tasks,
			prototype_buckets,
			resolve_input_count);

		if (diag->IsDecoupledCacheEnabled() && diag->IsDecoupledCacheExecuteShortCircuitEnabled())
		{
			auto &tiered_cache = gc->GetMaterialResolveTieredCache();

			for (auto &task : tasks)
			{
				if (!task.slot)
					continue;

				const auto program_key = BuildProgramCacheKey(task);
				auto *cached_program = tiered_cache.FindProgram(program_key);
				if (!cached_program)
					continue;

				const auto payload_key = BuildPayloadCacheKey(task);
				auto *cached_payload = tiered_cache.FindPayload(payload_key);
				if (!cached_payload)
					continue;

				const auto binding_key = BuildBindingCacheKey(task, cached_program, cached_payload);
				auto *cached_binding = tiered_cache.FindBinding(binding_key);
				if (!cached_binding)
					continue;

				const bool guard_dirty_ok = task.slot->dirty;
				const bool guard_domain_ok = ResolveStableDomainID(task) != 0xFFFFFFFFu;
				const bool guard_vil_ok = cached_binding->legacy_vil != nullptr;
				const bool guard_primitive_ok = task.comp
				                             && (task.comp->GetPrimitive() != nullptr || task.comp->GetUnresolvedGeometry() != nullptr);

				if (!(guard_dirty_ok && guard_domain_ok && guard_vil_ok && guard_primitive_ok))
					continue;

				if (TryApplyCachedLegacyBinding(task, prim_mgr, cached_binding))
				{
					++short_circuit_executed_frame;
					diag->OnShortCircuitApplySuccess();
				}
				else
				{
					++short_circuit_failed_frame;
					if (diag->OnShortCircuitApplyFailure(hgl::GetTimeMs()))
					{
						LogInfo("[ECS::MaterialResolveSystem][R2-C] auto-disable short-circuit after consecutive failures=%u (threshold=%u)",
						        static_cast<unsigned>(diag->GetConsecutiveShortCircuitFailures()),
						        static_cast<unsigned>(diag->GetDecoupledCacheExecuteShortCircuitFailureThreshold()));
					}
				}
			}

			prototype_buckets.clear();
			resolve_input_count = 0;

			for (size_t i = 0; i < tasks.size(); ++i)
			{
				const auto &task = tasks[i];
				if (!task.slot || !task.slot->dirty)
					continue;

				PrototypeKey pkey{};
				pkey.material_key = task.material_key;
				pkey.gvf_hash = task.gvf_hash;
				prototype_buckets[pkey].push_back(i);
				++resolve_input_count;
			}
		}

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

		if (diag->IsDecoupledCacheEnabled())
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

				auto *legacy_program = task.slot->resolved_program;
				auto *legacy_domain = task.slot->resolved_domain;
				const uint32_t legacy_domain_id = task.slot->resolved_domain_id;
				const uint64_t legacy_instance_hash = task.instance_hash;
				if (!cached_program && legacy_program)
				{
					tiered_cache.UpsertProgram(program_key, legacy_program, true);
					cached_program = legacy_program;
				}

				if (!cached_program)
					++r21_dry_run_stats.miss_program;

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

				if (!cached_payload)
					++r21_dry_run_stats.miss_payload;

				if (cached_payload
				 && cached_payload->instance_hash == legacy_instance_hash
				 && cached_payload->domain_id == legacy_domain_id)
				{
					++r21_dry_run_stats.payload_match_with_legacy;
				}

				if (!cached_program || !cached_payload)
					continue;

				const auto binding_key = BuildBindingCacheKey(task, cached_program, cached_payload);
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

				if (!cached_binding)
					++r21_dry_run_stats.miss_binding;

				if (cached_binding)
				{
					cached_binding->program = cached_program;
					cached_binding->payload = cached_payload;
					cached_binding->domain = task.slot->resolved_domain;
					cached_binding->legacy_binding_instance = task.slot->resolved_binding_instance;
					cached_binding->legacy_vil = task.slot->resolved_vil;
				}

				if (cached_binding
				 && cached_binding->program == legacy_program
				 && cached_binding->domain == legacy_domain)
				{
					++r21_dry_run_stats.binding_match_with_legacy;
				}

				const bool program_match = cached_program && cached_program == legacy_program;
				const bool payload_match = cached_payload
				                        && cached_payload->instance_hash == legacy_instance_hash
				                        && cached_payload->domain_id == legacy_domain_id;
				const bool binding_match = cached_binding
				                        && cached_binding->program == legacy_program
				                        && cached_binding->domain == legacy_domain;

				if (diag->IsDecoupledCacheDryRunShortCircuitCheckEnabled())
				{
					++r21_dry_run_stats.short_circuit_checks;

					if (!program_match)
						++r21_dry_run_stats.short_circuit_blocked_by_program;

					if (!payload_match)
						++r21_dry_run_stats.short_circuit_blocked_by_payload;

					if (!binding_match)
						++r21_dry_run_stats.short_circuit_blocked_by_binding;

					if (program_match && payload_match && binding_match)
						++r21_dry_run_stats.dry_run_short_circuit_eligible;

					if (diag->IsDecoupledCacheDryRunWhitelistEnabled())
					{
						const bool guard_dirty_ok = !task.slot->dirty;
						const bool guard_domain_ok = legacy_domain_id != 0xFFFFFFFFu;
						const bool guard_vil_ok = task.slot->resolved_vil != nullptr;
						const bool guard_primitive_ok = task.comp && task.comp->GetPrimitive() != nullptr;

						if (!guard_dirty_ok)
							++r21_dry_run_stats.short_circuit_blocked_by_guard_dirty;

						if (!guard_domain_ok)
							++r21_dry_run_stats.short_circuit_blocked_by_guard_domain;

						if (!guard_vil_ok)
							++r21_dry_run_stats.short_circuit_blocked_by_guard_vil;

						if (!guard_primitive_ok)
							++r21_dry_run_stats.short_circuit_blocked_by_guard_primitive;

						if (program_match
						 && payload_match
						 && binding_match
						 && guard_dirty_ok
						 && guard_domain_ok
						 && guard_vil_ok
						 && guard_primitive_ok)
						{
							++r21_dry_run_stats.would_short_circuit_execute;
						}
					}
				}
			}
		}

		resolved_count = apply_stats.resolved_count + short_circuit_executed_frame;
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

		EmitPeriodicDiagnosticsLogs(gc,
		                           diag,
		                           r21_dry_run_stats,
		                           short_circuit_executed_frame,
		                           short_circuit_failed_frame);
	}
}//namespace hgl::ecs

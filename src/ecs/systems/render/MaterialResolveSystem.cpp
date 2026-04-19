#include<hgl/ecs/systems/render/MaterialResolveSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialRecipeRegistry.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/log/Log.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace hgl::ecs
{
	namespace
	{
		struct ResolveTask
		{
			std::shared_ptr<PrimitiveComponent> comp;
			graph::MaterialResolveRequest *slot = nullptr;
			graph::Geometry *geometry = nullptr;
			const graph::mtl::MaterialRecipe *recipe = nullptr;
			uint64_t gvf_hash = 0;
			uint64_t instance_hash = 0;
		};

		struct PrototypeKey
		{
			const graph::mtl::MaterialRecipe *recipe = nullptr;
			uint64_t gvf_hash = 0;

			bool operator==(const PrototypeKey &o) const
			{
				return recipe == o.recipe && gvf_hash == o.gvf_hash;
			}
		};

		struct PrototypeKeyHash
		{
			size_t operator()(const PrototypeKey &k) const
			{
				const size_t h1 = std::hash<const void *>{}(static_cast<const void *>(k.recipe));
				const size_t h2 = std::hash<uint64_t>{}(k.gvf_hash);
				return h1 ^ (h2 + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
			}
		};

		struct MIKey
		{
			uint64_t prototype_hash = 0;
			uint64_t instance_hash = 0;

			bool operator==(const MIKey &o) const
			{
				return prototype_hash == o.prototype_hash
					&& instance_hash == o.instance_hash;
			}
		};

		struct MIKeyHash
		{
			size_t operator()(const MIKey &k) const
			{
				const size_t h1 = std::hash<uint64_t>{}(k.prototype_hash);
				const size_t h2 = std::hash<uint64_t>{}(k.instance_hash);
				return h1 ^ (h2 + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
			}
		};

		static inline uint64_t HashBytes(const void *data, const uint32_t size)
		{
			if (!data || size == 0)
				return 1469598103934665603ull;

			const auto *bytes = static_cast<const uint8_t *>(data);
			uint64_t h = 1469598103934665603ull;

			for (uint32_t i = 0; i < size; ++i)
			{
				h ^= static_cast<uint64_t>(bytes[i]);
				h *= 1099511628211ull;
			}

			return h;
		}

		static inline uint64_t HashGeometryVertexFormat(const graph::GeometryVertexFormat &gvf)
		{
			uint64_t h = 1469598103934665603ull;

			const auto mix = [&h](const uint64_t v)
			{
				h ^= v;
				h *= 1099511628211ull;
			};

			mix(static_cast<uint64_t>(gvf.GetActiveCount()));

			for (int i = 0; i < static_cast<int>(graph::VertexAttrib::RANGE_SIZE); ++i)
			{
				const auto attrib = static_cast<graph::VertexAttrib>(i);
				const auto *slot = gvf.GetSlot(attrib);
				if (!slot)
					continue;

				mix(static_cast<uint64_t>(i + 1));
				mix(static_cast<uint64_t>(slot->format));
				mix(static_cast<uint64_t>(slot->stride));
				mix(static_cast<uint64_t>(static_cast<uint32_t>(slot->binding + 1)));
			}

			return h;
		}

		static inline uint64_t HashPrototypeKey(const PrototypeKey &k)
		{
			const uint64_t p = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(k.recipe));
			return (p * 1099511628211ull) ^ (k.gvf_hash + 0x9e3779b97f4a7c15ull + (p << 6) + (p >> 2));
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
		if (!registry || !prim_mgr)
			return;

		std::vector<std::shared_ptr<PrimitiveComponent>> primitives;
		world->GetComponents<PrimitiveComponent>(primitives);

		std::vector<ResolveTask> tasks;
		tasks.reserve(primitives.size());

		std::unordered_map<PrototypeKey, std::vector<size_t>, PrototypeKeyHash> prototype_buckets;
		prototype_buckets.reserve(primitives.size());

		uint32_t resolve_input_count = 0;
		uint32_t resolved_count = 0;
		uint32_t resolve_fail_count = 0;
		uint32_t primitive_created_count = 0;
		uint32_t primitive_updated_count = 0;
		uint32_t primitive_recreated_count = 0;
		uint32_t mi_bucket_count = 0;

		for (auto &comp : primitives)
		{
			if (!comp || !comp->NeedsMaterialBindingResolve())
				continue;

			auto &slot = comp->GetMaterialResolveRequest();
			if (!slot.record)
				continue;

			graph::Geometry *geom = comp->GetUnresolvedGeometry();
			if (!geom && comp->GetPrimitive())
				geom = comp->GetPrimitive()->GetGeometry();

			if (!geom)
				continue;

			const auto &gvf = geom->GetGeometryVertexFormat();
			ResolveTask task;
			task.comp = comp;
			task.slot = &slot;
			task.geometry = geom;
			task.recipe = slot.record;
			task.gvf_hash = HashGeometryVertexFormat(gvf);
			task.instance_hash = HashBytes(slot.GetInstanceDataPtr(), slot.GetInstanceDataSize());

			const size_t index = tasks.size();
			tasks.push_back(task);
			++resolve_input_count;

			PrototypeKey pkey;
			pkey.recipe = task.recipe;
			pkey.gvf_hash = task.gvf_hash;
			prototype_buckets[pkey].push_back(index);
		}

		for (const auto &[pkey, indices] : prototype_buckets)
		{
			if (indices.empty())
				continue;

			std::unordered_map<MIKey, std::vector<size_t>, MIKeyHash> mi_buckets;
			mi_buckets.reserve(indices.size());

			const uint64_t proto_hash = HashPrototypeKey(pkey);

			for (const size_t idx : indices)
			{
				const ResolveTask &task = tasks[idx];

				MIKey mikey;
				mikey.prototype_hash = proto_hash;
				mikey.instance_hash = task.instance_hash;
				mi_buckets[mikey].push_back(idx);
			}

			for (const auto &[mikey, mi_indices] : mi_buckets)
			{
				++mi_bucket_count;

				if (mi_indices.empty())
					continue;

				const ResolveTask &seed = tasks[mi_indices.front()];
				const auto &seed_gvf = seed.geometry->GetGeometryVertexFormat();

				graph::MaterialBindingInstance *mi =
					registry->ResolveOrCreateBindingInstance(*seed.recipe,
															 seed_gvf,
															 seed.slot->GetInstanceDataPtr(),
															 seed.slot->GetInstanceDataSize());
				if (!mi)
				{
					resolve_fail_count += static_cast<uint32_t>(mi_indices.size());
					continue;
				}

				for (const size_t idx : mi_indices)
				{
					ResolveTask &task = tasks[idx];

					task.slot->resolved_binding_instance = mi;
					task.slot->resolved_material = mi->GetShaderMaterialProgram();
					task.slot->resolved_domain = mi->GetDomain();
					task.slot->resolved_domain_id = mi->GetDomainID();
					task.slot->resolved_vil = mi->GetVIL();
					task.slot->resolved_mi_id = mi->GetMIID();
					task.slot->resolved_preset = mi->GetRenderPreset();
					task.slot->dirty = false;
					++resolved_count;

					// Stage-2: unresolved geometry still creates Primitive in-place.
					if (task.comp->GetUnresolvedGeometry())
					{
						if (auto *prim = prim_mgr->CreatePrimitive(task.geometry, mi))
						{
							task.comp->SetPrimitive(prim);
							task.comp->SetUnresolvedGeometry(nullptr);
							++primitive_created_count;
						}
					}
					else if (auto *existing_prim = task.comp->GetPrimitive())
					{
						if (existing_prim->ChangeMaterialInstance(mi))
						{
							++primitive_updated_count;
						}
						else
						{
							auto *replacement = prim_mgr->CreatePrimitive(task.geometry, mi);
							if (replacement)
							{
								task.comp->SetPrimitive(replacement);
								prim_mgr->Release(existing_prim);
								++primitive_recreated_count;
							}
							else
							{
								++resolve_fail_count;
							}
						}
					}
				}
			}
		}

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
	}
}//namespace hgl::ecs

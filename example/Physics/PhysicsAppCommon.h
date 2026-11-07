#pragma once
// PhysicsAppCommon.h —— JoltPhysics 范例 1~4 的公共「渲染 + 交互」层（header-only）
//
// 职责：
//   · 复用 RenderBoundBox 的配方/调色板（RenderBoundBox.Solid/.Wire +
//     render_bound_box.material_data.toml 的 16 行颜色），零新增资产；
//   · 从 ModelCollisionProxyComponent 自动为物理实体生成视觉子实体：
//       Box     -> geo:builtin:cube_2x2x2（±0.5 单位立方体，scale = 2*half_extents）
//       Capsule -> geo:builtin:capsule   （r=0.5/hh=1.0，轴局部 Z；非均匀缩放，端盖呈椭球，演示精度足够）
//       复合体  -> 多个子实体按 local_center 偏移挂到物理实体变换下
//   · 相机（ViewModel：左键旋转 / 滚轮缩放 / 右键平移）、物理系统注册、鼠标射线。
//
// 注意：内建 cube/plane/bounding_box 预设的实际尺寸都是 ±0.5（单位盒），
// 任何「按物理半长摆放」的视觉都必须 scale = 2 * half。

#include <hgl/framework/WorkObject.h>
#include <hgl/vk/VertexDataManager.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/asset/GeometryAssetRegistry.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/ssbo/GlobalSSBOTypes.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/math/geometry/Ray.h>
#include <hgl/log/Log.h>

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/entity/Entity.h>
#include <hgl/ecs/support/transform/TransformAccessor.h>
#include <hgl/ecs/components/GeometryData.h>
#include <hgl/ecs/components/MaterialData.h>
#include <hgl/ecs/components/CameraComponent.h>
#include <hgl/ecs/components/CameraControlMode.h>
#include <hgl/ecs/components/ModelCollisionProxyComponent.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/ecs/systems/tick/JoltPhysicsSystem.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace hgl::example
{
    /// 调色板行数（与 render_bound_box.material_data.toml 行数一致，门禁测试核对）
    constexpr uint32_t kPhysicsColorCount = 16;

    /// 语义化行号（行序 = 文件内顺序）
    enum PhysicsRow : uint32_t
    {
        kRowGround  = 0,   ///< 深灰：静态地面
        kRowBone    = 1,
        kRowCrimson = 2,   ///< 绯红：弹性落箱
        kRowGreen   = 3,
        kRowBlue    = 4,   ///< 蓝：静态斜坡
        kRowYellow  = 5,   ///< 黄：运动学平台 / 新生成体
        kRowPink    = 6,
        kRowSky     = 7,
        kRowGreen2  = 8,
        kRowRed     = 9,   ///< 红：位于传感器体积内
        kRowWhite   = 10,  ///< 白：普通靶箱
        kRowTeal    = 11,
        kRowSalmon  = 12,
        kRowOrange  = 13,  ///< 橙：胶囊
        kRowCyan    = 14,  ///< 青：传感器线框
        kRowPurple  = 15,  ///< 紫：复合小车
    };

    /**
     * @brief GUI 范例共用装配器：材质/几何/视觉生成/相机/射线/物理系统注册。
     *
     * 生命周期 = WorkObject 子类（在 Init() 里 Init(this)，析构自动清理）。
     */
    class PhysicsVisualKit
    {
    public:
        PhysicsVisualKit() = default;
        ~PhysicsVisualKit() { Shutdown(); }

        PhysicsVisualKit(const PhysicsVisualKit &) = delete;
        PhysicsVisualKit &operator=(const PhysicsVisualKit &) = delete;

        bool Init(WorkObject *app);
        std::shared_ptr<ecs::JoltPhysicsSystem> EnsurePhysics();
        /// 为物理实体挂视觉子实体（每个代理 body 一个子实体）；wire=true 走线框盒
        void AttachVisuals(ecs::Entity *phys, uint32_t row, bool wire = false);
        /// 重新着色已挂视觉（传感器高亮/命中反馈用）
        void SetVisualRow(ecs::Entity *phys, uint32_t row, bool wire = false);
        bool InitCamera(const math::Vector3f &target, float distance, float yaw_deg, float pitch_deg);
        /// 视口鼠标 -> 世界射线（范例 04 拾取）
        bool PickRay(math::Ray &out_ray) const;
        /// 动态体轮询取鲜艳色行（跳过灰/白底）
        uint32_t NextDynamicRow();

    private:
        static uint32_t RowClamp(uint32_t row)
        {
            return (row < kPhysicsColorCount) ? row : 0u;
        }

        bool InitRecipes();
        bool InitVDM();
        bool InitGeometries();
        graph::PrimitiveAsset *PushAsset(graph::Geometry *geo,
                                         const graph::mtl::MaterialRecipe &recipe,
                                         graph::PrimitiveType type);
        void Shutdown();

        WorkObject *app_ = nullptr;
        ecs::ECSContext *ctx_ = nullptr;

        graph::VertexDataManager *vdm_ = nullptr;
        graph::mtl::MaterialRecipe solid_recipe_{};
        graph::mtl::MaterialRecipe wire_recipe_{};
        graph::GlobalSSBOBinding solid_rows_[kPhysicsColorCount]{};
        graph::GlobalSSBOBinding wire_rows_[kPhysicsColorCount]{};

        graph::Geometry *cube_geo_    = nullptr;
        graph::Geometry *capsule_geo_ = nullptr;
        graph::Geometry *bbox_geo_    = nullptr;

        std::deque<graph::PrimitiveAsset> assets_;
        graph::PrimitiveAsset *cube_solid_    = nullptr;
        graph::PrimitiveAsset *capsule_solid_ = nullptr;
        graph::PrimitiveAsset *cube_wire_     = nullptr;
        graph::PrimitiveAsset *capsule_wire_  = nullptr;

        std::unordered_map<ecs::Entity *, std::vector<ecs::MaterialData *>> visuals_;
        uint32_t vis_counter_ = 0;
        uint32_t dynamic_cursor_ = 0;
    };

    // ── header-only 实现（每个范例都是独立 exe，一个 TU 一份）────────────────

    inline bool PhysicsVisualKit::Init(WorkObject *app)
    {
        app_ = app;
        ctx_ = app->GetECSContext();
        if(ctx_ == nullptr)
        {
            GLogError("[PhysicsKit] WorkObject has no ECS context");
            return false;
        }
        return InitRecipes() && InitVDM() && InitGeometries();
    }

    inline bool PhysicsVisualKit::InitRecipes()
    {
        const char *const kMtlUri =
            "mtldata:file:ShaderLibrary/asset/material_data/render_bound_box.material_data.toml";

        const graph::mtl::MaterialRecipe *loaded_solid = app_->GetRecipe("RenderBoundBox.Solid");
        const graph::mtl::MaterialRecipe *loaded_wire  = app_->GetRecipe("RenderBoundBox.Wire");
        if(loaded_solid == nullptr || loaded_wire == nullptr)
        {
            GLogError("[PhysicsKit] RenderBoundBox recipes not available");
            return false;
        }
        solid_recipe_ = *loaded_solid;
        wire_recipe_  = *loaded_wire;

        graph::GlobalSSBOBinding first{};
        if(!app_->GetMaterialDataBinding(kMtlUri, 0, first))
        {
            GLogError("[PhysicsKit] material_data.toml binding failed");
            return false;
        }
        // 同一数据块内的相邻行：基行 + 序号，不必逐行查 URI
        for(uint32_t i = 0; i < kPhysicsColorCount; ++i)
        {
            solid_rows_[i] = first.RowOffset(i);
        }
        // 线框配方共用同一份行数据：同色相、不同渲染状态
        for(uint32_t i = 0; i < kPhysicsColorCount; ++i)
        {
            wire_rows_[i] = solid_rows_[i];
        }
        // 配方的默认行只需合法；逐实体颜色经 MaterialData 行覆盖
        solid_recipe_.material_ssbo_binding = solid_rows_[0];
        wire_recipe_.material_ssbo_binding  = wire_rows_[0];
        return true;
    }

    inline bool PhysicsVisualKit::InitVDM()
    {
        auto *buf_mgr = app_->GetManager<graph::BufferManager>();
        if(buf_mgr == nullptr)
        {
            GLogError("[PhysicsKit] BufferManager unavailable");
            return false;
        }
        vdm_ = new graph::VertexDataManager(buf_mgr,
            {
                {graph::VertexSemantic::Position, VF_V3F},
                {graph::VertexSemantic::Normal,   VF_V3F},
            });
        if(vdm_ == nullptr || !vdm_->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, graph::IndexType::U32))
        {
            GLogError("[PhysicsKit] VertexDataManager init failed");
            return false;
        }
        return true;
    }

    inline bool PhysicsVisualKit::InitGeometries()
    {
        graph::GeometryCreater pc(vdm_);
        graph::SharedCreaterScope scope(app_->GetGeometryAssetRegistry(), &pc);

        cube_geo_    = app_->GetGeometry("geo:builtin:cube_2x2x2");
        capsule_geo_ = app_->GetGeometry("geo:builtin:capsule");
        if(cube_geo_ == nullptr || capsule_geo_ == nullptr)
        {
            GLogError("[PhysicsKit] builtin cube/capsule geometry unavailable");
            return false;
        }

        // 线框盒：独立纯色顶点布局（仅 Position）⇒ format_override
        const graph::GeometryVertexFormat bbox_format{
            {graph::VertexSemantic::Position, VF_V3F},
        };
        bbox_geo_ = app_->GetGeometry("geo:builtin:bounding_box", &bbox_format);
        if(bbox_geo_ == nullptr)
        {
            GLogError("[PhysicsKit] builtin bounding_box geometry unavailable");
            return false;
        }

        cube_solid_    = PushAsset(cube_geo_,    solid_recipe_, graph::PrimitiveType::Triangles);
        capsule_solid_ = PushAsset(capsule_geo_, solid_recipe_, graph::PrimitiveType::Triangles);
        // 线框统一用包围盒几何：盒与胶囊都用 12 条棱示意，避免胶囊没有线框预设
        cube_wire_     = PushAsset(bbox_geo_,    wire_recipe_,  graph::PrimitiveType::Lines);
        capsule_wire_  = cube_wire_;
        return cube_solid_ != nullptr && capsule_solid_ != nullptr && cube_wire_ != nullptr;
    }

    inline graph::PrimitiveAsset *PhysicsVisualKit::PushAsset(graph::Geometry *geo,
                                                              const graph::mtl::MaterialRecipe &recipe,
                                                              graph::PrimitiveType type)
    {
        assets_.emplace_back(geo, &recipe, type);
        graph::PrimitiveAsset *asset = &assets_.back();
        if(!asset->IsValid())
        {
            GLogError("[PhysicsKit] PrimitiveAsset invalid");
            return nullptr;
        }
        return asset;
    }

    inline std::shared_ptr<ecs::JoltPhysicsSystem> PhysicsVisualKit::EnsurePhysics()
    {
        auto physics = ctx_->GetSystem<ecs::JoltPhysicsSystem>();
        if(physics == nullptr)
        {
            physics = ctx_->RegisterSystem<ecs::JoltPhysicsSystem>();
            if(physics == nullptr)
            {
                GLogError("[PhysicsKit] JoltPhysicsSystem register failed");
                return nullptr;
            }
            physics->Initialize();
        }
        return physics;
    }

    inline void PhysicsVisualKit::AttachVisuals(ecs::Entity *phys, uint32_t row, bool wire)
    {
        if(phys == nullptr || ctx_ == nullptr)
        {
            return;
        }
        const ecs::ModelCollisionProxyComponent *proxy = ecs::GetModelCollisionProxy(phys);
        if(proxy == nullptr)
        {
            return;
        }
        ecs::TransformAccessor parent_t = ctx_->GetTransformByEntity(phys->GetEntityID());
        if(!parent_t.IsValid())
        {
            GLogWarning("[PhysicsKit] entity %p has no transform; visuals skipped", (void *)phys);
            return;
        }

        auto &mats = visuals_[phys];
        const uint32_t body_count = proxy->GetBodyCount();
        for(uint32_t i = 0; i < body_count; ++i)
        {
            const ecs::ModelCollisionBody *b = proxy->GetBody(i);
            if(b == nullptr || b->shape == ecs::ModelCollisionShape::None)
            {
                continue;
            }
            // ConvexHull 没有专用内建几何：以 local_center 处的包围盒近似示意
            const bool is_capsule = (b->shape == ecs::ModelCollisionShape::Capsule);

            std::string name = "PV" + std::to_string(vis_counter_++);
            ecs::Entity *child = ctx_->CreateEntity<ecs::Entity>(name);
            if(child == nullptr)
            {
                continue;
            }
            ecs::TransformID tid = ctx_->CreateTransform(child->GetEntityID(), ecs::Mobility::Movable);
            ecs::TransformAccessor t = ctx_->GetTransform(tid);
            if(!t.IsValid())
            {
                continue;
            }
            t.SetParent(parent_t.GetID());

            const glm::vec3 pos(b->local_center);
            // local_rotation：局部欧拉（弧度，XYZ 序）
            const glm::quat rot = (b->shape == ecs::ModelCollisionShape::Box)
                                      ? glm::quat(glm::radians(glm::vec3(b->local_rotation)))
                                      : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            glm::vec3 scale(1.0f);
            if(b->shape == ecs::ModelCollisionShape::Box)
            {
                scale = 2.0f * glm::vec3(b->half_extents);   // 内建盒 = ±0.5 单位盒
            }
            else if(is_capsule)
            {
                // 内建胶囊 r=0.5 / hh=1.0（总半高 1.5，轴局部 Z）：非均匀缩放，端盖呈椭球
                scale = glm::vec3(2.0f * b->radius, 2.0f * b->radius,
                                  (b->half_height + b->radius) / 1.5f);
            }
            else
            {
                scale = glm::vec3(2.0f * b->radius);          // ConvexHull 近似
            }
            t.SetLocalTRS(pos, rot, scale);

            graph::PrimitiveAsset *asset = nullptr;
            if(wire)
            {
                asset = cube_wire_;   // 线框统一 bbox 几何
            }
            else
            {
                asset = is_capsule ? capsule_solid_ : cube_solid_;
            }
            if(asset == nullptr)
            {
                continue;
            }
            if(ecs::GeometryData *geo = ctx_->GetOrCreateGeometryData(child))
            {
                geo->SetPrimitiveAsset(asset);
            }
            if(ecs::MaterialData *mat = ctx_->GetOrCreateMaterialData(child))
            {
                ecs::MaterialData::MaterialDataAuthoringResource res{};
                res = solid_rows_[RowClamp(row)];
                mat->SetDataResource(res);
                mats.push_back(mat);
            }
        }
    }

    inline void PhysicsVisualKit::SetVisualRow(ecs::Entity *phys, uint32_t row, bool /*wire*/)
    {
        auto it = visuals_.find(phys);
        if(it == visuals_.end())
        {
            return;
        }
        const graph::GlobalSSBOBinding &binding = solid_rows_[RowClamp(row)];
        for(ecs::MaterialData *mat : it->second)
        {
            if(mat == nullptr)
            {
                continue;
            }
            ecs::MaterialData::MaterialDataAuthoringResource res{};
            res = binding;
            mat->SetDataResource(res);
        }
    }

    inline bool PhysicsVisualKit::InitCamera(const math::Vector3f &target, float distance,
                                             float yaw_deg, float pitch_deg)
    {
        ctx_->EnsureCameraSystem();
        ecs::Entity *cam_entity = ctx_->CreateEntity<ecs::Entity>("MainCamera");
        if(cam_entity == nullptr)
        {
            return false;
        }
        ecs::CameraComponent *camera = ctx_->GetOrCreateCamera(cam_entity);
        if(camera == nullptr)
        {
            return false;
        }
        camera->control_mode   = ecs::CameraControlMode::ViewModel;
        camera->target         = target;
        camera->distance       = distance;
        camera->yaw            = yaw_deg;
        camera->pitch          = pitch_deg;
        camera->is_main_camera = true;
        camera->matrix_dirty   = true;
        camera->viewport_info  = app_->GetViewportInfo();
        return true;
    }

    inline bool PhysicsVisualKit::PickRay(math::Ray &out_ray) const
    {
        const math::Vector2i *mouse = app_->GetMouseCoord();
        const graph::CameraInfo *ci = ctx_->ResolveActiveCameraInfo();
        const graph::ViewportInfo *vi = app_->GetViewportInfo();
        if(mouse == nullptr || ci == nullptr || vi == nullptr)
        {
            return false;
        }
        out_ray.SetFromViewportPoint(*mouse, ci, vi->GetViewport());
        return true;
    }

    inline uint32_t PhysicsVisualKit::NextDynamicRow()
    {
        static constexpr uint32_t kPool[] = {3, 4, 5, 6, 8, 11, 12, 13, 14, 15};
        constexpr uint32_t kPoolCount = sizeof(kPool) / sizeof(kPool[0]);
        return kPool[dynamic_cursor_++ % kPoolCount];
    }

    inline void PhysicsVisualKit::Shutdown()
    {
        visuals_.clear();
        assets_.clear();
        cube_solid_ = capsule_solid_ = cube_wire_ = capsule_wire_ = nullptr;
        // 几何归注册器所有：只清注册器缓存，不 delete Geometry
        if(app_ != nullptr)
        {
            app_->GetGeometryAssetRegistry()->Clear();
        }
        cube_geo_ = capsule_geo_ = bbox_geo_ = nullptr;
        SAFE_CLEAR(vdm_);
        app_ = nullptr;
        ctx_ = nullptr;
    }

}//namespace hgl::example

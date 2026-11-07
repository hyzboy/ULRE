#include <hgl/ecs/systems/tick/JoltPhysicsSystem.h>
#include <hgl/ecs/core/entity/Entity.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/support/transform/TransformAccessor.h>
#include <hgl/log/Log.h>

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollideShape.h>

#include <cstdarg>
#include <thread>
#include <unordered_set>
#include <atomic>
#include <algorithm>

namespace hgl::ecs
{
    // =========================================================================
    // Jolt 回调与全局生命周期
    // =========================================================================

    static void JoltTraceImpl(const char *inFMT, ...)
    {
        va_list list;
        va_start(list, inFMT);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), inFMT, list);
        va_end(list);
        GLogInfo("[Jolt] %s", buffer);
    }

#ifdef JPH_ENABLE_ASSERTS
    static bool JoltAssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, JPH::uint inLine)
    {
        GLogError("[Jolt Assert] %s:%u: (%s) %s", inFile, inLine, inExpression, inMessage ? inMessage : "");
        return true;
    }
#endif

    static std::atomic<int> s_jolt_instance_count{0};

    static void EnsureJoltGlobalInit()
    {
        if (s_jolt_instance_count.fetch_add(1) == 0)
        {
            JPH::RegisterDefaultAllocator();
            JPH::Trace = JoltTraceImpl;
            JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailedImpl;)
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }
    }

    static void ReleaseJoltGlobal()
    {
        if (s_jolt_instance_count.fetch_sub(1) == 1)
        {
            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }
    }

    // =========================================================================
    // 物理层定义（ObjectLayer 与 BroadPhaseLayer）
    // =========================================================================

    namespace Layers
    {
        static constexpr JPH::ObjectLayer NON_MOVING = 0;
        static constexpr JPH::ObjectLayer MOVING = 1;
        static constexpr JPH::ObjectLayer SENSOR = 2;
        static constexpr JPH::ObjectLayer NUM_LAYERS = 3;
    }

    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr JPH::BroadPhaseLayer SENSOR(2);
        static constexpr JPH::uint NUM_LAYERS(3);
    }

    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
    public:
        BPLayerInterfaceImpl()
        {
            mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
            mObjectToBroadPhase[Layers::MOVING]     = BroadPhaseLayers::MOVING;
            mObjectToBroadPhase[Layers::SENSOR]     = BroadPhaseLayers::SENSOR;
        }

        JPH::uint GetNumBroadPhaseLayers() const override
        {
            return BroadPhaseLayers::NUM_LAYERS;
        }

        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
        {
            JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
            return mObjectToBroadPhase[inLayer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
        {
            switch ((JPH::BroadPhaseLayer::Type)inLayer)
            {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:     return "MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::SENSOR:     return "SENSOR";
            default: JPH_ASSERT(false); return "INVALID";
            }
        }
#endif

    private:
        JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
    };

    class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
        {
            switch (inLayer1)
            {
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
                return inLayer2 == BroadPhaseLayers::NON_MOVING || inLayer2 == BroadPhaseLayers::MOVING || inLayer2 == BroadPhaseLayers::SENSOR;
            case Layers::SENSOR:
                return inLayer2 == BroadPhaseLayers::MOVING;
            default:
                return false;
            }
        }
    };

    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override
        {
            switch (inLayer1)
            {
            case Layers::NON_MOVING:
                return inLayer2 == Layers::MOVING;
            case Layers::MOVING:
                return inLayer2 == Layers::NON_MOVING || inLayer2 == Layers::MOVING || inLayer2 == Layers::SENSOR;
            case Layers::SENSOR:
                return inLayer2 == Layers::MOVING;
            default:
                return false;
            }
        }
    };

    // =========================================================================
    // 数学转换辅助
    // =========================================================================

    static inline JPH::Vec3 ToJPH(const math::Vector3f &v) { return JPH::Vec3(v.x, v.y, v.z); }
    static inline math::Vector3f FromJPH(const JPH::Vec3 &v) { return math::Vector3f(v.GetX(), v.GetY(), v.GetZ()); }

    static inline JPH::Quat ToJPH(const glm::quat &q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
    static inline glm::quat FromJPH(const JPH::Quat &q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

    static inline uint64_t EntityIDToU64(EntityID id)
    {
        return (static_cast<uint64_t>(id.index) << 16) | static_cast<uint64_t>(id.generation);
    }

    static inline EntityID U64ToEntityID(uint64_t val)
    {
        return EntityID(static_cast<uint32_t>(val >> 16), static_cast<uint16_t>(val & 0xFFFF));
    }

    // =========================================================================
    // JoltPhysicsSystem::Impl 内部封装
    // =========================================================================

    class JoltPhysicsSystem::Impl
    {
    public:
        std::unique_ptr<JPH::TempAllocatorImpl> temp_allocator;
        std::unique_ptr<JPH::JobSystemThreadPool> job_system;
        BPLayerInterfaceImpl broad_phase_layer_interface;
        ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
        ObjectLayerPairFilterImpl object_layer_pair_filter;
        std::unique_ptr<JPH::PhysicsSystem> physics_system;

        Impl()
        {
            EnsureJoltGlobalInit();

            // 预留 16MB 物理临时内存
            temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(16 * 1024 * 1024);

            // 多线程工作线程池
            const uint32_t num_threads = std::max(1u, std::thread::hardware_concurrency() - 1);
            job_system = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, num_threads);

            physics_system = std::make_unique<JPH::PhysicsSystem>();

            constexpr uint32_t cMaxBodies = 10240;
            constexpr uint32_t cNumBodyMutexes = 0;
            constexpr uint32_t cMaxBodyPairs = 10240;
            constexpr uint32_t cMaxContactConstraints = 10240;

            physics_system->Init(
                cMaxBodies,
                cNumBodyMutexes,
                cMaxBodyPairs,
                cMaxContactConstraints,
                broad_phase_layer_interface,
                object_vs_broadphase_layer_filter,
                object_layer_pair_filter
            );

            // 引擎为 Vulkan 右手系 Z-up，默认重力沿 -Z 轴
            physics_system->SetGravity(JPH::Vec3(0.0f, 0.0f, -9.81f));
        }

        ~Impl()
        {
            physics_system.reset();
            job_system.reset();
            temp_allocator.reset();
            ReleaseJoltGlobal();
        }
    };

    // =========================================================================
    // JoltPhysicsSystem 外部接口实现
    // =========================================================================

    JoltPhysicsSystem::JoltPhysicsSystem(const std::string &name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::TickPhysics);
    }

    JoltPhysicsSystem::~JoltPhysicsSystem()
    {
        Shutdown();
    }

    void JoltPhysicsSystem::Initialize()
    {
        if (!impl)
        {
            impl = std::make_unique<Impl>();
        }
    }

    void JoltPhysicsSystem::Shutdown()
    {
        impl.reset();
    }

    uint32_t JoltPhysicsSystem::CreateOrUpdateBody(Entity *entity)
    {
        if (!entity || !impl)
            return 0xFFFFFFFFu;

        auto *proxy = GetModelCollisionProxy(entity);
        if (!proxy || !proxy->HasCollision())
            return 0xFFFFFFFFu;

        // 若实体尚未挂载 RigidBodyComponent，按 Transform 的静态/动态特性自动补齐
        auto *rb = GetRigidBody(entity);
        ECSContext *w = GetWorld();
        if (!rb)
        {
            rb = entity->AddComponent<RigidBodyComponent>();
            if (!rb)
                return 0xFFFFFFFFu;

            if (w)
            {
                TransformAccessor transform = w->GetTransformByEntity(entity->GetEntityID());
                if (transform.IsValid() && transform.IsMovable())
                {
                    rb->motion_type = BodyMotionType::Kinematic;
                }
                else
                {
                    rb->motion_type = BodyMotionType::Static;
                }
            }
        }

        // 若已有物理刚体，先销毁旧体
        if (rb->IsValidBody())
        {
            DestroyBody(rb);
        }

        // 构造 JPH Shape
        JPH::Ref<JPH::StaticCompoundShapeSettings> compound = new JPH::StaticCompoundShapeSettings();
        const uint32_t body_count = proxy->GetBodyCount();

        for (uint32_t i = 0; i < body_count; ++i)
        {
            const ModelCollisionBody *body = proxy->GetBody(i);
            if (!body)
                continue;

            switch (body->shape)
            {
            case ModelCollisionShape::Box:
            {
                JPH::Quat rot = JPH::Quat::sEulerAngles(ToJPH(body->local_rotation));
                compound->AddShape(ToJPH(body->local_center), rot, new JPH::BoxShapeSettings(ToJPH(body->half_extents)));
                break;
            }
            case ModelCollisionShape::Capsule:
            {
                // ULRE 胶囊体沿局部 Z 轴；Jolt 胶囊体原生沿 Y 轴。
                // 绕 X 轴旋转 +90 度把 Y 轴映射到 +Z 轴
                JPH::Quat align_z = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 0.5f * JPH::JPH_PI);
                JPH::Quat rot = JPH::Quat::sEulerAngles(ToJPH(body->local_rotation)) * align_z;
                compound->AddShape(ToJPH(body->local_center), rot, new JPH::CapsuleShapeSettings(body->half_height, body->radius));
                break;
            }
            case ModelCollisionShape::ConvexHull:
            {
                const math::Vector3f *verts = proxy->GetHullVertices(i);
                if (verts && body->hull_vertex_count > 0)
                {
                    JPH::Array<JPH::Vec3> points;
                    points.reserve(body->hull_vertex_count);
                    for (uint32_t v = 0; v < body->hull_vertex_count; ++v)
                    {
                        points.push_back(ToJPH(verts[v]));
                    }
                    compound->AddShape(ToJPH(body->local_center), JPH::Quat::sIdentity(), new JPH::ConvexHullShapeSettings(points));
                }
                break;
            }
            case ModelCollisionShape::None:
            default:
                break;
            }
        }

        JPH::Shape::ShapeResult shapeResult = compound->Create(*impl->temp_allocator);
        if (!shapeResult.IsValid())
        {
            GLogError("[JoltPhysicsSystem] Failed to create shape for Entity %u: %s",
                      entity->GetEntityID(), shapeResult.GetError().c_str());
            return 0xFFFFFFFFu;
        }

        JPH::Ref<JPH::Shape> shape = shapeResult.Get();

        // 初始世界变换
        glm::vec3 world_pos(0.0f);
        glm::quat world_rot = glm::identity<glm::quat>();

        if (w)
        {
            TransformAccessor transform = w->GetTransformByEntity(entity->GetEntityID());
            if (transform.IsValid())
            {
                world_pos = transform.GetWorldPosition();
                world_rot = transform.GetWorldRotation();
            }
        }

        // 运动学与碰撞层匹配
        JPH::ObjectLayer layer = Layers::NON_MOVING;
        if (rb->is_sensor)
        {
            layer = Layers::SENSOR;
        }
        else if (rb->motion_type != BodyMotionType::Static)
        {
            layer = Layers::MOVING;
        }

        JPH::EMotionType motion_type = JPH::EMotionType::Static;
        if (rb->motion_type == BodyMotionType::Kinematic)
            motion_type = JPH::EMotionType::Kinematic;
        else if (rb->motion_type == BodyMotionType::Dynamic)
            motion_type = JPH::EMotionType::Dynamic;

        JPH::BodyCreationSettings settings(shape, ToJPH(world_pos), ToJPH(world_rot), motion_type, layer);
        settings.mFriction = rb->friction;
        settings.mRestitution = rb->restitution;
        settings.mLinearDamping = rb->linear_damping;
        settings.mAngularDamping = rb->angular_damping;
        settings.mGravityFactor = rb->gravity_factor;
        settings.mIsSensor = rb->is_sensor;
        settings.mAllowSleeping = rb->allow_sleeping;
        settings.mUserData = EntityIDToU64(entity->GetEntityID());

        if (rb->motion_type == BodyMotionType::Dynamic && rb->mass > 0.0f)
        {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = rb->mass;
        }

        JPH::BodyInterface &bi = impl->physics_system->GetBodyInterface();
        JPH::EActivation activation = (rb->motion_type == BodyMotionType::Static) ? JPH::EActivation::DontActivate : JPH::EActivation::Activate;
        JPH::BodyID body_id = bi.CreateAndAddBody(settings, activation);

        if (body_id.IsInvalid())
        {
            GLogError("[JoltPhysicsSystem] Failed to CreateAndAddBody for Entity %u", entity->GetEntityID());
            return 0xFFFFFFFFu;
        }

        rb->jolt_body_id = body_id.GetIndexAndSequenceNumber();
        rb->physics_system = this;

        return rb->jolt_body_id;
    }

    void JoltPhysicsSystem::DestroyBody(Entity *entity)
    {
        if (!entity)
            return;
        auto *rb = GetRigidBody(entity);
        if (rb)
            DestroyBody(rb);
    }

    void JoltPhysicsSystem::DestroyBody(RigidBodyComponent *rb)
    {
        if (!rb || !impl || !rb->IsValidBody())
            return;

        JPH::BodyID body_id(rb->jolt_body_id);
        JPH::BodyInterface &bi = impl->physics_system->GetBodyInterface();

        if (bi.IsAdded(body_id))
        {
            bi.RemoveBody(body_id);
        }
        bi.DestroyBody(body_id);

        rb->jolt_body_id = 0xFFFFFFFFu;
        rb->physics_system = nullptr;
    }

    void JoltPhysicsSystem::ActivateBody(uint32_t body_id)
    {
        if (impl && body_id != 0xFFFFFFFFu)
            impl->physics_system->GetBodyInterface().ActivateBody(JPH::BodyID(body_id));
    }

    void JoltPhysicsSystem::DeactivateBody(uint32_t body_id)
    {
        if (impl && body_id != 0xFFFFFFFFu)
            impl->physics_system->GetBodyInterface().DeactivateBody(JPH::BodyID(body_id));
    }

    bool JoltPhysicsSystem::IsBodyActive(uint32_t body_id) const
    {
        if (impl && body_id != 0xFFFFFFFFu)
            return impl->physics_system->GetBodyInterface().IsActive(JPH::BodyID(body_id));
        return false;
    }

    void JoltPhysicsSystem::SetLinearVelocity(uint32_t body_id, const math::Vector3f &velocity)
    {
        if (impl && body_id != 0xFFFFFFFFu)
            impl->physics_system->GetBodyInterface().SetLinearVelocity(JPH::BodyID(body_id), ToJPH(velocity));
    }

    math::Vector3f JoltPhysicsSystem::GetLinearVelocity(uint32_t body_id) const
    {
        if (impl && body_id != 0xFFFFFFFFu)
            return FromJPH(impl->physics_system->GetBodyInterface().GetLinearVelocity(JPH::BodyID(body_id)));
        return math::Vector3f(0.0f);
    }

    void JoltPhysicsSystem::SetAngularVelocity(uint32_t body_id, const math::Vector3f &angular_velocity)
    {
        if (impl && body_id != 0xFFFFFFFFu)
            impl->physics_system->GetBodyInterface().SetAngularVelocity(JPH::BodyID(body_id), ToJPH(angular_velocity));
    }

    math::Vector3f JoltPhysicsSystem::GetAngularVelocity(uint32_t body_id) const
    {
        if (impl && body_id != 0xFFFFFFFFu)
            return FromJPH(impl->physics_system->GetBodyInterface().GetAngularVelocity(JPH::BodyID(body_id)));
        return math::Vector3f(0.0f);
    }

    void JoltPhysicsSystem::AddForce(uint32_t body_id, const math::Vector3f &force)
    {
        if (impl && body_id != 0xFFFFFFFFu)
            impl->physics_system->GetBodyInterface().AddForce(JPH::BodyID(body_id), ToJPH(force));
    }

    void JoltPhysicsSystem::AddImpulse(uint32_t body_id, const math::Vector3f &impulse)
    {
        if (impl && body_id != 0xFFFFFFFFu)
            impl->physics_system->GetBodyInterface().AddImpulse(JPH::BodyID(body_id), ToJPH(impulse));
    }

    void JoltPhysicsSystem::AddTorque(uint32_t body_id, const math::Vector3f &torque)
    {
        if (impl && body_id != 0xFFFFFFFFu)
            impl->physics_system->GetBodyInterface().AddTorque(JPH::BodyID(body_id), ToJPH(torque));
    }

    void JoltPhysicsSystem::AddAngularImpulse(uint32_t body_id, const math::Vector3f &angular_impulse)
    {
        if (impl && body_id != 0xFFFFFFFFu)
            impl->physics_system->GetBodyInterface().AddAngularImpulse(JPH::BodyID(body_id), ToJPH(angular_impulse));
    }

    void JoltPhysicsSystem::SetPositionAndRotation(uint32_t body_id, const math::Vector3f &pos, const glm::quat &rot, bool activate)
    {
        if (impl && body_id != 0xFFFFFFFFu)
        {
            impl->physics_system->GetBodyInterface().SetPositionAndRotation(
                JPH::BodyID(body_id),
                ToJPH(pos),
                ToJPH(rot),
                activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate
            );
        }
    }

    void JoltPhysicsSystem::Update(float deltaTime)
    {
        ECSContext *w = GetWorld();
        if (!w || !impl)
            return;

        JPH::BodyInterface &bi = impl->physics_system->GetBodyInterface();

        // 阶段 1：自动为包含 ModelCollisionProxyComponent 的实体创建或更新刚体
        w->ForEachAliveEntity([this](Entity *entity)
        {
            if (!entity)
                return;

            auto *proxy = GetModelCollisionProxy(entity);
            if (!proxy || !proxy->HasCollision())
                return;

            auto *rb = GetRigidBody(entity);
            if (!rb || !rb->IsValidBody())
            {
                CreateOrUpdateBody(entity);
            }
        });

        // 阶段 2：模拟前变换同步（ECS -> Jolt）
        // Kinematic 刚体推送逻辑位移；静态刚体脏变换同步
        w->ForEachAliveEntity([&bi, this, deltaTime, w](Entity *entity)
        {
            if (!entity)
                return;

            auto *rb = GetRigidBody(entity);
            if (!rb || !rb->IsValidBody())
                return;

            TransformAccessor transform = w->GetTransformByEntity(entity->GetEntityID());
            if (!transform.IsValid())
                return;

            JPH::BodyID bid(rb->jolt_body_id);

            if (rb->motion_type == BodyMotionType::Kinematic)
            {
                glm::vec3 pos = transform.GetWorldPosition();
                glm::quat rot = transform.GetWorldRotation();
                bi.MoveKinematic(bid, ToJPH(pos), ToJPH(rot), deltaTime > 0.0f ? deltaTime : 1.0f / 60.0f);
            }
            else if (rb->motion_type == BodyMotionType::Static)
            {
                if (transform.IsDirty())
                {
                    glm::vec3 pos = transform.GetWorldPosition();
                    glm::quat rot = transform.GetWorldRotation();
                    bi.SetPositionAndRotation(bid, ToJPH(pos), ToJPH(rot), JPH::EActivation::DontActivate);
                }
            }
        });

        // 阶段 3：执行物理模拟
        if (deltaTime > 0.0f)
        {
            const float clampedDeltaTime = std::min(deltaTime, 0.1f);
            constexpr int collisionSteps = 1;
            impl->physics_system->Update(clampedDeltaTime, collisionSteps, impl->temp_allocator.get(), impl->job_system.get());
        }

        // 阶段 4：模拟后变换写回（Jolt -> ECS）
        // Dynamic 刚体将解算后的世界位置与姿态写回 TransformAccessor
        w->ForEachAliveEntity([&bi, this, w](Entity *entity)
        {
            if (!entity)
                return;

            auto *rb = GetRigidBody(entity);
            if (!rb || !rb->IsValidBody())
                return;

            if (rb->motion_type == BodyMotionType::Dynamic)
            {
                JPH::BodyID bid(rb->jolt_body_id);
                if (bi.IsActive(bid))
                {
                    JPH::RVec3 jpos = bi.GetPosition(bid);
                    JPH::Quat jrot = bi.GetRotation(bid);

                    TransformAccessor transform = w->GetTransformByEntity(entity->GetEntityID());
                    if (transform.IsValid())
                    {
                        transform.SetWorldPosition(FromJPH(jpos));
                        transform.SetWorldRotation(FromJPH(jrot));
                    }
                }
            }
        });
    }

    bool JoltPhysicsSystem::Raycast(const math::Vector3f &origin, const math::Vector3f &direction, float max_distance, PhysicsRaycastHit &out_hit) const
    {
        out_hit = PhysicsRaycastHit();
        if (!impl || max_distance <= 0.0f)
            return false;

        JPH::RRayCast ray(ToJPH(origin), ToJPH(direction * max_distance));
        JPH::RayCastResult result;

        if (impl->physics_system->GetNarrowPhaseQuery().CastRay(ray, result))
        {
            out_hit.has_hit = true;
            out_hit.fraction = result.mFraction;
            out_hit.distance = result.mFraction * max_distance;
            out_hit.position = origin + direction * out_hit.distance;
            out_hit.body_id = result.mBodyID.GetIndexAndSequenceNumber();

            uint64_t user_data = impl->physics_system->GetBodyInterface().GetUserData(result.mBodyID);
            out_hit.entity_id = U64ToEntityID(user_data);
            ECSContext *w = GetWorld();
            if (w)
                out_hit.entity = w->GetEntity(out_hit.entity_id);

            JPH::BodyLockRead lock(impl->physics_system->GetBodyLockInterface(), result.mBodyID);
            if (lock.Succeeded())
            {
                const JPH::Body &body = lock.GetBody();
                JPH::Vec3 jnormal = body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ray.GetPointOnRay(result.mFraction));
                out_hit.normal = FromJPH(jnormal);
            }

            return true;
        }

        return false;
    }

    bool JoltPhysicsSystem::RaycastSegment(const math::Vector3f &start, const math::Vector3f &end, PhysicsRaycastHit &out_hit) const
    {
        const math::Vector3f diff = end - start;
        const float len = glm::length(diff);
        if (len < 1e-6f)
            return false;

        return Raycast(start, diff / len, len, out_hit);
    }

    void JoltPhysicsSystem::OverlapSphere(const math::Vector3f &center, float radius, std::vector<PhysicsOverlapHit> &out_hits) const
    {
        out_hits.clear();
        if (!impl || radius <= 0.0f)
            return;

        JPH::SphereShape sphere_shape(radius);
        JPH::CollideShapeSettings settings;
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;

        impl->physics_system->GetNarrowPhaseQuery().CollideShape(
            &sphere_shape,
            JPH::Vec3::sReplicate(1.0f),
            JPH::RMat44::sTranslation(ToJPH(center)),
            settings,
            ToJPH(center),
            collector
        );

        std::unordered_set<uint32_t> seen;
        ECSContext *w = GetWorld();
        for (const auto &hit : collector.mHits)
        {
            const uint32_t bid = hit.mBodyID2.GetIndexAndSequenceNumber();
            if (seen.insert(bid).second)
            {
                PhysicsOverlapHit o;
                o.body_id = bid;
                uint64_t user_data = impl->physics_system->GetBodyInterface().GetUserData(hit.mBodyID2);
                o.entity_id = U64ToEntityID(user_data);
                if (w)
                    o.entity = w->GetEntity(o.entity_id);
                out_hits.push_back(o);
            }
        }
    }

    void JoltPhysicsSystem::OverlapBox(const math::Vector3f &center, const math::Vector3f &half_extents, const glm::quat &rotation, std::vector<PhysicsOverlapHit> &out_hits) const
    {
        out_hits.clear();
        if (!impl || half_extents.x <= 0.0f || half_extents.y <= 0.0f || half_extents.z <= 0.0f)
            return;

        JPH::BoxShape box_shape(ToJPH(half_extents));
        JPH::CollideShapeSettings settings;
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;

        JPH::RMat44 transform = JPH::RMat44::sRotationTranslation(ToJPH(rotation), ToJPH(center));

        impl->physics_system->GetNarrowPhaseQuery().CollideShape(
            &box_shape,
            JPH::Vec3::sReplicate(1.0f),
            transform,
            settings,
            ToJPH(center),
            collector
        );

        std::unordered_set<uint32_t> seen;
        ECSContext *w = GetWorld();
        for (const auto &hit : collector.mHits)
        {
            const uint32_t bid = hit.mBodyID2.GetIndexAndSequenceNumber();
            if (seen.insert(bid).second)
            {
                PhysicsOverlapHit o;
                o.body_id = bid;
                uint64_t user_data = impl->physics_system->GetBodyInterface().GetUserData(hit.mBodyID2);
                o.entity_id = U64ToEntityID(user_data);
                if (w)
                    o.entity = w->GetEntity(o.entity_id);
                out_hits.push_back(o);
            }
        }
    }

    JPH::PhysicsSystem *JoltPhysicsSystem::GetJoltPhysicsSystem()
    {
        return impl ? impl->physics_system.get() : nullptr;
    }

    JPH::BodyInterface *JoltPhysicsSystem::GetJoltBodyInterface()
    {
        return impl ? &impl->physics_system->GetBodyInterface() : nullptr;
    }

    void JoltPhysicsSystem::SetGravity(const math::Vector3f &gravity)
    {
        if (impl)
            impl->physics_system->SetGravity(ToJPH(gravity));
    }

    math::Vector3f JoltPhysicsSystem::GetGravity() const
    {
        if (impl)
            return FromJPH(impl->physics_system->GetGravity());
        return math::Vector3f(0.0f);
    }
}//namespace hgl::ecs

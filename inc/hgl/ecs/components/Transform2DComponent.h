#pragma once

#include<hgl/ecs/core/entity/ComponentRow.h>
#include<hgl/math/Matrix.h>
#include<hgl/math/VectorTypes.h>
#include<hgl/type/RectScope.h>
#include<cstdint>

namespace hgl::ecs
{
    /**
     * Transform2DComponent -- 2D 层级变换组件（Hierarchy2D 系统组的树节点）
     *
     * 坐标系约定：屏幕像素空间，原点在渲染目标左下角，+X 向右、+Y 向上
     * （与 TextComponent 的 start_position 同一套语义，bridge 处直接取整换算）。
     *
     * 树结构用侵入式指针（parent / first_child / next_sibling 全是 Entity*）：
     * 实体住块池、地址永久稳定（T2-a 结构性不变量），因此不需要句柄与间接索引。
     * 挂点缺失（父实体没有 Transform2D）时按根节点处理（orphan 视同 root）。
     *
     * 求值由 Transform2DSystem 每帧从根 DFS 级联完成：
     * 本地属性变化只置 dirty；系统只对 dirty 或父世界已变的子树重算，
     * 世界矩阵未实际变化的分支会截断传播。
     *
     * 字段全部 public（供系统批量遍历零开销）；world_* / tree_order /
     * has_clip / effective_clip 只应由 Transform2DSystem 写，消费方一律走 getter。
     */
    class Transform2DComponent : public ComponentRow
    {
    public:

        // -- 树关系（Entity* 稳定；nullptr = 无父 / 无子 / 尾兄弟） --
        Entity *parent_entity = nullptr;
        Entity *first_child = nullptr;
        Entity *next_sibling = nullptr;

        // -- 本地属性（相对父节点） --
        math::Vector2f local_position{0.0f, 0.0f};  // 锚点位置（父空间像素）
        float    local_rotation = 0.0f;       // 旋转（弧度，绕锚点逆时针）
        math::Vector2f local_scale{1.0f, 1.0f};     // 缩放（逐轴）
        math::Vector2f size{0.0f, 0.0f};            // 设计尺寸（像素，局部矩形 [0,size]）
        math::Vector2f pivot{0.0f, 0.0f};           // 锚点比例 (0..1)：局部矩形的 pivot*size 点对齐到 local_position

        // -- 世界缓存（仅 Transform2DSystem 写） --
        math::Matrix3f   world_matrix{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}; // 局部->世界仿射矩阵（列主序，第三列为平移）
        math::Vector2f   world_position{0.0f, 0.0f};// 世界平移分量（局部原点的世界坐标）
        math::RectScope2f world_bounds;             // 局部矩形四角变换后的 AABB（世界像素）
        math::RectScope2f effective_clip;           // 继承后的有效裁切矩形（世界像素），has_clip 才有效
        bool       has_clip = false;
        uint32_t   tree_order = 0;            // DFS 序（CullAndSort2D 的第三排序键）

        // -- 脏标记：本地属性变过 / 结构变过，等待系统重算 --
        bool dirty = true;

    public:

        Transform2DComponent() = default;

        ~Transform2DComponent() = default;

    public: // -- 树操作（实现在 .cpp） --

        Entity *GetParentEntity() const { return parent_entity; }
        Entity *GetFirstChildEntity() const { return first_child; }
        Entity *GetNextSiblingEntity() const { return next_sibling; }

        bool HasChildren() const { return first_child != nullptr; }

        /**
         * 把本实体挂到 new_parent 之下（new_parent == nullptr 升为根）。
         * 防环：new_parent 是自己或自己的后代时拒绝并返回 false。
         * 成功后本子树全部置 dirty。
         */
        bool SetParent(Transform2DComponent *new_parent);

        /// 从父节点摘下（变根）；原本就是根则无操作
        void DetachFromParent() { SetParent(nullptr); }

        /// 把 child 追加为本节点的最后一个子节点
        bool AddChild(Transform2DComponent *child);

        /// 置脏本节点及全部后代（结构变更后由系统重算子树）
        void MarkSubtreeDirty();

    public: // -- 本地属性访问（setter 置 dirty） --

        const math::Vector2f &GetLocalPosition() const { return local_position; }
        float GetLocalRotation() const { return local_rotation; }
        const math::Vector2f &GetLocalScale() const { return local_scale; }
        const math::Vector2f &GetSize() const { return size; }
        const math::Vector2f &GetPivot() const { return pivot; }

        void SetLocalPosition(const math::Vector2f &v)
        {
            if (v == local_position) return;
            local_position = v;
            dirty = true;
        }

        void SetLocalPosition(float x, float y) { SetLocalPosition(math::Vector2f(x, y)); }

        void SetLocalRotation(float rad)
        {
            if (rad == local_rotation) return;
            local_rotation = rad;
            dirty = true;
        }

        void SetLocalScale(const math::Vector2f &v)
        {
            if (v == local_scale) return;
            local_scale = v;
            dirty = true;
        }

        void SetLocalScale(float s) { SetLocalScale(math::Vector2f(s, s)); }

        void SetSize(const math::Vector2f &v)
        {
            if (v == size) return;
            size = v;
            dirty = true;
        }

        void SetSize(float w, float h) { SetSize(math::Vector2f(w, h)); }

        void SetPivot(const math::Vector2f &v)
        {
            if (v == pivot) return;
            pivot = v;
            dirty = true;
        }

        /// 批量直写本地属性后的统一置脏入口
        void MarkDirty() { dirty = true; }

    public: // -- 世界值读取（消费方接口） --

        const math::Matrix3f &GetWorldMatrix() const { return world_matrix; }
        const math::Vector2f &GetWorldPosition() const { return world_position; }
        const math::RectScope2f &GetWorldBounds() const { return world_bounds; }
        uint32_t GetTreeOrder() const { return tree_order; }

        /// 有效裁切矩形（世界像素）；无裁切返回 false
        bool GetEffectiveClip(math::RectScope2f &out) const
        {
            if (!has_clip) return false;
            out = effective_clip;
            return true;
        }

    public: // -- 求值（由 Transform2DSystem 调用） --

        /**
         * 依据父世界矩阵重算本节点全部世界缓存。
         * parent == nullptr 按根处理。不做 dirty 判断（调用方负责）。
         * @return 世界矩阵是否发生变化（false 时系统可截断子树传播）
         */
        bool RecomputeWorld(const Transform2DComponent *parent);

        /// 置入继承裁切（仅系统内部：父 clip 与 本节点 clip_children 区域的交集）
        void SetInheritedClip(const math::RectScope2f *clip)
        {
            if (clip) { effective_clip = *clip; has_clip = true; }
            else      { has_clip = false; }
        }
    };//class Transform2DComponent

    /// 登记类型号（CPU-only：无 GPU 行、不写 EntityGPU.row[]）。
    template<> struct ComponentTypeOf<Transform2DComponent> { static constexpr ComponentType value = ComponentType::Transform2D; };
}//namespace hgl::ecs
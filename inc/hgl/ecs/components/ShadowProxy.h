#pragma once

#include<hgl/ecs/core/ComponentRow.h>
#include<hgl/ecs/core/Entity.h>
#include<cmath>
#include<string>

namespace hgl::ecs
{
    /**
     * ShadowProxy —— 实体阴影属性控制（A5b：由原阴影组件正名）。
     *
     * 挂载在可渲染实体（拥有 GeometryData）上。若实体未挂载本组件，渲染管线一律采用
     * **全局缺省约定**：可投射、可接收、最大投射距离 0.0f（不限）、倍率 1.0f。
     *
     * 缺省约定的**唯一出处**是下面那组自由函数（CanCastShadow / GetShadowMaxDistance /
     * CanReceiveShadow / GetShadowBiasMultiplier）：原可渲染组件的便利方法随该类
     * 删除后，约定不再有第二份拷贝（本类自身只承载显式配置）。
     */
    class ShadowProxy : public ComponentRow
    {
    private:

        bool  cast_shadow        = true;   ///< 是否投射阴影（默认开启）
        float max_cast_distance  = 0.0f;   ///< 最大投射距离（米；<= 0.0f 代表不限/使用场景全局级联）
        bool  receive_shadow     = true;   ///< 是否接收阴影
        float bias_multiplier    = 1.0f;   ///< 局部 Bias 调节倍率（防止特异薄片物体阴影悬浮/acne）

    public:

        ShadowProxy() = default;

        /// 非虚（T2-c：组件行里没有 vptr）；对象住**类型化对象池**，析构由池显式调用
        ~ShadowProxy() = default;

        // ── 投射控制 (Caster) ──
        bool CanCastShadow() const { return cast_shadow; }
        void SetCastShadow(bool enable) { cast_shadow = enable; }

        float GetMaxDistance() const { return max_cast_distance; }
        void SetMaxDistance(float dist) { max_cast_distance = dist; }

        // ── 接收控制 (Receiver) ──
        bool CanReceiveShadow() const { return receive_shadow; }
        void SetReceiveShadow(bool enable) { receive_shadow = enable; }

        // ── 局部偏差 ──
        float GetBiasMultiplier() const { return bias_multiplier; }
        void SetBiasMultiplier(float multiplier) { bias_multiplier = multiplier; }
    };

    // ── 阴影访问器与缺省约定（**唯一出处**；未挂载 ⇒ 全局缺省）──
    // 参数为所有者实体（nullptr ⇒ 视作未挂载）。世界级访问器见
    ShadowProxy *GetShadowProxy(Entity *owner);
    const ShadowProxy *GetShadowProxy(const Entity *owner);

    /// 缺省：未挂载 ShadowProxy ⇒ 可投射
    bool CanCastShadow(const Entity *owner);
    /// 缺省：未挂载 ShadowProxy ⇒ 最大投射距离 0.0f（不限）
    float GetShadowMaxDistance(const Entity *owner);
    /// 缺省：未挂载 ShadowProxy ⇒ 可接收
    bool CanReceiveShadow(const Entity *owner);
    /// 缺省：未挂载 ShadowProxy ⇒ 倍率 1.0f
    float GetShadowBiasMultiplier(const Entity *owner);

    /// T3h：Bias 的**行字节归一**（唯一出处）：<=0 / 非有限 ⇒ 0.0f（消费侧约定
    /// 「行 0 = 引擎默认 1.0」，见 pcf_shadow.glsl）。collect 事实上行点、台架值链判据
    /// 两处共用本式 ⇒ 无第二口径（原 CPU 批行写入端已随整族删除）。
    inline float NormalizeShadowBiasToRow(float bias)
    {
        return (bias > 0.0f && std::isfinite(bias)) ? bias : 0.0f;
    }

    /// T2-c：登记类型号（**CPU-only**：无 GPU 行、不写 `EntityGPU.row[]`）。
    template<> struct ComponentTypeOf<ShadowProxy> { static constexpr ComponentType value = ComponentType::ShadowProxy; };
}//namespace hgl::ecs

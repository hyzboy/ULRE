// Hierarchy2DShowcase —— 2D World 层级管理基础框架演示
//
// 覆盖点：
//   1) Transform2D 父子级联：旋转根节点，子/孙文本随动（TextRenderPipeline bridge 同步世界坐标）
//   2) Layer2D 层内序：两块重叠文本每 2 秒交换 order_in_layer，CullAndSort2D 表序实时反映
//   3) 父容器裁切：clip_children 容器的 effective_clip 下发到摆动的子节点
//   4) 持久 HUD：每帧按需 SetText（文本不变则不改写）
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/TextComponent.h>
#include<hgl/ecs/components/Transform2DComponent.h>
#include<hgl/ecs/components/Layer2DComponent.h>
#include<hgl/ecs/support/hierarchy2d/CullAndSort2DSystem.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/graph/font/TextCharSSBO.h>
#include<hgl/color/Color4ub.h>
#include<hgl/framework/WorkManager.h>
#include<cmath>

using namespace hgl;
using namespace hgl::ecs;
using namespace hgl::graph;

class Hierarchy2DShowcaseApp:public WorkObject
{
private:

    ECSContext *    world       = nullptr;
    FontSource *    font        = nullptr;

    // 演示句柄（实体由 world 持有，指针稳定）
    Transform2DComponent *  root_t2     = nullptr;  // 旋转根
    Transform2DComponent *  child_a_t2  = nullptr;  // 根之子
    Transform2DComponent *  grand_t2    = nullptr;  // 根之孙
    Transform2DComponent *  clip_t2     = nullptr;  // 裁切容器
    Transform2DComponent *  pend_t2     = nullptr;  // 在裁切盒里外摆动的子节点
    Layer2DComponent *      pair_hi     = nullptr;  // 重叠对 A
    Layer2DComponent *      pair_lo     = nullptr;  // 重叠对 B
    TextComponent *         hud         = nullptr;  // 每帧刷新的信息面板

    double elapsed = 0.0;
    U16String last_hud;

    /// 建一个带 T2+Layer2D+Text 的 2D 文本节点；parent 为空则为根
    TextComponent *MakeLabel(const U16String &content, const math::Vector2f &pos,
                             uint32_t color, Transform2DComponent *parent,
                             Transform2DComponent **out_t2,
                             Layer2DComponent **out_layer,
                             uint16_t layer_id, uint16_t order)
    {
        Entity *entity = world->CreateEntity();
        if(!entity) return nullptr;

        Transform2DComponent *t2 = entity->AddComponent<Transform2DComponent>();
        Layer2DComponent *layer  = entity->AddComponent<Layer2DComponent>();
        TextComponent *tc        = entity->AddComponent<TextComponent>();
        if(!t2 || !layer || !tc) return nullptr;

        t2->SetLocalPosition(pos);
        layer->SetLayer(layer_id, order);

        tc->SetText(content);
        tc->SetFontSource(font);

        hgl::graph::layout::CharStyle cs{};
        cs.text_color = color;
        tc->SetCharStyle(cs);

        if(parent && !parent->AddChild(t2))
            return nullptr;

        if(out_t2)    *out_t2    = t2;
        if(out_layer) *out_layer = layer;
        return tc;
    }

public:

    bool Init() override
    {
        world = GetECSContext();
        if(!world) return false;

        font = CreateFontSource(OS_TEXT("微软雅黑"), 24);
        if(!font) return false;

        const uint32_t WHITE  = HGL_U8_TO_RGBA8(255,255,255,255);
        const uint32_t YELLOW = HGL_U8_TO_RGBA8(255,220,  80,255);
        const uint32_t CYAN   = HGL_U8_TO_RGBA8( 80,220,255,255);
        const uint32_t GREEN  = HGL_U8_TO_RGBA8(120,255,120,255);
        const uint32_t RED    = HGL_U8_TO_RGBA8(255,100,100,255);

        // ── 1) 旋转根 + 子 + 孙：改父变换，子树文本全部随动 ──
        MakeLabel(U16_TEXT("ROOT（绕中心旋转）"), math::Vector2f(420.0f, 620.0f), YELLOW,
                  nullptr, &root_t2, nullptr, 0, 10);
        if(root_t2)
        {
            root_t2->SetSize(420.0f, 220.0f);
            root_t2->SetPivot(math::Vector2f(0.5f, 0.5f));
        }

        MakeLabel(U16_TEXT("CHILD-A（局部 30,150）"), math::Vector2f(30.0f, 150.0f), WHITE,
                  root_t2, &child_a_t2, nullptr, 0, 11);
        MakeLabel(U16_TEXT("GRAND-A1（再嵌一层）"), math::Vector2f(40.0f, 70.0f), GREEN,
                  child_a_t2, &grand_t2, nullptr, 0, 12);

        // ── 2) 重叠对：每 2 秒交换 order_in_layer（层内序换遮挡次序）──
        MakeLabel(U16_TEXT("OVERLAP-1（黄）"), math::Vector2f(1180.0f, 700.0f), YELLOW,
                  nullptr, nullptr, &pair_hi, 1, 10);
        MakeLabel(U16_TEXT("OVERLAP-2（青）"), math::Vector2f(1210.0f, 680.0f), CYAN,
                  nullptr, nullptr, &pair_lo, 1, 20);

        // ── 3) 裁切容器 + 摆动子节点：父 clip 下发到子树 ──
        MakeLabel(U16_TEXT("CLIP 容器（320x80）"), math::Vector2f(1180.0f, 300.0f), CYAN,
                  nullptr, &clip_t2, nullptr, 2, 10);
        if(clip_t2)
        {
            clip_t2->SetSize(320.0f, 80.0f);

            if(Layer2DComponent *cl = clip_t2->GetOwner()
                ? clip_t2->GetOwner()->GetComponent<Layer2DComponent>() : nullptr)
            {
                cl->SetClipChildren(true);   // clip_rect 空 → 用容器 world_bounds
            }
        }
        MakeLabel(U16_TEXT("我在裁切盒里外摆动 ~~~~"), math::Vector2f(-160.0f, 20.0f), RED,
                  clip_t2, &pend_t2, nullptr, 2, 11);

        // ── 4) HUD（高图层，文本不变则不重写）──
        hud = MakeLabel(U16_TEXT("Hierarchy2D Showcase"), math::Vector2f(60.0f, 940.0f), WHITE,
                        nullptr, nullptr, nullptr, 9, 0);

        return true;
    }

    void Tick(double delta_time) override
    {
        elapsed += delta_time;

        // 旋转根：±0.35 rad 摆动
        if(root_t2)
            root_t2->SetLocalRotation(0.35f * sin(elapsed * 0.8));

        // 摆动的裁切子节点：在容器局部 x ∈ [-160, 380] 之间来回
        if(pend_t2)
        {
            const float x = 110.0f + 270.0f * sin(elapsed * 0.7);
            pend_t2->SetLocalPosition(x, 20.0f);
        }

        // 每 2 秒交换重叠对层内序
        if(pair_hi && pair_lo)
        {
            const bool swap = (static_cast<int>(elapsed / 2.0) & 1) != 0;
            pair_hi->SetLayer(1, swap ? 20 : 10);
            pair_lo->SetLayer(1, swap ? 10 : 20);
        }

        // HUD：文本变化才写，避免每帧无谓置脏
        U16String text = U16_TEXT("== 2D Hierarchy ==");
        text += U16_TEXT("\nrot%=") + U16String::numberOf(static_cast<int>(100.0 * sin(elapsed * 0.8)));

        if(root_t2)
        {
            const math::Vector2f &wp = root_t2->GetWorldPosition();
            text += U16_TEXT("\nroot world=(") + U16String::numberOf(static_cast<int>(wp.x))
                  + U16_TEXT(",") + U16String::numberOf(static_cast<int>(wp.y)) + U16_TEXT(")");
        }

        if(grand_t2)
            text += U16_TEXT("\ngrand tree_order=") + U16String::numberOf(grand_t2->GetTreeOrder());

        if(pend_t2)
        {
            math::RectScope2f clip;
            text += U16_TEXT("\npendant has_clip=")
                  + U16String(pend_t2->GetEffectiveClip(clip) ? U16_TEXT("true") : U16_TEXT("false"));
            if(pend_t2->GetEffectiveClip(clip))
                text += U16_TEXT(" (") + U16String::numberOf(static_cast<int>(clip.Left))
                      + U16_TEXT(",") + U16String::numberOf(static_cast<int>(clip.Top))
                      + U16_TEXT(",") + U16String::numberOf(static_cast<int>(clip.Width))
                      + U16_TEXT("x") + U16String::numberOf(static_cast<int>(clip.Height)) + U16_TEXT(")");
        }

        if(auto cull = world->GetSystem<CullAndSort2DSystem>())
        {
            const auto &entries = cull->GetSortedEntries();
            text += U16_TEXT("\ncull entries=") + U16String::numberOf(static_cast<int>(entries.size()));
            if(!entries.empty())
                text += U16_TEXT("\nfront key=") + U16String::numberOf(static_cast<int>(entries.back().sort_key));
        }

        if(hud && text != last_hud)
        {
            last_hud = text;
            hud->SetText(std::move(text));
        }
    }
};//class Hierarchy2DShowcaseApp:public WorkObject

int os_main(int argc, os_char **argv)
{
    return RunFramework<Hierarchy2DShowcaseApp>(OS_TEXT("Hierarchy2DShowcase"), argc, argv, 1600, 1000);
}
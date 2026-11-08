// Sprite2DShowcase —— Sprite2DComponent + Sprite2DSyncSystem 演示
//
// 覆盖点：
//   1) 最小精灵：挂 Transform2D + Sprite2D 即出图（纹理来自配方默认图 lena）
//   2) 纹理覆盖：SetTexture 换成运行时加载的 PeachHouse 纹理
//   3) 层级随动：父子精灵，父节点每帧移动，子精灵 world_bounds 级联
//   4) 尺寸缩放：每帧 SetSize 呼吸缩放，TRS 按新 AABB 重算
// 限制：精灵本体不随 2D 旋转而旋转（TRS 只携带平移+缩放）。
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/Transform2DComponent.h>
#include<hgl/ecs/components/Sprite2DComponent.h>
#include<hgl/ecs/components/TextComponent.h>
#include<hgl/ecs/components/Layer2DComponent.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/framework/WorkManager.h>
#include<cmath>

using namespace hgl;
using namespace hgl::ecs;
using namespace hgl::graph;

class Sprite2DShowcaseApp:public WorkObject
{
private:

    ECSContext *world = nullptr;

    Transform2DComponent *mover_t2 = nullptr;   // 父子演示：摆动的父节点
    Sprite2DComponent    *mover_sp = nullptr;
    Sprite2DComponent    *child_sp = nullptr;   // 父子演示：挂在其下的子精灵

    double elapsed = 0.0;

    /// 建一个 2D 精灵节点（T2 + Sprite2D，可选纹理 URI）；parent 为空则为根
    Sprite2DComponent *MakeSprite(const math::Vector2f &pos, float w, float h,
                                  const OSString &tex_uri, Transform2DComponent *parent)
    {
        Entity *entity = world->CreateEntity();
        if(!entity) return nullptr;

        Transform2DComponent *t2 = entity->AddComponent<Transform2DComponent>();
        Sprite2DComponent    *sp = entity->AddComponent<Sprite2DComponent>();
        if(!t2 || !sp) return nullptr;

        t2->SetSize(w, h);
        t2->SetLocalPosition(pos);

        if(!tex_uri.IsEmpty()) sp->SetTextureURI(tex_uri);

        if(parent && !parent->AddChild(t2)) return nullptr;

        return sp;
    }

public:

    bool Init() override
    {
        world = GetECSContext();
        if(!world) return false;

        // ── 1) 默认图精灵：配方 res/image/lena.Tex2D，左上区域 ──
        if(!MakeSprite(math::Vector2f(120.0f, 620.0f), 256.0f, 256.0f, OSString(), nullptr))
            return false;

        // ── 2) 覆盖图精灵：直接声明 URI，走全 ECS 异步流式加载，右上区域 ──
        const OSString peach_uri = OS_TEXT("res/image/PeachHouse.Tex2D");
        MakeSprite(math::Vector2f(900.0f, 620.0f), 256.0f, 256.0f, peach_uri, nullptr);

        // ── 3) 父子随动：父精灵（PeachHouse 异步加载）水平摆动，子精灵（默认 lena）随动 ──
        mover_sp = MakeSprite(math::Vector2f(400.0f, 260.0f), 192.0f, 192.0f, peach_uri, nullptr);
        if(mover_sp)
        {
            mover_t2 = mover_sp->GetOwner()->GetComponent<Transform2DComponent>();
            child_sp = MakeSprite(math::Vector2f(60.0f, 200.0f), 96.0f, 96.0f,
                                  OSString(), mover_t2);   // 子用默认 lena 以示区别
        }

        // ── 4) 呼吸缩放精灵：中间底部，每帧 SetSize ──
        scaler_sp = MakeSprite(math::Vector2f(1100.0f, 180.0f), 200.0f, 200.0f, peach_uri, nullptr);

        // ── HUD：说明文字 ──
        if(FontSource *font = CreateFontSource(OS_TEXT("微软雅黑"), 22))
        {
            Entity *hud_e = world->CreateEntity();
            if(hud_e)
            {
                auto *t2  = hud_e->AddComponent<Transform2DComponent>();
                auto *ly  = hud_e->AddComponent<Layer2DComponent>();
                auto *tc  = hud_e->AddComponent<TextComponent>();
                if(t2 && ly && tc)
                {
                    t2->SetLocalPosition(math::Vector2f(60.0f, 940.0f));
                    tc->SetText(U16_TEXT("Sprite2D：默认图 | 纯 URI 异步流送覆盖 | 父子随动 | 呼吸缩放"));
                    tc->SetFontSource(font);
                }
            }
        }

        return true;
    }

    void Tick(double delta_time) override
    {
        elapsed += delta_time;

        // 父子演示：父精灵在 x ∈ [300, 620] 摆动，子精灵随之平移
        if(mover_t2)
            mover_t2->SetLocalPosition(460.0f + 160.0f * sin(elapsed * 0.9f), 260.0f);

        // 呼吸缩放：尺寸 160..240 正弦变化，同步系统按新 AABB 重写 TRS
        if(scaler_sp)
        {
            const float s = 200.0f + 40.0f * sin(elapsed * 1.6f);
            if(Transform2DComponent *t2 = scaler_sp->GetOwner()->GetComponent<Transform2DComponent>())
                t2->SetSize(s, s);
        }

        // 尾调基类：WorkObject::Tick 转发 ECSContext::Tick，
        // 2D 层级求值/精灵同步等 tick 相位系统由此驱动（覆写不调基类会全停）。
        WorkObject::Tick(delta_time);
    }

private:

    Sprite2DComponent *scaler_sp = nullptr;     // 呼吸缩放精灵
};//class Sprite2DShowcaseApp:public WorkObject

int os_main(int argc, os_char **argv)
{
    return RunFramework<Sprite2DShowcaseApp>(OS_TEXT("Sprite2DShowcase"), argc, argv, 1600, 1000);
}
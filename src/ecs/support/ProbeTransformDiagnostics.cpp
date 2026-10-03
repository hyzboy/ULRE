/**
 * ProbeTransformDiagnostics.cpp - 变换体系诊断探针（只读；不建窗口、不建 GPU 资源）
 *
 * 对应 doc/future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md 的 T0 / T4：
 *
 *   [T0] 量化基线
 *        结构尺寸（ComponentRow / GeometryData / TransformAccessor / Entity / TransformDataStorage）、
 *        TransformDataStorage 每行字节，以及 N 个 [实体 + 变换行] 的
 *        堆净增块数/字节（CRT 堆快照的**有符号**逐类别差值）——用来替换文档里
 *        "48B 的 TRS 膨胀至 120~160 字节" 那组估算。
 *
 *   [T4] 证据（非均匀缩放 / 镜像下的旋转提取）
 *        对 M = T * R * S 比较几种写法：
 *          (a) 现状写法  glm::quat_cast(glm::mat3(M))            ← LoadScene.cpp:198 同款
 *          (b) 既有引擎函数 math::DecomposeTransform(M, t, r, s)
 *          (c) 列归一化后 quat_cast（不含镜像分支）
 *          (d) 正确镜像约定（保留 c0、把负号放进 S.x）
 *        判据以「由 (t,r,s) 重建的矩阵与 M 的最大元素误差」为准（角度误差仅作参考：
 *        镜像无法只用旋转表达，任何约定只要重建一致就是合法分解）。
 *
 * 运行：build/out/Windows_64_Debug/ProbeTransformDiagnostics.exe
 * 退出码 0 = 全部测量完成。本探针不做通过/失败判定，数值即结论。
 */

#include<cstdio>
#include<cmath>
#include<cstddef>
#include<vector>
#include<filesystem>

#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/quaternion.hpp>

#include<hgl/math/Matrix.h>
#include<hgl/ecs/support/TransformDataStorage.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/GeometryData.h>   // [T0b] AddComponent<GeometryData> 需完整类型
#include<hgl/ecs/components/ShadowProxy.h>
#include<hgl/ecs/support/EntityGPUTable.h>  // [T0c] T3b 实体行表自检    // [T2-c] 行尺寸对照（壳拆掉后每组件只剩载荷）
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/EntityGPU.h>
#include<hgl/ecs/support/TransformAccessor.h>

#ifdef _MSC_VER
#include<crtdbg.h>
#endif

using namespace hgl;
using namespace hgl::ecs;

namespace
{
    /// 两个四元数之间的夹角（度）；忽略符号（q 与 -q 表示同一旋转）
    double AngleDeg(const glm::quat &a,const glm::quat &b)
    {
        float d=std::fabs(glm::dot(glm::normalize(a),glm::normalize(b)));
        if(d>1.0f)
            d=1.0f;

        return static_cast<double>(glm::degrees(2.0f*std::acos(d)));
    }

    /// 两个矩阵对应元素的最大绝对差
    double MaxAbsDiff(const glm::mat4 &a,const glm::mat4 &b)
    {
        double m=0.0;

        for(int c=0;c<4;++c)
            for(int r=0;r<4;++r)
            {
                const double d=std::fabs((double)a[c][r]-(double)b[c][r]);
                if(d>m)
                    m=d;
            }

        return m;
    }

    /// 用分解结果重建矩阵（**不**归一化四元数：非单位四元数本身就是错的证据）
    glm::mat4 Rebuild(const glm::vec3 &t,const glm::quat &r,const glm::vec3 &s)
    {
        return glm::translate(glm::mat4(1.0f),t)
             * glm::mat4_cast(r)
             * glm::scale(glm::mat4(1.0f),s);
    }

    void PrintSeparator(const char *title)
    {
        printf("\n================ %s ================\n",title);
    }
}//namespace

// ─────────────────────────────────────────────────────────────────────────────
// [T0] 结构尺寸
//
// T2-c 后：`sizeof(Component)`（104 B）不再是量 —— 组件本体住世界的**类型化对象池**，
// 实体只持 `component_rows[ComponentType]`（1 基行号）。每组件成本 = 池行字节（摊薄）+ 0 次分配。
// ─────────────────────────────────────────────────────────────────────────────
static void SectionSizes()
{
    PrintSeparator("[T0] 结构尺寸");

    printf("sizeof(hgl::ecs::EntityID)            = %zu B\n",sizeof(EntityID));
    // T2-c：`Component` 基类已整删（104 B 中 95% 是名字字符串 + 虚表 + 控制块相关的壳）
    // ⇒ 这里对照打印"行头"与两个小载荷组件的行尺寸（[T0b] 的差值才是 T2-c 的验收判据）。
    printf("sizeof(hgl::ecs::ComponentRow)        = %zu B (T2-c 行头：宿主元数据，无 vptr/无名字)\n",sizeof(ComponentRow));
    printf("sizeof(hgl::ecs::GeometryData)        = %zu B (行载荷)\n",sizeof(GeometryData));
    printf("sizeof(hgl::ecs::ShadowProxy)         = %zu B (行载荷)\n",sizeof(ShadowProxy));
    printf("sizeof(hgl::ecs::TransformAccessor)   = %zu B\n",sizeof(TransformAccessor));
    printf("sizeof(hgl::ecs::Entity)              = %zu B\n",sizeof(Entity));
    printf("sizeof(hgl::ecs::TransformDataStorage)= %zu B\n",sizeof(TransformDataStorage));
    printf("sizeof(MaterialRuntimeSlot)           = %zu B (B5-b 行宽冻结：static_assert 88)\n",sizeof(hgl::ecs::MaterialRuntimeSlot));
    printf("sizeof(hgl::ecs::EntityGPU)           = %zu B (T1 定稿：88 B 载荷 + 40 B 填充，alignas(64))\n",sizeof(hgl::ecs::EntityGPU));
    printf("alignof(hgl::ecs::EntityGPU)          = %zu B\n",alignof(hgl::ecs::EntityGPU));
    printf("  offsetof: flags=%zu type=%zu row=%zu work_flags=%zu (期望 0/4/20/84)\n",
           offsetof(hgl::ecs::EntityGPU,flags),offsetof(hgl::ecs::EntityGPU,type),
           offsetof(hgl::ecs::EntityGPU,row),offsetof(hgl::ecs::EntityGPU,work_flags));

    // 每行字节账目：由**存储自己**列出（TransformDataStorage::GetPerRowFields），
    // 探针不再手工维护字段清单 —— 新增每行数组只需改存储里那一处，探针与测试自动跟上。
    TransformDataStorage probe_storage;      // 只为取账目，不 Allocate（零成本）

    // 让"各数组元素数 == 行数"这条不变量检查有实义（0 行时恒真）：分配两行并结算一次拓扑
    probe_storage.Allocate();
    probe_storage.Allocate();
    probe_storage.UpdateDirtyWorldMatricesFlat();

    const uint32_t per_row    = probe_storage.PerRowBytes();
    const uint32_t per_row_derived = probe_storage.DerivedRowBytes();

    printf("\nTransformDataStorage 每行字节（存储自列 %u 条平行数组，不含 level_offsets / entity_rows）：\n",
           TransformDataStorage::PER_ROW_FIELD_COUNT);

    for (const auto &f : probe_storage.GetPerRowFields())
        printf("  %-13s %3u B/行\n", f.name, f.bytes);

    printf("  => 每行合计 %u B（其中 local_mat4 + world_mat4 = %u B 是可讨论的派生/缓存部分）\n",
           per_row, per_row_derived);
    printf("  => 新增每行字段后重跑本探针即可；各数组元素数 == 行数：%s\n",
           probe_storage.FindPerRowCountMismatch() ? "否 ✗（漏在 Allocate/Deallocate 里同步）" : "是");
}

// ─────────────────────────────────────────────────────────────────────────────
// [T0] 堆分配实测
// ─────────────────────────────────────────────────────────────────────────────
static void SectionAllocation()
{
    PrintSeparator("[T0] 堆分配实测：ECSContext + N 个 [Entity + Transform(Static)]");

#ifdef _MSC_VER
    struct Result
    {
        uint32_t  n;
        long long blocks;
        long long bytes;
    };

    std::vector<Result> results;

    for(const uint32_t n:{1000u,10000u})
    {
        ECSContext context;

        std::vector<Entity *> entities;
        entities.reserve(n);                 // 预留在 checkpoint 之前，避免污染统计
        uint32_t addr_mismatch = 0;    // T2-a：块池扩容不得移动已发出的 Entity*

        // 预热：触发引擎侧一次性惰性分配（不计入统计）
        for(int i=0;i<8;++i)
        {
            auto *e=context.CreateEntity<Entity>("Warmup");
            auto  t=context.GetTransform(context.CreateTransform(e->GetEntityID(),Mobility::Static));
            t.SetLocalPosition(glm::vec3(0.0f));
        }

        _CrtMemState before {},after {};
        _CrtMemCheckpoint(&before);

        for(uint32_t i=0;i<n;++i)
        {
            auto *e=context.CreateEntity<Entity>("T");
            auto  t=context.GetTransform(context.CreateTransform(e->GetEntityID(),Mobility::Static));
            t.SetLocalPosition(glm::vec3(static_cast<float>(i),0.0f,0.0f));
            entities.push_back(e);

            // T2-a 不变量：块池扩容不得移动已发出的 Entity*（每 64 个边界复核第 0 个）
            if(i==0 || (i%64)==0)
            {
                if(context.GetEntity(entities[0]->GetEntityID())!=entities[0])
                    ++addr_mismatch;
            }
        }

        _CrtMemCheckpoint(&after);

        // 有符号逐类别求差：CRT 的 _CrtMemDifference 在类别计数下降时按无符号下溢
        // （实测会打印出 1518 次分配/实体这种不可能的数字），这里自己按 signed 算
        long long net_blocks=0,net_bytes=0;

        for(int i=0;i<_MAX_BLOCKS;++i)
        {
            const long long db=(long long)after.lCounts[i]-(long long)before.lCounts[i];
            const long long ds=(long long)after.lSizes[i] -(long long)before.lSizes[i];

            net_blocks+=db;
            net_bytes +=ds;

            if(db||ds)
                printf("   cat[%d]: 块 %+lld  字节 %+lld\n",i,db,ds);
        }

        results.push_back({n,net_blocks,net_bytes});

        printf("  n=%-6u 净增块=%-7lld 净增字节=%-10lld => %7.1f B/实体, %6.3f 块/实体\n",
               n,net_blocks,net_bytes,
               (double)net_bytes/(double)n,(double)net_blocks/(double)n);

        printf("  T2-a 地址稳定性：每 64 个边界复核 Entity* 地址，mismatch=%u（期望 0）\n",addr_mismatch);

    }

    // ── [T0c] T3b：实体行表（EntityGPUTable）与实体按类型行号表对拍 ──
    //   [T0]/[T0b] 用 10000 实体量堆开销，已超实体行表容量（4096）⇒ 上方那条
    //   EntityGPUTable 超容量报错是**预期的 fail-fast**；本段用 n=512 验证表本身。
    {
        const uint32_t n=512;

        ECSContext context;

        std::vector<Entity *> entities;
        entities.reserve(n);

        for(uint32_t i=0;i<n;++i)
        {
            auto *e=context.CreateEntity<Entity>("T3bProbe");

            context.GetTransform(context.CreateTransform(e->GetEntityID(),Mobility::Static));
            e->AddComponent<GeometryData>();
            entities.push_back(e);
        }

        const EntityGPUTable &table=context.GetEntityGPUTable();

        uint32_t active=0;
        uint32_t first_bad=UINT32_MAX;
        uint32_t bad_type=UINT32_MAX;
        uint32_t bad_row=UINT32_MAX;

        for(uint32_t i=0;i<n;++i)
        {
            const EntityGPU *row=table.Row(entities[i]->GetEntityID());

            if(!row){ first_bad=i; break; }

            if(EntityGPUTable::IsActive(*row))
                ++active;

            if(!table.VerifyAgainst(entities[i],&bad_type,&bad_row)){ first_bad=i; break; }
        }

        // 组件全卸 ⇒ 行号表归零、实体行同步归零（卸载路径）
        entities[0]->DetachAllComponents();
        const EntityGPU *detached_row=table.Row(entities[0]->GetEntityID());
        const bool detach_cleared=detached_row
                                  && detached_row->row[uint32_t(ComponentType::Geometry)]==0
                                  && detached_row->type[uint32_t(ComponentType::Geometry)]==0;

        // 实体销毁 ⇒ Active 清零（生死路径）
        const EntityID dead=entities[n-1]->GetEntityID();
        context.DestroyEntity(dead);
        const EntityGPU *dead_row=table.Row(dead);
        const bool dead_cleared=dead_row && !EntityGPUTable::IsActive(*dead_row);

        printf("\n================ [T0c] 实体行表（T3b）================\n");
        printf("  行尺寸=%zu B（期望 128，alignas(64)）、容量=%u、整表=%zu KB\n",
               sizeof(EntityGPU),EntityGPUTable::kCapacity,
               size_t(EntityGPUTable::kCapacity)*sizeof(EntityGPU)/1024);
        printf("  n=%u 槽在用=%u（期望 %u）\n",n,active,n);
        if(first_bad==UINT32_MAX)
            printf("  逐字段对拍（type[]/row[] vs Entity::component_rows[]）：全部一致\n");
        else
            printf("  逐字段对拍：不一致（首个不符 idx=%u type=%u row=%u）\n",first_bad,bad_type,bad_row);
        printf("  组件全卸后行归零=%s、实体销毁后 Active 清零=%s\n",
               detach_cleared?"是":"否",dead_cleared?"是":"否");
        printf("  注：[T0]/[T0b] 的 10000 实体场景超 4096 容量 ⇒ 上方那条超容量报错是预期 fail-fast。\n");

        // T3c-2a：脏行上传计划（CPU 侧语义：无脏 ⇒ 0 行；改一行 ⇒ 恰好 1 行）
        std::vector<uint32_t> plan;
        EntityGPUTable &mut_table=context.GetEntityGPUTable();

        mut_table.ClearDirty();
        mut_table.CollectDirtyRows(plan);
        const size_t plan_clean=plan.size();

        context.SetEntityVisible(entities[1]->GetEntityID(),false);
        mut_table.CollectDirtyRows(plan);
        const size_t plan_one=plan.size();
        const bool plan_one_ok=(plan_one==1 && !plan.empty() && plan[0]==entities[1]->GetEntityID().index);

        mut_table.ClearDirty();
        mut_table.CollectDirtyRows(plan);

        printf("  脏行计划：无脏=%zu 行（期望 0）、改 1 个实体可见性=%zu 行（期望 1，且正是该实体槽=%s）、清脏后=%zu 行（期望 0）\n",
               plan_clean,plan_one,plan_one_ok?"是":"否",plan.size());

        // T3c-2b：静态段的**机理**判据（纯数据；无需设备/系统）——
        //   静态实体的行号是**常量**，写者每帧重下发它**不该**产生脏行（值变才标脏），
        //   这正是"静态只在首帧上行一次"的根据；公式 `1 + slot` 由变换侧写者负责（真世界）。
        mut_table.ClearDirty();
        mut_table.SetTransformRow(entities[0]->GetEntityID(),7);
        const uint32_t tr_dirty_first=mut_table.GetDirtyCount();

        mut_table.ClearDirty();
        mut_table.SetTransformRow(entities[0]->GetEntityID(),7);      // 同值重复下发
        const uint32_t tr_dirty_same=mut_table.GetDirtyCount();

        mut_table.SetTransformRow(entities[0]->GetEntityID(),8);      // 值变
        const uint32_t tr_dirty_changed=mut_table.GetDirtyCount();
        mut_table.ClearDirty();

        printf("  静态行号重下发：首写脏=%u（期望 1）、同值重下发脏=%u（期望 0=静态零重传依据）、值变后脏=%u（期望 1）\n",
               tr_dirty_first,tr_dirty_same,tr_dirty_changed);


        // T3c-2b（B1）：`row[Transform]` 的写者是**变换侧**（`TransformSystem::WriteEntityTransformRows`）。
        // 本探针是**裸世界**（无系统、无设备 ⇒ 无环布局）⇒ 这里**没有写者**，行号保持 0 属**预期**；
        // 该判据改到**真世界**验（示例：可动实体行号非 0 且随帧槽变化），不在裸世界做假绿。
    }

    if(results.size()==2)
    {
        const double dn=(double)results[1].n-(double)results[0].n;
        const double db=(double)results[1].bytes-(double)results[0].bytes;
        const double dk=(double)results[1].blocks-(double)results[0].blocks;

        printf("\n  边际成本（n=10000 相对 n=1000，扣除固定开销）：%.1f B/实体, %.3f 块/实体\n",
               db/dn,dk/dn);
    }

    // ── [T0b] T2-c 的量化基准 ────────────────────────────────────────────────
    // [T0] 只建 Transform ⇒ **量不到组件对象本身的成本**。这里同 n、同流程，只多挂一个
    // GeometryData ⇒ 与 [T0] 同 n 的差值 = 每个组件对象的真实开销（= T2-c 要消灭的目标）。
    {
        const uint32_t n=10000;

        ECSContext context;

        for(int i=0;i<8;++i)                        // 预热（同 [T0]，排除一次性惰性分配）
        {
            auto *e=context.CreateEntity<Entity>("WarmupG");
            context.GetTransform(context.CreateTransform(e->GetEntityID(),Mobility::Static));
            e->AddComponent<GeometryData>();
        }

        _CrtMemState before {},after {};
        _CrtMemCheckpoint(&before);

        for(uint32_t i=0;i<n;++i)
        {
            auto *e=context.CreateEntity<Entity>("TG");
            context.GetTransform(context.CreateTransform(e->GetEntityID(),Mobility::Static));
            e->AddComponent<GeometryData>();
        }

        _CrtMemCheckpoint(&after);

        long long nb=0,nbytes=0;

        for(int i=0;i<_MAX_BLOCKS;++i)
        {
            nb     += (long long)after.lCounts[i]-(long long)before.lCounts[i];
            nbytes += (long long)after.lSizes[i] -(long long)before.lSizes[i];
        }

        double base_b=0.0,base_k=0.0;

        if(!results.empty())
        {
            base_b=(double)results.back().bytes /(double)results.back().n;
            base_k=(double)results.back().blocks/(double)results.back().n;
        }

        printf("[T0b] [Entity + Transform + GeometryData] n=%u => %7.1f B/实体, %6.3f 块/实体\n"
               "        ├ 对照 [T0] 同 n：%7.1f B/实体, %6.3f 块/实体\n"
               "        └ 差（= 每个 GeometryData 组件的成本，T2-c 的靶子）：%+7.1f B, %+6.3f 块\n",
               n,(double)nbytes/(double)n,(double)nb/(double)n,
               base_b,base_k,
               (double)nbytes/(double)n-base_b,(double)nb/(double)n-base_k);
    }
#else
    printf("  (跳过：需要 MSVC Debug CRT 的 _CrtMemCheckpoint)\n");
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// [T4] 非均匀缩放 / 镜像下的旋转提取
// ─────────────────────────────────────────────────────────────────────────────
static void SectionRotationExtraction()
{
    PrintSeparator("[T4] 非均匀缩放 / 镜像下的旋转提取");

    const glm::vec3 axis=glm::normalize(glm::vec3(0.3f,0.8f,0.5f));
    const glm::quat R=glm::angleAxis(glm::radians(35.0f),axis);
    const glm::vec3 T(1.0f,2.0f,3.0f);

    struct Case
    {
        const char *name;
        glm::vec3   scale;
    };

    const Case cases[]=
    {
        { "uniform       S=(1,1,1)",    glm::vec3( 1.0f,1.0f,1.0f) },
        { "non-uniform   S=(2,0.5,1)",  glm::vec3( 2.0f,0.5f,1.0f) },
        { "mirror(det<0) S=(-2,0.5,1)", glm::vec3(-2.0f,0.5f,1.0f) },
    };

    printf("参考：R = 35° around (%.3f,%.3f,%.3f)，T = (%.1f,%.1f,%.1f)\n",
           axis.x,axis.y,axis.z,T.x,T.y,T.z);
    printf("判据：重建矩阵与 M 的最大元素误差（≈0 = 该分解合法）；角度误差仅作参考。\n");

    for(const auto &c:cases)
    {
        const glm::mat4 M=glm::translate(glm::mat4(1.0f),T)
                         *glm::mat4_cast(R)
                         *glm::scale(glm::mat4(1.0f),c.scale);

        const glm::mat3 m3(M);
        const glm::vec3 c0(m3[0]),c1(m3[1]),c2(m3[2]);
        const float l0=glm::length(c0),l1=glm::length(c1),l2=glm::length(c2);
        const bool  mirrored=glm::determinant(m3)<0.0f;

        printf("\n  [%s]  det(mat3)=%+.4f  列模长=(%.3f,%.3f,%.3f)\n",
               c.name,glm::determinant(m3),l0,l1,l2);

        // (a) 现状写法：LoadScene.cpp:197-202 的三行
        {
            const glm::vec3 t = glm::vec3(M[3]);   // 注意：写成 vec3 t(vec3(M[3])) 会被 MSVC 当成函数声明（most vexing parse）
            const glm::vec3 s(l0,l1,l2);
            const glm::quat q=glm::quat_cast(m3);

            printf("    (a) 现状 quat_cast(mat3(M))          |q|=%.6f 角度误差=%8.3f° 重建误差=%.6g\n",
                   (double)glm::length(q),AngleDeg(q,R),MaxAbsDiff(M,Rebuild(t,q,s)));
        }

        // (b) 既有引擎函数
        {
            math::Vector3f dt;
            math::Quatf    dr;
            math::Vector3f ds;

            math::DecomposeTransform(M,dt,dr,ds);

            printf("    (b) math::DecomposeTransform         |q|=%.6f 角度误差=%8.3f° 重建误差=%.6g"
                   "   还原 T=(%.3f,%.3f,%.3f) S=(%.3f,%.3f,%.3f)\n",
                   (double)glm::length(dr),AngleDeg(dr,R),MaxAbsDiff(M,Rebuild(dt,dr,ds)),
                   dt.x,dt.y,dt.z,ds.x,ds.y,ds.z);
        }

        // (c) 列归一化后 quat_cast，但不处理镜像
        {
            const glm::quat q=glm::normalize(glm::quat_cast(glm::mat3(c0/l0,c1/l1,c2/l2)));

            printf("    (c) 列归一化 quat_cast（无镜像分支） |q|=%.6f 角度误差=%8.3f° 重建误差=%.6g\n",
                   (double)glm::length(q),AngleDeg(q,R),
                   MaxAbsDiff(M,Rebuild(T,q,glm::vec3(l0,l1,l2))));
        }

        // (d) 正确镜像约定：保留 c0，负号放进 S.x（det<0 时）
        {
            glm::vec3 s(l0,l1,l2);
            if(mirrored)
                s.x=-l0;

            const glm::quat q=glm::quat_cast(glm::mat3(c0/s.x,c1/s.y,c2/s.z));

            printf("    (d) 保留 c0 + S.x 取负（镜像约定）  |q|=%.6f 角度误差=%8.3f° 重建误差=%.6g"
                   "   S=(%.3f,%.3f,%.3f)\n",
                   (double)glm::length(q),AngleDeg(q,R),MaxAbsDiff(M,Rebuild(T,q,s)),
                   s.x,s.y,s.z);
        }
    }

    printf("\n  读法：(a) 非均匀缩放下 |q|>1 且重建误差大 ⇒ 现状写法确实错；\n"
           "        (b) 镜像行 |q|≠1 ⇒ math::DecomposeTransform 的镜像分支有缺陷；\n"
           "        (d) 重建误差≈0 ⇒ 该约定合法，可作修法基准。\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// 备注：示例级（资产）验证的前置条件
// ─────────────────────────────────────────────────────────────────────────────
static void SectionAssetNote()
{
    PrintSeparator("资产备注");

    const char *path="res/ABeautifulGame.StaticMesh/ABeautifulGame.Scene.scene";
    const bool  exists=std::filesystem::exists(path);

    printf("LoadScene 加载路径：%s -> %s\n",path,exists?"存在":"不存在");
    if(!exists)
        printf("  （本检出未包含该场景资产：example/Geometry/LoadScene/LoadScene.cpp 的资产级验证需要先"
               "用 GLTFConvert 由 res/model/*.glb 生成 .scene，或补回该资产）\n");
}

int main(int argc,char **argv)
{
    (void)argc;
    (void)argv;

    printf("ProbeTransformDiagnostics - ULRE transform baseline probe\n");

    SectionSizes();
    SectionAllocation();
    SectionRotationExtraction();
    SectionAssetNote();

    printf("\n=== Probe finished ===\n");
    return 0;
}

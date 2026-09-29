/**
 * ProbeTransformDiagnostics.cpp - 变换体系诊断探针（只读；不建窗口、不建 GPU 资源）
 *
 * 对应 doc/future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md 的 T0 / T4：
 *
 *   [T0] 量化基线
 *        结构尺寸（Component / TransformComponent / Entity / TransformDataStorage）、
 *        TransformDataStorage 每行字节，以及 N 个 [实体 + TransformComponent] 的
 *        堆净增块数/字节（CRT 堆快照的**有符号**逐类别差值）——用来替换文档里
 *        "48B 的 TRS 膨胀至 120~160 字节" 那组估算。
 *
 *   [T4] 证据（非均匀缩放 / 镜像下的旋转提取）
 *        对 M = T * R * S 比较几种写法：
 *          (a) 现状写法  glm::quat_cast(glm::mat3(M))            ← SceneTest.cpp:198 同款
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
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>

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
// ─────────────────────────────────────────────────────────────────────────────
static void SectionSizes()
{
    PrintSeparator("[T0] 结构尺寸");

    printf("sizeof(hgl::ecs::EntityID)            = %zu B\n",sizeof(EntityID));
    printf("sizeof(hgl::ecs::Component)           = %zu B\n",sizeof(Component));
    printf("sizeof(hgl::ecs::TransformComponent)  = %zu B\n",sizeof(TransformComponent));
    printf("sizeof(hgl::ecs::Entity)              = %zu B\n",sizeof(Entity));
    printf("sizeof(hgl::ecs::TransformDataStorage)= %zu B\n",sizeof(TransformDataStorage));

    const size_t per_row_positions        =sizeof(glm::vec3);
    const size_t per_row_rotations        =sizeof(glm::quat);
    const size_t per_row_scales           =sizeof(glm::vec3);
    const size_t per_row_local_matrices   =sizeof(glm::mat4);
    const size_t per_row_parent_indices   =sizeof(uint32_t);
    const size_t per_row_world_matrices   =sizeof(glm::mat4);
    const size_t per_row_hierarchy_depths =sizeof(uint16_t);
    const size_t per_row_eval_order       =sizeof(uint32_t);
    const size_t per_row_local_dirty      =sizeof(uint8_t);
    const size_t per_row_matrix_dirty     =sizeof(uint8_t);
    const size_t per_row_mobility         =sizeof(uint8_t);

    const size_t per_row=per_row_positions+per_row_rotations+per_row_scales
                        +per_row_local_matrices+per_row_parent_indices
                        +per_row_world_matrices+per_row_hierarchy_depths
                        +per_row_eval_order+per_row_local_dirty
                        +per_row_matrix_dirty+per_row_mobility;

    printf("\nTransformDataStorage 每行字节（各平行数组元素之和，不含 level_offsets）：\n");
    printf("  positions %zu | rotations %zu | scales %zu | local_mat4 %zu | parent %zu\n",
           per_row_positions,per_row_rotations,per_row_scales,per_row_local_matrices,per_row_parent_indices);
    printf("  world_mat4 %zu | depth %zu | eval_order %zu | local_dirty %zu | matrix_dirty %zu | mobility %zu\n",
           per_row_world_matrices,per_row_hierarchy_depths,per_row_eval_order,
           per_row_local_dirty,per_row_matrix_dirty,per_row_mobility);
    printf("  => 每行合计 %zu B（其中 local_mat4 + world_mat4 = %zu B 是可讨论的派生/缓存部分）\n",
           per_row,per_row_local_matrices+per_row_world_matrices);
}

// ─────────────────────────────────────────────────────────────────────────────
// [T0] 堆分配实测
// ─────────────────────────────────────────────────────────────────────────────
static void SectionAllocation()
{
    PrintSeparator("[T0] 堆分配实测：ECSContext + N 个 [Entity + TransformComponent(Static)]");

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

        // 预热：触发引擎侧一次性惰性分配（不计入统计）
        for(int i=0;i<8;++i)
        {
            auto *e=context.CreateEntity<Entity>("Warmup");
            auto  t=e->AddComponent<TransformComponent>(Mobility::Static);
            t->SetLocalPosition(glm::vec3(0.0f));
        }

        _CrtMemState before {},after {};
        _CrtMemCheckpoint(&before);

        for(uint32_t i=0;i<n;++i)
        {
            auto *e=context.CreateEntity<Entity>("T");
            auto  t=e->AddComponent<TransformComponent>(Mobility::Static);
            t->SetLocalPosition(glm::vec3(static_cast<float>(i),0.0f,0.0f));
            entities.push_back(e);
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
    }

    if(results.size()==2)
    {
        const double dn=(double)results[1].n-(double)results[0].n;
        const double db=(double)results[1].bytes-(double)results[0].bytes;
        const double dk=(double)results[1].blocks-(double)results[0].blocks;

        printf("\n  边际成本（n=10000 相对 n=1000，扣除固定开销）：%.1f B/实体, %.3f 块/实体\n",
               db/dn,dk/dn);
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

        // (a) 现状写法：SceneTest.cpp:197-202 的三行
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

    printf("SceneTest 加载路径：%s -> %s\n",path,exists?"存在":"不存在");
    if(!exists)
        printf("  （本检出未包含该场景资产：example/Geometry/LoadScene/SceneTest.cpp 的资产级验证需要先"
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

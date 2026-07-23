#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/color/Color.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/DescriptorBindingSet.h>
#include"GizmoResource.h"

namespace hgl::graph
{
    //bool InitGizmoScaleMesh();
    //void ClearGizmoScaleMesh();
    //
    //bool InitGizmoRotateMesh();
    //void ClearGizmoRotateMesh();

    namespace
    {
        GeometryVertexFormat CreateGizmoGeometryVertexFormat()
        {
            GeometryVertexFormat gvf;
            gvf.Add(VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT, 3, sizeof(float) * 3);
            return gvf;
        }

        static GraphicsContext *graphics_context=nullptr;
        static RenderPass *gizmo_render_pass=nullptr;
        static MaterialManager *gizmo_mtl_manager=nullptr;

        struct GizmoResource
        {
            Material *          mtl;
            const VIL *         binding_vil;
            DescriptorBindingSet *binding_sets[size_t(GizmoColor::RANGE_SIZE)];
            Pipeline *          pipeline;
            DeviceBuffer *      mi_ssbo;
            VertexDataManager * vdm;

            GeometryCreater *  prim_creater;
        };

        static GizmoResource    gizmo_triangle{};

        struct GizmoMesh
        {
            Geometry *geometry;
            Primitive *primitive;

        public:

            void Create(Geometry *p)
            {
                geometry=p;
                if (graphics_context)
                {
                    auto *primitive_manager = graphics_context->GetPrimitiveManager();
                    auto *dbs = gizmo_triangle.binding_sets[0];
                    primitive = primitive_manager ? primitive_manager->CreatePrimitive(geometry, dbs, gizmo_triangle.pipeline) : nullptr;
                    if (!primitive)
                        GLogError("[GizmoResource] CreatePrimitive failed for descriptor binding path");
                }
                else
                {
                    primitive = nullptr;
                }
            }

            void Clear()
            {
                primitive=nullptr;
                geometry=nullptr;
            }
        };//class GizmoMesh

        GizmoMesh         gizmo_mesh[size_t(GizmoShape::RANGE_SIZE)]{};

        void InitGizmoMesh(const GizmoShape &gs,Geometry *geometry)
        {
            if(!geometry)
                return;

            gizmo_mesh[size_t(gs)].Create(geometry);
        }

        // Create the SSBO holding one color-struct per GizmoColor slot.
        // slot_index = color enum value (0=Black, 1=White, 2=Red, …)
        bool InitColorSSBO(GizmoResource *gr)
        {
            if (!gr || !gr->mtl)
                return false;

            if (!graphics_context)
                return false;

            auto *buffer_manager = graphics_context->GetBufferManager();
            auto *domain_manager = graphics_context->GetResourceDomainManager();
            if (!buffer_manager || !domain_manager)
                return false;

            const uint32_t stride = gr->mtl->GetMIDataBytes();
            if (stride == 0)
                return true;

            const uint32_t color_count = uint32_t(GizmoColor::RANGE_SIZE);
            const VkDeviceSize ssbo_size = VkDeviceSize(color_count) * stride;

            gr->mi_ssbo = buffer_manager->CreateSSBO("GizmoResource:PureColor3D:MIData", ssbo_size, nullptr, SharingMode::Exclusive);
            if (!gr->mi_ssbo)
                return false;

            auto *gpu_buf = gr->mi_ssbo->GetGPUBuffer();
            if (!gpu_buf)
                return false;

            auto *dst = static_cast<uint8_t *>(gpu_buf->Map(0, ssbo_size));
            if (!dst)
                return false;

            memset(dst, 0, static_cast<size_t>(ssbo_size));

            // Write Color4f directly at each slot — no need for scratch MaterialInstance.
            // PureColor3D MI data is exactly one Color4f; if stride has alignment padding
            // the extra bytes remain zero (already memset above).
            const uint32_t copy_bytes = hgl_min(stride, static_cast<uint32_t>(sizeof(Color4f)));
            for (uint32_t i = 0; i < color_count; ++i)
            {
                const Color4f color = GetColor4f(gizmo_color[i], 1.0f);
                memcpy(dst + VkDeviceSize(i) * stride, &color, copy_bytes);
            }

            gpu_buf->Unmap();

            bool has_struct_binding = false;
            for (const auto &req : gr->mtl->GetBindingContract().requirements)
            {
                if (req.semantic != mtl::DescriptorSemantic::MaterialInstance)
                    continue;

                has_struct_binding = true;
                const mtl::SSBOAddress addr{req.ssbo_type, req.ssbo_id, 0};
                if (!domain_manager->RegisterBuffer(addr, gr->mi_ssbo, color_count))
                    return false;

                // Create one DescriptorBindingSet per color, each pointing at the same
                // SSBO but with a different slot_index.
                gr->binding_vil = gr->mtl->GetDefaultVIL();
                for (uint32_t c = 0; c < color_count; ++c)
                {
                    gr->binding_sets[c] = new DescriptorBindingSet(gr->mtl, gr->binding_vil);
                    if (!gr->binding_sets[c])
                        return false;
                    gr->binding_sets[c]->SetSSBOBinding(req.ssbo_type, req.ssbo_id, c);
                }
            }

            return has_struct_binding;
        }

        bool InitGizmoResource3D()
        {
            if(!graphics_context)
                return(false);

            VulkanDevice *device=graphics_context->GetDevice();
            auto *buffer_manager = graphics_context->GetBufferManager();
            RenderPass *render_pass=gizmo_render_pass;

            if(!device || !render_pass)
                return(false);

            gizmo_mtl_manager=graphics_context->GetMaterialManager();

            {
                mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);

                cfg.local_to_world=true;
                cfg.material_instance=true;

                gizmo_triangle.mtl=gizmo_mtl_manager->CreateMaterial(mtl::MaterialPreset::PureColor3D,&cfg);
                if(!gizmo_triangle.mtl)
                    return(false);

                gizmo_triangle.mtl->Update();
            }

            {
                const GeometryVertexFormat gizmo_gvf = CreateGizmoGeometryVertexFormat();

                gizmo_triangle.pipeline=render_pass->CreatePipeline(
                    gizmo_triangle.mtl,
                    gizmo_triangle.mtl->GetDefaultVIL(),
                    InlinePipeline::GizmoOverlay3D,
                    false,
                    &gizmo_gvf);
                if(!gizmo_triangle.pipeline)
                    return(false);

                if(!InitColorSSBO(&gizmo_triangle))
                    return(false);

                gizmo_triangle.vdm=new VertexDataManager(
                    buffer_manager,
                    gizmo_gvf);

                if(!gizmo_triangle.vdm)
                    return(false);

                if(!gizmo_triangle.vdm->Init(   HGL_SIZE_1MB,       //最大顶点数量
                                                HGL_SIZE_1MB,       //最大索引数量
                                                IndexType::U16))    //索引类型
                    return(false);
            }

            {
                gizmo_triangle.prim_creater=new GeometryCreater(gizmo_triangle.vdm);

                if(!gizmo_triangle.prim_creater)
                    return(false);
            }

            {
                using namespace inline_geometry;

                {
                    InitGizmoMesh(GizmoShape::Square,CreatePlaneSqaure(gizmo_triangle.prim_creater));
                }

                {
                    CircleCreateInfo cci;

                    cci.center=math::Vector2f(0,0);
                    cci.radius=math::Vector2f(0.5,0.5);
                    cci.field_count=16;
                    cci.has_center=false;

                    InitGizmoMesh(GizmoShape::Circle,CreateCircle3DByIndexTriangles(gizmo_triangle.prim_creater,&cci));
                }

                {
                    CubeCreateInfo cci;

                    cci.normal=true;
                    cci.tangent=false;
                    cci.tex_coord=false;

                    InitGizmoMesh(GizmoShape::Cube,CreateCube(gizmo_triangle.prim_creater,&cci));
                }

                {
                    InitGizmoMesh(GizmoShape::Sphere,CreateSphere(gizmo_triangle.prim_creater,16));
                }

                {
                    ConeCreateInfo cci;

                    cci.radius      =GIZMO_CONE_RADIUS;         //圆锥半径
                    cci.halfExtend  =1;                 //圆锤一半高度
                    cci.numberSlices=16;        //圆锥底部分割数
                    cci.numberStacks=3;         //圆锥高度分割数

                    InitGizmoMesh(GizmoShape::Cone,CreateCone(gizmo_triangle.prim_creater,&cci));
                }

                {
                    struct CylinderCreateInfo cci;

                    cci.halfExtend  =1;         //圆柱一半高度
                    cci.numberSlices=16;        //圆柱底部分割数
                    cci.radius      =1;         //圆柱半径

                    InitGizmoMesh(GizmoShape::Cylinder,CreateCylinder(gizmo_triangle.prim_creater,&cci));
                }

                {
                    struct TorusCreateInfo tci;

                    tci.innerRadius=0.975;
                    tci.outerRadius=1.0;
                    tci.numberSlices=64;
                    tci.numberStacks=8;

                    InitGizmoMesh(GizmoShape::Torus,CreateTorus(gizmo_triangle.prim_creater,&tci));
                }

                ENUM_CLASS_FOR(GizmoShape,int,i)
                {
                    if(!gizmo_mesh[i].geometry)
                        return(false);
                }
            }

            return(true);
        }
    }//namespace

    bool InitGizmoResource(GraphicsContext *gc, RenderPass *rp)
    {
        if(!gc)
            return(false);

        graphics_context=gc;
        gizmo_render_pass=rp;

        VulkanDevice *device=graphics_context->GetDevice();
        if(!device)
            return(false);

        if(!InitGizmoResource3D())
            return(false);

        return(true);
    }

    void FreeGizmoResource()
    {
        for(GizmoMesh &gr:gizmo_mesh)
            gr.Clear();

        SAFE_CLEAR(gizmo_triangle.prim_creater);
        SAFE_CLEAR(gizmo_triangle.vdm);
        SAFE_CLEAR(gizmo_triangle.mi_ssbo);

        for (size_t i = 0; i < size_t(GizmoColor::RANGE_SIZE); ++i)
        {
            delete gizmo_triangle.binding_sets[i];
            gizmo_triangle.binding_sets[i] = nullptr;
        }
        gizmo_triangle.binding_vil = nullptr;

        gizmo_triangle.pipeline = nullptr;
        gizmo_triangle.mtl = nullptr;

        gizmo_mtl_manager = nullptr;
        gizmo_render_pass = nullptr;
        graphics_context = nullptr;
    }

    DescriptorBindingSet *GetGizmoBindingSet3D(const GizmoColor &color)
    {
        RANGE_CHECK_RETURN_NULLPTR(color)
        return gizmo_triangle.binding_sets[size_t(color)];
    }

    // Legacy alias kept for backward compatibility — returns nullptr in new path.
    MaterialInstance *GetGizmoMI3D(const GizmoColor &)
    {
        return nullptr;
    }

    Primitive *GetGizmoMeshPrimitive(const GizmoShape &shape)
    {
        if(!graphics_context)
            return nullptr;

        RANGE_CHECK_RETURN_NULLPTR(shape)

        return gizmo_mesh[size_t(shape)].primitive;
    }
}//namespace hgl::graph

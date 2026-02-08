#pragma once

#include<hgl/type/String.h>

#define STD_MTL_NAMESPACE_BEGIN namespace hgl::shader_schema::mtl{
#define STD_MTL_NAMESPACE_END   }

#define STD_MTL_NAMESPACE hgl::shader_schema::mtl
#define STD_MTL_NAMESPACE_USING using namespace STD_MTL_NAMESPACE;

#define STD_MTL_FUNC_NAMESPACE_BEGIN namespace hgl::shader_schema::mtl::func{
#define STD_MTL_FUNC_NAMESPACE_END   }

#define STD_MTL_FUNC_NAMESPACE hgl::shader_schema::mtl::func
#define STD_MTL_FUNC_NAMESPACE_USING using namespace STD_MTL_FUNC_NAMESPACE;

namespace hgl::graph
{
	class ShaderCreateInfoVertex;
	class ShaderCreateInfoGeometry;
	class ShaderCreateInfoFragment;
	struct VulkanDevAttr;
}

namespace hgl::shader_schema
{
	namespace mtl
	{
		enum class WithSky:uint8
		{
			Without=0,
			With
		};

		enum class WithCamera:uint8
		{
			Without=0,
			With
		};

		enum class WithLocalToWorld:uint8
		{
			Without=0,
			With
		};

		class MaterialCreateInfo;
		struct MaterialCreateConfig;

		class StdMaterial
		{
		protected:

			MaterialCreateInfo *mci;

		protected:

			virtual bool BeginCustomShader(){return true;/*some work before create shader*/};

			virtual bool CustomVertexShader(hgl::graph::ShaderCreateInfoVertex *)=0;
			virtual bool CustomGeometryShader(hgl::graph::ShaderCreateInfoGeometry *){return false;}
			virtual bool CustomFragmentShader(hgl::graph::ShaderCreateInfoFragment *)=0;

			virtual bool EndCustomShader(){return true;/*some work after create shader*/};

		public:

			StdMaterial(const MaterialCreateConfig *);
			virtual ~StdMaterial()=default;

			virtual MaterialCreateInfo *Create(const hgl::graph::VulkanDevAttr *dev_attr);
		};//class StdMaterial
	}//namespace mtl
}//namespace hgl::shader_schema

// Backward compatibility aliases for hgl::graph
namespace hgl::graph::mtl
{
	using hgl::shader_schema::mtl::WithSky;
	using hgl::shader_schema::mtl::WithCamera;
	using hgl::shader_schema::mtl::WithLocalToWorld;
	using hgl::shader_schema::mtl::MaterialCreateInfo;
	using hgl::shader_schema::mtl::MaterialCreateConfig;
	using hgl::shader_schema::mtl::StdMaterial;
}//namespace hgl::graph::mtl

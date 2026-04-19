#pragma once

#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/vk/VertexAttribDataAccess.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/GeometryBuilder.h>
#include<hgl/graph/geo/IndexGenerator.h>
#include"GeometryValidator.h"
#include<algorithm>
#include<cmath>

namespace hgl::graph::inline_geometry
{
	/**
	 * 校验 CreateInfo 请求的属性格式是否与 GeometryVertexFormat 一致。
	 * - requested_format == VK_FORMAT_UNDEFINED: 不校验，表示不要求该语义
	 * - requested_format != VK_FORMAT_UNDEFINED: 要求该语义存在且格式一致
	 */
	inline bool ValidateRequestedAttribFormat(GeometryCreater *pc, const VertexAttrib attrib, const VkFormat requested_format)
	{
		if(!pc)
			return false;

		if(requested_format == VK_FORMAT_UNDEFINED)
			return true;

		return pc->GetVAB(attrib, requested_format) != nullptr;
	}
}

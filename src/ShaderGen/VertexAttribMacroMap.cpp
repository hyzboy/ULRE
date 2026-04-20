#include <hgl/shadergen/VertexAttribMacroMap.h>
#include <array>
#include <cctype>
#include <string>

namespace hgl::graph {

namespace
{
	constexpr size_t kVertexAttribCount = static_cast<size_t>(VertexAttrib::RANGE_SIZE);

	std::array<std::string, kVertexAttribCount> BuildVertexAttribMacroTable()
	{
		std::array<std::string, kVertexAttribCount> table{};

		for(size_t i = 0; i < kVertexAttribCount; ++i)
		{
			const auto attrib = static_cast<VertexAttrib>(i);
			const char* name = GetVertexAttribName(attrib);

			if(!name || !name[0])
				continue;

			std::string macro = "HAS_";

			for(const unsigned char ch : std::string(name))
				macro.push_back(static_cast<char>(std::toupper(ch)));

			table[i] = std::move(macro);
		}

		return table;
	}

	const std::array<std::string, kVertexAttribCount>& GetVertexAttribMacroTable()
	{
		static const std::array<std::string, kVertexAttribCount> table = BuildVertexAttribMacroTable();
		return table;
	}
}

const char* GetVertexAttribMacroName(const VertexAttrib va)
{
	if(va < VertexAttrib::Position || va >= VertexAttrib::RANGE_SIZE)
		return nullptr;

	const auto index = static_cast<size_t>(va);
	const auto& table = GetVertexAttribMacroTable();
	const std::string& macro = table[index];

	return macro.empty() ? nullptr : macro.c_str();
}

void EmitVertexAttribDefine(ShaderWriter& writer, const VertexAttrib attrib)
{
	if(const char* macro = GetVertexAttribMacroName(attrib))
		writer.EmitDefine(macro);
}

void EmitVertexAttribDefines(ShaderWriter& writer, const uint32_t attrib_mask)
{
	for(uint8 i = 0; i < static_cast<uint8>(VertexAttrib::RANGE_SIZE); ++i)
	{
		if(attrib_mask & (1u << i))
			EmitVertexAttribDefine(writer, static_cast<VertexAttrib>(i));
	}
}
}

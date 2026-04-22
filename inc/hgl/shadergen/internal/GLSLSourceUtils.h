#pragma once

#include <cstddef>
#include <string>

namespace hgl::graph::internal {

/// Returns the byte offset past any UTF-8 BOM and leading whitespace characters.
size_t SkipBOMAndLeadingWhitespace(const std::string &source);

/// Returns the byte offset immediately after the '\n' that ends the #version line.
/// Skips any UTF-8 BOM and leading whitespace before the directive.
/// Returns std::string::npos if no #version directive is found.
size_t FindVersionDirectiveLineEnd(const std::string &source);

/// Inserts "\n" + injection + "\n" after the #version line.
/// If no #version line is found, prepends injection + "\n" before the source.
/// Returns source unchanged if injection is empty.
std::string InjectAfterVersion(const std::string &source, const std::string &injection);

/// Returns the first max_lines lines of source as a substring.
/// Returns source unchanged if it has fewer lines than max_lines.
std::string BuildGLSLPreviewFirstLines(const std::string &source, size_t max_lines);

/// Opens the file at path and reads its full text content into out_content.
/// Returns false and sets out_error on failure.
bool ReadTextFile(const std::string &path, std::string &out_content, std::string &out_error);

} // namespace hgl::graph::internal

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
代码文件格式化脚本
功能：
1. 扫描目录下所有的 .h/.cpp 文件
2. 转换为 UTF-8 BOM 格式（非UTF格式认为是GB18030/GBK/GB2312）
3. 转换为 UNIX 换行格式
4. 将 TAB 转换为 4 个空格
5. 去掉行尾空格
6. 根据 .gitignore 跳过不需要处理的目录和文件
"""

import os
import sys
import re
from pathlib import Path
from typing import Set, List


class GitignoreParser:
    """解析和处理 .gitignore 文件"""

    def __init__(self, gitignore_path: str):
        self.patterns = []
        self.root_dir = os.path.dirname(gitignore_path)
        self._parse_gitignore(gitignore_path)

    def _parse_gitignore(self, gitignore_path: str):
        """解析 .gitignore 文件"""
        if not os.path.exists(gitignore_path):
            return

        with open(gitignore_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                line = line.rstrip('\n\r')
                # 去掉注释和空行
                if not line or line.startswith('#'):
                    continue

                # 移除前后空格
                line = line.strip()
                if not line:
                    continue

                self.patterns.append(line)

    def should_ignore(self, path: str) -> bool:
        """检查路径是否应该被忽略"""
        rel_path = os.path.relpath(path, self.root_dir)
        rel_path = rel_path.replace('\\', '/')

        for pattern in self.patterns:
            if self._match_pattern(rel_path, pattern):
                return True

        return False

    def _match_pattern(self, path: str, pattern: str) -> bool:
        """匹配 gitignore 模式"""
        # 移除开头的 /
        if pattern.startswith('/'):
            pattern = pattern[1:]

        # 如果模式以 / 结尾，只匹配目录
        is_dir_pattern = pattern.endswith('/')
        if is_dir_pattern:
            pattern = pattern[:-1]

        # 处理简单的通配符和路径匹配
        path_parts = path.split('/')

        # 精确匹配目录或文件
        if is_dir_pattern:
            return pattern in path_parts
        else:
            # 匹配完整路径或最后一个部分
            if path == pattern:
                return True
            if path.startswith(pattern + '/'):
                return True
            if path_parts[-1] == pattern:
                return True

        return False


class CodeFormatter:
    """代码文件格式化类"""

    def __init__(self, root_dir: str):
        self.root_dir = root_dir
        gitignore_path = os.path.join(root_dir, '.gitignore')
        self.gitignore = GitignoreParser(gitignore_path)
        self.processed_count = 0
        self.skipped_count = 0
        self.error_count = 0

    def format_directory(self):
        """递归格式化目录下的所有 .h 和 .cpp 文件"""
        print(f"开始扫描目录: {self.root_dir}")

        for root, dirs, files in os.walk(self.root_dir):
            # 过滤需要忽略的目录
            dirs[:] = [d for d in dirs if not self.gitignore.should_ignore(os.path.join(root, d))]

            for file in files:
                if file.endswith(('.h', '.cpp')):
                    file_path = os.path.join(root, file)

                    if self.gitignore.should_ignore(file_path):
                        print(f"跳过 (gitignore): {os.path.relpath(file_path, self.root_dir)}")
                        self.skipped_count += 1
                        continue

                    self.format_file(file_path)

    def format_file(self, file_path: str):
        """格式化单个文件"""
        try:
            # 读取文件
            content = self._read_file(file_path)

            if content is None:
                self.error_count += 1
                return

            # 格式化内容
            formatted = self._format_content(content)

            # 写回文件
            self._write_file(file_path, formatted)

            rel_path = os.path.relpath(file_path, self.root_dir)
            print(f"✓ 处理: {rel_path}")
            self.processed_count += 1

        except Exception as e:
            rel_path = os.path.relpath(file_path, self.root_dir)
            print(f"✗ 错误: {rel_path} - {str(e)}")
            self.error_count += 1

    def _read_file(self, file_path: str) -> str:
        """读取文件，自动检测编码"""
        # 尝试不同的编码，优先使用 utf-8-sig 以自动去除已有的 BOM
        encodings = ['utf-8-sig', 'utf-8', 'gb2312', 'gbk', 'gb18030', 'cp936', 'latin-1']

        for encoding in encodings:
            try:
                with open(file_path, 'r', encoding=encoding) as f:
                    content = f.read()

                # 循环去除所有的 BOM 字符（修复双重或多重 BOM 问题）
                while content.startswith('\ufeff'):
                    content = content[1:]

                return content
            except (UnicodeDecodeError, UnicodeError):
                continue

        print(f"无法读取文件 (编码检测失败): {os.path.relpath(file_path, self.root_dir)}")
        return None

    def _format_content(self, content: str) -> str:
        """格式化文件内容"""
        # 1. 将 TAB 按对齐规则转换为空格（tab size = 4）
        content = self._expand_tabs(content, tab_size=4)

        # 2. 转换为 UNIX 换行格式 (CRLF -> LF)
        content = content.replace('\r\n', '\n')
        content = content.replace('\r', '\n')

        # 3. 去掉行尾空格和制表符
        lines = content.split('\n')
        # 显式移除行尾的空格和 TAB（避免混淆），同时保留 rstrip() 可移除的其它空白如必要
        lines = [line.rstrip(' \t') for line in lines]
        content = '\n'.join(lines)

        # 4. 确保文件以新行结尾
        if content and not content.endswith('\n'):
            content += '\n'

        return content

    def _expand_tabs(self, content: str, tab_size: int = 4) -> str:
        """按列对齐展开制表符为若干空格。

        每遇到一个 TAB，填充到下一个 tab stop（tab_size 的倍数列）。
        这样例如当 tab_size=4 时，如果当前列为 2，再遇到一个 TAB，
        则只插入 2 个空格以对齐到列 4。
        """
        lines = content.split('\n')
        out_lines = []

        for line in lines:
            new_chars = []
            col = 0
            for ch in line:
                if ch == '\t':
                    spaces = tab_size - (col % tab_size)
                    new_chars.append(' ' * spaces)
                    col += spaces
                else:
                    new_chars.append(ch)
                    col += 1
            out_lines.append(''.join(new_chars))

        return '\n'.join(out_lines)

    def _write_file(self, file_path: str, content: str):
        """写入文件，使用 UTF-8 BOM 编码"""
        with open(file_path, 'w', encoding='utf-8-sig') as f:
            f.write(content)

    def print_summary(self):
        """打印处理总结"""
        print(f"\n{'='*50}")
        print(f"处理完成!")
        print(f"  已处理: {self.processed_count} 个文件")
        print(f"  已跳过: {self.skipped_count} 个文件")
        print(f"  错误数: {self.error_count} 个")
        print(f"{'='*50}")


def main():
    """主函数"""
    # 使用脚本所在目录的父目录作为扫描根目录
    if len(sys.argv) > 1:
        root_dir = sys.argv[1]
    else:
        # 默认使用脚本所在目录作为根目录
        root_dir = os.path.dirname(os.path.abspath(__file__))

    if not os.path.isdir(root_dir):
        print(f"错误: 目录不存在 - {root_dir}")
        sys.exit(1)

    # 询问用户确认
    print(f"将扫描目录: {root_dir}")
    print("将执行以下操作:")
    print("  1. 转换为 UTF-8 BOM 格式")
    print("  2. 转换为 UNIX 换行格式")
    print("  3. TAB 转换为 4 个空格")
    print("  4. 去掉行尾空格")
    print("  5. 根据 .gitignore 跳过文件")

    response = input("\n确认执行？(y/n): ").strip().lower()
    if response != 'y':
        print("已取消操作")
        sys.exit(0)

    # 执行格式化
    formatter = CodeFormatter(root_dir)
    formatter.format_directory()
    formatter.print_summary()


if __name__ == '__main__':
    main()

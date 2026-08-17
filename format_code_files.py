#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
代码文件格式化脚本
功能：
1. 扫描目录下所有的 .h/.cpp 文件
2. 转换为 UTF-8 BOM 格式（非UTF格式认为是GB18030/GBK/GB2312）
3. 转换为 UNIX 换行格式（自动清理 \r\r\n 双CR假空行、CRLF/LF 混用、孤立 CR）
4. 去除双重/多重 BOM（EF BB BF 重复出现）
5. 将 TAB 转换为 4 个空格
6. 去掉行尾空格
7. 根据 .gitignore 跳过不需要处理的目录和文件

用法：
    python format_code_files.py [目录] [--dry-run]
    --dry-run  仅检测并打印每个文件将做的修改，不写盘
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
        self.dry_run = False

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

            # 分析原始格式问题（行尾/BOM 等，供预览与报告）
            issues = self._analyze_issues(file_path, content)

            # 格式化内容
            formatted = self._format_content(content)

            rel_path = os.path.relpath(file_path, self.root_dir)

            if formatted == content:
                print(f"· 无变化: {rel_path}")
                return

            if self.dry_run:
                self._print_issues(rel_path, issues)
                print(f"○ 预览(不写盘): {rel_path}")
                self.processed_count += 1
                return

            # 写回文件
            self._write_file(file_path, formatted)

            self._print_issues(rel_path, issues)
            print(f"✓ 处理: {rel_path}")
            self.processed_count += 1

        except Exception as e:
            rel_path = os.path.relpath(file_path, self.root_dir)
            print(f"✗ 错误: {rel_path} - {str(e)}")
            self.error_count += 1

    def _analyze_issues(self, file_path: str, content: str) -> dict:
        """分析原始内容中的格式问题，返回问题字典（用于 dry-run 预览）"""
        with open(file_path, 'rb') as f:
            raw = f.read()

        issues = {}

        # BOM 相关
        if raw.startswith(b'\xef\xbb\xbf\xef\xbb\xbf'):
            issues['double_bom'] = True
        if not raw.startswith(b'\xef\xbb\xbf') and content:
            issues['bom_add'] = True

        # 换行相关：\r\r\n 是"双CR换行"（假空行来源），\r\n 是标准 CRLF
        crcrlf = raw.count(b'\r\r\n')
        crlf_total = raw.count(b'\r\n')          # 含 crcrlf 内嵌的一个 \r\n
        lf_total = raw.count(b'\n')
        lone_lf = lf_total - crlf_total
        lone_cr = raw.count(b'\r') - crlf_total - crcrlf

        if crcrlf:
            issues['crcrlf'] = crcrlf
        if crlf_total - crcrlf:
            issues['crlf'] = crlf_total - crcrlf
        if lone_lf:
            issues['lone_lf'] = lone_lf
        if lone_cr:
            issues['lone_cr'] = lone_cr

        # 行尾空白 / TAB / Ctrl-Z
        trailing = sum(1 for ln in content.split('\n') if ln.rstrip(' \t\x0b\x0c\xa0') != ln)
        if trailing:
            issues['trailing_ws'] = trailing
        tab_lines = sum(1 for ln in content.split('\n') if '\t' in ln)
        if tab_lines:
            issues['tabs'] = tab_lines
        if '\x1a' in content:
            issues['ctrl_z'] = content.count('\x1a')

        return issues

    def _print_issues(self, rel_path: str, issues: dict):
        """打印检测到的格式问题"""
        if not issues:
            return
        parts = []
        if issues.get('bom_add'):
            parts.append('将添加BOM')
        if issues.get('double_bom'):
            parts.append('多重BOM')
        if issues.get('crcrlf'):
            parts.append(f"双CR换行x{issues['crcrlf']}(假空行)")
        if issues.get('crlf'):
            parts.append(f"CRLFx{issues['crlf']}")
        if issues.get('lone_lf'):
            parts.append(f"裸LFx{issues['lone_lf']}")
        if issues.get('lone_cr'):
            parts.append(f"裸CRx{issues['lone_cr']}")
        if issues.get('trailing_ws'):
            parts.append(f"尾空白x{issues['trailing_ws']}")
        if issues.get('tabs'):
            parts.append(f"TABx{issues['tabs']}")
        if issues.get('ctrl_z'):
            parts.append(f"Ctrl-Zx{issues['ctrl_z']}")
        print(f"  [格式问题] {rel_path}: {'; '.join(parts)}")

    def _read_file(self, file_path: str) -> str:
        """读取文件，自动检测编码，并去除所有前导 BOM 字符（含双重/多重 BOM）"""
        # 先读取原始字节检查 BOM 标记
        with open(file_path, 'rb') as f:
            raw_bytes = f.read()

        # 检查文件是否有 UTF-8 BOM (EF BB BF)
        if raw_bytes.startswith(b'\xef\xbb\xbf'):
            try:
                return raw_bytes.decode('utf-8-sig').lstrip('\ufeff')
            except (UnicodeDecodeError, UnicodeError):
                pass

        # 优先尝试不带 BOM 的 UTF-8（处理 UTF-8 无 BOM 的情况）
        try:
            return raw_bytes.decode('utf-8').lstrip('\ufeff')
        except (UnicodeDecodeError, UnicodeError):
            pass

        # 如果 UTF-8 失败，再尝试 GB 系编码
        encodings = ['gb2312', 'gbk', 'gb18030', 'cp936', 'latin-1']

        for encoding in encodings:
            try:
                content = raw_bytes.decode(encoding)
                # 去除所有前导 BOM 字符（修复双重或多重 BOM 问题）
                return content.lstrip('\ufeff')
            except (UnicodeDecodeError, UnicodeError):
                continue

        print(f"无法读取文件 (编码检测失败): {os.path.relpath(file_path, self.root_dir)}")
        return None

    def _format_content(self, content: str) -> str:
        """格式化文件内容"""
        # 1. 转换为 UNIX 换行格式 (LF)。顺序关键：
        #    必须先处理 \r\r\n（双CR换行——通常由"对已是 CRLF 的文件再做
        #    \n->\r\n 替换"产生，编辑器会把它渲染成每行后多一个空行），
        #    再处理 \r\n 与孤立 \r。顺序颠倒的话，\r\r\n 会被拆成 \n\n，
        #    凭空多出一行空行。
        content = content.replace('\r\r\n', '\n')
        content = content.replace('\r\n', '\n')
        content = content.replace('\r', '\n')

        # 2. 去掉行尾空白（空格/TAB/垂直制表/换页/不间断空格 NBSP）
        lines = content.split('\n')
        lines = [line.rstrip(' \t\x0b\x0c\xa0') for line in lines]
        content = '\n'.join(lines)

        # 3. 去除 Windows 遗留的 Ctrl-Z (SUB, 0x1A) 文件结束标记
        content = content.replace('\x1a', '')

        # 4. 将 TAB 按对齐规则转换为空格（tab size = 4）
        content = self._expand_tabs(content, tab_size=4)

        # 5. 确保文件以新行结尾
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
        """写入文件，使用 UTF-8 BOM 编码；newline='\n' 避免 Windows 文本模式把 LF 翻译回 CRLF"""
        with open(file_path, 'w', encoding='utf-8-sig', newline='\n') as f:
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
    # 解析命令行参数：可选目录 + --dry-run（仅预览不写盘）
    root_dir = os.path.dirname(os.path.abspath(__file__))
    dry_run = False

    for arg in sys.argv[1:]:
        if arg == '--dry-run':
            dry_run = True
        elif arg.startswith('-'):
            print(f"未知参数: {arg}")
            sys.exit(1)
        else:
            root_dir = arg

    if not os.path.isdir(root_dir):
        print(f"错误: 目录不存在 - {root_dir}")
        sys.exit(1)

    # 询问用户确认（预览模式直接执行）
    print(f"将扫描目录: {root_dir}")
    print("将执行以下操作:")
    print("  1. 转换为 UTF-8 BOM 格式")
    print("  2. 转换为 UNIX 换行格式")
    print("  3. TAB 转换为 4 个空格")
    print("  4. 去掉行尾空格")
    print("  5. 根据 .gitignore 跳过文件")
    if dry_run:
        print("\n[预览模式] 仅检测并打印将做的修改，不写盘")

    if not dry_run:
        response = input("\n确认执行？(y/n): ").strip().lower()
        if response != 'y':
            print("已取消操作")
            sys.exit(0)

    # 执行格式化
    formatter = CodeFormatter(root_dir)
    formatter.dry_run = dry_run
    formatter.format_directory()
    formatter.print_summary()


if __name__ == '__main__':
    main()

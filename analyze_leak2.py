#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Memory Leak Analyzer for ULRE Vulkan Runtime
分析 run.log 中的内存泄漏
"""

import re
from collections import Counter, defaultdict

def analyze_leaks(log_file='run.log'):
    """分析内存泄漏日志"""
    
    # Type mapping
    type_names = {
        0x0: 'Material/MaterialInstance',
        0x6: 'VkCommandBuffer',
        0x7: 'VkFence',
        0x8: 'VkDeviceMemory',
        0x9: 'VkBuffer',
        0xa: 'VkImage',
        0xe: 'VkImageView',
        0xf: 'VkShaderModule',
        0x11: 'VkPipelineLayout',
        0x12: 'VkRenderPass',
        0x13: 'VkPipeline',
        0x14: 'VkDescriptorSetLayout',
        0x17: 'VkDescriptorSet',
        0x18: 'VkFramebuffer',
    }
    
    leaks = []
    leak_by_type = defaultdict(list)
    leak_by_file = defaultdict(list)
    
    try:
        with open(log_file, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            
        # 提取所有泄漏行
        leak_lines = re.findall(r'\[LEAK\] .*', content)
        
        for line in leak_lines:
            # 提取类型
            type_match = re.search(r'Type=(0x[0-9a-fA-F]+)', line)
            # 提取名称
            name_match = re.search(r'Name=([^ ]+)', line)
            # 提取创建位置（如果有）
            location_match = re.search(r'Created at ([^:]+):(\d+) in (.+)\(\)', line)
            
            if type_match:
                type_hex = type_match.group(1)
                type_int = int(type_hex, 16)
                type_name = type_names.get(type_int, f'UNKNOWN({type_hex})')
                obj_name = name_match.group(1) if name_match else 'UNKNOWN'
                
                leak_info = {
                    'type_int': type_int,
                    'type_hex': type_hex,
                    'type_name': type_name,
                    'name': obj_name,
                    'line': line
                }
                
                if location_match:
                    file_path = location_match.group(1)
                    line_num = location_match.group(2)
                    function = location_match.group(3)
                    leak_info['file'] = file_path
                    leak_info['line_num'] = line_num
                    leak_info['function'] = function
                    
                    # 提取文件名
                    filename = file_path.split('\\')[-1]
                    leak_by_file[filename].append(leak_info)
                else:
                    leak_info['file'] = 'UNKNOWN'
                    leak_info['line_num'] = '?'
                    leak_info['function'] = 'UNKNOWN'
                
                leaks.append(leak_info)
                leak_by_type[type_name].append(leak_info)
    
    except Exception as e:
        print(f"Error reading log file: {e}")
        return
    
    # 打印统计结果
    print("="*80)
    print("MEMORY LEAK ANALYSIS REPORT")
    print("="*80)
    print(f"\nTotal Leaks: {len(leaks)}\n")
    
    # 按类型统计
    print("-"*80)
    print("LEAK COUNT BY TYPE:")
    print("-"*80)
    type_counts = Counter([leak['type_name'] for leak in leaks])
    for type_name, count in sorted(type_counts.items(), key=lambda x: x[1], reverse=True):
        print(f"  {type_name:30s}: {count:3d} leaks")
    
    # 按文件统计
    print("\n" + "-"*80)
    print("LEAK COUNT BY SOURCE FILE:")
    print("-"*80)
    for filename, leak_list in sorted(leak_by_file.items(), key=lambda x: len(x[1]), reverse=True):
        print(f"  {filename:40s}: {len(leak_list):3d} leaks")
    
    # 详细列出每个类型的泄漏
    print("\n" + "="*80)
    print("DETAILED LEAK INFORMATION BY TYPE:")
    print("="*80)
    
    for type_name in sorted(type_counts.keys(), key=lambda x: type_counts[x], reverse=True):
        print(f"\n[{type_name}] - {type_counts[type_name]} leaks:")
        print("-"*80)
        
        leaks_of_type = leak_by_type[type_name]
        
        # 按文件分组
        by_file = defaultdict(list)
        for leak in leaks_of_type:
            key = f"{leak['file']}:{leak['line_num']}"
            by_file[key].append(leak)
        
        for location, leak_list in sorted(by_file.items(), key=lambda x: len(x[1]), reverse=True):
            count = len(leak_list)
            first_leak = leak_list[0]
            print(f"  [{count}x] {location}")
            if first_leak['file'] != 'UNKNOWN':
                print(f"       Function: {first_leak['function']}")
            print(f"       Names: {', '.join([l['name'] for l in leak_list[:5]])}")
            if count > 5:
                print(f"              ... and {count-5} more")
    
    # 推荐修复优先级
    print("\n" + "="*80)
    print("RECOMMENDED FIX PRIORITY:")
    print("="*80)
    
    priority = []
    for filename, leak_list in leak_by_file.items():
        if filename != 'UNKNOWN':
            # 按文件中的行号分组
            by_line = defaultdict(list)
            for leak in leak_list:
                by_line[leak['line_num']].append(leak)
            
            for line_num, line_leaks in by_line.items():
                priority.append({
                    'file': leak_list[0]['file'],
                    'filename': filename,
                    'line': line_num,
                    'count': len(line_leaks),
                    'types': set([l['type_name'] for l in line_leaks])
                })
    
    priority.sort(key=lambda x: x['count'], reverse=True)
    
    for i, item in enumerate(priority[:10], 1):
        types_str = ', '.join(sorted(item['types']))
        print(f"{i:2d}. {item['filename']:30s}:{item['line']:>4s}  "
              f"({item['count']:2d} leaks) - {types_str}")

if __name__ == '__main__':
    analyze_leaks()

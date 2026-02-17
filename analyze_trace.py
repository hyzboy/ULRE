#!/usr/bin/env python3
"""
ObjectTracker 离线分析工具
用于分析从崩溃/泄露时导出的分配追踪文件

使用方法:
  python3 analyze_trace.py trace.bin query <object_id>
  python3 analyze_trace.py trace.bin stats
  python3 analyze_trace.py trace.bin list [limit]
"""

import struct
import sys
from pathlib import Path
from collections import defaultdict
from datetime import datetime

class AllocationEvent:
    """对应 C++ 中的 AllocationEvent 结构"""
    
    # ObjectTypeTag 枚举值
    OBJECT_TYPES = {
        0: "None",
        1: "Queue",
        2: "Semaphore",
        3: "Fence",
        4: "RenderCommandBuffer",
        5: "TextureCommandBuffer",
        6: "ComputeCommandBuffer",
        7: "Buffer",
        8: "Memory",
        9: "Image",
        10: "ImageView",
        11: "Sampler",
        12: "Framebuffer",
        13: "RenderPass",
        14: "Pipeline",
        15: "PipelineLayout",
        16: "DescriptorSet",
        17: "DescriptorSetLayout",
        18: "ShaderModule",
        19: "Swapchain",
        20: "RenderTarget",
        21: "Texture",
        22: "Material",
        23: "MaterialInstance",
        24: "Mesh",
        25: "IndirectDrawBuffer",
        26: "IndirectDrawIndexedBuffer",
        27: "IndirectDispatchBuffer",
        28: "VertexBuffer",
        29: "IndexBuffer",
        30: "UniformBuffer",
        31: "StorageBuffer",
        32: "TextureBuffer",
        33: "ReadbackBuffer",
        34: "RenderSystem",
        35: "BatchSystem",
        36: "CommandRecorder",
        37: "FrameResource",
        38: "SwapchainFrame",
    }
    
    def __init__(self):
        self.object_id = 0
        self.timestamp = 0
        self.object_type = 0
        self.object_name = ""
        self.stack_depth = 0
        self.stack = []
    
    def __repr__(self):
        type_name = self.OBJECT_TYPES.get(self.object_type, f"Unknown({self.object_type})")
        return f"Event(id={self.object_id}, type={type_name}, name={self.object_name!r}, depth={self.stack_depth})"

class TraceReader:
    """读取追踪文件"""
    
    def __init__(self, filename):
        self.filename = filename
        self.events = []
        self._read_file()
    
    def _read_file(self):
        """从二进制文件读取所有事件"""
        try:
            with open(self.filename, 'rb') as f:
                while True:
                    # 读取事件头
                    header = f.read(8 + 8 + 1)  # object_id + timestamp + object_type
                    if len(header) < 17:
                        break
                    
                    object_id, timestamp, object_type = struct.unpack('<QQB', header)
                    
                    # 读取对象名称（32字节）
                    name_bytes = f.read(32)
                    if len(name_bytes) < 32:
                        break
                    object_name = name_bytes.rstrip(b'\x00').decode('utf-8', errors='ignore')
                    
                    # 读取栈深度
                    depth_bytes = f.read(4)
                    if len(depth_bytes) < 4:
                        break
                    stack_depth, = struct.unpack('<I', depth_bytes)
                    
                    # 读取栈帧
                    stack = []
                    for _ in range(stack_depth):
                        frame_bytes = f.read(8 + 4 + 4 + 8)  # file_hash + line + column + func_hash
                        if len(frame_bytes) < 24:
                            break
                        file_hash, line, column, func_hash = struct.unpack('<QIIQ', frame_bytes)
                        stack.append({
                            'file_hash': file_hash,
                            'line': line,
                            'column': column,
                            'func_hash': func_hash,
                        })
                    
                    event = AllocationEvent()
                    event.object_id = object_id
                    event.timestamp = timestamp
                    event.object_type = object_type
                    event.object_name = object_name
                    event.stack_depth = len(stack)
                    event.stack = stack
                    
                    self.events.append(event)
        
        except Exception as e:
            print(f"Error reading trace file: {e}", file=sys.stderr)
    
    def get_event(self, object_id):
        """查询特定ID的事件"""
        for event in self.events:
            if event.object_id == object_id:
                return event
        return None
    
    def get_events_by_type(self, type_id):
        """查询特定类型的事件"""
        return [e for e in self.events if e.object_type == type_id]
    
    def get_stats(self):
        """获取统计信息"""
        stats = {
            'total_events': len(self.events),
            'by_type': defaultdict(int),
            'by_name': defaultdict(int),
        }
        
        for event in self.events:
            type_name = AllocationEvent.OBJECT_TYPES.get(event.object_type, f"Unknown({event.object_type})")
            stats['by_type'][type_name] += 1
            stats['by_name'][event.object_name] += 1
        
        return stats

def cmd_query(reader, object_id):
    """查询单个对象"""
    event = reader.get_event(object_id)
    
    if not event:
        print(f"[ERROR] object_id {object_id} not found", file=sys.stderr)
        return 1
    
    type_name = AllocationEvent.OBJECT_TYPES.get(event.object_type, f"Unknown({event.object_type})")
    ts_sec = event.timestamp / 1e9
    
    print(f"object_id:     {event.object_id}")
    print(f"object_type:   {type_name}")
    print(f"object_name:   {event.object_name!r}")
    print(f"timestamp:     {ts_sec:.3f}s")
    print(f"stack_depth:   {event.stack_depth}")
    print()
    
    if event.stack_depth > 0:
        print("Allocation Stack:")
        for i, frame in enumerate(event.stack):
            print(f"  [{i}] file_hash=0x{frame['file_hash']:016x} "
                  f"line={frame['line']} col={frame['column']} "
                  f"func_hash=0x{frame['func_hash']:016x}")
    else:
        print("(No stack captured)")
    
    return 0

def cmd_stats(reader):
    """显示统计信息"""
    stats = reader.get_stats()
    
    print(f"Total Events: {stats['total_events']}")
    print()
    
    print("By Type:")
    for type_name in sorted(stats['by_type'].keys()):
        count = stats['by_type'][type_name]
        print(f"  {type_name:30s}  {count:6d}")
    print()
    
    print("By Name (Top 20):")
    sorted_names = sorted(stats['by_name'].items(), key=lambda x: x[1], reverse=True)
    for name, count in sorted_names[:20]:
        name_display = name if name else "(empty)"
        print(f"  {name_display:40s}  {count:6d}")
    
    return 0

def cmd_list(reader, limit=20):
    """列出前N个事件"""
    limit = min(limit, len(reader.events))
    
    print(f"Listing first {limit} events:")
    print()
    
    for i, event in enumerate(reader.events[:limit]):
        type_name = AllocationEvent.OBJECT_TYPES.get(event.object_type, f"Unknown({event.object_type})")
        ts_sec = event.timestamp / 1e9
        name_display = event.object_name if event.object_name else "(empty)"
        
        print(f"[{i}] ID={event.object_id:10d} Type={type_name:25s} "
              f"Name={name_display:20s} Depth={event.stack_depth:2d} "
              f"T={ts_sec:.3f}s")
    
    return 0

def main():
    if len(sys.argv) < 3:
        print("Usage:")
        print(f"  {sys.argv[0]} <trace.bin> query <object_id>")
        print(f"  {sys.argv[0]} <trace.bin> stats")
        print(f"  {sys.argv[0]} <trace.bin> list [limit]")
        return 1
    
    trace_file = sys.argv[1]
    
    if not Path(trace_file).exists():
        print(f"Error: Trace file '{trace_file}' not found", file=sys.stderr)
        return 1
    
    print(f"Loading trace file: {trace_file}")
    reader = TraceReader(trace_file)
    print(f"Loaded {len(reader.events)} events\n")
    
    command = sys.argv[2]
    
    if command == "query":
        if len(sys.argv) < 4:
            print("Error: Missing object_id", file=sys.stderr)
            return 1
        try:
            object_id = int(sys.argv[3])
            return cmd_query(reader, object_id)
        except ValueError:
            print(f"Error: Invalid object_id '{sys.argv[3]}'", file=sys.stderr)
            return 1
    
    elif command == "stats":
        return cmd_stats(reader)
    
    elif command == "list":
        limit = 20
        if len(sys.argv) >= 4:
            try:
                limit = int(sys.argv[3])
            except ValueError:
                print(f"Warning: Invalid limit '{sys.argv[3]}', using default 20")
        return cmd_list(reader, limit)
    
    else:
        print(f"Error: Unknown command '{command}'", file=sys.stderr)
        return 1

if __name__ == '__main__':
    sys.exit(main())

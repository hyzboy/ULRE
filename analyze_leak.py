import re
from collections import defaultdict

with open('run.log', 'r', encoding='utf-16') as f:
    content = f.read()

# 统计泄漏
leaks = re.findall(r'\[LEAK\].*Created at (.+\.cpp):(\d+)', content)
files = defaultdict(int)
for file, line in leaks:
    filename = file.split('\\')[-1]
    files[filename] += 1

print(f"=== 内存泄漏分析 ===")
print(f"总泄漏数: {len(leaks)}\n")
print("泄漏TOP文件:")
for name, count in sorted(files.items(), key=lambda x: x[1], reverse=True)[:5]:
    print(f"  {name}: {count}")

# 统计类型
types_leak = defaultdict(int)
for leak in re.findall(r'\[LEAK\] Type=(0x[a-f0-9]+)', content):
    types_leak[leak] += 1

print("\n泄漏对象类型TOP:")
type_map = {
    '0x0': 'Material',
    '0x6': 'CmdBuf',
    '0x8': 'Memory',
    '0x9': 'Buffer',
    '0xa': 'Image',
    '0xe': 'ImageView',
    '0xf': 'Shader',
    '0x11': 'PipelineLayout',
    '0x12': 'RenderPass',
    '0x13': 'Pipeline',
    '0x14': 'DescSetLayout',
    '0x17': 'DescSet',
}
for ty, count in sorted(types_leak.items(), key=lambda x: x[1], reverse=True)[:5]:
    name = type_map.get(ty, ty)
    print(f"  {name}: {count}")

# 双重销毁
double = re.findall(r'NOT TRACKED|double destroy', content)
print(f"\n可能的二次销毁: {len(double)}")

# 泄漏详情
print("\n主要泄漏源代码位置:")
sources = defaultdict(int)
for match in re.finditer(r'Created at (.+):(\d+) in', content):
    loc = f"{match.group(1)}:{match.group(2)}"
    sources[loc] += 1

for loc, count in sorted(sources.items(), key=lambda x: x[1], reverse=True)[:5]:
    print(f"  {loc}: {count}×")

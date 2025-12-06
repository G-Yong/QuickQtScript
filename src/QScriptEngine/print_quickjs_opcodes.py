# 保存为 scripts/print_quickjs_opcodes.py
import re
import sys
from pathlib import Path

# 用相对或绝对路径指向仓库中的 quickjs-opcode.h
file_path = Path(r"./quickjs/quickjs-opcode.h")
if not file_path.exists():
    print("文件未找到:", file_path)
    sys.exit(2)

pattern = re.compile(r'^\s*(?:DEF|def)\(\s*([A-Za-z0-9_]+)\s*,')
names = []
with file_path.open(encoding='utf-8') as f:
    for line in f:
        m = pattern.match(line)
        if m:
            names.append(m.group(1))

# 按出现顺序编号，从 0 开始（quickjs 通常如此）
for i, name in enumerate(names):
    print(f"OP_{name} = {i}")
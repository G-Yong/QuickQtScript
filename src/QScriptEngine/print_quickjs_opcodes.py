# 保存为 scripts/print_quickjs_opcodes.py
import re
import sys
from pathlib import Path

# 用相对或绝对路径指向仓库中的 quickjs-opcode.h
file_path = Path(r"./quickjs/quickjs-opcode.h")
if not file_path.exists():
    print("文件未找到:", file_path)
    sys.exit(2)

pattern = re.compile(r'^\s*DEF\(\s*([A-Za-z0-9_]+)\s*,')
names = []
with file_path.open(encoding='utf-8') as f:
    for line in f:
        # 跳过宏定义行
        if '#define' in line or line.strip().startswith('#'):
            continue
        m = pattern.match(line)
        if m:
            names.append(m.group(1))

# 按出现顺序编号，从 0 开始（quickjs 通常如此）
op_map = {i: f"OP_{name}" for i, name in enumerate(names)}
name_map = {f"OP_{name}": i for i, name in enumerate(names)}

def opcode_to_name(value: int) -> str:
    """Return the opcode name for the given numeric value.

    If the value is not found, returns a string like 'UNKNOWN_OP_<value>'.
    """
    return op_map.get(int(value), f"UNKNOWN_OP_{value}")


def name_to_opcode(name: str) -> int:
    """Return the numeric opcode for a given name like 'OP_call'.

    Raises KeyError if not found.
    """
    return name_map[name]


if __name__ == '__main__':
    # CLI: no args -> print full mapping; one arg (int) -> print name
    if len(sys.argv) == 1:
        for i in sorted(op_map.keys()):
            # Print in the format: OP_name = value,
            print(f"{op_map[i]} = {i},")
    else:
        arg = sys.argv[1]
        try:
            val = int(arg, 0)
            print(opcode_to_name(val))
        except ValueError:
            # maybe it's a name
            try:
                print(name_to_opcode(arg))
            except KeyError:
                print(f"Unknown opcode or name: {arg}")
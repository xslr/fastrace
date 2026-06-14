import re

with open("cpp/include/BlfTypes.h") as f:
    content = f.read()

enum_match = re.search(r"enum BLFObjectType : uint32_t \{(.*?)\};", content, re.DOTALL)
enum_text = enum_match.group(1)

assignments = []
for line in enum_text.split("\n"):
    m = re.search(r"^\s*([A-Za-z0-9_]+)\s*=", line)
    if m:
        name = m.group(1)
        assignments.append(f"    arr[{name}] = \"{name}\";")

new_arr_text = "\n".join(assignments)

with open("cpp/src/Analyzer.cpp") as f:
    analyzer = f.read()

start_marker = "    std::array<std::string_view, 256> arr{};\n"
end_marker = "    return arr;"
start_idx = analyzer.find(start_marker)
if start_idx == -1:
    print("start marker not found")
    exit(1)
start_idx += len(start_marker)

end_idx = analyzer.find(end_marker, start_idx)
if end_idx == -1:
    print("end marker not found")
    exit(1)

new_analyzer = analyzer[:start_idx] + new_arr_text + "\n" + analyzer[end_idx:]
with open("cpp/src/Analyzer.cpp", "w") as f:
    f.write(new_analyzer)
print("Replaced successfully")

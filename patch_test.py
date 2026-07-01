import re

cpp_path = "ui/tests/tst_TimelineOverviewWidget.cpp"
with open(cpp_path, "r") as f:
    cpp = f.read()

# Update the number of expected checkboxes
cpp = re.sub(r'QVERIFY2\(children\.size\(\) >= 2, "Expected at least two QCheckBox children \(CAN \+ Ethernet\)"\);', 'QVERIFY2(children.size() >= 4, "Expected at least four QCheckBox children");', cpp)

with open(cpp_path, "w") as f:
    f.write(cpp)
